#ifndef __TRIVIAL_TRACER_H__
#define __TRIVIAL_TRACER_H__

#include "trace.h"
#include "trivial_detector.h"
#include "trivial_logger.h"
#include "trivial_table.h"
#include "trivial_define.h"
#include "dr_api.h"
#include "drreg.h"
#include "drcctlib.h"

/* Cache data structure to store in buffered trace */
template<int size>
struct cache_t {
    int32_t DFGLog_ctxt;
    int8_t val[size];
};

/* Max number of mem_ref a buffer can have. */
#define MAX_NUM_MEM_REFS 4096
/* The maximum size of buffer for holding mem_refs. */
#define MEM_BUF_SIZE(size) (sizeof(cache_t<size>) * MAX_NUM_MEM_REFS)

// trace buffer for integer
#define TRACE_BUFFER_INT_NUM (6)
// trace buffer for single
#define TRACE_BUFFER_SP_NUM (3)
// trace buffer for double
#define TRACE_BUFFER_DP_NUM (3)
// total number of trace buffers
#define TRACE_BUFFER_NUM (TRACE_BUFFER_INT_NUM + TRACE_BUFFER_SP_NUM + TRACE_BUFFER_DP_NUM)
static trace_buf_t* trace_buffer[TRACE_BUFFER_NUM];

/* Call backs to handle full trace buffers */
template<class T, int sz, int num>
void trace_buf_full_cb(void *buf_base, void *buf_end) {
    cache_t<sz>* cache_ptr = (cache_t<sz>*)buf_base;
    cache_t<sz>* cache_end = (cache_t<sz>*)buf_end;
    per_thread_log_t *pt = (per_thread_log_t *)drmgr_get_tls_field(dr_get_current_drcontext(), global_log_space.tls_idx);
    for(; cache_ptr<cache_end; ++cache_ptr) {
        // extract data from cache
        int DFGLog_ctxt = cache_ptr->DFGLog_ctxt;
        void* val = cache_ptr->val;
        DFGLog* log = getDFGLog(DFGLog_ctxt);
        pt->log[DFGLog_ctxt].total += num;
        // use registered counting function
        // pt->log[DFGLog_ctxt].trivial += log->trivialFuncs[0](val);
        switch(log->val) {
            case IS_ZERO:
                pt->log[DFGLog_ctxt].trivial += TrivialDetector<T, num, IS_ZERO>::run(val);
                break;
            case IS_ONE:
                pt->log[DFGLog_ctxt].trivial += TrivialDetector<T, num, IS_ONE>::run(val);
                break;
            case IS_FULL:
                pt->log[DFGLog_ctxt].trivial += TrivialDetector<T, num, IS_FULL>::run(val);
                break;
            default:
                assert(0);
        }
    }
}

#define HANDLE_CONDVAL(condval) do { \
                pt->log[DFGLog_ctxt].trivial += TrivialDetector<T, num, IS_ZERO>::run(val); \
                if(soft) { \
                    pt->log[DFGLog_ctxt].soft_approx += ApproxTrivialDetectorSoft<T, num, IS_ZERO>::run(val); \
                } \
                if(hard) { \
                    if(sizeof(T)==4) { \
                        pt->log[DFGLog_ctxt].hard_approx += ApproxTrivialDetectorHard<num, IS_ZERO>::run_single(val); \
                    } else { \
                        pt->log[DFGLog_ctxt].hard_approx += ApproxTrivialDetectorHard<num, IS_ZERO>::run_double(val); \
                    } \
                } } while(0)

