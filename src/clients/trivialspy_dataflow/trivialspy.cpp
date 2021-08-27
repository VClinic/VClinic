#include <unordered_map>
#include <vector>
#include <string>
#include <sys/stat.h>
#include <assert.h>
#include <algorithm>
#include <stddef.h>
#include <math.h>

#define _WERROR

#define MAX_DEPTH 10

#ifdef TIMING
#include <time.h>
#include <math.h>
uint64_t get_miliseconds() {
    struct timespec spec;
    clock_gettime(CLOCK_REALTIME, &spec);
    return spec.tv_sec*1000 + round(spec.tv_nsec / 1.0e6); // Convert nanoseconds to milliseconds
}
#endif

#include "dr_api.h"
#include "drmgr.h"
#include "drreg.h"
#include "drutil.h"
#include "drwrap.h"
#include "drcctlib.h"
#include "dataflow.h"

#include "utils.h"
#include "trivial_table.h"
#include "trivial_report.h"
#include "trivial_tracer.h"


#define WINDOW_ENABLE 1000000
#define WINDOW_DISABLE 100000000
// #define WINDOW_CLEAN 10

// Client Options
#include "droption.h"
static droption_t<bool> op_enable_sampling
(DROPTION_SCOPE_CLIENT, "enable_sampling", 0, 0, 64, "Enable Bursty Sampling",
 "Enable bursty sampling for lower overhead with less profiling accuracy.");

static droption_t<bool> op_no_trace
(DROPTION_SCOPE_CLIENT, "no_trace", 0, 0, 64, "Profile without value tracing",
 "Profile without value tracing for each dataflow graph.");

static droption_t<bool> op_help
(DROPTION_SCOPE_CLIENT, "help", 0, 0, 64, "Show this help",
 "Show this help.");

static droption_t<int> op_window
(DROPTION_SCOPE_CLIENT, "window", WINDOW_DISABLE, 0, INT32_MAX, "Window size configuration of sampling",
 "Window size of sampling. Only available when sampling is enabled.");

static droption_t<int> op_window_enable
(DROPTION_SCOPE_CLIENT, "window_enable", WINDOW_ENABLE, 0, INT32_MAX, "Window enabled size configuration of sampling",
 "Window enabled size of sampling. Only available when sampling is enabled.");

static droption_t<bool> op_enable_soft
(DROPTION_SCOPE_CLIENT, "enable_soft", 0, 0, 64, "Enable soft approximation.",
 "Enable soft approximation.");

static droption_t<bool> op_enable_hard
(DROPTION_SCOPE_CLIENT, "enable_hard", 0, 0, 64, "Enable hard approximation.",
 "Enable hard approximation.");

static droption_t<bool> op_enable_opcode
(DROPTION_SCOPE_CLIENT, "enable_opcode", 1, 0, 64, "Enable opcode triviality detection.",
 "Enable opcode triviality detection.");

static droption_t<bool> op_enable_code
(DROPTION_SCOPE_CLIENT, "enable_code", 0, 0, 64, "Enable code-centric triviality detection.",
 "Enable code-centric triviality detection.");

static droption_t<double> op_epsilon
(DROPTION_SCOPE_CLIENT, "epsilon", 1e-6, 0, 1, "Threshold for soft approximation.", "Threshold for soft approximation.");

// 0.00000095367431640625 ~ 1e-6
static droption_t<double> op_bit_count
(DROPTION_SCOPE_CLIENT, "bit_count", 20, 0, 64, "Approximated bit counts for hard approxiation.", "Approximated bit counts for hard approxiation.");

using namespace std;

#ifdef ARM_CCTLIB
#    define OPND_CREATE_CCT_INT OPND_CREATE_INT
#else
#    define OPND_CREATE_CCT_INT OPND_CREATE_INT32
#endif

static string g_folder_name;
static int tls_idx;
// static TrivialReportGeneratorBase* report_generators[ExtractorModeNum] = {0};

file_t gFile;

struct per_thread_out_file_t {
    file_t out_file;
};

#define HEAVY_THRESHOLD 5
#define MAX_CHAIN 5

