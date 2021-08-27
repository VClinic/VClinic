#ifndef __TRIVIAL_REPORT_H__
#define __TRIVIAL_REPORT_H__
#include "trivial_logger.h"
#include "trivial_detector.h"

struct RedundancyData {
    int64_t benifit;
    int64_t speculate;
    int DFGLog_ctxt;
};

static inline bool RedundancyCompare(const struct RedundancyData &first, const struct RedundancyData &second) {
    return first.benifit > second.benifit ? true : false;
}

void generateThreadReport(file_t file, uint64_t globalMetrics[5]) {
    per_thread_log_t *pt = (per_thread_log_t *)drmgr_get_tls_field(dr_get_current_drcontext(), global_log_space.tls_idx);
    uint64_t totalCost = pt->totalCost;
    TrivialLog* log = pt->log;
    int threadId = pt->threadId;

    dr_fprintf(file, "------ [Thread=%d] Dumping Dataflow-aware Trivial Inefficiency Report ------\n", threadId);
    std::vector<RedundancyData> tmpList;
    tmpList.reserve(DFGLogList_curr);
    uint64_t totalSpeculate = 0;
    uint64_t totalBenifit = 0;
    uint64_t totalAbsChainedCost = 0;
    uint64_t totalBwdSliceCost = 0;
    uint64_t totalHeavyCost = 0;
    for(int i=0; i<DFGLogList_curr; ++i) {
        if(log[i].total) {
            DFGLog* dfgLog = getDFGLog(i);
            int64_t speculate = log[i].trivial * dfgLog->benifit;
            int64_t benifit = speculate - log[i].total * dfgLog->detail->estimate_condCost();
            // filter positive logs
            if(benifit>0) {
                RedundancyData data = { benifit, speculate, i };
                tmpList.push_back(data);
                totalSpeculate += speculate;
                totalBenifit += benifit;
                totalAbsChainedCost += log[i].trivial * dfgLog->absChainedCost;
                totalBwdSliceCost += log[i].trivial * dfgLog->bwd_slice_cost;
                totalHeavyCost += log[i].trivial * dfgLog->heavyCost;
            }
        }
    }
    __sync_fetch_and_add(&globalMetrics[0],totalSpeculate);
    __sync_fetch_and_add(&globalMetrics[1],totalBenifit);
    __sync_fetch_and_add(&globalMetrics[2],totalAbsChainedCost);
    __sync_fetch_and_add(&globalMetrics[3],totalBwdSliceCost);
    __sync_fetch_and_add(&globalMetrics[4],totalHeavyCost);
    // sort by the benifits
    std::sort(tmpList.begin(), tmpList.end(), RedundancyCompare);
    // thread-level overview report
    dr_fprintf(file, "\nTotal Speculate Benifit: %.3lf (%ld benifit / %ld total cost)", 100.0*(double)totalSpeculate/(double)totalCost, totalSpeculate, totalCost);
    dr_fprintf(file, "\nTotal Benifit: %.3lf (%ld benifit / %ld total cost)", 100.0*(double)totalBenifit/(double)totalCost, totalBenifit, totalCost);
    dr_fprintf(file, "\nTotal Absorbing Chain: %.3lf (%ld / %ld benifit)", 100.0*(double)totalAbsChainedCost/(double)totalBenifit, totalAbsChainedCost, totalBenifit);
    dr_fprintf(file, "\nTotal Backward Slice: %.3lf (%ld / %ld benifit)", 100.0*(double)totalBwdSliceCost/(double)totalBenifit, totalBwdSliceCost, totalBenifit);
    dr_fprintf(file, "\nTotal Heavy Instruction: %.3lf (%ld / %ld benifit)\n", 100.0*(double)totalHeavyCost/(double)totalBenifit, totalHeavyCost, totalBenifit);
    // thread-level detailed report
    for(auto it=tmpList.begin(); it!=tmpList.end(); ++it) {
        // print benifit rate and importance metric
        dr_fprintf(file, "\n==== Benifit: %.3lf (%ld / %ld), Importance: %.3lf (%ld benifit / %ld total cost) ====\n",
            100.0 * (double)(*it).benifit / (double)totalBenifit, (*it).benifit, totalBenifit,
            100.0 * (double)(*it).benifit / (double)totalCost, (*it).benifit, totalCost);
        dr_fprintf(file, "^^ Speculate Benifit: %.3lf (%ld / %ld), Importance: %.3lf (%ld benifit / %ld total cost) ^^\n",
            100.0 * (double)(*it).speculate / (double)totalSpeculate, (*it).speculate, totalSpeculate,
            100.0 * (double)(*it).speculate / (double)totalCost, (*it).speculate, totalCost);
        int i = (*it).DFGLog_ctxt;
        DFGLog* dfgLog = getDFGLog(i);
        dr_fprintf(file, "Combined Trivial Num: %d\n", dfgLog->combinedNum);
        dr_fprintf(file, "Chained Rate: %.3lf (%ld / %ld)\n", 100.0*(double)dfgLog->absChainedCost/(double)dfgLog->benifit, dfgLog->absChainedCost, dfgLog->benifit);
        dr_fprintf(file, "Backward Slice Rate: %.3lf (%ld / %ld)\n", 100.0*(double)dfgLog->bwd_slice_cost/(double)dfgLog->benifit, dfgLog->bwd_slice_cost, dfgLog->benifit);
        dr_fprintf(file, "Heavy Instruction Rate: %.3lf (%ld / %ld)\n", 100.0*(double)dfgLog->heavyCost/(double)dfgLog->benifit, dfgLog->heavyCost, dfgLog->benifit);
        dr_fprintf(file, "^^^ Trivial Rate: %.3lf (%ld / %ld) ^^^\n", 
            100.0 * (double)log[i].trivial / (double)log[i].total, log[i].trivial, log[i].total);
        // print detailed dataflow-aware metrics
        dr_fprintf(file, "---- Trivial Dataflow Info ----\n");
        dfgLog->detail->print_summary(file);
    }
}

