#include <vector>
#include <stddef.h> /* for offsetof */

#include "vprofile.h"
#include "drmgr.h"
#include "drutil.h"
#include "drvector.h"
#include "utils.h"

#ifdef DEBUG
    #define VPROFILE_DEBUG
#endif

#define TEST_FLAG(flag, mask) (((flag)&(mask))==(mask))

/* Max val size (INT512). */
#define MAX_CLASS_SIZE 64
/* Max number of mem_ref a buffer can have. */
#define MAX_NUM_MEM_REFS 4096

/* constant defines */
#define VPROFILE_TRACE_MASK 0xf
#ifdef X86
/* global TLS implementation */
enum {
    INSTRACE_TLS_OFFS_BUF_PTR,
    INSTRACE_TLS_COUNT, /* total number of TLS slots allocated */
};

typedef struct _per_thread_t {
    std::vector<instr_t*> *instr_clones;
    std::vector<opnd_t*> *opnd_clones;
} per_thread_t;

static int tls_idx;
static reg_id_t tls_seg;
static uint tls_offs;
#endif
struct opnd_info_t {
    uint8_t size;
    uint8_t esize;
    uint16_t opnd_type;
};

union opnd_info_pack_t {
    opnd_info_t info;
    uint32_t packed_info;
};

template<int sz>
struct cache_VCAI_sz_t {
    uint64_t addr;
    opnd_info_pack_t opnd_info;
    int32_t ctxt_hndl;
    void* user_data;
    int8_t val[sz];
};

template<int sz>
struct cache_VCA_sz_t {
    uint64_t addr;
    opnd_info_pack_t opnd_info;
    int32_t ctxt_hndl;
    int8_t val[sz];
};

template<int sz>
struct cache_VCI_sz_t {
    opnd_info_pack_t opnd_info;
    int32_t ctxt_hndl;
    void* user_data;
    int8_t val[sz];
};

template<int sz>
struct cache_VC_sz_t {
    opnd_info_pack_t opnd_info;
    int32_t ctxt_hndl;
    int8_t val[sz];
};

template<int sz>
struct cache_VAI_sz_t {
    uint64_t addr;
    opnd_info_pack_t opnd_info;
    void* user_data;
    int8_t val[sz];
};

template<int sz>
struct cache_VA_sz_t {
    uint64_t addr;
    opnd_info_pack_t opnd_info;
    int8_t val[sz];
};

template<int sz>
struct cache_VI_sz_t {
    opnd_info_pack_t opnd_info;
    void* user_data;
    int8_t val[sz];
};

template<int sz>
struct cache_V_sz_t {
    opnd_info_pack_t opnd_info;
    int8_t val[sz];
};

struct cache_CAI_t {
    uint64_t addr;
    opnd_info_pack_t opnd_info;
    int32_t ctxt_hndl;
    void* user_data;
};

struct cache_CA_t {
    uint64_t addr;
    opnd_info_pack_t opnd_info;
    int32_t ctxt_hndl;
};

struct cache_CI_t {
    opnd_info_pack_t opnd_info;
    int32_t ctxt_hndl;
    void* user_data;
};

struct cache_AI_t {
    uint64_t addr;
    opnd_info_pack_t opnd_info;
    void* user_data;
};

struct cache_A_t {
    uint64_t addr;
    opnd_info_pack_t opnd_info;
};

typedef cache_VCAI_sz_t<MAX_CLASS_SIZE> cache_VCAI_t;
typedef cache_VCI_sz_t<MAX_CLASS_SIZE> cache_VCI_t;
typedef cache_VCA_sz_t<MAX_CLASS_SIZE> cache_VCA_t;
typedef cache_VC_sz_t<MAX_CLASS_SIZE> cache_VC_t;
typedef cache_VAI_sz_t<MAX_CLASS_SIZE> cache_VAI_t;
typedef cache_VA_sz_t<MAX_CLASS_SIZE> cache_VA_t;
typedef cache_VI_sz_t<MAX_CLASS_SIZE> cache_VI_t;
typedef cache_V_sz_t<MAX_CLASS_SIZE> cache_V_t;

typedef struct _client_cb_t {
    void* (*user_data_cb)(void *, instr_t *, instrlist_t *, opnd_t);
    void (*ins_instrument_cb)(void *, instr_t *, instrlist_t *);
    void (*bb_instrument_cb)(void *, instrlist_t *);
} client_cb_t;

static client_cb_t global_client_cb;

static bool (*global_instr_filter)(instr_t *) = VPROFILE_FILTER_ALL_INSTR;

static uint8_t global_flags = VPROFILE_DEFAULT;

/* record the vtrace registered by user*/
static drvector_t vtrace_t_list;
/* A flag to avoid work when no traces were ever created. */
static bool any_traces_created;

#ifdef VPROFILE_DEBUG
file_t gDebug;

void debug_unknown_case(void *drcontext, instr_t * instr, opnd_t opnd) {
    int size = opnd_size_in_bytes(opnd_get_size(opnd));
    dr_fprintf(gDebug, "size=%d\n", size);
    opnd_disassemble(drcontext, opnd, gDebug);
    dr_fprintf(gDebug, "\n");
    instr_disassemble(drcontext, instr, gDebug);
    dr_fprintf(gDebug, "\n");
    dr_fprintf(gDebug, "--------------------------------\n");
    dr_fprintf(gDebug, "\n");
}
#endif

/* help function to get buffer trace */
vtrace_buffer_t* get_trace_buf(vtrace_t *trace, bool is_float, int esize, int size) {
    if (is_float) {
        if(esize==4) {
            switch(size) {
                case 4:
                    return trace->buff_ex[SPx1];
                case 16:
                    return trace->buff_ex[SPx4];
                case 32:
                    return trace->buff_ex[SPx8];
                case 64:
                    return trace->buff_ex[SPx16];
            }
        } else if(esize==8) {
            switch(size) {
                case 8:
                    return trace->buff_ex[DPx1];
                case 16:
                    return trace->buff_ex[DPx2];
                case 32:
                    return trace->buff_ex[DPx4];
                case 64:
                    return trace->buff_ex[DPx8];
            }
        }
    } else {
        // assert(size == esize);
        if(esize == size) {
            switch(size) {
                case 1: return trace->buff_ex[INT8];
                case 2: return trace->buff_ex[INT16];
                case 4: return trace->buff_ex[INT32];
                case 8: return trace->buff_ex[INT64];
                case 16: return trace->buff_ex[INT128];
                case 32: return trace->buff_ex[INT256];
                case 64: return trace->buff_ex[INT512];
            }
        } else if(esize == 1) {
            switch(size) {
                case 16: return trace->buff_ex[INT8x16];
                case 32: return trace->buff_ex[INT8x32];
                case 64: return trace->buff_ex[INT8x64];
            }
        }
    }
    // Unknown trace buffer, alert for bug
#ifdef VPROFILE_DEBUG
    dr_fprintf(gDebug, "ERROR: is_float=%d, esize=%d, size=%d\n", is_float, esize, size);
#endif
    // assert(false && "Should not reach this");
    return NULL;
}

