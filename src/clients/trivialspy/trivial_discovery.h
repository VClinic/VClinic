#ifndef __TRIVIAL_DISCOVERY_H__
#define __TRIVIAL_DISCOVERY_H__

#include "dataflow_summary.h"

struct SingularTrivialCondition {
    ConditionVal_t cond;
    DFGDirectedLink* stc;
};

typedef std::vector<SingularTrivialCondition> STCList;

struct IndividualSTC {
    // DFG summary context handle
    int DFG_ctxt;
    // singular trivial condition
    DFGDirectedLink* stc;
    IndividualSTC(int _ctxt, DFGDirectedLink* _stc) {
        DFG_ctxt = _ctxt;
        stc = _stc;
    }
};

struct CombinedSTC {
    int DFG_ctxt;
    std::vector<DFGDirectedLink*> stc_list;
    CombinedSTC(int _ctxt) {
        DFG_ctxt = _ctxt;
    }
};

typedef std::vector<IndividualSTC*> IndividualSTCList;
typedef std::vector<CombinedSTC*> CombinedSTCList;

void __propagate_result(DFGDirectedLink* tce, ResultVal_t res, opnd_t res_opnd, bool force_propagate, std::list<DFGDirectedLink*>& trivial_edges) {
    // propagate the result and push into the queue if not propogated
    DFGNode* target = tce->target;
    for(auto ci=target->children.begin(); ci!=target->children.end(); ++ci) {
        if(force_propagate || opnd_same(res_opnd, (*ci)->opnd)) {
            if((*ci)->val==UNKNOWN) {
                (*ci)->val = res;
                SET_PROPAGATED((*ci)->target);
                trivial_edges.push_back(*ci);
#ifdef DEBUG_TRIVIALSPY
                dr_fprintf(STDOUT, "[TRIVIALSPY DEBUG] Propagate result %s(%d) from source: ", getResultValString(res), res);
                print_DFGNode(dr_get_current_drcontext(), STDOUT, tce->source);
                dr_fprintf(STDOUT, "--> to target: ");
                print_DFGNode(dr_get_current_drcontext(), STDOUT, target);
                dr_fprintf(STDOUT, "==> Detailed info: \n");
                DFGDirectedLink::print(STDOUT, tce);
                dr_fprintf(STDOUT, "\n");
#endif
            } else {
                // make sure the consistancy of the results
                if((*ci)->val!=res) {
                    dr_fprintf(STDOUT, "Propagation result not consistant: stored %s(%d), propagated %s(%d)\n", 
                        getResultValString((*ci)->val), (*ci)->val, getResultValString(res), res);
                    dr_fprintf(STDOUT, "==> Detailed info: \n");
                    DFGDirectedLink::print(STDOUT, tce);
                    dr_fprintf(STDOUT, "\n--> Target info: \n");
                    print_DFGNode(dr_get_current_drcontext(), STDOUT, target);
                }
                DR_ASSERT((*ci)->val==res);
            }
        }
    }
}

