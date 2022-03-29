#ifndef __VTRACER_H__
#define __VTRACER_H__

#include "../common/log.h"
#ifdef DEBUG
#define VTRACER_LOG(level, format, args...) LOG("<vtracer>", level, format, ##args)
#else
#define VTRACER_LOG(level, format, args...)
#endif

enum {
    /** Priority of drx_buf thread init event */
    DRMGR_PRIORITY_THREAD_INIT_TRACE_BUF = -7500,
    /** Priority of drx_buf thread exit event */
    DRMGR_PRIORITY_THREAD_EXIT_TRACE_BUF = -7500,
};

#define DRMGR_PRIORITY_NAME_TRACE_BUF_INIT "vtracer.init"
#define DRMGR_PRIORITY_NAME_TRACE_BUF_EXIT "vtracer.exit"

typedef void (*vtracer_buf_full_cb_t)(void *buf_base, void *buf_end, void* user_data);
typedef size_t (*vtracer_buf_fill_num_cb_t)(void *drcontext, instr_t *where, void* user_data);

/* Low-level VTracer Interfaces for VProfile Framework */
typedef struct {
    uint buf_size;
    uint vec_idx; /* index into the clients vector */
    /* callbacks for buffer checking and updating */
    vtracer_buf_full_cb_t full_cb;
    void* user_data_full;
    vtracer_buf_fill_num_cb_t fill_num_cb;
    void* user_data_fill_num;
    /* tls implementation */
    int tls_idx;
    uint tls_offs;
    reg_id_t tls_seg;
} vtrace_buffer_t;

/* VTracer Interface Functions */

/* Initialize VTracer. Return false when any failure detected. */
DR_EXPORT
bool vtracer_init(void);
/* Clear and free all the resourced allocated by VTracer. */
DR_EXPORT
void vtracer_exit(void);

/**
 * Create trace buffer with size specified as buffer_size:
 * @param buffer_size: buffer size in bytes for trace buffer
 *
 * This routine will create trace buffer with NULL callbacks registered:
 * @see vtracer_create_trace_buffer_ex(buffer_size, NULL, NULL, NULL, NULL)
 *
 * Return value: a newly created trace buffer pointer with the given size;
 * return NULL when an failure detected. */
DR_EXPORT
vtrace_buffer_t *vtracer_create_trace_buffer(uint buffer_size);

/**
 * Create trace buffer with additional callbacks:
 * @param buffer_size: buffer size in bytes for trace buffer.
 * @param full_cb: the callback to register which will be called to update with
 * the buffered trace when the trace buffer is detected as full.
 * May only be NULL when @see fill_num_cb is NULL; otherwise assert for usage
 * error.
 * @param user_data_full: the user-specified data for the full_cb callback. This
 * registered user_data will send as the user_data parameter of full_cb
 * callback.
 * @param fill_num_cb: the callback to register which will be called during
 * analysis phase to obtain how many slots (elements) will be filled in for the
 * given instruction.
 * @param user_data_fill_num: the user-specified data for the fill_num_cb callbacks.
 * This registered user_data will send as the user_data parameter of fill_num_cb
 * callback.
 * May only be NULL when @see full_cb is NULL; otherwise assert for usage error.
 *
 * With a given non-NULL callbacks, VTracer will guarantee that the full_cb will
 * be called once the trace buffer is detected as full, and fill_num_cb is used
 * to estimate the filled in slots for detecting potential trace buffer
 * overflow. When the VTracer detects any inconsistancy of estimated filled in
 * number given by fill_num_cb and actual filled in slots (expensive check only
 * in DEBUG mode), it will assert for a usage error with sufficient information
 * for further debugging.
 *
 * Return value: a newly created trace buffer pointer with the given size and
 * registered callbacks; return NULL when an failure detected. */
