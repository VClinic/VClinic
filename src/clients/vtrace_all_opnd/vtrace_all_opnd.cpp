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
#include "utils.h"

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

#ifdef AARCH64
#define MEM_BUF_SIZE(size) 4095
#else
/* Max number of mem_ref a buffer can have. */
#define MAX_NUM_MEM_REFS 4096
/* The maximum size of buffer for holding mem_refs. */
#define MEM_BUF_SIZE(size) (sizeof(cache_t<size>) * MAX_NUM_MEM_REFS)
#endif

// total number of trace buffers
#define TRACE_BUFFER_NUM 18
static vtrace_buffer_t* trace_buffer[TRACE_BUFFER_NUM];

/* Call backs to handle full trace buffers */
void trace_buf_full_cb(void *buf_base, void *buf_end, void* user_data) {
    return;
}

template<int sz, int esize, bool is_float>
size_t trace_buf_fill_num_cb(void *drcontext, instr_t *where, void* user_data) {
    size_t fill_num = 0;
    int num = instr_num_srcs(where);
    // instr is floating == opnd is floating?
    bool is_float_actual = instr_is_floating(where);
    if(is_float != is_float_actual) return fill_num;
    for(int j = 0; j < num; j++) {
        opnd_t opnd = instr_get_src(where, j);
        int esize_actual = (is_float) ? FloatOperandSizeTable(where, opnd) : IntegerOperandSizeTable(where, opnd);
        if(esize == esize_actual && opnd_size_in_bytes(opnd_get_size(opnd)) == sz) {
            fill_num += sizeof(cache_t<sz>);
        }
    }

    return fill_num;
}