bool propagateSTC_impl(std::list<DFGDirectedLink*>& trivial_edges, int threshold) {
#ifdef DEBUG_TRIVIALSPY
    dr_fprintf(STDOUT, "[TRIVIALSPY DEBUG] enter propagateSTC_impl\n");
#endif
    while(!trivial_edges.empty()) {
        DFGDirectedLink* tce = trivial_edges.front();
        DR_ASSERT(tce!=NULL);
        DR_ASSERT(tce->target!=NULL);
        // propagate directly if the target instruction is copy
        if(tce->target->is_copy) {
            if(tce->target->parents.size()!=1) {
                DFGDirectedLink::print(STDOUT, tce);
                dr_fprintf(STDOUT, "\nDFGNode:\n");
                print_DFGNode(dr_get_current_drcontext(), STDOUT, tce->target);
            }
            DR_ASSERT(tce->target->parents.size()==1);
            __propagate_result(tce, tce->val, tce->opnd/*not used*/, true, trivial_edges);
            trivial_edges.pop_front();
            continue;
        }
        ConditionVal_t cval = convertRes2CVal(tce->val);
        for(auto it=tce->condlist.begin(); it!=tce->condlist.end(); ++it) {
            // check if the result of opnd matches the condition
            if(cval==(*it).cond) {
                (*it).propagated = true;
                // if the condition will never be satisfied, skip the propagation.
                if((*it).attr==Never) {
                    return false;
                }
                // find and set the result of the opnd from the target nodes children edges
                DFGNode* target = tce->target;
                SET_DEAD_MARK(target);
                // Principle 1: heavy instruction
                if(target->cost >= threshold) {
                    SET_HEAVY(target);
                }
                // Principle 2: trivial chain propagation
                ResultVal_t res = (*it).result;
                DR_ASSERT(res!=INVALID);
                if(res==OTHER) {
                    // the result operand can be directly propogated as the edges
                    // are appended by the control flow order, where all the DFG
                    // edges to the node must have a result value.
                    for (auto pi=target->parents.begin(); pi!=target->parents.end(); ++pi) {
                        if (opnd_same((*it).id_opnd, (*pi)->opnd)) {
                            res = (*pi)->val;
                        }
                    }
                    DR_ASSERT(res!=OTHER);
                }
                if(res!=UNKNOWN) {
                    // propagate the result and push into the queue if not propogated
                    __propagate_result(tce, res, (*it).res_opnd, false, trivial_edges);
                }
                // Principle 3: mark the absorbing backward slices
                if((*it).attr==Absorbing) {
                    for(auto pi=target->parents.begin(); pi!=target->parents.end(); ++pi) {
                        DR_ASSERT((*pi)!=NULL);
                        if(opnd_same((*it).abs_opnd, (*pi)->opnd)) {
                            mark_backward((*pi)->source);
                        }
                    }
                }
            }
        }
        trivial_edges.pop_front();
    }
    return true;
}

bool propagateSTC(DFGDirectedLink* stc, int threshold) {
#ifdef DEBUG_TRIVIALSPY
    dr_fprintf(STDOUT, "[TRIVIALSPY DEBUG] enter propagateSTC\n");
#endif
    DR_ASSERT(stc!=NULL);
    ResultVal_t val = stc->val;
    opnd_t stc_opnd = stc->opnd;
    DFGNode* root = stc->source;
    DR_ASSERT(root!=NULL);
    std::list<DFGDirectedLink*> trivial_edges;
    if(root->ins!=NULL && root->is_copy) {
        // this opnd is loaded by the source instruction from memory
        DR_ASSERT(root->parents.size()==1);
        DFGDirectedLink* stc_mem = root->parents[0];
        DR_ASSERT(stc_mem!=NULL);
        if(opnd_is_memory_reference(stc_mem->opnd)) {
            stc = stc_mem;
            stc->val = val;
            SET_DEAD_MARK(root);
            SET_PROPAGATED(root);
        }
    }
    stc->isSingular = true;
    for(auto it=root->children.begin(); it!=root->children.end(); ++it) {
        DR_ASSERT((*it)!=NULL);
        if(opnd_same(stc_opnd,(*it)->opnd)) {
            (*it)->val = val;
            trivial_edges.push_back(*it);
#ifdef DEBUG_TRIVIALSPY
            dr_fprintf(STDOUT, "[TRIVIALSPY DEBUG] Initialize Propagate result %s(%d) from source: ", getResultValString(val), val);
            print_DFGNode(dr_get_current_drcontext(), STDOUT, root);
            dr_fprintf(STDOUT, "--> to target: ");
            print_DFGNode(dr_get_current_drcontext(), STDOUT, (*it)->target);
            dr_fprintf(STDOUT, "==> Detailed info: \n");
            DFGDirectedLink::print(STDOUT, *it);
            dr_fprintf(STDOUT, "\n");
#endif
        }
    }
    return propagateSTC_impl(trivial_edges, threshold);
}

