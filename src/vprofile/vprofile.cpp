#include <stddef.h> /* for offsetof */

#include "vprofile.h"
#include "drmgr.h"
#include "drutil.h"
#include "drvector.h"
#include "utils.h"

#ifdef DEBUG
    #define VPROFILE_DEBUG
#endif

/* Max val size (INT512). */
#define MAX_CLASS_SIZE 64
/* Max number of mem_ref a buffer can have. */
#define MAX_NUM_MEM_REFS 4096

/* Cache data structure to store in buffered trace */
struct cache_t_ctxt {
    uint64_t addr;
    int32_t ctxt_hndl;
    int8_t val[MAX_CLASS_SIZE];
    void *info;
    uint8_t size;
    uint8_t esize;
    bool is_float;
};

struct cache_t_ctxt_no_info {
    uint64_t addr;
    int32_t ctxt_hndl;
    int8_t val[MAX_CLASS_SIZE];
    uint8_t size;
    uint8_t esize;
    bool is_float;
};

struct cache_t_ctxt_no_addr {
    int32_t ctxt_hndl;
    int8_t val[MAX_CLASS_SIZE];
    void *info;
    uint8_t size;
    uint8_t esize;
    bool is_float;
};

struct cache_t_ctxt_no_info_addr {
    int32_t ctxt_hndl;
    int8_t val[MAX_CLASS_SIZE];
    uint8_t size;
    uint8_t esize;
    bool is_float;
};

template<int sz>
struct cache_t_ctxt_no_sz {
    uint64_t addr;
    int32_t ctxt_hndl;
    int8_t val[sz];
    void *info;
};

template<int sz>
struct cache_t_ctxt_no_info_sz {
    uint64_t addr;
    int32_t ctxt_hndl;
    int8_t val[sz];
};

template<int sz>
struct cache_t_ctxt_no_addr_sz {
    int32_t ctxt_hndl;
    int8_t val[sz];
    void *info;
};

template<int sz>
struct cache_t_ctxt_no_info_addr_sz {
    int32_t ctxt_hndl;
    int8_t val[sz];
};

struct cache_t {
    uint64_t addr;
    int8_t val[MAX_CLASS_SIZE];
    void *info;
    uint8_t size;
    uint8_t esize;
    bool is_float;
};

struct cache_t_no_info {
    uint64_t addr;
    int8_t val[MAX_CLASS_SIZE];
    uint8_t size;
    uint8_t esize;
    bool is_float;
};

struct cache_t_no_addr {
    int8_t val[MAX_CLASS_SIZE];
    void *info;
    uint8_t size;
    uint8_t esize;
    bool is_float;
};

struct cache_t_no_info_addr {
    int8_t val[MAX_CLASS_SIZE];
    uint8_t size;
    uint8_t esize;
    bool is_float;
};

template<int sz>
struct cache_t_no_sz {
    uint64_t addr;
    int8_t val[sz];
    void *info;
};

template<int sz>
struct cache_t_no_info_sz {
    uint64_t addr;
    int8_t val[sz];
};

template<int sz>
struct cache_t_no_addr_sz {
    int8_t val[sz];
    void *info;
};

template<int sz>
struct cache_t_no_info_addr_sz {
    int8_t val[sz];
};

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