void generateThreadSoftApproxReport(file_t file, uint64_t globalMetrics[5]) {
    per_thread_log_t *pt = (per_thread_log_t *)drmgr_get_tls_field(dr_get_current_drcontext(), global_log_space.tls_idx);
    uint64_t totalCost = pt->totalCost;
    TrivialLog* log = pt->log;
    int threadId = pt->threadId;

    dr_fprintf(file, "------ [Thread=%d] Dumping Soft Approximated (epsilon=%le) Dataflow-aware Trivial Inefficiency Report ------\n", threadId, epsilon);
    std::vector<RedundancyData> tmpList;
    tmpList.reserve(DFGLogList_curr);
    uint64_t totalSpeculate = 0;
    uint64_t totalBenifit = 0;
    uint64_t totalAbsChainedCost = 0;
    uint64_t totalBwdSliceCost = 0;
    uint64_t totalHeavyCost = 0;
    for(int i=0; i<DFGLogList_curr; ++i) {
        if(log[i].total) {
            DFGLog* dfgLog = getDFGLog(i);
            int64_t speculate = log[i].soft_approx * dfgLog->benifit;
            int64_t benifit = log[i].soft_approx * dfgLog->benifit - log[i].total * dfgLog->detail->estimate_condCost();
            // filter positive logs
            if(benifit>0) {
                RedundancyData data = { benifit, speculate, i };
                tmpList.push_back(data);
                totalSpeculate += speculate;
                totalBenifit += benifit;
                totalAbsChainedCost += log[i].soft_approx * dfgLog->absChainedCost;
                totalBwdSliceCost += log[i].soft_approx * dfgLog->bwd_slice_cost;
                totalHeavyCost += log[i].soft_approx * dfgLog->heavyCost;
            }
        }
    }
    __sync_fetch_and_add(&globalMetrics[0],totalSpeculate);
    __sync_fetch_and_add(&globalMetrics[1],totalBenifit);
    __sync_fetch_and_add(&globalMetrics[2],totalAbsChainedCost);
    __sync_fetch_and_add(&globalMetrics[3],totalBwdSliceCost);
    __sync_fetch_and_add(&globalMetrics[4],totalHeavyCost);
    // sort by the benifits
    std::sort(tmpList.begin(), tmpList.end(), RedundancyCompare);
    // thread-level overview report
    dr_fprintf(file, "\nTotal Speculate Benifit: %.3lf (%ld benifit / %ld total cost)", 100.0*(double)totalSpeculate/(double)totalCost, totalSpeculate, totalCost);
    dr_fprintf(file, "\nTotal Benifit: %.3lf (%ld benifit / %ld total cost)", 100.0*(double)totalBenifit/(double)totalCost, totalBenifit, totalCost);
    dr_fprintf(file, "\nTotal Absorbing Chain: %.3lf (%ld / %ld benifit)", 100.0*(double)totalAbsChainedCost/(double)totalBenifit, totalAbsChainedCost, totalBenifit);
    dr_fprintf(file, "\nTotal Backward Slice: %.3lf (%ld / %ld benifit)", 100.0*(double)totalBwdSliceCost/(double)totalBenifit, totalBwdSliceCost, totalBenifit);
    dr_fprintf(file, "\nTotal Heavy Instruction: %.3lf (%ld / %ld benifit)\n", 100.0*(double)totalHeavyCost/(double)totalBenifit, totalHeavyCost, totalBenifit);
    // thread-level detailed report
    for(auto it=tmpList.begin(); it!=tmpList.end(); ++it) {
        // print benifit rate and importance metric
        dr_fprintf(file, "\n==== Benifit: %.3lf (%ld / %ld), Importance: %.3lf (%ld benifit / %ld total cost) ====\n",
            100.0 * (double)(*it).benifit / (double)totalBenifit, (*it).benifit, totalBenifit,
            100.0 * (double)(*it).benifit / (double)totalCost, (*it).benifit, totalCost);
        dr_fprintf(file, "^^ Speculate Benifit: %.3lf (%ld / %ld), Importance: %.3lf (%ld benifit / %ld total cost) ^^\n",
            100.0 * (double)(*it).speculate / (double)totalSpeculate, (*it).speculate, totalSpeculate,
            100.0 * (double)(*it).speculate / (double)totalCost, (*it).speculate, totalCost);
        int i = (*it).DFGLog_ctxt;
        DFGLog* dfgLog = getDFGLog(i);
        dr_fprintf(file, "Combined Trivial Num: %d\n", dfgLog->combinedNum);
        dr_fprintf(file, "Chained Rate: %.3lf (%ld / %ld)\n", 100.0*(double)dfgLog->absChainedCost/(double)dfgLog->benifit, dfgLog->absChainedCost, dfgLog->benifit);
        dr_fprintf(file, "Backward Slice Rate: %.3lf (%ld / %ld)\n", 100.0*(double)dfgLog->bwd_slice_cost/(double)dfgLog->benifit, dfgLog->bwd_slice_cost, dfgLog->benifit);
        dr_fprintf(file, "Heavy Instruction Rate: %.3lf (%ld / %ld)\n", 100.0*(double)dfgLog->heavyCost/(double)dfgLog->benifit, dfgLog->heavyCost, dfgLog->benifit);
        dr_fprintf(file, "^^^ Soft Approximated Trivial Rate: %.3lf (%ld / %ld) ^^^\n", 
            100.0 * (double)log[i].soft_approx / (double)log[i].total, log[i].soft_approx, log[i].total);
        // print detailed dataflow-aware metrics
        dr_fprintf(file, "---- Trivial Dataflow Info ----\n");
        dfgLog->detail->print_summary(file);
    }
}

