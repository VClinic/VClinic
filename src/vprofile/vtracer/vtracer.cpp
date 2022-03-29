#include <dr_api.h>
#include "vtracer.h"
#include "drmgr.h"
#include "drutil.h"
#include "drvector.h"
#include <stdint.h>
#include <stddef.h> /* for offsetof */
#include <string.h> /* for memcpy */

#ifdef DEBUG
    #define VTRACER_DEBUG
//    #define VTRACER_DEBUG_DETAIL
#endif

#define ALIGNED(x, alignment) ((((ptr_uint_t)x) & ((alignment)-1)) == 0)
#define ALIGN_FORWARD(x, alignment) \
    ((((ptr_uint_t)x) + ((alignment)-1)) & (~((alignment)-1)))
#define ALIGN_BACKWARD(x, alignment) (((ptr_uint_t)x) & (~((ptr_uint_t)(alignment)-1)))

#include "vtracer_define.h"

enum {
    TRACE_BUF_TLS_OFFS_BUF_PTR,
    TRACE_BUF_TLS_OFFS_BUF_END,
    // The buffer base can be calculated from buffer end with statically known buffer size
    // so no need to waste the restricted tls slots for fast storage.
    // TRACE_BUF_TLS_OFFS_BUF_BASE,
    TRACE_BUF_TLS_COUNT
};

typedef struct {
    byte *seg_base;
    byte *cli_base;    /* the base of the buffer from the client's perspective */
    byte *buf_base;    /* the actual base of the buffer */
    size_t total_size; /* the actual size of the buffer */
#ifdef VTRACER_DEBUG
    int16_t scratch;
#endif
} per_thread_t;

/* global TLS implementation */
enum {
    INSTRACE_TLS_OFFS_BUF_PTR,
    INSTRACE_TLS_COUNT, /* total number of TLS slots allocated */
};

static int tls_idx;
static uint tls_offs;
static reg_id_t tls_seg;

typedef struct {
    byte *numInsBuff;
} ins_per_thread_t;

/* sampling variables */
static bool enable_sampling = false;
static int window_enable = 0;
static int window_disable = 0;

/* holds per-client (also per-buf) information */
static drvector_t clients;
/* A flag to avoid work when no buffers were ever created. */
static bool any_bufs_created;

static per_thread_t *
per_thread_init(void *drcontext, vtrace_buffer_t *buf);

static void
event_thread_init(void *drcontext);
static void
event_thread_exit(void *drcontext);

void vtracer_enable_sampling(int win_enable,
                             int win_disable)
{
    enable_sampling = true;
    window_enable = win_enable;
    window_disable = win_disable;
    // only register restricted tls resources when needed
    if(tls_idx==-1) {
        tls_idx = drmgr_register_tls_field();
        DR_ASSERT_MSG(tls_idx!=-1, "vtracer_enable_sampling: drmgr_register_tls_field failed!\n");
        if (!dr_raw_tls_calloc(&tls_seg, &tls_offs, INSTRACE_TLS_COUNT, 0))
            DR_ASSERT_MSG(false, "vtracer_enable_sampling: dr_raw_tls_calloc failed!\n");
    }
}

void vtracer_disable_sampling()
{
    enable_sampling = false;
}

bool vtracer_get_sampling_state(void *drcontext)
{
    DR_ASSERT_MSG(enable_sampling, "vtracer_get_sampling_state usage error: must called after vtracer_enable_sampling!");
    ins_per_thread_t *pt = (ins_per_thread_t*)drmgr_get_tls_field(drcontext, tls_idx);
    return (size_t)BUF_PTR(pt->numInsBuff, tls_offs) < (size_t)window_enable;
}

vtrace_buffer_t *vtracer_create_trace_buffer(uint buffer_size)
{
    return vtracer_create_trace_buffer_ex(buffer_size, NULL, NULL, NULL, NULL);
}

vtrace_buffer_t *
vtracer_create_trace_buffer_ex(uint buffer_size,
                               vtracer_buf_full_cb_t full_cb,
                               void* user_data_full,
                               vtracer_buf_fill_num_cb_t fill_num_cb,
                               void* user_data_fill_num)
{
// #ifdef AARCH64
//     buffer_size = buffer_size < 4096 ? buffer_size : 4095;
// #endif
    vtrace_buffer_t *new_client;
    int tls_idx;
    uint tls_offs;
    reg_id_t tls_seg;

    /* allocate raw TLS so we can access it from the code cache */
    if (!dr_raw_tls_calloc(&tls_seg, &tls_offs, TRACE_BUF_TLS_COUNT, 0))
        return NULL;

    tls_idx = drmgr_register_tls_field();
    if (tls_idx == -1)
        return NULL;

    /* init the client struct */
    new_client = (vtrace_buffer_t*)dr_global_alloc(sizeof(*new_client));
    new_client->buf_size = buffer_size;
    new_client->full_cb  = full_cb;
    new_client->user_data_full = user_data_full;
    new_client->fill_num_cb = fill_num_cb;
    new_client->user_data_fill_num = user_data_fill_num;
    new_client->tls_offs = tls_offs;
    new_client->tls_seg = tls_seg;
    new_client->tls_idx = tls_idx;

    /* We don't attempt to re-use NULL entries (presumably which
     * have already been freed), for simplicity.
     */
    new_client->vec_idx = clients.entries;
    drvector_append(&clients, new_client);

    if (!any_bufs_created)
        any_bufs_created = true;

    return new_client;
}

void vtracer_buffer_free(vtrace_buffer_t *buf)
{
    if (!(buf != NULL && (vtrace_buffer_t*)drvector_get_entry(&clients, buf->vec_idx) == buf)) {
        assert(false && "Unknown error in vtracer_buffer_free!");
    }
    /* NULL out the entry in the vector */
    ((vtrace_buffer_t **)clients.array)[buf->vec_idx] = NULL;

    if (!drmgr_unregister_tls_field(buf->tls_idx) || !dr_raw_tls_cfree(buf->tls_offs, TRACE_BUF_TLS_COUNT))
        assert(false && "Unknown error in vtracer_buffer_free!");
    dr_global_free(buf, sizeof(*buf));
}

static void bb_update(int insCnt) {
    void *drcontext = dr_get_current_drcontext();
    ins_per_thread_t *pt = (ins_per_thread_t *)drmgr_get_tls_field(drcontext, tls_idx);
    uint64_t val = reinterpret_cast<uint64_t>(BUF_PTR(pt->numInsBuff, tls_offs + INSTRACE_TLS_OFFS_BUF_PTR));
    val += insCnt;
    BUF_PTR(pt->numInsBuff, tls_offs + INSTRACE_TLS_OFFS_BUF_PTR) = reinterpret_cast<byte*>(val);
}

static void bb_update_and_check(int insCnt) {
    void *drcontext = dr_get_current_drcontext();
    ins_per_thread_t *pt = (ins_per_thread_t *)drmgr_get_tls_field(drcontext, tls_idx);
    uint64_t val = reinterpret_cast<uint64_t>(BUF_PTR(pt->numInsBuff, tls_offs + INSTRACE_TLS_OFFS_BUF_PTR));
    val += insCnt;
    if(val>=(uint64_t)window_disable) { val=val-(uint64_t)window_disable; }
    BUF_PTR(pt->numInsBuff, tls_offs + INSTRACE_TLS_OFFS_BUF_PTR) = reinterpret_cast<byte*>(val);
}

#if defined(ARM) || defined(AARCH64)
#        define TRACE_LOAD_IMM32_0(dc, Rt, imm) \
            INSTR_CREATE_movz((dc), (Rt), (imm), OPND_CREATE_INT(0))
#        define TRACE_LOAD_IMM32_16(dc, Rt, imm) \
            INSTR_CREATE_movk((dc), (Rt), (imm), OPND_CREATE_INT(16))
#        define TRACE_LOAD_IMM32_32(dc, Rt, imm) \
            INSTR_CREATE_movk((dc), (Rt), (imm), OPND_CREATE_INT(32))
#        define TRACE_LOAD_IMM32_48(dc, Rt, imm) \
            INSTR_CREATE_movk((dc), (Rt), (imm), OPND_CREATE_INT(48))
static inline void
minstr_load_wint_to_reg(void *drcontext, instrlist_t *ilist, instr_t *where, reg_id_t reg,
                        int32_t wint_num)
{
    MINSERT(ilist, where,
            TRACE_LOAD_IMM32_0(drcontext, opnd_create_reg(reg),
                                  OPND_CREATE_INT(wint_num & 0xffff)));
    wint_num = (wint_num >> 16) & 0xffff;
    if(wint_num) {
        MINSERT(ilist, where,
                TRACE_LOAD_IMM32_16(drcontext, opnd_create_reg(reg),
                                    OPND_CREATE_INT(wint_num)));
    }
}

