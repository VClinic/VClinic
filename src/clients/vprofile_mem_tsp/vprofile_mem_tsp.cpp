#include <map>
#include <vector>
#include <string>
#include <sys/stat.h>

#include <dr_api.h>
#include "drmgr.h"
#include "drreg.h"
#include "drutil.h"
#include "drcctlib.h"
#include "dr_tools.h"
#include <sys/time.h>
#include "vprofile.h"
#include "droption.h"

#define VPROFILE_MEM_TSP_EXIT_PROCESS(format, args...)                            \
    DRCCTLIB_CLIENT_EXIT_PROCESS_TEMPLATE("vprofile_mem_tsp", format, ##args)

#define WINDOW_ENABLE 1000000
#define WINDOW_DISABLE 100000000
// #define WINDOW_CLEAN 10

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

static droption_t<bool> op_help
(DROPTION_SCOPE_CLIENT, "help", 0, 0, 64, "Show this help",
 "Show this help.");

static droption_t<int> op_window
(DROPTION_SCOPE_CLIENT, "window", WINDOW_DISABLE, 0, INT32_MAX, "Window size configuration of sampling",
 "Window size of sampling. Only available when sampling is enabled.");

static droption_t<int> op_window_enable
(DROPTION_SCOPE_CLIENT, "window_enable", WINDOW_ENABLE, 0, INT32_MAX, "Window enabled size configuration of sampling",
 "Window enabled size of sampling. Only available when sampling is enabled.");

using namespace std;

vtrace_t* vtrace;
static string g_folder_name;
static int tls_idx;

struct INTRedLog_t{
    uint64_t tot;
    uint64_t red;
    uint64_t fred;
    uint64_t redByteMap;
};

struct FPRedLog_t{
    uint64_t ftot;
    uint64_t fred;
    uint64_t redByteMap;
};

typedef std::map<context_handle_t, INTRedLog_t> INTRedLogMap_t;
typedef std::map<context_handle_t, FPRedLog_t> FPRedLogMap_t;

typedef struct _per_thread_t {
    FPRedLogMap_t* FPRedLogMap;
    INTRedLogMap_t* INTRedLogMap;
    file_t output_file;
    int32_t threadId;
    vector<instr_t*> *instr_clones;
} per_thread_t;

file_t gFlagF;

file_t gFile;
FILE* gJson;
static void *gLock;
#ifndef _WERROR
file_t fwarn;
bool warned=false;
#endif

enum {
    INSTRACE_TLS_OFFS_BUF_PTR,
    INSTRACE_TLS_OFFS_INTLOG_PTR,
    INSTRACE_TLS_OFFS_FPLOG_PTR,
    INSTRACE_TLS_COUNT, /* total number of TLS slots allocated */
};

static reg_id_t tls_seg;
static uint tls_offs;

bool 
VPROFILE_FILTER_OPND(opnd_t opnd, vprofile_src_t opmask) {
    uint32_t user_mask = (ANY_DATA_TYPE | MEMORY | READ | WRITE | BEFORE | AFTER);
    return ((user_mask & opmask) == opmask);
}

template<int size, int esize, bool is_float>
void update(val_info_t *info) {
    per_thread_t* pt = (per_thread_t *)drmgr_get_tls_field(dr_get_current_drcontext(), tls_idx);
    file_t gTraceFile = pt->output_file;

    dr_fprintf(gTraceFile, "  type     : 0x%x\t", info->type);
    //dr_fprintf(gTraceFile, "  is_float : %d\t", info->is_float);
    //dr_fprintf(gTraceFile, "  size     : %zu\t", info->size);
    //dr_fprintf(gTraceFile, "  esize    : %zu\t", info->esize);
    dr_fprintf(gTraceFile, "  addr     : %p\t", info->addr);
    dr_fprintf(gTraceFile, "  app pc   : %ld\t", info->pc);
    dr_fprintf(gTraceFile, "  tsc      : %ld\n", info->tsc);

    //dr_fprintf(gTraceFile, "  info ptr  : %p\n", info->info);
	return;
}

/*
template<int size, int esize, bool is_float>
void update(val_info_t *info) {
	return;
}
*/

static void 
basic_block_isb(void *drcontext, instrlist_t *bb) {
    instr_t* ins = instrlist_first(bb);
    MINSERT(bb, ins, instr_create_0dst_1src(drcontext, OP_isb, OPND_CREATE_INT8(15)));
}

static void
ThreadOutputFileInit(per_thread_t *pt, void *drcontext)
{
    int32_t id = dr_get_thread_id(drcontext);
    pt->threadId = id;
    char name[MAXIMUM_PATH] = "";
    sprintf(name + strlen(name), "%s/thread-%d.topn.log", g_folder_name.c_str(), id);
    pt->output_file = dr_open_file(name, DR_FILE_WRITE_OVERWRITE | DR_FILE_ALLOW_LARGE);
    DR_ASSERT(pt->output_file != INVALID_FILE);
    if (op_enable_sampling.get_value()) {
        dr_fprintf(pt->output_file, "[MEM_TSP INFO] Sampling Enabled\n");
    } else {
        dr_fprintf(pt->output_file, "[MEM_TSP INFO] Sampling Disabled\n");
    }
}

static void
ClientThreadStart(void *drcontext)
{
    // assert(dr_get_itimer(ITIMER_REAL));
    per_thread_t *pt = (per_thread_t *)dr_thread_alloc(drcontext, sizeof(per_thread_t));
    if (pt == NULL) {
        VPROFILE_MEM_TSP_EXIT_PROCESS("pt == NULL");
    }
    pt->INTRedLogMap = new INTRedLogMap_t();
    pt->FPRedLogMap = new FPRedLogMap_t();
    // pt->FPRedLogMap->rehash(10000000);
    // pt->INTRedLogMap->rehash(10000000);
    pt->instr_clones = new vector<instr_t*>();
    drmgr_set_tls_field(drcontext, tls_idx, (void *)pt);
    // init output files
    ThreadOutputFileInit(pt,drcontext);
}

static void
ClientThreadEnd(void *drcontext)
{
    per_thread_t *pt = (per_thread_t *)drmgr_get_tls_field(drcontext, tls_idx);
    dr_close_file(pt->output_file);
    for(size_t i=0;i<pt->instr_clones->size();++i) {
        instr_destroy(drcontext, (*pt->instr_clones)[i]);
    }
    delete pt->instr_clones;
    delete pt->INTRedLogMap;
    delete pt->FPRedLogMap;
#ifdef DEBUG_REUSE
    dr_close_file(pt->log_file);
#endif 
    dr_thread_free(drcontext, pt, sizeof(per_thread_t));
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
    /* Creating result directories */

    pid_t pid = getpid();
#ifdef ARM_CCTLIB
    char name[MAXIMUM_PATH] = "arm-";
#else
    char name[MAXIMUM_PATH] = "x86-";
#endif
    gethostname(name + strlen(name), MAXIMUM_PATH - strlen(name));
    sprintf(name + strlen(name), "-%d-MEM_TSP", pid);
    g_folder_name.assign(name, strlen(name));
    mkdir(g_folder_name.c_str(), S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH);

    dr_fprintf(STDOUT, "[MEM_TSP INFO] Profiling result directory: %s\n", g_folder_name.c_str());

    sprintf(name+strlen(name), "/MEM_TSP.log");
    gFile = dr_open_file(name, DR_FILE_WRITE_OVERWRITE | DR_FILE_ALLOW_LARGE);
    gJson = fopen("report.json", "w");
    DR_ASSERT(gFile != INVALID_FILE);
    DR_ASSERT(gJson != NULL);
    if (op_enable_sampling.get_value()) {
        dr_fprintf(STDOUT, "[MEM_TSP INFO] Sampling Enabled\n");
        dr_fprintf(gFile, "[MEM_TSP INFO] Sampling Enabled\n");
        win_enable = op_window_enable.get_value();
        win_disable= op_window.get_value();
        float rate = (float)win_enable / (float)win_disable;
        dr_fprintf(STDOUT, "[MEM_TSP INFO] Sampling Rate: %.3f, Window Size: %ld\n", rate, win_disable);
        dr_fprintf(gFile,  "[MEM_TSP INFO] Sampling Rate: %.3f, Window Size: %ld\n", rate, win_disable);
    } else {
        dr_fprintf(STDOUT, "[MEM_TSP INFO] Sampling Disabled\n");
        dr_fprintf(gFile, "[MEM_TSP INFO] Sampling Disabled\n");
    }
    if (dr_using_all_private_caches()) {
        dr_fprintf(STDOUT, "[MEM_TSP INFO] Thread Private is enabled.\n");
        dr_fprintf(gFile,  "[MEM_TSP INFO] Thread Private is enabled.\n");
    } else {
        dr_fprintf(STDOUT, "[MEM_TSP INFO] Thread Private is disabled.\n");
        dr_fprintf(gFile,  "[MEM_TSP INFO] Thread Private is disabled.\n");
    }
    if (op_help.get_value()) {
        dr_fprintf(STDOUT, "%s\n", droption_parser_t::usage_long(DROPTION_SCOPE_CLIENT).c_str());
        exit(1);
    }
#ifdef TSP_DEBUG
    sprintf(name+strlen(name), ".debug");
    gDebug = dr_open_file(name, DR_FILE_WRITE_OVERWRITE | DR_FILE_ALLOW_LARGE);
    DR_ASSERT(gDebug != INVALID_FILE);
#endif
    gFlagF = dr_open_file("debug.log", DR_FILE_WRITE_OVERWRITE | DR_FILE_ALLOW_LARGE);
    DR_ASSERT(gFlagF != INVALID_FILE);
}

static void
ClientExit(void)
{

#ifndef _WERROR
    if(warned) {
        dr_fprintf(gFile, "####################################\n");
        dr_fprintf(gFile, "WARNING: some unexpected instructions are ignored. Please check zerospy.log.warn for detail.\n");
        dr_fprintf(gFile, "####################################\n");
    }
#endif
    dr_close_file(gFlagF);
    dr_close_file(gFile);
    fclose(gJson);
    if (!dr_raw_tls_cfree(tls_offs, INSTRACE_TLS_COUNT)) {
        VPROFILE_MEM_TSP_EXIT_PROCESS(
            "ERROR: mem_tsp dr_raw_tls_cfree fail");
    }

    dr_mutex_destroy(gLock);
    if (!drmgr_unregister_thread_init_event(ClientThreadStart) ||
        !drmgr_unregister_thread_exit_event(ClientThreadEnd) ||
        !drmgr_unregister_tls_field(tls_idx)) {
        printf("ERROR: mem_tsp failed to unregister in ClientExit");
        fflush(stdout);
        exit(-1);
    }
    vprofile_unregister_trace(vtrace);
    vprofile_exit();
}

#ifdef __cplusplus
extern "C" {
#endif

DR_EXPORT void
dr_client_main(client_id_t id, int argc, const char *argv[])
{
    dr_set_client_name("DynamoRIO Client 'vprofile_mem_tsp'",
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

    if(op_enable_sampling.get_value()) {
        vtracer_enable_sampling(op_window_enable.get_value(), op_window.get_value());
    }

    drmgr_priority_t thread_init_pri = { sizeof(thread_init_pri),
                                         "thread-init", NULL, NULL,
                                         DRCCTLIB_THREAD_EVENT_PRI + 1 };
    drmgr_priority_t thread_exit_pri = { sizeof(thread_exit_pri),
                                         "thread-exit", NULL, NULL,
                                         DRCCTLIB_THREAD_EVENT_PRI - 1 };

    if (   !drmgr_register_thread_init_event_ex(ClientThreadStart, &thread_init_pri) 
        || !drmgr_register_thread_exit_event_ex(ClientThreadEnd, &thread_exit_pri) ) {
        VPROFILE_MEM_TSP_EXIT_PROCESS("ERROR:unable to register events");
    }

    tls_idx = drmgr_register_tls_field();
    if (tls_idx == -1) {
        VPROFILE_MEM_TSP_EXIT_PROCESS("ERROR:drmgr_register_tls_field fail");
    }
    if (!dr_raw_tls_calloc(&tls_seg, &tls_offs, INSTRACE_TLS_COUNT, 0)) {
        VPROFILE_MEM_TSP_EXIT_PROCESS(
            "ERROR: dr_raw_tls_calloc fail");
    }
    gLock = dr_mutex_create();

    vtrace = vprofile_allocate_trace(VPROFILE_TRACE_ADDR_TSC_PC | VPROFILE_TRACE_BEFORE_WRITE);
    uint32_t opnd_mask = (ANY_DATA_TYPE | MEMORY | READ | WRITE | BEFORE | AFTER);

    vprofile_register_trace_template_cb(vtrace, VPROFILE_FILTER_OPND, opnd_mask, update);
    dr_register_exit_event(ClientExit);
}

#ifdef __cplusplus
}
#endif