void bb_update_cost(int cost) {
    per_thread_log_t *pt = (per_thread_log_t *)drmgr_get_tls_field(dr_get_current_drcontext(), global_log_space.tls_idx);
    pt->totalCost += cost;
}

void bb_update_bb_ref(int bb_idx) {
    per_thread_log_t *pt = (per_thread_log_t *)drmgr_get_tls_field(dr_get_current_drcontext(), global_log_space.tls_idx);
    pt->bb_ref[bb_idx] += 1;
}

#ifdef DEBUG_COMBINED
bool tested = false;
dr_emit_flags_t
event_basic_block_analysis(void *drcontext, void *tag, instrlist_t *bb, bool for_trace, bool translating, OUT void **user_data)
{
    if(tested) return DR_EMIT_DEFAULT;
    tested = true;
    // test
    instrlist_t* test = instrlist_create(drcontext);
    //instrlist_append(test, INSTR_CREATE_vdivss(drcontext, opnd_create_reg_partial(DR_REG_XMM1, OPSZ_4), opnd_create_reg_partial(DR_REG_XMM4, OPSZ_4), opnd_create_reg_partial(DR_REG_XMM1, OPSZ_4)));
    instrlist_append(test, INSTR_CREATE_vmulss(drcontext, opnd_create_reg_partial(DR_REG_XMM3, OPSZ_4), opnd_create_reg_partial(DR_REG_XMM2, OPSZ_4), opnd_create_reg_partial(DR_REG_XMM1, OPSZ_4)));
    instrlist_append(test, INSTR_CREATE_vmulss(drcontext, opnd_create_reg_partial(DR_REG_XMM0, OPSZ_4), opnd_create_reg_partial(DR_REG_XMM0, OPSZ_4), opnd_create_reg_partial(DR_REG_XMM1, OPSZ_4)));
    instrlist_append(test, INSTR_CREATE_vaddss(drcontext, opnd_create_reg_partial(DR_REG_XMM2, OPSZ_4), opnd_create_reg_partial(DR_REG_XMM0, OPSZ_4), opnd_create_reg_partial(DR_REG_XMM3, OPSZ_4)));
    instrlist_append(test, INSTR_CREATE_vaddss(drcontext, opnd_create_reg_partial(DR_REG_XMM5, OPSZ_4), opnd_create_reg_partial(DR_REG_XMM6, OPSZ_4), opnd_create_reg_partial(DR_REG_XMM7, OPSZ_4)));
    instrlist_append(test, INSTR_CREATE_vmulss(drcontext, opnd_create_reg_partial(DR_REG_XMM2, OPSZ_4), opnd_create_reg_partial(DR_REG_XMM2, OPSZ_4), opnd_create_reg_partial(DR_REG_XMM5, OPSZ_4)));
    instrlist_append(test, INSTR_CREATE_mulss(drcontext, opnd_create_reg_partial(DR_REG_XMM3, OPSZ_4), opnd_create_reg_partial(DR_REG_XMM2, OPSZ_4)));
    instrlist_append(test, INSTR_CREATE_mulss(drcontext, opnd_create_reg_partial(DR_REG_XMM0, OPSZ_4), opnd_create_reg_partial(DR_REG_XMM0, OPSZ_4)));
    instrlist_append(test, INSTR_CREATE_addss(drcontext, opnd_create_reg_partial(DR_REG_XMM2, OPSZ_4), opnd_create_reg_partial(DR_REG_XMM0, OPSZ_4)));
    instrlist_append(test, INSTR_CREATE_addss(drcontext, opnd_create_reg_partial(DR_REG_XMM5, OPSZ_4), opnd_create_reg_partial(DR_REG_XMM6, OPSZ_4)));
    instrlist_append(test, INSTR_CREATE_mulss(drcontext, opnd_create_reg_partial(DR_REG_XMM2, OPSZ_4), opnd_create_reg_partial(DR_REG_XMM2, OPSZ_4)));
    for(instr_t* instr = instrlist_first(test); instr!=NULL; instr=instr_get_next(instr)) {
        instr_set_app(instr);
        assert(instr_is_app(instr));
    }
    DFG* dfg = new DFG(test);
    combined_trivial_entry_list_t comb_telist;
    discoverCombinedTrivial(dfg, &comb_telist, MAX_CHAIN, HEAVY_THRESHOLD);
    dfg->print_detail(STDOUT);
    printAllDFGLog(STDOUT, true);
    assert(comb_telist.size());
    delete dfg;
    return DR_EMIT_DEFAULT;
}
#else