template<int size>
inline __attribute__((always_inline)) void insertStoreTraceBuffer(void *drcontext, instrlist_t *bb, instr_t *instr, int32_t slot, opnd_t opnd, vtrace_t *trace, reg_id_t reg_addr, reg_id_t reg_ptr, reg_id_t scratch)
{
    bool is_float = instr_is_floating(instr);
    int esize = (is_float) ? FloatOperandSizeTable(instr, opnd) : IntegerOperandSizeTable(instr, opnd);
#ifdef VPROFILE_DEBUG
    if(esize == 0) {
        debug_unknown_case(drcontext, instr, opnd);
        // esize = size;
    }
#endif
    // int size = opnd_size_in_bytes(opnd_get_size(opnd));
    if(trace->strictly_ordered) {
        if((*(bool (*)(opnd_t))trace->buff->user_data_fill_num)(opnd)) {
            // use size&&esize&&is_float and then insert trace val
            if(trace->trace_addr || trace->trace_cct) {
                if(trace->trace_cct) {
                    if(trace->trace_addr) {
                        if(trace->trace_info) {
                            // cache_t_ctxt
                            // ctxt_hndl
                            drcctlib_get_context_handle_in_reg(drcontext, bb, instr, slot, reg_addr, reg_ptr);
                            vtracer_get_trace_buffer_in_reg(drcontext, instr, bb, trace->buff, reg_ptr);
                            vtracer_insert_trace_val(drcontext, instr, bb, opnd_create_reg(reg_addr), reg_ptr, scratch, offsetof(cache_t_ctxt, ctxt_hndl));
                            // addr
                            if(!drutil_insert_get_mem_addr(drcontext, bb, instr, opnd, reg_addr/*addr*/, reg_ptr/*scratch*/)) {
                                    DR_ASSERT_MSG(false, "InstrumentInsCallback drutil_insert_get_mem_addr failed!");
                            }
                            vtracer_get_trace_buffer_in_reg(drcontext, instr, bb, trace->buff, reg_ptr);
                            vtracer_insert_trace_val(drcontext, instr, bb, opnd_create_reg(reg_addr), reg_ptr, scratch, offsetof(cache_t_ctxt, addr));
                            // info
                            opnd_t info = OPND_CREATE_INTPTR((*global_client_cb.user_data_cb)(drcontext, instr, bb, opnd));
                            vtracer_insert_trace_val(drcontext, instr, bb, info, reg_ptr, scratch, offsetof(cache_t_ctxt, info));
                            // val/size/esize/is_float
                            vtracer_insert_trace_val(drcontext, instr, bb, opnd, reg_ptr, scratch, offsetof(cache_t_ctxt, val));
                            vtracer_insert_trace_val(drcontext, instr, bb, OPND_CREATE_INT8(size), reg_ptr, scratch, offsetof(cache_t_ctxt, size));
                            vtracer_insert_trace_val(drcontext, instr, bb, OPND_CREATE_INT8(esize), reg_ptr, scratch, offsetof(cache_t_ctxt, esize));
                            vtracer_insert_trace_val(drcontext, instr, bb, OPND_CREATE_INT8(is_float), reg_ptr, scratch, offsetof(cache_t_ctxt, is_float));
                            // update buf ptr
                            vtracer_insert_trace_forward(drcontext, instr,  bb, sizeof(cache_t_ctxt), trace->buff, reg_ptr, scratch);
                        } else {
                            // cache_t_ctxt_no_info
                            // ctxt_hndl
                            drcctlib_get_context_handle_in_reg(drcontext, bb, instr, slot, reg_addr, reg_ptr);
                            vtracer_get_trace_buffer_in_reg(drcontext, instr, bb, trace->buff, reg_ptr);
                            vtracer_insert_trace_val(drcontext, instr, bb, opnd_create_reg(reg_addr), reg_ptr, scratch, offsetof(cache_t_ctxt_no_info, ctxt_hndl));
                            // addr
                            if(!drutil_insert_get_mem_addr(drcontext, bb, instr, opnd, reg_addr/*addr*/, reg_ptr/*scratch*/)) {
                                    DR_ASSERT_MSG(false, "InstrumentInsCallback drutil_insert_get_mem_addr failed!");
                            }
                            vtracer_get_trace_buffer_in_reg(drcontext, instr, bb, trace->buff, reg_ptr);
                            vtracer_insert_trace_val(drcontext, instr, bb, opnd_create_reg(reg_addr), reg_ptr, scratch, offsetof(cache_t_ctxt_no_info, addr));
                            // val/size/esize/is_float
                            vtracer_insert_trace_val(drcontext, instr, bb, opnd, reg_ptr, scratch, offsetof(cache_t_ctxt_no_info, val));
                            vtracer_insert_trace_val(drcontext, instr, bb, OPND_CREATE_INT8(size), reg_ptr, scratch, offsetof(cache_t_ctxt_no_info, size));
                            vtracer_insert_trace_val(drcontext, instr, bb, OPND_CREATE_INT8(esize), reg_ptr, scratch, offsetof(cache_t_ctxt_no_info, esize));
                            vtracer_insert_trace_val(drcontext, instr, bb, OPND_CREATE_INT8(is_float), reg_ptr, scratch, offsetof(cache_t_ctxt_no_info, is_float));
                            // update buf ptr
                            vtracer_insert_trace_forward(drcontext, instr,  bb, sizeof(cache_t_ctxt_no_info), trace->buff, reg_ptr, scratch);
                        }
                    } else {
                        if(trace->trace_info) {
                            // cache_t_ctxt_no_addr
                            // ctxt_hndl
                            drcctlib_get_context_handle_in_reg(drcontext, bb, instr, slot, reg_addr, reg_ptr);
                            vtracer_get_trace_buffer_in_reg(drcontext, instr, bb, trace->buff, reg_ptr);
                            vtracer_insert_trace_val(drcontext, instr, bb, opnd_create_reg(reg_addr), reg_ptr, scratch, offsetof(cache_t_ctxt_no_addr, ctxt_hndl));
                            // info
                            opnd_t info = OPND_CREATE_INTPTR((*global_client_cb.user_data_cb)(drcontext, instr, bb, opnd));
                            vtracer_insert_trace_val(drcontext, instr, bb, info, reg_ptr, scratch, offsetof(cache_t_ctxt_no_addr, info));
                            // val/size/esize/is_float
                            vtracer_insert_trace_val(drcontext, instr, bb, opnd, reg_ptr, scratch, offsetof(cache_t_ctxt_no_addr, val));
                            vtracer_insert_trace_val(drcontext, instr, bb, OPND_CREATE_INT8(size), reg_ptr, scratch, offsetof(cache_t_ctxt_no_addr, size));
                            vtracer_insert_trace_val(drcontext, instr, bb, OPND_CREATE_INT8(esize), reg_ptr, scratch, offsetof(cache_t_ctxt_no_addr, esize));
                            vtracer_insert_trace_val(drcontext, instr, bb, OPND_CREATE_INT8(is_float), reg_ptr, scratch, offsetof(cache_t_ctxt_no_addr, is_float));
                            // update buf ptr
                            vtracer_insert_trace_forward(drcontext, instr,  bb, sizeof(cache_t_ctxt_no_addr), trace->buff, reg_ptr, scratch);
                        } else {
                            // cache_t_ctxt_no_info_addr
                            // ctxt_hndl
                            drcctlib_get_context_handle_in_reg(drcontext, bb, instr, slot, reg_addr, reg_ptr);
                            vtracer_get_trace_buffer_in_reg(drcontext, instr, bb, trace->buff, reg_ptr);
                            vtracer_insert_trace_val(drcontext, instr, bb, opnd_create_reg(reg_addr), reg_ptr, scratch, offsetof(cache_t_ctxt_no_info_addr, ctxt_hndl));
                            // val/size/esize/is_float
                            vtracer_insert_trace_val(drcontext, instr, bb, opnd, reg_ptr, scratch, offsetof(cache_t_ctxt_no_info_addr, val));
                            vtracer_insert_trace_val(drcontext, instr, bb, OPND_CREATE_INT8(size), reg_ptr, scratch, offsetof(cache_t_ctxt_no_info_addr, size));
                            vtracer_insert_trace_val(drcontext, instr, bb, OPND_CREATE_INT8(esize), reg_ptr, scratch, offsetof(cache_t_ctxt_no_info_addr, esize));
                            vtracer_insert_trace_val(drcontext, instr, bb, OPND_CREATE_INT8(is_float), reg_ptr, scratch, offsetof(cache_t_ctxt_no_info_addr, is_float));
                            // update buf ptr
                            vtracer_insert_trace_forward(drcontext, instr,  bb, sizeof(cache_t_ctxt_no_info_addr), trace->buff, reg_ptr, scratch);
                        }
                    }
                } else {
                    if(!drutil_insert_get_mem_addr(drcontext, bb, instr, opnd, reg_addr/*addr*/, reg_ptr/*scratch*/)) {
                            DR_ASSERT_MSG(false, "InstrumentInsCallback drutil_insert_get_mem_addr failed!");
                    }
                    vtracer_get_trace_buffer_in_reg(drcontext, instr, bb, trace->buff, reg_ptr);
                    if(trace->trace_info) {
                        // info
                        opnd_t info = OPND_CREATE_INTPTR((*global_client_cb.user_data_cb)(drcontext, instr, bb, opnd));
                        vtracer_insert_trace_val(drcontext, instr, bb, info, reg_ptr, scratch, offsetof(cache_t, info));
                        vtracer_insert_trace_val(drcontext, instr, bb, opnd_create_reg(reg_addr), reg_ptr, scratch, offsetof(cache_t, addr));
                        vtracer_insert_trace_val(drcontext, instr, bb, opnd, reg_ptr, scratch, offsetof(cache_t, val));
                        vtracer_insert_trace_val(drcontext, instr, bb, OPND_CREATE_INT8(size), reg_ptr, scratch, offsetof(cache_t, size));
                        vtracer_insert_trace_val(drcontext, instr, bb, OPND_CREATE_INT8(esize), reg_ptr, scratch, offsetof(cache_t, esize));
                        vtracer_insert_trace_val(drcontext, instr, bb, OPND_CREATE_INT8(is_float), reg_ptr, scratch, offsetof(cache_t, is_float));
                        vtracer_insert_trace_forward(drcontext, instr,  bb, sizeof(cache_t), trace->buff, reg_ptr, scratch);
                    } else {
                        vtracer_insert_trace_val(drcontext, instr, bb, opnd_create_reg(reg_addr), reg_ptr, scratch, offsetof(cache_t_no_info, addr));
                        vtracer_insert_trace_val(drcontext, instr, bb, opnd, reg_ptr, scratch, offsetof(cache_t_no_info, val));
                        vtracer_insert_trace_val(drcontext, instr, bb, OPND_CREATE_INT8(size), reg_ptr, scratch, offsetof(cache_t_no_info, size));
                        vtracer_insert_trace_val(drcontext, instr, bb, OPND_CREATE_INT8(esize), reg_ptr, scratch, offsetof(cache_t_no_info, esize));
                        vtracer_insert_trace_val(drcontext, instr, bb, OPND_CREATE_INT8(is_float), reg_ptr, scratch, offsetof(cache_t_no_info, is_float));
                        vtracer_insert_trace_forward(drcontext, instr,  bb, sizeof(cache_t_no_info), trace->buff, reg_ptr, scratch);
                    }
                }
            } else {
                vtracer_get_trace_buffer_in_reg(drcontext, instr, bb, trace->buff, reg_ptr);
                if(trace->trace_info) {
                    // info
                    opnd_t info = OPND_CREATE_INTPTR((*global_client_cb.user_data_cb)(drcontext, instr, bb, opnd));
                    vtracer_insert_trace_val(drcontext, instr, bb, info, reg_ptr, scratch, offsetof(cache_t_no_addr, info));
                    vtracer_insert_trace_val(drcontext, instr, bb, opnd, reg_ptr, scratch, offsetof(cache_t_no_addr, val));
                    vtracer_insert_trace_val(drcontext, instr, bb, OPND_CREATE_INT8(size), reg_ptr, scratch, offsetof(cache_t_no_addr, size));
                    vtracer_insert_trace_val(drcontext, instr, bb, OPND_CREATE_INT8(esize), reg_ptr, scratch, offsetof(cache_t_no_addr, esize));
                    vtracer_insert_trace_val(drcontext, instr, bb, OPND_CREATE_INT8(is_float), reg_ptr, scratch, offsetof(cache_t_no_addr, is_float));
                    vtracer_insert_trace_forward(drcontext, instr,  bb, sizeof(cache_t_no_addr), trace->buff, reg_ptr, scratch);
                } else {
                    vtracer_insert_trace_val(drcontext, instr, bb, opnd, reg_ptr, scratch, offsetof(cache_t_no_info_addr, val));
                    vtracer_insert_trace_val(drcontext, instr, bb, OPND_CREATE_INT8(size), reg_ptr, scratch, offsetof(cache_t_no_info_addr, size));
                    vtracer_insert_trace_val(drcontext, instr, bb, OPND_CREATE_INT8(esize), reg_ptr, scratch, offsetof(cache_t_no_info_addr, esize));
                    vtracer_insert_trace_val(drcontext, instr, bb, OPND_CREATE_INT8(is_float), reg_ptr, scratch, offsetof(cache_t_no_info_addr, is_float));
                    vtracer_insert_trace_forward(drcontext, instr,  bb, sizeof(cache_t_no_info_addr), trace->buff, reg_ptr, scratch);
                }
            }
        }
    } else {
        vtrace_buffer_t* buf = get_trace_buf(trace, is_float, esize, size);
        if(!buf) {
#ifdef VPROFILE_DEBUG
            debug_unknown_case(drcontext, instr, opnd);
#endif
            return;
        }
        if((*(bool (*)(opnd_t))buf->user_data_fill_num)(opnd)) {
            // use size&&esize&&is_float and then choose the right buf to insert trace val
            if(trace->trace_addr || trace->trace_cct) {
                if(trace->trace_cct) {
                    if(trace->trace_addr) {
                        if(trace->trace_info) {
                            // cache_t_ctxt_no_sz
                            // ctxt_hndl
                            drcctlib_get_context_handle_in_reg(drcontext, bb, instr, slot, reg_addr, reg_ptr);
                            vtracer_get_trace_buffer_in_reg(drcontext, instr, bb, buf, reg_ptr);
                            vtracer_insert_trace_val(drcontext, instr, bb, opnd_create_reg(reg_addr), reg_ptr, scratch, offsetof(cache_t_ctxt_no_sz<size>, ctxt_hndl));
                            // addr
                            if(!drutil_insert_get_mem_addr(drcontext, bb, instr, opnd, reg_addr/*addr*/, reg_ptr/*scratch*/)) {
                                    DR_ASSERT_MSG(false, "InstrumentInsCallback drutil_insert_get_mem_addr failed!");
                            }
                            vtracer_get_trace_buffer_in_reg(drcontext, instr, bb, buf, reg_ptr);
                            vtracer_insert_trace_val(drcontext, instr, bb, opnd_create_reg(reg_addr), reg_ptr, scratch, offsetof(cache_t_ctxt_no_sz<size>, addr));
                            // info
                            opnd_t info = OPND_CREATE_INTPTR((*global_client_cb.user_data_cb)(drcontext, instr, bb, opnd));
                            vtracer_insert_trace_val(drcontext, instr, bb, info, reg_ptr, scratch, offsetof(cache_t_ctxt_no_sz<size>, info));
                            // val
                            vtracer_insert_trace_val(drcontext, instr, bb, opnd, reg_ptr, scratch, offsetof(cache_t_ctxt_no_sz<size>, val));
                            // update buf ptr
                            vtracer_insert_trace_forward(drcontext, instr,  bb, sizeof(cache_t_ctxt_no_sz<size>), buf, reg_ptr, scratch);
                        } else {
                            // cache_t_ctxt_no_info_sz
                            // ctxt_hndl
                            drcctlib_get_context_handle_in_reg(drcontext, bb, instr, slot, reg_addr, reg_ptr);
                            vtracer_get_trace_buffer_in_reg(drcontext, instr, bb, buf, reg_ptr);
                            vtracer_insert_trace_val(drcontext, instr, bb, opnd_create_reg(reg_addr), reg_ptr, scratch, offsetof(cache_t_ctxt_no_info_sz<size>, ctxt_hndl));
                            // addr
                            if(!drutil_insert_get_mem_addr(drcontext, bb, instr, opnd, reg_addr/*addr*/, reg_ptr/*scratch*/)) {
                                    DR_ASSERT_MSG(false, "InstrumentInsCallback drutil_insert_get_mem_addr failed!");
                            }
                            vtracer_get_trace_buffer_in_reg(drcontext, instr, bb, buf, reg_ptr);
                            vtracer_insert_trace_val(drcontext, instr, bb, opnd_create_reg(reg_addr), reg_ptr, scratch, offsetof(cache_t_ctxt_no_info_sz<size>, addr));
                            // val
                            vtracer_insert_trace_val(drcontext, instr, bb, opnd, reg_ptr, scratch, offsetof(cache_t_ctxt_no_info_sz<size>, val));
                            // update buf ptr
                            vtracer_insert_trace_forward(drcontext, instr,  bb, sizeof(cache_t_ctxt_no_info_sz<size>), buf, reg_ptr, scratch);
                        }
                    } else {
                        if(trace->trace_info) {
                            // cache_t_ctxt_no_addr_sz
                            // ctxt_hndl
                            drcctlib_get_context_handle_in_reg(drcontext, bb, instr, slot, reg_addr, reg_ptr);
                            vtracer_get_trace_buffer_in_reg(drcontext, instr, bb, buf, reg_ptr);
                            vtracer_insert_trace_val(drcontext, instr, bb, opnd_create_reg(reg_addr), reg_ptr, scratch, offsetof(cache_t_ctxt_no_addr_sz<size>, ctxt_hndl));
                            // info
                            opnd_t info = OPND_CREATE_INTPTR((*global_client_cb.user_data_cb)(drcontext, instr, bb, opnd));
                            vtracer_insert_trace_val(drcontext, instr, bb, info, reg_ptr, scratch, offsetof(cache_t_ctxt_no_addr_sz<size>, info));
                            // val
                            vtracer_insert_trace_val(drcontext, instr, bb, opnd, reg_ptr, scratch, offsetof(cache_t_ctxt_no_addr_sz<size>, val));
                            // update buf ptr
                            vtracer_insert_trace_forward(drcontext, instr,  bb, sizeof(cache_t_ctxt_no_addr_sz<size>), buf, reg_ptr, scratch);
                        } else {
                            // cache_t_ctxt_no_info_addr_sz
                            // ctxt_hndl
                            drcctlib_get_context_handle_in_reg(drcontext, bb, instr, slot, reg_addr, reg_ptr);
                            vtracer_get_trace_buffer_in_reg(drcontext, instr, bb, buf, reg_ptr);
                            vtracer_insert_trace_val(drcontext, instr, bb, opnd_create_reg(reg_addr), reg_ptr, scratch, offsetof(cache_t_ctxt_no_info_addr_sz<size>, ctxt_hndl));
                            // val
                            vtracer_insert_trace_val(drcontext, instr, bb, opnd, reg_ptr, scratch, offsetof(cache_t_ctxt_no_info_addr_sz<size>, val));
                            // update buf ptr
                            vtracer_insert_trace_forward(drcontext, instr,  bb, sizeof(cache_t_ctxt_no_info_addr_sz<size>), buf, reg_ptr, scratch);
                        }
                    }
                } else {
                    if(!drutil_insert_get_mem_addr(drcontext, bb, instr, opnd, reg_addr/*addr*/, reg_ptr/*scratch*/)) {
                        DR_ASSERT_MSG(false, "InstrumentInsCallback drutil_insert_get_mem_addr failed!");
                    }
                    vtracer_get_trace_buffer_in_reg(drcontext, instr, bb, buf, reg_ptr);
                    if(trace->trace_info) {
                        // info
                        opnd_t info = OPND_CREATE_INTPTR((*global_client_cb.user_data_cb)(drcontext, instr, bb, opnd));
                        vtracer_insert_trace_val(drcontext, instr, bb, info, reg_ptr, scratch, offsetof(cache_t_no_sz<size>, info));
                        vtracer_insert_trace_val(drcontext, instr, bb, opnd_create_reg(reg_addr), reg_ptr, scratch, offsetof(cache_t_no_sz<size>, addr));
                        vtracer_insert_trace_val(drcontext, instr, bb, opnd, reg_ptr, scratch, offsetof(cache_t_no_sz<size>, val));
                        vtracer_insert_trace_forward(drcontext, instr,  bb, sizeof(cache_t_no_sz<size>), buf, reg_ptr, scratch);
                    } else {
                        vtracer_insert_trace_val(drcontext, instr, bb, opnd_create_reg(reg_addr), reg_ptr, scratch, offsetof(cache_t_no_info_sz<size>, addr));
                        vtracer_insert_trace_val(drcontext, instr, bb, opnd, reg_ptr, scratch, offsetof(cache_t_no_info_sz<size>, val));
                        vtracer_insert_trace_forward(drcontext, instr,  bb, sizeof(cache_t_no_info_sz<size>), buf, reg_ptr, scratch);
                    }
                }
            } else {
                vtracer_get_trace_buffer_in_reg(drcontext, instr, bb, buf, reg_ptr);
                if(trace->trace_info) {
                    // info
                    opnd_t info = OPND_CREATE_INTPTR((*global_client_cb.user_data_cb)(drcontext, instr, bb, opnd));
                    vtracer_insert_trace_val(drcontext, instr, bb, info, reg_ptr, scratch, offsetof(cache_t_no_addr_sz<size>, info));
                    vtracer_insert_trace_val(drcontext, instr, bb, opnd, reg_ptr, scratch, offsetof(cache_t_no_addr_sz<size>, val));
                    vtracer_insert_trace_forward(drcontext, instr,  bb, sizeof(cache_t_no_addr_sz<size>), buf, reg_ptr, scratch);
                } else {
                    vtracer_insert_trace_val(drcontext, instr, bb, opnd, reg_ptr, scratch, offsetof(cache_t_no_info_addr_sz<size>, val));
                    vtracer_insert_trace_forward(drcontext, instr,  bb, sizeof(cache_t_no_info_addr_sz<size>), buf, reg_ptr, scratch);
                }
            }
        }
    }
    return ;
}

