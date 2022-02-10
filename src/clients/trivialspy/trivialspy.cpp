#include <unordered_map>
#include <vector>
#include <string>
#include <sys/stat.h>
#include <assert.h>
#include <algorithm>
#include <stddef.h>
#include <math.h>

#define MAX_PRINT 500
#define MAX_DEPTH 10
#define CACHE_NUM 4096

#ifdef DEBUG
// #define DEBUG_TRIVIALSPY
// #define TIMING
#endif

#ifdef TIMING
#include <iostream>
#include <ctime>
#include <chrono>
#endif

#include "dr_api.h"
#include "drmgr.h"
#include "drreg.h"
#include "drutil.h"
#include "drcctlib.h"
#include "vprofile.h"

#include "utils.h"
#include "trivial_report.h"
#include "trivial_logger.h"
#include "trivial_discovery.h"

// Client Options
#include "droption.h"

static droption_t<bool> op_no_trace
(DROPTION_SCOPE_CLIENT, "no_trace", 0, 0, 64, "Profile without value tracing",
 "Profile without value tracing for each dataflow graph.");

static droption_t<bool> op_help
(DROPTION_SCOPE_CLIENT, "help", 0, 0, 64, "Show this help",
 "Show this help.");

static droption_t<bool> op_enable_soft
(DROPTION_SCOPE_CLIENT, "enable_soft", 0, 0, 64, "Enable soft approximation.",
 "Enable soft approximation.");

static droption_t<bool> op_enable_hard
(DROPTION_SCOPE_CLIENT, "enable_hard", 0, 0, 64, "Enable hard approximation.",
 "Enable hard approximation.");

static droption_t<double> op_epsilon
(DROPTION_SCOPE_CLIENT, "epsilon", 1e-6, 0, 1, "Threshold for soft approximation.", "Threshold for soft approximation.");

// 0.00000095367431640625 ~ 1e-6
static droption_t<double> op_bit_count
(DROPTION_SCOPE_CLIENT, "bit_count", 20, 0, 64, "Approximated bit counts for hard approxiation.", "Approximated bit counts for hard approxiation.");

static droption_t<std::string> op_result_dir
(DROPTION_SCOPE_CLIENT, "r", "", "Result Directory",
 "Result directory for all profiling results");

using namespace std;

static string g_folder_name;
static int tls_idx;

file_t gFile;

vtrace_buffer_t* trace_buffer;

struct sidefunc_args_t {
    per_thread_log_t pt;
    file_t out;
    uint64_t* bb_ref;
    bool ready;
    bool clean;
};

struct per_thread_out_file_t {
    STCList* stc_list;
    file_t out_file;
    sidefunc_args_t* args;
};

#define HEAVY_THRESHOLD 5
#define MAX_CHAIN 5

struct cache_t {
    int32_t cct;
    int32_t bidx;
    int32_t stc_idx;
    uint8_t val[32];
};