// This data flow analysis routine should be triggered before DrCCTProf
dr_emit_flags_t
event_basic_block_analysis(void *drcontext, void *tag, instrlist_t *bb, bool for_trace, bool translating, OUT void **user_data)
{
    // forward analysis to construct DFG
    DFG* dfg = new DFG(bb);
    if (!op_no_trace.get_value()) {
        dr_insert_clean_call(drcontext, bb, instrlist_first(bb), (void*)bb_update_cost, false, 1, OPND_CREATE_INT32(dfg->estimate()));
    }
#ifdef DEBUG_DATAFLOW
    dr_fprintf(STDOUT, "BB DFG ins: %d\n", dfg->get_ins_count());
    dr_fprintf(STDOUT, "BB DFG mem: %d\n", dfg->get_mem_count());
    dr_fprintf(STDOUT, "BB DFG:\n");
    dfg->print(STDOUT);
#endif
    per_thread_log_t *pt = (per_thread_log_t *)drmgr_get_tls_field(drcontext, global_log_space.tls_idx);
    // slice DFG to several trivial candidates (trivial sub-DFG with trivial condition)
    telist_clear(pt->trivial_entry_list);
    discoverSingularTrivialEntries(dfg, pt->trivial_entry_list, HEAVY_THRESHOLD);
    
    // Note: currently, we only discover combined trivial conditions with dataflow-only analysis
    if (op_no_trace.get_value()) {
        comb_telist_clear(pt->comb_telist);
        discoverCombinedTrivial(dfg, pt->comb_telist, MAX_CHAIN, HEAVY_THRESHOLD);
        int bidx = get_bb_idx();
        dr_insert_clean_call(drcontext, bb, instrlist_first(bb), (void*)bb_update_bb_ref, false, 1, OPND_CREATE_INT32(bidx));
        for(auto it=pt->trivial_entry_list->begin(); it!=pt->trivial_entry_list->end(); ++it) {
            getDFGLog((*it)->DFG_ctxt)->bb_idx = bidx;
        }
        for(auto it=pt->comb_telist->begin(); it!=pt->comb_telist->end(); ++it) {
            getDFGLog((*it)->DFG_ctxt)->bb_idx = bidx;
        }
    }
#ifdef DEBUG
    // check uniqueness
    std::unordered_map<instr_t*, std::vector<opnd_t> > unique;
    for(auto it=pt->trivial_entry_list->begin(); it!=pt->trivial_entry_list->end(); ++it) {
        instr_t* instr = (*it)->trivial_entry.entry;
        opnd_t opnd = (*it)->trivial_entry.opnd;
        std::vector<opnd_t>& operands = unique[instr];
        for(size_t i=0; i<operands.size(); ++i) {
            if(opnd_same(operands[i], opnd)) {
                dr_fprintf(STDERR, "Error: same operand appear in the trivial entry list!");
                assert(0);
            }
        }
    }
#endif
#ifdef DEBUG_DATAFLOW
    dr_fprintf(STDOUT, "trivial_entry_list size: %ld\n", trivial_entry_list->size());
    // output the analyzed trivial info into a file
    for(auto it=trivial_entry_list->begin(); it!=trivial_entry_list->end(); ++it) {
        DFGLog* log = getDFGLog((*it)->DFG_ctxt);
        // dr_mutex_lock(global_log_space.gLock);
        print_DFGLog(STDOUT, log);
        // dr_mutex_unlock(global_log_space.gLock);
    }
#endif
    if (!op_no_trace.get_value()) {
        // TODO: overhead optimization with fusion of same trivial tracing
        // individual trivial conditions
        for(auto it=pt->trivial_entry_list->begin(); it!=pt->trivial_entry_list->end(); ++it) {
            reg_id_t scratch_0, scratch_1;
            instr_t* instr = (*it)->trivial_entry.entry;
            RESERVE_REG(drcontext, bb, instr, NULL, scratch_0);
            RESERVE_REG(drcontext, bb, instr, NULL, scratch_1);
            assert((*it)->DFG_ctxt >=0);
            assert((*it)->DFG_ctxt <= DFGLogList_curr);
            insert_trace<false>(drcontext, bb, instr, (*it)->trivial_entry.opnd, (*it)->DFG_ctxt, (*it)->trivial_entry.size, (*it)->trivial_entry.esize, (*it)->trivial_entry.is_float, scratch_0, scratch_1);
            UNRESERVE_REG(drcontext, bb, instr, scratch_1);
            UNRESERVE_REG(drcontext, bb, instr, scratch_0);
        }
        // Combined trivial conditions will cause significant overhead
        // TODO: implement combined trivial condition checking with lower overhead
    }
    // insert label instruction into each trivial candidate's entry
    delete dfg;
    return DR_EMIT_DEFAULT;
}
#endif