/* Call backs to handle full trace buffers */
template<class T, int sz, int num, bool soft, bool hard>
void trace_buf_full_fp_cb(void *buf_base, void *buf_end) {
    cache_t<sz>* cache_ptr = (cache_t<sz>*)buf_base;
    cache_t<sz>* cache_end = (cache_t<sz>*)buf_end;
    per_thread_log_t *pt = (per_thread_log_t *)drmgr_get_tls_field(dr_get_current_drcontext(), global_log_space.tls_idx);
    for(; cache_ptr<cache_end; ++cache_ptr) {
        // extract data from cache
        int DFGLog_ctxt = cache_ptr->DFGLog_ctxt;
        void* val = cache_ptr->val;
        DFGLog* log = getDFGLog(DFGLog_ctxt);
        pt->log[DFGLog_ctxt].total += num;
        // pt->log[DFGLog_ctxt].trivial += log->trivialFuncs[0](val);
        // if(soft) {
        //     pt->log[DFGLog_ctxt].soft_approx += log->trivialFuncs[1](val);
        // }
        // if(hard) {
        //     pt->log[DFGLog_ctxt].hard_approx += log->trivialFuncs[2](val);
        // }
        switch(log->val) {
            case IS_ZERO:
                HANDLE_CONDVAL(IS_ZERO);
                break;
            case IS_ONE:
                HANDLE_CONDVAL(IS_ONE);
                break;
            case IS_FULL:
                HANDLE_CONDVAL(IS_FULL);
                break;
            default:
                assert(0);
        }
    }
}

template<int sz, bool is_float>
size_t trace_buf_fill_num_cb(void *drcontext, instr_t *where) {
    size_t fill_num = 0;
    per_thread_log_t *pt = (per_thread_log_t *)drmgr_get_tls_field(dr_get_current_drcontext(), global_log_space.tls_idx);
    // TODO: the iterations are redundant. We should define tracing functions with better abstractions
    for(auto it=pt->trivial_entry_list->begin(); it!=pt->trivial_entry_list->end(); ++it) {
        // only add to this trace when it is the entry of a dataflow triviality
        if((*it)->trivial_entry.entry == where) {
            if((*it)->trivial_entry.is_float==is_float) {
                // uint32_t size = opnd_size_in_bytes(opnd_get_size((*it)->trivial_entry.opnd));
                // only add when the size matches
                if((*it)->trivial_entry.size==sz) {
                    fill_num += sizeof(cache_t<sz>);
                }
            }
        }
    }
    return fill_num;
}

template<bool en_soft, bool en_hard>
void TraceBufferInitApprox() {
    // single floating point buffers
    trace_buffer[6] = trace_buf_create_trace_buffer_ex(MEM_BUF_SIZE(4), trace_buf_full_fp_cb<float, 4, 1, en_soft, en_hard>, trace_buf_fill_num_cb<4, true>);
    trace_buffer[7] = trace_buf_create_trace_buffer_ex(MEM_BUF_SIZE(16), trace_buf_full_fp_cb<float, 16, 4, en_soft, en_hard>, trace_buf_fill_num_cb<16, true>);
    trace_buffer[8] = trace_buf_create_trace_buffer_ex(MEM_BUF_SIZE(32), trace_buf_full_fp_cb<float, 32, 8, en_soft, en_hard>, trace_buf_fill_num_cb<32, true>);
    // double floating point buffers
    trace_buffer[9] = trace_buf_create_trace_buffer_ex(MEM_BUF_SIZE(8), trace_buf_full_fp_cb<double, 8, 1, en_soft, en_hard>, trace_buf_fill_num_cb<8, true>);
    trace_buffer[10] = trace_buf_create_trace_buffer_ex(MEM_BUF_SIZE(16), trace_buf_full_fp_cb<double, 16, 2, en_soft, en_hard>, trace_buf_fill_num_cb<16, true>);
    trace_buffer[11] = trace_buf_create_trace_buffer_ex(MEM_BUF_SIZE(32), trace_buf_full_fp_cb<double, 32, 4, en_soft, en_hard>, trace_buf_fill_num_cb<32, true>);
}