/* Call backs to handle full trace buffers */
void trace_buf_full_cb(void *buf_base, void *buf_end, void* user_data) {
    cache_t* cache_ptr = (cache_t*)buf_base;
    cache_t* cache_end = (cache_t*)buf_end;
    int bidx = cache_ptr->bidx;
    int stc_idx;
    per_thread_log_t *pt = (per_thread_log_t *)drmgr_get_tls_field(dr_get_current_drcontext(), global_log_space.tls_idx);
    // use preserved stc_bmap to avoid frequent heavy memory allocations.
    std::vector<bool>& stc_bmap = *(pt->stc_bmap);
    int32_t cct;
    int n = 0;
    int stc_n = global_log_space.dfg_logs[bidx].n_stc;
    stc_bmap.clear();
    stc_bmap.resize(stc_n, 0);
    for(; cache_ptr<cache_end; ++cache_ptr, ++n) {
        // extract data from cache
        cct = cache_ptr->cct;
        DR_ASSERT(cct!=0);
        if(bidx!=cache_ptr->bidx || n==stc_n) {
            // as the cct handle of this bb is useless, we use caller handle for lower memory overhead
            cct = drcctlib_get_caller_handle(cct);
            auto it = pt->stc_counter[bidx].find(cct);
            if(it!=pt->stc_counter[bidx].end()) {
                auto it2 = (it->second).find(stc_bmap);
                if(it2!=(it->second).end()) {
                    ++(it2->second);
                } else {
                    (it->second)[stc_bmap] = 1;
                }
            } else {
                pt->stc_counter[bidx][cct][stc_bmap] = 1;
            }
#ifdef DEBUG_TRIVIALSPY
            auto it_new_cct = pt->stc_counter[bidx].find(cct);
            auto it_new = (it_new_cct->second).find(stc_bmap);
            DPRINTF("[TRIVIALSPY DEBUG] accumulated vector<bool> size=%d (stc_n=%d), value=%d\n", it_new->first.size(), stc_n, it_new->second);
            DR_ASSERT(it_new->first.size()==(size_t)stc_n);
            if(n!=stc_n) {
                dr_fprintf(STDOUT, "bb stc trace number not matched: n(%d), stc_n(%d)\n", n, stc_n);
            }
            DR_ASSERT_MSG(n==stc_n, "bb stc trace number not matched!");
#endif
            bidx = cache_ptr->bidx;
            n = 0;
            stc_n = global_log_space.dfg_logs[bidx].n_stc;
            stc_bmap.clear();
            stc_bmap.resize(stc_n, 0);
            IF_DEBUG_TRIVIAL(DR_ASSERT(stc_bmap.size()==(size_t)stc_n));
            IF_DEBUG_TRIVIAL(DR_ASSERT(global_log_space.dfg_logs[bidx].stc_list->size()==(size_t)stc_n));
        }
        int stc_idx = cache_ptr->stc_idx;
#ifdef DEBUG_TRIVIALSPY
        IF_DEBUG_TRIVIAL(
            if(stc_bmap.size()!=global_log_space.dfg_logs[bidx].stc_list->size()) {
                DPRINTF("[TRIVIALSPY DEBUG] STC_BMAP(%d) and STC_LIST(%d) size not matched!\n", 
                    stc_bmap.size(), global_log_space.dfg_logs[bidx].stc_list->size());
            }
        )
        DR_ASSERT(stc_bmap.size()==global_log_space.dfg_logs[bidx].stc_list->size());
        dr_fprintf(STDOUT, "bidx=%d, stc_idx=%d, stc_n=%d\n", bidx, stc_idx, stc_n);
        if(stc_n==0) {
            dr_fprintf(STDOUT, "STC_N is zero: bidx=%d, cache_ptr=%p, cache_base=%p, i=%ld, cache_end=%p, n=%ld\n", bidx, cache_ptr, buf_base, (uint64_t)cache_ptr-(uint64_t)buf_base, cache_end, (uint64_t)cache_end-(uint64_t)buf_base);
            dr_fprintf(STDOUT, "==> DFG=%p, stc_list=%p:\n", global_log_space.dfg_logs[bidx].dfg, global_log_space.dfg_logs[bidx].stc_list);
            global_log_space.dfg_logs[bidx].dfg->print(STDOUT);
        }
        DR_ASSERT(stc_n>0);
        if(stc_bmap.size()!=(size_t)stc_n) {
            dr_fprintf(STDOUT, "STC_BMAP size not matched: size(%d), stc_n(%d)\n", stc_bmap.size(), stc_n);
        }
        DR_ASSERT(stc_bmap.size()==(size_t)stc_n);
        if((size_t)stc_idx>=stc_bmap.size()) {
            dr_fprintf(STDOUT, "STC_BMAP idx exceed: size(%d), stc_idx(%d)\n", stc_bmap.size(), stc_idx);
        }
        DR_ASSERT((size_t)stc_idx<stc_bmap.size());
#endif
        stc_bmap[stc_idx] = global_log_space.dfg_logs[bidx]
                                .detect[stc_idx]((void*)cache_ptr->val);
    }
    // handle the last cache
    DR_ASSERT(cct!=0);
    cct = drcctlib_get_caller_handle(cct);
    auto it = pt->stc_counter[bidx].find(cct);
    if(it!=pt->stc_counter[bidx].end()) {
        auto it2 = (it->second).find(stc_bmap);
        if(it2!=(it->second).end()) {
            ++(it2->second);
        } else {
            (it->second)[stc_bmap] = 1;
        }
    } else {
        pt->stc_counter[bidx][cct][stc_bmap] = 1;
    }
#ifdef DEBUG_TRIVIALSPY
    auto it_new_cct = pt->stc_counter[bidx].find(cct);
    DR_ASSERT(it_new_cct!=pt->stc_counter[bidx].end());
    auto it_new = (it_new_cct->second).find(stc_bmap);
    DR_ASSERT(it_new!=(it_new_cct->second).end());
    DPRINTF("[TRIVIALSPY DEBUG] accumulated vector<bool> size=%d (stc_n=%d), value=%d\n", it_new->first.size(), stc_n, it_new->second);
    DR_ASSERT(it_new->first.size()==(size_t)stc_n);
    if(n!=stc_n) {
        dr_fprintf(STDOUT, "bb stc trace number not matched: n(%d), stc_n(%d)\n", n, stc_n);
    }
    DR_ASSERT_MSG(n==stc_n, "bb stc trace number not matched!");
#endif
    return;
}

