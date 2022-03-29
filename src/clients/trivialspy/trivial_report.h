#ifndef __TRIVIAL_REPORT_H__
#define __TRIVIAL_REPORT_H__
#include "trivial_logger.h"
#include "trivial_detector.h"

#define MAX_CCT_DEPTH 50

struct RedundancyData {
    int64_t benifit;
    int64_t speculate;
    uint64_t totalNum;
    uint64_t trivialNum;
    DFGSummary* dfg_s;
    int32_t cct;
};

static inline bool RedundancyCompareBB(const struct RedundancyData &first, const struct RedundancyData &second) {
    return first.benifit > second.benifit ? true : false;
}

static inline bool RedundancyCompareSB(const struct RedundancyData &first, const struct RedundancyData &second) {
    return first.speculate > second.speculate ? true : false;
}

void generateThreadOverviewReport(file_t file, per_thread_log_t *pt) {
    uint64_t totalCost = pt->total_cost;
    int64_t totalSB = pt->SB;
    int64_t totalBB = pt->BB;
    uint64_t totalHeavy = pt->heavy;
    uint64_t totalChained = pt->chained;
    uint64_t totalBwdSlice = pt->bwd_slice;
    uint64_t totalCombined = pt->combined;
    dr_fprintf(file, "\nTotal Speculate Benifit: %.3lf (%ld benifit / %ld total cost)", 100.0*(double)totalSB/(double)totalCost, totalSB, totalCost);
    dr_fprintf(file, "\nTotal Benifit: %.3lf (%ld benifit / %ld total cost)", 100.0*(double)totalBB/(double)totalCost, totalBB, totalCost);
    dr_fprintf(file, "\nTotal Heavy Instruction: %.3lf (%ld / %ld SB)", 100.0*(double)totalHeavy/(double)totalSB, totalHeavy, totalSB);
    dr_fprintf(file, "\nTotal Trivial Chain: %.3lf (%ld / %ld SB)", 100.0*(double)totalChained/(double)totalSB, totalChained, totalSB);
    dr_fprintf(file, "\nTotal Redundant Backward Slice: %.3lf (%ld / %ld SB)", 100.0*(double)totalBwdSlice/(double)totalSB, totalBwdSlice, totalSB);
    dr_fprintf(file, "\nTotal Absorbing Breakpoints: %.3lf (%ld / %ld SB)", 100.0*(double)totalCombined/(double)totalSB, totalCombined, totalSB);
    dr_fprintf(file, "\n");
}

void generateThreadDetailedReport(file_t file, per_thread_log_t *pt, std::vector<RedundancyData>& tmpList, int max_print, bool BB_order) {
    uint64_t totalCost = pt->total_cost;
    int64_t totalSB = pt->SB;
    int64_t totalBB = pt->BB;
    uint64_t totalHeavy = pt->heavy;
    uint64_t totalChained = pt->chained;
    uint64_t totalBwdSlice = pt->bwd_slice;
    uint64_t totalCombined = pt->combined;
    int num_print = 0;
    for(auto it=tmpList.begin(); it!=tmpList.end() && num_print<max_print; ++it, ++num_print) {
        if(BB_order && (*it).benifit<=0) {
            break;
        }
        // print benifit rate and importance metric
        dr_fprintf(file, "\n===============================\n");
        dr_fprintf(file, "Benifit: %.3lf (%ld local benifit / %ld total benifit)\n",
            100.0 * (double)(*it).benifit / (double)totalBB, (*it).benifit, totalBB);
        dr_fprintf(file, "Importance: %.3lf (%ld benifit / %ld total cost)\n\n",
            100.0 * (double)(*it).benifit / (double)totalCost, (*it).benifit, totalCost);
        dr_fprintf(file, "Speculate Benifit: %.3lf (%ld local SB / %ld SB)\n",
            100.0 * (double)(*it).speculate / (double)totalSB, (*it).speculate, totalSB);
        dr_fprintf(file, "Importance: %.3lf (%ld benifit / %ld total cost)\n\n",
            100.0 * (double)(*it).speculate / (double)totalCost, (*it).speculate, totalCost);
        dr_fprintf(file, "^^ Trivial Rate: %.3lf (%ld / %ld) ^^\n", 
            100.0 * (double)(*it).trivialNum / (double)(*it).totalNum, (*it).trivialNum, (*it).totalNum);
        dr_fprintf(file, "+++ DFG Caller CCT Info +++\n");
        drcctlib_print_backtrace(file, (*it).cct, false, true, MAX_CCT_DEPTH);
        dr_fprintf(file, "+++ DFG Summary Info +++\n");
        DFGSummary_print(file, (*it).dfg_s, (*it).trivialNum);
    }
    if(num_print==0) {
        dr_fprintf(file, "\n*** NO Positive Branch Benifitial DFG founded for triviality optimization ***\n");
    }
}