void generateThreadHardApproxReport(file_t file, uint64_t globalMetrics[5]) {
    per_thread_log_t *pt = (per_thread_log_t *)drmgr_get_tls_field(dr_get_current_drcontext(), global_log_space.tls_idx);
    uint64_t totalCost = pt->totalCost;
    TrivialLog* log = pt->log;
    int threadId = pt->threadId;

    dr_fprintf(file, "------ [Thread=%d] Dumping Hard Approximated (bit_cnt=%d) Dataflow-aware Trivial Inefficiency Report ------\n", threadId, bit_cnt);
    std::vector<RedundancyData> tmpList;
    tmpList.reserve(DFGLogList_curr);
    uint64_t totalSpeculate = 0;
    uint64_t totalBenifit = 0;
    uint64_t totalAbsChainedCost = 0;
    uint64_t totalBwdSliceCost = 0;
    uint64_t totalHeavyCost = 0;
    for(int i=0; i<DFGLogList_curr; ++i) {
        if(log[i].total) {
            DFGLog* dfgLog = getDFGLog(i);
            int64_t speculate = log[i].hard_approx * dfgLog->benifit;
            int64_t benifit = log[i].hard_approx * dfgLog->benifit - log[i].total * dfgLog->detail->estimate_condCost();
            // filter positive logs
            if(benifit>0) {
                RedundancyData data = { benifit, speculate, i };
                tmpList.push_back(data);
                totalSpeculate += speculate;
                totalBenifit += benifit;
                totalAbsChainedCost += log[i].hard_approx * dfgLog->absChainedCost;
                totalBwdSliceCost += log[i].hard_approx * dfgLog->bwd_slice_cost;
                totalHeavyCost += log[i].hard_approx * dfgLog->heavyCost;
            }
        }
    }
    __sync_fetch_and_add(&globalMetrics[0],totalSpeculate);
    __sync_fetch_and_add(&globalMetrics[1],totalBenifit);
    __sync_fetch_and_add(&globalMetrics[2],totalAbsChainedCost);
    __sync_fetch_and_add(&globalMetrics[3],totalBwdSliceCost);
    __sync_fetch_and_add(&globalMetrics[4],totalHeavyCost);
    // sort by the benifits
    std::sort(tmpList.begin(), tmpList.end(), RedundancyCompare);
    // thread-level overview report
    dr_fprintf(file, "\nTotal Speculate Benifit: %.3lf (%ld benifit / %ld total cost)", 100.0*(double)totalSpeculate/(double)totalCost, totalSpeculate, totalCost);
    dr_fprintf(file, "\nTotal Benifit: %.3lf (%ld benifit / %ld total cost)", 100.0*(double)totalBenifit/(double)totalCost, totalBenifit, totalCost);
    dr_fprintf(file, "\nTotal Absorbing Chain: %.3lf (%ld / %ld benifit)", 100.0*(double)totalAbsChainedCost/(double)totalBenifit, totalAbsChainedCost, totalBenifit);
    dr_fprintf(file, "\nTotal Backward Slice: %.3lf (%ld / %ld benifit)", 100.0*(double)totalBwdSliceCost/(double)totalBenifit, totalBwdSliceCost, totalBenifit);
    dr_fprintf(file, "\nTotal Heavy Instruction: %.3lf (%ld / %ld benifit)\n", 100.0*(double)totalHeavyCost/(double)totalBenifit, totalHeavyCost, totalBenifit);
    // thread-level detailed report
    for(auto it=tmpList.begin(); it!=tmpList.end(); ++it) {
        // print benifit rate and importance metric
        dr_fprintf(file, "\n==== Benifit: %.3lf (%ld / %ld), Importance: %.3lf (%ld benifit / %ld total cost) ====\n",
            100.0 * (double)(*it).benifit / (double)totalBenifit, (*it).benifit, totalBenifit,
            100.0 * (double)(*it).benifit / (double)totalCost, (*it).benifit, totalCost);
        dr_fprintf(file, "^^ Speculate Benifit: %.3lf (%ld / %ld), Importance: %.3lf (%ld benifit / %ld total cost) ^^\n",
            100.0 * (double)(*it).speculate / (double)totalSpeculate, (*it).speculate, totalSpeculate,
            100.0 * (double)(*it).speculate / (double)totalCost, (*it).speculate, totalCost);
        int i = (*it).DFGLog_ctxt;
        DFGLog* dfgLog = getDFGLog(i);
        dr_fprintf(file, "Combined Trivial Num: %d\n", dfgLog->combinedNum);
        dr_fprintf(file, "Chained Rate: %.3lf (%ld / %ld)\n", 100.0*(double)dfgLog->absChainedCost/(double)dfgLog->benifit, dfgLog->absChainedCost, dfgLog->benifit);
        dr_fprintf(file, "Backward Slice Rate: %.3lf (%ld / %ld)\n", 100.0*(double)dfgLog->bwd_slice_cost/(double)dfgLog->benifit, dfgLog->bwd_slice_cost, dfgLog->benifit);
        dr_fprintf(file, "Heavy Instruction Rate: %.3lf (%ld / %ld)\n", 100.0*(double)dfgLog->heavyCost/(double)dfgLog->benifit, dfgLog->heavyCost, dfgLog->benifit);
        dr_fprintf(file, "^^^ Soft Approximated Trivial Rate: %.3lf (%ld / %ld) ^^^\n", 
            100.0 * (double)log[i].hard_approx / (double)log[i].total, log[i].hard_approx, log[i].total);
        // print detailed dataflow-aware metrics
        dr_fprintf(file, "---- Trivial Dataflow Info ----\n");
        dfgLog->detail->print_summary(file);
    }
}