static void
ThreadOutputFileInit(per_thread_out_file_t *pt)
{
    int32_t id = getThreadId();
    char name[MAXIMUM_PATH] = "";
    sprintf(name + strlen(name), "%s/thread-%d.log", g_folder_name.c_str(), id);
    pt->out_file = dr_open_file(name, DR_FILE_WRITE_OVERWRITE | DR_FILE_ALLOW_LARGE);
    DR_ASSERT(pt->out_file != INVALID_FILE);
}

static void
ClientThreadStart(void *drcontext)
{
    TrivialLoggerThreadInit(drcontext, op_enable_soft.get_value(), op_enable_hard.get_value(), op_no_trace.get_value());
    if (!op_no_trace.get_value()) {
        per_thread_out_file_t *pt = (per_thread_out_file_t *)dr_thread_alloc(drcontext, sizeof(per_thread_out_file_t));
        if (pt == NULL) {
            TRIVIALSPY_EXIT_PROCESS("pt == NULL");
        }
        drmgr_set_tls_field(drcontext, tls_idx, (void *)pt);
        // init output files
        ThreadOutputFileInit(pt);
    }
}

static void
ClientThreadEnd(void *drcontext)
{
    if (!op_no_trace.get_value()) {
        per_thread_out_file_t *pt = (per_thread_out_file_t *)drmgr_get_tls_field(drcontext, tls_idx);

        generateThreadReport(pt->out_file, global_log_space.trivial_metrics);
        if(op_enable_soft.get_value()) {
            generateThreadSoftApproxReport(pt->out_file, global_log_space.soft_approx_metrics);
        }
        if(op_enable_hard.get_value()) {
            generateThreadHardApproxReport(pt->out_file, global_log_space.hard_approx_metrics);
        }

        accumulateGlobalMetric(drcontext);

        dr_close_file(pt->out_file);
        dr_thread_free(drcontext, pt, sizeof(per_thread_out_file_t));
    }
    TrivialLoggerThreadFini(drcontext);
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
    sprintf(name + strlen(name), "-%d-trivialspy", pid);
    g_folder_name.assign(name, strlen(name));
    mkdir(g_folder_name.c_str(), S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH);

    double eps = op_enable_soft.get_value() ? op_epsilon.get_value() : 0;
    int bcnt = op_enable_hard.get_value() ? op_bit_count.get_value() : 0;

    char* detail_base = name + strlen(name);

    dr_fprintf(STDOUT, "[TRIVIALSPY INFO] Profiling result directory: %s\n", g_folder_name.c_str());

    sprintf(name+strlen(name), "/trivialspy.log");
    gFile = dr_open_file(name, DR_FILE_WRITE_OVERWRITE | DR_FILE_ALLOW_LARGE);
    DR_ASSERT(gFile != INVALID_FILE);

    std::string cmd = "";
    for(int i=0; i<argc; ++i) cmd += argv[i];
    dr_fprintf(gFile, "Running: %s\n", cmd.c_str());

    if (op_no_trace.get_value()) {
        dr_fprintf(STDOUT, "[TRIVIALSPY INFO] Profiling without value tracing\n");
        dr_fprintf(gFile, "[TRIVIALSPY INFO] Profiling without value tracing\n");
    } else {
        dr_fprintf(STDOUT, "[TRIVIALSPY INFO] Profiling with value tracing\n");
        dr_fprintf(gFile, "[TRIVIALSPY INFO] Profiling with value tracing\n");
    }

    if (op_enable_sampling.get_value()) {
        dr_fprintf(STDOUT, "[TRIVIALSPY INFO] Sampling Enabled\n");
        dr_fprintf(gFile, "[TRIVIALSPY INFO] Sampling Enabled\n");
        int win_enable = op_window_enable.get_value();
        int win_disable= op_window.get_value();
        trace_buf_enable_sampling(win_enable, win_disable);
        float rate = (float)win_enable / (float)win_disable;
        dr_fprintf(STDOUT, "[TRIVIALSPY INFO] Sampling Rate: %.3f, Window Size: %ld\n", rate, win_disable);
        dr_fprintf(gFile,  "[TRIVIALSPY INFO] Sampling Rate: %.3f, Window Size: %ld\n", rate, win_disable);
    } else {
        dr_fprintf(STDOUT, "[TRIVIALSPY INFO] Sampling Disabled\n");
        dr_fprintf(gFile, "[TRIVIALSPY INFO] Sampling Disabled\n");
        trace_buf_disable_sampling();
    }
    if (dr_using_all_private_caches()) {
        dr_fprintf(STDOUT, "[TRIVIALSPY INFO] Thread Private is enabled.\n");
        dr_fprintf(gFile,  "[TRIVIALSPY INFO] Thread Private is enabled.\n");
    } else {
        dr_fprintf(STDOUT, "[TRIVIALSPY INFO] Thread Private is disabled.\n");
        dr_fprintf(gFile,  "[TRIVIALSPY INFO] Thread Private is disabled.\n");
    }
    if (op_enable_soft.get_value()) {
        double eps = op_epsilon.get_value();
        dr_fprintf(STDOUT, "[TRIVIALSPY INFO] Soft Approximation is enabled. Epsilon=%le\n", eps);
        dr_fprintf(gFile, "[TRIVIALSPY INFO] Soft Approximation is enabled. Epsilon=%le\n", eps);
        set_approx_soft(eps);
    } else {
        dr_fprintf(STDOUT, "[TRIVIALSPY INFO] Soft Approximation is disabled\n");
        dr_fprintf(gFile, "[TRIVIALSPY INFO] Soft Approximation is disabled\n");
    }
    if (op_enable_hard.get_value()) {
        int bcnt = op_bit_count.get_value();
        dr_fprintf(STDOUT, "[TRIVIALSPY INFO] Hard Approximation is enabled. Bit Counts=%d\n", bcnt);
        dr_fprintf(gFile, "[TRIVIALSPY INFO] Hard Approximation is enabled. Bit Counts=%d\n", bcnt);
        set_approx_hard(bcnt);
    } else {
        dr_fprintf(STDOUT, "[TRIVIALSPY INFO] Hard Approximation is disabled\n");
        dr_fprintf(gFile, "[TRIVIALSPY INFO] Hard Approximation is disabled\n");
    }
    if (op_help.get_value()) {
        dr_fprintf(STDOUT, "%s\n", droption_parser_t::usage_long(DROPTION_SCOPE_CLIENT).c_str());
        exit(1);
    }
}

