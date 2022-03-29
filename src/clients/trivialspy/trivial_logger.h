#ifndef __TRIVIAL_LOGGER_H__
#define __TRIVIAL_LOGGER_H__

#include <stdint.h>
#include <vector>
#include <unordered_map>

#include "drwrap.h"

#include "trivial_define.h"
#include "trivial_detector.h"
#include "trivial_discovery.h"

typedef std::unordered_map<std::vector<bool>, uint64_t> TrivialMap;
typedef std::unordered_map<std::vector<bool>, DFGSummary*> TrivialSummaryMap;

typedef std::unordered_map<int32_t, TrivialMap> TrivialCCTMap;

struct dfglog_t {
    DFG* dfg;
    int n_stc;
    STCList* stc_list;
    trivial_detector_t* detect;
    TrivialSummaryMap* trivial_summary_cache;
};

void free_trivial_summary_cache(TrivialSummaryMap* cache) {
    for(auto it=cache->begin(); it!=cache->end(); ++it) {
        DFGSummary_free(it->second);
    }
    delete cache;
}

struct metric_t {
    int64_t SB;
    int64_t BB;
    uint64_t heavy;
    uint64_t chained;
    uint64_t bwd_slice;
    uint64_t combined;
};

struct log_space_t {
    int tls_idx;
    uint tls_offs;
    reg_id_t tls_seg;
    void *gLock;
    int thread_id_max;
    uint64_t totalCost;
    metric_t trivial_metrics;
    metric_t soft_approx_metrics;
    metric_t hard_approx_metrics;
    uint64_t* bb_ref;
    dfglog_t* dfg_logs;
};

static log_space_t global_log_space;

enum {
    RAW_LOG_BB_REF_COUNTS,
    RAW_LOG_COUNTS
};

struct trivial_func_desc_t {
    std::string name;
    int entry;
    void (*pre_func_cb)(void *, void **);
    DFGSummary* dfg_s;
};

struct func_log_t {
    int idx;
    uint64_t total;
    uint64_t trivial;
};

std::vector<trivial_func_desc_t> trivial_func_list;
typedef std::unordered_map<int32_t/*cct*/, func_log_t> TrivialFuncMap;

struct per_thread_log_t {
    byte* seg_base;
    TrivialCCTMap* stc_counter;
    std::vector<bool>* stc_bmap;
    TrivialFuncMap* trivial_func_tmap;
    int threadId;
    uint64_t total_cost;
    uint64_t SB;
    uint64_t BB;
    uint64_t heavy;
    uint64_t chained;
    uint64_t bwd_slice;
    uint64_t combined;
};

static int _bb_idx = -1;
static int get_bb_idx() {
    DR_ASSERT_MSG(_bb_idx<MAX_DFGLOG_NUM, "bb number exceed the preallocated buffer");
    return __sync_add_and_fetch(&_bb_idx, 1);
}

inline __attribute__((always_inline))
static uint64_t* get_bb_ref(void* drcontext) {
    per_thread_log_t *pt = (per_thread_log_t *)drmgr_get_tls_field(drcontext, global_log_space.tls_idx);
    return (uint64_t*)BUF_PTR(pt->seg_base, global_log_space.tls_offs);
}

static void update_global_bb_ref(uint64_t* bb_ref) {
    for(int i=0; i<MAX_DFGLOG_NUM; ++i) {
        if(bb_ref[i]) {
            __sync_fetch_and_add(&global_log_space.bb_ref[i], bb_ref[i]);
        }
    }
}

static void update_global_bb_ref_from_tls(void* drcontext) {
    per_thread_log_t *pt = (per_thread_log_t *)drmgr_get_tls_field(drcontext, global_log_space.tls_idx);
    uint64_t* bb_ref = (uint64_t*)BUF_PTR(pt->seg_base, global_log_space.tls_offs);
    update_global_bb_ref(bb_ref);
}