/* Trace Buffer Initialization */
void TraceBufferInit(bool enable_approx_soft, bool enable_approx_hard) {
    // integer buffers
    trace_buffer[0] = trace_buf_create_trace_buffer_ex(MEM_BUF_SIZE(1), trace_buf_full_cb<uint8_t, 1, 1>, trace_buf_fill_num_cb<1, false>);
    trace_buffer[1] = trace_buf_create_trace_buffer_ex(MEM_BUF_SIZE(2), trace_buf_full_cb<uint16_t, 2, 1>, trace_buf_fill_num_cb<2, false>);
    trace_buffer[2] = trace_buf_create_trace_buffer_ex(MEM_BUF_SIZE(4), trace_buf_full_cb<uint32_t, 4, 1>, trace_buf_fill_num_cb<4, false>);
    trace_buffer[3] = trace_buf_create_trace_buffer_ex(MEM_BUF_SIZE(8), trace_buf_full_cb<uint64_t, 8, 1>, trace_buf_fill_num_cb<8, false>);
    trace_buffer[4] = trace_buf_create_trace_buffer_ex(MEM_BUF_SIZE(16), trace_buf_full_cb<uint64_t, 16, 2>, trace_buf_fill_num_cb<16, false>);
    trace_buffer[5] = trace_buf_create_trace_buffer_ex(MEM_BUF_SIZE(32), trace_buf_full_cb<uint64_t, 32, 4>, trace_buf_fill_num_cb<32, false>);
    if(enable_approx_soft && !enable_approx_hard) {
        TraceBufferInitApprox<true, false>();
    } else if(enable_approx_soft && enable_approx_hard) {
        TraceBufferInitApprox<true, true>();
    } else if(!enable_approx_soft && enable_approx_hard) {
        TraceBufferInitApprox<false, true>();
    } else {
        TraceBufferInitApprox<false, false>();
    }
}

/* Trace Buffer Finalize */
void TraceBufferFini() {
    for(int i=0; i<TRACE_BUFFER_NUM; ++i) {
        if(trace_buffer[i]!=NULL) {
            trace_buf_free(trace_buffer[i]);
        }
    }
}

/********************************************************************/
template <int size>
void insert_trace_info(void *drcontext, instrlist_t *ilist, instr_t *where,
                       trace_buf_t *buf, int DFGLog_ctxt,
                       reg_id_t reg_ptr, reg_id_t scratch) {
  // load current buffer pointer into reg_ptr
  trace_buf_insert_load_buf_ptr(drcontext, buf, ilist, where, reg_ptr);
  // store operand handle id
  assert(DFGLog_ctxt>=0);
  trace_buf_insert_buf_store(drcontext, buf, ilist, where, reg_ptr, DR_REG_NULL,
                             OPND_CREATE_INT32(DFGLog_ctxt), OPSZ_4,
                             offsetof(cache_t<size>, DFGLog_ctxt));
}

template <int size>
void insert_trace_value_in_reg(void *drcontext, instrlist_t *ilist,
                               instr_t *where, trace_buf_t *buf,
                               opnd_size_t opsz, reg_id_t reg_val,
                               reg_id_t reg_ptr) {
  // load current buffer pointer into reg_ptr
  trace_buf_insert_load_buf_ptr(drcontext, buf, ilist, where, reg_ptr);
  // store the register value into the buffer
  trace_buf_insert_buf_store(drcontext, buf, ilist, where, reg_ptr, DR_REG_NULL,
                             opnd_create_reg(reg_resize_to_opsz(reg_val, opsz)), opsz,
                             offsetof(cache_t<size>, val));
}

template <int size>
void insert_trace_value_in_simd(void *drcontext, instrlist_t *ilist,
                                instr_t *where, trace_buf_t *buf,
                                reg_id_t reg_simd, reg_id_t reg_ptr) {
  size_t offset = offsetof(cache_t<size>, val);
  // store the value of the simd register into the buffered trace
#ifdef X86
    switch(size) {
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
            TRIVIALSPY_EXIT_PROCESS("insert_trace_value_in_simd: Unknown simd size!");
    }
#elif defined(ARM)
    TRIVIALSPY_EXIT_PROCESS("insert_trace_value_in_simd not implemented!");
#elif defined(AARCH64)
    TRIVIALSPY_EXIT_PROCESS("insert_trace_value_in_simd not implemented!");
#endif
}

