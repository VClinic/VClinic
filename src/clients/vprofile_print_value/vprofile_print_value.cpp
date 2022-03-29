#include "dr_api.h"
#include "vprofile.h"

#define MAXIMUM_LEN 32

vtrace_t* vtrace;

static int instr_count = 0;
static void *count_mutex; /* for multithread support */

static void
ClientInit(int argc, const char *argv[])
{
    return;
}

static void
ClientExit(void)
{
    dr_fprintf(STDOUT, "count = %d\n", instr_count);
    dr_mutex_destroy(count_mutex);
    vprofile_unregister_trace(vtrace);
    vprofile_exit();
}

#if !defined(ARM) && !defined(AARCH64)
bool VPROFILE_FILTER_PADDB_INSTR(instr_t* instr) {
    int opc = instr_get_opcode(instr);
    int opc_nxt = -1;
    if(instr_get_next(instr) != NULL)
    	opc_nxt = instr_get_opcode(instr_get_next(instr));
    return ((opc == OP_paddb && opc_nxt == OP_add) || (opc == OP_paddb && opc_nxt == OP_paddb) || (opc == OP_add && opc_nxt == OP_paddb));
}

void bb_instrument_cb(void *drcontext, instrlist_t *bb) {
    // *(uint64_t *) user_data = 1; // can pass!
}

void ins_instrument_cb(void *drcontext, instr_t *instr, instrlist_t *bb) {
    int opc = instr_get_opcode(instr);
    int opc_nxt = -1;
    if(instr_get_next(instr) != NULL)
    	opc_nxt = instr_get_opcode(instr_get_next(instr));
    if (opc == OP_paddb && opc_nxt == OP_div) {
        instr_disassemble(drcontext, instr, STDOUT);
        dr_fprintf(STDOUT, "\n");
    }
}
#endif

template<int size, int esize, bool is_float>
void update(val_info_t *info) {

    dr_mutex_lock(count_mutex);
    instr_count++;
    dr_mutex_unlock(count_mutex);

    const int num = size/esize;
    if(is_float) {
        switch(esize) {
            case 4:
                for(int i=0; i<num; ++i) {
                    dr_fprintf(STDOUT, "is_float=%d, esize=%d, size=%d, val = %f\n", is_float, esize, size, reinterpret_cast<uint32_t*>(info->val)[i]);
                }
                break;
            case 8:
                for(int i=0; i<num; ++i) {
                    dr_fprintf(STDOUT, "is_float=%d, esize=%d, size=%d, val = %lf\n", is_float, esize, size, reinterpret_cast<uint64_t*>(info->val)[i]);
                }
                break;
            default:
                return;
        }
    } else {
        switch(esize) {
            case 1:
                for(int i=0; i<num; ++i) {
                    dr_fprintf(STDOUT, "is_float=%d, esize=%d, size=%d, val = %d\n", is_float, esize, size, reinterpret_cast<uint8_t*>(info->val)[i]);
                }
                break;
            case 2:
                for(int i=0; i<num; ++i) {
                    dr_fprintf(STDOUT, "is_float=%d, esize=%d, size=%d, val = %d\n", is_float, esize, size, reinterpret_cast<uint16_t*>(info->val)[i]);
                }
                break;
            case 4:
                for(int i=0; i<num; ++i) {
                    dr_fprintf(STDOUT, "is_float=%d, esize=%d, size=%d, val = %d\n", is_float, esize, size, reinterpret_cast<uint32_t*>(info->val)[i]);
                }
                break;
            case 8:
                for(int i=0; i<num; ++i) {
                    dr_fprintf(STDOUT, "is_float=%d, esize=%d, size=%d, val = %d\n", is_float, esize, size, reinterpret_cast<uint64_t*>(info->val)[i]);
                }
                break;
            default:
                return;
        }
    }
}

#ifdef __cplusplus
extern "C" {
#endif

DR_EXPORT void
dr_client_main(client_id_t id, int argc, const char *argv[])
{
    dr_set_client_name("DynamoRIO Client 'vprofile_print_value'",
                       "http://dynamorio.org/issues");
    ClientInit(argc, argv);
#if !defined(ARM) && !defined(AARCH64)
    vprofile_init(VPROFILE_FILTER_PADDB_INSTR, NULL, ins_instrument_cb, bb_instrument_cb,
                     VPROFILE_COLLECT_CCT);
#else
    vprofile_init(VPROFILE_FILTER_ALL_INSTR, NULL, NULL, NULL,
                        VPROFILE_DEFAULT);
#endif
    vtrace = vprofile_allocate_trace(VPROFILE_TRACE_DEFAULT);
    vprofile_register_trace_template_cb(vtrace, VPROFILE_FILTER_ALL_OPND, VPROFILE_OPND_MASK_ALL, update);
    dr_register_exit_event(ClientExit);

    count_mutex = dr_mutex_create();
}

#ifdef __cplusplus
}
#endif