// aflag-free instrumentation for bb counting
static void BBUpdateInstrumentation(void* drcontext, instrlist_t* ilist, instr_t* where, int bidx) {
    per_thread_log_t *pt = (per_thread_log_t *)drmgr_get_tls_field(drcontext, global_log_space.tls_idx);
    reg_id_t reg_ptr, scratch;
    RESERVE_REG(drcontext, ilist, where, NULL, reg_ptr);
    RESERVE_REG(drcontext, ilist, where, NULL, scratch);
    // thread local bb_ref pointer
    dr_insert_read_raw_tls(drcontext, ilist, where, global_log_space.tls_seg, global_log_space.tls_offs, reg_ptr);
    // ++bb_ref[bidx]
    opnd_t bb_cnt = opnd_create_reg(scratch);
    opnd_t bb_ref = OPND_CREATE_MEMPTR(reg_ptr, bidx*sizeof(uint64_t));
    MINSERT(ilist, where, XINST_CREATE_load(drcontext, bb_cnt, bb_ref));
    MINSERT(ilist, where, XINST_CREATE_add(drcontext, bb_cnt, OPND_CREATE_INT32(1)));
    MINSERT(ilist, where, XINST_CREATE_store(drcontext, bb_ref, bb_cnt));
    // unreserve regs
    UNRESERVE_REG(drcontext, ilist, where, reg_ptr);
    UNRESERVE_REG(drcontext, ilist, where, scratch);
}

static inline bool
register_func_entry(const module_data_t *info, const char *func_name, int entry, 
                    void (*pre_func_cb)(void *wrapcxt,
                                        INOUT void **user_data))
{
    app_pc func_entry = (app_pc)dr_get_proc_address(info->handle, func_name);
    if (func_entry != NULL) {
        return drwrap_wrap(func_entry, pre_func_cb, NULL);
    } else {
        return false;
    }
    return true;
}

static inline
void register_trivial_func_entry(const module_data_t *info)
{
    for(auto it=trivial_func_list.begin(); it!=trivial_func_list.end(); ++it) {
        IF_DEBUG_TRIVIAL(bool suc=) register_func_entry(info, (*it).name.c_str(), (*it).entry, (*it).pre_func_cb);
        DPRINTF("Registering trivial func entry %s @ %s : %s\n", (*it).name.c_str(), info->full_path, suc?"SUCCESS":"FAILED");
    }
}

#ifdef ARM
    #error "ARM not supported!"
#endif
// check 1st arg with class T is zero, X86 only
template<class T, int idx>
void pre_func_cb_1Z(void *wrapcxt, INOUT void **user_data) {
    void *drcontext = (void *)drwrap_get_drcontext(wrapcxt);
    per_thread_log_t *pt = (per_thread_log_t *)drmgr_get_tls_field(drcontext, global_log_space.tls_idx);
    TrivialFuncMap& trivial_func_tmap = *(pt->trivial_func_tmap);
    dr_mcontext_t* mc = drwrap_get_mcontext_ex(wrapcxt, DR_MC_MULTIMEDIA/*only need xmm0*/);
    T value = reinterpret_cast<T*>(mc->simd[0].u8/*XMM0*/)[0];
    DPRINTF("LOADING VALUE: %lf, sizeof(T)=%d, idx=%d, func=%s\n", value, sizeof(T), idx, trivial_func_list[idx].name.c_str());
    context_handle_t cct = drcctlib_get_context_handle(drcontext);
    // as the cct handle of this bb is useless, we use caller handle for lower memory overhead
    //cct = drcctlib_get_caller_handle(cct);
    auto it = trivial_func_tmap.find(cct);
    if(it!=trivial_func_tmap.end()) {
        if(value==(T)0) {
            it->second.trivial++;
        }
        it->second.total++;
        DPRINTF("\tACCUMULATE: total=%ld, trivial=%ld\n", it->second.total, it->second.trivial);
    } else {
        func_log_t func_log;
        func_log.idx = idx;
        if(value==(T)0) {
            func_log.trivial = 1;            
        } else {
            func_log.trivial = 0;
        }
        func_log.total = 1;
        trivial_func_tmap[cct] = func_log;
        DPRINTF("\tINSERT: total=%ld, trivial=%ld\n", func_log.total, func_log.trivial);
    }
}