static void
ClientExit(void)
{
    if (!op_no_trace.get_value()) {
        generateGlobalReport(gFile, op_enable_soft.get_value(), op_enable_hard.get_value());
    } else {
        dr_fprintf(gFile, "--- static dataflow info ---\n");
        printAllDFGLog(gFile, true/*sorted*/, global_log_space.bb_ref);
    }

    dr_close_file(gFile);

    TrivialLoggerFini();
    if (!op_no_trace.get_value()) {
        TraceBufferFini();
    }

    freeDFGList();

    //drcctlib_exit();
    if (drsym_exit() != DRSYM_SUCCESS) {
        dr_fprintf(STDERR, "failed to exit drsym");
        exit(-1);
    }

    if (!drmgr_unregister_thread_init_event(ClientThreadStart) ||
        !drmgr_unregister_thread_exit_event(ClientThreadEnd) ||
        !drmgr_unregister_bb_instrumentation_event(event_basic_block_analysis) ||
        !drmgr_unregister_tls_field(tls_idx)) {
        printf("ERROR: trivialspy failed to unregister in ClientExit");
        fflush(stdout);
        exit(-1);
    }
    drwrap_exit();
    drutil_exit();
    drreg_exit();
    if (!op_no_trace.get_value()) {
        trace_exit();
    }
    drmgr_exit();

    TrivialOpTable::finiTrivialOpTable();
}

