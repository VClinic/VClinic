#include "dr_api.h"
#include "vprofile.h"

static void
ClientInit(int argc, const char *argv[])
{
}

static void
ClientExit(void)
{
    vprofile_exit();
}

#ifdef __cplusplus
extern "C" {
#endif

DR_EXPORT void
dr_client_main(client_id_t id, int argc, const char *argv[])
{
    dr_set_client_name("DynamoRIO Client 'value_freq'",
                       "http://dynamorio.org/issues");
    ClientInit(argc, argv);
    vprofile_init(VPROFILE_FILTER_ALL_INSTR, NULL, NULL,
                     VPROFILE_DEFAULT);
    dr_register_exit_event(ClientExit);
}

#ifdef __cplusplus
}
#endif