template<class T, int idx>
void pre_func_cb_1O(void *wrapcxt, INOUT void **user_data) {
    void *drcontext = (void *)drwrap_get_drcontext(wrapcxt);
    per_thread_log_t *pt = (per_thread_log_t *)drmgr_get_tls_field(drcontext, global_log_space.tls_idx);
    TrivialFuncMap& trivial_func_tmap = *(pt->trivial_func_tmap);
    dr_mcontext_t* mc = drwrap_get_mcontext_ex(wrapcxt, DR_MC_MULTIMEDIA/*only need xmm0*/);
    T value = reinterpret_cast<T*>(mc->simd[0].u8/*XMM0*/)[0];
    DPRINTF("LOADING VALUE: %lf, sizeof(T)=%d, idx=%d, func=%s\n", value, sizeof(T), idx, trivial_func_list[idx].name.c_str());
    context_handle_t cct = drcctlib_get_context_handle(drcontext);
    // as the cct handle of this bb is useless, we use caller handle for lower memory overhead
    //cct = drcctlib_get_caller_handle(cct);
    auto it = trivial_func_tmap.find(cct);
    if(it!=trivial_func_tmap.end()) {
        if(value==(T)1) {
            it->second.trivial++;
        }
        it->second.total++;
        DPRINTF("\tACCUMULATE: total=%ld, trivial=%ld\n", it->second.total, it->second.trivial);
    } else {
        func_log_t func_log;
        func_log.idx = idx;
        if(value==(T)1) {
            func_log.trivial = 1;            
        } else {
            func_log.trivial = 0;
        }
        func_log.total = 1;
        trivial_func_tmap[cct] = func_log;
        DPRINTF("\tINSERT: total=%ld, trivial=%ld\n", func_log.total, func_log.trivial);
    }
}

void init_trivial_func_list() {
    ConditionVal_t cval = IS_ZERO;
    trivial_func_list.push_back({"exp", _TRIVIAL_EXP, pre_func_cb_1Z<double, 0>, DFGSummary_generate_func(1, &cval, "exp", MATH_FUNC_COST_BOOST)});
    trivial_func_list.push_back({"expf", _TRIVIAL_EXPF, pre_func_cb_1Z<float, 1>, DFGSummary_generate_func(1, &cval, "expf", MATH_FUNC_COST_BOOST)});
    trivial_func_list.push_back({"sin", _TRIVIAL_SIN, pre_func_cb_1Z<double, 2>, DFGSummary_generate_func(1, &cval, "sin", MATH_FUNC_COST_BOOST)});
    trivial_func_list.push_back({"sinf", _TRIVIAL_SINF, pre_func_cb_1Z<float, 3>, DFGSummary_generate_func(1, &cval, "sinf", MATH_FUNC_COST_BOOST)});
    trivial_func_list.push_back({"cos", _TRIVIAL_COS, pre_func_cb_1Z<double, 4>, DFGSummary_generate_func(1, &cval, "cos", MATH_FUNC_COST_BOOST)});
    trivial_func_list.push_back({"cosf", _TRIVIAL_COSF, pre_func_cb_1Z<float, 5>, DFGSummary_generate_func(1, &cval, "cosf", MATH_FUNC_COST_BOOST)});
    trivial_func_list.push_back({"__exp_finite", _TRIVIAL_EXP, pre_func_cb_1Z<double, 6>, DFGSummary_generate_func(1, &cval, "__exp_finite", MATH_FUNC_COST_BOOST)});
    trivial_func_list.push_back({"__expf_finite", _TRIVIAL_EXP, pre_func_cb_1Z<float, 7>, DFGSummary_generate_func(1, &cval, "__expf_finite", MATH_FUNC_COST_BOOST)});
    trivial_func_list.push_back({"__log_finite", _TRIVIAL_LOG, pre_func_cb_1O<double, 8>, DFGSummary_generate_func(1, &cval, "__log_finite", MATH_FUNC_COST_BOOST)});
    trivial_func_list.push_back({"__logf_finite", _TRIVIAL_LOGF, pre_func_cb_1O<float, 9>, DFGSummary_generate_func(1, &cval, "__logf_finite", MATH_FUNC_COST_BOOST)});
    // TODO: multiple trivial entries
    trivial_func_list.push_back({"__pow_finite", _TRIVIAL_POWF, pre_func_cb_1Z<double, 10>, DFGSummary_generate_func(1, &cval, "__pow_finite", MATH_FUNC_COST_BOOST)});
    trivial_func_list.push_back({"__powf_finite", _TRIVIAL_POWF, pre_func_cb_1Z<float, 11>, DFGSummary_generate_func(1, &cval, "__powf_finite", MATH_FUNC_COST_BOOST)});
}

