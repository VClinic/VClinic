#include "dr_api.h"
#include "vprofile.h"
vtrace_t* vtrace;

VLogger::MetricLogs<uint64_t, uint64_t> int_freq(true);
VLogger::MetricLogs<uint32_t, uint64_t> sp_freq(true);
VLogger::MetricLogs<uint64_t, uint64_t> dp_freq(true);

static void
PrintTopN() {

}

static void
ClientInit(int argc, const char *argv[])
{
}

static void
ClientExit(void)
{
    PrintTopN();
    vprofile_unregister_trace(vtrace);
    vprofile_exit();
    vlogger_exit();
}

template<int size, int esize, bool is_float>
void update(val_info_t *info) {
    const int num = size/esize;
    if(is_float) {
        switch(esize) {
            case 4:
                for(int i=0; i<num; ++i) {
                    sp_freq.increament(reinterpret_cast<uint32_t*>(info->val)[i]);
                }
                break;
            case 8:
                for(int i=0; i<num; ++i) {
                    dp_freq.increament(reinterpret_cast<uint64_t*>(info->val)[i]);
                }
                break;
            default:
                return;
        }
    } else {
        switch(esize) {
            case 1:
                for(int i=0; i<num; ++i) {
                    int_freq.increament(reinterpret_cast<uint8_t*>(info->val)[i]);
                }
                break;
            case 2:
                for(int i=0; i<num; ++i) {
                    int_freq.increament(reinterpret_cast<uint16_t*>(info->val)[i]);
                }
                break;
            case 4:
                for(int i=0; i<num; ++i) {
                    int_freq.increament(reinterpret_cast<uint32_t*>(info->val)[i]);
                }
                break;
            case 8:
                for(int i=0; i<num; ++i) {
                    int_freq.increament(reinterpret_cast<uint64_t*>(info->val)[i]);
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
    dr_set_client_name("DynamoRIO Client 'value_freq'",
                       "http://dynamorio.org/issues");
    ClientInit(argc, argv);
    vlogger_init();
    vprofile_init(VPROFILE_FILTER_ALL_INSTR, NULL, NULL,
                     VPROFILE_DEFAULT);
    vtrace = vprofile_allocate_trace(false, false, false, false, false);
    vprofile_register_trace_template_cb(vtrace, VPROFILE_FILTER_ALL_OPND, update);
    dr_register_exit_event(ClientExit);
}

#ifdef __cplusplus
}
#endif