void generateGlobalReport(file_t file, bool enable_soft, bool enable_hard) {
  dr_fprintf(file, "=== Overall Triviality Metric ===\n");
  dr_fprintf(file, "\nTotal Speculate Benifit: %.3lf (%ld benifit / %ld total cost)",
             100.0 * (double)global_log_space.trivial_metrics[0] /
                 (double)global_log_space.totalCost,
             global_log_space.trivial_metrics[0], global_log_space.totalCost);
  dr_fprintf(file, "\nTotal Benifit: %.3lf (%ld benifit / %ld total cost)",
             100.0 * (double)global_log_space.trivial_metrics[1] /
                 (double)global_log_space.totalCost,
             global_log_space.trivial_metrics[1], global_log_space.totalCost);
  dr_fprintf(file, "\nTotal Absorbing Chain: %.3lf (%ld / %ld benifit)",
             100.0 * (double)global_log_space.trivial_metrics[2] /
                 (double)global_log_space.trivial_metrics[1],
             global_log_space.trivial_metrics[2],
             global_log_space.trivial_metrics[1]);
  dr_fprintf(file, "\nTotal Backward Slice: %.3lf (%ld / %ld benifit)",
             100.0 * (double)global_log_space.trivial_metrics[3] /
                 (double)global_log_space.trivial_metrics[1],
             global_log_space.trivial_metrics[3],
             global_log_space.trivial_metrics[1]);
  dr_fprintf(file, "\nTotal Heavy Instruction: %.3lf (%ld / %ld benifit)\n",
             100.0 * (double)global_log_space.trivial_metrics[4] /
                 (double)global_log_space.trivial_metrics[1],
             global_log_space.trivial_metrics[4],
             global_log_space.trivial_metrics[1]);

  if(enable_soft) {
    dr_fprintf(file, "=== Overall Soft Approximated (epsilon=%le) Triviality Metric ===\n", epsilon);
    dr_fprintf(file, "\nTotal Speculate Benifit: %.3lf (%ld benifit / %ld total cost)",
                100.0 * (double)global_log_space.soft_approx_metrics[0] /
                    (double)global_log_space.totalCost,
                global_log_space.soft_approx_metrics[0], global_log_space.totalCost);
    dr_fprintf(file, "\nTotal Benifit: %.3lf (%ld benifit / %ld total cost)",
                100.0 * (double)global_log_space.soft_approx_metrics[1] /
                    (double)global_log_space.totalCost,
                global_log_space.soft_approx_metrics[1], global_log_space.totalCost);
    dr_fprintf(file, "\nTotal Absorbing Chain: %.3lf (%ld / %ld benifit)",
                100.0 * (double)global_log_space.soft_approx_metrics[2] /
                    (double)global_log_space.soft_approx_metrics[1],
                global_log_space.soft_approx_metrics[2],
                global_log_space.soft_approx_metrics[1]);
    dr_fprintf(file, "\nTotal Backward Slice: %.3lf (%ld / %ld benifit)",
                100.0 * (double)global_log_space.soft_approx_metrics[3] /
                    (double)global_log_space.soft_approx_metrics[1],
                global_log_space.soft_approx_metrics[3],
                global_log_space.soft_approx_metrics[1]);
    dr_fprintf(file, "\nTotal Heavy Instruction: %.3lf (%ld / %ld benifit)\n",
                100.0 * (double)global_log_space.soft_approx_metrics[4] /
                    (double)global_log_space.soft_approx_metrics[1],
                global_log_space.soft_approx_metrics[4],
                global_log_space.soft_approx_metrics[1]);
  }

  if(enable_hard) {
    dr_fprintf(file, "=== Overall Hard Approximated (bit_cnt=%d) Triviality Metric ===\n", bit_cnt);
    dr_fprintf(file, "\nTotal Speculate Benifit: %.3lf (%ld benifit / %ld total cost)",
                100.0 * (double)global_log_space.hard_approx_metrics[0] /
                    (double)global_log_space.totalCost,
                global_log_space.hard_approx_metrics[0], global_log_space.totalCost);
    dr_fprintf(file, "\nTotal Benifit: %.3lf (%ld benifit / %ld total cost)",
                100.0 * (double)global_log_space.hard_approx_metrics[1] /
                    (double)global_log_space.totalCost,
                global_log_space.hard_approx_metrics[1], global_log_space.totalCost);
    dr_fprintf(file, "\nTotal Absorbing Chain: %.3lf (%ld / %ld benifit)",
                100.0 * (double)global_log_space.hard_approx_metrics[2] /
                    (double)global_log_space.hard_approx_metrics[1],
                global_log_space.hard_approx_metrics[2],
                global_log_space.hard_approx_metrics[1]);
    dr_fprintf(file, "\nTotal Backward Slice: %.3lf (%ld / %ld benifit)",
                100.0 * (double)global_log_space.hard_approx_metrics[3] /
                    (double)global_log_space.hard_approx_metrics[1],
                global_log_space.hard_approx_metrics[3],
                global_log_space.hard_approx_metrics[1]);
    dr_fprintf(file, "\nTotal Heavy Instruction: %.3lf (%ld / %ld benifit)\n",
                100.0 * (double)global_log_space.hard_approx_metrics[4] /
                    (double)global_log_space.hard_approx_metrics[1],
                global_log_space.hard_approx_metrics[4],
                global_log_space.hard_approx_metrics[1]);
  }
}

#endif