/* Trace Buffer Initialization */
void TraceBufferInit() {
    // integer buffers
    trace_buffer[0] = vtracer_create_trace_buffer_ex(MEM_BUF_SIZE(1), trace_buf_full_cb, NULL, trace_buf_fill_num_cb<1, 1, false>, NULL);

    trace_buffer[1] = vtracer_create_trace_buffer_ex(MEM_BUF_SIZE(2), trace_buf_full_cb, NULL, trace_buf_fill_num_cb<2, 2, false>, NULL);

    trace_buffer[2] = vtracer_create_trace_buffer_ex(MEM_BUF_SIZE(4), trace_buf_full_cb, NULL, trace_buf_fill_num_cb<4, 4, false>, NULL);

    trace_buffer[3] = vtracer_create_trace_buffer_ex(MEM_BUF_SIZE(8), trace_buf_full_cb, NULL, trace_buf_fill_num_cb<8, 8, false>, NULL);

    trace_buffer[4] = vtracer_create_trace_buffer_ex(MEM_BUF_SIZE(16), trace_buf_full_cb, NULL, trace_buf_fill_num_cb<16, 16, false>, NULL);
    trace_buffer[5] = vtracer_create_trace_buffer_ex(MEM_BUF_SIZE(16), trace_buf_full_cb, NULL, trace_buf_fill_num_cb<16, 1, false>, NULL);

    trace_buffer[6] = vtracer_create_trace_buffer_ex(MEM_BUF_SIZE(32), trace_buf_full_cb, NULL, trace_buf_fill_num_cb<32, 32, false>, NULL);
    trace_buffer[7] = vtracer_create_trace_buffer_ex(MEM_BUF_SIZE(32), trace_buf_full_cb, NULL, trace_buf_fill_num_cb<32, 1, false>, NULL);

    trace_buffer[8] = vtracer_create_trace_buffer_ex(MEM_BUF_SIZE(32), trace_buf_full_cb, NULL, trace_buf_fill_num_cb<64, 64, false>, NULL);
    trace_buffer[9] = vtracer_create_trace_buffer_ex(MEM_BUF_SIZE(32), trace_buf_full_cb, NULL, trace_buf_fill_num_cb<64, 1, false>, NULL);

    // fp buffers
    trace_buffer[10] = vtracer_create_trace_buffer_ex(MEM_BUF_SIZE(4), trace_buf_full_cb, NULL, trace_buf_fill_num_cb<4, 4, true>, NULL);

    trace_buffer[11] = vtracer_create_trace_buffer_ex(MEM_BUF_SIZE(8), trace_buf_full_cb, NULL, trace_buf_fill_num_cb<8, 8, true>, NULL);

    trace_buffer[12] = vtracer_create_trace_buffer_ex(MEM_BUF_SIZE(16), trace_buf_full_cb, NULL, trace_buf_fill_num_cb<16, 4, true>, NULL);
    trace_buffer[13] = vtracer_create_trace_buffer_ex(MEM_BUF_SIZE(16), trace_buf_full_cb, NULL, trace_buf_fill_num_cb<16, 8, true>, NULL);

    trace_buffer[14] = vtracer_create_trace_buffer_ex(MEM_BUF_SIZE(32), trace_buf_full_cb, NULL, trace_buf_fill_num_cb<32, 4, true>, NULL);
    trace_buffer[15] = vtracer_create_trace_buffer_ex(MEM_BUF_SIZE(32), trace_buf_full_cb, NULL, trace_buf_fill_num_cb<32, 8, true>, NULL);

    trace_buffer[16] = vtracer_create_trace_buffer_ex(MEM_BUF_SIZE(32), trace_buf_full_cb, NULL, trace_buf_fill_num_cb<64, 4, true>, NULL);
    trace_buffer[17] = vtracer_create_trace_buffer_ex(MEM_BUF_SIZE(32), trace_buf_full_cb, NULL, trace_buf_fill_num_cb<64, 8, true>, NULL);
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
vtrace_buffer_t* get_trace_buf(bool is_float, int esize, int size) {
    if (is_float) {
        if(esize==4) {
            switch(size) {
                case 4:
                    return trace_buffer[10];
                case 16:
                    return trace_buffer[12];
                case 32:
                    return trace_buffer[14];
                case 64:
                    return trace_buffer[16];
            }
        } else if(esize==8) {
            switch(size) {
                case 8:
                    return trace_buffer[11];
                case 16:
                    return trace_buffer[13];
                case 32:
                    return trace_buffer[15];
                case 64:
                    return trace_buffer[17];
            }
        }
    } else {
        // assert(size == esize);
        if(esize == size) {
            switch(size) {
                case 1: return trace_buffer[0];
                case 2: return trace_buffer[1];
                case 4: return trace_buffer[2];
                case 8: return trace_buffer[3];
                case 16: return trace_buffer[4];
                case 32: return trace_buffer[6];
                case 64: return trace_buffer[8];
            }
        } else if(esize == 1) {
            switch(size) {
                case 16: return trace_buffer[5];
                case 32: return trace_buffer[7];
                case 64: return trace_buffer[9];
            }
        }
    }
    // Unknown trace buffer, alert for bug
    // assert(false && "Should not reach this");
    return NULL;
}

void
exit_event(void)
{
    TraceBufferFini();

    vtracer_exit();
    drmgr_exit();
}

dr_emit_flags_t
event_basic_block(void *drcontext, void *tag, instrlist_t *bb,
                      bool for_trace, bool translating, OUT void **user_data)
{
    for (instr_t* instr = instrlist_first(bb); instr != NULL; instr = instr_get_next(instr)) {
        if(!instr_is_app(instr) || instr_is_ignorable(instr)) continue;
        int num = instr_num_srcs(instr);
        for(int j = 0; j < num; j++) {
            opnd_t opnd = instr_get_src(instr, j);
            int size = opnd_size_in_bytes(opnd_get_size(opnd));
            if(size == 0) {
                continue;
            }
            bool is_float = instr_is_floating(instr);
            int esize = (is_float) ? FloatOperandSizeTable(instr, opnd) : IntegerOperandSizeTable(instr, opnd);
            vtrace_buffer_t* buf = get_trace_buf(is_float, esize, size);
            if(!buf) continue;

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
                case 64:
                    vtracer_insert_trace_val(drcontext, instr,
                                bb, opnd, reg_ptr,
                                scratch, offsetof(cache_t<64>, val));
                    vtracer_insert_trace_forward(drcontext, instr,
                                  bb, sizeof(cache_t<64>),
                                  buf,
                                  reg_ptr, scratch);
                    break;
                case 128:
                    vtracer_insert_trace_val(drcontext, instr,
                                bb, opnd, reg_ptr,
                                scratch, offsetof(cache_t<128>, val));
                    vtracer_insert_trace_forward(drcontext, instr,
                                  bb, sizeof(cache_t<128>),
                                  buf,
                                  reg_ptr, scratch);
                    break;
                case 256:
                    vtracer_insert_trace_val(drcontext, instr,
                                bb, opnd, reg_ptr,
                                scratch, offsetof(cache_t<256>, val));
                    vtracer_insert_trace_forward(drcontext, instr,
                                  bb, sizeof(cache_t<256>),
                                  buf,
                                  reg_ptr, scratch);
                    break;
                case 512:
                    vtracer_insert_trace_val(drcontext, instr,
                                bb, opnd, reg_ptr,
                                scratch, offsetof(cache_t<512>, val));
                    vtracer_insert_trace_forward(drcontext, instr,
                                  bb, sizeof(cache_t<512>),
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
    dr_set_client_name("DynamoRIO Sample Client 'vtrace_all_pond'", "http://dynamorio.org/issues");

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