static inline __attribute__((always_inline))
void TrivialLoggerGenerateDFGSummaryCache(per_thread_log_t *pt, uint64_t* bb_ref, int threshold) {
    //per_thread_log_t *pt = (per_thread_log_t *)drmgr_get_tls_field(drcontext, global_log_space.tls_idx);
    DR_ASSERT(pt->stc_counter!=NULL);
    pt->total_cost = 0;
    pt->SB = 0;
    pt->BB = 0;
    pt->heavy = 0;
    pt->chained = 0;
    pt->bwd_slice = 0;
    pt->combined = 0;
    /* first update the function triviality */
    TrivialFuncMap& trivial_func_tmap = *(pt->trivial_func_tmap);
    for(auto it=trivial_func_tmap.begin(); it!=trivial_func_tmap.end(); ++it) {
        DFGSummary* dfg_s = trivial_func_list[it->second.idx].dfg_s;
        uint64_t trivial_num = it->second.trivial;
        pt->total_cost += dfg_s->total_cost*it->second.total;
        pt->SB += dfg_s->benifit*trivial_num;
        int64_t BB = dfg_s->benifit-dfg_s->cond_cost;
        if(BB>0) {
            pt->BB += BB*trivial_num;
        }
        pt->heavy += dfg_s->heavy*trivial_num;
        pt->chained += dfg_s->chained*trivial_num;
        pt->bwd_slice += dfg_s->bwd_slice*trivial_num;
        if(dfg_s->cvalNum>1) {
            pt->combined += dfg_s->benifit*trivial_num;
        }
    }
    // lock the global space for updating DFGSummary cache
    dr_mutex_lock(global_log_space.gLock);
    for(int i=0; i<=_bb_idx; ++i) {
        uint64_t bb_exe_num = bb_ref[i];
        if(bb_exe_num==0) continue;
        TrivialCCTMap& stc_counter = pt->stc_counter[i];
        DFG* dfg = global_log_space.dfg_logs[i].dfg;
        TrivialSummaryMap* cache = global_log_space.dfg_logs[i].trivial_summary_cache;
        STCList& stc_list = *(global_log_space.dfg_logs[i].stc_list);
        if(stc_list.empty()) {
            pt->total_cost += dfg->estimate()*bb_exe_num;
            continue;
        }
        for(auto cct_it=stc_counter.begin(); cct_it!=stc_counter.end(); ++cct_it) {
            for(auto it=cct_it->second.begin(); it!=cct_it->second.end(); ++it) {
                const std::vector<bool>& stc_map = it->first;
                uint64_t trivial_num = it->second;
                DFGSummary* dfg_s;
                // check if the stc_map is already cached
                auto tsm_iter = cache->find(stc_map);
                // only generate summary if not cached
                if(tsm_iter==cache->end()) {
                    DPRINTF("[TRIVIALSPY DEBUG] DFG Summary is missing in cache, generate new cache\n");
                    // first clear the DFG annotations for propagation
                    dfg->clear_annotation();
                    // propagate the detected combination of triviality
                    int cvalNum = 0;
                    int stc_map_num = stc_map.size();
                    IF_DEBUG_TRIVIAL(
                        if(stc_map.size()!=stc_list.size()) {
                            DPRINTF("[TRIVIALSPY DEBUG] STC_MAP(%d) and STC_LIST(%d) size not matched!\n", stc_map.size(), stc_list.size());
                        }
                    )
                    DR_ASSERT(stc_map.size()==stc_list.size());
                    for(int j=0; j<stc_map_num; ++j) {
                        if(stc_map[j]) {
                            DPRINTF("[TRIVIALSPY DEBUG] Propagate STC...\n");
                            stc_list[j].stc->val = convertCVal2Res(stc_list[j].cond);
                            bool check = propagateSTC(stc_list[j].stc, threshold);
                            // as all values are runtime-detected, so must be safely propagated
                            // if failed, there must be unknown errors/bugs. We check for safety
                            DR_ASSERT(check);
                            ++cvalNum;
                        }
                    }
                    // dead backward only exists only when at least a trivial condition satisfies
                    if(cvalNum) {
                        DPRINTF("[TRIVIALSPY DEBUG] Mark dead backwrad codes...\n");
                        mark_dead_backward(dfg);
                    }
                    // now the dfg has propagated trivialities, generate the summary for reports
                    DPRINTF("[TRIVIALSPY DEBUG] Generating DFG Summary...\n");
                    dfg_s = DFGSummary_generate(i, cvalNum, NULL/*not used*/, dfg);
                    (*cache)[stc_map] = dfg_s;
                    DPRINTF("[TRIVIALSPY DEBUG] DFG Summary generated & stored in cache\n");
                } else {
                    DPRINTF("[TRIVIALSPY DEBUG] DFG Summary found in cache\n");
                    dfg_s = tsm_iter->second;
                }
                // accumulate the global metrics
                pt->total_cost += dfg_s->total_cost*trivial_num;
                pt->SB += dfg_s->benifit*trivial_num;
                int64_t BB = dfg_s->benifit-dfg_s->cond_cost;
                if(BB>0) {
                    pt->BB += BB*trivial_num;
                }
                pt->heavy += dfg_s->heavy*trivial_num;
                pt->chained += dfg_s->chained*trivial_num;
                pt->bwd_slice += dfg_s->bwd_slice*trivial_num;
                if(dfg_s->cvalNum>1) {
                    pt->combined += dfg_s->benifit*trivial_num;
                }
            }
        }
    }
    // finish updating
    dr_mutex_unlock(global_log_space.gLock);
}

