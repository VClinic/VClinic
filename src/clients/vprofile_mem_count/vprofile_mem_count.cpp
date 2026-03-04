#include "dr_api.h"
#include "vprofile.h"

#include <map>
#include <vector>
#include <string>
#include <sys/stat.h>

#include "droption.h"
#include "drmgr.h"
#include "drreg.h"
#include "drutil.h"
#include "drcctlib.h"
#include "dr_tools.h"
#include <sys/time.h>
#include "droption.h"

#define MEM_COUNT_EXIT_PROCESS(format, args...)                            \
    DRCCTLIB_CLIENT_EXIT_PROCESS_TEMPLATE("vprofile_mem_count", format, ##args)
#define WINDOW_ENABLE 10000
#define WINDOW_DISABLE 1000000

using namespace std;

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

vtrace_t* vtrace;
static string g_folder_name;
static int tls_idx;
static uint64_t global_total_count = 0;

typedef struct _per_thread_t {
    uint64_t count;
    file_t output_file;
    int32_t threadId;
} per_thread_t;

static void *gLock;
file_t gFlagF;
static reg_id_t tls_seg;
static uint tls_offs;
file_t gFile;
FILE* gJson;

bool 
VPROFILE_FILTER_OPND(opnd_t opnd, vprofile_src_t opmask) {
    uint32_t user_mask = (ANY_DATA_TYPE | MEMORY | READ | WRITE | BEFORE | AFTER);
    return ((user_mask & opmask) == opmask);
}

template<int size, int esize, bool is_float>
void update(val_info_t *info) {
    per_thread_t* pt = (per_thread_t *)drmgr_get_tls_field(dr_get_current_drcontext(), tls_idx);
    pt->count++;
	return;
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
    dr_fprintf(pt->output_file, "[MEM_COUNT INFO] START\n");
}

static void
ClientThreadStart(void *drcontext)
{
    // assert(dr_get_itimer(ITIMER_REAL));
    per_thread_t *pt = (per_thread_t *)dr_thread_alloc(drcontext, sizeof(per_thread_t));
    if (pt == NULL) {
        MEM_COUNT_EXIT_PROCESS("pt == NULL");
    }
    drmgr_set_tls_field(drcontext, tls_idx, (void *)pt);
    // init output files
    ThreadOutputFileInit(pt, drcontext);
}

static void
ClientThreadEnd(void *drcontext)
{
    per_thread_t *pt = (per_thread_t *)drmgr_get_tls_field(drcontext, tls_idx);
    uint64_t byteCount = pt->count * 28;
    uint64_t KBCount = byteCount / 1024;
    uint64_t MBCount = KBCount / 1024;
    uint64_t GBCount = MBCount / 1024;
    dr_fprintf(pt->output_file, "[MEM_COUNT INFO] END\n");
    dr_fprintf(pt->output_file, "[MEM_COUNT INFO] Local Instrument: %ld\n", pt->count);
    dr_fprintf(pt->output_file, "[MEM_COUNT INFO] Approximate Memory Useage: %ld KB\n", KBCount );
    dr_fprintf(pt->output_file, "[MEM_COUNT INFO] Approximate Memory Useage: %ld MB\n", MBCount );
    dr_fprintf(pt->output_file, "[MEM_COUNT INFO] Approximate Memory Useage: %ld GB\n", GBCount );
    dr_mutex_lock(gLock);
    dr_fprintf(gFile, "[MEM_COUNT INFO]#THREAD %d END\n", pt->threadId);    
    dr_fprintf(gFile, "[MEM_COUNT INFO] Local Instrument: %ld\n", pt->count);
    global_total_count += pt->count;
    //dr_fprintf(gFile, "[MEM_COUNT INFO] Approximate Memory Useage: %ld KB\n", KBCount );
    //dr_fprintf(gFile, "[MEM_COUNT INFO] Approximate Memory Useage: %ld MB\n", MBCount );
    //dr_fprintf(gFile, "[MEM_COUNT INFO] Approximate Memory Useage: %ld GB\n", GBCount );
    dr_mutex_unlock(gLock);




    dr_close_file(pt->output_file);
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
    sprintf(name + strlen(name), "-%d-mem_count", pid);
    g_folder_name.assign(name, strlen(name));
    mkdir(g_folder_name.c_str(), S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH);

    dr_fprintf(STDOUT, "[MEM_COUNT INFO] Profiling result directory: %s\n", g_folder_name.c_str());

    sprintf(name+strlen(name), "/mem_count.log");
    gFile = dr_open_file(name, DR_FILE_WRITE_OVERWRITE | DR_FILE_ALLOW_LARGE);
    gJson = fopen("report.json", "w");
    DR_ASSERT(gFile != INVALID_FILE);
    DR_ASSERT(gJson != NULL);

    if (dr_using_all_private_caches()) {
        dr_fprintf(STDOUT, "[MEM_COUNT INFO] Thread Private is enabled.\n");
        dr_fprintf(gFile,  "[MEM_COUNT INFO] Thread Private is enabled.\n");
    } else {
        dr_fprintf(STDOUT, "[MEM_COUNT INFO] Thread Private is disabled.\n");
        dr_fprintf(gFile,  "[MEM_COUNT INFO] Thread Private is disabled.\n");
    }
    gFlagF = dr_open_file("debug.log", DR_FILE_WRITE_OVERWRITE | DR_FILE_ALLOW_LARGE);
    DR_ASSERT(gFlagF != INVALID_FILE);
}

static void
ClientExit(void)
{
    uint64_t byteCount = global_total_count * 28;
    uint64_t KBCount = byteCount / 1024;
    uint64_t MBCount = KBCount / 1024;
    uint64_t GBCount = MBCount / 1024;
    dr_fprintf(gFile, "[MEM_COUNT INFO] END\n");
    dr_fprintf(gFile, "[MEM_COUNT INFO] Total Instrument: %ld\n", global_total_count);
    dr_fprintf(gFile, "[MEM_COUNT INFO] Approximate Memory Useage: %ld KB\n", KBCount );
    dr_fprintf(gFile, "[MEM_COUNT INFO] Approximate Memory Useage: %ld MB\n", MBCount );
    dr_fprintf(gFile, "[MEM_COUNT INFO] Approximate Memory Useage: %ld GB\n", GBCount );

    dr_close_file(gFlagF);
    dr_close_file(gFile);
    fclose(gJson);

    dr_mutex_destroy(gLock);
    if (!drmgr_unregister_thread_init_event(ClientThreadStart) ||
        !drmgr_unregister_thread_exit_event(ClientThreadEnd) ||
        !drmgr_unregister_tls_field(tls_idx)) {
        printf("ERROR: vprofile_mem_count failed to unregister in ClientExit");
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
    dr_set_client_name("DynamoRIO Client 'vprofile_mem_count'",
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

    drmgr_priority_t thread_init_pri = { sizeof(thread_init_pri),
                                         "mem_count-thread-init", NULL, NULL,
                                         DRCCTLIB_THREAD_EVENT_PRI + 1 };
    drmgr_priority_t thread_exit_pri = { sizeof(thread_exit_pri),
                                         "mem_count-thread-exit", NULL, NULL,
                                         DRCCTLIB_THREAD_EVENT_PRI - 1 };

    if (   !drmgr_register_thread_init_event_ex(ClientThreadStart, &thread_init_pri) 
        || !drmgr_register_thread_exit_event_ex(ClientThreadEnd, &thread_exit_pri) ) {
        MEM_COUNT_EXIT_PROCESS("ERROR: memcount unable to register events");
    }

    tls_idx = drmgr_register_tls_field();
    if (tls_idx == -1) {
        MEM_COUNT_EXIT_PROCESS("ERROR: memcount drmgr_register_tls_field fail");
    }

    gLock = dr_mutex_create();

    vtrace = vprofile_allocate_trace(VPROFILE_TRACE_ADDR | VPROFILE_TRACE_BEFORE_WRITE);

    uint32_t opnd_mask = (ANY_DATA_TYPE | MEMORY | READ | WRITE | BEFORE | AFTER);

    vprofile_register_trace_template_cb(vtrace, VPROFILE_FILTER_OPND, opnd_mask, update);
    dr_register_exit_event(ClientExit);
}

#ifdef __cplusplus
}
#endif