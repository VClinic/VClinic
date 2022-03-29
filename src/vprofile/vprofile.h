#ifndef __VPROFILE_H__
#define __VPROFILE_H__

#include "drcctlib.h"
// #include "vlogger/vlogger.h"
// #include "vreporter/vreporter.h"
#include "vtracer/vtracer.h"
#include "vtracer/vtracer_define.h"
/* High-level Interfaces for VProfile Framework */
#define VPROFILE_LOG(level, format, args...) LOG("<vprofile>", level, format, ##args)
/* vprofile modes */
enum {
  VPROFILE_DEFAULT = 0x00,
  VPROFILE_COLLECT_CCT = 0x01,
  VPROFILE_COLLECT_DATAOBJ = 0x02,
  VPROFILE_COLLECT_DATAOBJ_TREE_BASED = 0x04,
  VPROFILE_COLLECT_DATAOBJ_ADDR_RANGE = 0x08,
};

/* TODO: Complete the data type table with all the possible cases */
/* Enumulate all possible automatically generated trace types */
enum vprofile_data_t {
  ANY = -1,
  INT8 = 0,
  INT16,
  INT32,
  SPx1,
  INT64,
  DPx1,
  INT128,
  INT8x16,
  SPx4,
  DPx2,
  INT256,
  INT8x32,
  SPx8,
  DPx4,
  INT512,
  INT8x64,
  SPx16,
  DPx8,
  NUM_DATA_TYPES
};

/* Data structure to store information of a registered trace */
struct vtrace_t {
  uint32_t trace_flag;
  uint32_t opnd_mask;
  union {
    /* strictly_ordered, so only single buffer with extra tracing of data types
     * is allocated */
    vtrace_buffer_t *buff;
    /* not strictly ordered, so each data type maintains its own buffer */
    vtrace_buffer_t *buff_ex[NUM_DATA_TYPES];
  };
};

// within int32
enum vprofile_src_t {
  VPROFILE_SRC_INVALID=0x0,
  // basic opnd attributes
  GPR_REGISTER=0x1,
  SIMD_REGISTER=0x2,
  CTR_REGISTER=0x4,
  OTH_REGISTER=0x8,
  PC=0x10,
  MEMORY=0x20,
  IMMEDIATE=0x40,
  // basic action attributes
  READ=0x100,
  WRITE=0x200,
  // basic value profiling position attributes
  // this is annotation of where the value trace point come from
  // BEFORE is before the instruction execution, and
  // AFTER is after the instruction execution.
  BEFORE=0x1000,
  AFTER=0x2000,
  // is floating
  IS_INTEGER=0x4000,
  IS_FLOATING=0x8000,
  ANY_DATA_TYPE=(IS_INTEGER|IS_FLOATING),
  // derived attributes
  REGISTER=(GPR_REGISTER|SIMD_REGISTER|CTR_REGISTER|OTH_REGISTER),
  REGISTER_READ=(REGISTER|READ|BEFORE),
  REGISTER_WRITE=(REGISTER|WRITE|AFTER),
  REGISTER_BEFORE_WRITE=(REGISTER|WRITE|BEFORE),
  PC_READ=(PC|READ|BEFORE),
  PC_WRITE=(PC|WRITE|AFTER),
  PC_BEFORE_WRITE=(PC|WRITE|BEFORE),
  MEMORY_READ=(MEMORY|READ|BEFORE),
  MEMORY_WRITE=(MEMORY|WRITE|BEFORE),
  MEMORY_BEFORE_WRITE=(REGISTER|WRITE|BEFORE)
};

#define VPROFILE_OPND_MASK_ALL ((uint32_t)0xffffffff)

#define FILTER_OPND_MASK(filter_mask, opnd_mask) ((filter_mask & opnd_mask) == opnd_mask)
#define TEST_OPND_MASK(target_mask, tester) FILTER_OPND_MASK(target_mask, tester)
enum {
  VPROFILE_TRACE_INVALID=0x0,
  VPROFILE_TRACE_VALUE=0x1,
  VPROFILE_TRACE_ADDR=0x2,
  VPROFILE_TRACE_CCT=0x4,
  VPROFILE_TRACE_INFO=0x8,
  VPROFILE_TRACE_STRICTLY_ORDERED=0x10,
  VPROFILE_TRACE_REG_IN_MEMREF=0x20,
  VPROFILE_TRACE_BEFORE_WRITE=0x40,
  // DEFAULT
  VPROFILE_TRACE_DEFAULT=VPROFILE_TRACE_VALUE,
  // valid combinations
  VPROFILE_TRACE_VAL_CCT_ADDR_INFO = 
    VPROFILE_TRACE_VALUE | VPROFILE_TRACE_CCT | VPROFILE_TRACE_ADDR | VPROFILE_TRACE_INFO,
  VPROFILE_TRACE_VAL_CCT_ADDR = 
    VPROFILE_TRACE_VALUE | VPROFILE_TRACE_CCT | VPROFILE_TRACE_ADDR,
  VPROFILE_TRACE_VAL_CCT_INFO = 
    VPROFILE_TRACE_VALUE | VPROFILE_TRACE_CCT | VPROFILE_TRACE_INFO,
  VPROFILE_TRACE_VAL_CCT =
    VPROFILE_TRACE_VALUE | VPROFILE_TRACE_CCT,
  VPROFILE_TRACE_VAL_ADDR_INFO = 
    VPROFILE_TRACE_VALUE | VPROFILE_TRACE_ADDR | VPROFILE_TRACE_INFO,
  VPROFILE_TRACE_VAL_ADDR = 
    VPROFILE_TRACE_VALUE | VPROFILE_TRACE_ADDR,
  VPROFILE_TRACE_VAL_INFO = 
    VPROFILE_TRACE_VALUE | VPROFILE_TRACE_INFO,
  VPROFILE_TRACE_CCT_ADDR_INFO = 
    VPROFILE_TRACE_CCT | VPROFILE_TRACE_ADDR | VPROFILE_TRACE_INFO,
  VPROFILE_TRACE_CCT_ADDR = 
    VPROFILE_TRACE_CCT | VPROFILE_TRACE_ADDR,
  VPROFILE_TRACE_CCT_INFO = 
    VPROFILE_TRACE_CCT | VPROFILE_TRACE_INFO,
  VPROFILE_TRACE_ADDR_INFO = 
    VPROFILE_TRACE_ADDR | VPROFILE_TRACE_INFO
};