static inline __attribute__((always_inline))
void TrivialLoggerThreadInit(void* drcontext, bool enable_approx_soft, bool enable_approx_hard, bool no_trace) {
    per_thread_log_t* pt = (per_thread_log_t *)dr_thread_alloc(drcontext, sizeof(per_thread_log_t));
    pt->total_cost = 0;
    if(!no_trace) {
        pt->stc_counter = new TrivialCCTMap[MAX_DFGLOG_NUM];
        pt->stc_bmap = new std::vector<bool>();
        pt->stc_bmap->reserve(128);
        pt->trivial_func_tmap = new TrivialFuncMap();
    } else {
        pt->stc_counter = NULL;
        pt->stc_bmap = NULL;
        pt->trivial_func_tmap = NULL;
    }
    pt->seg_base = (byte*)dr_get_dr_segment_base(global_log_space.tls_seg);
    BUF_PTR(pt->seg_base, global_log_space.tls_offs) = 
        (byte*)dr_raw_mem_alloc(MAX_DFGLOG_NUM*sizeof(uint64_t), DR_MEMPROT_READ | DR_MEMPROT_WRITE, NULL);
    // store the thread id for final report
    pt->threadId = dr_atomic_add32_return_sum(&global_log_space.thread_id_max, 1);
    drmgr_set_tls_field(drcontext, global_log_space.tls_idx, (void *)pt);
}

