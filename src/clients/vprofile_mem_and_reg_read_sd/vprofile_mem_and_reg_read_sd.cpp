#include "dr_api.h"
#include "vprofile.h"
#include <string>
#include "droption.h"
#define WINDOW_ENABLE 10000
#define WINDOW_DISABLE 1000000

int win_enable;
int win_disable;

static droption_t<bool> op_enable_sampling
(DROPTION_SCOPE_CLIENT, "enable_sampling", 0, 0, 64, "Enable Bursty Sampling",
 "Enable bursty sampling for lower overhead with less profiling accuracy.");

 static droption_t<bool> op_enable_cct
(DROPTION_SCOPE_CLIENT, "enable_cct", 0, 0, 64, "Enable Calling Context Collection",
 "Enable calling context collection.");

static droption_t<bool> op_enable_opnd_sampling
(DROPTION_SCOPE_CLIENT, "enable_opnd_sampling", 0, 0, 64, "Enable Operand-level Bursty Sampling",
 "Enable bursty sampling for lower overhead with less profiling accuracy.");

static droption_t<bool> op_enable_period_sampling
(DROPTION_SCOPE_CLIENT, "enable_period_sampling", 0, 0, 64, "Enable Operand-level Periodical Sampling",
 "Enable periodical sampling for lower overhead with less profiling accuracy.");

static droption_t<int> op_period
(DROPTION_SCOPE_CLIENT, "period", 100, 1, 10000, "Periodical window size of Operand-level Periodical Sampling",
 "Periodical window size of Operand-level Periodical Sampling.");

static droption_t<int> op_window
(DROPTION_SCOPE_CLIENT, "window", WINDOW_DISABLE, 0, INT32_MAX, "Window size configuration of sampling",
 "Window size of sampling. Only available when sampling is enabled.");

static droption_t<int> op_window_enable
(DROPTION_SCOPE_CLIENT, "window_enable", WINDOW_ENABLE, 0, INT32_MAX, "Window enabled size configuration of sampling",
 "Window enabled size of sampling. Only available when sampling is enabled.");

static droption_t<bool> op_help
(DROPTION_SCOPE_CLIENT, "help", 0, 0, 64, "Show this help",
 "Show this help.");

vtrace_t* vtrace;

bool 
VPROFILE_FILTER_OPND(opnd_t opnd, vprofile_src_t opmask) {
    uint32_t user_mask = (ANY_DATA_TYPE | REGISTER | MEMORY | READ | BEFORE);
    return ((user_mask & opmask) == opmask);
}

void update(val_info_t *info) {
	return;
}

static void
ClientInit(int argc, const char *argv[])
{
    /* Parse options */
    std::string parse_err;
    int last_index;
    if (!droption_parser_t::parse_argv(DROPTION_SCOPE_CLIENT, argc, argv, &parse_err, &last_index)) {
        dr_fprintf(STDERR, "Usage error: %s", parse_err.c_str());
        dr_abort();
    }
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
    dr_set_client_name("DynamoRIO Client 'vprofile_mem_and_reg_read_sd'",
                       "http://dynamorio.org/issues");
    ClientInit(argc, argv);
    uint8_t ex_flag = 0;
    uint32_t ex_trace_flag = 0;
    win_enable = op_window_enable.get_value();
    win_disable = op_window.get_value();
    vprofile_options_t vprofile_opts;
    vprofile_opts.win_disable = win_disable;
    vprofile_opts.win_enable = win_enable;
    vprofile_opts.filter = VPROFILE_FILTER_ALL_INSTR;
    vprofile_opts.user_data_cb = NULL;
    vprofile_opts.ins_instrument_cb = NULL;
    vprofile_opts.bb_instrument_cb = NULL;

    if (op_enable_cct.get_value()) {
        dr_fprintf(STDOUT, "[CLIENT LOG] enable cct collection!\n");
        ex_flag = VPROFILE_COLLECT_CCT;
        ex_trace_flag = VPROFILE_TRACE_CCT;
    }
    if (op_enable_sampling.get_value()) {
        dr_fprintf(STDOUT, "[CLIENT LOG] sampling enabled!\n");
        vprofile_opts.flag = VPROFILE_SAMPLE_BURSTY_INSTRUCTION | ex_flag;
        vprofile_init_ex(&vprofile_opts);
    } else if(op_enable_opnd_sampling.get_value()) {
        dr_fprintf(STDOUT, "[CLIENT LOG] operand sampling enabled!\n");
        vprofile_opts.flag = VPROFILE_SAMPLE_BURSTY_OPERAND | ex_flag;
        vprofile_init_ex(&vprofile_opts);
    } else if(op_enable_period_sampling.get_value()) {
        int window = op_period.get_value();
        dr_fprintf(STDOUT, "[CLIENT LOG] periodical sampling enabled: window=%d!\n", window);
        vprofile_opts.flag = VPROFILE_SAMPLE_PERIODICAL_OPERAND | ex_flag;
        vprofile_init_ex(&vprofile_opts);
        vprofile_set_period_sampling_window(window);
    } else {
        vprofile_opts.flag = VPROFILE_DEFAULT | ex_flag;
        vprofile_init_ex(&vprofile_opts);
    }
    vtrace = vprofile_allocate_trace(VPROFILE_TRACE_DEFAULT | VPROFILE_TRACE_STRICTLY_ORDERED | ex_trace_flag);

    uint32_t opnd_mask = (ANY_DATA_TYPE | REGISTER | MEMORY | READ | BEFORE);
    vprofile_register_trace_cb(vtrace, VPROFILE_FILTER_OPND, opnd_mask, ANY, update);

    dr_register_exit_event(ClientExit);
}

#ifdef __cplusplus
}
#endif