size_t trace_buf_fill_num_cb(void *drcontext, instr_t *where, void* user_data) {
    if(!instr_is_app(where) || instr_is_ignorable(where)) return 0;
    per_thread_out_file_t *pt = (per_thread_out_file_t *)drmgr_get_tls_field(drcontext, tls_idx);
    size_t fill_num = 0;
    STCList& stc_list = *(pt->stc_list);
    int n_stc = stc_list.size();
    for(int i=0; i<n_stc; ++i) {
        if(stc_list[i].stc->target->ins==where) {
            fill_num += sizeof(cache_t);
        }
    }
    return fill_num;
}

// dataflow-only profiling without value tracing
dr_emit_flags_t
event_basic_block_analysis_no_trace(void *drcontext, void *tag, instrlist_t *bb, bool for_trace, bool translating, OUT void **user_data)
{
#ifdef TIMING
    auto start = std::chrono::system_clock::now();
#endif    
    int bidx = get_bb_idx();
    DFG* dfg = new DFG(bb);
#ifdef DEBUG_TRIVIALSPY
    dr_fprintf(STDOUT, "BB DFG ins: %d\n", dfg->get_ins_count());
    dr_fprintf(STDOUT, "BB DFG mem: %d\n", dfg->get_mem_count());
    dr_fprintf(STDOUT, "BB DFG:\n");
    dfg->print(STDOUT);
#endif
    // slice DFG to several trivial candidates (trivial sub-DFG with trivial condition)
    discoverIndividualSTC(bidx, dfg, NULL, HEAVY_THRESHOLD);
    discoverCombinedSTC(bidx, dfg, NULL, HEAVY_THRESHOLD, MAX_CHAIN);

    BBUpdateInstrumentation(drcontext, bb, instrlist_first(bb), bidx);
    delete dfg;
#ifdef TIMING
    auto end = std::chrono::system_clock::now();
    std::chrono::duration<double> elapsed_seconds = end-start;
    std::time_t end_time = std::chrono::system_clock::to_time_t(end);
    std::cerr << "[bidx=" << bidx << "]: finished computation at " << std::ctime(&end_time)
              << "elapsed time: " << elapsed_seconds.count() << "s\n";
#endif
    return DR_EMIT_DEFAULT;
}

