#ifndef __DATAFLOW_SUMMARY_H__
#define __DATAFLOW_SUMMARY_H__

#include "dr_api.h"
#include "trivial_table.h"
#include <string>

#include "dataflow.h"

struct DFGNodeSummary {
    int8_t principle;
    int info_idx;
};

struct DFGEdgeSummary {
    int src;
    int dst;
    opnd_t opnd;
    ResultVal_t val;
    bool isSingular;
};

struct DFGSummary {
    int bb_idx;
    uint64_t boost;
    // metrics;
    int32_t total_cost; // total cost of this dfg
    int32_t benifit; // total specular benifit estimation
    int32_t cond_cost; // condition cost estimation
    int32_t heavy;
    int32_t chained;
    int32_t bwd_slice;
    // trivial condition
    int32_t cvalNum;
    union {
        ConditionVal_t  val;
        ConditionVal_t* val_list;
    };
    // graph info
    int N, E;
    DFGNodeSummary* nodes;
    DFGEdgeSummary* edges;
};

DFGSummary* DFGSummary_generate_func(int cvalNum, ConditionVal_t* val, const char* func_name, int func_cost) {
    DFGSummary* dfg_s = new DFGSummary();
    dfg_s->bb_idx = -1; /*invalid*/
    dfg_s->total_cost = func_cost;
    dfg_s->benifit = func_cost;
    dfg_s->cond_cost = 0;
    dfg_s->heavy = func_cost;
    dfg_s->chained = 0;
    dfg_s->bwd_slice = 0;
    dfg_s->cvalNum = cvalNum;
    DR_ASSERT(cvalNum>=0);
    if(cvalNum==1 && val!=NULL) {
        dfg_s->val = *val;
    } else {
        dfg_s->val_list = val;
    }
    dfg_s->N = 1;
    dfg_s->E = 0;
    dfg_s->nodes = new DFGNodeSummary[1];
    dfg_s->nodes[0].principle = 0;
    SET_DEAD_MARK(dfg_s->nodes);
    SET_HEAVY(dfg_s->nodes);
    dfg_s->nodes[0].info_idx = register_info(std::string("<MATH CALL> ")+std::string(func_name));
    return dfg_s;
}

DFGSummary* DFGSummary_generate(int bb_idx, int cvalNum, ConditionVal_t* val, DFG* dfg) {
    // dfg->disassemble();
    DFGSummary* dfg_s = new DFGSummary();
    dfg_s->bb_idx = bb_idx;
    dfg_s->total_cost = dfg->estimate();
    dfg_s->benifit = dfg->estimate_benifit();
    dfg_s->cond_cost = dfg->estimate_condCost();
    dfg_s->heavy = dfg->estimate_detail(HEAVY);
    dfg_s->chained = dfg->estimate_detail(PROPAGATED);
    dfg_s->bwd_slice = dfg->estimate_detail(IN_BACKWARD_SLICE);
    dfg_s->cvalNum = cvalNum;
    DR_ASSERT(cvalNum>=0);
    if(cvalNum==1 && val!=NULL) {
        dfg_s->val = *val;
    } else {
        dfg_s->val_list = val;
    }
    // generate node summary
    dfg_s->N = dfg->entries.size();
    dfg_s->nodes = new DFGNodeSummary[dfg_s->N];
    for(int i=0; i<dfg_s->N; ++i) {
        DR_ASSERT_MSG(i==dfg->entries[i]->idx, "the index stored in dfg entry is not match the actual index!");
        dfg_s->nodes[i].principle = dfg->entries[i]->principle;
        dfg_s->nodes[i].info_idx = dfg->entries[i]->info_idx;
    }
    // generate edge summary
    dfg_s->E = dfg->edges.size();
    if(dfg_s->E) {
        dfg_s->edges = new DFGEdgeSummary[dfg_s->E];
        for(int i=0; i<dfg_s->E; ++i) {
            dfg_s->edges[i].src = dfg->edges[i]->source->idx;
            dfg_s->edges[i].dst = dfg->edges[i]->target->idx;
            dfg_s->edges[i].opnd = dfg->edges[i]->opnd;
            dfg_s->edges[i].val = dfg->edges[i]->val;
            dfg_s->edges[i].isSingular = dfg->edges[i]->isSingular;
        }
    }
    return dfg_s;
}