template <int size>
inline __attribute__((always_inline)) void
insert_trace_value_in_mem(void *drcontext, instrlist_t *ilist, instr_t *where,
                          reg_id_t reg_addr, reg_id_t reg_ptr,
                          trace_buf_t *buf) {
  trace_buf_insert_load_buf_ptr(drcontext, buf, ilist, where, reg_ptr);
  switch (size) {
  case 1: {
    reg_id_t reg_val = reg_resize_to_opsz(reg_addr, OPSZ_1);
    insert_load(drcontext, ilist, where, reg_val, reg_addr, 0, OPSZ_1);
    trace_buf_insert_buf_store(drcontext, buf, ilist, where, reg_ptr,
                               DR_REG_NULL, opnd_create_reg(reg_val), OPSZ_1,
                               offsetof(cache_t<size>, val));
    break;
  }
  case 2: {
    reg_id_t reg_val = reg_resize_to_opsz(reg_addr, OPSZ_2);
    insert_load(drcontext, ilist, where, reg_val, reg_addr, 0, OPSZ_2);
    trace_buf_insert_buf_store(drcontext, buf, ilist, where, reg_ptr,
                               DR_REG_NULL, opnd_create_reg(reg_val), OPSZ_2,
                               offsetof(cache_t<size>, val));
    break;
  }
  case 4: {
    reg_id_t reg_val = reg_resize_to_opsz(reg_addr, OPSZ_4);
    insert_load(drcontext, ilist, where, reg_val, reg_addr, 0, OPSZ_4);
    trace_buf_insert_buf_store(drcontext, buf, ilist, where, reg_ptr,
                               DR_REG_NULL, opnd_create_reg(reg_val), OPSZ_4,
                               offsetof(cache_t<size>, val));
    break;
  }
  case 8: {
    insert_load(drcontext, ilist, where, reg_addr, reg_addr, 0, OPSZ_8);
    trace_buf_insert_buf_store(drcontext, buf, ilist, where, reg_ptr,
                               DR_REG_NULL, opnd_create_reg(reg_addr), OPSZ_8,
                               offsetof(cache_t<size>, val));
    break;
  }
  case 16: {
    reg_id_t scratch;
    RESERVE_REG(drcontext, ilist, where, NULL, scratch);
    // 0-7B
    insert_load(drcontext, ilist, where, scratch, reg_addr, 0, OPSZ_8);
    trace_buf_insert_buf_store(drcontext, buf, ilist, where, reg_ptr,
                               DR_REG_NULL, opnd_create_reg(scratch), OPSZ_8,
                               offsetof(cache_t<size>, val));
    // 8-15B
    insert_load(drcontext, ilist, where, scratch, reg_addr, 8, OPSZ_8);
    trace_buf_insert_buf_store(drcontext, buf, ilist, where, reg_ptr,
                               DR_REG_NULL, opnd_create_reg(scratch), OPSZ_8,
                               offsetof(cache_t<size>, val) + 8);
    UNRESERVE_REG(drcontext, ilist, where, scratch);
    break;
  }
  case 32: {
    reg_id_t scratch;
    RESERVE_REG(drcontext, ilist, where, NULL, scratch);
    // 0-7B
    insert_load(drcontext, ilist, where, scratch, reg_addr, 0, OPSZ_8);
    trace_buf_insert_buf_store(drcontext, buf, ilist, where, reg_ptr,
                               DR_REG_NULL, opnd_create_reg(scratch), OPSZ_8,
                               offsetof(cache_t<size>, val));
    // 8-15B
    insert_load(drcontext, ilist, where, scratch, reg_addr, 8, OPSZ_8);
    trace_buf_insert_buf_store(drcontext, buf, ilist, where, reg_ptr,
                               DR_REG_NULL, opnd_create_reg(scratch), OPSZ_8,
                               offsetof(cache_t<size>, val) + 8);
    // 16-23B
    insert_load(drcontext, ilist, where, scratch, reg_addr, 16, OPSZ_8);
    trace_buf_insert_buf_store(drcontext, buf, ilist, where, reg_ptr,
                               DR_REG_NULL, opnd_create_reg(scratch), OPSZ_8,
                               offsetof(cache_t<size>, val) + 16);
    // 24-31B
    insert_load(drcontext, ilist, where, scratch, reg_addr, 24, OPSZ_8);
    trace_buf_insert_buf_store(drcontext, buf, ilist, where, reg_ptr,
                               DR_REG_NULL, opnd_create_reg(scratch), OPSZ_8,
                               offsetof(cache_t<size>, val) + 24);
    UNRESERVE_REG(drcontext, ilist, where, scratch);
    break;
  }
  }
}