void generateThreadReport(per_thread_log_t *pt, uint64_t* bb_ref, file_t file, file_t gFile, int threshold, int max_print) {
    // first generate dfg summaries for reporting
    TrivialLoggerGenerateDFGSummaryCache(pt, bb_ref, threshold);
    // extract the results
    int threadId = pt->threadId;
    uint64_t totalCost = pt->total_cost;
    uint64_t totalSB = pt->SB;
    uint64_t totalBB = pt->BB;
    uint64_t totalHeavy = pt->heavy;
    uint64_t totalChained = pt->chained;
    uint64_t totalBwdSlice = pt->bwd_slice;
    uint64_t totalCombined = pt->combined;

    dr_fprintf(file, "------ [Thread=%d] Dumping Dataflow-aware Trivial Inefficiency Report ------\n", threadId);
    std::vector<int32_t> logged_cct;
    std::vector<RedundancyData> tmpList;
    int bb_num = _bb_idx;
    for(int i=0; i<=bb_num; ++i) {
        uint64_t total_num = bb_ref[i];
        if(total_num) {
            TrivialCCTMap& stc_counter = pt->stc_counter[i];
            TrivialSummaryMap* cache = global_log_space.dfg_logs[i].trivial_summary_cache;
            logged_cct.clear();
            for(auto cct_it=stc_counter.begin(); cct_it!=stc_counter.end(); ++cct_it) {
                // merge all cct with the same caller prefix (and thus same cct info as the cct is bb-level)
                int32_t cct = cct_it->first;
                bool found = false;
                for(auto it=logged_cct.begin(); it!=logged_cct.end(); ++it) {
                    if(drcctlib_have_same_call_path(*it, cct)) {
                        cct = *it;
                        found = true;
                        break;
                    }
                }
                if(!found) {
                    logged_cct.push_back(cct);
                }
                for(auto it=cct_it->second.begin(); it!=cct_it->second.end(); ++it) {
                    const std::vector<bool>& stc_map = it->first;
                    uint64_t trivial_num = it->second;
                    DFGSummary* dfg_s = (*cache)[stc_map];
                    int64_t SB = dfg_s->benifit*(int64_t)trivial_num;
                    int64_t BB = (dfg_s->benifit-dfg_s->cond_cost)*(int64_t)trivial_num;
                    RedundancyData data = { BB, SB, total_num, trivial_num, dfg_s, cct };
                    tmpList.push_back(data);
                }
            }
        }
    }
    /* extract the function triviality */
    TrivialFuncMap& trivial_func_tmap = *(pt->trivial_func_tmap);
    for(auto it=trivial_func_tmap.begin(); it!=trivial_func_tmap.end(); ++it) {
        int32_t cct = it->first;
        DFGSummary* dfg_s = trivial_func_list[it->second.idx].dfg_s;
        uint64_t total_num = it->second.total;
        uint64_t trivial_num = it->second.trivial;
        int64_t SB = dfg_s->benifit*(int64_t)trivial_num;
        int64_t BB = (dfg_s->benifit-dfg_s->cond_cost)*(int64_t)trivial_num;
        RedundancyData data = { BB, SB, total_num, trivial_num, dfg_s, cct };
        tmpList.push_back(data);
    }
    // accumulate global trivial metrics
    __sync_fetch_and_add(&global_log_space.trivial_metrics.SB, totalSB);
    __sync_fetch_and_add(&global_log_space.trivial_metrics.BB, totalBB);
    __sync_fetch_and_add(&global_log_space.trivial_metrics.heavy, totalHeavy);
    __sync_fetch_and_add(&global_log_space.trivial_metrics.chained, totalChained);
    __sync_fetch_and_add(&global_log_space.trivial_metrics.bwd_slice, totalBwdSlice);
    __sync_fetch_and_add(&global_log_space.trivial_metrics.combined, totalCombined);
    // now generate thread-level reports
    // sort by BB
    dr_fprintf(file, "\n------- Trivial Hotspots ordered by BB --------\n");
    std::sort(tmpList.begin(), tmpList.end(), RedundancyCompareBB);
    // thread-level overview report
    generateThreadOverviewReport(file, pt);
    // thread-level detailed report
    generateThreadDetailedReport(file, pt, tmpList, max_print, true);
    // sort by SB
    dr_fprintf(file, "\n------- Trivial Hotspots ordered by SB --------\n");
    std::sort(tmpList.begin(), tmpList.end(), RedundancyCompareSB);
    // thread-level overview report
    generateThreadOverviewReport(file, pt);
    // thread-level detailed report
    generateThreadDetailedReport(file, pt, tmpList, max_print, false);
    // generate thread-level overview report into the global overview report
    dr_mutex_lock(global_log_space.gLock);
    dr_fprintf(gFile, "\n------ [Thread=%d] Dumping Dataflow-aware Trivial Inefficiency Overview Report ------\n", threadId);
    generateThreadOverviewReport(gFile, pt);
    dr_mutex_unlock(global_log_space.gLock);
}