template<uint8_t size, bool with_cct>
inline __attribute__((always_inline)) void insertStoreTraceBuffer(void *drcontext, instrlist_t *bb, instr_t *where, instr_t *insert_where, int32_t slot, opnd_t opnd, vtrace_t *trace, reg_id_t reg_addr, reg_id_t reg_ptr, reg_id_t scratch, bool before, uint32_t type)
{
    if(TEST_FLAG(trace->trace_flag,VPROFILE_TRACE_CCT)) {
        DR_ASSERT_MSG(with_cct, "Usage Error: CCT info required while VPROFILE_COLLECT_CCT is not set in vprofile_init().");
    }

    instr_t* instr = insert_where;

    bool is_float = TEST_OPND_MASK(type, IS_FLOATING);
    int esize = (is_float) ? FloatOperandSizeTable(where, opnd) : IntegerOperandSizeTable(where, opnd);
#ifdef VPROFILE_DEBUG
    if(esize == 0) {
        debug_unknown_case(drcontext, where, opnd);
        // esize = size;
    }
#endif
    DR_ASSERT(esize <= size);

    bool strictly_ordered = TEST_FLAG(trace->trace_flag, VPROFILE_TRACE_STRICTLY_ORDERED);

    vtrace_buffer_t* buf = strictly_ordered ?
            trace->buff : get_trace_buf(trace, is_float, esize, size);
    
    if(!buf) {
#ifdef VPROFILE_DEBUG
        debug_unknown_case(drcontext, where, opnd);
#endif
        return;
    }

    if(!(*(bool (*)(opnd_t, vprofile_src_t))buf->user_data_fill_num)(opnd, (vprofile_src_t)type)) {
        return;
    }

    opnd_info_pack_t opnd_info_pack;
    opnd_info_pack.info.size = size;
    opnd_info_pack.info.esize = esize;
    opnd_info_pack.info.opnd_type = (uint16_t)(type&0xffff);
    DR_ASSERT((type & 0xffff) == type);

    int32_t packed_info = opnd_info_pack.packed_info;
    uint32_t trace_flag = (trace->trace_flag & VPROFILE_TRACE_MASK);

    switch(trace_flag) {
        case VPROFILE_TRACE_VAL_CCT_ADDR_INFO:
        {
            vtracer_get_trace_buffer_in_reg(drcontext, instr, bb, buf, reg_ptr);
            // addr
            if(opnd_is_memory_reference(opnd)) {
                if(!drutil_insert_get_mem_addr(drcontext, bb, instr, opnd, reg_addr/*addr*/, scratch/*scratch*/)) {
                        DR_ASSERT_MSG(false, "InstrumentInsCallback drutil_insert_get_mem_addr failed!");
                }
                vtracer_insert_trace_val(drcontext, instr, bb, opnd_create_reg(reg_addr), reg_ptr, scratch, offsetof(cache_VCAI_t, addr));
            } else if(opnd_is_reg(opnd)) {
                vtracer_insert_trace_val(drcontext, instr, bb, OPND_CREATE_INT32((int32_t)opnd_get_reg(opnd)), reg_ptr, scratch, offsetof(cache_VCAI_t, addr));
            }
            // opnd_info
            vtracer_insert_trace_val(drcontext, instr, bb, OPND_CREATE_INT32(packed_info), reg_ptr, scratch, offsetof(cache_VCAI_t, opnd_info));
            // CCT
            drcctlib_get_context_handle_in_reg(drcontext, bb, instr, slot, reg_addr, scratch);
            vtracer_insert_trace_val(drcontext, instr, bb, opnd_create_reg(reg_addr), reg_ptr, scratch, offsetof(cache_VCAI_t, ctxt_hndl));
            // info, where is not for trace
            opnd_t info = OPND_CREATE_INTPTR((*global_client_cb.user_data_cb)(drcontext, where, bb, opnd));
            vtracer_insert_trace_val(drcontext, instr, bb, info, reg_ptr, scratch, offsetof(cache_VCAI_t, user_data));
            // value
            vtracer_insert_trace_val(drcontext, instr, bb, opnd, reg_ptr, scratch, offsetof(cache_VCAI_t, val));
            // update buf ptr
            if(strictly_ordered) {
                vtracer_insert_trace_forward(drcontext, instr, bb, sizeof(cache_VCAI_t), buf, reg_ptr, scratch);
            } else {
                vtracer_insert_trace_forward(drcontext, instr, bb, sizeof(cache_VCAI_sz_t<size>), buf, reg_ptr, scratch);
            }
            break;
        }
        case VPROFILE_TRACE_VAL_CCT_ADDR:
        {
            vtracer_get_trace_buffer_in_reg(drcontext, instr, bb, buf, reg_ptr);
            // addr
            if(opnd_is_memory_reference(opnd)) {
                if(!drutil_insert_get_mem_addr(drcontext, bb, instr, opnd, reg_addr/*addr*/, scratch/*scratch*/)) {
                        DR_ASSERT_MSG(false, "InstrumentInsCallback drutil_insert_get_mem_addr failed!");
                }
                vtracer_insert_trace_val(drcontext, instr, bb, opnd_create_reg(reg_addr), reg_ptr, scratch, offsetof(cache_VCA_t, addr));
            } else if(opnd_is_reg(opnd)) {
                vtracer_insert_trace_val(drcontext, instr, bb, OPND_CREATE_INT32(opnd_get_reg(opnd)), reg_ptr, scratch, offsetof(cache_VCA_t, addr));
            }
            // opnd_info
            vtracer_insert_trace_val(drcontext, instr, bb, OPND_CREATE_INT32(packed_info), reg_ptr, scratch, offsetof(cache_VCA_t, opnd_info));
            // CCT
            drcctlib_get_context_handle_in_reg(drcontext, bb, instr, slot, reg_addr, scratch);
            vtracer_insert_trace_val(drcontext, instr, bb, opnd_create_reg(reg_addr), reg_ptr, scratch, offsetof(cache_VCA_t, ctxt_hndl));
            // value
            vtracer_insert_trace_val(drcontext, instr, bb, opnd, reg_ptr, scratch, offsetof(cache_VCA_t, val));
            // update buf ptr
            if(strictly_ordered) {
                vtracer_insert_trace_forward(drcontext, instr, bb, sizeof(cache_VCA_t), buf, reg_ptr, scratch);
            } else {
                vtracer_insert_trace_forward(drcontext, instr, bb, sizeof(cache_VCA_sz_t<size>), buf, reg_ptr, scratch);
            }
            break;
        }
        case VPROFILE_TRACE_VAL_CCT_INFO:
        {
            vtracer_get_trace_buffer_in_reg(drcontext, instr, bb, buf, reg_ptr);
            // opnd_info
            vtracer_insert_trace_val(drcontext, instr, bb, OPND_CREATE_INT32(packed_info), reg_ptr, scratch, offsetof(cache_VCI_t, opnd_info));
            // CCT
            drcctlib_get_context_handle_in_reg(drcontext, bb, instr, slot, reg_addr, scratch);
            vtracer_insert_trace_val(drcontext, instr, bb, opnd_create_reg(reg_addr), reg_ptr, scratch, offsetof(cache_VCI_t, ctxt_hndl));
            // info, where is not for trace
            opnd_t info = OPND_CREATE_INTPTR((*global_client_cb.user_data_cb)(drcontext, where, bb, opnd));
            vtracer_insert_trace_val(drcontext, instr, bb, info, reg_ptr, scratch, offsetof(cache_VCI_t, user_data));
            // value
            vtracer_insert_trace_val(drcontext, instr, bb, opnd, reg_ptr, scratch, offsetof(cache_VCI_t, val));
            // update buf ptr
            if(strictly_ordered) {
                vtracer_insert_trace_forward(drcontext, instr, bb, sizeof(cache_VCI_t), buf, reg_ptr, scratch);
            } else {
                vtracer_insert_trace_forward(drcontext, instr, bb, sizeof(cache_VCI_sz_t<size>), buf, reg_ptr, scratch);
            }
            break;
        }
        case VPROFILE_TRACE_VAL_CCT:
        {
            vtracer_get_trace_buffer_in_reg(drcontext, instr, bb, buf, reg_ptr);
            // opnd_info
            vtracer_insert_trace_val(drcontext, instr, bb, OPND_CREATE_INT32(packed_info), reg_ptr, scratch, offsetof(cache_VC_t, opnd_info));
            // CCT
            drcctlib_get_context_handle_in_reg(drcontext, bb, instr, slot, reg_addr, scratch);
            vtracer_insert_trace_val(drcontext, instr, bb, opnd_create_reg(reg_addr), reg_ptr, scratch, offsetof(cache_VC_t, ctxt_hndl));
            // value
            vtracer_insert_trace_val(drcontext, instr, bb, opnd, reg_ptr, scratch, offsetof(cache_VC_t, val));
            // update buf ptr
            if(strictly_ordered) {
                vtracer_insert_trace_forward(drcontext, instr, bb, sizeof(cache_VC_t), buf, reg_ptr, scratch);
            } else {
                vtracer_insert_trace_forward(drcontext, instr, bb, sizeof(cache_VC_sz_t<size>), buf, reg_ptr, scratch);
            }
            break;
        }
        case VPROFILE_TRACE_VAL_ADDR_INFO:
        {
            vtracer_get_trace_buffer_in_reg(drcontext, instr, bb, buf, reg_ptr);
            // addr
            if(opnd_is_memory_reference(opnd)) {
                if(!drutil_insert_get_mem_addr(drcontext, bb, instr, opnd, reg_addr/*addr*/, scratch/*scratch*/)) {
                        DR_ASSERT_MSG(false, "InstrumentInsCallback drutil_insert_get_mem_addr failed!");
                }
                vtracer_insert_trace_val(drcontext, instr, bb, opnd_create_reg(reg_addr), reg_ptr, scratch, offsetof(cache_VAI_t, addr));
            } else if(opnd_is_reg(opnd)) {
                vtracer_insert_trace_val(drcontext, instr, bb, OPND_CREATE_INT32(opnd_get_reg(opnd)), reg_ptr, scratch, offsetof(cache_VAI_t, addr));
            }
            // opnd_info
            vtracer_insert_trace_val(drcontext, instr, bb, OPND_CREATE_INT32(packed_info), reg_ptr, scratch, offsetof(cache_VAI_t, opnd_info));
            // info, where is not for trace
            opnd_t info = OPND_CREATE_INTPTR((*global_client_cb.user_data_cb)(drcontext, where, bb, opnd));
            vtracer_insert_trace_val(drcontext, instr, bb, info, reg_ptr, scratch, offsetof(cache_VAI_t, user_data));
            // value
            vtracer_insert_trace_val(drcontext, instr, bb, opnd, reg_ptr, scratch, offsetof(cache_VAI_t, val));
            // update buf ptr
            if(strictly_ordered) {
                vtracer_insert_trace_forward(drcontext, instr, bb, sizeof(cache_VAI_t), buf, reg_ptr, scratch);
            } else {
                vtracer_insert_trace_forward(drcontext, instr, bb, sizeof(cache_VAI_sz_t<size>), buf, reg_ptr, scratch);
            }
            break;
        }
        case VPROFILE_TRACE_VAL_ADDR:
        {
            vtracer_get_trace_buffer_in_reg(drcontext, instr, bb, buf, reg_ptr);
            // addr
            if(opnd_is_memory_reference(opnd)) {
                if(!drutil_insert_get_mem_addr(drcontext, bb, instr, opnd, reg_addr/*addr*/, scratch/*scratch*/)) {
                        DR_ASSERT_MSG(false, "InstrumentInsCallback drutil_insert_get_mem_addr failed!");
                }
                vtracer_insert_trace_val(drcontext, instr, bb, opnd_create_reg(reg_addr), reg_ptr, scratch, offsetof(cache_VA_t, addr));
            } else if(opnd_is_reg(opnd)) {
                vtracer_insert_trace_val(drcontext, instr, bb, OPND_CREATE_INT32(opnd_get_reg(opnd)), reg_ptr, scratch, offsetof(cache_VA_t, addr));
            }
            // opnd_info
            vtracer_insert_trace_val(drcontext, instr, bb, OPND_CREATE_INT32(packed_info), reg_ptr, scratch, offsetof(cache_VA_t, opnd_info));
            // value
            vtracer_insert_trace_val(drcontext, instr, bb, opnd, reg_ptr, scratch, offsetof(cache_VA_t, val));
            // update buf ptr
            if(strictly_ordered) {
                vtracer_insert_trace_forward(drcontext, instr, bb, sizeof(cache_VA_t), buf, reg_ptr, scratch);
            } else {
                vtracer_insert_trace_forward(drcontext, instr, bb, sizeof(cache_VA_sz_t<size>), buf, reg_ptr, scratch);
            }
            break;
        }
        case VPROFILE_TRACE_VAL_INFO:
        {
            vtracer_get_trace_buffer_in_reg(drcontext, instr, bb, buf, reg_ptr);
            // opnd_info
            vtracer_insert_trace_val(drcontext, instr, bb, OPND_CREATE_INT32(packed_info), reg_ptr, scratch, offsetof(cache_VI_t, opnd_info));
            // info, where is not for trace
            opnd_t info = OPND_CREATE_INTPTR((*global_client_cb.user_data_cb)(drcontext, where, bb, opnd));
            vtracer_insert_trace_val(drcontext, instr, bb, info, reg_ptr, scratch, offsetof(cache_VI_t, user_data));
            // value
            vtracer_insert_trace_val(drcontext, instr, bb, opnd, reg_ptr, scratch, offsetof(cache_VI_t, val));
            // update buf ptr
            if(strictly_ordered) {
                vtracer_insert_trace_forward(drcontext, instr, bb, sizeof(cache_VI_t), buf, reg_ptr, scratch);
            } else {
                vtracer_insert_trace_forward(drcontext, instr, bb, sizeof(cache_VI_sz_t<size>), buf, reg_ptr, scratch);
            }
            break;
        }
        case VPROFILE_TRACE_VALUE:
        {
            vtracer_get_trace_buffer_in_reg(drcontext, instr, bb, buf, reg_ptr);
            // opnd_info
            vtracer_insert_trace_val(drcontext, instr, bb, OPND_CREATE_INT32(packed_info), reg_ptr, scratch, offsetof(cache_V_t, opnd_info));
            // value
            vtracer_insert_trace_val(drcontext, instr, bb, opnd, reg_ptr, scratch, offsetof(cache_V_t, val));
            // update buf ptr
            if(strictly_ordered) {
                vtracer_insert_trace_forward(drcontext, instr, bb, sizeof(cache_V_t), buf, reg_ptr, scratch);
            } else {
                vtracer_insert_trace_forward(drcontext, instr, bb, sizeof(cache_V_sz_t<size>), buf, reg_ptr, scratch);
            }
            break;
        }
        case VPROFILE_TRACE_CCT_ADDR_INFO:
        {
            vtracer_get_trace_buffer_in_reg(drcontext, instr, bb, buf, reg_ptr);
            // addr
            if(opnd_is_memory_reference(opnd)) {
                if(!drutil_insert_get_mem_addr(drcontext, bb, instr, opnd, reg_addr/*addr*/, scratch/*scratch*/)) {
                        DR_ASSERT_MSG(false, "InstrumentInsCallback drutil_insert_get_mem_addr failed!");
                }
                vtracer_insert_trace_val(drcontext, instr, bb, opnd_create_reg(reg_addr), reg_ptr, scratch, offsetof(cache_CAI_t, addr));
            } else if(opnd_is_reg(opnd)) {
                vtracer_insert_trace_val(drcontext, instr, bb, OPND_CREATE_INT32(opnd_get_reg(opnd)), reg_ptr, scratch, offsetof(cache_CAI_t, addr));
            }
            // opnd_info
            vtracer_insert_trace_val(drcontext, instr, bb, OPND_CREATE_INT32(packed_info), reg_ptr, scratch, offsetof(cache_CAI_t, opnd_info));
            // CCT
            drcctlib_get_context_handle_in_reg(drcontext, bb, instr, slot, reg_addr, scratch);
            vtracer_insert_trace_val(drcontext, instr, bb, opnd_create_reg(reg_addr), reg_ptr, scratch, offsetof(cache_CAI_t, ctxt_hndl));
            // info, where is not for trace
            opnd_t info = OPND_CREATE_INTPTR((*global_client_cb.user_data_cb)(drcontext, where, bb, opnd));
            vtracer_insert_trace_val(drcontext, instr, bb, info, reg_ptr, scratch, offsetof(cache_CAI_t, user_data));
            // update buf ptr
            vtracer_insert_trace_forward(drcontext, instr, bb, sizeof(cache_CAI_t), buf, reg_ptr, scratch);
            break;
        }
        case VPROFILE_TRACE_CCT_ADDR:
        {
            vtracer_get_trace_buffer_in_reg(drcontext, instr, bb, buf, reg_ptr);
            // addr
            if(opnd_is_memory_reference(opnd)) {
                if(!drutil_insert_get_mem_addr(drcontext, bb, instr, opnd, reg_addr/*addr*/, scratch/*scratch*/)) {
                        DR_ASSERT_MSG(false, "InstrumentInsCallback drutil_insert_get_mem_addr failed!");
                }
                vtracer_insert_trace_val(drcontext, instr, bb, opnd_create_reg(reg_addr), reg_ptr, scratch, offsetof(cache_CA_t, addr));
            } else if(opnd_is_reg(opnd)) {
                vtracer_insert_trace_val(drcontext, instr, bb, OPND_CREATE_INT32(opnd_get_reg(opnd)), reg_ptr, scratch, offsetof(cache_CA_t, addr));
            }
            // opnd_info
            vtracer_insert_trace_val(drcontext, instr, bb, OPND_CREATE_INT32(packed_info), reg_ptr, scratch, offsetof(cache_CA_t, opnd_info));
            // CCT
            drcctlib_get_context_handle_in_reg(drcontext, bb, instr, slot, reg_addr, scratch);
            vtracer_insert_trace_val(drcontext, instr, bb, opnd_create_reg(reg_addr), reg_ptr, scratch, offsetof(cache_CA_t, ctxt_hndl));
            // update buf ptr
            vtracer_insert_trace_forward(drcontext, instr, bb, sizeof(cache_CA_t), buf, reg_ptr, scratch);
            break;
        }
        case VPROFILE_TRACE_CCT_INFO:
        {
            vtracer_get_trace_buffer_in_reg(drcontext, instr, bb, buf, reg_ptr);
            // opnd_info
            vtracer_insert_trace_val(drcontext, instr, bb, OPND_CREATE_INT32(packed_info), reg_ptr, scratch, offsetof(cache_CI_t, opnd_info));
            // CCT
            drcctlib_get_context_handle_in_reg(drcontext, bb, instr, slot, reg_addr, scratch);
            vtracer_insert_trace_val(drcontext, instr, bb, opnd_create_reg(reg_addr), reg_ptr, scratch, offsetof(cache_CI_t, ctxt_hndl));
            // info, where is not for trace
            opnd_t info = OPND_CREATE_INTPTR((*global_client_cb.user_data_cb)(drcontext, where, bb, opnd));
            vtracer_insert_trace_val(drcontext, instr, bb, info, reg_ptr, scratch, offsetof(cache_CI_t, user_data));
            // update buf ptr
            vtracer_insert_trace_forward(drcontext, instr, bb, sizeof(cache_CI_t), buf, reg_ptr, scratch);
            break;
        }
        case VPROFILE_TRACE_ADDR_INFO:
        {
            vtracer_get_trace_buffer_in_reg(drcontext, instr, bb, buf, reg_ptr);
            // addr
            if(opnd_is_memory_reference(opnd)) {
                if(!drutil_insert_get_mem_addr(drcontext, bb, instr, opnd, reg_addr/*addr*/, scratch/*scratch*/)) {
                        DR_ASSERT_MSG(false, "InstrumentInsCallback drutil_insert_get_mem_addr failed!");
                }
                vtracer_insert_trace_val(drcontext, instr, bb, opnd_create_reg(reg_addr), reg_ptr, scratch, offsetof(cache_AI_t, addr));
            } else if(opnd_is_reg(opnd)) {
                vtracer_insert_trace_constant(drcontext, instr, bb, (uint32_t)opnd_get_reg(opnd), reg_ptr, scratch, offsetof(cache_AI_t, addr));
            }
            // opnd_info
            vtracer_insert_trace_val(drcontext, instr, bb, OPND_CREATE_INT32(packed_info), reg_ptr, scratch, offsetof(cache_AI_t, opnd_info));
            // info, where is not for trace
            opnd_t info = OPND_CREATE_INTPTR((*global_client_cb.user_data_cb)(drcontext, where, bb, opnd));
            vtracer_insert_trace_val(drcontext, instr, bb, info, reg_ptr, scratch, offsetof(cache_AI_t, user_data));
            // update buf ptr
            vtracer_insert_trace_forward(drcontext, instr, bb, sizeof(cache_AI_t), buf, reg_ptr, scratch);
            break;
        }
        case VPROFILE_TRACE_ADDR:
        {
            vtracer_get_trace_buffer_in_reg(drcontext, instr, bb, buf, reg_ptr);
            // addr
            if(opnd_is_memory_reference(opnd)) {
                if(!drutil_insert_get_mem_addr(drcontext, bb, instr, opnd, reg_addr/*addr*/, scratch/*scratch*/)) {
                        DR_ASSERT_MSG(false, "InstrumentInsCallback drutil_insert_get_mem_addr failed!");
                }
                vtracer_insert_trace_val(drcontext, instr, bb, opnd_create_reg(reg_addr), reg_ptr, scratch, offsetof(cache_A_t, addr));
            } else if(opnd_is_reg(opnd)) {
                vtracer_insert_trace_constant(drcontext, instr, bb, (uint32_t)opnd_get_reg(opnd), reg_ptr, scratch, offsetof(cache_A_t, addr));
            }
            // opnd_info
            vtracer_insert_trace_val(drcontext, instr, bb, OPND_CREATE_INT32(packed_info), reg_ptr, scratch, offsetof(cache_A_t, opnd_info));
            // update buf ptr
            vtracer_insert_trace_forward(drcontext, instr, bb, sizeof(cache_A_t), buf, reg_ptr, scratch);
            break;
        }
        default:
            DR_ASSERT_MSG(false, "Unknown trace flag!");
    }
    
    return ;
}

template<bool is_dst, bool before_instr>
uint32_t 
getOpndMask(instr_t* instr, opnd_t opnd) {
    uint32_t mask = 0;
    if(opnd_is_floating(instr, opnd)) {
        mask |= IS_FLOATING;
    } else {
        mask |= IS_INTEGER;
    }
    if(is_dst) {
        mask |= (WRITE);
        if(before_instr) {
            mask |= (BEFORE);
        } else {
            mask |= (AFTER);
        }
    } else {
        mask |= (READ | BEFORE);
    }
    int size = opnd_size_in_bytes(opnd_get_size(opnd));
    if(size != 0) {
        if(opnd_is_reg(opnd)) {
            reg_id_t opnd_reg = opnd_get_reg(opnd);
            if(reg_is_gpr(opnd_reg)) {
                mask |= GPR_REGISTER;
            } else if(reg_is_simd(opnd_reg)) {
                mask |= SIMD_REGISTER;
            }
#ifdef X86
            else if(opnd_reg >= DR_REG_CR0 && opnd_reg <= DR_REG_CR15) 
#endif

#ifdef AARCH64
            else if(opnd_reg >= DR_REG_NZCV && opnd_reg <= DR_REG_FPSR) 
#endif
                mask |= CTR_REGISTER;
            else {
                mask |= OTH_REGISTER;
            }
        } else if(opnd_is_memory_reference(opnd)) {
            mask |= MEMORY;
        } else if(opnd_is_pc(opnd)) {
            mask |= PC;
        } else if(opnd_is_immed(opnd)) {
            mask |= IMMEDIATE;
        } else {
            DR_ASSERT_MSG(false, "getOpndMask Unknown opnd!");
        }
    }

    return mask;
}

template<bool with_cct>
void
instrument_opnd(void *drcontext, instrlist_t *bb, instr_t *where, instr_t *insert_where, int32_t slot, opnd_t opnd, vtrace_t *trace, reg_id_t reg_addr, reg_id_t reg_ptr, reg_id_t scratch, bool before, uint32_t type)
{
    int size = opnd_size_in_bytes(opnd_get_size(opnd));
    switch(size) {
        case 1: insertStoreTraceBuffer<1, with_cct>(drcontext, bb, where, insert_where, slot, opnd, trace, reg_addr, reg_ptr, scratch, before, type);break;
        case 2: insertStoreTraceBuffer<2, with_cct>(drcontext, bb, where, insert_where, slot, opnd, trace, reg_addr, reg_ptr, scratch, before, type);break;
        case 4: insertStoreTraceBuffer<4, with_cct>(drcontext, bb, where, insert_where, slot, opnd, trace, reg_addr, reg_ptr, scratch, before, type);break;
        case 8: insertStoreTraceBuffer<8, with_cct>(drcontext, bb, where, insert_where, slot, opnd, trace, reg_addr, reg_ptr, scratch, before, type);break;
        // 128-bit, AVX, SSE
        case 16: insertStoreTraceBuffer<16, with_cct>(drcontext, bb, where, insert_where, slot, opnd, trace, reg_addr, reg_ptr, scratch, before, type);break;
        // 256-bit, AVX2
        case 32: insertStoreTraceBuffer<32, with_cct>(drcontext, bb, where, insert_where, slot, opnd, trace, reg_addr, reg_ptr, scratch, before, type);break;
        // 512-bit, AVX512
        case 64: insertStoreTraceBuffer<64, with_cct>(drcontext, bb, where, insert_where, slot, opnd, trace, reg_addr, reg_ptr, scratch, before, type);break;
        // case 128: insertStoreTraceBuffer<128, with_cct>(drcontext, bb, where, insert_where, slot, opnd, trace, reg_addr, reg_ptr, scratch, before, type);break;
        // case 256: insertStoreTraceBuffer<256, with_cct>(drcontext, bb, where, insert_where, slot, opnd, trace, reg_addr, reg_ptr, scratch, before, type);break;
        // case 512: insertStoreTraceBuffer<512, with_cct>(drcontext, bb, where, insert_where, slot, opnd, trace, reg_addr, reg_ptr, scratch, before, type);break;
        default: {
            // DR_ASSERT_MSG(false, "InstrumentInsCallback Unknown size!");
#ifdef VPROFILE_DEBUG
        debug_unknown_case(drcontext, where, opnd);
#endif
        }
    }
}