/* Trace Memory Operands */
template <int sz, bool reg_dirty>
void insert_trace_for_mem(void *drcontext, instrlist_t *ilist, instr_t *where,
                          int DFGLog_ctxt, opnd_t mem_opnd, trace_buf_t *buf, 
                          reg_id_t scratch_0, reg_id_t scratch_1) {
  // restore if necessary
  if (reg_dirty) {
    if (opnd_uses_reg(mem_opnd, scratch_0) &&
        drreg_get_app_value(drcontext, ilist, where, scratch_0, scratch_0) !=
            DRREG_SUCCESS) {
      TRIVIALSPY_EXIT_PROCESS(
          "insert_trace_for_mem drreg_get_app_value reg_addr failed!");
    }
    if (opnd_uses_reg(mem_opnd, scratch_1) &&
        drreg_get_app_value(drcontext, ilist, where, scratch_1, scratch_1) !=
            DRREG_SUCCESS) {
      TRIVIALSPY_EXIT_PROCESS(
          "insert_trace_for_mem drreg_get_app_value scratch failed!");
    }
  }
  // calculate the memory address
  if (!drutil_insert_get_mem_addr(drcontext, ilist, where, mem_opnd,
                                  scratch_0 /*addr*/, scratch_1 /*scratch*/)) {
    TRIVIALSPY_EXIT_PROCESS(
        "insert_trace_for_mem drutil_insert_get_mem_addr failed!");
  }
  // insert loads from the given memory and store into the buffered trace
  insert_trace_value_in_mem<sz>(drcontext, ilist, where,
                                scratch_0 /*addr*/, scratch_1 /*ptr*/, buf);
  // insert stores of operand information
  insert_trace_info<sz>(drcontext, ilist, where, buf, DFGLog_ctxt,
                        scratch_0 /*reg_ptr*/, scratch_1);
  // update trace buffer
  trace_buf_insert_update_buf_ptr(drcontext, buf, ilist, where, scratch_0,
                                  DR_REG_NULL, sizeof(cache_t<sz>));
}
/* If the scratch registers are assumed to be dirty (modified), reg_dirty must be true */
template <int sz, bool reg_dirty>
/* Trace General Purpose Register Operands */
void insert_trace_for_gpr(void *drcontext, instrlist_t *ilist, instr_t *where,
                          int DFGLog_ctxt, opnd_t gpr_opnd, trace_buf_t *buf, 
                          reg_id_t scratch_0, reg_id_t scratch_1) {
  DR_ASSERT_MSG(opnd_is_reg(gpr_opnd),
                "insert_trace_for_gpr should only used for GPR operand!");
  // if there are register used in this operand, we directly use this operand
  reg_id_t reg_val, scratch, reg_used;
  reg_used = opnd_get_reg_used(gpr_opnd, 0);
  DR_ASSERT_MSG(reg_is_gpr(reg_used),
                "insert_trace_for_gpr should only used for GPR operand!");
  // get the operand size to resize the scratch registers
  opnd_size_t opsz = opnd_get_size(gpr_opnd);
  if (reg_used == reg_resize_to_opsz(scratch_0, opsz)) {
    // may need to restore the application value if it is modified
    if (reg_dirty) {
      if (drreg_get_app_value(drcontext, ilist, where, scratch_0, scratch_0) !=
          DRREG_SUCCESS) {
        TRIVIALSPY_EXIT_PROCESS(
            "insert_trace_for_gpr drreg_get_app_value scratch_0 failed!");
      }
    }
    reg_val = scratch_0;
    scratch = scratch_1;
  } else if (reg_used == reg_resize_to_opsz(scratch_1, opsz)) {
    // may need to restore the application value if it is modified
    if (reg_dirty) {
      if (drreg_get_app_value(drcontext, ilist, where, scratch_1, scratch_1) !=
          DRREG_SUCCESS) {
        TRIVIALSPY_EXIT_PROCESS(
            "insert_trace_for_gpr drreg_get_app_value scratch_0 failed!");
      }
    }
    reg_val = scratch_1;
    scratch = scratch_0;
  } else {
    reg_val = scratch_0;
    scratch = scratch_1;
    // copy the value in reg_used to reg_val;
    //dr_fprintf(STDOUT, "reg_val=%s, resized reg_val=%s, reg_used=%s\n", get_register_name(reg_val), get_register_name(reg_resize_to_opsz(reg_val, opsz)), get_register_name(reg_used));
    MINSERT(ilist, where,
            XINST_CREATE_move(
                drcontext, opnd_create_reg(reg_resize_to_opsz(reg_val, opsz)),
                opnd_create_reg(reg_used)));
  }
  // now the value is in the reserved register, insert stores to the buffered
  // trace
  insert_trace_value_in_reg<sz>(drcontext, ilist, where, buf, opsz, reg_val,
                                scratch);
  // insert stores of operand information
  insert_trace_info<sz>(drcontext, ilist, where, buf, DFGLog_ctxt,
                        reg_val /*reg_ptr*/, scratch);
  // update trace buffer
  trace_buf_insert_update_buf_ptr(drcontext, buf, ilist, where, reg_val,
                                  DR_REG_NULL, sizeof(cache_t<sz>));
}