#ifdef AARCH64
static inline void
minstr_load_wwint_to_reg(void *drcontext, instrlist_t *ilist, instr_t *where,
                         reg_id_t reg, uint64_t wwint_num)
{
    MINSERT(ilist, where,
            TRACE_LOAD_IMM32_0(drcontext, opnd_create_reg(reg),
                                  OPND_CREATE_INT(wwint_num & 0xffff)));
    uint64_t tmp = (wwint_num >> 16) & 0xffff;
    if(tmp) {
        MINSERT(ilist, where,
            TRACE_LOAD_IMM32_16(drcontext, opnd_create_reg(reg),
                                OPND_CREATE_INT(tmp)));
    }
    tmp = (wwint_num >> 32) & 0xffff;
    if(tmp) {
        MINSERT(ilist, where,
            TRACE_LOAD_IMM32_32(drcontext, opnd_create_reg(reg),
                                OPND_CREATE_INT(tmp)));
    }
    tmp = (wwint_num >> 48) & 0xffff;
    if(tmp) {
        MINSERT(ilist, where,
            TRACE_LOAD_IMM32_48(drcontext, opnd_create_reg(reg),
                                OPND_CREATE_INT(tmp)));
    }
}
#endif
#    endif

#ifdef VTRACER_DEBUG
void debug_scratch_check(vtrace_buffer_t *buf, int scr, int i) {
    per_thread_t *data = (per_thread_t*)drmgr_get_tls_field(dr_get_current_drcontext(), buf->tls_idx);
#ifdef VTRACER_DEBUG_DETAIL
    dr_fprintf(STDOUT, "buf %d actual fill num is %d\n", i, data->scratch);
#endif
    DR_ASSERT_MSG(data->scratch==0, "Usage Error: Estimated filled num not matched for actual fill num.");
    data->scratch = scr;
#ifdef VTRACER_DEBUG_DETAIL
    dr_fprintf(STDOUT, "buf %d next scratch = %d\n", i, data->scratch);
#endif
}

void debug_print_ins(instr_t *ins_clone) {
    instr_disassemble(dr_get_current_drcontext(), ins_clone, STDOUT);
}
#endif

static void insert_buf_check(void *drcontext, instrlist_t *bb, instr_t *ins, ushort *scratch) {
    unsigned int i;
    // buffered checking
    reg_id_t reg_ptr, reg_end;
    // reserve registers if not dead
#ifndef AARCH64
    // AArch64 can be aflag-free branched with TBZ instruction
    RESERVE_AFLAGS(drcontext, bb, ins);
#endif
    RESERVE_REG(drcontext, bb, ins, NULL, reg_ptr);
#if defined(ARM) || defined(AARCH64)
    // for ARM, we always need two register for the check of bursty sampling.
    RESERVE_REG(drcontext, bb, ins, NULL, reg_end);
#endif
    instr_t* skip_to_end = INSTR_CREATE_label(drcontext);
    if (enable_sampling) {
        instr_t* skip_to_update = INSTR_CREATE_label(drcontext);
        dr_insert_read_raw_tls(drcontext, bb, ins, tls_seg, tls_offs + INSTRACE_TLS_OFFS_BUF_PTR, reg_ptr);
        // TODO: use JECXZ (x86)/TBZ (arm) for aflag-free comparison & conditional jump
        // Clear insCnt when insCnt > WINDOW_DISABLE
#if defined(ARM) || defined(AARCH64)
    #ifdef AARCH64
        minstr_load_wwint_to_reg(drcontext, bb, ins, reg_end, window_enable);
        MINSERT(bb, ins, XINST_CREATE_sub(drcontext, opnd_create_reg(reg_ptr), opnd_create_reg(reg_end)));
        // if reg_ptr > reg_end, the top bit of (reg_ptr-reg_end) will be 0
        MINSERT(bb, ins, INSTR_CREATE_tbnz(drcontext, opnd_create_instr(skip_to_update),
                                /* If the top bit is still zero, skip the call. */
                                opnd_create_reg(reg_ptr), OPND_CREATE_INT(63)));
    #else
        minstr_load_wint_to_reg(drcontext, bb, ins, reg_end, window_enable);
        MINSERT(bb, ins, XINST_CREATE_cmp(drcontext, opnd_create_reg(reg_ptr), opnd_create_reg(reg_end)));
        MINSERT(bb, ins, XINST_CREATE_jump_cond(drcontext, DR_PRED_LE, opnd_create_instr(skip_to_update)));
    #endif
#else
        MINSERT(bb, ins, XINST_CREATE_cmp(drcontext, opnd_create_reg(reg_ptr), OPND_CREATE_INT32(window_enable)));
        MINSERT(bb, ins, XINST_CREATE_jump_cond(drcontext, DR_PRED_LE, opnd_create_instr(skip_to_update)));
#endif
        // clear the buffer when not sampled
        for (i = 0; i < clients.entries; ++i) {
            vtrace_buffer_t *buf = (vtrace_buffer_t*)drvector_get_entry(&clients, i);
            if (buf != NULL && buf->full_cb!=NULL && scratch[i]>0) {
                vtracer_insert_clear_buf(drcontext, buf, bb, ins, reg_ptr/*scratch*/);
            }
        }
        MINSERT(bb, ins, XINST_CREATE_jump(drcontext, opnd_create_instr(skip_to_end)));
        // update when insCnt <= WINDOW_ENABLE?
        MINSERT(bb, ins, skip_to_update);
    }
#if !defined(ARM) && !defined(AARCH64)
    // for X86/64, we can lazily reserve the register to avoid unnecessary spilling.
    RESERVE_REG(drcontext, bb, ins, NULL, reg_end);
#endif
    // now check if any buffers are full and update if necessary
    for (i = 0; i < clients.entries; ++i) {
        vtrace_buffer_t *buf = (vtrace_buffer_t*)drvector_get_entry(&clients, i);
        if (buf != NULL && buf->full_cb!=NULL && scratch[i]>0) {
            DR_ASSERT_MSG(scratch[i] <= buf->buf_size, "The filling number exceed the registered trace buffer size!");
#ifdef VTRACER_DEBUG
            dr_insert_clean_call(drcontext, bb, ins, (void *) debug_scratch_check, false, 3, OPND_CREATE_INTPTR(buf), OPND_CREATE_INT32(scratch[i]), OPND_CREATE_INT32(i));
#endif
            instr_t* skip_update = INSTR_CREATE_label(drcontext);
            // when there may be overflow, we check and update the trace buffer if possible
            // current buffer pointer
            vtracer_insert_load_buf_ptr(drcontext, buf, bb, ins, reg_ptr);
#ifdef AARCH64
            // for aarch64, larger scratch[i] will result in more instrumentation to track
            if(scratch[i] >= (1<<12)/*max immediate value for sub is 12-bit integer*/) {
                MINSERT(bb, ins, INSTR_CREATE_movz(drcontext, opnd_create_reg(reg_end), OPND_CREATE_INT16(scratch[i] & 0xffff), OPND_CREATE_INT8(0)));
                if(scratch[i] > 0xffff) {
                    MINSERT(bb, ins, INSTR_CREATE_movk(drcontext, opnd_create_reg(reg_end), OPND_CREATE_INT16((scratch[i] >> 16) & 0xffff), OPND_CREATE_INT8(16)));
                }
                MINSERT(bb, ins,
                    XINST_CREATE_add(drcontext, opnd_create_reg(reg_ptr), opnd_create_reg(reg_end)));
            } else
#endif
            // increament the buffer pointer with memRefCnt
            MINSERT(bb, ins,
                    XINST_CREATE_add(drcontext, opnd_create_reg(reg_ptr),
                                     OPND_CREATE_INT16(scratch[i])));
            // the end buffer pointer
            vtracer_insert_load_buf_end(drcontext, buf, bb, ins, reg_end);

            // if buffer will not be full, we will skip the heavy update
            // TODO: use JECXZ (x86)/TBZ (arm) for aflag-free comparison & conditional jump
#ifdef AARCH64
            MINSERT(bb, ins, XINST_CREATE_sub(drcontext, opnd_create_reg(reg_ptr), opnd_create_reg(reg_end)));
            // if reg_ptr > reg_end, the top bit of (reg_ptr-reg_end) will be 0
            MINSERT(bb, ins, INSTR_CREATE_tbnz(drcontext, opnd_create_instr(skip_update),
                                 /* If the top bit is still zero, skip the call. */
                                 opnd_create_reg(reg_ptr), OPND_CREATE_INT(63)));
#else
            MINSERT(bb, ins, XINST_CREATE_cmp(drcontext, opnd_create_reg(reg_ptr), opnd_create_reg(reg_end)));
            MINSERT(bb, ins, XINST_CREATE_jump_cond(drcontext, DR_PRED_LT, opnd_create_instr(skip_update)));
#endif
            // get current buffer base
            // reg_ptr = reg_end - buf->buf_size
            MINSERT(bb, ins, XINST_CREATE_move(drcontext, opnd_create_reg(reg_ptr), opnd_create_reg(reg_end)));
#ifdef AARCH64
            // for aarch64, larger buffer size will result in more instrumentation to track
            if(buf->buf_size >= (1<<12)/*max immediate value for sub is 12-bit integer*/) {
                MINSERT(bb, ins, INSTR_CREATE_movz(drcontext, opnd_create_reg(reg_end), OPND_CREATE_INT16(buf->buf_size & 0xffff), OPND_CREATE_INT8(0)));
                if(buf->buf_size > 0xffff) {
                    MINSERT(bb, ins, INSTR_CREATE_movk(drcontext, opnd_create_reg(reg_end), OPND_CREATE_INT16((buf->buf_size >> 16) & 0xffff), OPND_CREATE_INT8(16)));
                }
                MINSERT(bb, ins, XINST_CREATE_sub(drcontext, opnd_create_reg(reg_ptr), opnd_create_reg(reg_end)));
            } else
#endif
            MINSERT(bb, ins, XINST_CREATE_sub(drcontext, opnd_create_reg(reg_ptr), OPND_CREATE_INT32(buf->buf_size)));
            // get current buffer end
            vtracer_insert_load_buf_ptr(drcontext, buf, bb, ins, reg_end);
            // insert cleancall for updating the buffered trace
            dr_insert_clean_call(drcontext, bb, ins, (void*)buf->full_cb, false, 3, opnd_create_reg(reg_ptr), opnd_create_reg(reg_end), OPND_CREATE_INTPTR(buf->user_data_full));
            // fast clear the buffer with the restored buf base in reg_ptr
            dr_insert_write_raw_tls(drcontext, bb, ins, buf->tls_seg, buf->tls_offs,
                            reg_ptr);
            // skip to here
            MINSERT(bb, ins, skip_update);
        }
    }
    // restore registers if reserved
#if defined(ARM) || defined(AARCH64)
    MINSERT(bb, ins, skip_to_end);
    UNRESERVE_REG(drcontext, bb, ins, reg_end);
#else
    UNRESERVE_REG(drcontext, bb, ins, reg_end);
    MINSERT(bb, ins, skip_to_end);
#endif
    UNRESERVE_REG(drcontext, bb, ins, reg_ptr);
#ifndef AARCH64
    UNRESERVE_AFLAGS(drcontext, bb, ins);
#endif
}