void getUnusedRegMap(drvector_t* allowed, opnd_t opnd) {
    drreg_init_and_fill_vector(allowed, true);
    for (int i = opnd_num_regs_used(opnd) - 1; i >= 0; i--) {
        reg_id_t reg_used = opnd_get_reg_used(opnd, i);
        // resize for simd or gpr, mmx regs are not supported!
        // resize for simd may occur error
        if(!reg_is_gpr(reg_used)) {
            continue;
        }
        if(reg_is_simd(reg_used) || reg_is_mmx(reg_used)) {
            drreg_set_vector_entry(allowed, reg_used, false);
            continue;
        }
        drreg_set_vector_entry(allowed, reg_resize_to_opsz(reg_used, OPSZ_1), false);
        drreg_set_vector_entry(allowed, reg_resize_to_opsz(reg_used, OPSZ_2), false);
        drreg_set_vector_entry(allowed, reg_resize_to_opsz(reg_used, OPSZ_4), false);
        drreg_set_vector_entry(allowed, reg_resize_to_opsz(reg_used, OPSZ_8), false);
    }
}

// Value tracing for singular trivial variable
dr_emit_flags_t
event_basic_block_analysis(void *drcontext, void *tag, instrlist_t *bb, bool for_trace, bool translating, OUT void **user_data)
{
#ifdef DEBUG_TRIVIALSPY
    dr_fprintf(STDOUT, "[TRIVIALSPY DEBUG] event_basic_block_analysis triggered\n");
#endif
    per_thread_out_file_t *pt = (per_thread_out_file_t *)drmgr_get_tls_field(drcontext, tls_idx);
    // obtain a thread-safe unique bb identifier for the captured basic block
    int bidx = get_bb_idx();
    // instrumentation for bb counting
    BBUpdateInstrumentation(drcontext, bb, instrlist_first(bb), bidx);
    // forward analysis to construct DFG from the captured basic block
    DFG* dfg = new DFG(bb);
    // pass the analysis result to registered call backs via tls
    pt->stc_list = new STCList();
#ifdef DEBUG_DATAFLOW
    dr_fprintf(STDOUT, "BB DFG ins: %d\n", dfg->get_ins_count());
    dr_fprintf(STDOUT, "BB DFG mem: %d\n", dfg->get_mem_count());
    dr_fprintf(STDOUT, "BB DFG:\n");
    dfg->print(STDOUT);
#endif
    // discover all singular trivial conditions within this DFG
    // the DFG is shared within all captured stc logs
    discoverSTC(bidx, dfg, pt->stc_list, HEAVY_THRESHOLD, MAX_CHAIN);
    // cache all detected potential STCs into the logs
    STCList& stc_list = *(pt->stc_list);
    int n_stc = stc_list.size();
    global_log_space.dfg_logs[bidx].dfg = dfg;
    global_log_space.dfg_logs[bidx].n_stc = n_stc;
    global_log_space.dfg_logs[bidx].stc_list = pt->stc_list;
    global_log_space.dfg_logs[bidx].trivial_summary_cache = new TrivialSummaryMap();
    if(n_stc) {
        global_log_space.dfg_logs[bidx].detect = new trivial_detector_t[n_stc];
        // instrumentation for stc value tracing: bidx, stc_idx, value
        for(int i=0; i<n_stc; ++i) {
            opnd_t ref = stc_list[i].stc->opnd;
            instr_t* instr = stc_list[i].stc->target->ins;
            // skip if the instruction is not application code or it is ignorable
            if(!instr_is_app(instr) || instr_is_ignorable(instr)) {
                // our DFG analysis results should not include any instr that is 1) not app instr, or 2) ignorable
                DR_ASSERT(0);
                continue;
            }
            // statically determine the detect helper function
            // TODO: support soft/hard approximated detector
            global_log_space.dfg_logs[bidx].detect[i] = getTrivialDetector(stc_list[i].cond, stc_list[i].stc->info, TRIVIAL_DETECTOR_EXACT);
            // prepare for value tracing
            reg_id_t reg_ptr, scratch, reg_cct;
            drvector_t allowed;
            getUnusedRegMap(&allowed, ref);
            RESERVE_REG(drcontext, bb, instr, &allowed, reg_ptr);
            RESERVE_REG(drcontext, bb, instr, &allowed, scratch);
            RESERVE_REG(drcontext, bb, instr, &allowed, reg_cct);
            drvector_delete(&allowed);
#ifdef DEBUG_TRIVIALSPY
            DR_ASSERT(global_log_space.dfg_logs[bidx].n_stc>0);
#endif
            // value tracing
            drcctlib_get_context_handle_in_reg(drcontext, bb, instr, 0, reg_cct/*store_reg*/, scratch/*scratch*/);
            vtracer_get_trace_buffer_in_reg(drcontext, instr, bb, trace_buffer, reg_ptr);
            vtracer_insert_trace_val(drcontext, instr, bb, opnd_create_reg(reg_cct), reg_ptr, scratch, offsetof(cache_t, cct));
            vtracer_insert_trace_val(drcontext, instr, bb, OPND_CREATE_INT32(bidx), reg_ptr, scratch, offsetof(cache_t, bidx));
            vtracer_insert_trace_val(drcontext, instr, bb, OPND_CREATE_INT32(i), reg_ptr, scratch, offsetof(cache_t, stc_idx));
            vtracer_insert_trace_val(drcontext, instr, bb, ref, reg_ptr, scratch, offsetof(cache_t, val));
            vtracer_insert_trace_forward(drcontext, instr, bb, sizeof(cache_t), trace_buffer, reg_ptr, scratch);
            // free the reserved registers
            UNRESERVE_REG(drcontext, bb, instr, reg_ptr);
            UNRESERVE_REG(drcontext, bb, instr, scratch);
            UNRESERVE_REG(drcontext, bb, instr, reg_cct);
        }
    } else {
        global_log_space.dfg_logs[bidx].detect = NULL;
    }
#ifdef DEBUG_TRIVIALSPY
    dr_fprintf(STDOUT, "[TRIVIALSPY DEBUG] event_basic_block_analysis exit\n");
#endif
    return DR_EMIT_DEFAULT;
}