#include "vprofile_filter_func_list.h"

/* Data structure to pass the traced values into trace updating callbacks. Note
 * that some of the values may not be filled in to avoid unnecessary message
 * passing when the information can be assumed to be already known for the
 * caller (e.g., size/esize when not strictly_ordered, no addr info, etc). */
struct val_info_t {
  // if type & GPR_REGISTER, addr is gpr register id;
  // else if type & SIMD_REGISTER, addr is simd register id;
  // else if type & CTR_REGISTER, addr is control register id;
  // else if type & OTH_REGISTER, addr is dynamorio register id;
  // else if type & MEMORY, addr is a memory address
  // else: undefined
  uint64_t addr;
  // vprofile_src_t
  uint32_t type;
  int32_t ctxt_hndl;
  void *val;
  void *info;
  uint8_t size;
  uint8_t esize;
  bool is_float;
};

/****************************************************************/

/**
 * Initialize VProfile with
 * an instruction filter,
 * an instrumentation function for basic blocks,
 * an instrumentation function for instructions,
 * and a mode bitvector flag.
 *
 * Options for the instruction filter can be found in
 * @see vprofile_filter_func_list.h ,
 *
 * Options for the mode flag include
 * @see VPROFILE_DEFAULT
 * @see VPROFILE_COLLECT_CCT
 * @see VPROFILE_COLLECT_DATAOBJ
 * @see VPROFILE_COLLECT_DATAOBJ_TREE_BASED
 * @see VPROFILE_COLLECT_DATAOBJ_ADDR_RANGE
 *
 * And examples of their usage can be found in the clients directory.
 *
 * @param filter the instruction filter
 * @param user_data_cb the user_data delivery function
 * allowing the user to return a tracable info pointer for the given opnd.
 * May be null.
 * @param ins_instrument_cb the instrumentation function
 * allowing the user to inject code before each instruction.
 * May be null.
 * @param bb_instrument_cb the instrumentation function
 * allowing the user to inject code before each basic block.
 * May be null.
 * @param flag the flag telling VProfile how to operate.
 *
 * Return false when any failure detected
 */
DR_EXPORT
bool vprofile_init(bool (*filter)(instr_t *),
                   void* (*user_data_cb)(void *, instr_t *, instrlist_t *, opnd_t),
                   void (*ins_instrument_cb)(void *, instr_t *, instrlist_t *),
                   void (*bb_instrument_cb)(void *, instrlist_t *),
                   uint8_t flag);

/* Clear and free all the data used by VProfile */
DR_EXPORT
void vprofile_exit();

/**
 * Register a general value trace with
 * an operand filter,
 * an update function to handle traced data when buffer is full,
 * and a mode flag.
 *
 * Options for the instruction filter can be found in
 * @see vprofile_filter_func_list.h,
 *
 * And examples of their usage can be found in the clients directory.
 *
 * @param filter the operand filter
 * @param opnd_mask the mask to filter operand, should be OR combined with 
 * values defined in @see vprofile_src_t.
 * @param update_cb the update function
 * allowing the user to update with the traced values when the buffer is full.
 * May be null.
 * @param do_data_centric tell VProfile tracer to trace values and information
 * for data centric analysis.
 *
 * Return NULL when any failure detected
 *
 * This function may be equivilant with @see vprofile_register_trace_ex, when:
 *   trace_addr=do_data_centric,
 *   trace_cct=!do_data_centric,
 *   trace_info=false,
 *   strictly_ordered=true,
 *   trace_reg_in_memref=false,
 *   trace_before_write=false
 */
DR_EXPORT
vtrace_t *vprofile_register_trace(bool (*filter)(opnd_t, vprofile_src_t),
                                  uint32_t opnd_mask,
                                  void (*update_cb)(val_info_t *),
                                  bool do_data_centric);