static dr_emit_flags_t
event_basic_block(void *drcontext, void *tag, instrlist_t *bb, bool for_trace, bool translating, OUT void **user_data)
{
    // quick return when there are no entries exist
    if(!any_bufs_created) {
        return DR_EMIT_DEFAULT;
    }
    unsigned int i;
    instr_t *instr;
    int num_instructions = 0;
    ushort* scratch = (ushort*)dr_thread_alloc(drcontext, sizeof(ushort)*clients.entries);
    // clear the scratch for accumulation
    for (i = 0; i < clients.entries; ++i) {
        vtrace_buffer_t *buf = (vtrace_buffer_t*)drvector_get_entry(&clients, i);
        if (buf != NULL && buf->full_cb!=NULL) {
            DR_ASSERT(buf->fill_num_cb!=NULL);
            scratch[i] = 0;
        }
    }
    // estimate the future fill in slot numbers of each buffer
    for (instr = instrlist_first(bb); instr != NULL; instr = instr_get_next(instr)) {
        if(!instr_is_app(instr) || instr_is_ignorable(instr)) continue;
        num_instructions++;
        for (i = 0; i < clients.entries; ++i) {
            vtrace_buffer_t *buf = (vtrace_buffer_t*)drvector_get_entry(&clients, i);
            if (buf != NULL && buf->full_cb!=NULL) {
                scratch[i] += buf->fill_num_cb(drcontext, instr, buf->user_data_fill_num);
            }
        }
    }
    // quick return if it doesn't have any application instructions
    if(num_instructions==0) {
        dr_thread_free(drcontext, scratch, sizeof(ushort)*clients.entries);
        return DR_EMIT_DEFAULT;
    }
    // all instrumentation is insert into the beginning of the basic block
    instr_t* insert_pt = instrlist_first(bb);
    bool enable_check = false;
    for (i = 0; i < clients.entries; ++i) {
        vtrace_buffer_t *buf = (vtrace_buffer_t*)drvector_get_entry(&clients, i);
        if (buf != NULL && buf->full_cb!=NULL && scratch[i]>0) {
            enable_check = true;
        }
    }
    // if sampling enabled, we need to insert instruction counts
    if (enable_sampling) {
        // TODO: inline these simple cleancalls for lower overhead
        if(enable_check) {
            dr_insert_clean_call(drcontext, bb, insert_pt, (void *)bb_update_and_check, false, 1, OPND_CREATE_INT32(num_instructions));
        } else {
            dr_insert_clean_call(drcontext, bb, insert_pt, (void *)bb_update, false, 1, OPND_CREATE_INT32(num_instructions));
        }
    }
    if (enable_check) {
        insert_buf_check(drcontext, bb, insert_pt, scratch);
    }
    dr_thread_free(drcontext, scratch, sizeof(ushort)*clients.entries);
    return DR_EMIT_DEFAULT;
}

void
event_thread_init(void *drcontext)
{
    // allocate raw tls field for fast update for sampling
    if(tls_idx!=-1) {
        ins_per_thread_t *pt = (ins_per_thread_t *)dr_thread_alloc(drcontext, sizeof(ins_per_thread_t));
        pt->numInsBuff = (byte*)dr_get_dr_segment_base(tls_seg);
        BUF_PTR(pt->numInsBuff, tls_offs) = 0;
        drmgr_set_tls_field(drcontext, tls_idx, (void *)pt);
    }
    // allcoate tls field for each buffer
    unsigned int i;
    for (i = 0; i < clients.entries; ++i) {
        per_thread_t *data;
        vtrace_buffer_t *buf = (vtrace_buffer_t*)drvector_get_entry(&clients, i);
        if (buf != NULL) {
            data = per_thread_init(drcontext, buf);
            drmgr_set_tls_field(drcontext, buf->tls_idx, data);
            BUF_PTR(data->seg_base, buf->tls_offs) = data->cli_base;
            BUF_PTR(data->seg_base,
                    buf->tls_offs + sizeof(void *) * TRACE_BUF_TLS_OFFS_BUF_END) =
                data->cli_base + buf->buf_size;
            // BUF_PTR(data->seg_base,
            //         buf->tls_offs + sizeof(void *) * TRACE_BUF_TLS_OFFS_BUF_BASE) =
            //     data->cli_base;
        }
    }
}

void
event_thread_exit(void *drcontext)
{
    if(tls_idx!=-1) {
        // free raw tls field for fast update for sampling
        ins_per_thread_t *pt = (ins_per_thread_t *)drmgr_get_tls_field(drcontext, tls_idx);
        dr_thread_free(drcontext, pt, sizeof(ins_per_thread_t));
    }
    // free tls field of each buffer
    unsigned int i;
    for (i = 0; i < clients.entries; ++i) {
        vtrace_buffer_t *buf = (vtrace_buffer_t*)drvector_get_entry(&clients, i);
        if (buf != NULL) {
            per_thread_t *data = (per_thread_t*)drmgr_get_tls_field(drcontext, buf->tls_idx);
            if (buf->full_cb!=NULL) {
                byte *cli_ptr = BUF_PTR(data->seg_base, buf->tls_offs);
                buf->full_cb(data->cli_base, cli_ptr, buf->user_data_full);
            }
            dr_raw_mem_free(data->buf_base, data->total_size);
            dr_thread_free(drcontext, data, sizeof(per_thread_t));
        }
    }
}