void discoverIndividualSTC(int bb_idx, DFG* dfg, IndividualSTCList* stc_list, int threshold) {
#ifdef DEBUG_TRIVIALSPY
    dr_fprintf(STDOUT, "[TRIVIALSPY DEBUG] enter discoverIndividualSTC\n");
#endif
    if(stc_list) stc_list->clear();
    for(auto it=dfg->edges.begin(); it!=dfg->edges.end(); ++it) {
        DFGDirectedLink* stc = *it;
        for(auto ci=stc->condlist.begin(); ci!=stc->condlist.end(); ++ci) {
            // only try to propagate when it is not propagated from the previous STC
            if(!(*ci).propagated && (*ci).result!=INVALID && (*ci).result!=OTHER) {
                ConditionVal_t val = (*ci).cond;
                stc->val = convertCVal2Res(val);
#ifdef DEBUG_TRIVIALSPY
                dr_fprintf(STDOUT, "[TRIVIALSPY DEBUG] Initialize STC %s(%d): ", getResultValString(stc->val), stc->val);
                DFGDirectedLink::print(STDOUT, stc);
                dr_fprintf(STDOUT, "\n");
#endif
                if(propagateSTC(stc, threshold)) {
                    mark_dead_backward(dfg);
                    int DFG_ctxt = allocateDFGList();
                    DFGLogList[DFG_ctxt] = DFGSummary_generate(bb_idx, 1, &val, dfg);
                    if(stc_list) {
                        stc_list->push_back(new IndividualSTC(DFG_ctxt, stc));
                    }
                }
                dfg->clear_annotation();
            }
        }
    }
}

typedef std::vector<std::pair<ResultVal_t,DFGDirectedLink*> > trivial_roots_t;

bool findAbsorbingRoot(DFGDirectedLink* stc, ConditionVal_t val, trivial_roots_t* roots) {
#ifdef DEBUG_TRIVIALSPY
    dr_fprintf(STDOUT, "[TRIVIALSPY DEBUG] enter findAbsorbingRoot\n");
#endif
    bool found = false;
    DFGNode* src = stc->source;
    for(auto it=src->parents.begin(); it!=src->parents.end(); ++it) {
        DFGDirectedLink* p = *it;
        for(auto ci=p->condlist.begin(); ci!=p->condlist.end(); ++ci) {
            if((*ci).result==convertCVal2Res(val)) {
                found = true;
                if(!findAbsorbingRoot(p, (*ci).cond, roots)) {
                    roots->push_back(std::make_pair(convertCVal2Res((*ci).cond),p));
                }
            }
        }
    }
    return found;
}

struct breakpoint_t {
    ResultVal_t val;
    DFGDirectedLink* stc;
    trivial_roots_t roots;
};

typedef std::vector<breakpoint_t> breakpoint_list_t;

bool findAbsorbingSuccessor(DFGDirectedLink* stc, breakpoint_list_t* break_pt, ConditionVal_t* val) {
#ifdef DEBUG_TRIVIALSPY
    dr_fprintf(STDOUT, "[TRIVIALSPY DEBUG] enter findAbsorbingSuccessor\n");
#endif
    DFGNode* dst = stc->target;
    for(auto it=dst->children.begin(); it!=dst->children.end(); ++it) {
        DFGDirectedLink* p = *it;
        for(auto ci=p->condlist.begin(); ci!=p->condlist.end(); ++ci) {
            if((*ci).result==ZERO || (*ci).result==ONE || (*ci).result==FULL) {
                *val = (*ci).cond;
                return true;
            } else if((*ci).result==OTHER) {
                breakpoint_t breakpoint;
                breakpoint.val = convertCVal2Res((*ci).cond);
                breakpoint.stc = stc;
                break_pt->push_back(breakpoint);
                return findAbsorbingSuccessor(p, break_pt, val);
            }
        }
    }
    return false;
}