void
InstrumentInsCallback(void *drcontext, instr_instrument_msg_t *instrument_msg)
{
    // extract data from drcctprof's given message
    instrlist_t *bb = instrument_msg->bb;
    instr_t *instr = instrument_msg->instr;
    int32_t slot = instrument_msg->slot;
    if(slot==0) {
        event_basic_block_analysis(drcontext, NULL, bb, false, false, NULL);
    }
}

static void
ThreadOutputFileInit(per_thread_out_file_t *pt)
{
    int32_t id = getThreadId();
    char name[MAXIMUM_PATH] = "";
    sprintf(name + strlen(name), "%s/thread-%d.log", g_folder_name.c_str(), id);
    pt->out_file = dr_open_file(name, DR_FILE_WRITE_OVERWRITE | DR_FILE_ALLOW_LARGE);
    DR_ASSERT(pt->out_file != INVALID_FILE);
}

void threadSideWork(void* args) {
    sidefunc_args_t* data = (sidefunc_args_t*)args;
    dr_fprintf(STDOUT, "[TrivialSpy] Running Side Work Helper Thread for Thread %d\n", data->pt.threadId);
    while(!data->ready) dr_sleep(100);
    dr_fprintf(STDOUT, "[TrivialSpy] Generate Profile Report for Thread %d\n", data->pt.threadId);
    generateThreadReport(&(data->pt), data->bb_ref, data->out, gFile, HEAVY_THRESHOLD, MAX_PRINT);
    accumulateGlobalMetric(&(data->pt));
    dr_close_file(data->out);

    dr_fprintf(STDOUT, "[TrivialSpy] Cleaning for Thread %d\n", data->pt.threadId);
    data->ready = false;
    TrivialLoggerThreadFini_impl(&(data->pt), data->bb_ref);

    dr_fprintf(STDOUT, "[TrivialSpy] Exit Side Work Helper Thread for Thread %d\n", data->pt.threadId);
    data->clean = true;
}

std::vector<sidefunc_args_t*> arg_list;