void
InstrumentInsCallback(void *drcontext, instr_instrument_msg_t *instrument_msg)
{
    // quick return when there are no entries exist
    if(!any_traces_created) {
        return;
    }
    unsigned int i;
    instrlist_t *bb = instrument_msg->bb;
    instr_t *instr = instrument_msg->instr;
    int32_t slot = instrument_msg->slot;

    if (slot == 0) {
        if(global_client_cb.bb_instrument_cb)
            (*global_client_cb.bb_instrument_cb)(drcontext, bb);
    } 

    if(!instr_is_app(instr) || instr_is_ignorable(instr)) return;
    
    if(global_client_cb.ins_instrument_cb)
        (*global_client_cb.ins_instrument_cb)(drcontext, instr, bb);

    // try to quick return when there is no opnd valid
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
    // quick return as there is no opnd valid
    if(!is_valid) return;
    
    reg_id_t reg_addr, reg_ptr;
    reg_id_t scratch;
    drvector_t allowed;
    getUnusedRegEntryInstr(&allowed, instr);

    RESERVE_REG(drcontext, bb, instr, &allowed, scratch);
    RESERVE_REG(drcontext, bb, instr, &allowed, reg_addr);
    RESERVE_REG(drcontext, bb, instr, &allowed, reg_ptr);
    drvector_delete(&allowed);
    // for each trace registered by user, we need to call func in vtrace to trace value.
    for (i = 0; i < vtrace_t_list.entries; ++i) {
        vtrace_t *trace = (vtrace_t*)drvector_get_entry(&vtrace_t_list, i);
        for(int j = 0; j < num; j++) {
            opnd_t opnd = instr_get_src(instr, j);
            int size = opnd_size_in_bytes(opnd_get_size(opnd));
            if(size == 0) {
                continue;
            }
            switch(size) {
                case 1: insertStoreTraceBuffer<1>(drcontext, bb, instr, slot, opnd, trace, reg_addr, reg_ptr, scratch);break;
                case 2: insertStoreTraceBuffer<2>(drcontext, bb, instr, slot, opnd, trace, reg_addr, reg_ptr, scratch);break;
                case 4: insertStoreTraceBuffer<4>(drcontext, bb, instr, slot, opnd, trace, reg_addr, reg_ptr, scratch);break;
                case 8: insertStoreTraceBuffer<8>(drcontext, bb, instr, slot, opnd, trace, reg_addr, reg_ptr, scratch);break;
                case 16: insertStoreTraceBuffer<16>(drcontext, bb, instr, slot, opnd, trace, reg_addr, reg_ptr, scratch);break;
                case 32: insertStoreTraceBuffer<32>(drcontext, bb, instr, slot, opnd, trace, reg_addr, reg_ptr, scratch);break;
                case 64: insertStoreTraceBuffer<64>(drcontext, bb, instr, slot, opnd, trace, reg_addr, reg_ptr, scratch);break;
                case 128: insertStoreTraceBuffer<128>(drcontext, bb, instr, slot, opnd, trace, reg_addr, reg_ptr, scratch);break;
                case 256: insertStoreTraceBuffer<256>(drcontext, bb, instr, slot, opnd, trace, reg_addr, reg_ptr, scratch);break;
                case 512: insertStoreTraceBuffer<512>(drcontext, bb, instr, slot, opnd, trace, reg_addr, reg_ptr, scratch);break;
                default: {
                    // DR_ASSERT_MSG(false, "InstrumentInsCallback Unknown size!");
#ifdef VPROFILE_DEBUG
                    debug_unknown_case(drcontext, instr, opnd);
#endif
                    continue;
                }
            }
            if(trace->trace_reg_in_memref && opnd_is_memory_reference(opnd)) {
                int num = opnd_num_regs_used(opnd);
                for(int k = 0; k < num; k++) {
                    opnd_t reg_used = opnd_create_reg(opnd_get_reg_used(opnd, k));
                    int reg_size = opnd_size_in_bytes(opnd_get_size(reg_used));
                    if(reg_size == 0) {
                        continue;
                    }
                    switch(reg_size) {
                        case 1: insertStoreTraceBuffer<1>(drcontext, bb, instr, slot, reg_used, trace, reg_addr, reg_ptr, scratch);break;
                        case 2: insertStoreTraceBuffer<2>(drcontext, bb, instr, slot, reg_used, trace, reg_addr, reg_ptr, scratch);break;
                        case 4: insertStoreTraceBuffer<4>(drcontext, bb, instr, slot, reg_used, trace, reg_addr, reg_ptr, scratch);break;
                        case 8: insertStoreTraceBuffer<8>(drcontext, bb, instr, slot, reg_used, trace, reg_addr, reg_ptr, scratch);break;
                        case 16: insertStoreTraceBuffer<16>(drcontext, bb, instr, slot, reg_used, trace, reg_addr, reg_ptr, scratch);break;
                        case 32: insertStoreTraceBuffer<32>(drcontext, bb, instr, slot, reg_used, trace, reg_addr, reg_ptr, scratch);break;
                        case 64: insertStoreTraceBuffer<64>(drcontext, bb, instr, slot, reg_used, trace, reg_addr, reg_ptr, scratch);break;
                        case 128: insertStoreTraceBuffer<128>(drcontext, bb, instr, slot, reg_used, trace, reg_addr, reg_ptr, scratch);break;
                        case 256: insertStoreTraceBuffer<256>(drcontext, bb, instr, slot, reg_used, trace, reg_addr, reg_ptr, scratch);break;
                        case 512: insertStoreTraceBuffer<512>(drcontext, bb, instr, slot, reg_used, trace, reg_addr, reg_ptr, scratch);break;
                        default: {
                            // DR_ASSERT_MSG(false, "InstrumentInsCallback Unknown size!");
#ifdef VPROFILE_DEBUG
                            debug_unknown_case(drcontext, instr, reg_used);
#endif
                            continue;
                        }
                    }
                }
            } 
        }
    }
    UNRESERVE_REG(drcontext, bb, instr, scratch);
    UNRESERVE_REG(drcontext, bb, instr, reg_addr);
    UNRESERVE_REG(drcontext, bb, instr, reg_ptr);
}

