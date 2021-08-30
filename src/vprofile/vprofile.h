#ifndef __VPROFILE_H__
#define __VPROFILE_H__

#include "drcctlib.h"
#include "vlogger/vlogger.h"
#include "vreporter/vreporter.h"
#include "vtracer/vtracer.h"
/* High-level Interfaces for VProfile Framework */

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
  bool trace_addr;
  bool trace_cct;
  bool trace_info;
  bool trace_reg_in_memref;
  bool strictly_ordered;
  union {
    /* strictly_ordered, so only single buffer with extra tracing of data types
     * is allocated */
    vtrace_buffer_t *buff;
    /* not strictly ordered, so each data type maintains its own buffer */
    vtrace_buffer_t *buff_ex[NUM_DATA_TYPES];
  };
};

/* Data structure to pass the traced values into trace updating callbacks. Note
 * that some of the values may not be filled in to avoid unnecessary message
 * passing when the information can be assumed to be already known for the
 * caller (e.g., size/esize when not strictly_ordered, no addr info, etc). */
struct val_info_t {
  uint64_t addr;
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
bool vprofile_init(bool (*filter)(instr_t *),
                   void (*ins_instrument_cb)(void *, instr_t *, instrlist_t *),
                   void (*bb_instrument_cb)(void *, instrlist_t *),
                   uint8_t flag);

/* Clear and free all the data used by VProfile */
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
 *   trace_reg_in_memref=false
 */
vtrace_t *vprofile_register_trace(bool (*filter)(opnd_t),
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
 * @param update_cb the update function
 * allowing the user to update with the traced values when the buffer is full.
 * May be null.
 * @param trace_addr tell VProfile tracer to trace memory address/source
 * register.
 * @param trace_cct tell VProfile tracer to trace calling context
 * @param trace_info tell VProfile tracer to trace additional info specified by
 * the user
 * @param strictly_ordered tell VProfile to trace the value in a single trace
 * buffer and trace the data type of each access
 * @param trace_reg_in_memref tell VProfile tracer to trace the register values
 * used in memory operand
 *
 * Return NULL when any failure detected
 */
vtrace_t* vprofile_register_trace_ex(bool (*filter)(opnd_t),
                                     void (*update_cb)(val_info_t *),
                                     bool trace_addr, bool trace_cct,
                                     bool trace_info, bool strictly_ordered,
                                     bool trace_reg_in_memref);

/* Allocate a new vprofile trace data structure with the given configurations.
 * Note that the trace buffer is not registered (thus NULL). */
vtrace_t* vprofile_allocate_trace(bool trace_addr, bool trace_cct,
                                  bool trace_info, bool strictly_ordered,
                                  bool trace_reg_in_memref);

/* Register update callbacks for the specified data_type within the given
 * vtrace. If the vtrace is configured as strictly_ordered, the data_type must
 * be ANY; otherwise it will result in assertion failure (usage error).
 * If the data_type is set as ANY, all trace buffer is registered with the given
 * callbacks.
 */
void vprofile_register_trace_cb(vtrace_t *vtrace, bool (*filter)(opnd_t),
                                vprofile_data_t data_type,
                                void (*update_cb)(val_info_t *));

#define vprofile_register_trace_template_cb(vtrace, filter, update_cb) do {\
  vprofile_register_trace_cb(vtrace, filter, INT8, update_cb<1,1,false>);\
  vprofile_register_trace_cb(vtrace, filter, INT16, update_cb<2,2,false>);\
  vprofile_register_trace_cb(vtrace, filter, INT32, update_cb<4,4,false>);\
  vprofile_register_trace_cb(vtrace, filter, SPx1, update_cb<4,4,true>);\
  vprofile_register_trace_cb(vtrace, filter, INT64, update_cb<8,8,false>);\
  vprofile_register_trace_cb(vtrace, filter, DPx1, update_cb<8,8,true>);\
  vprofile_register_trace_cb(vtrace, filter, INT128, update_cb<16,1,false>);\
  vprofile_register_trace_cb(vtrace, filter, INT8x16, update_cb<16,1,false>);\
  vprofile_register_trace_cb(vtrace, filter, SPx4, update_cb<16,4,true>);\
  vprofile_register_trace_cb(vtrace, filter, DPx2, update_cb<16,8,true>);\
  vprofile_register_trace_cb(vtrace, filter, INT256, update_cb<32,1,false>);\
  vprofile_register_trace_cb(vtrace, filter, INT8x32, update_cb<32,1,false>);\
  vprofile_register_trace_cb(vtrace, filter, SPx8, update_cb<32,4,true>);\
  vprofile_register_trace_cb(vtrace, filter, DPx4, update_cb<32,8,true>);\
  vprofile_register_trace_cb(vtrace, filter, INT512, update_cb<64,1,false>);\
  vprofile_register_trace_cb(vtrace, filter, INT8x64, update_cb<64,1,false>);\
  vprofile_register_trace_cb(vtrace, filter, SPx16, update_cb<64,4,true>);\
  vprofile_register_trace_cb(vtrace, filter, DPx8, update_cb<64,8,true>); \
} while(0)

/* Clear and free all the data allocated in the given vtrace */
void vprofile_unregister_trace(vtrace_t *vtrace);

/* Insert inlined instrumentation codes to trace user-specified additional information */
void vprofile_insert_trace_info(void *drcontext, instr_t *where,
                                instrlist_t *ilist, vtrace_t *vtrace,
                                void *info);

#endif