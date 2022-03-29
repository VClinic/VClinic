#include <unordered_map>
#include <vector>
#include <string>
#include <sys/stat.h>
#include <assert.h>
#include <algorithm>
#include <stddef.h>
#include <math.h>

#include "dr_api.h"
#include "drmgr.h"
#include "drreg.h"
#include "drutil.h"
#include "drwrap.h"
#include "drvector.h"
#include "vprofile.h"

dr_emit_flags_t
event_basic_block(void *drcontext, void *tag, instrlist_t *bb,
                      bool for_trace, bool translating, OUT void **user_data);
void
exit_event(void);

/* Cache data structure to store in buffered trace */
template<int size>
struct cache_t {
    int8_t val[size];
};

/* Max number of mem_ref a buffer can have. */
#define MAX_NUM_MEM_REFS 4096
/* The maximum size of buffer for holding mem_refs. */
#define MEM_BUF_SIZE(size) (sizeof(cache_t<size>) * MAX_NUM_MEM_REFS)

// trace buffer for integer
#define TRACE_BUFFER_INT_NUM (6)
// total number of trace buffers
#define TRACE_BUFFER_NUM (TRACE_BUFFER_INT_NUM)
static vtrace_buffer_t* trace_buffer[TRACE_BUFFER_NUM];

bool
instr_is_div(instr_t *instr, OUT opnd_t *opnd);

bool
instr_is_mul(instr_t *instr);

/* Call backs to handle full trace buffers */
template<class T, int sz, int num>
void trace_buf_full_cb(void *buf_base, void *buf_end, void* user_data) {
    cache_t<sz>* cache_ptr = (cache_t<sz>*)buf_base;
    cache_t<sz>* cache_end = (cache_t<sz>*)buf_end;
    dr_fprintf(STDOUT, "buf size = %ld\n", cache_ptr - cache_end);
    for(; cache_ptr<cache_end; ++cache_ptr) {
        // extract data from cache
        void* val = cache_ptr->val;
        dr_fprintf(STDOUT, "val = %d\n", *((int*)(val)));
    }
}

template<int sz, bool is_float>
size_t trace_buf_fill_num_cb(void *drcontext, instr_t *where, void* user_data) {
    size_t fill_num = 0;
    if(!instr_is_app(where) || instr_is_ignorable(where)) return fill_num;
    opnd_t opnd;
    if (instr_is_mul(where) && instr_is_div(instr_get_next(where), &opnd) && opnd_size_in_bytes(opnd_get_size(opnd)) == sz) {
        fill_num += sizeof(cache_t<sz>);
    }
    return fill_num;
}

/* Trace Buffer Initialization */
void TraceBufferInit() {
    // integer buffers
    trace_buffer[0] = vtracer_create_trace_buffer_ex(MEM_BUF_SIZE(1), trace_buf_full_cb<uint8_t, 1, 1>, NULL, trace_buf_fill_num_cb<1, false>, NULL);
    trace_buffer[1] = vtracer_create_trace_buffer_ex(MEM_BUF_SIZE(2), trace_buf_full_cb<uint16_t, 2, 1>, NULL, trace_buf_fill_num_cb<2, false>, NULL);
    trace_buffer[2] = vtracer_create_trace_buffer_ex(MEM_BUF_SIZE(4), trace_buf_full_cb<uint32_t, 4, 1>, NULL, trace_buf_fill_num_cb<4, false>, NULL);
    trace_buffer[3] = vtracer_create_trace_buffer_ex(MEM_BUF_SIZE(8), trace_buf_full_cb<uint64_t, 8, 1>, NULL, trace_buf_fill_num_cb<8, false>, NULL);
    trace_buffer[4] = vtracer_create_trace_buffer_ex(MEM_BUF_SIZE(16), trace_buf_full_cb<uint64_t, 16, 2>, NULL, trace_buf_fill_num_cb<16, false>, NULL);
    trace_buffer[5] = vtracer_create_trace_buffer_ex(MEM_BUF_SIZE(32), trace_buf_full_cb<uint64_t, 32, 4>, NULL, trace_buf_fill_num_cb<32, false>, NULL);
}

/* Trace Buffer Finalize */
void TraceBufferFini() {
    for(int i=0; i<TRACE_BUFFER_NUM; ++i) {
        if(trace_buffer[i]!=NULL) {
            vtracer_buffer_free(trace_buffer[i]);
        }
    }
}

/* help function to get buffer trace */
vtrace_buffer_t* get_trace_buf(int size) {
    switch(size) {
        case 1: return trace_buffer[0];
        case 2: return trace_buffer[1];
        case 4: return trace_buffer[2];
        case 8: return trace_buffer[3];
        case 16: return trace_buffer[4];
        case 32: return trace_buffer[5];
    }
    // Unknown trace buffer, alert for bug
    assert(false && "Should not reach this");
    return NULL;
}

void
exit_event(void)
{
    TraceBufferFini();

    vtracer_exit();
    drmgr_exit();
}

/* If instr is unsigned division, return true and set *opnd to divisor. */
bool
instr_is_div(instr_t *instr, OUT opnd_t *opnd)
{
    int opc = instr_get_opcode(instr);
#if defined(X86)
    if (opc == OP_div) {
        *opnd = instr_get_src(instr, 0); /* divisor is 1st src */
        return true;
    }
#elif defined(AARCHXX)
    if (opc == OP_udiv) {
        *opnd = instr_get_src(instr, 1); /* divisor is 2nd src */
        return true;
    }
#else
#    error NYI
#endif
    return false;
}