static per_thread_t *
per_thread_init(void *drcontext, vtrace_buffer_t *buf)
{
    size_t page_size = dr_page_size();
    per_thread_t *per_thread = (per_thread_t*)dr_thread_alloc(drcontext, sizeof(per_thread_t));
    byte *ret;
    /* Keep seg_base in a per-thread data structure so we can get the TLS
     * slot and find where the pointer points to in the buffer.
     */
    per_thread->seg_base = (byte*)dr_get_dr_segment_base(buf->tls_seg);
    /* We construct a buffer right before a fault by allocating as
     * many pages as needed to fit the buffer, plus another read-only
     * page. Then, we return an address such that we have exactly
     * buf_size bytes usable before we hit the ro page.
     */
    /* We no longer use the fault for updating the buffer, 
     * so the extra page will not be allocated and protected.
     */
    per_thread->total_size = ALIGN_FORWARD(buf->buf_size, page_size);
    ret = (byte*)dr_raw_mem_alloc(per_thread->total_size, DR_MEMPROT_READ | DR_MEMPROT_WRITE,
                           NULL);
    per_thread->buf_base = ret;
    per_thread->cli_base = ret + ALIGN_FORWARD(buf->buf_size, page_size) - buf->buf_size;
#ifdef VTRACER_DEBUG
    per_thread->scratch = 0;
#endif
    return per_thread;
}

bool
vtracer_init(void)
{

    if (!drmgr_init()) {
        DR_ASSERT_MSG(false, "ERROR: vtracer unable to initialize drmgr");
    }

    drreg_options_t ops = { sizeof(ops), 4 /*max slots needed*/, false };
    if (drreg_init(&ops) != DRREG_SUCCESS) {
        DR_ASSERT_MSG(false, "ERROR: vtracer unable to initialize drreg");
    }
    if (!drutil_init()) {
        DR_ASSERT_MSG(false, "ERROR: vtracer unable to initialize drutil");
    }

    drmgr_priority_t exit_priority = { sizeof(exit_priority),
                                       DRMGR_PRIORITY_NAME_TRACE_BUF_EXIT, NULL, NULL,
                                       DRMGR_PRIORITY_THREAD_EXIT_TRACE_BUF };
    drmgr_priority_t init_priority = { sizeof(init_priority),
                                       DRMGR_PRIORITY_NAME_TRACE_BUF_INIT, NULL, NULL,
                                       DRMGR_PRIORITY_THREAD_INIT_TRACE_BUF };

    if (!drvector_init(&clients, 1, false /*!synch*/, NULL) ||
        !drmgr_register_thread_init_event_ex(event_thread_init, &init_priority) ||
        !drmgr_register_thread_exit_event_ex(event_thread_exit, &exit_priority) ||
        !drmgr_register_bb_instrumentation_event(event_basic_block, NULL, NULL)
        )
        return false;
    tls_idx = -1;
    return true;
}

void
vtracer_exit(void)
{
    drmgr_unregister_thread_init_event(event_thread_init);
    drmgr_unregister_thread_exit_event(event_thread_exit);
    drmgr_unregister_bb_instrumentation_event(event_basic_block);
    if(tls_idx!=-1) {
        drmgr_unregister_tls_field(tls_idx);
        dr_raw_tls_cfree(tls_offs, INSTRACE_TLS_COUNT);
    }
    drvector_delete(&clients);

    drutil_exit();
    drreg_exit();
    drmgr_exit();
}

void
vtracer_insert_load_buf_ptr(void *drcontext, vtrace_buffer_t *buf, instrlist_t *ilist,
                            instr_t *where, reg_id_t buf_ptr)
{
    dr_insert_read_raw_tls(drcontext, ilist, where, buf->tls_seg, buf->tls_offs, buf_ptr);
}

void
vtracer_insert_load_buf_end(void *drcontext, vtrace_buffer_t *buf, instrlist_t *ilist,
                            instr_t *where, reg_id_t buf_ptr)
{
    dr_insert_read_raw_tls(drcontext, ilist, where, buf->tls_seg,
                           buf->tls_offs + sizeof(void *) * TRACE_BUF_TLS_OFFS_BUF_END,
                           buf_ptr);
}

void
vtracer_insert_clear_buf(void *drcontext, vtrace_buffer_t *buf, instrlist_t *ilist,
                            instr_t *where, reg_id_t scratch)
{
    dr_insert_read_raw_tls(drcontext, ilist, where, buf->tls_seg,
                           buf->tls_offs + sizeof(void *) * TRACE_BUF_TLS_OFFS_BUF_END,
                           scratch);
    MINSERT(ilist, where, XINST_CREATE_sub(drcontext, opnd_create_reg(scratch), OPND_CREATE_INT32(buf->buf_size)));
    dr_insert_write_raw_tls(drcontext, ilist, where, buf->tls_seg, buf->tls_offs,
                            scratch);
}

void *
vtracer_get_buf_ptr(void *drcontext, vtrace_buffer_t *buf)
{
    per_thread_t *data = (per_thread_t*)drmgr_get_tls_field(drcontext, buf->tls_idx);
    return BUF_PTR(data->seg_base, buf->tls_offs);
}

void *
vtracer_get_buf_base(void *drcontext, vtrace_buffer_t *buf)
{
    per_thread_t *data = (per_thread_t*)drmgr_get_tls_field(drcontext, buf->tls_idx);
    return data->cli_base;
}

#ifdef VTRACER_DEBUG
void debug_check_foward_num(vtrace_buffer_t *vtrace_buffer, int size) {
    per_thread_t *data = (per_thread_t*)drmgr_get_tls_field(dr_get_current_drcontext(), vtrace_buffer->tls_idx);
    data->scratch -= size;
    DR_ASSERT_MSG(data->scratch >= 0, "Usage Error: Estimated Fill number too small!");
}
#endif

void vtracer_insert_trace_forward(void *drcontext, instr_t *where,
                                  instrlist_t *ilist, int size,
                                  vtrace_buffer_t *vtrace_buffer,
                                  reg_id_t reg_ptr, reg_id_t scratch)
{
#ifdef VTRACER_DEBUG
    dr_insert_clean_call(drcontext, ilist, where, (void *) debug_check_foward_num, false, 2, OPND_CREATE_INTPTR(vtrace_buffer), OPND_CREATE_INT32(size));
#endif

    /* straightforward, just increment buf_ptr */
    MINSERT(
        ilist, where,
        XINST_CREATE_add(drcontext, opnd_create_reg(reg_ptr), OPND_CREATE_INT32(size)));
    dr_insert_write_raw_tls(drcontext, ilist, where, vtrace_buffer->tls_seg, vtrace_buffer->tls_offs,
                            reg_ptr);
}

void vtracer_get_trace_buffer_in_reg(void *drcontext, instr_t *where,
                                     instrlist_t *ilist,
                                     vtrace_buffer_t *vtrace_buffer,
                                     reg_id_t reg_ptr)
{
    dr_insert_read_raw_tls(drcontext, ilist, where, vtrace_buffer->tls_seg, vtrace_buffer->tls_offs, reg_ptr);
}

static bool
vtrace_buf_insert_buf_store_1byte(void *drcontext, instrlist_t *ilist,
                               instr_t *where, reg_id_t buf_ptr, reg_id_t scratch,
                               opnd_t opnd, short offset)
{
    instr_t *instr;
    if (!opnd_is_reg(opnd) && !opnd_is_immed(opnd))
        return false;
    if (opnd_is_immed(opnd)) {
#ifdef X86
        instr =
            XINST_CREATE_store_1byte(drcontext, OPND_CREATE_MEM8(buf_ptr, offset), opnd);
#elif defined(AARCHXX)
        /* this will certainly not fault, so don't set a translation */
        MINSERT(ilist, where,
                XINST_CREATE_load_int(drcontext, opnd_create_reg(scratch), opnd));
        instr = XINST_CREATE_store_1byte(drcontext, OPND_CREATE_MEM8(buf_ptr, offset),
                                         opnd_create_reg(scratch));
#else
#    error NYI
#endif
    } else {
        instr =
            XINST_CREATE_store_1byte(drcontext, OPND_CREATE_MEM8(buf_ptr, offset), opnd);
    }
    INSTR_XL8(instr, instr_get_app_pc(where));
    MINSERT(ilist, where, instr);
    return true;
}