DR_EXPORT
vtrace_buffer_t *
vtracer_create_trace_buffer_ex(uint buffer_size,
                               vtracer_buf_full_cb_t full_cb,
                               void* user_data_full,
                               vtracer_buf_fill_num_cb_t fill_num_cb,
                               void *user_data_fill_num);
/**
 * Free all resources allocated for this vtrace buffer: @param buf */
DR_EXPORT
void vtracer_buffer_free(vtrace_buffer_t *buf);

/* TODO: May need to extend API to support more sampling methods */
/* Sampling Configurations */
/**
 * Enable sampling (bursty sampling) for specified vtrace buffer:
 * @param vtrace_buffer: sampled vtrace buffer with bursty sampling method.
 * @param window_enable: sample window size to enable value tracing & updating.
 * @param window_disable: non-sample window size to disable value tracing &
 * updating.
 *
 * The sample rate can be calculated as:
 *       window_enable / window_disable
 * The sampling window is applied in the number of executed instructions. */
DR_EXPORT
void vtracer_enable_sampling(int window_enable,
                             int window_disable);

/* Disable sampling for vtracer buffer. */
DR_EXPORT
void vtracer_disable_sampling();

/**
 * Get the sampling state of vtrace buffer:
 * @param vtrace_buffer: the target vtrace buffer to query the sampling state;
 * @param window_enable: if not NULL, it will store the returned value
 * indicating the registered window_enable value; May be NULL if not wanted.
 * @param window_disable: if not NULL, it will store the returned value
 * indicating the registered window_disable value; May be NULL if not wanted.
 *
 * Return true if sampling enabled; otherwise return false. */
DR_EXPORT
bool vtracer_get_sampling_state(void *drcontext);

/**
 * Get the current trace buffer pointer in the given register:
 * @param vtrace_buffer: the vtrace buffer to query the pointer value
 * @param reg_ptr: the target register to store the current trace buffer of
 * vtrace_buffer. The register should be reserved by the caller.
 *
 * The instrumentation will be inserted before @param where in @param ilist */
DR_EXPORT
void vtracer_get_trace_buffer_in_reg(void *drcontext, instr_t *where,
                                     instrlist_t *ilist,
                                     vtrace_buffer_t *vtrace_buffer,
                                     reg_id_t reg_ptr);

/**
 * Move the trace buffer pointer forward and store back to the vtrace_buffer if
 * valid:
 * @param size: the size in bytes to move forward the trace buffer pointer in
 * reg_ptr.
 * @param vtrace_buffer: the vtrace buffer to store back the forwarded pointer.
 * May be NULL when writeback to the vtrace buffer is not expected. In that
 * case, VTracer will not insert instrumetations to update trace buffer
 * pointers.
 * @param reg_ptr: the register to hold the value of current buffer pointer. It
 * will hold new forwarded buffer pointer if vtrace_buffer is not NULL; otherwise
 * its return value is undefined.
 * @param scratch: free scratch register given by caller. its return value is
 * undefined.
 * */
DR_EXPORT
void vtracer_insert_trace_forward(void *drcontext, instr_t *where,
                                  instrlist_t *ilist, int size,
                                  vtrace_buffer_t *vtrace_buffer,
                                  reg_id_t reg_ptr, reg_id_t scratch);

void
vtracer_insert_load_buf_ptr(void *drcontext, vtrace_buffer_t *buf, instrlist_t *ilist,
                            instr_t *where, reg_id_t buf_ptr);

void
vtracer_insert_load_buf_end(void *drcontext, vtrace_buffer_t *buf, instrlist_t *ilist,
                            instr_t *where, reg_id_t buf_ptr);

void
vtracer_insert_clear_buf(void *drcontext, vtrace_buffer_t *buf, instrlist_t *ilist,
                            instr_t *where, reg_id_t scratch);
DR_EXPORT
void *
vtracer_get_buf_ptr(void *drcontext, vtrace_buffer_t *buf);

DR_EXPORT
void *
vtracer_get_buf_base(void *drcontext, vtrace_buffer_t *buf);