static void
ClientThreadStart(void *drcontext)
{
    TrivialLoggerThreadInit(drcontext, op_enable_soft.get_value(), op_enable_hard.get_value(), op_no_trace.get_value());
    if (!op_no_trace.get_value()) {
        per_thread_out_file_t *pt = (per_thread_out_file_t *)dr_thread_alloc(drcontext, sizeof(per_thread_out_file_t));
        if (pt == NULL) {
            TRIVIALSPY_EXIT_PROCESS("pt == NULL");
        }
        pt->stc_list = NULL;
        drmgr_set_tls_field(drcontext, tls_idx, (void *)pt);
        // init output files
        ThreadOutputFileInit(pt);
        
        pt->args = (sidefunc_args_t*)dr_global_alloc(sizeof(sidefunc_args_t));
        per_thread_log_t *log_pt = (per_thread_log_t *)drmgr_get_tls_field(drcontext, global_log_space.tls_idx);
        memcpy(&(pt->args->pt), log_pt, sizeof(per_thread_log_t));
        pt->args->ready = false;
        pt->args->clean = false;
        pt->args->out = pt->out_file;
        pt->args->bb_ref = get_bb_ref(drcontext);
        arg_list.push_back(pt->args);
        //dr_create_client_thread(threadSideWork, (void*)pt->args);
    }
}