static bool
vtrace_buf_insert_buf_store_2bytes(void *drcontext, instrlist_t *ilist,
                                instr_t *where, reg_id_t buf_ptr, reg_id_t scratch,
                                opnd_t opnd, short offset)
{
    instr_t *instr;
    if (!opnd_is_reg(opnd) && !opnd_is_immed(opnd))
        return false;
    if (opnd_is_immed(opnd)) {
#ifdef X86
        instr = XINST_CREATE_store_2bytes(drcontext, OPND_CREATE_MEM16(buf_ptr, offset),
                                          opnd);
#elif defined(AARCHXX)
        /* this will certainly not fault, so don't set a translation */
        MINSERT(ilist, where,
                XINST_CREATE_load_int(drcontext, opnd_create_reg(scratch), opnd));
        instr = XINST_CREATE_store_2bytes(drcontext, OPND_CREATE_MEM16(buf_ptr, offset),
                                          opnd_create_reg(scratch));
#else
#    error NYI
#endif
    } else {
        instr = XINST_CREATE_store_2bytes(drcontext, OPND_CREATE_MEM16(buf_ptr, offset),
                                          opnd);
    }
    INSTR_XL8(instr, instr_get_app_pc(where));
    MINSERT(ilist, where, instr);
    return true;
}

#if defined(X86_64) || defined(AARCH64)
/* only valid on platforms where OPSZ_PTR != OPSZ_4 */
static bool
vtrace_buf_insert_buf_store_4bytes(void *drcontext, instrlist_t *ilist,
                                instr_t *where, reg_id_t buf_ptr, reg_id_t scratch,
                                opnd_t opnd, short offset)
{
    instr_t *instr;
    if (!opnd_is_reg(opnd) && !opnd_is_immed(opnd))
        return false;
    if (opnd_is_immed(opnd)) {
#    ifdef X86_64
        instr = XINST_CREATE_store(drcontext, OPND_CREATE_MEM32(buf_ptr, offset), opnd);
#    elif defined(AARCH64)
        /* this will certainly not fault, so don't set a translation */
        instrlist_insert_mov_immed_ptrsz(drcontext, opnd_get_immed_int(opnd),
                                         opnd_create_reg(scratch), ilist, where, NULL,
                                         NULL);
        instr = XINST_CREATE_store(drcontext, OPND_CREATE_MEM32(buf_ptr, offset),
                                   opnd_create_reg(scratch));
#    endif
    } else {
        instr = XINST_CREATE_store(drcontext, OPND_CREATE_MEM32(buf_ptr, offset), opnd);
    }
    INSTR_XL8(instr, instr_get_app_pc(where));
    MINSERT(ilist, where, instr);
    return true;
}
#endif

static bool
vtrace_buf_insert_buf_store_ptrsz(void *drcontext, instrlist_t *ilist,
                               instr_t *where, reg_id_t buf_ptr, reg_id_t scratch,
                               opnd_t opnd, short offset)
{
    VTRACER_LOG(SUMMARY, "enter vtrace_buf_insert_buf_store_ptrsz\n");
    if (!opnd_is_reg(opnd) && !opnd_is_immed(opnd))
        return false;
    if (opnd_is_immed(opnd)) {
        instr_t *first, *last;
        ptr_int_t immed = opnd_get_immed_int(opnd);
#ifdef X86
        instrlist_insert_mov_immed_ptrsz(drcontext, immed,
                                         OPND_CREATE_MEMPTR(buf_ptr, offset), ilist,
                                         where, &first, &last);
        for (;; first = instr_get_next(first)) {
            INSTR_XL8(first, instr_get_app_pc(where));
            if (last == NULL || first == last)
                break;
        }
#elif defined(AARCHXX)
        instr_t *instr;
        instrlist_insert_mov_immed_ptrsz(drcontext, immed, opnd_create_reg(scratch),
                                         ilist, where, &first, &last);
        instr = XINST_CREATE_store(drcontext, OPND_CREATE_MEMPTR(buf_ptr, offset),
                                   opnd_create_reg(scratch));
        INSTR_XL8(instr, instr_get_app_pc(where));
        MINSERT(ilist, where, instr);
#else
#    error NYI
#endif
    } else {
        instr_t *instr =
            XINST_CREATE_store(drcontext, OPND_CREATE_MEMPTR(buf_ptr, offset), opnd);
        INSTR_XL8(instr, instr_get_app_pc(where));
        MINSERT(ilist, where, instr);
    }
    VTRACER_LOG(SUMMARY, "exit vtrace_buf_insert_buf_store_ptrsz\n");
    return true;
}


bool
vtrace_buf_insert_buf_store(void *drcontext, instrlist_t *ilist,
                         instr_t *where, reg_id_t buf_ptr, reg_id_t scratch, opnd_t opnd,
                         opnd_size_t opsz, short offset)
{
    VTRACER_LOG(SUMMARY, "enter vtrace_buf_insert_buf_store\n");
    switch (opsz) {
#if defined(AARCH64)
    // case OPSZ_2b for optional shift(lsl lsr...) in arm, treat it as 1 bytes e.g. add    %sp $0x0000 **lsl** $0x00 -> %x0
    case OPSZ_2b:
    // case OPSZ_3b for SUBS (extended register) imm3 in arm, treat it as 1 bytes e.g. subs   %x1 %x2 lsl **$0x00** -> %xzr
    case OPSZ_3b:
    // case OPSZ_4b for CSEL cond in arm, treat it as 1 bytes e.g. csel   %x1 %x22 **ne** -> %x22
    case OPSZ_4b:
    // case OPSZ_5b for shift(lsl lsr...) amount in arm, treat it as 1 bytes e.g. add    %sp $0x0000 lsl **$0x00** -> %x0
    case OPSZ_5b:
    // case OPSZ_6b for SUBS (shifted register) imm6 in arm, treat it as 1 bytes e.g. subs   %x1 %x2 lsl **$0x00** -> %xzr
    case OPSZ_6b:
#endif
    case OPSZ_1:
        return vtrace_buf_insert_buf_store_1byte(drcontext, ilist, where, buf_ptr,
                                              scratch, opnd, offset);
#if defined(AARCH64)
    // case OPSZ_12b for imm in arm, treat it as 2 bytes e.g. add    %sp $0x0000 lsl $0x00 -> %x0
    case OPSZ_12b:
#endif
    case OPSZ_2:
        return vtrace_buf_insert_buf_store_2bytes(drcontext, ilist, where, buf_ptr,
                                               scratch, opnd, offset);
#if defined(X86_64) || defined(AARCH64)
    case OPSZ_4:
        return vtrace_buf_insert_buf_store_4bytes(drcontext, ilist, where, buf_ptr,
                                               scratch, opnd, offset);
#endif
    case OPSZ_PTR:
        return vtrace_buf_insert_buf_store_ptrsz(drcontext, ilist, where, buf_ptr,
                                              scratch, opnd, offset);
    default: assert(false); return false;
    }
    VTRACER_LOG(SUMMARY, "exit vtrace_buf_insert_buf_store\n");
}

#ifdef VTRACER_DEBUG
void debug_print(void* src, int offset) {
    dr_fprintf(STDOUT, "src=%lx, offset=%d, mem=%p\n", src, offset, (uint8_t*)src+offset);
    dr_fprintf(STDOUT, "         memval=%d\n", *((uint8_t*)((uint8_t*)src+offset)));
}
#endif

void insert_load(void *drcontext, instrlist_t *ilist, instr_t *where, reg_id_t dst,
            reg_id_t src, int offset, opnd_size_t opsz)
{
    VTRACER_LOG(SUMMARY, "enter insert_load\n");
#ifdef VTRACER_DEBUG_DETAIL
    dr_insert_clean_call(drcontext, ilist, where, (void*)debug_print, false, 2, opnd_create_reg(src), OPND_CREATE_INT32(offset));
#endif
    switch (opsz) {
    case OPSZ_1:
        MINSERT(ilist, where,
                XINST_CREATE_load_1byte(
                    drcontext, opnd_create_reg(reg_resize_to_opsz(dst, opsz)),
                    opnd_create_base_disp(src, DR_REG_NULL, 0, offset, opsz)));
        break;
    case OPSZ_2:
        MINSERT(ilist, where,
                XINST_CREATE_load_2bytes(
                    drcontext, opnd_create_reg(reg_resize_to_opsz(dst, opsz)),
                    opnd_create_base_disp(src, DR_REG_NULL, 0, offset, opsz)));
        break;
    case OPSZ_4:
#if defined(X86_64) || defined(AARCH64)
    case OPSZ_8:
#endif
        MINSERT(ilist, where,
                XINST_CREATE_load(drcontext,
                                  opnd_create_reg(reg_resize_to_opsz(dst, opsz)),
                                  opnd_create_base_disp(src, DR_REG_NULL, 0, offset, opsz)));
        break;
    default: DR_ASSERT(false); break;
    }
    VTRACER_LOG(SUMMARY, "exit insert_load\n");
}