template<bool with_cct>
void
InstrumentInstruction(void *drcontext, instrlist_t *bb, instr_t* instr, int slot)
{
    if(!instr_is_app(instr) || instr_is_ignorable(instr)) return;
    
    if(global_client_cb.ins_instrument_cb)
        (*global_client_cb.ins_instrument_cb)(drcontext, instr, bb);

    // try to quick return when there is no opnd valid
    unsigned int i;
    bool is_valid = false;
    int num = instr_num_srcs(instr);
    for(int j = 0; j < num; j++) {
        opnd_t opnd = instr_get_src(instr, j);
        int size = opnd_size_in_bytes(opnd_get_size(opnd));
        if(size != 0) {
            is_valid = true;
            break;
        }
    }
    if(!is_valid) {
        num = instr_num_dsts(instr);
        for(int j = 0; j < num; j++) {
            opnd_t opnd = instr_get_dst(instr, j);
            int size = opnd_size_in_bytes(opnd_get_size(opnd));
            if(size != 0) {
                is_valid = true;
                break;
            }
        }
    }
    // quick return as there is no opnd valid
    if(!is_valid) return;
    
    reg_id_t reg_addr, reg_ptr;
    reg_id_t scratch;
    drvector_t allowed;

    // insert before
    getUnusedRegEntryInstr(&allowed, instr);

    RESERVE_REG(drcontext, bb, instr, &allowed, scratch);
    RESERVE_REG(drcontext, bb, instr, &allowed, reg_addr);
    RESERVE_REG(drcontext, bb, instr, &allowed, reg_ptr);
    // for each trace registered by user, we need to call func in vtrace to trace value.
    for (i = 0; i < vtrace_t_list.entries; ++i) {
        vtrace_t *trace = (vtrace_t*)drvector_get_entry(&vtrace_t_list, i);
        if(TEST_FLAG(trace->trace_flag, VPROFILE_TRACE_BEFORE_WRITE)) {
            num = instr_num_dsts(instr);
            for(int j=0; j<num; ++j) {
                opnd_t opnd = instr_get_dst(instr, j);
                if(opnd_size_in_bytes(opnd_get_size(opnd)) == 0) continue;
                uint32_t opmask = getOpndMask<true, true>(instr, opnd);
                // if(TEST_FLAG(trace->trace_flag, VPROFILE_TRACE_STRICTLY_ORDERED)) {
                //     vtrace_buffer_t* buf = trace->buff;
                //     if(!(*(bool (*)(opnd_t, vprofile_src_t))buf->user_data_fill_num)(opnd, (vprofile_src_t)opmask)) {
                //         continue;
                //     }
                // }
                if(FILTER_OPND_MASK(trace->opnd_mask, opmask)) {
                    instrument_opnd<with_cct>(drcontext, bb, instr, instr, slot, opnd, trace, reg_addr, reg_ptr, scratch, true, opmask);
                }
            }
        }
        // if(!FILTER_OPND_MASK(trace->opnd_mask, READ)) continue;
        num = instr_num_srcs(instr);
        for(int j=0; j<num; ++j) {
            opnd_t opnd = instr_get_src(instr, j);
            if(opnd_size_in_bytes(opnd_get_size(opnd)) == 0) continue;
            uint32_t opmask = getOpndMask<false, true>(instr, opnd);
            if(FILTER_OPND_MASK(trace->opnd_mask, opmask)) {
                instrument_opnd<with_cct>(drcontext, bb, instr, instr, slot, opnd, trace, reg_addr, reg_ptr, scratch, true, opmask);
                if(TEST_FLAG(trace->trace_flag, VPROFILE_TRACE_REG_IN_MEMREF) && TEST_OPND_MASK(opmask, MEMORY)) {
                    int reg_num = opnd_num_regs_used(opnd);
                    for(int k = 0; k < reg_num; k++) {
                        opnd_t reg_used = opnd_create_reg(opnd_get_reg_used(opnd, k));
                        int reg_size = opnd_size_in_bytes(opnd_get_size(reg_used));
                        if(reg_size == 0) {
                            continue;
                        }
                        instrument_opnd<with_cct>(drcontext, bb, instr, instr, slot, reg_used, trace, reg_addr, reg_ptr, scratch, true, opmask);
                    }
                } 
            }
        }
    }
    UNRESERVE_REG(drcontext, bb, instr, scratch);
    UNRESERVE_REG(drcontext, bb, instr, reg_addr);
    UNRESERVE_REG(drcontext, bb, instr, reg_ptr);

    // insert after
    // skip cti and syscall instr, cause they may terminate the block and cannot insert instrs after them.
    // remember to modify fill num cb as well
    if(!instr_is_cti(instr) && !instr_is_syscall(instr)) {
        instr_t *insert_where = instr_get_next_app(instr);
        if(insert_where) {
            getUnusedRegEntryInstrWithoutInit(&allowed, insert_where);
        } else {
            instrlist_meta_postinsert(bb, instr, XINST_CREATE_nop(drcontext));
            insert_where = instr_get_next(instr);
        }
        RESERVE_REG(drcontext, bb, insert_where, &allowed, scratch);
        RESERVE_REG(drcontext, bb, insert_where, &allowed, reg_addr);
        RESERVE_REG(drcontext, bb, insert_where, &allowed, reg_ptr);

        // for each trace registered by user, we need to call func in vtrace to trace value.
        for (i = 0; i < vtrace_t_list.entries; ++i) {
            vtrace_t *trace = (vtrace_t*)drvector_get_entry(&vtrace_t_list, i);
            // if(!FILTER_OPND_MASK(trace->opnd_mask, WRITE)) continue;
            num = instr_num_dsts(instr);
            for(int j=0; j<num; ++j) {
                opnd_t opnd = instr_get_dst(instr, j);
                if(opnd_size_in_bytes(opnd_get_size(opnd)) == 0) continue;
                uint32_t opmask = getOpndMask<true, false>(instr, opnd);
                if(FILTER_OPND_MASK(trace->opnd_mask, opmask)) {
                    instrument_opnd<with_cct>(drcontext, bb, instr, insert_where, slot, opnd, trace, reg_addr, reg_ptr, scratch, false, opmask);
                    if(TEST_FLAG(trace->trace_flag, VPROFILE_TRACE_REG_IN_MEMREF) && TEST_OPND_MASK(opmask, MEMORY)) {
                        int reg_num = opnd_num_regs_used(opnd);
                        for(int k = 0; k < reg_num; k++) {
                            opnd_t reg_used = opnd_create_reg(opnd_get_reg_used(opnd, k));
                            int reg_size = opnd_size_in_bytes(opnd_get_size(reg_used));
                            if(reg_size == 0) {
                                continue;
                            }
                            instrument_opnd<with_cct>(drcontext, bb, instr, insert_where, slot, reg_used, trace, reg_addr, reg_ptr, scratch, false, opmask);
                        }
                    } 
                }
            }
        }
        UNRESERVE_REG(drcontext, bb, insert_where, scratch);
        UNRESERVE_REG(drcontext, bb, insert_where, reg_addr);
        UNRESERVE_REG(drcontext, bb, insert_where, reg_ptr);
    }
    drvector_delete(&allowed);
}

#ifdef X86
template<uint8_t size>
void handleVCAI(vtrace_t *vtrace, int slot, instrlist_t *bb, instr_t* instr, opnd_t *opnd, uint32_t type) {
    void *drcontext = dr_get_current_drcontext();
    dr_mcontext_t mcontext;
    mcontext.size = sizeof(mcontext);
    mcontext.flags = DR_MC_ALL;
    dr_get_mcontext(drcontext, &mcontext);

    context_handle_t ctxt_hndl = drcctlib_get_context_handle(drcontext, slot);

    opnd_info_pack_t opnd_info_pack;
    opnd_info_pack.info.size = size;
    opnd_info_pack.info.esize = size;
    opnd_info_pack.info.opnd_type = (uint16_t)(type&0xffff);
    DR_ASSERT((type & 0xffff) == type);

    vtrace_buffer_t* buf = vtrace->buff;

    cache_VCAI_t cache;
    cache.opnd_info = opnd_info_pack;
    cache.ctxt_hndl = ctxt_hndl;
    cache.user_data = (*global_client_cb.user_data_cb)(drcontext, instr, bb, *opnd);
    if(opnd_is_reg(*opnd)) {
        cache.addr = (uint64_t)opnd_get_reg(*opnd);
        reg_get_value_ex((reg_id_t)cache.addr, &mcontext, (byte*)cache.val);
        vtracer_update_clean_call<cache_VCAI_t>(drcontext, buf, cache);
    } else {
        app_pc addr;
        bool is_write;
        uint32_t pos;
        for( int index=0; instr_compute_address_ex_pos(instr, &mcontext, index, &addr, &is_write, &pos); ++index ) {
            DR_ASSERT(!is_write);
            cache.addr = (uint64_t)addr;
            memcpy((void*)cache.val, (void*)addr, size);
            vtracer_update_clean_call<cache_VCAI_t>(drcontext, buf, cache);
        }
    }
}

template<uint8_t size>
void handleVCA(vtrace_t *vtrace, int slot, instrlist_t *bb, instr_t* instr, opnd_t *opnd, uint32_t type) {
    void *drcontext = dr_get_current_drcontext();
    dr_mcontext_t mcontext;
    mcontext.size = sizeof(mcontext);
    mcontext.flags = DR_MC_ALL;
    dr_get_mcontext(drcontext, &mcontext);

    context_handle_t ctxt_hndl = drcctlib_get_context_handle(drcontext, slot);

    opnd_info_pack_t opnd_info_pack;
    opnd_info_pack.info.size = size;
    opnd_info_pack.info.esize = size;
    opnd_info_pack.info.opnd_type = (uint16_t)(type&0xffff);
    DR_ASSERT((type & 0xffff) == type);

    vtrace_buffer_t* buf = vtrace->buff;

    cache_VCA_t cache;
    cache.opnd_info = opnd_info_pack;
    cache.ctxt_hndl = ctxt_hndl;
    if(opnd_is_reg(*opnd)) {
        cache.addr = (uint64_t)opnd_get_reg(*opnd);
        reg_get_value_ex((reg_id_t)cache.addr, &mcontext, (byte*)cache.val);
        vtracer_update_clean_call<cache_VCA_t>(drcontext, buf, cache);
    } else {
        app_pc addr;
        bool is_write;
        uint32_t pos;
        for( int index=0; instr_compute_address_ex_pos(instr, &mcontext, index, &addr, &is_write, &pos); ++index ) {
            DR_ASSERT(!is_write);
            cache.addr = (uint64_t)addr;
            memcpy((void*)cache.val, (void*)addr, size);
            vtracer_update_clean_call<cache_VCA_t>(drcontext, buf, cache);
        }
    }
}

template<uint8_t size>
void handleVCI(vtrace_t *vtrace, int slot, instrlist_t *bb, instr_t* instr, opnd_t *opnd, uint32_t type) {
    void *drcontext = dr_get_current_drcontext();
    dr_mcontext_t mcontext;
    mcontext.size = sizeof(mcontext);
    mcontext.flags = DR_MC_ALL;
    dr_get_mcontext(drcontext, &mcontext);

    context_handle_t ctxt_hndl = drcctlib_get_context_handle(drcontext, slot);

    opnd_info_pack_t opnd_info_pack;
    opnd_info_pack.info.size = size;
    opnd_info_pack.info.esize = size;
    opnd_info_pack.info.opnd_type = (uint16_t)(type&0xffff);
    DR_ASSERT((type & 0xffff) == type);

    vtrace_buffer_t* buf = vtrace->buff;

    cache_VCI_t cache;
    cache.opnd_info = opnd_info_pack;
    cache.ctxt_hndl = ctxt_hndl;
    cache.user_data = (*global_client_cb.user_data_cb)(drcontext, instr, bb, *opnd);
    if(opnd_is_reg(*opnd)) {
        reg_get_value_ex(opnd_get_reg(*opnd), &mcontext, (byte*)cache.val);
        vtracer_update_clean_call<cache_VCI_t>(drcontext, buf, cache);
    } else {
        app_pc addr;
        bool is_write;
        uint32_t pos;
        for( int index=0; instr_compute_address_ex_pos(instr, &mcontext, index, &addr, &is_write, &pos); ++index ) {
            DR_ASSERT(!is_write);
            memcpy((void*)cache.val, (void*)addr, size);
            vtracer_update_clean_call<cache_VCI_t>(drcontext, buf, cache);
        }
    }
}

template<uint8_t size>
void handleVC(vtrace_t *vtrace, int slot, instrlist_t *bb, instr_t* instr, opnd_t *opnd, uint32_t type) {
    void *drcontext = dr_get_current_drcontext();
    dr_mcontext_t mcontext;
    mcontext.size = sizeof(mcontext);
    mcontext.flags = DR_MC_ALL;
    dr_get_mcontext(drcontext, &mcontext);

    context_handle_t ctxt_hndl = drcctlib_get_context_handle(drcontext, slot);

    opnd_info_pack_t opnd_info_pack;
    opnd_info_pack.info.size = size;
    opnd_info_pack.info.esize = size;
    opnd_info_pack.info.opnd_type = (uint16_t)(type&0xffff);
    DR_ASSERT((type & 0xffff) == type);

    vtrace_buffer_t* buf = vtrace->buff;

    cache_VC_t cache;
    cache.opnd_info = opnd_info_pack;
    cache.ctxt_hndl = ctxt_hndl;
    if(opnd_is_reg(*opnd)) {
        reg_get_value_ex(opnd_get_reg(*opnd), &mcontext, (byte*)cache.val);
        vtracer_update_clean_call<cache_VC_t>(drcontext, buf, cache);
    } else {
        app_pc addr;
        bool is_write;
        uint32_t pos;
        for( int index=0; instr_compute_address_ex_pos(instr, &mcontext, index, &addr, &is_write, &pos); ++index ) {
            DR_ASSERT(!is_write);
            memcpy((void*)cache.val, (void*)addr, size);
            vtracer_update_clean_call<cache_VC_t>(drcontext, buf, cache);
        }
    }
}

template<uint8_t size>
void handleVAI(vtrace_t *vtrace, int slot, instrlist_t *bb, instr_t* instr, opnd_t *opnd, uint32_t type) {
    void *drcontext = dr_get_current_drcontext();
    dr_mcontext_t mcontext;
    mcontext.size = sizeof(mcontext);
    mcontext.flags = DR_MC_ALL;
    dr_get_mcontext(drcontext, &mcontext);

    opnd_info_pack_t opnd_info_pack;
    opnd_info_pack.info.size = size;
    opnd_info_pack.info.esize = size;
    opnd_info_pack.info.opnd_type = (uint16_t)(type&0xffff);
    DR_ASSERT((type & 0xffff) == type);

    vtrace_buffer_t* buf = vtrace->buff;

    cache_VAI_t cache;
    cache.opnd_info = opnd_info_pack;
    cache.user_data = (*global_client_cb.user_data_cb)(drcontext, instr, bb, *opnd);
    if(opnd_is_reg(*opnd)) {
        cache.addr = (uint64_t)opnd_get_reg(*opnd);
        reg_get_value_ex((reg_id_t)cache.addr, &mcontext, (byte*)cache.val);
        vtracer_update_clean_call<cache_VAI_t>(drcontext, buf, cache);
    } else {
        app_pc addr;
        bool is_write;
        uint32_t pos;
        for( int index=0; instr_compute_address_ex_pos(instr, &mcontext, index, &addr, &is_write, &pos); ++index ) {
            DR_ASSERT(!is_write);
            cache.addr = (uint64_t)addr;
            memcpy((void*)cache.val, (void*)addr, size);
            vtracer_update_clean_call<cache_VAI_t>(drcontext, buf, cache);
        }
    }
}

template<uint8_t size>
void handleVA(vtrace_t *vtrace, int slot, instrlist_t *bb, instr_t* instr, opnd_t *opnd, uint32_t type) {
    void *drcontext = dr_get_current_drcontext();
    dr_mcontext_t mcontext;
    mcontext.size = sizeof(mcontext);
    mcontext.flags = DR_MC_ALL;
    dr_get_mcontext(drcontext, &mcontext);

    opnd_info_pack_t opnd_info_pack;
    opnd_info_pack.info.size = size;
    opnd_info_pack.info.esize = size;
    opnd_info_pack.info.opnd_type = (uint16_t)(type&0xffff);
    DR_ASSERT((type & 0xffff) == type);

    vtrace_buffer_t* buf = vtrace->buff;

    cache_VA_t cache;
    cache.opnd_info = opnd_info_pack;
    if(opnd_is_reg(*opnd)) {
        cache.addr = (uint64_t)opnd_get_reg(*opnd);
        reg_get_value_ex((reg_id_t)cache.addr, &mcontext, (byte*)cache.val);
        vtracer_update_clean_call<cache_VA_t>(drcontext, buf, cache);
    } else {
        app_pc addr;
        bool is_write;
        uint32_t pos;
        for( int index=0; instr_compute_address_ex_pos(instr, &mcontext, index, &addr, &is_write, &pos); ++index ) {
            DR_ASSERT(!is_write);
            cache.addr = (uint64_t)addr;
            memcpy((void*)cache.val, (void*)addr, size);
            vtracer_update_clean_call<cache_VA_t>(drcontext, buf, cache);
        }
    }
}