/* Trace SIMD Register Operands */
template <int sz>
void insert_trace_for_simd(void *drcontext, instrlist_t *ilist, instr_t *where,
                           int DFGLog_ctxt, opnd_t simd_opnd, trace_buf_t *buf,
                           reg_id_t scratch_0, reg_id_t scratch_1) {
  // store opid and context handle into buffered trace
  insert_trace_info<sz>(drcontext, ilist, where, buf, DFGLog_ctxt,
                        scratch_0, scratch_1);
  // directly store the value in the target SIMD register into the cache
  // Note that the scratch_0 has already been loaded as the buffer pointer, so
  // no need to load again
  reg_id_t reg_simd = opnd_get_reg_used(simd_opnd, 0);
  insert_trace_value_in_simd<sz>(drcontext, ilist, where, buf, reg_simd,
                                 scratch_0);
  // update trace buffer
  trace_buf_insert_update_buf_ptr(drcontext, buf, ilist, where, scratch_0,
                                  DR_REG_NULL, sizeof(cache_t<sz>));
}

/* TODO: Trace X87 Operands (X86/64 only) */
void insert_trace_for_x87(void *drcontext, instrlist_t *ilist, instr_t *where,
                          int DFGLog_ctxt, opnd_t fp_opnd, trace_buf_t *buf,
                          reg_id_t scratch_0, reg_id_t scratch_1) {
#ifdef DEBUG_X87
  DR_ASSERT_MSG(false, "Not Implemented for x87 tracing!");
#else
  dr_fprintf(STDERR, "Warning: Trivial tracing & detection for X87 instructions are ignored!\n");
#endif
}

/* help function to get buffer trace */
trace_buf_t* get_trace_buf(bool is_float, int esize, int size) {
    if (is_float) {
        if(esize==4) {
            switch(size) {
                case 4:
                    return trace_buffer[6];
                case 16:
                    return trace_buffer[7];
                case 32:
                    return trace_buffer[8];
            }
        } else if(esize==8) {
            switch(size) {
                case 8:
                    return trace_buffer[9];
                case 16:
                    return trace_buffer[10];
                case 32:
                    return trace_buffer[11];
            }
        }
    } else {
        assert(size == esize);
        switch(size) {
            case 1: return trace_buffer[0];
            case 2: return trace_buffer[1];
            case 4: return trace_buffer[2];
            case 8: return trace_buffer[3];
            case 16: return trace_buffer[4];
            case 32: return trace_buffer[5];
        }
    }
    // Unknown trace buffer, alert for bug
    dr_fprintf(STDOUT, "ERROR: is_float=%d, esize=%d, size=%d\n", is_float, esize, size);
    assert(false && "Should not reach this");
    return NULL;
}