template <int size>
inline __attribute__((always_inline)) void
insert_trace_value_in_mem(void *drcontext, instrlist_t *ilist, instr_t *where,
                          ushort offset, reg_id_t reg_addr, reg_id_t reg_ptr) {
  VTRACER_LOG(SUMMARY, "enter insert_trace_value_in_mem: size=%d\n", size);
  switch (size) {
  case 1: {
    reg_id_t reg_val = reg_resize_to_opsz(reg_addr, OPSZ_1);
    insert_load(drcontext, ilist, where, reg_val, reg_addr, 0, OPSZ_1);
    vtrace_buf_insert_buf_store(drcontext, ilist, where, reg_ptr,
                               DR_REG_NULL, opnd_create_reg(reg_val), OPSZ_1,
                               offset);
    break;
  }
  case 2: {
    reg_id_t reg_val = reg_resize_to_opsz(reg_addr, OPSZ_2);
    insert_load(drcontext, ilist, where, reg_val, reg_addr, 0, OPSZ_2);
    vtrace_buf_insert_buf_store(drcontext, ilist, where, reg_ptr,
                               DR_REG_NULL, opnd_create_reg(reg_val), OPSZ_2,
                               offset);
    break;
  }
  case 4: {
    reg_id_t reg_val = reg_resize_to_opsz(reg_addr, OPSZ_4);
    insert_load(drcontext, ilist, where, reg_val, reg_addr, 0, OPSZ_4);
    vtrace_buf_insert_buf_store(drcontext, ilist, where, reg_ptr,
                               DR_REG_NULL, opnd_create_reg(reg_val), OPSZ_4,
                               offset);
    break;
  }
  case 8: {
    insert_load(drcontext, ilist, where, reg_addr, reg_addr, 0, OPSZ_8);
    vtrace_buf_insert_buf_store(drcontext, ilist, where, reg_ptr,
                               DR_REG_NULL, opnd_create_reg(reg_addr), OPSZ_8,
                               offset);
    break;
  }
  case 16: {
    reg_id_t scratch;
    RESERVE_REG(drcontext, ilist, where, NULL, scratch);
    // 0-7B
    insert_load(drcontext, ilist, where, scratch, reg_addr, 0, OPSZ_8);
    vtrace_buf_insert_buf_store(drcontext, ilist, where, reg_ptr,
                               DR_REG_NULL, opnd_create_reg(scratch), OPSZ_8,
                               offset);
    // 8-15B
    insert_load(drcontext, ilist, where, scratch, reg_addr, 8, OPSZ_8);
    vtrace_buf_insert_buf_store(drcontext, ilist, where, reg_ptr,
                               DR_REG_NULL, opnd_create_reg(scratch), OPSZ_8,
                               offset + 8);
    UNRESERVE_REG(drcontext, ilist, where, scratch);
    break;
  }
  case 32: {
    reg_id_t scratch;
    RESERVE_REG(drcontext, ilist, where, NULL, scratch);
    // 0-7B
    insert_load(drcontext, ilist, where, scratch, reg_addr, 0, OPSZ_8);
    vtrace_buf_insert_buf_store(drcontext, ilist, where, reg_ptr,
                               DR_REG_NULL, opnd_create_reg(scratch), OPSZ_8,
                               offset);
    // 8-15B
    insert_load(drcontext, ilist, where, scratch, reg_addr, 8, OPSZ_8);
    vtrace_buf_insert_buf_store(drcontext, ilist, where, reg_ptr,
                               DR_REG_NULL, opnd_create_reg(scratch), OPSZ_8,
                               offset + 8);
    // 16-23B
    insert_load(drcontext, ilist, where, scratch, reg_addr, 16, OPSZ_8);
    vtrace_buf_insert_buf_store(drcontext, ilist, where, reg_ptr,
                               DR_REG_NULL, opnd_create_reg(scratch), OPSZ_8,
                               offset + 16);
    // 24-31B
    insert_load(drcontext, ilist, where, scratch, reg_addr, 24, OPSZ_8);
    vtrace_buf_insert_buf_store(drcontext, ilist, where, reg_ptr,
                               DR_REG_NULL, opnd_create_reg(scratch), OPSZ_8,
                               offset + 24);
    UNRESERVE_REG(drcontext, ilist, where, scratch);
    break;
  }
  case 64: {
    reg_id_t scratch;
    RESERVE_REG(drcontext, ilist, where, NULL, scratch);
    // 0-7B
    insert_load(drcontext, ilist, where, scratch, reg_addr, 0, OPSZ_8);
    vtrace_buf_insert_buf_store(drcontext, ilist, where, reg_ptr,
                               DR_REG_NULL, opnd_create_reg(scratch), OPSZ_8,
                               offset);
    // 8-15B
    insert_load(drcontext, ilist, where, scratch, reg_addr, 8, OPSZ_8);
    vtrace_buf_insert_buf_store(drcontext, ilist, where, reg_ptr,
                               DR_REG_NULL, opnd_create_reg(scratch), OPSZ_8,
                               offset + 8);
    // 16-23B
    insert_load(drcontext, ilist, where, scratch, reg_addr, 16, OPSZ_8);
    vtrace_buf_insert_buf_store(drcontext, ilist, where, reg_ptr,
                               DR_REG_NULL, opnd_create_reg(scratch), OPSZ_8,
                               offset + 16);
    // 24-31B
    insert_load(drcontext, ilist, where, scratch, reg_addr, 24, OPSZ_8);
    vtrace_buf_insert_buf_store(drcontext, ilist, where, reg_ptr,
                               DR_REG_NULL, opnd_create_reg(scratch), OPSZ_8,
                               offset + 24);
    // 32-39B
    insert_load(drcontext, ilist, where, scratch, reg_addr, 32, OPSZ_8);
    vtrace_buf_insert_buf_store(drcontext, ilist, where, reg_ptr,
                               DR_REG_NULL, opnd_create_reg(scratch), OPSZ_8,
                               offset + 32);
    // 40-47B
    insert_load(drcontext, ilist, where, scratch, reg_addr, 40, OPSZ_8);
    vtrace_buf_insert_buf_store(drcontext, ilist, where, reg_ptr,
                               DR_REG_NULL, opnd_create_reg(scratch), OPSZ_8,
                               offset + 40);
    // 48-55B
    insert_load(drcontext, ilist, where, scratch, reg_addr, 48, OPSZ_8);
    vtrace_buf_insert_buf_store(drcontext, ilist, where, reg_ptr,
                               DR_REG_NULL, opnd_create_reg(scratch), OPSZ_8,
                               offset + 48);
    // 56-63B
    insert_load(drcontext, ilist, where, scratch, reg_addr, 56, OPSZ_8);
    vtrace_buf_insert_buf_store(drcontext, ilist, where, reg_ptr,
                               DR_REG_NULL, opnd_create_reg(scratch), OPSZ_8,
                               offset + 56);
    UNRESERVE_REG(drcontext, ilist, where, scratch);
    break;
  }
  }
  VTRACER_LOG(SUMMARY, "exit insert_trace_value_in_mem: size=%d\n", size);
}

/* Trace Memory Operands */
template <int sz>
void insert_trace_for_mem(void *drcontext, instrlist_t *ilist, instr_t *where, opnd_t mem_opnd, ushort offset, 
                          reg_id_t reg_ptr, reg_id_t scratch) {
  VTRACER_LOG(SUMMARY, "enter insert_trace_for_mem: size=%d\n", sz);
  reg_id_t free_reg;
  
  // check if reg is dirty
  if (opnd_uses_reg(mem_opnd, reg_ptr)) {
        DR_ASSERT_MSG(0,
                "insert_trace_for_mem: reg_ptr should not overlap with register used in the target memory operand!");
  }

  if (opnd_uses_reg(mem_opnd, scratch) && drreg_get_app_value(drcontext, ilist, where, scratch, scratch) != DRREG_SUCCESS) {
        DR_ASSERT_MSG(0,
                "insert_trace_for_mem: scratch register is overlapped with register used in the target memory operand but it fails to restore the app value!");
  }

  drvector_t allowed;
  getUnusedRegEntry(&allowed, mem_opnd);

  RESERVE_REG(drcontext, ilist, where, &allowed, free_reg);
  drvector_delete(&allowed);
  // calculate the memory address
  if (!drutil_insert_get_mem_addr(drcontext, ilist, where, mem_opnd,
                                  free_reg /*addr*/, scratch /*scratch*/)) {
    DR_ASSERT_MSG(0,
                "insert_trace_for_mem drutil_insert_get_mem_addr failed!");
  }
  // insert loads from the given memory and store into the buffered trace
  insert_trace_value_in_mem<sz>(drcontext, ilist, where, offset, 
                                free_reg /*addr*/, reg_ptr /*ptr*/);
  UNRESERVE_REG(drcontext, ilist, where, free_reg);
  VTRACER_LOG(SUMMARY, "exit insert_trace_for_mem: size=%d\n", sz);
}