void discoverCombinedSTC(int bb_idx, DFG* dfg, CombinedSTCList* stc_list, int threshold, int max_cval_num) {
#ifdef DEBUG_TRIVIALSPY
    dr_fprintf(STDOUT, "[TRIVIALSPY DEBUG] enter discoverCombinedSTC\n");
#endif
    if(stc_list) stc_list->clear();
    std::list<DFGDirectedLink*> trivial_edges;
    std::vector<int> break_pt_num;
    breakpoint_list_t break_pt;
    for(auto it=dfg->edges.begin(); it!=dfg->edges.end(); ++it) {
        DFGDirectedLink* stc = *it;
        for(auto ci=stc->condlist.begin(); ci!=stc->condlist.end(); ++ci) {
            // Principle 4: Chained trivial breakpoint
            if((*ci).result==OTHER) {
                // try to find a absorbing successor within max_cval_num identical step
                ConditionVal_t val;
                break_pt.clear();
                breakpoint_t breakpoint;
                breakpoint.val = convertCVal2Res((*ci).cond);
                breakpoint.stc = stc;
                break_pt.push_back(breakpoint);
                bool success = findAbsorbingSuccessor(stc, &break_pt, &val);
                int cval_num = break_pt.size()+1/*abs root condition*/;
                if(success && cval_num<=max_cval_num) {
                    // try to find a absorbing desuccessor
                    trivial_roots_t abs_roots;
                    if(findAbsorbingRoot(stc, val, &abs_roots)) {
                        // now this must be a trivial breakpoint
                        // try to find a absorbing root to generate the identical trivial condition
                        int total_num = 1;
                        break_pt_num.clear();
                        break_pt_num.push_back(1);
                        int n_bp = break_pt.size();
                        for(int i=0; i<n_bp; ++i) {
                            if(!findAbsorbingRoot(break_pt[i].stc, convertRes2CVal(break_pt[i].val), &break_pt[i].roots)) {
                                break_pt[i].roots.push_back(std::make_pair(break_pt[i].val,break_pt[i].stc));
                            }
                            total_num *= break_pt[i].roots.size();
                            break_pt_num.push_back(total_num);
                        }
                        ConditionVal_t* cvals = NULL;
                        for(int k=0; k<total_num; ++k) {
                            int n_abs_roots = abs_roots.size();
                            for(int j=0; j<n_abs_roots; ++j) {
                                if(cvals==NULL) {
                                    cvals = new ConditionVal_t[cval_num];
                                }
                                cvals[0] = val;
                                // first propagate the selected abs_root
                                abs_roots[j].second->val = abs_roots[j].first;
#ifdef DEBUG_TRIVIALSPY
            dr_fprintf(STDOUT, "[TRIVIALSPY DEBUG] Initialize Combined Propagate ABS result %s(%d) from source: ", getResultValString(abs_roots[j].first), abs_roots[j].first);
            dr_fprintf(STDOUT, "==> Detailed info: \n");
            DFGDirectedLink::print(STDOUT, abs_roots[j].second);
            dr_fprintf(STDOUT, "\n");
#endif
                                if(propagateSTC(abs_roots[j].second, threshold)) {
                                    trivial_edges.clear();
                                    bool propagated = true;
                                    int n_bp = break_pt.size();
                                    for(int i=0; i<n_bp; ++i) {
                                        int index = (k / break_pt_num[i]) %
                                                    break_pt[i].roots.size();
                                        DFGDirectedLink *breakpt_stc = 
                                            break_pt[i].roots[index].second;
                                        if(breakpt_stc->val==UNKNOWN) {
                                            breakpt_stc->val = break_pt[i].roots[index].first;
                                            propagated = false;
                                        } else if(breakpt_stc->val!=break_pt[i].roots[index].first) {
                                            // Aleady propagated but not satisfy the condition
                                            break;
                                        }
                                        trivial_edges.push_back(breakpt_stc);
                                        cvals[i+1] = convertRes2CVal(breakpt_stc->val);
                                    }
                                    // skip if already propagated
                                    if(propagated) {
#ifdef DEBUG_TRIVIALSPY
                                        dr_fprintf(STDOUT, "[TRIVIALSPY DEBUG] skip as the breakpoint is propagated\n");
#endif
                                        dfg->clear_annotation();
                                        continue;
                                    }
                                    // continue trivial propagation
                                    if(propagateSTC_impl(trivial_edges, threshold)) {
                                        mark_dead_backward(dfg);
                                        int DFG_ctxt = allocateDFGList();
                                        DFGLogList[DFG_ctxt] = DFGSummary_generate(bb_idx, cval_num, cvals, dfg);
                                        if(stc_list) {
                                            CombinedSTC* combined_stc = new CombinedSTC(DFG_ctxt);
                                            combined_stc->stc_list.push_back(abs_roots[j].second);
                                            int n_bp = break_pt.size();
                                            for(int i=0; i<n_bp; ++i) {
                                                int index = (k / break_pt_num[i]) %
                                                    break_pt[i].roots.size();
                                                combined_stc->stc_list.push_back(break_pt[i].roots[index].second);
                                            }
                                            stc_list->push_back(combined_stc);
                                        }
                                        cvals = NULL;
                                    }
                                }
                                dfg->clear_annotation();
                            } // abs_roots
                        } // total_num
                        if(cvals!=NULL) {
                            delete[] cvals;
                        }
                    }
                }
            }
        }
    }
}