template<uint8_t size>
void handleVI(vtrace_t *vtrace, int slot, instrlist_t *bb, instr_t* instr, opnd_t *opnd, uint32_t type) {
    void *drcontext = dr_get_current_drcontext();
    dr_mcontext_t mcontext;
    mcontext.size = sizeof(mcontext);
    mcontext.flags = DR_MC_ALL;
    dr_get_mcontext(drcontext, &mcontext);

    opnd_info_pack_t opnd_info_pack;
    opnd_info_pack.info.size = size;
    opnd_info_pack.info.esize = size;
    opnd_info_pack.info.opnd_type = (uint16_t)(type&0xffff);
    DR_ASSERT((type & 0xffff) == type);

    vtrace_buffer_t* buf = vtrace->buff;

    cache_VI_t cache;
    cache.opnd_info = opnd_info_pack;
    cache.user_data = (*global_client_cb.user_data_cb)(drcontext, instr, bb, *opnd);
    if(opnd_is_reg(*opnd)) {
        reg_get_value_ex(opnd_get_reg(*opnd), &mcontext, (byte*)cache.val);
        vtracer_update_clean_call<cache_VI_t>(drcontext, buf, cache);
    } else {
        app_pc addr;
        bool is_write;
        uint32_t pos;
        for( int index=0; instr_compute_address_ex_pos(instr, &mcontext, index, &addr, &is_write, &pos); ++index ) {
            DR_ASSERT(!is_write);
            memcpy((void*)cache.val, (void*)addr, size);
            vtracer_update_clean_call<cache_VI_t>(drcontext, buf, cache);
        }
    }
}

template<uint8_t size>
void handleV(vtrace_t *vtrace, int slot, instrlist_t *bb, instr_t* instr, opnd_t *opnd, uint32_t type) {
    void *drcontext = dr_get_current_drcontext();
    dr_mcontext_t mcontext;
    mcontext.size = sizeof(mcontext);
    mcontext.flags = DR_MC_ALL;
    dr_get_mcontext(drcontext, &mcontext);

    opnd_info_pack_t opnd_info_pack;
    opnd_info_pack.info.size = size;
    opnd_info_pack.info.esize = size;
    opnd_info_pack.info.opnd_type = (uint16_t)(type&0xffff);
    DR_ASSERT((type & 0xffff) == type);

    vtrace_buffer_t* buf = vtrace->buff;

    cache_V_t cache;
    cache.opnd_info = opnd_info_pack;
    if(opnd_is_reg(*opnd)) {
        reg_get_value_ex(opnd_get_reg(*opnd), &mcontext, (byte*)cache.val);
        vtracer_update_clean_call<cache_V_t>(drcontext, buf, cache);
    } else {
        app_pc addr;
        bool is_write;
        uint32_t pos;
        for( int index=0; instr_compute_address_ex_pos(instr, &mcontext, index, &addr, &is_write, &pos); ++index ) {
            DR_ASSERT(!is_write);
            memcpy((void*)cache.val, (void*)addr, size);
            vtracer_update_clean_call<cache_V_t>(drcontext, buf, cache);
        }
    }
}

template<uint8_t size>
void handleVCAISZ(vtrace_t *vtrace, int slot, instrlist_t *bb, instr_t* instr, opnd_t *opnd, uint32_t type) {
    void *drcontext = dr_get_current_drcontext();
    dr_mcontext_t mcontext;
    mcontext.size = sizeof(mcontext);
    mcontext.flags = DR_MC_ALL;
    dr_get_mcontext(drcontext, &mcontext);

    bool is_float = TEST_OPND_MASK(type, IS_FLOATING);
    context_handle_t ctxt_hndl = drcctlib_get_context_handle(drcontext, slot);

    opnd_info_pack_t opnd_info_pack;
    opnd_info_pack.info.size = size;
    opnd_info_pack.info.esize = size;
    opnd_info_pack.info.opnd_type = (uint16_t)(type&0xffff);
    DR_ASSERT((type & 0xffff) == type);

    vtrace_buffer_t* buf = get_trace_buf(vtrace, is_float, size, size);

    cache_VCAI_sz_t<size> cache;
    cache.opnd_info = opnd_info_pack;
    cache.ctxt_hndl = ctxt_hndl;
    cache.user_data = (*global_client_cb.user_data_cb)(drcontext, instr, bb, *opnd);
    if(opnd_is_reg(*opnd)) {
        cache.addr = (uint64_t)opnd_get_reg(*opnd);
        reg_get_value_ex((reg_id_t)cache.addr, &mcontext, (byte*)cache.val);
        vtracer_update_clean_call<cache_VCAI_sz_t<size>>(drcontext, buf, cache);
    } else {
        app_pc addr;
        bool is_write;
        uint32_t pos;
        for( int index=0; instr_compute_address_ex_pos(instr, &mcontext, index, &addr, &is_write, &pos); ++index ) {
            DR_ASSERT(!is_write);
            cache.addr = (uint64_t)addr;
            memcpy((void*)cache.val, (void*)addr, size);
            vtracer_update_clean_call<cache_VCAI_sz_t<size>>(drcontext, buf, cache);
        }
    }
}

template<uint8_t size>
void handleVCASZ(vtrace_t *vtrace, int slot, instrlist_t *bb, instr_t* instr, opnd_t *opnd, uint32_t type) {
    void *drcontext = dr_get_current_drcontext();
    dr_mcontext_t mcontext;
    mcontext.size = sizeof(mcontext);
    mcontext.flags = DR_MC_ALL;
    dr_get_mcontext(drcontext, &mcontext);

    bool is_float = TEST_OPND_MASK(type, IS_FLOATING);
    context_handle_t ctxt_hndl = drcctlib_get_context_handle(drcontext, slot);

    opnd_info_pack_t opnd_info_pack;
    opnd_info_pack.info.size = size;
    opnd_info_pack.info.esize = size;
    opnd_info_pack.info.opnd_type = (uint16_t)(type&0xffff);
    DR_ASSERT((type & 0xffff) == type);

    vtrace_buffer_t* buf = get_trace_buf(vtrace, is_float, size, size);

    cache_VCA_sz_t<size> cache;
    cache.opnd_info = opnd_info_pack;
    cache.ctxt_hndl = ctxt_hndl;
    if(opnd_is_reg(*opnd)) {
        cache.addr = (uint64_t)opnd_get_reg(*opnd);
        reg_get_value_ex((reg_id_t)cache.addr, &mcontext, (byte*)cache.val);
        vtracer_update_clean_call<cache_VCA_sz_t<size>>(drcontext, buf, cache);
    } else {
        app_pc addr;
        bool is_write;
        uint32_t pos;
        for( int index=0; instr_compute_address_ex_pos(instr, &mcontext, index, &addr, &is_write, &pos); ++index ) {
            DR_ASSERT(!is_write);
            cache.addr = (uint64_t)addr;
            memcpy((void*)cache.val, (void*)addr, size);
            vtracer_update_clean_call<cache_VCA_sz_t<size>>(drcontext, buf, cache);
        }
    }
}

template<uint8_t size>
void handleVCISZ(vtrace_t *vtrace, int slot, instrlist_t *bb, instr_t* instr, opnd_t *opnd, uint32_t type) {
    void *drcontext = dr_get_current_drcontext();
    dr_mcontext_t mcontext;
    mcontext.size = sizeof(mcontext);
    mcontext.flags = DR_MC_ALL;
    dr_get_mcontext(drcontext, &mcontext);

    bool is_float = TEST_OPND_MASK(type, IS_FLOATING);
    context_handle_t ctxt_hndl = drcctlib_get_context_handle(drcontext, slot);

    opnd_info_pack_t opnd_info_pack;
    opnd_info_pack.info.size = size;
    opnd_info_pack.info.esize = size;
    opnd_info_pack.info.opnd_type = (uint16_t)(type&0xffff);
    DR_ASSERT((type & 0xffff) == type);

    vtrace_buffer_t* buf = get_trace_buf(vtrace, is_float, size, size);

    cache_VCI_sz_t<size> cache;
    cache.opnd_info = opnd_info_pack;
    cache.ctxt_hndl = ctxt_hndl;
    cache.user_data = (*global_client_cb.user_data_cb)(drcontext, instr, bb, *opnd);
    if(opnd_is_reg(*opnd)) {
        reg_get_value_ex(opnd_get_reg(*opnd), &mcontext, (byte*)cache.val);
        vtracer_update_clean_call<cache_VCI_sz_t<size>>(drcontext, buf, cache);
    } else {
        app_pc addr;
        bool is_write;
        uint32_t pos;
        for( int index=0; instr_compute_address_ex_pos(instr, &mcontext, index, &addr, &is_write, &pos); ++index ) {
            DR_ASSERT(!is_write);
            memcpy((void*)cache.val, (void*)addr, size);
            vtracer_update_clean_call<cache_VCI_sz_t<size>>(drcontext, buf, cache);
        }
    }
}

template<uint8_t size>
void handleVCSZ(vtrace_t *vtrace, int slot, instrlist_t *bb, instr_t* instr, opnd_t *opnd, uint32_t type) {
    void *drcontext = dr_get_current_drcontext();
    dr_mcontext_t mcontext;
    mcontext.size = sizeof(mcontext);
    mcontext.flags = DR_MC_ALL;
    dr_get_mcontext(drcontext, &mcontext);

    bool is_float = TEST_OPND_MASK(type, IS_FLOATING);
    context_handle_t ctxt_hndl = drcctlib_get_context_handle(drcontext, slot);

    opnd_info_pack_t opnd_info_pack;
    opnd_info_pack.info.size = size;
    opnd_info_pack.info.esize = size;
    opnd_info_pack.info.opnd_type = (uint16_t)(type&0xffff);
    DR_ASSERT((type & 0xffff) == type);

    vtrace_buffer_t* buf = get_trace_buf(vtrace, is_float, size, size);

    cache_VC_sz_t<size> cache;
    cache.opnd_info = opnd_info_pack;
    cache.ctxt_hndl = ctxt_hndl;
    if(opnd_is_reg(*opnd)) {
        reg_get_value_ex(opnd_get_reg(*opnd), &mcontext, (byte*)cache.val);
        vtracer_update_clean_call<cache_VC_sz_t<size>>(drcontext, buf, cache);
    } else {
        app_pc addr;
        bool is_write;
        uint32_t pos;
        for( int index=0; instr_compute_address_ex_pos(instr, &mcontext, index, &addr, &is_write, &pos); ++index ) {
            DR_ASSERT(!is_write);
            memcpy((void*)cache.val, (void*)addr, size);
            vtracer_update_clean_call<cache_VC_sz_t<size>>(drcontext, buf, cache);
        }
    }
}

template<uint8_t size>
void handleVAISZ(vtrace_t *vtrace, int slot, instrlist_t *bb, instr_t* instr, opnd_t *opnd, uint32_t type) {
    void *drcontext = dr_get_current_drcontext();
    dr_mcontext_t mcontext;
    mcontext.size = sizeof(mcontext);
    mcontext.flags = DR_MC_ALL;
    dr_get_mcontext(drcontext, &mcontext);

    bool is_float = TEST_OPND_MASK(type, IS_FLOATING);

    opnd_info_pack_t opnd_info_pack;
    opnd_info_pack.info.size = size;
    opnd_info_pack.info.esize = size;
    opnd_info_pack.info.opnd_type = (uint16_t)(type&0xffff);
    DR_ASSERT((type & 0xffff) == type);

    vtrace_buffer_t* buf = get_trace_buf(vtrace, is_float, size, size);

    cache_VAI_sz_t<size> cache;
    cache.opnd_info = opnd_info_pack;
    cache.user_data = (*global_client_cb.user_data_cb)(drcontext, instr, bb, *opnd);
    if(opnd_is_reg(*opnd)) {
        cache.addr = (uint64_t)opnd_get_reg(*opnd);
       reg_get_value_ex((reg_id_t)cache.addr, &mcontext, (byte*)cache.val);
        vtracer_update_clean_call<cache_VAI_sz_t<size>>(drcontext, buf, cache);
    } else {
        app_pc addr;
        bool is_write;
        uint32_t pos;
        for( int index=0; instr_compute_address_ex_pos(instr, &mcontext, index, &addr, &is_write, &pos); ++index ) {
            DR_ASSERT(!is_write);
            cache.addr = (uint64_t)addr;
            memcpy((void*)cache.val, (void*)addr, size);
            vtracer_update_clean_call<cache_VAI_sz_t<size>>(drcontext, buf, cache);
        }
    }
}

template<uint8_t size>
void handleVASZ(vtrace_t *vtrace, int slot, instrlist_t *bb, instr_t* instr, opnd_t *opnd, uint32_t type) {
    void *drcontext = dr_get_current_drcontext();
    dr_mcontext_t mcontext;
    mcontext.size = sizeof(mcontext);
    mcontext.flags = DR_MC_ALL;
    dr_get_mcontext(drcontext, &mcontext);

    bool is_float = TEST_OPND_MASK(type, IS_FLOATING);

    opnd_info_pack_t opnd_info_pack;
    opnd_info_pack.info.size = size;
    opnd_info_pack.info.esize = size;
    opnd_info_pack.info.opnd_type = (uint16_t)(type&0xffff);
    DR_ASSERT((type & 0xffff) == type);

    vtrace_buffer_t* buf = get_trace_buf(vtrace, is_float, size, size);

    cache_VA_sz_t<size> cache;
    cache.opnd_info = opnd_info_pack;
    if(opnd_is_reg(*opnd)) {
        cache.addr = (uint64_t)opnd_get_reg(*opnd);
        reg_get_value_ex((reg_id_t)cache.addr, &mcontext, (byte*)cache.val);
        vtracer_update_clean_call<cache_VA_sz_t<size>>(drcontext, buf, cache);
    } else {
        app_pc addr;
        bool is_write;
        uint32_t pos;
        for( int index=0; instr_compute_address_ex_pos(instr, &mcontext, index, &addr, &is_write, &pos); ++index ) {
            DR_ASSERT(!is_write);
            cache.addr = (uint64_t)addr;
            memcpy((void*)cache.val, (void*)addr, size);
            vtracer_update_clean_call<cache_VA_sz_t<size>>(drcontext, buf, cache);
        }
    }
}

template<uint8_t size>
void handleVISZ(vtrace_t *vtrace, int slot, instrlist_t *bb, instr_t* instr, opnd_t *opnd, uint32_t type) {
    void *drcontext = dr_get_current_drcontext();
    dr_mcontext_t mcontext;
    mcontext.size = sizeof(mcontext);
    mcontext.flags = DR_MC_ALL;
    dr_get_mcontext(drcontext, &mcontext);

    bool is_float = TEST_OPND_MASK(type, IS_FLOATING);

    opnd_info_pack_t opnd_info_pack;
    opnd_info_pack.info.size = size;
    opnd_info_pack.info.esize = size;
    opnd_info_pack.info.opnd_type = (uint16_t)(type&0xffff);
    DR_ASSERT((type & 0xffff) == type);

    vtrace_buffer_t* buf = get_trace_buf(vtrace, is_float, size, size);

    cache_VI_sz_t<size> cache;
    cache.opnd_info = opnd_info_pack;
    cache.user_data = (*global_client_cb.user_data_cb)(drcontext, instr, bb, *opnd);
    if(opnd_is_reg(*opnd)) {
        reg_get_value_ex(opnd_get_reg(*opnd), &mcontext, (byte*)cache.val);
        vtracer_update_clean_call<cache_VI_sz_t<size>>(drcontext, buf, cache);
    } else {
        app_pc addr;
        bool is_write;
        uint32_t pos;
        for( int index=0; instr_compute_address_ex_pos(instr, &mcontext, index, &addr, &is_write, &pos); ++index ) {
            DR_ASSERT(!is_write);
            memcpy((void*)cache.val, (void*)addr, size);
            vtracer_update_clean_call<cache_VI_sz_t<size>>(drcontext, buf, cache);
        }
    }
}

template<uint8_t size>
void handleVSZ(vtrace_t *vtrace, int slot, instrlist_t *bb, instr_t* instr, opnd_t *opnd, uint32_t type) {
    void *drcontext = dr_get_current_drcontext();
    dr_mcontext_t mcontext;
    mcontext.size = sizeof(mcontext);
    mcontext.flags = DR_MC_ALL;
    dr_get_mcontext(drcontext, &mcontext);

    bool is_float = TEST_OPND_MASK(type, IS_FLOATING);

    opnd_info_pack_t opnd_info_pack;
    opnd_info_pack.info.size = size;
    opnd_info_pack.info.esize = size;
    opnd_info_pack.info.opnd_type = (uint16_t)(type&0xffff);
    DR_ASSERT((type & 0xffff) == type);

    vtrace_buffer_t* buf = get_trace_buf(vtrace, is_float, size, size);

    cache_V_sz_t<size> cache;
    cache.opnd_info = opnd_info_pack;
    if(opnd_is_reg(*opnd)) {
        reg_get_value_ex(opnd_get_reg(*opnd), &mcontext, (byte*)cache.val);
        vtracer_update_clean_call<cache_V_sz_t<size>>(drcontext, buf, cache);
    } else {
        app_pc addr;
        bool is_write;
        uint32_t pos;
        for( int index=0; instr_compute_address_ex_pos(instr, &mcontext, index, &addr, &is_write, &pos); ++index ) {
            DR_ASSERT(!is_write);
            memcpy((void*)cache.val, (void*)addr, size);
            vtracer_update_clean_call<cache_V_sz_t<size>>(drcontext, buf, cache);
        }
    }
}