void DFGSummary_free(DFGSummary* dfg_s) {
    delete[] dfg_s->nodes;
    if(dfg_s->E) {
        delete[] dfg_s->edges;
    }
    if(dfg_s->cvalNum>1 && dfg_s->val_list) {
        delete[] dfg_s->val_list;
    }
    delete dfg_s;
}

void DFGNodeSummary_print(file_t file, DFGNodeSummary* node_s) {
    std::string res = "";
    if(IS_DEAD(node_s)) { res += "[D]"; }
    if(IS_HEAVY(node_s)) { res += "[H]"; }
    if(IS_PROPAGATED(node_s)) { res += "[P]"; }
    if(IS_IN_BACKWARD_SLICE(node_s)) { res += "[B]"; }
    dr_fprintf(file, "%s %s", node_s->info_idx>=0?info_list[node_s->info_idx]->c_str():"<no info>", res.c_str());
}

void DFGEdgeSummary_print(file_t file, DFGEdgeSummary* edge_s) {
    char buf[50];
    void* drcontext = dr_get_current_drcontext();
    size_t step = opnd_disassemble_to_buffer(
        drcontext, edge_s->opnd, buf,
        50);
    assert(step < 50);
    dr_fprintf(file, "<%d, %d>: opnd=%s, val=%s, isSingular=%d", edge_s->src, edge_s->dst, buf, getResultValString(edge_s->val), edge_s->isSingular);
}

void DFGSummary_print(file_t file, DFGSummary* dfg_s, uint64_t boost) {
    dr_fprintf(file, "======= DFGLog from Thread %d =======\n", dr_get_thread_id(dr_get_current_drcontext()));
    dr_fprintf(file, "exe count: %ld\n", boost);
    dr_fprintf(file, "total cost: %d\n", dfg_s->total_cost);
    dr_fprintf(file, "benifit: %d\n", dfg_s->benifit);
    dr_fprintf(file, "TC branch cost: %d\n", dfg_s->cond_cost);
    dr_fprintf(file, "heavy cost: %d\n", dfg_s->heavy);
    dr_fprintf(file, "chained cost: %d\n", dfg_s->chained);
    dr_fprintf(file, "backward slice cost: %d\n", dfg_s->bwd_slice);
    dr_fprintf(file, "Trivial Condition Num: %d\n", dfg_s->cvalNum);
    dr_fprintf(file, "++++++ Singular Trivial Condition(s): \n");
    for(int i=0; i<dfg_s->E; ++i) {
        if(dfg_s->edges[i].isSingular) {
            dr_fprintf(file, "  <%d>\t", i);
            DFGEdgeSummary_print(file, &(dfg_s->edges[i]));
            dr_fprintf(file, "\n");
        }
    }
    dr_fprintf(file, "==> detailed node info: \n");
    for(int i=0; i<dfg_s->N; ++i) {
        dr_fprintf(file, "  [%d]\t", i);
        DFGNodeSummary_print(file, &(dfg_s->nodes[i]));
        dr_fprintf(file, "\n");
    }
    dr_fprintf(file, "==> detailed edge info: \n");
    for(int i=0; i<dfg_s->E; ++i) {
        dr_fprintf(file, "  <%d>\t", i);
        DFGEdgeSummary_print(file, &(dfg_s->edges[i]));
        dr_fprintf(file, "\n");
    }
    dr_fprintf(file, "=====================================\n");
}

// dump binary DFG summary info for better illustration
void DFGSummary_dump(file_t file, DFGSummary* dfg_s) {
    dr_write_file(file, dfg_s, sizeof(DFGSummary));
    for(int i=0; i<dfg_s->N; ++i) {
        dr_write_file(file, &(dfg_s->nodes[i]), sizeof(DFGNodeSummary));
    }
    for(int i=0; i<dfg_s->E; ++i) {
        dr_write_file(file, &(dfg_s->edges[i]), sizeof(DFGEdgeSummary));
    }
    dr_write_file(file, &info_list_curr, sizeof(int));
    for(int i=0; i<info_list_curr; ++i) {
        size_t str_len = info_list[i]->size();
        dr_write_file(file, &str_len, sizeof(size_t));
        dr_write_file(file, info_list[i]->c_str(), str_len);
    }
}