/**
 * Allocate and register a general value trace with
 * an operand filter,
 * an update function to handle traced data when buffer is full,
 * and a mode flag.
 *
 * Options for the instruction filter can be found in
 * @see vprofile_filter_func_list.h,
 *
 * And examples of their usage can be found in the clients directory.
 *
 * @param filter the operand filter
 * @param opnd_mask the mask to filter operand, should be OR combined with 
 * values defined in @see vprofile_src_t.
 * @param update_cb the update function
 * allowing the user to update with the traced values when the buffer is full.
 * May be null.
 * @param trace_flag the tracing flag setting for this registered trace, including:
 * @param VPROFILE_TRACE_VALUE tell VProfile tracer to trace value of a opnd
 * @param VPROFILE_TRACE_ADDR tell VProfile tracer to trace memory address/source
 * register.
 * @param VPROFILE_TRACE_CCT tell VProfile tracer to trace calling context
 * @param VPROFILE_TRACE_INFO tell VProfile tracer to trace additional info specified by
 * the user
 * @param VPROFILE_TRACE_STRICTLY_ORDERED tell VProfile to trace the value in a single trace
 * buffer and trace the data type of each access
 * @param VPROFILE_TRACE_REG_IN_MEMREF tell VProfile tracer to trace the register values
 * used in memory operand
 * @param VPROFILE_TRACE_BEFORE_WRITE tell VProfile tracer to trace the register/memory values 
 * before overwritten (thus vprofile_src_t marked as WRITE|BEFORE)
 *
 * Return NULL when any failure detected
 */
DR_EXPORT
vtrace_t* vprofile_register_trace_ex(bool (*filter)(opnd_t, vprofile_src_t),
                                     uint32_t opnd_mask,
                                     void (*update_cb)(val_info_t *),
                                     uint32_t trace_flag);

/* Allocate a new vprofile trace data structure with the given configurations.
 * Note that the trace buffer is not registered (thus NULL). */
DR_EXPORT
vtrace_t* vprofile_allocate_trace(uint32_t trace_flag);

/* Register update callbacks for the specified data_type within the given
 * vtrace. If the vtrace is configured as strictly_ordered, the data_type must
 * be ANY; otherwise it will result in assertion failure (usage error).
 * If the data_type is set as ANY, all trace buffer is registered with the given
 * callbacks.
 */
DR_EXPORT
void vprofile_register_trace_cb(vtrace_t *vtrace, bool (*filter)(opnd_t, vprofile_src_t),
                                uint32_t opnd_mask, 
                                vprofile_data_t data_type,
                                void (*update_cb)(val_info_t *));

#define vprofile_register_trace_template_cb(vtrace, filter, opnd_mask, update_cb) do {\
  vprofile_register_trace_cb(vtrace, filter, opnd_mask, INT8, update_cb<1,1,false>);\
  vprofile_register_trace_cb(vtrace, filter, opnd_mask, INT16, update_cb<2,2,false>);\
  vprofile_register_trace_cb(vtrace, filter, opnd_mask, INT32, update_cb<4,4,false>);\
  vprofile_register_trace_cb(vtrace, filter, opnd_mask, SPx1, update_cb<4,4,true>);\
  vprofile_register_trace_cb(vtrace, filter, opnd_mask, INT64, update_cb<8,8,false>);\
  vprofile_register_trace_cb(vtrace, filter, opnd_mask, DPx1, update_cb<8,8,true>);\
  vprofile_register_trace_cb(vtrace, filter, opnd_mask, INT128, update_cb<16,16,false>);\
  vprofile_register_trace_cb(vtrace, filter, opnd_mask, INT8x16, update_cb<16,1,false>);\
  vprofile_register_trace_cb(vtrace, filter, opnd_mask, SPx4, update_cb<16,4,true>);\
  vprofile_register_trace_cb(vtrace, filter, opnd_mask, DPx2, update_cb<16,8,true>);\
  vprofile_register_trace_cb(vtrace, filter, opnd_mask, INT256, update_cb<32,32,false>);\
  vprofile_register_trace_cb(vtrace, filter, opnd_mask, INT8x32, update_cb<32,1,false>);\
  vprofile_register_trace_cb(vtrace, filter, opnd_mask, SPx8, update_cb<32,4,true>);\
  vprofile_register_trace_cb(vtrace, filter, opnd_mask, DPx4, update_cb<32,8,true>);\
  vprofile_register_trace_cb(vtrace, filter, opnd_mask, INT512, update_cb<64,64,false>);\
  vprofile_register_trace_cb(vtrace, filter, opnd_mask, INT8x64, update_cb<64,1,false>);\
  vprofile_register_trace_cb(vtrace, filter, opnd_mask, SPx16, update_cb<64,4,true>);\
  vprofile_register_trace_cb(vtrace, filter, opnd_mask, DPx8, update_cb<64,8,true>); \
} while(0)

/* Clear and free all the data allocated in the given vtrace */
DR_EXPORT
void vprofile_unregister_trace(vtrace_t *vtrace);

#endif