template<uint8_t size>
void handleCAI(vtrace_t *trace, int slot, instrlist_t *bb, instr_t* instr, opnd_t *opnd, uint32_t type) {
    void *drcontext = dr_get_current_drcontext();
    dr_mcontext_t mcontext;
    mcontext.size = sizeof(mcontext);
    mcontext.flags = DR_MC_ALL;
    dr_get_mcontext(drcontext, &mcontext);

    bool is_float = TEST_OPND_MASK(type, IS_FLOATING);
    context_handle_t ctxt_hndl = drcctlib_get_context_handle(drcontext, slot);

    opnd_info_pack_t opnd_info_pack;
    opnd_info_pack.info.size = size;
    opnd_info_pack.info.esize = size;
    opnd_info_pack.info.opnd_type = (uint16_t)(type&0xffff);
    DR_ASSERT((type & 0xffff) == type);

    bool strictly_ordered = TEST_FLAG(trace->trace_flag, VPROFILE_TRACE_STRICTLY_ORDERED);
    vtrace_buffer_t* buf = strictly_ordered ? trace->buff : get_trace_buf(trace, is_float, size, size);

    cache_CAI_t cache;
    cache.opnd_info = opnd_info_pack;
    cache.ctxt_hndl = ctxt_hndl;
    cache.user_data = (*global_client_cb.user_data_cb)(drcontext, instr, bb, *opnd);
    if(opnd_is_reg(*opnd)) {
        cache.addr = (uint64_t)opnd_get_reg(*opnd);
        vtracer_update_clean_call<cache_CAI_t>(drcontext, buf, cache);
    } else {
        app_pc addr;
        bool is_write;
        uint32_t pos;
        for( int index=0; instr_compute_address_ex_pos(instr, &mcontext, index, &addr, &is_write, &pos); ++index ) {
            DR_ASSERT(!is_write);
            cache.addr = (uint64_t)addr;
            vtracer_update_clean_call<cache_CAI_t>(drcontext, buf, cache);
        }
    }
}

template<uint8_t size>
void handleCA(vtrace_t *trace, int slot, instrlist_t *bb, instr_t* instr, opnd_t *opnd, uint32_t type) {
    void *drcontext = dr_get_current_drcontext();
    dr_mcontext_t mcontext;
    mcontext.size = sizeof(mcontext);
    mcontext.flags = DR_MC_ALL;
    dr_get_mcontext(drcontext, &mcontext);

    bool is_float = TEST_OPND_MASK(type, IS_FLOATING);
    context_handle_t ctxt_hndl = drcctlib_get_context_handle(drcontext, slot);

    opnd_info_pack_t opnd_info_pack;
    opnd_info_pack.info.size = size;
    opnd_info_pack.info.esize = size;
    opnd_info_pack.info.opnd_type = (uint16_t)(type&0xffff);
    DR_ASSERT((type & 0xffff) == type);

    bool strictly_ordered = TEST_FLAG(trace->trace_flag, VPROFILE_TRACE_STRICTLY_ORDERED);
    vtrace_buffer_t* buf = strictly_ordered ? trace->buff : get_trace_buf(trace, is_float, size, size);

    cache_CA_t cache;
    cache.opnd_info = opnd_info_pack;
    cache.ctxt_hndl = ctxt_hndl;
    if(opnd_is_reg(*opnd)) {
        cache.addr = (uint64_t)opnd_get_reg(*opnd);
        vtracer_update_clean_call<cache_CA_t>(drcontext, buf, cache);
    } else {
        app_pc addr;
        bool is_write;
        uint32_t pos;
        for( int index=0; instr_compute_address_ex_pos(instr, &mcontext, index, &addr, &is_write, &pos); ++index ) {
            DR_ASSERT(!is_write);
            cache.addr = (uint64_t)addr;
            vtracer_update_clean_call<cache_CA_t>(drcontext, buf, cache);
        }
    }
}

template<uint8_t size>
void handleCI(vtrace_t *trace, int slot, instrlist_t *bb, instr_t* instr, opnd_t *opnd, uint32_t type) {
    void *drcontext = dr_get_current_drcontext();
    dr_mcontext_t mcontext;
    mcontext.size = sizeof(mcontext);
    mcontext.flags = DR_MC_ALL;
    dr_get_mcontext(drcontext, &mcontext);

    bool is_float = TEST_OPND_MASK(type, IS_FLOATING);
    context_handle_t ctxt_hndl = drcctlib_get_context_handle(drcontext, slot);

    opnd_info_pack_t opnd_info_pack;
    opnd_info_pack.info.size = size;
    opnd_info_pack.info.esize = size;
    opnd_info_pack.info.opnd_type = (uint16_t)(type&0xffff);
    DR_ASSERT((type & 0xffff) == type);

    bool strictly_ordered = TEST_FLAG(trace->trace_flag, VPROFILE_TRACE_STRICTLY_ORDERED);
    vtrace_buffer_t* buf = strictly_ordered ? trace->buff : get_trace_buf(trace, is_float, size, size);

    cache_CI_t cache;
    cache.opnd_info = opnd_info_pack;
    cache.ctxt_hndl = ctxt_hndl;
    cache.user_data = (*global_client_cb.user_data_cb)(drcontext, instr, bb, *opnd);
    if(opnd_is_reg(*opnd)) {
        vtracer_update_clean_call<cache_CI_t>(drcontext, buf, cache);
    } else {
        app_pc addr;
        bool is_write;
        uint32_t pos;
        for( int index=0; instr_compute_address_ex_pos(instr, &mcontext, index, &addr, &is_write, &pos); ++index ) {
            DR_ASSERT(!is_write);
            vtracer_update_clean_call<cache_CI_t>(drcontext, buf, cache);
        }
    }
}

template<uint8_t size>
void handleAI(vtrace_t *trace, int slot, instrlist_t *bb, instr_t* instr, opnd_t *opnd, uint32_t type) {
    void *drcontext = dr_get_current_drcontext();
    dr_mcontext_t mcontext;
    mcontext.size = sizeof(mcontext);
    mcontext.flags = DR_MC_ALL;
    dr_get_mcontext(drcontext, &mcontext);

    bool is_float = TEST_OPND_MASK(type, IS_FLOATING);

    opnd_info_pack_t opnd_info_pack;
    opnd_info_pack.info.size = size;
    opnd_info_pack.info.esize = size;
    opnd_info_pack.info.opnd_type = (uint16_t)(type&0xffff);
    DR_ASSERT((type & 0xffff) == type);

    bool strictly_ordered = TEST_FLAG(trace->trace_flag, VPROFILE_TRACE_STRICTLY_ORDERED);
    vtrace_buffer_t* buf = strictly_ordered ? trace->buff : get_trace_buf(trace, is_float, size, size);

    cache_AI_t cache;
    cache.opnd_info = opnd_info_pack;
    cache.user_data = (*global_client_cb.user_data_cb)(drcontext, instr, bb, *opnd);
    if(opnd_is_reg(*opnd)) {
        cache.addr = (uint64_t)opnd_get_reg(*opnd);
        vtracer_update_clean_call<cache_AI_t>(drcontext, buf, cache);
    } else {
        app_pc addr;
        bool is_write;
        uint32_t pos;
        for( int index=0; instr_compute_address_ex_pos(instr, &mcontext, index, &addr, &is_write, &pos); ++index ) {
            DR_ASSERT(!is_write);
            cache.addr = (uint64_t)addr;
            vtracer_update_clean_call<cache_AI_t>(drcontext, buf, cache);
        }
    }
}

template<uint8_t size>
void handleA(vtrace_t *trace, int slot, instrlist_t *bb, instr_t* instr, opnd_t *opnd, uint32_t type) {
    void *drcontext = dr_get_current_drcontext();
    dr_mcontext_t mcontext;
    mcontext.size = sizeof(mcontext);
    mcontext.flags = DR_MC_ALL;
    dr_get_mcontext(drcontext, &mcontext);

    bool is_float = TEST_OPND_MASK(type, IS_FLOATING);

    opnd_info_pack_t opnd_info_pack;
    opnd_info_pack.info.size = size;
    opnd_info_pack.info.esize = size;
    opnd_info_pack.info.opnd_type = (uint16_t)(type&0xffff);
    DR_ASSERT((type & 0xffff) == type);

    bool strictly_ordered = TEST_FLAG(trace->trace_flag, VPROFILE_TRACE_STRICTLY_ORDERED);
    vtrace_buffer_t* buf = strictly_ordered ? trace->buff : get_trace_buf(trace, is_float, size, size);

    cache_A_t cache;
    cache.opnd_info = opnd_info_pack;
    if(opnd_is_reg(*opnd)) {
        cache.addr = (uint64_t)opnd_get_reg(*opnd);
        vtracer_update_clean_call<cache_A_t>(drcontext, buf, cache);
    } else {
        app_pc addr;
        bool is_write;
        uint32_t pos;
        for( int index=0; instr_compute_address_ex_pos(instr, &mcontext, index, &addr, &is_write, &pos); ++index ) {
            DR_ASSERT(!is_write);
            cache.addr = (uint64_t)addr;
            vtracer_update_clean_call<cache_A_t>(drcontext, buf, cache);
        }
    }
}

template<int size>
void handleType(void *drcontext, vtrace_t *vtrace, int slot, instrlist_t *bb, instr_t* instr, instr_t* insert_where, opnd_t opnd, uint32_t type) {
    uint32_t trace_flag = (vtrace->trace_flag & VPROFILE_TRACE_MASK);
    bool strictly_ordered = TEST_FLAG(vtrace->trace_flag, VPROFILE_TRACE_STRICTLY_ORDERED);

    per_thread_t *pt = (per_thread_t *)drmgr_get_tls_field(drcontext, tls_idx);

    instr_t* ins_clone = instr_clone(drcontext, instr);
    opnd_t* opnd_clone = new opnd_t(opnd);

    pt->instr_clones->push_back(ins_clone);
    pt->opnd_clones->push_back(opnd_clone);

    switch(trace_flag) {
        case VPROFILE_TRACE_VAL_CCT_ADDR_INFO: strictly_ordered ? dr_insert_clean_call(drcontext, bb, insert_where, (void *)handleVCAI<size>, false, 6, OPND_CREATE_INTPTR(vtrace), OPND_CREATE_INT32(slot), OPND_CREATE_INTPTR(bb), OPND_CREATE_INTPTR(ins_clone), OPND_CREATE_INTPTR(opnd_clone), OPND_CREATE_INT32(type)) : dr_insert_clean_call(drcontext, bb, insert_where, (void *)handleVCAISZ<size>, false, 6, OPND_CREATE_INTPTR(vtrace), OPND_CREATE_INT32(slot), OPND_CREATE_INTPTR(bb), OPND_CREATE_INTPTR(ins_clone), OPND_CREATE_INTPTR(opnd_clone), OPND_CREATE_INT32(type));break;

        case VPROFILE_TRACE_VAL_CCT_ADDR: strictly_ordered ? dr_insert_clean_call(drcontext, bb, insert_where, (void *)handleVCA<size>, false, 6, OPND_CREATE_INTPTR(vtrace), OPND_CREATE_INT32(slot), OPND_CREATE_INTPTR(bb), OPND_CREATE_INTPTR(ins_clone), OPND_CREATE_INTPTR(opnd_clone), OPND_CREATE_INT32(type)) : dr_insert_clean_call(drcontext, bb, insert_where, (void *)handleVCASZ<size>, false, 6, OPND_CREATE_INTPTR(vtrace), OPND_CREATE_INT32(slot), OPND_CREATE_INTPTR(bb), OPND_CREATE_INTPTR(ins_clone), OPND_CREATE_INTPTR(opnd_clone), OPND_CREATE_INT32(type));break;

        case VPROFILE_TRACE_VAL_CCT_INFO: strictly_ordered ? dr_insert_clean_call(drcontext, bb, insert_where, (void *)handleVCI<size>, false, 6, OPND_CREATE_INTPTR(vtrace), OPND_CREATE_INT32(slot), OPND_CREATE_INTPTR(bb), OPND_CREATE_INTPTR(ins_clone), OPND_CREATE_INTPTR(opnd_clone), OPND_CREATE_INT32(type)) : dr_insert_clean_call(drcontext, bb, insert_where, (void *)handleVCISZ<size>, false, 6, OPND_CREATE_INTPTR(vtrace), OPND_CREATE_INT32(slot), OPND_CREATE_INTPTR(bb), OPND_CREATE_INTPTR(ins_clone), OPND_CREATE_INTPTR(opnd_clone), OPND_CREATE_INT32(type));break;

        case VPROFILE_TRACE_VAL_CCT: strictly_ordered ? dr_insert_clean_call(drcontext, bb, insert_where, (void *)handleVC<size>, false, 6, OPND_CREATE_INTPTR(vtrace), OPND_CREATE_INT32(slot), OPND_CREATE_INTPTR(bb), OPND_CREATE_INTPTR(ins_clone), OPND_CREATE_INTPTR(opnd_clone), OPND_CREATE_INT32(type)) : dr_insert_clean_call(drcontext, bb, insert_where, (void *)handleVCSZ<size>, false, 6, OPND_CREATE_INTPTR(vtrace), OPND_CREATE_INT32(slot), OPND_CREATE_INTPTR(bb), OPND_CREATE_INTPTR(ins_clone), OPND_CREATE_INTPTR(opnd_clone), OPND_CREATE_INT32(type));break;

        case VPROFILE_TRACE_VAL_ADDR_INFO: strictly_ordered ? dr_insert_clean_call(drcontext, bb, insert_where, (void *)handleVAI<size>, false, 6, OPND_CREATE_INTPTR(vtrace), OPND_CREATE_INT32(slot), OPND_CREATE_INTPTR(bb), OPND_CREATE_INTPTR(ins_clone), OPND_CREATE_INTPTR(opnd_clone), OPND_CREATE_INT32(type)) : dr_insert_clean_call(drcontext, bb, insert_where, (void *)handleVAISZ<size>, false, 6, OPND_CREATE_INTPTR(vtrace), OPND_CREATE_INT32(slot), OPND_CREATE_INTPTR(bb), OPND_CREATE_INTPTR(ins_clone), OPND_CREATE_INTPTR(opnd_clone), OPND_CREATE_INT32(type));break;

        case VPROFILE_TRACE_VAL_ADDR: strictly_ordered ? dr_insert_clean_call(drcontext, bb, insert_where, (void *)handleVA<size>, false, 6, OPND_CREATE_INTPTR(vtrace), OPND_CREATE_INT32(slot), OPND_CREATE_INTPTR(bb), OPND_CREATE_INTPTR(ins_clone), OPND_CREATE_INTPTR(opnd_clone), OPND_CREATE_INT32(type)) : dr_insert_clean_call(drcontext, bb, insert_where, (void *)handleVASZ<size>, false, 6, OPND_CREATE_INTPTR(vtrace), OPND_CREATE_INT32(slot), OPND_CREATE_INTPTR(bb), OPND_CREATE_INTPTR(ins_clone), OPND_CREATE_INTPTR(opnd_clone), OPND_CREATE_INT32(type));break;

        case VPROFILE_TRACE_VAL_INFO: strictly_ordered ? dr_insert_clean_call(drcontext, bb, insert_where, (void *)handleVI<size>, false, 6, OPND_CREATE_INTPTR(vtrace), OPND_CREATE_INT32(slot), OPND_CREATE_INTPTR(bb), OPND_CREATE_INTPTR(ins_clone), OPND_CREATE_INTPTR(opnd_clone), OPND_CREATE_INT32(type)) : dr_insert_clean_call(drcontext, bb, insert_where, (void *)handleVISZ<size>, false, 6, OPND_CREATE_INTPTR(vtrace), OPND_CREATE_INT32(slot), OPND_CREATE_INTPTR(bb), OPND_CREATE_INTPTR(ins_clone), OPND_CREATE_INTPTR(opnd_clone), OPND_CREATE_INT32(type));break;

        case VPROFILE_TRACE_VALUE: strictly_ordered ? dr_insert_clean_call(drcontext, bb, insert_where, (void *)handleV<size>, false, 6, OPND_CREATE_INTPTR(vtrace), OPND_CREATE_INT32(slot), OPND_CREATE_INTPTR(bb), OPND_CREATE_INTPTR(ins_clone), OPND_CREATE_INTPTR(opnd_clone), OPND_CREATE_INT32(type)) : dr_insert_clean_call(drcontext, bb, insert_where, (void *)handleVSZ<size>, false, 6, OPND_CREATE_INTPTR(vtrace), OPND_CREATE_INT32(slot), OPND_CREATE_INTPTR(bb), OPND_CREATE_INTPTR(ins_clone), OPND_CREATE_INTPTR(opnd_clone), OPND_CREATE_INT32(type));break;

        case VPROFILE_TRACE_CCT_ADDR_INFO: dr_insert_clean_call(drcontext, bb, insert_where, (void *)handleCAI<size>, false, 6, OPND_CREATE_INTPTR(vtrace), OPND_CREATE_INT32(slot), OPND_CREATE_INTPTR(bb), OPND_CREATE_INTPTR(ins_clone), OPND_CREATE_INTPTR(opnd_clone), OPND_CREATE_INT32(type));break;

        case VPROFILE_TRACE_CCT_ADDR: dr_insert_clean_call(drcontext, bb, insert_where, (void *)handleCA<size>, false, 6, OPND_CREATE_INTPTR(vtrace), OPND_CREATE_INT32(slot), OPND_CREATE_INTPTR(bb), OPND_CREATE_INTPTR(ins_clone), OPND_CREATE_INTPTR(opnd_clone), OPND_CREATE_INT32(type));break;

        case VPROFILE_TRACE_CCT_INFO: dr_insert_clean_call(drcontext, bb, insert_where, (void *)handleCI<size>, false, 6, OPND_CREATE_INTPTR(vtrace), OPND_CREATE_INT32(slot), OPND_CREATE_INTPTR(bb), OPND_CREATE_INTPTR(ins_clone), OPND_CREATE_INTPTR(opnd_clone), OPND_CREATE_INT32(type));

        case VPROFILE_TRACE_ADDR_INFO: dr_insert_clean_call(drcontext, bb, insert_where, (void *)handleAI<size>, false, 6, OPND_CREATE_INTPTR(vtrace), OPND_CREATE_INT32(slot), OPND_CREATE_INTPTR(bb), OPND_CREATE_INTPTR(ins_clone), OPND_CREATE_INTPTR(opnd_clone), OPND_CREATE_INT32(type));break;

        case VPROFILE_TRACE_ADDR: dr_insert_clean_call(drcontext, bb, insert_where, (void *)handleA<size>, false, 6, OPND_CREATE_INTPTR(vtrace), OPND_CREATE_INT32(slot), OPND_CREATE_INTPTR(bb), OPND_CREATE_INTPTR(ins_clone), OPND_CREATE_INTPTR(opnd_clone), OPND_CREATE_INT32(type));break;
        default:
            DR_ASSERT_MSG(false, "Unknown trace flag!");
    }
}

