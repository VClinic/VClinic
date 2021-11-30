#include "dr_api.h"
#include "vprofile.h"

vtrace_t* vtrace;
vreporter_t* vreport;

logger_t int_freq, sp_freq, dp_freq;

void int_handler(vreporter_t* vreporter, uint64_t& key, uint64_t& data, uint64_t& global) {
    vreporter->metric("Value", key);
    vreporter->metric("Frequency", data, global);
}

void sp_handler(vreporter_t* vreporter, uint32_t& key, uint64_t& data, uint64_t& global) {
    vreporter->metric("Value", *reinterpret_cast<float*>(&key));
    vreporter->metric("Frequency", data, global);
}

void dp_handler(vreporter_t* vreporter, uint64_t& key, uint64_t& data, uint64_t& global) {
    vreporter->metric("Value", *reinterpret_cast<double*>(&key));
    vreporter->metric("Frequency", data, global);
}

static void
ClientInit(int argc, const char *argv[])
{
    vlogger_init();
    int_freq = VLogger::register_logger<uint64_t, uint64_t>();
    sp_freq = VLogger::register_logger<uint32_t, uint64_t>();
    dp_freq = VLogger::register_logger<uint64_t, uint64_t>();
    vreporter_init("value_freq", VREPORTER_DEFAULT);
    vreport = vreporter_create("", TEXT_REPORT);
    vreporter_register_section(vreport, "INT", int_freq, int_handler, true, 100, true);
    vreporter_register_section(vreport, "SINGLE", sp_freq, sp_handler, true, 100, true);
    vreporter_register_section(vreport, "DOUBLE", dp_freq, dp_handler, true, 100, true);
}

static void
ClientExit(void)
{
    vreporter_destroy(vreport);
    vreporter_exit();
    vprofile_unregister_trace(vtrace);
    vprofile_exit();
    VLogger::unregister_logger(int_freq);
    VLogger::unregister_logger(sp_freq);
    VLogger::unregister_logger(dp_freq);
    vlogger_exit();
}

template<int size, int esize, bool is_float>
void update(val_info_t *info) {
    void* logger_tls = VLogger::getLoggerTLS();
    const int num = size/esize;
    if(is_float) {
        switch(esize) {
            case 4:
                for(int i=0; i<num; ++i) {
                    VLogger::increament<uint64_t>(logger_tls, sp_freq, reinterpret_cast<uint32_t*>(info->val)[i]);
                }
                break;
            case 8:
                for(int i=0; i<num; ++i) {
                    VLogger::increament<uint64_t>(logger_tls, dp_freq, reinterpret_cast<uint64_t*>(info->val)[i]);
                }
                break;
            default:
                return;
        }
    } else {
        switch(esize) {
            case 1:
                for(int i=0; i<num; ++i) {
                    VLogger::increament<uint64_t>(logger_tls, int_freq, reinterpret_cast<uint8_t*>(info->val)[i]);
                }
                break;
            case 2:
                for(int i=0; i<num; ++i) {
                    VLogger::increament<uint64_t>(logger_tls, int_freq, reinterpret_cast<uint16_t*>(info->val)[i]);
                }
                break;
            case 4:
                for(int i=0; i<num; ++i) {
                    VLogger::increament<uint64_t>(logger_tls, int_freq, reinterpret_cast<uint32_t*>(info->val)[i]);
                }
                break;
            case 8:
                for(int i=0; i<num; ++i) {
                    VLogger::increament<uint64_t>(logger_tls, int_freq, reinterpret_cast<uint64_t*>(info->val)[i]);
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
    vprofile_init(VPROFILE_FILTER_ALL_INSTR, NULL, NULL,
                     VPROFILE_DEFAULT);
    vtrace = vprofile_allocate_trace(false, false, false, false, false, false);
    vprofile_register_trace_template_cb(vtrace, VPROFILE_FILTER_ALL_OPND, VPROFILE_OPND_MASK_ALL, update);
    dr_register_exit_event(ClientExit);
}

#ifdef __cplusplus
}
#endif