/**
 * Insert instrumentations to store the value of ref into vtrace_buffer, where:
 * @param ref: can be register/memory/immediate operand
 * @param reg_ptr: the register to hold the value of current buffer pointer. Its
 * value will not be modified.
 * @param scratch: free scratch register given by caller whose return value will
 * be undefined.
 * @param offset: offsets to the target domain of a value trace slot.
 *
 * In general, this function will insert codes equivilant to:
 *      <reg_ptr>.<offset> = <ref>
 * Instrumentation is inserted before @param where.
 * */
DR_EXPORT
void vtracer_insert_trace_val(void *drcontext, instr_t *where,
                              instrlist_t *ilist, opnd_t ref, reg_id_t reg_ptr,
                              reg_id_t scratch, ushort offset);

/**
 * Insert instrumentations to store the constant value of type T into
 * vtrace_buffer:
 * @param T: the constant value's data type. Only size of 1,2,4,8,16,32,(64 for
 * AVX512) is supported; otherwise it will result in static assertion failure at
 * compile time.
 * @param val: the source constant value for tracing
 * @param reg_ptr: the register to hold the value of current buffer pointer. Its
 * value will not be modified.
 * @param scratch: free scratch register given by caller whose return value will
 * be undefined.
 * @param offset: offsets to the target domain of a value trace slot.
 * 
 * In general, this function will insert codes equivilant to:
 *      <reg_ptr>.<offset> = <val>
 * Instrumentation is inserted before @param where.
 * */
template <typename T>
DR_EXPORT
void vtracer_insert_trace_constant(void *drcontext, instr_t *where,
                                   instrlist_t *ilist, T val, reg_id_t reg_ptr,
                                   reg_id_t scratch, ushort offset)
{
    /* inlined implementation to avoid undefined symbol error during usage */
    VTRACER_LOG(SUMMARY, "enter vtracer_insert_trace_constant\n");
    switch (sizeof(T)) {
        case 1:
            vtracer_insert_trace_val(drcontext, where, ilist, OPND_CREATE_INT8(val), reg_ptr, scratch, offset);
            break;
        case 2:
            vtracer_insert_trace_val(drcontext, where, ilist, OPND_CREATE_INT16(val), reg_ptr, scratch, offset);
            break;
        case 4:
            vtracer_insert_trace_val(drcontext, where, ilist, OPND_CREATE_INT32(val), reg_ptr, scratch, offset);
            break;
        case 8:
            vtracer_insert_trace_val(drcontext, where, ilist, OPND_CREATE_INT64(val), reg_ptr, scratch, offset);
            break;
        default:
            DR_ASSERT_MSG(false, "Unknown constant size!\n");
    }
    VTRACER_LOG(SUMMARY, "exit vtracer_insert_trace_constant\n");
}

/**
 * Can be called by clean call func to trace T into buffer.
 * @param T: passed in struct type.
 * @param buf: the vtrace buffer to be filled in.
 * @param cache: passed in struct, will be filled into buffer.
 * */
template<typename T>
DR_EXPORT
void vtracer_update_clean_call(void *drcontext, vtrace_buffer_t *buf, T cache) {
    byte *buf_ptr = (byte*)vtracer_get_buf_ptr(drcontext, buf);
    memcpy(buf_ptr, &cache, sizeof(T));
    buf_ptr = buf_ptr + sizeof(T);
#ifdef VTRACER_DEBUG
    per_thread_t *data = (per_thread_t*)drmgr_get_tls_field(drcontext, buf->tls_idx);
    data->scratch -= sizeof(T);
    DR_ASSERT_MSG(data->scratch >= 0, "Usage Error: Estimated Fill number too small!");
#endif
}

/**
 * Mark some instructions like nop are ignorable.
 * @param ins: instruction for checking.
 * 
 * return true if ins is ignorable.
 * */
DR_EXPORT
bool instr_is_ignorable(instr_t *ins);
#endif