void handleOpnd(void *drcontext, vtrace_t *vtrace, int slot, instrlist_t *bb, instr_t* instr, instr_t* insert_where, opnd_t opnd, uint32_t type) {
    bool is_float = TEST_OPND_MASK(type, IS_FLOATING);
    int esize = (is_float) ? FloatOperandSizeTable(instr, opnd) : IntegerOperandSizeTable(instr, opnd);

    switch(esize) {
        case 4: handleType<4>(drcontext, vtrace, slot, bb, instr, insert_where, opnd, type);break;
        case 8: handleType<8>(drcontext, vtrace, slot, bb, instr, insert_where, opnd, type);break;
        default: {
#ifdef VPROFILE_DEBUG
        debug_unknown_case(dr_get_current_drcontext(), instr, opnd);
#endif
        }
    }
}

// gather:load -- mem read before, reg write before/after
// scatter:store -- mem write before / after, reg read before
void handleVGatherScatter(int slot, instrlist_t *bb, instr_t* instr) {
    void *drcontext = dr_get_current_drcontext();
    if(global_client_cb.ins_instrument_cb)
        (*global_client_cb.ins_instrument_cb)(drcontext, instr, bb);

    for (unsigned int i = 0; i < vtrace_t_list.entries; ++i) {
        vtrace_t *trace = (vtrace_t*)drvector_get_entry(&vtrace_t_list, i);
        if(TEST_FLAG(trace->trace_flag, VPROFILE_TRACE_BEFORE_WRITE)) {
            int num = instr_num_dsts(instr);
            for(int j=0; j<num; ++j) {
                opnd_t opnd = instr_get_dst(instr, j);
                if(opnd_size_in_bytes(opnd_get_size(opnd)) == 0) continue;
                uint32_t opmask = getOpndMask<true, true>(instr, opnd);
                if(FILTER_OPND_MASK(trace->opnd_mask, opmask)) {
                    handleOpnd(drcontext, trace, slot, bb, instr, instr, opnd, opmask);
                }
            }
        }
        int num = instr_num_srcs(instr);
        for(int j=0; j<num; ++j) {
            opnd_t opnd = instr_get_src(instr, j);
            if(opnd_size_in_bytes(opnd_get_size(opnd)) == 0) continue;
            uint32_t opmask = getOpndMask<false, true>(instr, opnd);
            if(FILTER_OPND_MASK(trace->opnd_mask, opmask)) {
                handleOpnd(drcontext, trace, slot, bb, instr, instr, opnd, opmask);
                if(TEST_FLAG(trace->trace_flag, VPROFILE_TRACE_REG_IN_MEMREF) && TEST_OPND_MASK(opmask, MEMORY)) {
                    int reg_num = opnd_num_regs_used(opnd);
                    for(int k = 0; k < reg_num; k++) {
                        opnd_t reg_used = opnd_create_reg(opnd_get_reg_used(opnd, k));
                        int reg_size = opnd_size_in_bytes(opnd_get_size(reg_used));
                        if(reg_size == 0) {
                            continue;
                        }
                        handleOpnd(drcontext, trace, slot, bb, instr, instr, reg_used, opmask);
                    }
                } 
            }
        }
    }

    if(!instr_is_cti(instr) && !instr_is_syscall(instr)) {
        instr_t *insert_where = instr_get_next_app(instr);
        if(!insert_where) {
            instrlist_meta_postinsert(bb, instr, XINST_CREATE_nop(drcontext));
            insert_where = instr_get_next(instr);
        }

        for (unsigned int i = 0; i < vtrace_t_list.entries; ++i) {
            vtrace_t *trace = (vtrace_t*)drvector_get_entry(&vtrace_t_list, i);
            int num = instr_num_dsts(instr);
            for(int j=0; j<num; ++j) {
                opnd_t opnd = instr_get_dst(instr, j);
                if(opnd_size_in_bytes(opnd_get_size(opnd)) == 0) continue;
                uint32_t opmask = getOpndMask<true, false>(instr, opnd);
                if(FILTER_OPND_MASK(trace->opnd_mask, opmask)) {
                    handleOpnd(drcontext, trace, slot, bb, instr, insert_where, opnd, opmask);
                    if(TEST_FLAG(trace->trace_flag, VPROFILE_TRACE_REG_IN_MEMREF) && TEST_OPND_MASK(opmask, MEMORY)) {
                        int reg_num = opnd_num_regs_used(opnd);
                        for(int k = 0; k < reg_num; k++) {
                            opnd_t reg_used = opnd_create_reg(opnd_get_reg_used(opnd, k));
                            int reg_size = opnd_size_in_bytes(opnd_get_size(reg_used));
                            if(reg_size == 0) {
                                continue;
                            }
                            handleOpnd(drcontext, trace, slot, bb, instr, insert_where, reg_used, opmask);
                        }
                    } 
                }
            }
        }
    }
}
#endif

void
InstrumentInsCallback(void *drcontext, instr_instrument_msg_t *instrument_msg)
{
    // quick return when there are no entries exist
    if(!any_traces_created) {
        return;
    }
    instrlist_t *bb = instrument_msg->bb;
    instr_t *instr = instrument_msg->instr;
    int32_t slot = instrument_msg->slot;

    if (slot == 0) {
        if(global_client_cb.bb_instrument_cb)
            (*global_client_cb.bb_instrument_cb)(drcontext, bb);
    } 
#ifdef X86
    if(instr_is_gather(instr) || instr_is_scatter(instr)) {
        handleVGatherScatter(slot, bb, instr);
    } else 
#endif
        InstrumentInstruction<true>(drcontext, bb, instr, slot);
}

static dr_emit_flags_t
event_basic_block_default(void *drcontext, void *tag, instrlist_t *bb, bool for_trace, bool translating, OUT void **user_data)
{
    instr_t *instr;
    // client cb
    if(global_client_cb.bb_instrument_cb)
        (*global_client_cb.bb_instrument_cb)(drcontext, bb);

    for (instr = instrlist_first(bb); instr != NULL; instr = instr_get_next(instr)) {
        if(!instr_is_app(instr) || instr_is_ignorable(instr)) continue;
        if(global_instr_filter(instr)) {
#ifdef X86
            if(instr_is_gather(instr) || instr_is_scatter(instr)) {
                handleVGatherScatter(0, bb, instr);
            } else 
#endif
                InstrumentInstruction<false>(drcontext, bb, instr, 0/*not used*/);
        }
    }
    
    return DR_EMIT_DEFAULT;
}

void vprofile_register_instr_filter(bool (*filter)(instr_t *))
{
    global_instr_filter = filter;
}

void vprofile_register_client_cb(void* (*user_data_cb)(void *, instr_t *, instrlist_t *, opnd_t),
                                 void (*ins_instrument_cb)(void *, instr_t *, instrlist_t *),
                                 void (*bb_instrument_cb)(void *, instrlist_t *))
{
    global_client_cb.user_data_cb = user_data_cb;
    global_client_cb.ins_instrument_cb = ins_instrument_cb;
    global_client_cb.bb_instrument_cb = bb_instrument_cb;
}
#ifdef X86
static void
ClientThreadEnd(void *drcontext)
{
    per_thread_t *pt = (per_thread_t *)drmgr_get_tls_field(drcontext, tls_idx);
    delete pt->instr_clones;
    delete pt->opnd_clones;
    dr_thread_free(drcontext, pt, sizeof(per_thread_t));
}

static void
ClientThreadStart(void *drcontext)
{
    per_thread_t *pt = (per_thread_t *)dr_thread_alloc(drcontext, sizeof(per_thread_t));
    if (pt == NULL) {
        DR_ASSERT_MSG(0, "pt == NULL\n");
    }
    pt->opnd_clones = new std::vector<opnd_t*>();
    pt->instr_clones = new std::vector<instr_t*>();
    drmgr_set_tls_field(drcontext, tls_idx, (void *)pt);
}

#endif
bool vprofile_init(bool (*filter)(instr_t *),
                   void* (*user_data_cb)(void *, instr_t *, instrlist_t *, opnd_t),
                   void (*ins_instrument_cb)(void *, instr_t *, instrlist_t *),
                   void (*bb_instrument_cb)(void *, instrlist_t *),
                   uint8_t flag)
{
#ifdef VPROFILE_DEBUG
    LOG_INIT(EVERYTHING);
    pid_t pid = getpid();
#ifdef ARM_CCTLIB
    char name[MAXIMUM_PATH] = "arm-";
#else
    char name[MAXIMUM_PATH] = "x86-";
#endif
    gethostname(name + strlen(name), MAXIMUM_PATH - strlen(name));
    sprintf(name + strlen(name), "-%d-vprofile", pid);
    sprintf(name+strlen(name), ".debug");
    gDebug = dr_open_file(name, DR_FILE_WRITE_OVERWRITE | DR_FILE_ALLOW_LARGE);
    DR_ASSERT(gDebug != INVALID_FILE);
#endif

    vprofile_register_instr_filter(filter);
    vprofile_register_client_cb(user_data_cb, ins_instrument_cb, bb_instrument_cb);
    global_flags = flag;

    if(!vtracer_init()) {
        DR_ASSERT_MSG(false, "ERROR: vprofile unable to initialize vtracer");
    }
#ifdef X86
    drmgr_priority_t thread_init_pri = { sizeof(thread_init_pri),
                                         "vprofile-thread-init", NULL, NULL,
                                         DRCCTLIB_THREAD_EVENT_PRI + 1 };
    drmgr_priority_t thread_exit_pri = { sizeof(thread_exit_pri),
                                         "vprofile-thread-exit", NULL, NULL,
                                         DRCCTLIB_THREAD_EVENT_PRI - 1 };

    if (   !drmgr_register_thread_init_event_ex(ClientThreadStart, &thread_init_pri) 
        || !drmgr_register_thread_exit_event_ex(ClientThreadEnd, &thread_exit_pri) ) {
        DR_ASSERT_MSG(false, "ERROR: vprofile unable to register events");
    }

    tls_idx = drmgr_register_tls_field();
    if (tls_idx == -1) {
        DR_ASSERT_MSG(false, "ERROR: vprofile drmgr_register_tls_field fail");
    }
    if (!dr_raw_tls_calloc(&tls_seg, &tls_offs, INSTRACE_TLS_COUNT, 0)) {
        DR_ASSERT_MSG(false, "ERROR: vprofile dr_raw_tls_calloc fail");
    }

#endif
    if(!drvector_init(&vtrace_t_list, 1, false /*!synch*/, NULL)) {
        DR_ASSERT_MSG(false, "ERROR: vprofile unable to init drvector");
    }

    if(flag == VPROFILE_DEFAULT) {
        if(!drmgr_register_bb_instrumentation_event(event_basic_block_default, NULL, NULL)) {
            DR_ASSERT_MSG(false, "ERROR: vprofile unable to register events");
        }
        return true;
    }

    if((flag & (VPROFILE_COLLECT_DATAOBJ_TREE_BASED | VPROFILE_COLLECT_DATAOBJ_ADDR_RANGE)) != 0) {
        DR_ASSERT_MSG(false, "Not Implemented for VPROFILE_COLLECT_DATAOBJ_TREE_BASED and VPROFILE_COLLECT_DATAOBJ_ADDR_RANGE modes!");
    }

    if((flag & VPROFILE_COLLECT_DATAOBJ) != 0) {
        drcctlib_init_ex(filter, INVALID_FILE, InstrumentInsCallback, NULL, NULL, DRCCTLIB_COLLECT_DATA_CENTRIC_MESSAGE | DRCCTLIB_CACHE_MODE);
    } else if((flag & VPROFILE_COLLECT_CCT) != 0) {
        /* Currently, DRCCTLIB_CACHE_MODE is specified to implement mem_ref only traces */
        // drcctlib_init_ex(filter, INVALID_FILE, InstrumentInsCallback, NULL, NULL, DRCCTLIB_CACHE_MODE);
        // We use the default mode for CCT-only for correctness
        // TODO: Modify DRCCTLib to support more flex cache mode implementation
        drcctlib_init_ex(filter, INVALID_FILE, InstrumentInsCallback, NULL, NULL, DRCCTLIB_DEFAULT);
    }
    return true;
}

void vprofile_exit()
{
#ifdef X86
    drmgr_unregister_thread_init_event(ClientThreadStart);
    drmgr_unregister_thread_exit_event(ClientThreadEnd);
    drmgr_unregister_tls_field(tls_idx);
    dr_raw_tls_cfree(tls_offs, INSTRACE_TLS_COUNT);
#endif

#ifdef VPROFILE_DEBUG
    LOG_FINI();
    dr_close_file(gDebug);
#endif
    if(global_flags != VPROFILE_DEFAULT)
        drcctlib_exit();
    else drmgr_unregister_bb_instrumentation_event(event_basic_block_default);
    drvector_delete(&vtrace_t_list);
    vtracer_exit();
}

vtrace_t* vprofile_allocate_trace(uint32_t trace_flag)
{
    DR_ASSERT_MSG((trace_flag&VPROFILE_TRACE_MASK)!=VPROFILE_TRACE_INVALID,
        "Usage Error: trace_flag not correctly configured for vprofile_allocate_trace()!");
    if(TEST_FLAG(trace_flag,VPROFILE_TRACE_INFO)) {
        DR_ASSERT_MSG(global_client_cb.user_data_cb!=NULL, 
            "Usage Error: VPROFILE_TRACE_INFO is set while no user_data_cb registered to obtain the user-defined info for tracing!");
    }
    vtrace_t *new_trace;
    new_trace = (vtrace_t*)dr_global_alloc(sizeof(*new_trace));
    new_trace->trace_flag = trace_flag;
    if(!TEST_FLAG(trace_flag, VPROFILE_TRACE_STRICTLY_ORDERED)) {
        for(int i=0; i<NUM_DATA_TYPES; ++i) {
            new_trace->buff_ex[i] = NULL;
        }
    } else {
        new_trace->buff = NULL;
    }
    drvector_append(&vtrace_t_list, new_trace);
    if (!any_traces_created)
        any_traces_created = true;

    return new_trace;
}

template<int sz>
void vprofile_update_VCAI_cb(void *buf_base, void *buf_end, void* user_data)
{
    cache_VCAI_sz_t<sz>* cache_ptr = (cache_VCAI_sz_t<sz>*)buf_base;
    cache_VCAI_sz_t<sz>* cache_end = (cache_VCAI_sz_t<sz>*)buf_end;
    val_info_t user_info;
    for(; cache_ptr<cache_end; ++cache_ptr) {
        // extract data from cache
        user_info.type = cache_ptr->opnd_info.info.opnd_type;
        user_info.is_float = TEST_OPND_MASK(cache_ptr->opnd_info.info.opnd_type, IS_FLOATING);
        user_info.size = cache_ptr->opnd_info.info.size;
        user_info.esize = cache_ptr->opnd_info.info.esize;
        user_info.addr = cache_ptr->addr;
        user_info.ctxt_hndl = cache_ptr->ctxt_hndl;
        user_info.val = (void *) cache_ptr->val;
        user_info.info = cache_ptr->user_data;
        (*((void (*)(val_info_t *)) user_data))(&user_info);
    }
}