template<int size>
inline __attribute__((always_inline)) void insertStoreTraceBufferDefault(void *drcontext, instrlist_t *bb, instr_t *instr, opnd_t opnd, vtrace_t *trace, reg_id_t reg_addr, reg_id_t reg_ptr, reg_id_t scratch)
{
    bool is_float = instr_is_floating(instr);
    int esize = (is_float) ? FloatOperandSizeTable(instr, opnd) : IntegerOperandSizeTable(instr, opnd);
#ifdef VPROFILE_DEBUG
    if(esize == 0) {
        debug_unknown_case(drcontext, instr, opnd);
        // esize = size;
    }
#endif
    // int size = opnd_size_in_bytes(opnd_get_size(opnd));
    if(trace->strictly_ordered) {
        if((*(bool (*)(opnd_t))trace->buff->user_data_fill_num)(opnd)) {
            // use size&&esize&&is_float and then insert trace val
            if(trace->trace_addr) {
                if (!drutil_insert_get_mem_addr(drcontext, bb, instr, opnd, reg_addr/*addr*/,
                        reg_ptr/*scratch*/)) {
                        DR_ASSERT_MSG(false, "InstrumentInsCallback drutil_insert_get_mem_addr failed!");
                }
                vtracer_get_trace_buffer_in_reg(drcontext, instr, bb, trace->buff, reg_ptr);
                if(trace->trace_info) {
                    opnd_t info = OPND_CREATE_INTPTR((*global_client_cb.user_data_cb)(drcontext, instr, bb, opnd));
                    vtracer_insert_trace_val(drcontext, instr, bb, info, reg_ptr, scratch, offsetof(cache_t, info));
                    vtracer_insert_trace_val(drcontext, instr, bb, opnd_create_reg(reg_addr), reg_ptr, scratch, offsetof(cache_t, addr));
                    vtracer_insert_trace_val(drcontext, instr, bb, opnd, reg_ptr, scratch, offsetof(cache_t, val));
                    vtracer_insert_trace_val(drcontext, instr, bb, OPND_CREATE_INT8(size), reg_ptr, scratch, offsetof(cache_t, size));
                    vtracer_insert_trace_val(drcontext, instr, bb, OPND_CREATE_INT8(esize), reg_ptr, scratch, offsetof(cache_t, esize));
                    vtracer_insert_trace_val(drcontext, instr, bb, OPND_CREATE_INT8(is_float), reg_ptr, scratch, offsetof(cache_t, is_float));
                    vtracer_insert_trace_forward(drcontext, instr,  bb, sizeof(cache_t), trace->buff, reg_ptr, scratch);
                } else {
                    vtracer_insert_trace_val(drcontext, instr, bb, opnd_create_reg(reg_addr), reg_ptr, scratch, offsetof(cache_t_no_info, addr));
                    vtracer_insert_trace_val(drcontext, instr, bb, opnd, reg_ptr, scratch, offsetof(cache_t_no_info, val));
                    vtracer_insert_trace_val(drcontext, instr, bb, OPND_CREATE_INT8(size), reg_ptr, scratch, offsetof(cache_t_no_info, size));
                    vtracer_insert_trace_val(drcontext, instr, bb, OPND_CREATE_INT8(esize), reg_ptr, scratch, offsetof(cache_t_no_info, esize));
                    vtracer_insert_trace_val(drcontext, instr, bb, OPND_CREATE_INT8(is_float), reg_ptr, scratch, offsetof(cache_t_no_info, is_float));
                    vtracer_insert_trace_forward(drcontext, instr,  bb, sizeof(cache_t_no_info), trace->buff, reg_ptr, scratch);
                }
            } else {
                vtracer_get_trace_buffer_in_reg(drcontext, instr, bb, trace->buff, reg_ptr);
                if(trace->trace_info) {
                    opnd_t info = OPND_CREATE_INTPTR((*global_client_cb.user_data_cb)(drcontext, instr, bb, opnd));
                    vtracer_insert_trace_val(drcontext, instr, bb, info, reg_ptr, scratch, offsetof(cache_t_no_addr, info));
                    vtracer_insert_trace_val(drcontext, instr, bb, opnd, reg_ptr, scratch, offsetof(cache_t_no_addr, val));
                    vtracer_insert_trace_val(drcontext, instr, bb, OPND_CREATE_INT8(size), reg_ptr, scratch, offsetof(cache_t_no_addr, size));
                    vtracer_insert_trace_val(drcontext, instr, bb, OPND_CREATE_INT8(esize), reg_ptr, scratch, offsetof(cache_t_no_addr, esize));
                    vtracer_insert_trace_val(drcontext, instr, bb, OPND_CREATE_INT8(is_float), reg_ptr, scratch, offsetof(cache_t_no_addr, is_float));
                    vtracer_insert_trace_forward(drcontext, instr,  bb, sizeof(cache_t_no_addr), trace->buff, reg_ptr, scratch);
                } else {
                    vtracer_insert_trace_val(drcontext, instr, bb, opnd, reg_ptr, scratch, offsetof(cache_t_no_info_addr, val));
                    vtracer_insert_trace_val(drcontext, instr, bb, OPND_CREATE_INT8(size), reg_ptr, scratch, offsetof(cache_t_no_info_addr, size));
                    vtracer_insert_trace_val(drcontext, instr, bb, OPND_CREATE_INT8(esize), reg_ptr, scratch, offsetof(cache_t_no_info_addr, esize));
                    vtracer_insert_trace_val(drcontext, instr, bb, OPND_CREATE_INT8(is_float), reg_ptr, scratch, offsetof(cache_t_no_info_addr, is_float));
                    vtracer_insert_trace_forward(drcontext, instr,  bb, sizeof(cache_t_no_info_addr), trace->buff, reg_ptr, scratch);
                }
            }
        }
    } else {
        vtrace_buffer_t* buf = get_trace_buf(trace, is_float, esize, size);
        if(!buf) {
#ifdef VPROFILE_DEBUG
            debug_unknown_case(drcontext, instr, opnd);
#endif
            return;
        }
        if((*(bool (*)(opnd_t))buf->user_data_fill_num)(opnd)) {
            // use size&&esize&&is_float and then choose the right buf to insert trace val
            if(trace->trace_addr) {
                if (!drutil_insert_get_mem_addr(drcontext, bb, instr, opnd, reg_addr/*addr*/,
                        reg_ptr/*scratch*/)) {
                        DR_ASSERT_MSG(false, "InstrumentInsCallback drutil_insert_get_mem_addr failed!");
                }
                vtracer_get_trace_buffer_in_reg(drcontext, instr, bb, buf, reg_ptr);
                if(trace->trace_info) {
                    opnd_t info = OPND_CREATE_INTPTR((*global_client_cb.user_data_cb)(drcontext, instr, bb, opnd));
                    vtracer_insert_trace_val(drcontext, instr, bb, info, reg_ptr, scratch, offsetof(cache_t_no_sz<size>, info));
                    vtracer_insert_trace_val(drcontext, instr, bb, opnd_create_reg(reg_addr), reg_ptr, scratch, offsetof(cache_t_no_sz<size>, addr));
                    vtracer_insert_trace_val(drcontext, instr, bb, opnd, reg_ptr, scratch, offsetof(cache_t_no_sz<size>, val));
                    vtracer_insert_trace_forward(drcontext, instr,  bb, sizeof(cache_t_no_sz<size>), buf, reg_ptr, scratch);
                } else {
                    vtracer_insert_trace_val(drcontext, instr, bb, opnd_create_reg(reg_addr), reg_ptr, scratch, offsetof(cache_t_no_info_sz<size>, addr));
                    vtracer_insert_trace_val(drcontext, instr, bb, opnd, reg_ptr, scratch, offsetof(cache_t_no_info_sz<size>, val));
                    vtracer_insert_trace_forward(drcontext, instr,  bb, sizeof(cache_t_no_info_sz<size>), buf, reg_ptr, scratch);
                }
            } else {
                vtracer_get_trace_buffer_in_reg(drcontext, instr, bb, buf, reg_ptr);
                if(trace->trace_info) {
                    opnd_t info = OPND_CREATE_INTPTR((*global_client_cb.user_data_cb)(drcontext, instr, bb, opnd));
                    vtracer_insert_trace_val(drcontext, instr, bb, info, reg_ptr, scratch, offsetof(cache_t_no_addr_sz<size>, info));
                    vtracer_insert_trace_val(drcontext, instr, bb, opnd, reg_ptr, scratch, offsetof(cache_t_no_addr_sz<size>, val));
                    vtracer_insert_trace_forward(drcontext, instr,  bb, sizeof(cache_t_no_addr_sz<size>), buf, reg_ptr, scratch);
                } else {
                    vtracer_insert_trace_val(drcontext, instr, bb, opnd, reg_ptr, scratch, offsetof(cache_t_no_info_addr_sz<size>, val));
                    vtracer_insert_trace_forward(drcontext, instr,  bb, sizeof(cache_t_no_info_addr_sz<size>), buf, reg_ptr, scratch);
                }
            }
        }
    }
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
            if(global_client_cb.ins_instrument_cb) {
                (*global_client_cb.ins_instrument_cb)(drcontext, instr, bb);
            }

            // try to quick return when there is no opnd valid
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
            // quick return as there is no opnd valid
            if(!is_valid) continue;

            reg_id_t reg_addr, reg_ptr;
            reg_id_t scratch;
            drvector_t allowed;
            getUnusedRegEntryInstr(&allowed, instr);

            RESERVE_REG(drcontext, bb, instr, &allowed, scratch);
            RESERVE_REG(drcontext, bb, instr, &allowed, reg_addr);
            RESERVE_REG(drcontext, bb, instr, &allowed, reg_ptr);
            drvector_delete(&allowed);
            unsigned int i;
            for (i = 0; i < vtrace_t_list.entries; ++i) {
                vtrace_t *trace = (vtrace_t*)drvector_get_entry(&vtrace_t_list, i);
                for(int j = 0; j < num; j++) {
                    opnd_t opnd = instr_get_src(instr, j);
                    int size = opnd_size_in_bytes(opnd_get_size(opnd));
                    if(size == 0) {
                        continue;
                    }
                    switch(size) {
                        case 1: insertStoreTraceBufferDefault<1>(drcontext, bb, instr, opnd, trace, reg_addr, reg_ptr, scratch);break;
                        case 2: insertStoreTraceBufferDefault<2>(drcontext, bb, instr, opnd, trace, reg_addr, reg_ptr, scratch);break;
                        case 4: insertStoreTraceBufferDefault<4>(drcontext, bb, instr, opnd, trace, reg_addr, reg_ptr, scratch);break;
                        case 8: insertStoreTraceBufferDefault<8>(drcontext, bb, instr, opnd, trace, reg_addr, reg_ptr, scratch);break;
                        case 16: insertStoreTraceBufferDefault<16>(drcontext, bb, instr, opnd, trace, reg_addr, reg_ptr, scratch);break;
                        case 32: insertStoreTraceBufferDefault<32>(drcontext, bb, instr, opnd, trace, reg_addr, reg_ptr, scratch);break;
                        case 64: insertStoreTraceBufferDefault<64>(drcontext, bb, instr, opnd, trace, reg_addr, reg_ptr, scratch);break;
                        case 128: insertStoreTraceBufferDefault<128>(drcontext, bb, instr, opnd, trace, reg_addr, reg_ptr, scratch);break;
                        case 256: insertStoreTraceBufferDefault<256>(drcontext, bb, instr, opnd, trace, reg_addr, reg_ptr, scratch);break;
                        case 512: insertStoreTraceBufferDefault<512>(drcontext, bb, instr, opnd, trace, reg_addr, reg_ptr, scratch);break;
                        default: {
                            // DR_ASSERT_MSG(false, "InstrumentInsCallback Unknown size!");
#ifdef VPROFILE_DEBUG
                            debug_unknown_case(drcontext, instr, opnd);
#endif
                            continue;
                        }
                    }
                    if(trace->trace_reg_in_memref && opnd_is_memory_reference(opnd)) {
                        int num = opnd_num_regs_used(opnd);
                        for(int k = 0; k < num; k++) {
                            opnd_t reg_used = opnd_create_reg(opnd_get_reg_used(opnd, k));
                            int reg_size = opnd_size_in_bytes(opnd_get_size(reg_used));
                            if(reg_size == 0) {
                                continue;
                            }
                            switch(reg_size) {
                                case 1: insertStoreTraceBufferDefault<1>(drcontext, bb, instr, reg_used, trace, reg_addr, reg_ptr, scratch);break;
                                case 2: insertStoreTraceBufferDefault<2>(drcontext, bb, instr, reg_used, trace, reg_addr, reg_ptr, scratch);break;
                                case 4: insertStoreTraceBufferDefault<4>(drcontext, bb, instr, reg_used, trace, reg_addr, reg_ptr, scratch);break;
                                case 8: insertStoreTraceBufferDefault<8>(drcontext, bb, instr, reg_used, trace, reg_addr, reg_ptr, scratch);break;
                                case 16: insertStoreTraceBufferDefault<16>(drcontext, bb, instr, reg_used, trace, reg_addr, reg_ptr, scratch);break;
                                case 32: insertStoreTraceBufferDefault<32>(drcontext, bb, instr, reg_used, trace, reg_addr, reg_ptr, scratch);break;
                                case 64: insertStoreTraceBufferDefault<64>(drcontext, bb, instr, reg_used, trace, reg_addr, reg_ptr, scratch);break;
                                case 128: insertStoreTraceBufferDefault<128>(drcontext, bb, instr, reg_used, trace, reg_addr, reg_ptr, scratch);break;
                                case 256: insertStoreTraceBufferDefault<256>(drcontext, bb, instr, reg_used, trace, reg_addr, reg_ptr, scratch);break;
                                case 512: insertStoreTraceBufferDefault<512>(drcontext, bb, instr, reg_used, trace, reg_addr, reg_ptr, scratch);break;
                                default: {
                                    // DR_ASSERT_MSG(false, "InstrumentInsCallback Unknown size!");
#ifdef VPROFILE_DEBUG
                                    debug_unknown_case(drcontext, instr, reg_used);
#endif
                                    continue;
                                }
                            }
                        }
                    }  
                }
            }
            UNRESERVE_REG(drcontext, bb, instr, scratch);
            UNRESERVE_REG(drcontext, bb, instr, reg_addr);
            UNRESERVE_REG(drcontext, bb, instr, reg_ptr);
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
        drcctlib_init_ex(filter, INVALID_FILE, InstrumentInsCallback, NULL, NULL, DRCCTLIB_CACHE_MODE);
    }
    return true;
}