#ifdef __cplusplus
extern "C" {
#endif

DR_EXPORT void
dr_client_main(client_id_t id, int argc, const char *argv[])
{
    dr_set_client_name("DynamoRIO Client 'trivialspy'",
                       "http://dynamorio.org/issues");
    ClientInit(argc, argv);

    if (!drmgr_init()) {
        TRIVIALSPY_EXIT_PROCESS("ERROR: trivialspy unable to initialize drmgr");
    }

    drreg_options_t ops = { sizeof(ops), 4 /*max slots needed*/, false };
    // DRSYM will be initilized by CCTLib, so we don't need initilization here
    if (drreg_init(&ops) != DRREG_SUCCESS) {
        TRIVIALSPY_EXIT_PROCESS("ERROR: trivialspy unable to initialize drreg");
    }
    if (!drutil_init()) {
        TRIVIALSPY_EXIT_PROCESS("ERROR: trivialspy unable to initialize drutil");
    }
    if (!drwrap_init()) {
        TRIVIALSPY_EXIT_PROCESS("ERROR: trivialspy unable to initialize drwrap");
    }
    drmgr_priority_t thread_init_pri = { sizeof(thread_init_pri),
                                         "trivialspy-thread-init", NULL, NULL,
                                         DRCCTLIB_THREAD_EVENT_PRI + 1 };
    drmgr_priority_t thread_exit_pri = { sizeof(thread_exit_pri),
                                         "trivialspy-thread-exit", NULL, NULL,
                                         DRCCTLIB_THREAD_EVENT_PRI + 1 };
    if (!drmgr_register_thread_init_event_ex(ClientThreadStart, &thread_init_pri) ||
        !drmgr_register_thread_exit_event_ex(ClientThreadEnd, &thread_exit_pri) ||
        !drmgr_register_bb_instrumentation_event(event_basic_block_analysis, NULL, NULL)) {
        TRIVIALSPY_EXIT_PROCESS("ERROR: trivialspy unable to register events");
    }

    tls_idx = drmgr_register_tls_field();
    if (tls_idx == -1) {
        TRIVIALSPY_EXIT_PROCESS("ERROR: trivialspy drmgr_register_tls_field fail");
    }

    if (!op_no_trace.get_value()) {
        if(!trace_init()) {
            TRIVIALSPY_EXIT_PROCESS("ERROR: trivialspy unable to initialize trace");
        }
    }

    //drcctlib_init(DRCCTLIB_FILTER_ALL_INSTR, INVALID_FILE, InstrumentInsCallback, false/*do data centric*/);
    //drcctlib_init_ex(DRCCTLIB_FILTER_ALL_INSTR, INVALID_FILE, InstrumentInsCallback, NULL, NULL, DRCCTLIB_CACHE_MODE);
    if (drsym_init(0) != DRSYM_SUCCESS) {
        TRIVIALSPY_EXIT_PROCESS("ERROR: trivialspy drsym init fail");
    }
    dr_register_exit_event(ClientExit);

    if (!op_no_trace.get_value()) {
        // Tracing Buffer
        TraceBufferInit(op_enable_soft.get_value(), op_enable_hard.get_value());
    }
    TrivialLoggerInit(op_no_trace.get_value());

    TrivialOpTable::initTrivialOpTable();
}

#ifdef __cplusplus
}
#endif