template<int sz>
void vprofile_update_VCI_cb(void *buf_base, void *buf_end, void* user_data)
{
    cache_VCI_sz_t<sz>* cache_ptr = (cache_VCI_sz_t<sz>*)buf_base;
    cache_VCI_sz_t<sz>* cache_end = (cache_VCI_sz_t<sz>*)buf_end;
    val_info_t user_info;
    for(; cache_ptr<cache_end; ++cache_ptr) {
        // extract data from cache
        user_info.type = cache_ptr->opnd_info.info.opnd_type;
        user_info.is_float = TEST_OPND_MASK(cache_ptr->opnd_info.info.opnd_type, IS_FLOATING);
        user_info.size = cache_ptr->opnd_info.info.size;
        user_info.esize = cache_ptr->opnd_info.info.esize;
        user_info.ctxt_hndl = cache_ptr->ctxt_hndl;
        user_info.val = (void *) cache_ptr->val;
        user_info.info = cache_ptr->user_data;
        (*((void (*)(val_info_t *)) user_data))(&user_info);
    }
}

template<int sz>
void vprofile_update_VCA_cb(void *buf_base, void *buf_end, void* user_data)
{
    cache_VCA_sz_t<sz>* cache_ptr = (cache_VCA_sz_t<sz>*)buf_base;
    cache_VCA_sz_t<sz>* cache_end = (cache_VCA_sz_t<sz>*)buf_end;
    val_info_t user_info;
    for(; cache_ptr<cache_end; ++cache_ptr) {
        // extract data from cache
        user_info.type = cache_ptr->opnd_info.info.opnd_type;
        user_info.is_float = TEST_OPND_MASK(cache_ptr->opnd_info.info.opnd_type, IS_FLOATING);
        user_info.size = cache_ptr->opnd_info.info.size;
        user_info.esize = cache_ptr->opnd_info.info.esize;
        user_info.addr = cache_ptr->addr;
        user_info.ctxt_hndl = cache_ptr->ctxt_hndl;
        user_info.val = (void *) cache_ptr->val;
        (*((void (*)(val_info_t *)) user_data))(&user_info);
    }
}

template<int sz>
void vprofile_update_VC_cb(void *buf_base, void *buf_end, void* user_data)
{
    cache_VC_sz_t<sz>* cache_ptr = (cache_VC_sz_t<sz>*)buf_base;
    cache_VC_sz_t<sz>* cache_end = (cache_VC_sz_t<sz>*)buf_end;
    val_info_t user_info;
    for(; cache_ptr<cache_end; ++cache_ptr) {
        // extract data from cache
        user_info.type = cache_ptr->opnd_info.info.opnd_type;
        user_info.is_float = TEST_OPND_MASK(cache_ptr->opnd_info.info.opnd_type, IS_FLOATING);
        user_info.size = cache_ptr->opnd_info.info.size;
        user_info.esize = cache_ptr->opnd_info.info.esize;
        user_info.ctxt_hndl = cache_ptr->ctxt_hndl;
        user_info.val = (void *) cache_ptr->val;
        (*((void (*)(val_info_t *)) user_data))(&user_info);
    }
}

template<int sz>
void vprofile_update_VAI_cb(void *buf_base, void *buf_end, void* user_data)
{
    cache_VAI_sz_t<sz>* cache_ptr = (cache_VAI_sz_t<sz>*)buf_base;
    cache_VAI_sz_t<sz>* cache_end = (cache_VAI_sz_t<sz>*)buf_end;
    val_info_t user_info;
    for(; cache_ptr<cache_end; ++cache_ptr) {
        // extract data from cache
        user_info.type = cache_ptr->opnd_info.info.opnd_type;
        user_info.is_float = TEST_OPND_MASK(cache_ptr->opnd_info.info.opnd_type, IS_FLOATING);
        user_info.size = cache_ptr->opnd_info.info.size;
        user_info.esize = cache_ptr->opnd_info.info.esize;
        user_info.addr = cache_ptr->addr;
        user_info.val = (void *) cache_ptr->val;
        user_info.info = cache_ptr->user_data;
        (*((void (*)(val_info_t *)) user_data))(&user_info);
    }
}

template<int sz>
void vprofile_update_VA_cb(void *buf_base, void *buf_end, void* user_data)
{
    cache_VA_sz_t<sz>* cache_ptr = (cache_VA_sz_t<sz>*)buf_base;
    cache_VA_sz_t<sz>* cache_end = (cache_VA_sz_t<sz>*)buf_end;
    val_info_t user_info;
    for(; cache_ptr<cache_end; ++cache_ptr) {
        // extract data from cache
        user_info.type = cache_ptr->opnd_info.info.opnd_type;
        user_info.is_float = TEST_OPND_MASK(cache_ptr->opnd_info.info.opnd_type, IS_FLOATING);
        user_info.size = cache_ptr->opnd_info.info.size;
        user_info.esize = cache_ptr->opnd_info.info.esize;
        user_info.addr = cache_ptr->addr;
        user_info.val = (void *) cache_ptr->val;
        (*((void (*)(val_info_t *)) user_data))(&user_info);
    }
}

template<int sz>
void vprofile_update_VI_cb(void *buf_base, void *buf_end, void* user_data)
{
    cache_VI_sz_t<sz>* cache_ptr = (cache_VI_sz_t<sz>*)buf_base;
    cache_VI_sz_t<sz>* cache_end = (cache_VI_sz_t<sz>*)buf_end;
    val_info_t user_info;
    for(; cache_ptr<cache_end; ++cache_ptr) {
        // extract data from cache
        user_info.type = cache_ptr->opnd_info.info.opnd_type;
        user_info.is_float = TEST_OPND_MASK(cache_ptr->opnd_info.info.opnd_type, IS_FLOATING);
        user_info.size = cache_ptr->opnd_info.info.size;
        user_info.esize = cache_ptr->opnd_info.info.esize;
        user_info.val = (void *) cache_ptr->val;
        user_info.info = cache_ptr->user_data;
        (*((void (*)(val_info_t *)) user_data))(&user_info);
    }
}

template<int sz>
void vprofile_update_V_cb(void *buf_base, void *buf_end, void* user_data)
{
    cache_V_sz_t<sz>* cache_ptr = (cache_V_sz_t<sz>*)buf_base;
    cache_V_sz_t<sz>* cache_end = (cache_V_sz_t<sz>*)buf_end;
    val_info_t user_info;
    for(; cache_ptr<cache_end; ++cache_ptr) {
        // extract data from cache
        user_info.type = cache_ptr->opnd_info.info.opnd_type;
        user_info.is_float = TEST_OPND_MASK(cache_ptr->opnd_info.info.opnd_type, IS_FLOATING);
        user_info.size = cache_ptr->opnd_info.info.size;
        user_info.esize = cache_ptr->opnd_info.info.esize;
        user_info.val = (void *) cache_ptr->val;
        (*((void (*)(val_info_t *)) user_data))(&user_info);
    }
}

void vprofile_update_CAI_cb(void *buf_base, void *buf_end, void* user_data)
{
    cache_CAI_t* cache_ptr = (cache_CAI_t*)buf_base;
    cache_CAI_t* cache_end = (cache_CAI_t*)buf_end;
    val_info_t user_info;
    for(; cache_ptr<cache_end; ++cache_ptr) {
        // extract data from cache
        user_info.type = cache_ptr->opnd_info.info.opnd_type;
        user_info.is_float = TEST_OPND_MASK(cache_ptr->opnd_info.info.opnd_type, IS_FLOATING);
        user_info.size = cache_ptr->opnd_info.info.size;
        user_info.esize = cache_ptr->opnd_info.info.esize;
        user_info.addr = cache_ptr->addr;
        user_info.ctxt_hndl = cache_ptr->ctxt_hndl;
        user_info.info = cache_ptr->user_data;
        (*((void (*)(val_info_t *)) user_data))(&user_info);
    }
}

void vprofile_update_CA_cb(void *buf_base, void *buf_end, void* user_data)
{
    cache_CA_t* cache_ptr = (cache_CA_t*)buf_base;
    cache_CA_t* cache_end = (cache_CA_t*)buf_end;
    val_info_t user_info;
    for(; cache_ptr<cache_end; ++cache_ptr) {
        // extract data from cache
        user_info.type = cache_ptr->opnd_info.info.opnd_type;
        user_info.is_float = TEST_OPND_MASK(cache_ptr->opnd_info.info.opnd_type, IS_FLOATING);
        user_info.size = cache_ptr->opnd_info.info.size;
        user_info.esize = cache_ptr->opnd_info.info.esize;
        user_info.addr = cache_ptr->addr;
        user_info.ctxt_hndl = cache_ptr->ctxt_hndl;
        (*((void (*)(val_info_t *)) user_data))(&user_info);
    }
}

void vprofile_update_CI_cb(void *buf_base, void *buf_end, void* user_data)
{
    cache_CI_t* cache_ptr = (cache_CI_t*)buf_base;
    cache_CI_t* cache_end = (cache_CI_t*)buf_end;
    val_info_t user_info;
    for(; cache_ptr<cache_end; ++cache_ptr) {
        // extract data from cache
        user_info.type = cache_ptr->opnd_info.info.opnd_type;
        user_info.is_float = TEST_OPND_MASK(cache_ptr->opnd_info.info.opnd_type, IS_FLOATING);
        user_info.size = cache_ptr->opnd_info.info.size;
        user_info.esize = cache_ptr->opnd_info.info.esize;
        user_info.ctxt_hndl = cache_ptr->ctxt_hndl;
        user_info.info = cache_ptr->user_data;
        (*((void (*)(val_info_t *)) user_data))(&user_info);
    }
}

void vprofile_update_AI_cb(void *buf_base, void *buf_end, void* user_data)
{
    cache_AI_t* cache_ptr = (cache_AI_t*)buf_base;
    cache_AI_t* cache_end = (cache_AI_t*)buf_end;
    val_info_t user_info;
    for(; cache_ptr<cache_end; ++cache_ptr) {
        // extract data from cache
        user_info.type = cache_ptr->opnd_info.info.opnd_type;
        user_info.is_float = TEST_OPND_MASK(cache_ptr->opnd_info.info.opnd_type, IS_FLOATING);
        user_info.size = cache_ptr->opnd_info.info.size;
        user_info.esize = cache_ptr->opnd_info.info.esize;
        user_info.addr = cache_ptr->addr;
        user_info.info = cache_ptr->user_data;
        (*((void (*)(val_info_t *)) user_data))(&user_info);
    }
}

void vprofile_update_A_cb(void *buf_base, void *buf_end, void* user_data)
{
    cache_A_t* cache_ptr = (cache_A_t*)buf_base;
    cache_A_t* cache_end = (cache_A_t*)buf_end;
    val_info_t user_info;
    for(; cache_ptr<cache_end; ++cache_ptr) {
        // extract data from cache
        user_info.type = cache_ptr->opnd_info.info.opnd_type;
        user_info.is_float = TEST_OPND_MASK(cache_ptr->opnd_info.info.opnd_type, IS_FLOATING);
        user_info.size = cache_ptr->opnd_info.info.size;
        user_info.esize = cache_ptr->opnd_info.info.esize;
        user_info.addr = cache_ptr->addr;
        (*((void (*)(val_info_t *)) user_data))(&user_info);
    }
}

template <typename T, bool trace_before_write>
size_t
vprofile_fill_num_cb(void *drcontext, instr_t *where, void* user_data)
{
    size_t fill_num = 0;
    if(!global_instr_filter(where)) return fill_num;
    int num = instr_num_dsts(where);
    for(int j=0; j<num; ++j) {
        opnd_t opnd = instr_get_dst(where, j);
        if(opnd_size_in_bytes(opnd_get_size(opnd)) == 0) continue;

        if(!instr_is_cti(where) && !instr_is_syscall(where)) {
            uint32_t opndmask = getOpndMask<true, false>(where, opnd);
            if((*((bool (*)(opnd_t, vprofile_src_t)) user_data))(opnd, (vprofile_src_t)opndmask)) {
                fill_num += sizeof(T);
            }
        }

        if(trace_before_write) {
            uint32_t opndmask = getOpndMask<true, true>(where, opnd);
            if((*((bool (*)(opnd_t, vprofile_src_t)) user_data))(opnd, (vprofile_src_t)opndmask)) {
                fill_num += sizeof(T);
            }
        }
    }

    num = instr_num_srcs(where);
    for(int j=0; j<num; ++j) {
        opnd_t opnd = instr_get_src(where, j);
        if(opnd_size_in_bytes(opnd_get_size(opnd)) == 0) continue;
        uint32_t opndmask = getOpndMask<false, true>(where, opnd);
        if((*((bool (*)(opnd_t, vprofile_src_t)) user_data))(opnd, (vprofile_src_t)opndmask)) {
            fill_num += sizeof(T);
        }
    }

    return fill_num;
}

template<typename T, int sz, int esize, bool is_float, bool trace_before_write>
size_t vprofile_fill_num_type_specialized_cb(void *drcontext, instr_t *where, void* user_data)
{
    size_t fill_num = 0;
    if(!global_instr_filter(where)) return fill_num;
    int num = instr_num_dsts(where);
    for(int j=0; j<num; ++j) {
        opnd_t opnd = instr_get_dst(where, j);
        int esize_actual = (is_float) ? FloatOperandSizeTable(where, opnd) : IntegerOperandSizeTable(where, opnd);

        if(!instr_is_cti(where) && !instr_is_syscall(where)) {
            uint32_t opndmask = getOpndMask<true, false>(where, opnd);
            if((*((bool (*)(opnd_t, vprofile_src_t)) user_data))(opnd, (vprofile_src_t)opndmask) && 
               TEST_OPND_MASK(opndmask, IS_FLOATING) == is_float && 
               esize == esize_actual && 
               opnd_size_in_bytes(opnd_get_size(opnd)) == sz) {
                fill_num += sizeof(T);
            }
        }

        if(trace_before_write) {
            uint32_t opndmask = getOpndMask<true, true>(where, opnd);
            if((*((bool (*)(opnd_t, vprofile_src_t)) user_data))(opnd, (vprofile_src_t)opndmask) &&
              TEST_OPND_MASK(opndmask, IS_FLOATING) == is_float &&
              esize == esize_actual &&
              opnd_size_in_bytes(opnd_get_size(opnd)) == sz) {
                fill_num += sizeof(T);
            }
        }
    }

    num = instr_num_srcs(where);
    for(int j=0; j<num; ++j) {
        opnd_t opnd = instr_get_src(where, j);
        uint32_t opndmask = getOpndMask<false, true>(where, opnd);
        int esize_actual = (is_float) ? FloatOperandSizeTable(where, opnd) : IntegerOperandSizeTable(where, opnd);
        if((*((bool (*)(opnd_t, vprofile_src_t)) user_data))(opnd, (vprofile_src_t)opndmask) && 
          TEST_OPND_MASK(opndmask, IS_FLOATING) == is_float && 
          esize == esize_actual && 
          opnd_size_in_bytes(opnd_get_size(opnd)) == sz) {
            fill_num += sizeof(T);
        }
    }

    return fill_num;
}