// DFGSummary logs
#define MAX_DFGLOG_NUM (1<<22)
static int DFGLogList_curr = -1;
static DFGSummary* DFGLogList[MAX_DFGLOG_NUM];

void dumpAllDFGLog(file_t file) {
    dr_write_file(file, &DFGLogList_curr, sizeof(int));
    for(int i=0; i<=DFGLogList_curr; ++i) {
        DFGSummary_dump(file, DFGLogList[i]);
    }
}

static inline bool DFGLogCompare(const DFGSummary* first, const DFGSummary* second) {
    return first->benifit*first->boost > second->benifit*second->boost ? true : false;
}

// not thread-safe, just use for print static dfg analysis info in client_exit
void printAllDFGLog(file_t file, bool enable_sort, uint64_t* bb_ref) {
    dr_fprintf(file, "\n--------------------------------------------\n");
    dr_fprintf(file, "Total DFG Log: %d\n", DFGLogList_curr+1);
    if(DFGLogList_curr>=0) {
        uint64_t benifit = 0;
        uint64_t chained = 0;
        uint64_t bwdslice = 0;
        uint64_t heavy = 0;
        uint64_t combined = 0;
        uint64_t totalDFGCost = 0;
        for(int i=0; i<=DFGLogList_curr; ++i) {
            uint64_t boost =  bb_ref[DFGLogList[i]->bb_idx];
            DFGLogList[i]->boost = boost;
            // total cost
            totalDFGCost += DFGLogList[i]->total_cost * boost;
            // specutal benifit
            benifit += DFGLogList[i]->benifit * boost;
            // combined chained
            if(DFGLogList[i]->cvalNum > 1) {
                combined += DFGLogList[i]->benifit * boost;
            }
            // heavy instruction
            heavy += DFGLogList[i]->heavy * boost;
            // chained absorbing
            chained += DFGLogList[i]->chained * boost;
            // backward slice
            bwdslice += DFGLogList[i]->bwd_slice * boost;
        }
        dr_fprintf(file, "Potential Benifit: %.3lf (%ld / %ld)\n", 100.0*(double)benifit/(double)totalDFGCost, benifit, totalDFGCost);
        dr_fprintf(file, "Heavy Instruction Rate: %.3lf (%ld / %ld)\n", 100.0*(double)heavy/(double)benifit, heavy, benifit);
        dr_fprintf(file, "Trivial Chain Rate: %.3lf (%ld / %ld)\n", 100.0*(double)chained/(double)benifit, chained, benifit);
        dr_fprintf(file, "Redundant Backward Slice Rate: %.3lf (%ld / %ld)\n", 100.0*(double)bwdslice/(double)benifit, bwdslice, benifit);
        dr_fprintf(file, "Combined Trivial Rate: %.3lf (%ld / %ld)\n", 100.0*(double)combined/(double)benifit, combined, benifit);
        dr_fprintf(file, "----------- Detailed DFG Statistics -------------\n\n");
        if(enable_sort) {
            std::sort(DFGLogList, DFGLogList+DFGLogList_curr+1, DFGLogCompare);
        }
        for(int i=0; i<=DFGLogList_curr; ++i) {
            uint64_t boost =  bb_ref[DFGLogList[i]->bb_idx];
            DFGSummary_print(file, DFGLogList[i], boost);
        }
    }
}
int allocateDFGList() {
    int idx = __sync_add_and_fetch(&DFGLogList_curr, 1);
    assert(idx<MAX_DFGLOG_NUM);
    return idx;
}

void freeDFGList() {
    for(int i=0; i<=DFGLogList_curr; ++i) {
        DFGSummary_free(DFGLogList[i]);
    }
}

#endif