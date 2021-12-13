#include "dr_api.h"
#include "vprofile.h"

vtrace_t* vtrace;

bool 
VPROFILE_FILTER_OPND(opnd_t opnd, vprofile_src_t opmask) {
    uint32_t user_mask = (ANY_DATA_TYPE | MEMORY | READ | BEFORE);
    return ((user_mask & opmask) == opmask);
}

template<int size, int esize, bool is_float>
void update(val_info_t *info) {
	return;
}

static void
ClientInit(int argc, const char *argv[])
{
}

static void
ClientExit(void)
{
    vprofile_unregister_trace(vtrace);
    vprofile_exit();
}

#ifdef __cplusplus
extern "C" {
#endif

DR_EXPORT void
dr_client_main(client_id_t id, int argc, const char *argv[])
{
    dr_set_client_name("DynamoRIO Client 'vprofile_memory_read'",
                       "http://dynamorio.org/issues");
    ClientInit(argc, argv);
    vprofile_init(VPROFILE_FILTER_ALL_INSTR, NULL, NULL, NULL,
                     VPROFILE_DEFAULT);
    vtrace = vprofile_allocate_trace(VPROFILE_TRACE_DEFAULT);

    uint32_t opnd_mask = (ANY_DATA_TYPE | MEMORY | READ | BEFORE);

    vprofile_register_trace_template_cb(vtrace, VPROFILE_FILTER_OPND, opnd_mask, update);
    dr_register_exit_event(ClientExit);
}

#ifdef __cplusplus
}
#endif