static inline __attribute__((always_inline))
void TrivialLoggerThreadFini_impl(per_thread_log_t *pt, uint64_t* bb_ref) {
    if(pt->stc_counter) {
        delete[] pt->stc_counter;
        delete pt->trivial_func_tmap;
    }
    if(pt->stc_bmap) {
        delete pt->stc_bmap;
    }
    if(bb_ref) {
        update_global_bb_ref(bb_ref);
        dr_raw_mem_free(bb_ref, MAX_DFGLOG_NUM*sizeof(uint64_t));
    }
}

static inline __attribute__((always_inline))
void TrivialLoggerThreadFini(void* drcontext, bool external_fini) {
    per_thread_log_t *pt = (per_thread_log_t *)drmgr_get_tls_field(drcontext, global_log_space.tls_idx);
    if(!external_fini) {
        void* bb_ref = BUF_PTR(pt->seg_base, global_log_space.tls_offs);
        TrivialLoggerThreadFini_impl(pt, (uint64_t*)bb_ref);
    }
    // free thread data
    dr_thread_free(drcontext, pt, sizeof(per_thread_log_t));
}

static inline __attribute__((always_inline))
int getThreadId() {
    per_thread_log_t *pt = (per_thread_log_t *)drmgr_get_tls_field(dr_get_current_drcontext(), global_log_space.tls_idx);
    return pt->threadId;
}

static inline __attribute__((always_inline))
void accumulateGlobalMetric(per_thread_log_t *pt) {
    // per_thread_log_t *pt = (per_thread_log_t *)drmgr_get_tls_field(drcontext, global_log_space.tls_idx);
    __sync_fetch_and_add(&global_log_space.totalCost, pt->total_cost);
}

bool TrivialLoggerInit() {
    int tls_idx;
    uint tls_offs;
    reg_id_t tls_seg;

    /* allocate raw TLS so we can access it from the code cache */
    if (!dr_raw_tls_calloc(&tls_seg, &tls_offs, RAW_LOG_COUNTS, 0))
        return false;

    tls_idx = drmgr_register_tls_field();
    if (tls_idx == -1)
        return false;

    global_log_space.tls_idx = tls_idx;
    global_log_space.tls_offs= tls_offs;
    global_log_space.tls_seg = tls_seg;
    global_log_space.thread_id_max = -1;
    memset(&global_log_space.trivial_metrics, 0, sizeof(metric_t));
    memset(&global_log_space.soft_approx_metrics, 0, sizeof(metric_t));
    memset(&global_log_space.hard_approx_metrics, 0, sizeof(metric_t));

    global_log_space.bb_ref = (uint64_t*)dr_raw_mem_alloc(MAX_DFGLOG_NUM*sizeof(uint64_t), DR_MEMPROT_READ | DR_MEMPROT_WRITE, NULL);
    global_log_space.dfg_logs = new dfglog_t[MAX_DFGLOG_NUM];

    global_log_space.gLock = dr_mutex_create();
    return true;
}

bool TrivialLoggerFini() {
    dr_mutex_destroy(global_log_space.gLock);
    if (!drmgr_unregister_tls_field(global_log_space.tls_idx) ||
        !dr_raw_tls_cfree(global_log_space.tls_offs, RAW_LOG_COUNTS))
        return false;

    dr_raw_mem_free(global_log_space.bb_ref, MAX_DFGLOG_NUM*sizeof(uint64_t));
    for(int i=0; i<=_bb_idx; ++i) {
        delete global_log_space.dfg_logs[i].stc_list;
        if(global_log_space.dfg_logs[i].detect) {
            delete[] global_log_space.dfg_logs[i].detect;
        }
        delete global_log_space.dfg_logs[i].dfg;
        free_trivial_summary_cache(global_log_space.dfg_logs[i].trivial_summary_cache);
    }
    delete[] global_log_space.dfg_logs;

    return true;
}

#endif