/* Trace Imm Operands */
void insert_trace_for_imm(void *drcontext, instrlist_t *ilist, instr_t *where, opnd_t imm_opnd, ushort offset, 
                          opnd_size_t opsz, reg_id_t reg_ptr, reg_id_t scratch) {
  vtrace_buf_insert_buf_store(drcontext, ilist, where, reg_ptr, scratch,
                             imm_opnd, opsz,
                             offset);
}

/* Trace Reg Operands */
void insert_trace_value_in_reg(void *drcontext, instrlist_t *ilist,
                               instr_t *where, opnd_size_t opsz, ushort offset, 
                               reg_id_t reg_val, reg_id_t reg_ptr) {
  // store the register value into the buffer
  vtrace_buf_insert_buf_store(drcontext, ilist, where, reg_ptr, DR_REG_NULL,
                             opnd_create_reg(reg_resize_to_opsz(reg_val, opsz)), opsz,
                             offset);
}

template <int size>
void insert_trace_value_in_simd(void *drcontext, instrlist_t *ilist, instr_t *where, 
                                size_t offset, reg_id_t reg_simd, reg_id_t reg_ptr, reg_id_t scratch) {
  VTRACER_LOG(SUMMARY, "enter insert_trace_value_in_simd: size=%d\n", size);
  // store the value of the simd register into the buffered trace
#ifdef X86
    switch(size) {
	    case 1:
          assert(reg_is_strictly_xmm(reg_simd) || reg_is_mmx(reg_simd));
          MINSERT(ilist, where,
                  INSTR_CREATE_pextrb(drcontext,
                                          opnd_create_base_disp(reg_ptr,
                                                                DR_REG_NULL, 0,
                                                                offset, OPSZ_1),
                                          opnd_create_reg(reg_simd), opnd_create_immed_int(0, OPSZ_1)));
          break;
        case 2:
          assert(reg_is_strictly_xmm(reg_simd) || reg_is_mmx(reg_simd));
          MINSERT(ilist, where,
                  INSTR_CREATE_pextrw(drcontext,
                                          opnd_create_base_disp(reg_ptr,
                                                                DR_REG_NULL, 0,
                                                                offset, OPSZ_2),
                                          opnd_create_reg(reg_simd), opnd_create_immed_int(0, OPSZ_1)));
          break;
        case 4:
          assert(reg_is_strictly_xmm(reg_simd) || reg_is_mmx(reg_simd));
          MINSERT(ilist, where,
                  INSTR_CREATE_movd(drcontext,
                                          opnd_create_base_disp(reg_ptr,
                                                                DR_REG_NULL, 0,
                                                                offset, OPSZ_4),
                                          opnd_create_reg(reg_simd)));
          break;
        case 8:
            assert(reg_is_strictly_xmm(reg_simd) || reg_is_mmx(reg_simd));
            MINSERT(ilist, where,
                  INSTR_CREATE_movq(drcontext,
                                          opnd_create_base_disp(reg_ptr,
                                                                DR_REG_NULL, 0,
                                                                offset, OPSZ_8),
                                          opnd_create_reg(reg_simd)));
            break;
        case 16:
            assert(reg_is_strictly_xmm(reg_simd));
            MINSERT(ilist, where,
                  INSTR_CREATE_vmovdqu(drcontext,
                                          opnd_create_base_disp(reg_ptr,
                                                                DR_REG_NULL, 0,
                                                                offset, OPSZ_16),
                                          opnd_create_reg(reg_simd)));
            break;
        case 32:
            assert(reg_is_strictly_ymm(reg_simd));
            MINSERT(ilist, where,
                  INSTR_CREATE_vmovdqu(drcontext,
                                          opnd_create_base_disp(reg_ptr,
                                                                DR_REG_NULL, 0,
                                                                offset, OPSZ_32),
                                          opnd_create_reg(reg_simd)));
            break;
        default:
            DR_ASSERT_MSG(0,
                        "insert_trace_value_in_simd: Unknown simd size!");
    }
#elif defined(ARM)
    DR_ASSERT_MSG(0,
                "insert_trace_value_in_simd not implemented!");
#elif defined(AARCH64)
    int i = 0;
    switch(size) {
	    case 1:
          MINSERT(ilist, where,
                  XINST_CREATE_store_simd(drcontext,
                                          opnd_create_base_disp_aarch64(reg_ptr,
                                                                DR_REG_NULL, (dr_extend_type_t) i,
                                                                false, offset, (dr_opnd_flags_t) i, OPSZ_1),
                                          opnd_create_reg(reg_simd)));
          break;
        case 2:
          MINSERT(ilist, where,
                  XINST_CREATE_store_simd(drcontext,
                                          opnd_create_base_disp_aarch64(reg_ptr,
                                                                DR_REG_NULL, (dr_extend_type_t) i,
                                                                false, offset, (dr_opnd_flags_t) i, OPSZ_2),
                                          opnd_create_reg(reg_simd)));
          break;
        case 4:
          MINSERT(ilist, where,
                  XINST_CREATE_store_simd(drcontext,
                                          opnd_create_base_disp_aarch64(reg_ptr,
                                                                DR_REG_NULL, (dr_extend_type_t) i,
                                                                false, offset, (dr_opnd_flags_t) i, OPSZ_4),
                                          opnd_create_reg(reg_simd)));
          break;
        case 8:
            MINSERT(ilist, where, XINST_CREATE_move(drcontext, opnd_create_reg(scratch), opnd_create_reg(reg_ptr)));
            MINSERT(ilist, where,
                  INSTR_CREATE_str_imm(drcontext,
                                          opnd_create_base_disp_aarch64(scratch,
                                                                DR_REG_NULL, (dr_extend_type_t) i,
                                                                false, offset, (dr_opnd_flags_t) i, OPSZ_8),
                                          opnd_create_reg(reg_simd), opnd_create_reg(scratch), OPND_CREATE_INT(offset)));
            break;
        case 16:
            MINSERT(ilist, where, XINST_CREATE_move(drcontext, opnd_create_reg(scratch), opnd_create_reg(reg_ptr)));
            MINSERT(ilist, where,
                  INSTR_CREATE_str_imm(drcontext,
                                          opnd_create_base_disp_aarch64(scratch,
                                                                DR_REG_NULL, (dr_extend_type_t) i,
                                                                false, offset, (dr_opnd_flags_t) i, OPSZ_16),
                                          opnd_create_reg(reg_simd), opnd_create_reg(scratch), OPND_CREATE_INT(offset)));
            break;
        case 32:
            MINSERT(ilist, where, XINST_CREATE_move(drcontext, opnd_create_reg(scratch), opnd_create_reg(reg_ptr)));
            MINSERT(ilist, where,
                  INSTR_CREATE_str_imm(drcontext,
                                          opnd_create_base_disp_aarch64(scratch,
                                                                DR_REG_NULL, (dr_extend_type_t) i,
                                                                false, offset, (dr_opnd_flags_t) i, OPSZ_32),
                                          opnd_create_reg(reg_simd), opnd_create_reg(scratch), OPND_CREATE_INT(offset)));
            break;
        default:
            DR_ASSERT_MSG(0,
                        "insert_trace_value_in_simd: Unknown simd size!");
    }
#endif
    VTRACER_LOG(SUMMARY, "exit insert_trace_value_in_simd: size=%d\n", size);
}

