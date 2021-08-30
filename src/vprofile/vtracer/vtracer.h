#ifndef __VTRACER_H__
#define __VTRACER_H__

/* Low-level VTracer Interfaces for VProfile Framework */
struct _trace_buf_t;
typedef _trace_buf_t vtrace_buffer_t;

typedef void (*vtracer_buf_full_cb_t)(void *buf_base, void *buf_end);
typedef size_t (*vtracer_buf_fill_num_cb_t)(void *drcontext, instr_t *where);

/* VTracer Interface Functions */
bool vtracer_init(void);
void vtracer_fini(void);

/*  */
vtrace_buffer_t *
vtracer_create_trace_buffer(size_t buffer_size);
/*  */
vtrace_buffer_t *
vtracer_create_trace_buffer_ex(size_t buffer_size, vtracer_buf_full_cb_t full_cb,
                               vtracer_buf_fill_num_cb_t fill_num_cb);
/*  */
bool
vtracer_buffer_free(vtrace_buffer_t *buf);

/* TODO: May need to extend API to support more sampling methods */
/* Configure for bursty sampling */
void vtracer_enable_sampling(vtrace_buffer_t *vtrace_buffer, int window_enable,
                             int window_disable);
void vtracer_disable_sampling(vtrace_buffer_t *vtrace_buffer);
bool vtracer_get_sampling_state(vtrace_buffer_t *vtrace_buffer,
                                int *window_enable, int *window_disable);

/* store values of ref into vtrace_buffer, where ref can be: register/memory/immediate operand */
void vtracer_insert_trace_val(void* drcontext, instr_t* where, instrlist_t* ilist, opnd_t ref, vtrace_buffer_t* vtrace_buffer, ushort offset);

template<typename T>
void vtracer_insert_trace_constant(void* drcontext, instr_t* where, instrlist_t* ilist, T val, vtrace_buffer_t* vtrace_buffer, ushort offset);
#endif