static void
ClientThreadEnd(void *drcontext)
{
#ifdef DEBUG_TRIVIALSPY
    dr_fprintf(STDOUT, "[TRIVIALSPY DEBUG] ClientThreadEnd triggered\n");
#endif
    if (!op_no_trace.get_value()) {
        per_thread_out_file_t *pt = (per_thread_out_file_t *)drmgr_get_tls_field(drcontext, tls_idx);
        pt->args->ready = true;
        threadSideWork((void*)pt->args);
        dr_thread_free(drcontext, pt, sizeof(per_thread_out_file_t));
    }
    // TODO: dump compressed binary profiles for GUI presentation
    // if (!op_no_trace.get_value()) {
    //     per_thread_out_file_t *pt = (per_thread_out_file_t *)drmgr_get_tls_field(drcontext, tls_idx);
    //     // generateThreadReport(drcontext, pt->out_file, gFile, HEAVY_THRESHOLD, MAX_PRINT);
    //     // if(op_enable_soft.get_value()) {
    //     //     generateThreadSoftApproxReport(pt->out_file, global_log_space.soft_approx_metrics);
    //     // }
    //     // if(op_enable_hard.get_value()) {
    //     //     generateThreadHardApproxReport(pt->out_file, global_log_space.hard_approx_metrics);
    //     // }

    //     //accumulateGlobalMetric(drcontext);
    //     // dr_close_file(pt->out_file);
    //     dr_thread_free(drcontext, pt, sizeof(per_thread_out_file_t));
    // }
    TrivialLoggerThreadFini(drcontext, !op_no_trace.get_value());
#ifdef DEBUG_TRIVIALSPY
    dr_fprintf(STDOUT, "[TRIVIALSPY DEBUG] ClientThreadEnd exit\n");
#endif
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
    if(!op_result_dir.get_value().empty()) {
        // if the user defined result directory, try to create if not exist
        g_folder_name = op_result_dir.get_value();
        mkdir(g_folder_name.c_str(), S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH);
        g_folder_name += std::string("/");
    } else {
        g_folder_name.clear();
    }
    pid_t pid = getpid();
#ifdef ARM_CCTLIB
    char name[MAXIMUM_PATH] = "arm-";
#else
    char name[MAXIMUM_PATH] = "x86-";
#endif
    gethostname(name + strlen(name), MAXIMUM_PATH - strlen(name));
    sprintf(name + strlen(name), "-%d-trivialspy", pid);
    g_folder_name += std::string(name, strlen(name));
    //g_folder_name.assign(name, strlen(name));
    mkdir(g_folder_name.c_str(), S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH);

    double eps = op_enable_soft.get_value() ? op_epsilon.get_value() : 0;
    int bcnt = op_enable_hard.get_value() ? op_bit_count.get_value() : 0;

    char* detail_base = name + strlen(name);

    dr_fprintf(STDOUT, "[TRIVIALSPY INFO] Profiling result directory: %s\n", g_folder_name.c_str());

    // sprintf(name+strlen(name), "/trivialspy.log");
    sprintf(name, "%s/trivialspy.log", g_folder_name.c_str());
    gFile = dr_open_file(name, DR_FILE_WRITE_OVERWRITE | DR_FILE_ALLOW_LARGE);
    DR_ASSERT(gFile != INVALID_FILE);

    std::string cmd = "";
    for(int i=0; i<argc; ++i) cmd += argv[i] + std::string(" ");
    dr_fprintf(gFile, "Running: %s\n", cmd.c_str());

    if (op_no_trace.get_value()) {
        dr_fprintf(STDOUT, "[TRIVIALSPY INFO] Profiling without value tracing\n");
        dr_fprintf(gFile, "[TRIVIALSPY INFO] Profiling without value tracing\n");
    } else {
        dr_fprintf(STDOUT, "[TRIVIALSPY INFO] Profiling with value tracing\n");
        dr_fprintf(gFile, "[TRIVIALSPY INFO] Profiling with value tracing\n");
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
        // TODO: implement soft approximation
        DR_ASSERT_MSG(0, "Soft Approximation currently not supported!");
    } else {
        dr_fprintf(STDOUT, "[TRIVIALSPY INFO] Soft Approximation is disabled\n");
        dr_fprintf(gFile, "[TRIVIALSPY INFO] Soft Approximation is disabled\n");
    }
    if (op_enable_hard.get_value()) {
        int bcnt = op_bit_count.get_value();
        dr_fprintf(STDOUT, "[TRIVIALSPY INFO] Hard Approximation is enabled. Bit Counts=%d\n", bcnt);
        dr_fprintf(gFile, "[TRIVIALSPY INFO] Hard Approximation is enabled. Bit Counts=%d\n", bcnt);
        set_approx_hard(bcnt);
        // TODO: implement hard approximation
        DR_ASSERT_MSG(0, "Hard Approximation currently not supported!");
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
event_module_load_analysis(void *drcontext, const module_data_t *info, bool loaded)
{
    register_trivial_func_entry(info);
}

static void
event_module_unload_analysis(void *drcontext, const module_data_t *info)
{
}

static void
ClientExit(void)
{
    // TODO: dump compressed binary profiles for GUI presentation
    if (!op_no_trace.get_value()) {
        dr_fprintf(STDOUT, "Waiting for reporting threads...\n");
        for(size_t i=0; i<arg_list.size(); ++i) {
            while(arg_list[i]->ready) dr_sleep(100);
        }
        dr_fprintf(STDOUT, "Thread reporting all finished!\n");
        generateGlobalReport(gFile, op_enable_soft.get_value(), op_enable_hard.get_value());
    } else {
        dr_fprintf(gFile, "--- static dataflow info ---\n");
        printAllDFGLog(gFile, true/*sorted*/, global_log_space.bb_ref);
    }

    dr_close_file(gFile);

    if (!op_no_trace.get_value()) {
        dr_fprintf(STDOUT, "Waiting for cleaning reporting threads...\n");
        for(size_t i=0; i<arg_list.size(); ++i) {
            while(!arg_list[i]->clean) dr_sleep(100);
            dr_global_free(arg_list[i], sizeof(sidefunc_args_t));
        }
        dr_fprintf(STDOUT, "All helper threads cleaned!\n");
    }

    TrivialLoggerFini();
    freeDFGList();
    free_info_list();

    if (op_no_trace.get_value()) {
        if (drsym_exit() != DRSYM_SUCCESS) {
            dr_fprintf(STDERR, "failed to exit drsym");
            exit(-1);
        }
        if(!drmgr_unregister_bb_instrumentation_event(event_basic_block_analysis_no_trace)) {
            printf("ERROR: trivialspy failed to unregister bb event in ClientExit");
            fflush(stdout);
            exit(-1);
        }
    } else {
        // if(!drmgr_unregister_bb_instrumentation_event(event_basic_block_analysis)) {
        //     printf("ERROR: trivialspy failed to unregister bb event in ClientExit");
        //     fflush(stdout);
        //     exit(-1);
        // }
        drmgr_unregister_module_load_event(event_module_load_analysis);
        drmgr_unregister_module_unload_event(event_module_unload_analysis);
        drcctlib_exit();
    }

    if (!drmgr_unregister_thread_init_event(ClientThreadStart) ||
        !drmgr_unregister_thread_exit_event(ClientThreadEnd) ||
        !drmgr_unregister_tls_field(tls_idx)) {
        printf("ERROR: trivialspy failed to unregister in ClientExit");
        fflush(stdout);
        exit(-1);
    }
    if (!op_no_trace.get_value()) {
        vtracer_buffer_free(trace_buffer);
        vtracer_exit();
    }
    drutil_exit();
    drreg_exit();
    drmgr_exit();
    TrivialFuncTable::fini();
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
    drmgr_priority_t thread_init_pri = { sizeof(thread_init_pri),
                                         "trivialspy-thread-init", NULL, NULL,
                                         DRMGR_PRIORITY_THREAD_INIT_TRACE_BUF + 1 };
    drmgr_priority_t thread_exit_pri = { sizeof(thread_exit_pri),
                                         "trivialspy-thread-exit", NULL, NULL,
                                         DRMGR_PRIORITY_THREAD_EXIT_TRACE_BUF + 1 };
    if (!drmgr_register_thread_init_event_ex(ClientThreadStart, &thread_init_pri) ||
        !drmgr_register_thread_exit_event_ex(ClientThreadEnd, &thread_exit_pri)) {
        TRIVIALSPY_EXIT_PROCESS("ERROR: trivialspy unable to register events");
    }

    tls_idx = drmgr_register_tls_field();
    if (tls_idx == -1) {
        TRIVIALSPY_EXIT_PROCESS("ERROR: trivialspy drmgr_register_tls_field fail");
    }

    if (!op_no_trace.get_value()) {
        //drcctlib_init_ex(DRCCTLIB_FILTER_ALL_INSTR, INVALID_FILE, InstrumentInsCallback, NULL, NULL, DRCCTLIB_CACHE_MODE);
        drcctlib_init_ex(DRCCTLIB_FILTER_ALL_INSTR, INVALID_FILE, InstrumentInsCallback, NULL, NULL, DRCCTLIB_DEFAULT);
        // if(!drmgr_register_bb_instrumentation_event(event_basic_block_analysis, NULL, NULL)) {
        //     TRIVIALSPY_EXIT_PROCESS("ERROR: trivialspy unable to register events");
        // }
        if(!vtracer_init()) {
            TRIVIALSPY_EXIT_PROCESS("ERROR: trivialspy unable to initialize vtracer");
        }
        // Tracing Buffer must created after registering bb analysis to ensure the analysis result is ready
        trace_buffer = vtracer_create_trace_buffer_ex(CACHE_NUM*sizeof(cache_t), trace_buf_full_cb, NULL, trace_buf_fill_num_cb, NULL);
        drmgr_priority_t module_load_pri = { sizeof(module_load_pri), "trivialspy-module_load",
                                         NULL, NULL, DRCCTLIB_MODULE_REGISTER_PRI };
        drmgr_priority_t module_unload_pri = { sizeof(module_unload_pri), "trivialspy-module_unload",
                                            NULL, NULL, DRCCTLIB_MODULE_REGISTER_PRI };
        drmgr_register_module_load_event_ex(event_module_load_analysis, &module_load_pri);
        drmgr_register_module_unload_event_ex(event_module_unload_analysis, &module_unload_pri);
        drwrap_set_global_flags(DRWRAP_NO_FRILLS);
        drwrap_set_global_flags(DRWRAP_FAST_CLEANCALLS);
    } else {
        if(!drmgr_register_bb_instrumentation_event(event_basic_block_analysis_no_trace, NULL, NULL)) {
            TRIVIALSPY_EXIT_PROCESS("ERROR: trivialspy unable to register events");
        }
        if (drsym_init(0) != DRSYM_SUCCESS) {
            TRIVIALSPY_EXIT_PROCESS("ERROR: trivialspy drsym init fail");
        }
    }

    dr_register_exit_event(ClientExit);

    TrivialLoggerInit();

    init_trivial_func_list();
    TrivialFuncTable::init();
    TrivialOpTable::initTrivialOpTable();
}

#ifdef __cplusplus
}
#endif