template <int sz>
/* Trace General Purpose Register Operands */
void insert_trace_for_gpr(void *drcontext, instrlist_t *ilist, instr_t *where,
                          opnd_t gpr_opnd, ushort offset, reg_id_t reg_ptr, reg_id_t scratch) {
  VTRACER_LOG(SUMMARY, "enter insert_trace_for_gpr: sz=%d\n", sz);
  DR_ASSERT_MSG(opnd_is_reg(gpr_opnd),
                "insert_trace_for_gpr should only used for GPR operand!");
  // if there are register used in this operand, we directly use this operand
  reg_id_t reg_used;
  reg_used = opnd_get_reg_used(gpr_opnd, 0);
    DR_ASSERT_MSG(reg_is_gpr(reg_used) 
#ifdef AARCH64  
  || reg_used == DR_REG_XZR || reg_used == DR_REG_WZR
#endif  
  , "insert_trace_for_gpr should only used for GPR operand!");
  // get the operand size to resize the scratch registers
  opnd_size_t opsz = opnd_get_size(gpr_opnd);

  // check if reg is dirty
  if (reg_used == reg_resize_to_opsz(reg_ptr, opsz)) {
      DR_ASSERT_MSG(0,
                "insert_trace_for_gpr opnd cannot use reg_ptr!");
  }

  if (reg_used == reg_resize_to_opsz(scratch, opsz) && drreg_get_app_value(drcontext, ilist, where, scratch, scratch) != DRREG_SUCCESS) {
	dr_fprintf(STDOUT, "scratch = %d, reg_used = %d\n", reg_resize_to_opsz(scratch, opsz), reg_used);
        DR_ASSERT_MSG(0,
                "insert_trace_for_gpr drreg_get_app_value scratch failed!");
  }

  // copy the value in reg_used to reg_val;
  MINSERT(ilist, where,
          XINST_CREATE_move(
              drcontext, opnd_create_reg(reg_resize_to_opsz(scratch, opsz)),
              opnd_create_reg(reg_used)));
  // now the value is in the reserved register, insert stores to the buffered
  // trace
  insert_trace_value_in_reg(drcontext, ilist, where, opsz, offset, scratch,
                                reg_ptr);
  VTRACER_LOG(SUMMARY, "exit insert_trace_for_gpr: sz=%d\n", sz);
}

/* Trace SIMD Register Operands */
template <int sz>
void insert_trace_for_simd(void *drcontext, instrlist_t *ilist, instr_t *where,
                           opnd_t simd_opnd, ushort offset, reg_id_t reg_ptr, reg_id_t scratch) {
  VTRACER_LOG(SUMMARY, "enter insert_trace_for_simd: sz=%d\n", sz);
  // directly store the value in the target SIMD register into the cache
  // Note that the reg_ptr has already been loaded as the buffer pointer, so
  // no need to load again
  reg_id_t reg_simd = opnd_get_reg_used(simd_opnd, 0);
  insert_trace_value_in_simd<sz>(drcontext, ilist, where, offset, reg_simd, reg_ptr, scratch);
  VTRACER_LOG(SUMMARY, "exit insert_trace_for_simd: sz=%d\n", sz);
}

/* TODO: Trace X87 Operands (X86/64 only) */
void insert_trace_for_x87(void *drcontext, instrlist_t *ilist, instr_t *where,
                          opnd_t fp_opnd, reg_id_t reg_ptr) {
#ifdef DEBUG_X87
  DR_ASSERT_MSG(false, "Not Implemented for x87 tracing!");
#else
  VTRACER_LOG(SUMMARY, "Warning: Trivial tracing & detection for X87 instructions are ignored!\n");
#endif
}

void vtracer_insert_trace_val(void *drcontext, instr_t *where,
                              instrlist_t *ilist, opnd_t ref, reg_id_t reg_ptr,
                              reg_id_t scratch, ushort offset)
{
    VTRACER_LOG(SUMMARY, "enter vtracer_insert_trace_val\n");
#ifdef AARCH64
    // BUG in dynamorio: the size query of DR_REG_CNTCVT_EL0 will fail as it has not implemented.
    // Hotfix: early query the type of opnd and register to filter out this buggy case
    if(opnd_is_reg(ref)) {
        reg_id_t reg = opnd_get_reg(ref);
        if(reg == DR_REG_CNTVCT_EL0 || (reg >= DR_REG_NZCV && reg <= DR_REG_FPSR)) {
            VTRACER_LOG(SUMMARY, "exit vtracer_insert_trace_val: not implemented control regiter!\n");
            return;
        }
    }
#endif
    int size = opnd_size_in_bytes(opnd_get_size(ref));
    if(opnd_is_memory_reference(ref)) {
        switch(size) {
            case 1:
                insert_trace_for_mem<1>(drcontext, ilist, where, ref, offset, reg_ptr, scratch);
                break;
            case 2:
                insert_trace_for_mem<2>(drcontext, ilist, where, ref, offset, reg_ptr, scratch);
                break;
            case 4:
                insert_trace_for_mem<4>(drcontext, ilist, where, ref, offset, reg_ptr, scratch);
                break;
            case 8:
                insert_trace_for_mem<8>(drcontext, ilist, where, ref, offset, reg_ptr, scratch);
                break;
            case 16:
                insert_trace_for_mem<16>(drcontext, ilist, where, ref, offset, reg_ptr, scratch);
                break;
            case 32:
                insert_trace_for_mem<32>(drcontext, ilist, where, ref, offset, reg_ptr, scratch);
                break;
            case 64:
                insert_trace_for_mem<64>(drcontext, ilist, where, ref, offset, reg_ptr, scratch);
                break;
            default:
                assert(false && "Unknown memory operand size!");
        }
    } else if(opnd_is_reg(ref)) {
        reg_id_t reg = opnd_get_reg(ref);
#ifdef AARCH64
        if(reg == DR_REG_TPIDRURW || reg == DR_REG_TPIDRURO) {
            VTRACER_LOG(SUMMARY, "exit vtracer_insert_trace_val: reg==DR_REG_TPIDRURW || reg==DR_REG_TPIDRURO\n");
            return;
        }
#endif
        if(reg_is_gpr(reg) 
#ifdef AARCH64        
        || reg == DR_REG_XZR || reg == DR_REG_WZR
#endif
        ) {
            switch(size) {
                case 1:
                    insert_trace_for_gpr<1>(drcontext, ilist, where, ref, offset, reg_ptr, scratch);
                    break;
                case 2:
                    insert_trace_for_gpr<2>(drcontext, ilist, where, ref, offset, reg_ptr, scratch);
                    break;
                case 4:
                    insert_trace_for_gpr<4>(drcontext, ilist, where, ref, offset, reg_ptr, scratch);
                    break;
                case 8:
                    insert_trace_for_gpr<8>(drcontext, ilist, where, ref, offset, reg_ptr, scratch);
                    break;
                default:
                    assert(false && "Unknown GPR operand size!");
            }
        } else if(reg_is_simd(reg)) {
            switch(size) {
                case 1:
                    // vpbroadcast
                    insert_trace_for_simd<1>(drcontext, ilist, where, ref, offset, reg_ptr, scratch);
                    break;
                case 2:
                    // pextrw
                    insert_trace_for_simd<2>(drcontext, ilist, where, ref, offset, reg_ptr, scratch);
                    break;
                case 4:
                    insert_trace_for_simd<4>(drcontext, ilist, where, ref, offset, reg_ptr, scratch);
                    break;
                case 8:
                    insert_trace_for_simd<8>(drcontext, ilist, where, ref, offset, reg_ptr, scratch);
                    break;
                case 16:
                    insert_trace_for_simd<16>(drcontext, ilist, where, ref, offset, reg_ptr, scratch);
                    break;
                case 32:
                    insert_trace_for_simd<32>(drcontext, ilist, where, ref, offset, reg_ptr, scratch);
                    break;
                default:
                    assert(false && "Unknown SIMD operand size!");
            }
        } 
#if !defined(ARM) && !defined(AARCH64)        
        else if(reg_is_fp(reg)) {
            // TODO: Not implemented (as the modern compilers will avoid using heavy x87 instructions)
            insert_trace_for_x87(drcontext, ilist, where, ref, reg_ptr);
        } 
#endif        
        else {
            dr_fprintf(STDERR, "For REG %s\n", get_register_name(reg)); fflush(stdout);
            assert(false && "Unknown register type!");
        }
    } else if(opnd_is_immed(ref)) {
        // get the immediate value and check it statically
        insert_trace_for_imm(drcontext, ilist, where, ref, offset, opnd_get_size(ref), reg_ptr, scratch);
    } else if(opnd_is_pc(ref)) {
        // TODO: Not implemented
        VTRACER_LOG(SUMMARY, "exit vtracer_insert_trace_val: opnd_is_pc not implemented\n");
        return;
    } else {
        assert(false && "Unknown operand type!\n");
    }
    VTRACER_LOG(SUMMARY, "exit vtracer_insert_trace_val: successfully\n");
}

bool instr_is_ignorable(instr_t *ins) {
    int opc = instr_get_opcode(ins);
#ifdef AARCH64
    if(instr_is_exclusive_load(ins) || instr_is_exclusive_store(ins)) {
        return false;
    }
#endif
    switch (opc) {
        case OP_nop:
#ifdef X86
	    case OP_nop_modrm:
#endif

#if defined(AARCH64)
        case OP_isb:
        case OP_ld3:
        case OP_ld3r:
#endif
                return true;
        default:
                return false;
    }

    return false;
}