bool
instr_is_mul(instr_t *instr)
{
    int opc = instr_get_opcode(instr);
    if (opc == OP_mul) {
        return true;
    }
    return false;
}

dr_emit_flags_t
event_basic_block(void *drcontext, void *tag, instrlist_t *bb,
                      bool for_trace, bool translating, OUT void **user_data)
{
    for (instr_t* instr = instrlist_first(bb); instr != NULL; instr = instr_get_next(instr)) {
        if(!instr_is_app(instr) || instr_is_ignorable(instr)) continue;
        opnd_t opnd;
        if (instr_is_mul(instr) && instr_is_div(instr_get_next(instr), &opnd)) {
            const int size = opnd_size_in_bytes(opnd_get_size(opnd));
            dr_fprintf(STDOUT, "opnd size = %d\n", size);
            vtrace_buffer_t* buf = get_trace_buf(size);
            reg_id_t reg_ptr, scratch;

            drvector_t allowed;
            drreg_init_and_fill_vector(&allowed, true);
            for (int i = opnd_num_regs_used(opnd) - 1; i >= 0; i--) {
                reg_id_t reg_used = opnd_get_reg_used(opnd, i);
                // resize for simd or gpr, mmx regs are not supported!
                // resize for simd may occur error
                if(!reg_is_gpr(reg_used)) {
                    continue;
                }
                if(reg_is_simd(reg_used) || reg_is_mmx(reg_used)) {
                    drreg_set_vector_entry(&allowed, reg_used, false);
                    continue;
                }
                drreg_set_vector_entry(&allowed, reg_resize_to_opsz(reg_used, OPSZ_1), false);
                drreg_set_vector_entry(&allowed, reg_resize_to_opsz(reg_used, OPSZ_2), false);
                drreg_set_vector_entry(&allowed, reg_resize_to_opsz(reg_used, OPSZ_4), false);
                drreg_set_vector_entry(&allowed, reg_resize_to_opsz(reg_used, OPSZ_8), false);

            }

            RESERVE_REG(drcontext, bb, instr, &allowed, reg_ptr);
            RESERVE_REG(drcontext, bb, instr, &allowed, scratch);
            drvector_delete(&allowed);

            vtracer_get_trace_buffer_in_reg(drcontext, instr,
                                     bb,
                                     buf,
                                     reg_ptr);
            switch(size) {
                case 1:
                    vtracer_insert_trace_val(drcontext, instr,
                                bb, opnd, reg_ptr,
                                scratch, offsetof(cache_t<1>, val));
                    vtracer_insert_trace_forward(drcontext, instr,
                                  bb, sizeof(cache_t<1>),
                                  buf,
                                  reg_ptr, scratch);
                    break;
                case 2:
                    vtracer_insert_trace_val(drcontext, instr,
                                bb, opnd, reg_ptr,
                                scratch, offsetof(cache_t<2>, val));
                    vtracer_insert_trace_forward(drcontext, instr,
                                  bb, sizeof(cache_t<2>),
                                  buf,
                                  reg_ptr, scratch);
                    break;
                case 4:
                    vtracer_insert_trace_val(drcontext, instr,
                                bb, opnd, reg_ptr,
                                scratch, offsetof(cache_t<4>, val));
                    vtracer_insert_trace_forward(drcontext, instr,
                                  bb, sizeof(cache_t<4>),
                                  buf,
                                  reg_ptr, scratch);
                    break;
		        case 8: {
                    vtracer_insert_trace_val(drcontext, instr,
                                bb, opnd, reg_ptr,
                                scratch, offsetof(cache_t<8>, val));
                    vtracer_insert_trace_forward(drcontext, instr,
                                  bb, sizeof(cache_t<8>),
                                  buf,
                                  reg_ptr, scratch);
                    break;
                }
                case 16:
                    vtracer_insert_trace_val(drcontext, instr,
                                bb, opnd, reg_ptr,
                                scratch, offsetof(cache_t<16>, val));
                    vtracer_insert_trace_forward(drcontext, instr,
                                  bb, sizeof(cache_t<16>),
                                  buf,
                                  reg_ptr, scratch);
                    break;
                case 32:
                    vtracer_insert_trace_val(drcontext, instr,
                                bb, opnd, reg_ptr,
                                scratch, offsetof(cache_t<32>, val));
                    vtracer_insert_trace_forward(drcontext, instr,
                                  bb, sizeof(cache_t<32>),
                                  buf,
                                  reg_ptr, scratch);
                    break;
            }
            UNRESERVE_REG(drcontext, bb, instr, scratch);
            UNRESERVE_REG(drcontext, bb, instr, reg_ptr);
        }
    }
    
    return DR_EMIT_DEFAULT;
}

#ifdef __cplusplus
extern "C" {
#endif

DR_EXPORT void
dr_client_main(client_id_t id, int argc, const char *argv[])
{
    dr_set_client_name("DynamoRIO Sample Client 'mul_followed_by_div'", "http://dynamorio.org/issues");

    if(!vtracer_init()) {
        assert(false);
    }
    dr_register_exit_event(exit_event);
    if (!drmgr_register_bb_instrumentation_event(event_basic_block, NULL, NULL))
        assert(false);
    TraceBufferInit();
}

#ifdef __cplusplus
}
#endif