template<bool reg_dirty>
bool insert_trace(void* drcontext, instrlist_t* ilist, instr_t* where, opnd_t ref, int DFGLog_ctxt, int size, int esize, bool is_float, reg_id_t scratch_0, reg_id_t scratch_1) {
    // uint32_t size = opnd_size_in_bytes(opnd_get_size(ref));
    trace_buf_t* buf = get_trace_buf(is_float, esize, size);
    // now all information is ready, just dispatch the instrumentation with its types
    if(opnd_is_memory_reference(ref)) {
        switch(size) {
            case 1:
                insert_trace_for_mem<1, reg_dirty>(drcontext, ilist, where, DFGLog_ctxt, ref, buf, scratch_0, scratch_1);
                break;
            case 2:
                insert_trace_for_mem<2, reg_dirty>(drcontext, ilist, where, DFGLog_ctxt, ref, buf, scratch_0, scratch_1);
                break;
            case 4:
                insert_trace_for_mem<4, reg_dirty>(drcontext, ilist, where, DFGLog_ctxt, ref, buf, scratch_0, scratch_1);
                break;
            case 8:
                insert_trace_for_mem<8, reg_dirty>(drcontext, ilist, where, DFGLog_ctxt, ref, buf, scratch_0, scratch_1);
                break;
            case 16:
                insert_trace_for_mem<16, reg_dirty>(drcontext, ilist, where, DFGLog_ctxt, ref, buf, scratch_0, scratch_1);
                break;
            case 32:
                insert_trace_for_mem<32, reg_dirty>(drcontext, ilist, where, DFGLog_ctxt, ref, buf, scratch_0, scratch_1);
                break;
            default:
                assert(false && "Unknown operand size!");
        }
    } else if(opnd_is_reg(ref)) {
        reg_id_t reg = opnd_get_reg(ref);
        if(reg_is_gpr(reg)) {
            switch(size) {
                case 1:
                    insert_trace_for_gpr<1, reg_dirty>(drcontext, ilist, where, DFGLog_ctxt, ref, buf, scratch_0, scratch_1);
                    break;
                case 2:
                    insert_trace_for_gpr<2, reg_dirty>(drcontext, ilist, where, DFGLog_ctxt, ref, buf, scratch_0, scratch_1);
                    break;
                case 4:
                    insert_trace_for_gpr<4, reg_dirty>(drcontext, ilist, where, DFGLog_ctxt, ref, buf, scratch_0, scratch_1);
                    break;
                case 8:
                    insert_trace_for_gpr<8, reg_dirty>(drcontext, ilist, where, DFGLog_ctxt, ref, buf, scratch_0, scratch_1);
                    break;
                default:
                    assert(false && "Unknown operand size!");
            }
        } else if(reg_is_simd(reg)) {
            switch(size) {
                case 4:
                    insert_trace_for_simd<4>(drcontext, ilist, where, DFGLog_ctxt, ref, buf, scratch_0, scratch_1);
                    break;
                case 8:
                    insert_trace_for_simd<8>(drcontext, ilist, where, DFGLog_ctxt, ref, buf, scratch_0, scratch_1);
                    break;
                case 16:
                    insert_trace_for_simd<16>(drcontext, ilist, where, DFGLog_ctxt, ref, buf, scratch_0, scratch_1);
                    break;
                case 32:
                    insert_trace_for_simd<32>(drcontext, ilist, where, DFGLog_ctxt, ref, buf, scratch_0, scratch_1);
                    break;
                default:
                    assert(false && "Unknown operand size!");
            }
        } else if(reg_is_fp(reg)) {
            // TODO: Not implemented (as the modern compilers will avoid using heavy x87 instructions)
            insert_trace_for_x87(drcontext, ilist, where, DFGLog_ctxt, ref, buf, scratch_0, scratch_1);
        } else {
            dr_fprintf(STDERR, "For REG %s\n", get_register_name(reg)); fflush(stdout);
            assert(false && "Unknown register type!");
        }
    } else if(opnd_is_immed(ref)) {
        // get the immediate value and check it statically
        assert(false && "Should not trace for immediate value!\n");
    } else {
        assert(false && "Unknown operand type!\n");
    }
    return true;
}

#endif