template<int sz, int esize, bool is_float, bool trace_before_write>
void vprofile_register_trace_cb_impl(vtrace_buffer_t* buff, 
                                     bool (*filter)(opnd_t, vprofile_src_t), 
                                     void (*update_cb)(val_info_t *),
                                     uint32_t trace_flag)
{
    buff->user_data_full = (void *)update_cb;
    buff->user_data_fill_num = (void *)filter;

    uint32_t masked_trace_flag = (trace_flag & VPROFILE_TRACE_MASK);
    bool strictly_ordered = TEST_FLAG(trace_flag, VPROFILE_TRACE_STRICTLY_ORDERED);

    switch(masked_trace_flag) {
        case VPROFILE_TRACE_VAL_CCT_ADDR_INFO:
            if(strictly_ordered) {
                buff->full_cb = vprofile_update_VCAI_cb<MAX_CLASS_SIZE>;
                buff->fill_num_cb = vprofile_fill_num_cb<cache_VCAI_t, trace_before_write>;
            } else {
                buff->full_cb = vprofile_update_VCAI_cb<sz>;
                buff->fill_num_cb = vprofile_fill_num_type_specialized_cb<cache_VCAI_sz_t<sz>, sz, esize, is_float, trace_before_write>;
            }
            break;
        case VPROFILE_TRACE_VAL_CCT_ADDR:
            if(strictly_ordered) {
                buff->full_cb = vprofile_update_VCA_cb<MAX_CLASS_SIZE>;
                buff->fill_num_cb = vprofile_fill_num_cb<cache_VCA_t, trace_before_write>;
            } else {
                buff->full_cb = vprofile_update_VCA_cb<sz>;
                buff->fill_num_cb = vprofile_fill_num_type_specialized_cb<cache_VCA_sz_t<sz>, sz, esize, is_float, trace_before_write>;
            }
            break;
        case VPROFILE_TRACE_VAL_CCT_INFO:
            if(strictly_ordered) {
                buff->full_cb = vprofile_update_VCI_cb<MAX_CLASS_SIZE>;
                buff->fill_num_cb = vprofile_fill_num_cb<cache_VCI_t, trace_before_write>;
            } else {
                buff->full_cb = vprofile_update_VCI_cb<sz>;
                buff->fill_num_cb = vprofile_fill_num_type_specialized_cb<cache_VCI_sz_t<sz>, sz, esize, is_float, trace_before_write>;
            }
            break;
        case VPROFILE_TRACE_VAL_CCT:
            if(strictly_ordered) {
                buff->full_cb = vprofile_update_VC_cb<MAX_CLASS_SIZE>;
                buff->fill_num_cb = vprofile_fill_num_cb<cache_VC_t, trace_before_write>;
            } else {
                buff->full_cb = vprofile_update_VC_cb<sz>;
                buff->fill_num_cb = vprofile_fill_num_type_specialized_cb<cache_VC_sz_t<sz>, sz, esize, is_float, trace_before_write>;
            }
            break;
        case VPROFILE_TRACE_VAL_ADDR_INFO:
            if(strictly_ordered) {
                buff->full_cb = vprofile_update_VAI_cb<MAX_CLASS_SIZE>;
                buff->fill_num_cb = vprofile_fill_num_cb<cache_VAI_t, trace_before_write>;
            } else {
                buff->full_cb = vprofile_update_VAI_cb<sz>;
                buff->fill_num_cb = vprofile_fill_num_type_specialized_cb<cache_VAI_sz_t<sz>, sz, esize, is_float, trace_before_write>;
            }
            break;
        case VPROFILE_TRACE_VAL_ADDR:
            if(strictly_ordered) {
                buff->full_cb = vprofile_update_VA_cb<MAX_CLASS_SIZE>;
                buff->fill_num_cb = vprofile_fill_num_cb<cache_VA_t, trace_before_write>;
            } else {
                buff->full_cb = vprofile_update_VA_cb<sz>;
                buff->fill_num_cb = vprofile_fill_num_type_specialized_cb<cache_VA_sz_t<sz>, sz, esize, is_float, trace_before_write>;
            }
            break;
        case VPROFILE_TRACE_VAL_INFO:
            if(strictly_ordered) {
                buff->full_cb = vprofile_update_VI_cb<MAX_CLASS_SIZE>;
                buff->fill_num_cb = vprofile_fill_num_cb<cache_VI_t, trace_before_write>;
            } else {
                buff->full_cb = vprofile_update_VI_cb<sz>;
                buff->fill_num_cb = vprofile_fill_num_type_specialized_cb<cache_VI_sz_t<sz>, sz, esize, is_float, trace_before_write>;
            }
            break;
        case VPROFILE_TRACE_VALUE:
            if(strictly_ordered) {
                buff->full_cb = vprofile_update_V_cb<MAX_CLASS_SIZE>;
                buff->fill_num_cb = vprofile_fill_num_cb<cache_V_t, trace_before_write>;
            } else {
                buff->full_cb = vprofile_update_V_cb<sz>;
                buff->fill_num_cb = vprofile_fill_num_type_specialized_cb<cache_V_sz_t<sz>, sz, esize, is_float, trace_before_write>;
            }
            break;
        case VPROFILE_TRACE_CCT_ADDR_INFO:
            buff->full_cb = vprofile_update_CAI_cb;
            buff->fill_num_cb = strictly_ordered ? vprofile_fill_num_cb<cache_CAI_t, trace_before_write> :
                                    vprofile_fill_num_type_specialized_cb<cache_CAI_t, sz, esize, is_float, trace_before_write>;
            break;
        case VPROFILE_TRACE_CCT_ADDR:
            buff->full_cb = vprofile_update_CA_cb;
            buff->fill_num_cb = strictly_ordered ? vprofile_fill_num_cb<cache_CA_t, trace_before_write> :
                                    vprofile_fill_num_type_specialized_cb<cache_CA_t, sz, esize, is_float, trace_before_write>;
            break;
        case VPROFILE_TRACE_CCT_INFO:
            buff->full_cb = vprofile_update_CI_cb;
            buff->fill_num_cb = strictly_ordered ? vprofile_fill_num_cb<cache_CI_t, trace_before_write> :
                                    vprofile_fill_num_type_specialized_cb<cache_CI_t, sz, esize, is_float, trace_before_write>;
            break;
        case VPROFILE_TRACE_ADDR_INFO:
            buff->full_cb = vprofile_update_AI_cb;
            buff->fill_num_cb = strictly_ordered ? vprofile_fill_num_cb<cache_AI_t, trace_before_write> :
                                    vprofile_fill_num_type_specialized_cb<cache_AI_t, sz, esize, is_float, trace_before_write>;
            break;
        case VPROFILE_TRACE_ADDR:
            buff->full_cb = vprofile_update_A_cb;
            buff->fill_num_cb = strictly_ordered ? vprofile_fill_num_cb<cache_A_t, trace_before_write> :
                                    vprofile_fill_num_type_specialized_cb<cache_A_t, sz, esize, is_float, trace_before_write>;
            break;
        default:
            DR_ASSERT_MSG(false, "Unknown trace flag!");
    }
}

template<bool trace_before_write>
void vprofile_register_trace_cb_for_data_type(int i, vtrace_buffer_t* buff, 
                                              bool (*filter)(opnd_t, vprofile_src_t), 
                                              void (*update_cb)(val_info_t *),
                                              uint32_t trace_flag) {
    switch(i) {
        case INT8:
            vprofile_register_trace_cb_impl<1,1,false, trace_before_write>(buff, filter, update_cb, trace_flag);
            break;
        case INT16:
            vprofile_register_trace_cb_impl<2,2,false, trace_before_write>(buff, filter, update_cb, trace_flag);
            break;
        case INT32:
            vprofile_register_trace_cb_impl<4,4,false, trace_before_write>(buff, filter, update_cb, trace_flag);
            break;
        case SPx1:
            vprofile_register_trace_cb_impl<4,4,true, trace_before_write>(buff, filter, update_cb, trace_flag);
            break;
        case INT64:
            vprofile_register_trace_cb_impl<8,8,false, trace_before_write>(buff, filter, update_cb, trace_flag);
            break;
        case DPx1:
            vprofile_register_trace_cb_impl<8,8,true, trace_before_write>(buff, filter, update_cb, trace_flag);
            break;
        case INT128:
            vprofile_register_trace_cb_impl<16,16,false, trace_before_write>(buff, filter, update_cb, trace_flag);
            break;
        case INT8x16:
            vprofile_register_trace_cb_impl<16,1,false, trace_before_write>(buff, filter, update_cb, trace_flag);
            break;
        case SPx4:
            vprofile_register_trace_cb_impl<16,4,true, trace_before_write>(buff, filter, update_cb, trace_flag);
            break;
        case DPx2:
            vprofile_register_trace_cb_impl<16,8,true, trace_before_write>(buff, filter, update_cb, trace_flag);
            break;
        case INT256:
            vprofile_register_trace_cb_impl<32,32,false, trace_before_write>(buff, filter, update_cb, trace_flag);
            break;
        case INT8x32:
            vprofile_register_trace_cb_impl<32,1,false, trace_before_write>(buff, filter, update_cb, trace_flag);
            break;
        case SPx8:
            vprofile_register_trace_cb_impl<32,4,true, trace_before_write>(buff, filter, update_cb, trace_flag);
            break;
        case DPx4:
            vprofile_register_trace_cb_impl<32,8,true, trace_before_write>(buff, filter, update_cb, trace_flag);
            break;
        case INT512:
            vprofile_register_trace_cb_impl<64,64,false, trace_before_write>(buff, filter, update_cb, trace_flag);
            break;
        case INT8x64:
            vprofile_register_trace_cb_impl<64,1,false, trace_before_write>(buff, filter, update_cb, trace_flag);
            break;
        case SPx16:
            vprofile_register_trace_cb_impl<64,4,true, trace_before_write>(buff, filter, update_cb, trace_flag);
            break;
        case DPx8:
            vprofile_register_trace_cb_impl<64,8,true, trace_before_write>(buff, filter, update_cb, trace_flag);
            break;
        default: DR_ASSERT_MSG(false, "vprofile_register_trace_cb Unknown error!");
        
    }
}

template<int sz>
int get_buf_size_impl(uint32_t trace_flag) {
    uint32_t masked_trace_flag = (trace_flag & VPROFILE_TRACE_MASK);
    bool strictly_ordered = TEST_FLAG(trace_flag, VPROFILE_TRACE_STRICTLY_ORDERED);

    switch(masked_trace_flag) {
        case VPROFILE_TRACE_VAL_CCT_ADDR_INFO:
            if(strictly_ordered) {
                return (sizeof(cache_VCAI_t) * MAX_NUM_MEM_REFS);
            } else {
                return (sizeof(cache_VCAI_sz_t<sz>) * MAX_NUM_MEM_REFS);
            }
        case VPROFILE_TRACE_VAL_CCT_ADDR:
            if(strictly_ordered) {
                return (sizeof(cache_VCA_t) * MAX_NUM_MEM_REFS);
            } else {
                return (sizeof(cache_VCA_sz_t<sz>) * MAX_NUM_MEM_REFS);
            }
        case VPROFILE_TRACE_VAL_CCT_INFO:
            if(strictly_ordered) {
                return (sizeof(cache_VCI_t) * MAX_NUM_MEM_REFS);
            } else {
                return (sizeof(cache_VCI_sz_t<sz>) * MAX_NUM_MEM_REFS);
            }
        case VPROFILE_TRACE_VAL_CCT:
            if(strictly_ordered) {
                return (sizeof(cache_VC_t) * MAX_NUM_MEM_REFS);
            } else {
                return (sizeof(cache_VC_sz_t<sz>) * MAX_NUM_MEM_REFS);
            }
        case VPROFILE_TRACE_VAL_ADDR_INFO:
            if(strictly_ordered) {
                return (sizeof(cache_VAI_t) * MAX_NUM_MEM_REFS);
            } else {
                return (sizeof(cache_VAI_sz_t<sz>) * MAX_NUM_MEM_REFS);
            }
        case VPROFILE_TRACE_VAL_ADDR:
            if(strictly_ordered) {
                return (sizeof(cache_VA_t) * MAX_NUM_MEM_REFS);
            } else {
                return (sizeof(cache_VA_sz_t<sz>) * MAX_NUM_MEM_REFS);
            }
        case VPROFILE_TRACE_VAL_INFO:
            if(strictly_ordered) {
                return (sizeof(cache_VI_t) * MAX_NUM_MEM_REFS);
            } else {
                return (sizeof(cache_VI_sz_t<sz>) * MAX_NUM_MEM_REFS);
            }
        case VPROFILE_TRACE_VALUE:
            if(strictly_ordered) {
                return (sizeof(cache_V_t) * MAX_NUM_MEM_REFS);
            } else {
                return (sizeof(cache_V_sz_t<sz>) * MAX_NUM_MEM_REFS);
            }
        case VPROFILE_TRACE_CCT_ADDR_INFO:
            return (sizeof(cache_CAI_t) * MAX_NUM_MEM_REFS);
        case VPROFILE_TRACE_CCT_ADDR:
            return (sizeof(cache_CA_t) * MAX_NUM_MEM_REFS);
        case VPROFILE_TRACE_CCT_INFO:
            return (sizeof(cache_CI_t) * MAX_NUM_MEM_REFS);
        case VPROFILE_TRACE_ADDR_INFO:
            return (sizeof(cache_AI_t) * MAX_NUM_MEM_REFS);
        case VPROFILE_TRACE_ADDR:
            return (sizeof(cache_A_t) * MAX_NUM_MEM_REFS);
        default:
            DR_ASSERT_MSG(false, "Unknown trace flag!");
    }
    return 0;
}

int get_buf_size(int i, uint32_t trace_flag)
{
    switch(i) {
        case INT8:
            return get_buf_size_impl<1>(trace_flag);
        case INT16:
            return get_buf_size_impl<2>(trace_flag);
        case INT32:
        case SPx1:
            return get_buf_size_impl<4>(trace_flag);
        case INT64:
        case DPx1:
            return get_buf_size_impl<8>(trace_flag);
        case INT128:
        case INT8x16:
        case SPx4:
        case DPx2:
            return get_buf_size_impl<16>(trace_flag);
        case INT256:
        case INT8x32:
        case SPx8:
        case DPx4:
            return get_buf_size_impl<32>(trace_flag);
        case INT512:
        case INT8x64:
        case SPx16:
        case DPx8:
            return get_buf_size_impl<64>(trace_flag);
        default: DR_ASSERT_MSG(false, "vprofile_register_trace_cb Unknown error!");
        
    }
    return -1;
}

void vprofile_register_trace_cb(vtrace_t *vtrace, bool (*filter)(opnd_t, vprofile_src_t), 
                                uint32_t opnd_mask, 
                                vprofile_data_t data_type,
                                void (*update_cb)(val_info_t *))
{
    // set opnd_mask
    vtrace->opnd_mask = opnd_mask;
    bool trace_before_write = TEST_FLAG(vtrace->trace_flag, VPROFILE_TRACE_BEFORE_WRITE);
    bool strictly_ordered = TEST_FLAG(vtrace->trace_flag, VPROFILE_TRACE_STRICTLY_ORDERED);

    if(strictly_ordered) {
        DR_ASSERT_MSG(data_type==ANY, "vprofile_register_trace_cb usage error!");
        vtrace->buff = vtracer_create_trace_buffer(get_buf_size_impl<MAX_CLASS_SIZE>(vtrace->trace_flag));
        DR_ASSERT_MSG(vtrace->buff, "vprofile_register_trace_cb usage error: buffer not allocated!");
        if(trace_before_write) {
            vprofile_register_trace_cb_impl<MAX_CLASS_SIZE,1,false,true>(vtrace->buff, filter, update_cb, vtrace->trace_flag);
        } else {
            vprofile_register_trace_cb_impl<MAX_CLASS_SIZE,1,false,false>(vtrace->buff, filter, update_cb, vtrace->trace_flag);
        }
    } else {
        if(data_type==ANY) {
            for(int i=0; i<NUM_DATA_TYPES; ++i) {
                if(!vtrace->buff_ex[i]) {
                    vtrace->buff_ex[i] = vtracer_create_trace_buffer(get_buf_size(i, vtrace->trace_flag));
                }
                if(trace_before_write) {
                    vprofile_register_trace_cb_for_data_type<true>(i, vtrace->buff_ex[i], filter, update_cb, vtrace->trace_flag);
                } else {
                    vprofile_register_trace_cb_for_data_type<false>(i, vtrace->buff_ex[i], filter, update_cb, vtrace->trace_flag);
                }
            }
        } else {
            vtrace->buff_ex[data_type] = vtracer_create_trace_buffer(get_buf_size((int) data_type, vtrace->trace_flag));
            DR_ASSERT_MSG(vtrace->buff_ex[data_type], "vprofile_register_trace_cb usage error: buffer not allocated for data_type!");
            if(trace_before_write) {
                vprofile_register_trace_cb_for_data_type<true>((int) data_type, vtrace->buff_ex[data_type], filter, update_cb, vtrace->trace_flag);
            } else {
                vprofile_register_trace_cb_for_data_type<false>((int) data_type, vtrace->buff_ex[data_type], filter, update_cb, vtrace->trace_flag);
            }
        }
    }
}

vtrace_t *vprofile_register_trace(bool (*filter)(opnd_t, vprofile_src_t),
                                  uint32_t opnd_mask, 
                                  void (*update_cb)(val_info_t *),
                                  bool do_data_centric)
{
    uint32_t trace_flag = VPROFILE_TRACE_STRICTLY_ORDERED;
    if(do_data_centric) {
        trace_flag |= VPROFILE_TRACE_ADDR;
    } else {
        trace_flag |= VPROFILE_TRACE_CCT;
    }
    return vprofile_register_trace_ex(filter, opnd_mask, update_cb, trace_flag);
}

vtrace_t* vprofile_register_trace_ex(bool (*filter)(opnd_t, vprofile_src_t), 
                                     uint32_t opnd_mask,
                                     void (*update_cb)(val_info_t *),
                                     uint32_t trace_flag)
{
    // alloc trace
    vtrace_t *vtrace = vprofile_allocate_trace(trace_flag);

    // register cb
    vprofile_register_trace_cb(vtrace, filter, opnd_mask, ANY, update_cb);
    return vtrace;

}

void vprofile_unregister_trace(vtrace_t *vtrace)
{
    if(!vtrace) return;
    if(TEST_FLAG(vtrace->trace_flag, VPROFILE_TRACE_STRICTLY_ORDERED)) {
        if(vtrace->buff!=NULL)
            vtracer_buffer_free(vtrace->buff);
    } else {
        for(int i = 0; i < NUM_DATA_TYPES; i++) {
            if(vtrace->buff_ex[i]!=NULL)
                vtracer_buffer_free(vtrace->buff_ex[i]);
        }
    }
    dr_global_free(vtrace, sizeof(*vtrace));
}