bool discoverTrivialBreakPoints(DFGDirectedLink* stc, ConditionResult& ci, breakpoint_list_t* break_pt, int max_cval_num) {
     // Principle 4: Chained trivial breakpoint
    if(ci.result==OTHER) {
        // try to find a absorbing successor within max_cval_num identical step
        ConditionVal_t val;
        break_pt->clear();
        breakpoint_t breakpoint;
        breakpoint.val = convertCVal2Res(ci.cond);
        breakpoint.stc = stc;
        break_pt->push_back(breakpoint);
        bool success = findAbsorbingSuccessor(stc, break_pt, &val);
        int cval_num = break_pt->size()+1/*abs root condition*/;
        if(success && cval_num<=max_cval_num) {
            // try to find a absorbing desuccessor
            trivial_roots_t abs_roots;
            if(findAbsorbingRoot(stc, val, &abs_roots)) {
                // now this must be a trivial breakpoint
                return true;
            }
        }
    }
    return false;
}

void discoverSTC(int bb_idx, DFG* dfg, STCList* stc_list, int threshold, int max_cval_num) {
    breakpoint_list_t break_pt;
    std::list<SingularTrivialCondition> stc_breakpts;
    DR_ASSERT(dfg!=NULL);
    DR_ASSERT(stc_list!=NULL);
    stc_list->clear();
    for(auto it=dfg->edges.begin(); it!=dfg->edges.end(); ++it) {
        DFGDirectedLink* stc = *it;
        DR_ASSERT(stc!=NULL);
        for(auto ci=stc->condlist.begin(); ci!=stc->condlist.end(); ++ci) {
            // only try to propagate when it is not propagated from the previous STC
            if(!(*ci).propagated && (*ci).result!=INVALID) {
                if((*ci).result!=OTHER) {
                    // discover STC via Principle 1 / 2 / 3
                    stc->val = convertCVal2Res((*ci).cond);
                    if(propagateSTC(stc, threshold)) {
                        // mark_dead_backward(dfg);
                        stc_list->push_back({(*ci).cond, stc});
                    }
                } else {
                    // an identical trivial condition, use Principle 4
                    // check if it is within the cached break points
                    // Note that we only add the current stc into the list and store
                    // the other break points to the cache to guarantee the CFG order
                    // within stc_list (which can simplify the further detection).
                    bool cached = false;
                    for(auto bp=stc_breakpts.begin(); bp!=stc_breakpts.end(); ++bp) {
                        if((*bp).stc == stc && (*bp).cond == (*ci).cond) {
                            stc_breakpts.erase(bp);
                            cached = true;
                            break;
                        }
                    }
                    if(cached || discoverTrivialBreakPoints(stc, *ci, &break_pt, max_cval_num)) {
                        // Principle 4 matched, add to the stc list
                        stc_list->push_back({(*ci).cond, stc});
                        // cache the break_pt if exist
                        for(auto bp=break_pt.begin(); bp!=break_pt.end(); ++bp) {
                            // avoid caching already handled stc
                            if((*bp).stc!=stc) {
                                stc_breakpts.push_back({convertRes2CVal((*bp).val), (*bp).stc});
                            }
                        }
                    }
                }
                dfg->clear_annotation();
            }
        }
    }
#ifdef DEBUG_TRIVIALSPY
    // all stc within the stc list must not be duplicated
    int n=stc_list->size();
    STCList& stc_list_ref = *stc_list;
    for(int i=0; i<n; ++i) {
        for(int j=i+1; j<n; ++j) {
            DR_ASSERT(stc_list_ref[i].cond != stc_list_ref[j].cond ||
                      stc_list_ref[i].stc != stc_list_ref[j].stc);
#ifdef FATAL_WHEN_NOT_CRITICAL
            DR_ASSERT(stc_list_ref[i].cond != stc_list_ref[j].cond ||
                      stc_list_ref[i].stc->source !=
                          stc_list_ref[j].stc->source ||
                      !opnd_same(stc_list_ref[i].stc->opnd,
                                 stc_list_ref[j].stc->opnd));
#endif
        }
        if(i>0) {
            // ensure the control flow consistency
            DR_ASSERT(stc_list_ref[i].stc->target->idx >= stc_list_ref[i-1].stc->target->idx);
        }
    }
#endif
}

#endif