void vprofile_exit()
{
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

vtrace_t* vprofile_allocate_trace(bool trace_addr, bool trace_cct,
                                  bool trace_info, bool strictly_ordered,
                                  bool trace_reg_in_memref)
{
    vtrace_t *new_trace;
    new_trace = (vtrace_t*)dr_global_alloc(sizeof(*new_trace));
    new_trace->trace_addr = trace_addr;
    new_trace->trace_cct = trace_cct;
    new_trace->trace_info = trace_info;
    new_trace->strictly_ordered = strictly_ordered;
    new_trace->trace_reg_in_memref = trace_reg_in_memref;
    if(!strictly_ordered) {
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

void vprofile_update_cb_ctxt(void *buf_base, void *buf_end, void* user_data)
{
    cache_t_ctxt* cache_ptr = (cache_t_ctxt*)buf_base;
    cache_t_ctxt* cache_end = (cache_t_ctxt*)buf_end;
    val_info_t user_info;
    for(; cache_ptr<cache_end; ++cache_ptr) {
        // extract data from cache
        user_info.addr = cache_ptr->addr;
        user_info.ctxt_hndl = cache_ptr->ctxt_hndl;
        user_info.val = (void *) cache_ptr->val;
        user_info.info = cache_ptr->info;
        user_info.size = cache_ptr->size;
        user_info.esize = cache_ptr->esize;
        user_info.is_float = cache_ptr->is_float;
        (*((void (*)(val_info_t *)) user_data))(&user_info);
    }
}

void vprofile_update_cb_ctxt_no_info(void *buf_base, void *buf_end, void* user_data)
{
    cache_t_ctxt_no_info* cache_ptr = (cache_t_ctxt_no_info*)buf_base;
    cache_t_ctxt_no_info* cache_end = (cache_t_ctxt_no_info*)buf_end;
    val_info_t user_info;
    for(; cache_ptr<cache_end; ++cache_ptr) {
        // extract data from cache
        user_info.addr = cache_ptr->addr;
        user_info.ctxt_hndl = cache_ptr->ctxt_hndl;
        user_info.val = (void *) cache_ptr->val;
        user_info.size = cache_ptr->size;
        user_info.esize = cache_ptr->esize;
        user_info.is_float = cache_ptr->is_float;
        (*((void (*)(val_info_t *)) user_data))(&user_info);
    }
}

void vprofile_update_cb_ctxt_no_info_addr(void *buf_base, void *buf_end, void* user_data)
{
    cache_t_ctxt_no_info_addr* cache_ptr = (cache_t_ctxt_no_info_addr*)buf_base;
    cache_t_ctxt_no_info_addr* cache_end = (cache_t_ctxt_no_info_addr*)buf_end;
    val_info_t user_info;
    for(; cache_ptr<cache_end; ++cache_ptr) {
        // extract data from cache
        user_info.ctxt_hndl = cache_ptr->ctxt_hndl;
        user_info.val = (void *) cache_ptr->val;
        user_info.size = cache_ptr->size;
        user_info.esize = cache_ptr->esize;
        user_info.is_float = cache_ptr->is_float;
        (*((void (*)(val_info_t *)) user_data))(&user_info);
    }
}

void vprofile_update_cb_ctxt_no_addr(void *buf_base, void *buf_end, void* user_data)
{
    cache_t_ctxt_no_addr* cache_ptr = (cache_t_ctxt_no_addr*)buf_base;
    cache_t_ctxt_no_addr* cache_end = (cache_t_ctxt_no_addr*)buf_end;
    val_info_t user_info;
    for(; cache_ptr<cache_end; ++cache_ptr) {
        // extract data from cache
        user_info.ctxt_hndl = cache_ptr->ctxt_hndl;
        user_info.val = (void *) cache_ptr->val;
        user_info.info = cache_ptr->info;
        user_info.size = cache_ptr->size;
        user_info.esize = cache_ptr->esize;
        user_info.is_float = cache_ptr->is_float;
        (*((void (*)(val_info_t *)) user_data))(&user_info);
    }
}

template<int sz>
void vprofile_update_cb_ctxt_no_sz(void *buf_base, void *buf_end, void* user_data)
{
    cache_t_ctxt_no_sz<sz>* cache_ptr = (cache_t_ctxt_no_sz<sz>*)buf_base;
    cache_t_ctxt_no_sz<sz>* cache_end = (cache_t_ctxt_no_sz<sz>*)buf_end;
    val_info_t user_info;
    for(; cache_ptr<cache_end; ++cache_ptr) {
        // extract data from cache
        user_info.addr = cache_ptr->addr;
        user_info.ctxt_hndl = cache_ptr->ctxt_hndl;
        user_info.val = (void *) cache_ptr->val;
        user_info.info = cache_ptr->info;
        (*((void (*)(val_info_t *)) user_data))(&user_info);
    }
}

template<int sz>
void vprofile_update_cb_ctxt_no_info_sz(void *buf_base, void *buf_end, void* user_data)
{
    cache_t_ctxt_no_info_sz<sz>* cache_ptr = (cache_t_ctxt_no_info_sz<sz>*)buf_base;
    cache_t_ctxt_no_info_sz<sz>* cache_end = (cache_t_ctxt_no_info_sz<sz>*)buf_end;
    val_info_t user_info;
    for(; cache_ptr<cache_end; ++cache_ptr) {
        // extract data from cache
        user_info.addr = cache_ptr->addr;
        user_info.ctxt_hndl = cache_ptr->ctxt_hndl;
        user_info.val = (void *) cache_ptr->val;
        (*((void (*)(val_info_t *)) user_data))(&user_info);
    }
}

template<int sz>
void vprofile_update_cb_ctxt_no_info_addr_sz(void *buf_base, void *buf_end, void* user_data)
{
    cache_t_ctxt_no_info_addr_sz<sz>* cache_ptr = (cache_t_ctxt_no_info_addr_sz<sz>*)buf_base;
    cache_t_ctxt_no_info_addr_sz<sz>* cache_end = (cache_t_ctxt_no_info_addr_sz<sz>*)buf_end;
    val_info_t user_info;
    for(; cache_ptr<cache_end; ++cache_ptr) {
        // extract data from cache
        user_info.ctxt_hndl = cache_ptr->ctxt_hndl;
        user_info.val = (void *) cache_ptr->val;
        (*((void (*)(val_info_t *)) user_data))(&user_info);
    }
}

template<int sz>
void vprofile_update_cb_ctxt_no_addr_sz(void *buf_base, void *buf_end, void* user_data)
{
    cache_t_ctxt_no_addr_sz<sz>* cache_ptr = (cache_t_ctxt_no_addr_sz<sz>*)buf_base;
    cache_t_ctxt_no_addr_sz<sz>* cache_end = (cache_t_ctxt_no_addr_sz<sz>*)buf_end;
    val_info_t user_info;
    for(; cache_ptr<cache_end; ++cache_ptr) {
        // extract data from cache
        user_info.ctxt_hndl = cache_ptr->ctxt_hndl;
        user_info.val = (void *) cache_ptr->val;
        user_info.info = cache_ptr->info;
        (*((void (*)(val_info_t *)) user_data))(&user_info);
    }
}

void vprofile_update_cb(void *buf_base, void *buf_end, void* user_data)
{
    cache_t* cache_ptr = (cache_t*)buf_base;
    cache_t* cache_end = (cache_t*)buf_end;
    val_info_t user_info;
    for(; cache_ptr<cache_end; ++cache_ptr) {
        // extract data from cache
        user_info.addr = cache_ptr->addr;
        user_info.val = (void *) cache_ptr->val;
        user_info.info = cache_ptr->info;
        user_info.size = cache_ptr->size;
        user_info.esize = cache_ptr->esize;
        user_info.is_float = cache_ptr->is_float;
        (*((void (*)(val_info_t *)) user_data))(&user_info);
    }
}

void vprofile_update_cb_no_info(void *buf_base, void *buf_end, void* user_data)
{
    cache_t_no_info* cache_ptr = (cache_t_no_info*)buf_base;
    cache_t_no_info* cache_end = (cache_t_no_info*)buf_end;
    val_info_t user_info;
    for(; cache_ptr<cache_end; ++cache_ptr) {
        // extract data from cache
        user_info.addr = cache_ptr->addr;
        user_info.val = (void *) cache_ptr->val;
        user_info.size = cache_ptr->size;
        user_info.esize = cache_ptr->esize;
        user_info.is_float = cache_ptr->is_float;
        (*((void (*)(val_info_t *)) user_data))(&user_info);
    }
}

void vprofile_update_cb_no_info_addr(void *buf_base, void *buf_end, void* user_data)
{
    cache_t_no_info_addr* cache_ptr = (cache_t_no_info_addr*)buf_base;
    cache_t_no_info_addr* cache_end = (cache_t_no_info_addr*)buf_end;
    val_info_t user_info;
    for(; cache_ptr<cache_end; ++cache_ptr) {
        // extract data from cache
        user_info.val = (void *) cache_ptr->val;
        user_info.size = cache_ptr->size;
        user_info.esize = cache_ptr->esize;
        user_info.is_float = cache_ptr->is_float;
        (*((void (*)(val_info_t *)) user_data))(&user_info);
    }
}

void vprofile_update_cb_no_addr(void *buf_base, void *buf_end, void* user_data)
{
    cache_t_no_addr* cache_ptr = (cache_t_no_addr*)buf_base;
    cache_t_no_addr* cache_end = (cache_t_no_addr*)buf_end;
    val_info_t user_info;
    for(; cache_ptr<cache_end; ++cache_ptr) {
        // extract data from cache
        user_info.val = (void *) cache_ptr->val;
        user_info.info = cache_ptr->info;
        user_info.size = cache_ptr->size;
        user_info.esize = cache_ptr->esize;
        user_info.is_float = cache_ptr->is_float;
        (*((void (*)(val_info_t *)) user_data))(&user_info);
    }
}

template<int sz>
void vprofile_update_cb_no_sz(void *buf_base, void *buf_end, void* user_data)
{
    cache_t_no_sz<sz>* cache_ptr = (cache_t_no_sz<sz>*)buf_base;
    cache_t_no_sz<sz>* cache_end = (cache_t_no_sz<sz>*)buf_end;
    val_info_t user_info;
    for(; cache_ptr<cache_end; ++cache_ptr) {
        // extract data from cache
        user_info.addr = cache_ptr->addr;
        user_info.val = (void *) cache_ptr->val;
        user_info.info = cache_ptr->info;
        (*((void (*)(val_info_t *)) user_data))(&user_info);
    }
}

template<int sz>
void vprofile_update_cb_no_info_sz(void *buf_base, void *buf_end, void* user_data)
{
    cache_t_no_info_sz<sz>* cache_ptr = (cache_t_no_info_sz<sz>*)buf_base;
    cache_t_no_info_sz<sz>* cache_end = (cache_t_no_info_sz<sz>*)buf_end;
    val_info_t user_info;
    for(; cache_ptr<cache_end; ++cache_ptr) {
        // extract data from cache
        user_info.addr = cache_ptr->addr;
        user_info.val = (void *) cache_ptr->val;
        (*((void (*)(val_info_t *)) user_data))(&user_info);
    }
}

template<int sz>
void vprofile_update_cb_no_info_addr_sz(void *buf_base, void *buf_end, void* user_data)
{
    cache_t_no_info_addr_sz<sz>* cache_ptr = (cache_t_no_info_addr_sz<sz>*)buf_base;
    cache_t_no_info_addr_sz<sz>* cache_end = (cache_t_no_info_addr_sz<sz>*)buf_end;
    val_info_t user_info;
    for(; cache_ptr<cache_end; ++cache_ptr) {
        // extract data from cache
        user_info.val = (void *) cache_ptr->val;
        (*((void (*)(val_info_t *)) user_data))(&user_info);
    }
}

template<int sz>
void vprofile_update_cb_no_addr_sz(void *buf_base, void *buf_end, void* user_data)
{
    cache_t_no_addr_sz<sz>* cache_ptr = (cache_t_no_addr_sz<sz>*)buf_base;
    cache_t_no_addr_sz<sz>* cache_end = (cache_t_no_addr_sz<sz>*)buf_end;
    val_info_t user_info;
    for(; cache_ptr<cache_end; ++cache_ptr) {
        // extract data from cache
        user_info.val = (void *) cache_ptr->val;
        user_info.info = cache_ptr->info;
        (*((void (*)(val_info_t *)) user_data))(&user_info);
    }
}

size_t vprofile_fill_num_cb_ctxt(void *drcontext, instr_t *where, void* user_data)
{
    size_t fill_num = 0;
    if(!global_instr_filter(where)) return fill_num;
    int num = instr_num_srcs(where);
    for(int j = 0; j < num; j++) {
        opnd_t opnd = instr_get_src(where, j);
        if((*((bool (*)(opnd_t)) user_data))(opnd)) {
            fill_num += sizeof(cache_t_ctxt);
        }
    }

    return fill_num;
}

size_t vprofile_fill_num_cb_ctxt_no_info(void *drcontext, instr_t *where, void* user_data)
{
    size_t fill_num = 0;
    if(!global_instr_filter(where)) return fill_num;
    int num = instr_num_srcs(where);
    for(int j = 0; j < num; j++) {
        opnd_t opnd = instr_get_src(where, j);
        if((*((bool (*)(opnd_t)) user_data))(opnd)) {
            fill_num += sizeof(cache_t_ctxt_no_info);
        }
    }

    return fill_num;
}

size_t vprofile_fill_num_cb_ctxt_no_addr(void *drcontext, instr_t *where, void* user_data)
{
    size_t fill_num = 0;
    if(!global_instr_filter(where)) return fill_num;
    int num = instr_num_srcs(where);
    for(int j = 0; j < num; j++) {
        opnd_t opnd = instr_get_src(where, j);
        if((*((bool (*)(opnd_t)) user_data))(opnd)) {
            fill_num += sizeof(cache_t_ctxt_no_addr);
        }
    }

    return fill_num;
}

size_t vprofile_fill_num_cb_ctxt_no_info_addr(void *drcontext, instr_t *where, void* user_data)
{
    size_t fill_num = 0;
    if(!global_instr_filter(where)) return fill_num;
    int num = instr_num_srcs(where);
    for(int j = 0; j < num; j++) {
        opnd_t opnd = instr_get_src(where, j);
        if((*((bool (*)(opnd_t)) user_data))(opnd)) {
            fill_num += sizeof(cache_t_ctxt_no_info_addr);
        }
    }

    return fill_num;
}

template<int sz, int esize, bool is_float>
size_t vprofile_fill_num_cb_ctxt_no_sz(void *drcontext, instr_t *where, void* user_data)
{
    size_t fill_num = 0;
    int num = instr_num_srcs(where);
    bool is_float_actual = instr_is_floating(where);
    if(!global_instr_filter(where) || is_float != is_float_actual) return fill_num;
    for(int j = 0; j < num; j++) {
        opnd_t opnd = instr_get_src(where, j);
        int esize_actual = (is_float) ? FloatOperandSizeTable(where, opnd) : IntegerOperandSizeTable(where, opnd);
        if((*((bool (*)(opnd_t)) user_data))(opnd) && esize == esize_actual && opnd_size_in_bytes(opnd_get_size(opnd)) == sz) {
            fill_num += sizeof(cache_t_ctxt_no_sz<sz>);
        }
    }

    return fill_num;
}

template<int sz, int esize, bool is_float>
size_t vprofile_fill_num_cb_ctxt_no_info_sz(void *drcontext, instr_t *where, void* user_data)
{
    size_t fill_num = 0;
    int num = instr_num_srcs(where);
    bool is_float_actual = instr_is_floating(where);
    if(!global_instr_filter(where) || is_float != is_float_actual) return fill_num;
    for(int j = 0; j < num; j++) {
        opnd_t opnd = instr_get_src(where, j);
        int esize_actual = (is_float) ? FloatOperandSizeTable(where, opnd) : IntegerOperandSizeTable(where, opnd);
        if((*((bool (*)(opnd_t)) user_data))(opnd) && esize == esize_actual && opnd_size_in_bytes(opnd_get_size(opnd)) == sz) {
            fill_num += sizeof(cache_t_ctxt_no_info_sz<sz>);
        }
    }

    return fill_num;
}

template<int sz, int esize, bool is_float>
size_t vprofile_fill_num_cb_ctxt_no_addr_sz(void *drcontext, instr_t *where, void* user_data)
{
    size_t fill_num = 0;
    int num = instr_num_srcs(where);
    bool is_float_actual = instr_is_floating(where);
    if(!global_instr_filter(where) || is_float != is_float_actual) return fill_num;
    for(int j = 0; j < num; j++) {
        opnd_t opnd = instr_get_src(where, j);
        int esize_actual = (is_float) ? FloatOperandSizeTable(where, opnd) : IntegerOperandSizeTable(where, opnd);
        if((*((bool (*)(opnd_t)) user_data))(opnd) && esize == esize_actual && opnd_size_in_bytes(opnd_get_size(opnd)) == sz) {
            fill_num += sizeof(cache_t_ctxt_no_addr_sz<sz>);
        }
    }

    return fill_num;
}

template<int sz, int esize, bool is_float>
size_t vprofile_fill_num_cb_ctxt_no_info_addr_sz(void *drcontext, instr_t *where, void* user_data)
{
    size_t fill_num = 0;
    int num = instr_num_srcs(where);
    // instr is floating == opnd is floating?
    bool is_float_actual = instr_is_floating(where);
    if(!global_instr_filter(where) || is_float != is_float_actual) return fill_num;
    for(int j = 0; j < num; j++) {
        opnd_t opnd = instr_get_src(where, j);
        int esize_actual = (is_float) ? FloatOperandSizeTable(where, opnd) : IntegerOperandSizeTable(where, opnd);
        if((*((bool (*)(opnd_t)) user_data))(opnd) && esize == esize_actual && opnd_size_in_bytes(opnd_get_size(opnd)) == sz) {
            fill_num += sizeof(cache_t_ctxt_no_info_addr_sz<sz>);
        }
    }

    return fill_num;
}

size_t vprofile_fill_num_cb(void *drcontext, instr_t *where, void* user_data)
{
    size_t fill_num = 0;
    if(!global_instr_filter(where)) return fill_num;
    int num = instr_num_srcs(where);
    for(int j = 0; j < num; j++) {
        opnd_t opnd = instr_get_src(where, j);
        if((*((bool (*)(opnd_t)) user_data))(opnd)) {
            fill_num += sizeof(cache_t);
        }
    }

    return fill_num;
}

size_t vprofile_fill_num_cb_no_info(void *drcontext, instr_t *where, void* user_data)
{
    size_t fill_num = 0;
    if(!global_instr_filter(where)) return fill_num;
    int num = instr_num_srcs(where);
    for(int j = 0; j < num; j++) {
        opnd_t opnd = instr_get_src(where, j);
        if((*((bool (*)(opnd_t)) user_data))(opnd)) {
            fill_num += sizeof(cache_t_no_info);
        }
    }

    return fill_num;
}

size_t vprofile_fill_num_cb_no_addr(void *drcontext, instr_t *where, void* user_data)
{
    size_t fill_num = 0;
    if(!global_instr_filter(where)) return fill_num;
    int num = instr_num_srcs(where);
    for(int j = 0; j < num; j++) {
        opnd_t opnd = instr_get_src(where, j);
        if((*((bool (*)(opnd_t)) user_data))(opnd)) {
            fill_num += sizeof(cache_t_no_addr);
        }
    }

    return fill_num;
}

size_t vprofile_fill_num_cb_no_info_addr(void *drcontext, instr_t *where, void* user_data)
{
    size_t fill_num = 0;
    if(!global_instr_filter(where)) return fill_num;
    int num = instr_num_srcs(where);
    for(int j = 0; j < num; j++) {
        opnd_t opnd = instr_get_src(where, j);
        if((*((bool (*)(opnd_t)) user_data))(opnd)) {
            fill_num += sizeof(cache_t_no_info_addr);
        }
    }

    return fill_num;
}

template<int sz, int esize, bool is_float>
size_t vprofile_fill_num_cb_no_sz(void *drcontext, instr_t *where, void* user_data)
{
    size_t fill_num = 0;
    int num = instr_num_srcs(where);
    bool is_float_actual = instr_is_floating(where);
    if(!global_instr_filter(where) || is_float != is_float_actual) return fill_num;
    for(int j = 0; j < num; j++) {
        opnd_t opnd = instr_get_src(where, j);
        int esize_actual = (is_float) ? FloatOperandSizeTable(where, opnd) : IntegerOperandSizeTable(where, opnd);
        if((*((bool (*)(opnd_t)) user_data))(opnd) && esize == esize_actual && opnd_size_in_bytes(opnd_get_size(opnd)) == sz) {
            fill_num += sizeof(cache_t_no_sz<sz>);
        }
    }

    return fill_num;
}

template<int sz, int esize, bool is_float>
size_t vprofile_fill_num_cb_no_info_sz(void *drcontext, instr_t *where, void* user_data)
{
    size_t fill_num = 0;
    int num = instr_num_srcs(where);
    bool is_float_actual = instr_is_floating(where);
    if(!global_instr_filter(where) || is_float != is_float_actual) return fill_num;
    for(int j = 0; j < num; j++) {
        opnd_t opnd = instr_get_src(where, j);
        int esize_actual = (is_float) ? FloatOperandSizeTable(where, opnd) : IntegerOperandSizeTable(where, opnd);
        if((*((bool (*)(opnd_t)) user_data))(opnd) && esize == esize_actual && opnd_size_in_bytes(opnd_get_size(opnd)) == sz) {
            fill_num += sizeof(cache_t_no_info_sz<sz>);
        }
    }

    return fill_num;
}

template<int sz, int esize, bool is_float>
size_t vprofile_fill_num_cb_no_addr_sz(void *drcontext, instr_t *where, void* user_data)
{
    size_t fill_num = 0;
    int num = instr_num_srcs(where);
    bool is_float_actual = instr_is_floating(where);
    if(!global_instr_filter(where) || is_float != is_float_actual) return fill_num;
    for(int j = 0; j < num; j++) {
        opnd_t opnd = instr_get_src(where, j);
        int esize_actual = (is_float) ? FloatOperandSizeTable(where, opnd) : IntegerOperandSizeTable(where, opnd);
        if((*((bool (*)(opnd_t)) user_data))(opnd) && esize == esize_actual && opnd_size_in_bytes(opnd_get_size(opnd)) == sz) {
            fill_num += sizeof(cache_t_no_addr_sz<sz>);
        }
    }

    return fill_num;
}

template<int sz, int esize, bool is_float>
size_t vprofile_fill_num_cb_no_info_addr_sz(void *drcontext, instr_t *where, void* user_data)
{
    size_t fill_num = 0;
    int num = instr_num_srcs(where);
    // instr is floating == opnd is floating?
    bool is_float_actual = instr_is_floating(where);
    if(!global_instr_filter(where) || is_float != is_float_actual) return fill_num;
    for(int j = 0; j < num; j++) {
        opnd_t opnd = instr_get_src(where, j);
        int esize_actual = (is_float) ? FloatOperandSizeTable(where, opnd) : IntegerOperandSizeTable(where, opnd);
        if((*((bool (*)(opnd_t)) user_data))(opnd) && esize == esize_actual && opnd_size_in_bytes(opnd_get_size(opnd)) == sz) {
            fill_num += sizeof(cache_t_no_info_addr_sz<sz>);
        }
    }

    return fill_num;
}

template<int sz, int esize, bool is_float>
void vprofile_register_trace_cb_impl(vtrace_buffer_t* buff, 
                                     bool (*filter)(opnd_t), 
                                     void (*update_cb)(val_info_t *),
                                     bool trace_addr, bool trace_cct,
                                     bool trace_info, 
                                     bool strictly_ordered)
{
    buff->user_data_full = (void *)update_cb;
    buff->user_data_fill_num = (void *)filter;

    if(strictly_ordered) {
        if(trace_addr || trace_cct) {
            if(trace_cct) {
                if(trace_addr) {
                    if(trace_info) {
                        buff->full_cb = vprofile_update_cb_ctxt;
                        buff->fill_num_cb = vprofile_fill_num_cb_ctxt;
                    } else {
                        buff->full_cb = vprofile_update_cb_ctxt_no_addr;
                        buff->fill_num_cb = vprofile_fill_num_cb_ctxt_no_addr;
                    }
                } else {
                    if(trace_info) {
                        buff->full_cb = vprofile_update_cb_ctxt_no_addr;
                        buff->fill_num_cb = vprofile_fill_num_cb_ctxt_no_addr;
                    } else {
                        buff->full_cb = vprofile_update_cb_ctxt_no_info_addr;
                        buff->fill_num_cb = vprofile_fill_num_cb_ctxt_no_info_addr;
                    }
                }
            } else {
                if(trace_info) {
                    buff->full_cb = vprofile_update_cb;
                    buff->fill_num_cb = vprofile_fill_num_cb;
                } else {
                    buff->full_cb = vprofile_update_cb_no_info;
                    buff->fill_num_cb = vprofile_fill_num_cb_no_info;
                }
            }
        } else {
            if(trace_info) {
                buff->full_cb = vprofile_update_cb_no_addr;
                buff->fill_num_cb = vprofile_fill_num_cb_no_addr;
            } else {
                buff->full_cb = vprofile_update_cb_no_info_addr;
                buff->fill_num_cb = vprofile_fill_num_cb_no_info_addr;
            }
        }
    } else {
        if(trace_addr || trace_cct) {
            if(trace_cct) {
                if(trace_addr) {
                    if(trace_info) {
                        buff->full_cb = vprofile_update_cb_ctxt_no_sz<sz>;
                        buff->fill_num_cb = vprofile_fill_num_cb_ctxt_no_sz<sz, esize, is_float>;
                    } else {
                        buff->full_cb = vprofile_update_cb_ctxt_no_info_sz<sz>;
                        buff->fill_num_cb = vprofile_fill_num_cb_ctxt_no_info_sz<sz, esize, is_float>;
                    }
                } else {
                    if(trace_info) {
                        buff->full_cb = vprofile_update_cb_ctxt_no_addr_sz<sz>;
                        buff->fill_num_cb = vprofile_fill_num_cb_ctxt_no_addr_sz<sz, esize, is_float>;
                    } else {
                        buff->full_cb = vprofile_update_cb_ctxt_no_info_addr_sz<sz>;
                        buff->fill_num_cb = vprofile_fill_num_cb_ctxt_no_info_addr_sz<sz, esize, is_float>;
                    }
                }
            } else {
                if(trace_info) {
                    buff->full_cb = vprofile_update_cb_no_sz<sz>;
                    buff->fill_num_cb = vprofile_fill_num_cb_no_sz<sz, esize, is_float>;
                } else {
                    buff->full_cb = vprofile_update_cb_no_info_sz<sz>;
                    buff->fill_num_cb = vprofile_fill_num_cb_no_info_sz<sz, esize, is_float>;
                }
            }
        } else {
            if(trace_info) {
                buff->full_cb = vprofile_update_cb_no_addr_sz<sz>;
                buff->fill_num_cb = vprofile_fill_num_cb_no_addr_sz<sz, esize, is_float>;
            } else {
                buff->full_cb = vprofile_update_cb_no_info_addr_sz<sz>;
                buff->fill_num_cb = vprofile_fill_num_cb_no_info_addr_sz<sz, esize, is_float>;
            }
        }
    }
}

void vprofile_register_trace_cb_for_data_type(int i, vtrace_buffer_t* buff, 
                                              bool (*filter)(opnd_t), 
                                              void (*update_cb)(val_info_t *),
                                              bool trace_addr, bool trace_cct,
                                              bool trace_info) {
    switch(i) {
        case INT8:
            vprofile_register_trace_cb_impl<1,1,false>(buff, filter, update_cb, trace_addr, trace_cct, trace_info, false);
            break;
        case INT16:
            vprofile_register_trace_cb_impl<2,2,false>(buff, filter, update_cb, trace_addr, trace_cct, trace_info, false);
            break;
        case INT32:
            vprofile_register_trace_cb_impl<4,4,false>(buff, filter, update_cb, trace_addr, trace_cct, trace_info, false);
            break;
        case SPx1:
            vprofile_register_trace_cb_impl<4,4,true>(buff, filter, update_cb, trace_addr, trace_cct, trace_info, false);
            break;
        case INT64:
            vprofile_register_trace_cb_impl<8,8,false>(buff, filter, update_cb, trace_addr, trace_cct, trace_info, false);
            break;
        case DPx1:
            vprofile_register_trace_cb_impl<8,8,true>(buff, filter, update_cb, trace_addr, trace_cct, trace_info, false);
            break;
        case INT128:
            vprofile_register_trace_cb_impl<16,16,false>(buff, filter, update_cb, trace_addr, trace_cct, trace_info, false);
            break;
        case INT8x16:
            vprofile_register_trace_cb_impl<16,1,false>(buff, filter, update_cb, trace_addr, trace_cct, trace_info, false);
            break;
        case SPx4:
            vprofile_register_trace_cb_impl<16,4,true>(buff, filter, update_cb, trace_addr, trace_cct, trace_info, false);
            break;
        case DPx2:
            vprofile_register_trace_cb_impl<16,8,true>(buff, filter, update_cb, trace_addr, trace_cct, trace_info, false);
            break;
        case INT256:
            vprofile_register_trace_cb_impl<32,32,false>(buff, filter, update_cb, trace_addr, trace_cct, trace_info, false);
            break;
        case INT8x32:
            vprofile_register_trace_cb_impl<32,1,false>(buff, filter, update_cb, trace_addr, trace_cct, trace_info, false);
            break;
        case SPx8:
            vprofile_register_trace_cb_impl<32,4,true>(buff, filter, update_cb, trace_addr, trace_cct, trace_info, false);
            break;
        case DPx4:
            vprofile_register_trace_cb_impl<32,8,true>(buff, filter, update_cb, trace_addr, trace_cct, trace_info, false);
            break;
        case INT512:
            vprofile_register_trace_cb_impl<64,64,false>(buff, filter, update_cb, trace_addr, trace_cct, trace_info, false);
            break;
        case INT8x64:
            vprofile_register_trace_cb_impl<64,1,false>(buff, filter, update_cb, trace_addr, trace_cct, trace_info, false);
            break;
        case SPx16:
            vprofile_register_trace_cb_impl<64,4,true>(buff, filter, update_cb, trace_addr, trace_cct, trace_info, false);
            break;
        case DPx8:
            vprofile_register_trace_cb_impl<64,8,true>(buff, filter, update_cb, trace_addr, trace_cct, trace_info, false);
            break;
        default: DR_ASSERT_MSG(false, "vprofile_register_trace_cb Unknown error!");
        
    }
}

template<int sz>
int get_buf_size_impl(bool trace_addr, bool trace_cct,
                 bool trace_info, 
                 bool strictly_ordered) {
    if(strictly_ordered) {
        if(trace_addr || trace_cct) {
            if(trace_info) {
                return (sizeof(cache_t) * MAX_NUM_MEM_REFS);
            } else {
                return (sizeof(cache_t_no_info) * MAX_NUM_MEM_REFS);
            }
        } else {
            if(trace_info) {
                return (sizeof(cache_t_no_addr) * MAX_NUM_MEM_REFS);
            } else {
                return (sizeof(cache_t_no_info_addr) * MAX_NUM_MEM_REFS);
            }
        }
    } else {
        if(trace_addr || trace_cct) {
            if(trace_info) {
                return (sizeof(cache_t_no_sz<sz>) * MAX_NUM_MEM_REFS);
            } else {
                return (sizeof(cache_t_no_info_sz<sz>) * MAX_NUM_MEM_REFS);
            }
        } else {
            if(trace_info) {
                return (sizeof(cache_t_no_addr_sz<sz>) * MAX_NUM_MEM_REFS);
            } else {
                return (sizeof(cache_t_no_info_addr_sz<sz>) * MAX_NUM_MEM_REFS);
            }
        }
    }
}

int get_buf_size(int i, bool trace_addr,
                 bool trace_cct,
                 bool trace_info)
{
    switch(i) {
        case INT8:
            return get_buf_size_impl<1>(trace_addr, trace_cct,
                                     trace_info, 
                                     false);
        case INT16:
            return get_buf_size_impl<2>(trace_addr, trace_cct,
                                     trace_info, 
                                     false);
        case INT32:
        case SPx1:
            return get_buf_size_impl<4>(trace_addr, trace_cct,
                                     trace_info, 
                                     false);
        case INT64:
        case DPx1:
            return get_buf_size_impl<8>(trace_addr, trace_cct,
                                     trace_info, 
                                     false);
        case INT128:
        case INT8x16:
        case SPx4:
        case DPx2:
            return get_buf_size_impl<16>(trace_addr, trace_cct,
                                     trace_info, 
                                     false);
        case INT256:
        case INT8x32:
        case SPx8:
        case DPx4:
            return get_buf_size_impl<32>(trace_addr, trace_cct,
                                     trace_info, 
                                     false);
        case INT512:
        case INT8x64:
        case SPx16:
        case DPx8:
            return get_buf_size_impl<64>(trace_addr, trace_cct,
                                     trace_info, 
                                     false);
        default: DR_ASSERT_MSG(false, "vprofile_register_trace_cb Unknown error!");
        
    }
    return -1;
}

void vprofile_register_trace_cb(vtrace_t *vtrace, bool (*filter)(opnd_t),
                                vprofile_data_t data_type,
                                void (*update_cb)(val_info_t *))
{
    if(vtrace->strictly_ordered) {
        DR_ASSERT_MSG(data_type==ANY, "vprofile_register_trace_cb usage error!");
        vtrace->buff = vtracer_create_trace_buffer(get_buf_size_impl<64>(vtrace->trace_addr, vtrace->trace_cct, vtrace->trace_info, true));
        DR_ASSERT_MSG(vtrace->buff, "vprofile_register_trace_cb usage error: buffer not allocated!");
        vprofile_register_trace_cb_impl<MAX_CLASS_SIZE,1,false>(vtrace->buff, filter, update_cb, vtrace->trace_addr, vtrace->trace_cct, vtrace->trace_info, true);
    } else {
        if(data_type==ANY) {
            for(int i=0; i<NUM_DATA_TYPES; ++i) {
                if(!vtrace->buff_ex[i]) {
                    vtrace->buff_ex[i] = vtracer_create_trace_buffer(get_buf_size(i, vtrace->trace_addr, vtrace->trace_cct, vtrace->trace_info));
                }
                vprofile_register_trace_cb_for_data_type(i, vtrace->buff_ex[i], filter, update_cb, vtrace->trace_addr, vtrace->trace_cct, vtrace->trace_info);
            }
        } else {
            vtrace->buff_ex[data_type] = vtracer_create_trace_buffer(get_buf_size((int) data_type, vtrace->trace_addr, vtrace->trace_cct, vtrace->trace_info));
            DR_ASSERT_MSG(vtrace->buff_ex[data_type], "vprofile_register_trace_cb usage error: buffer not allocated for data_type!");
            vprofile_register_trace_cb_for_data_type((int) data_type, vtrace->buff_ex[data_type], filter, update_cb, vtrace->trace_addr, vtrace->trace_cct, vtrace->trace_info);
        }
    }
}

vtrace_t *vprofile_register_trace(bool (*filter)(opnd_t),
                                  void (*update_cb)(val_info_t *),
                                  bool do_data_centric)
{
    bool trace_addr = do_data_centric;
    bool trace_cct = !do_data_centric;
    bool trace_info = false;
    bool strictly_ordered = true;
    bool trace_reg_in_memref=false;
    return vprofile_register_trace_ex(filter, update_cb, trace_addr, 
                                      trace_cct, trace_info, strictly_ordered, trace_reg_in_memref);
}

vtrace_t* vprofile_register_trace_ex(bool (*filter)(opnd_t),
                                     void (*update_cb)(val_info_t *),
                                     bool trace_addr, bool trace_cct,
                                     bool trace_info, bool strictly_ordered,
                                     bool trace_reg_in_memref)
{
    vtrace_t *vtrace = vprofile_allocate_trace(trace_addr, trace_cct,
                                              trace_info, strictly_ordered,
                                              trace_reg_in_memref);
    vprofile_register_trace_cb(vtrace, filter, ANY, update_cb);
    return vtrace;

}

void vprofile_unregister_trace(vtrace_t *vtrace)
{
    if(!vtrace) return;
    for(int i = 0; i < NUM_DATA_TYPES; i++) {
        if(vtrace->buff_ex[i]!=NULL)
            vtracer_buffer_free(vtrace->buff_ex[i]);
    }
    dr_global_free(vtrace, sizeof(*vtrace));
}