// TODO: soft/hard approximated triviality reports
// void generateApproxSoftThreadReport(void* drcontext, file_t file, int threshold, int max_print);
// void generateApproxHardThreadReport(void* drcontext, file_t file, int threshold, int max_print);

void generateGlobalOverviewReport(file_t file, uint64_t totalCost, metric_t& metrics) {
    uint64_t totalSB = metrics.SB;
    uint64_t totalBB = metrics.BB;
    uint64_t totalHeavy = metrics.heavy;
    uint64_t totalChained = metrics.chained;
    uint64_t totalBwdSlice = metrics.bwd_slice;
    uint64_t totalCombined = metrics.combined;
    dr_fprintf(file, "\nTotal Speculate Benifit: %.3lf (%ld benifit / %ld total cost)", 100.0*(double)totalSB/(double)totalCost, totalSB, totalCost);
    dr_fprintf(file, "\nTotal Benifit: %.3lf (%ld benifit / %ld total cost)\n", 100.0*(double)totalBB/(double)totalCost, totalBB, totalCost);
    dr_fprintf(file, "\nTotal Heavy Instruction: %.3lf (%ld / %ld SB)", 100.0*(double)totalHeavy/(double)totalSB, totalHeavy, totalSB);
    dr_fprintf(file, "\nTotal Trivial Chain: %.3lf (%ld / %ld SB)", 100.0*(double)totalChained/(double)totalSB, totalChained, totalSB);
    dr_fprintf(file, "\nTotal Redundant Backward Slice: %.3lf (%ld / %ld SB)", 100.0*(double)totalBwdSlice/(double)totalSB, totalBwdSlice, totalSB);
    dr_fprintf(file, "\nTotal Absorbing Breakpoints: %.3lf (%ld / %ld SB)", 100.0*(double)totalCombined/(double)totalSB, totalCombined, totalSB);
    dr_fprintf(file, "\n");
}

// thread-safe global report generation
void generateGlobalReport(file_t file, bool enable_soft, bool enable_hard) {
    dr_fprintf(file, "\n=== Overall Triviality Metric ===\n");
    generateGlobalOverviewReport(file, global_log_space.totalCost, global_log_space.trivial_metrics);

    if(enable_soft) {
        dr_fprintf(file, "\n=== Overall Soft Approximated (epsilon=%le) Triviality Metric ===\n", epsilon);
        generateGlobalOverviewReport(file, global_log_space.totalCost, global_log_space.soft_approx_metrics);
    }

    if(enable_hard) {
        dr_fprintf(file, "\n=== Overall Hard Approximated (bit_cnt=%d) Triviality Metric ===\n", bit_cnt);
        generateGlobalOverviewReport(file, global_log_space.totalCost, global_log_space.hard_approx_metrics);
    }
}

#endif