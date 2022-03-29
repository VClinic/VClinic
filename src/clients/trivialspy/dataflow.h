#ifndef __DATAFLOW_H__
#define __DATAFLOW_H__

#include "dr_api.h"
#include "utils.h"
#include "trivial_table.h"
#include "trivial_detector.h"
#include <list>
#include <vector>
#include <string>
#include <algorithm>

#ifndef DR_STATCK_REG
#  ifdef AARCHXX
#    define DR_STACK_REG DR_REG_SP
#  else
#    define DR_STACK_REG DR_REG_RSP
#  endif
#endif

#define DEAD_MARK 0
#define IN_BACKWARD_SLICE 1
#define HEAVY 2
#define PROPAGATED 3

#define SET_PRINCIPLE(node, p) (node)->principle |= (1<<(p))
#define SET_DEAD_MARK(node) SET_PRINCIPLE(node, DEAD_MARK)
#define SET_BACKWARD_SLICE(node) SET_PRINCIPLE(node, IN_BACKWARD_SLICE)
#define SET_HEAVY(node) SET_PRINCIPLE(node, HEAVY)
#define SET_PROPAGATED(node) SET_PRINCIPLE(node, PROPAGATED)

#define HAS_PRINCIPLE(node, p) ((node)->principle & (1<<(p)))
#define IS_DEAD(node) HAS_PRINCIPLE(node, DEAD_MARK)
#define IS_IN_BACKWARD_SLICE(node) HAS_PRINCIPLE(node, IN_BACKWARD_SLICE)
#define IS_HEAVY(node) HAS_PRINCIPLE(node, HEAVY)
#define IS_PROPAGATED(node) HAS_PRINCIPLE(node, PROPAGATED)

struct ConditionResult {
    ConditionVal_t cond;
    Attribute_t attr;
    opnd_t res_opnd;
    opnd_t abs_opnd;
    opnd_t id_opnd;
    ResultVal_t result;
    bool propagated;
};

struct DFGDirectedLink;
struct DFGNode {
    int idx;
    instr_t* ins;
    bool is_copy;
    int cost;
    std::vector<DFGDirectedLink*> children;
    std::vector<DFGDirectedLink*> parents;
    // principle attributes
    int8_t principle;
    int info_idx;
    DFGNode(int i, instr_t *instr) {
      idx = i;
      ins = instr;
      // convert is not a copy as non-zero fp may result in zero int value
      is_copy = instr_is_copy(instr); // || instr_is_convert(instr);
      cost = estimate_cost(instr);
      principle = 0;
      info_idx = -1;
    }
    DFGNode() {
        idx = -1;
        ins = NULL;
        is_copy = false;
        principle = 0;
        info_idx = -1;
    }
};

struct DFGDirectedLink {
    opnd_t opnd;
    ResultVal_t val;
    DFGNode* source;
    DFGNode* target;
    bool isSingular;
    ConditionListInfo_t info;
    std::list<ConditionResult> condlist;
    static void print(file_t file, DFGDirectedLink* edge) {
        char buf[50];
        void* drcontext = dr_get_current_drcontext();
        size_t step = opnd_disassemble_to_buffer(
            drcontext, edge->opnd, buf,
            50);
        assert(step < 50);
        dr_fprintf(file,
                   "<opnd=%s, val=%s, source=[%d], target=[%d], isSingular=%d>",
                   buf, getResultValString(edge->val), edge->source->idx,
                   edge->target->idx, edge->isSingular);
    }
    DFGDirectedLink(opnd_t _opnd, DFGNode *_source, DFGNode *_target) {
        opnd = _opnd;
        source = _source;
        target = _target;
        val = UNKNOWN;
        isSingular = false;
    }
};

// info buffer list
#define MAX_INFO_NUM (1<<22)
static int info_list_curr = -1;
static std::string* info_list[MAX_INFO_NUM];
int allocate_info_idx() {
    int idx = __sync_add_and_fetch(&info_list_curr, 1);
    DR_ASSERT_MSG(idx<MAX_INFO_NUM, "ALLOCATE_INFO_IDX failed: preallocated info exhaustive!\n");
    return idx;
}

int register_info(std::string info) {
    int idx = allocate_info_idx();
    info_list[idx] = new std::string(info);
    return idx;
}

void free_info_list() {
    for(int i=0; i<=info_list_curr; ++i) {
        delete info_list[i];
    }
}

void print_DFGNode(void* drcontext, file_t file, DFGNode* node) {
    dr_fprintf(file, "[%d] ", node->idx);
    if(node->ins==NULL) {
        dr_fprintf(file, "<NULL>");
    } else {
        dr_fprintf(file, "%s", info_list[node->info_idx]->c_str());
    }
    dr_fprintf(file, "\n\tchildren={ ");
    for(auto it=node->children.begin(); it!=node->children.end(); ++it) {
        dr_fprintf(file, "<%p>:", *it);
        DFGDirectedLink::print(file, *it);
        dr_fprintf(file, "\n\t\t");
    }
    dr_fprintf(file, "\t}, parents={ ");
    for(auto it=node->parents.begin(); it!=node->parents.end(); ++it) {
        dr_fprintf(file, "<%p>:", *it);
        DFGDirectedLink::print(file, *it);
        dr_fprintf(file, "\n\t\t");
    }
    dr_fprintf(file, "}, principle=<bw=%d,heavy=%d,propagated=%d>\n", 
        IS_IN_BACKWARD_SLICE(node), IS_HEAVY(node), IS_PROPAGATED(node));
}


// data flow can be represented as a directed linked graph
// each edge is def-use relation
struct DFG {
    bool disassembled;
    // global entry node
    DFGNode entry_node;
    // the edges inside this list indicates original linear control flow inside a bb
    std::vector<DFGNode*> entries;
    std::vector<DFGDirectedLink*> edges;
    // construct DFG from basic block
    DFG(instrlist_t* bb);
    ~DFG();
    void recompute();
    void forward_analysis();
    size_t get_ins_count() { return entries.size(); }
    size_t get_mem_count() {
        size_t mem_count = 0; 
        for(auto it=entries.begin(); it!=entries.end(); ++it) {
            for(auto cit=(*it)->children.begin(); cit<(*it)->children.end(); ++cit) {
                if(opnd_is_memory_reference((*cit)->opnd)) {
                    ++mem_count;
                }
            }
            for(auto cit=(*it)->parents.begin(); cit<(*it)->parents.end(); ++cit) {
                if(opnd_is_memory_reference((*cit)->opnd)) {
                    ++mem_count;
                }
            }
        }
        return mem_count;
    }
    void clear_annotation();
    void disassemble();
    void print(file_t file);
    /********************************************
     *              Estimators                  *
     ********************************************/
    int estimate_detail(int principle);
    int estimate();
    int estimate_benifit();
    int estimate_condCost();

    private:
    void _estimate_all();
    int _detail[3];
    int _cost;
    int _benifit;
    int _condCost;
};

void DFG::clear_annotation() {
#ifdef DEBUG_TRIVIALSPY
    dr_fprintf(STDOUT, "[TRIVIALSPY DEBUG] DFG::clear_annotation triggered\n");
#endif
    int n_entry = entries.size();
    for(int i=0; i<n_entry; ++i) {
        entries[i]->principle = 0;
    }
    int n_edges = edges.size();
    for(int i=0; i<n_edges; ++i) {
        edges[i]->val = UNKNOWN;
        edges[i]->isSingular = false;
    }
#ifdef DEBUG_TRIVIALSPY
    dr_fprintf(STDOUT, "[TRIVIALSPY DEBUG] DFG::clear_annotation cleared DFG:\n");
    print(STDOUT);
    dr_fprintf(STDOUT, "\n");
    dr_fprintf(STDOUT, "[TRIVIALSPY DEBUG] DFG::clear_annotation exit\n");
#endif
}

DFG::~DFG() {
#ifdef DEBUG_TRIVIALSPY
    dr_fprintf(STDOUT, "Free DFG: %p\n", this);
#endif
    for(auto it=entries.begin(); it!=entries.end(); ++it) {
        DR_ASSERT(*it!=NULL);
        for(auto cit=(*it)->children.begin(); cit<(*it)->children.end(); ++cit) {
            delete (*cit);
        }
        delete (*it);
    }
    entries.clear();
    edges.clear();
}

void DFG::_estimate_all() {
    _cost = 0;
    _benifit = 0;
    _condCost = 0;
    _detail[0] = 0;
    _detail[1] = 0;
    _detail[2] = 0;
    for(auto it=entries.begin(); it!=entries.end(); ++it) {
        int cost = (*it)->cost;
        _cost += cost;
        if(IS_DEAD(*it)) {
            _benifit += cost;
            if(HAS_PRINCIPLE(*it, IN_BACKWARD_SLICE)) {
                _detail[0] += cost;
            }
            if(HAS_PRINCIPLE(*it, HEAVY)) {
                _detail[1] += cost;
            }
            if(HAS_PRINCIPLE(*it, PROPAGATED)) {
                _detail[2] += cost;
            }
        }
        // for each condition checking, one needs: 1) cmp [], imm8; 2) bne [pc]
        // s.t. cost = [MEMCOST +] 2
        for(auto p=(*it)->parents.begin(); p!=(*it)->parents.end(); ++p) {
            if((*p)->isSingular) {
                if(opnd_is_memory_reference((*p)->opnd)) {
                    _condCost += MEMORY_COST_BOOST;
                }
                _condCost += 2;
            }
        }
    }
}

int DFG::estimate_detail(int principle) {
    if(_cost==0) {
        _estimate_all();
    }
    return _detail[principle-1];
}

int DFG::estimate() {
    _estimate_all();
    return _cost;
}

int DFG::estimate_benifit() {
    if(_cost==0) {
        _estimate_all();
    }
    return _benifit;
}

int DFG::estimate_condCost() {
    if(_cost==0) {
        _estimate_all();
    }
    return _condCost;
}

#ifdef MAX_STRING_BUF_SIZE
#define MAX_STRING_BUF_SIZE_BK MAX_STRING_BUF_SIZE
#undef MAX_STRING_BUF_SIZE_BK
#endif
#define MAX_STRING_BUF_SIZE 100
int getStaticInstrInfo(void* drcontext, instr_t* ins) {
    int info_idx = allocate_info_idx();
    // string pointer to return
    char rptr[1024];
    // now try to fill in the buffer
    // trivial asm codes
    char code[MAX_STRING_BUF_SIZE+1];
    size_t res = instr_disassemble_to_buffer(drcontext, ins, code, MAX_STRING_BUF_SIZE);
    assert(res <= MAX_STRING_BUF_SIZE);
    // file path and source line information
    app_pc addr = instr_get_app_pc(ins);
    drsym_error_t symres;
    drsym_info_t sym;
    char name[MAXIMUM_SYMNAME];
    char file[MAXIMUM_PATH];
    file[0] = '\0';
    module_data_t *data;
    data = dr_lookup_module(addr);
    if (data == NULL) {
        size_t n = snprintf(rptr, 1024, "badIp[%s]", code);
        info_list[info_idx] = new std::string(rptr);
        return info_idx;
    }
    sym.struct_size = sizeof(sym);
    sym.name = name;
    sym.name_size = MAXIMUM_SYMNAME;
    sym.file = file;
    sym.file_size = MAXIMUM_PATH;
    symres = drsym_lookup_address(data->full_path, addr - data->start, &sym,
                                  DRSYM_DEFAULT_FLAGS);
    if (symres == DRSYM_SUCCESS || symres == DRSYM_ERROR_LINE_NOT_AVAILABLE) {
        int line;
        if (symres == DRSYM_ERROR_LINE_NOT_AVAILABLE) {
            line = 0;
        } else {
            line = sym.line;
        }
        size_t n = snprintf(rptr, 1024, "%s @%s[%s:%d]", code, sym.name, sym.file, line);
    } else {
        size_t n = snprintf(rptr, 1024, "%s @%s[%s:%d]", code, "<noname>", sym.file, 0);
    }
    dr_free_module_data(data);
    info_list[info_idx] = new std::string(rptr);
    return info_idx;
}
#undef MAX_STRING_BUF_SIZE
#ifdef MAX_STRING_BUF_SIZE_BK
#define MAX_STRING_BUF_SIZE MAX_STRING_BUF_SIZE_BK
#undef MAX_STRING_BUF_SIZE_BK
#endif

void DFG::disassemble() {
    if(!disassembled) {
#ifdef DEBUG_TRIVIALSPY
        dr_fprintf(STDOUT, "[TRIVIALSPY DEBUG] DFG::disassemble triggered\n");
#endif
        int n = 0;
        void* drcontext = dr_get_current_drcontext();
        for(auto it=entries.begin(); it!=entries.end(); ++it, ++n) {
            (*it)->info_idx = getStaticInstrInfo(drcontext, (*it)->ins);
        }
        disassembled = true;
#ifdef DEBUG_TRIVIALSPY
        dr_fprintf(STDOUT, "[TRIVIALSPY DEBUG] DFG::disassemble exit\n");
#endif
    }
}

void DFG::print(file_t file) {
    void* drcontext = dr_get_current_drcontext();
    int i=0;
    for(auto it=entries.begin(); it!=entries.end(); ++it, ++i) {
        print_DFGNode(drcontext, file, (*it));
    }
}

static reg_id_t
reg_resize_to_xmm(reg_id_t simd_reg)
{
#ifdef X86
    if (reg_is_strictly_xmm(simd_reg)) {
        return simd_reg;
    } else if (reg_is_strictly_ymm(simd_reg)) {
        return simd_reg - DR_REG_START_YMM + DR_REG_START_XMM;
    } else if (reg_is_strictly_zmm(simd_reg)) {
        return simd_reg - DR_REG_START_ZMM + DR_REG_START_XMM;
    }
#endif
    return DR_REG_INVALID;
}

// we naively lose track of stack when stack reg is updated.
// TODO: stack reg may be tracked to enumurate common update cases: call, sub, add, ...
#define TRACK_REG(reg_def, node) do {\
    if(reg_def == DR_STACK_REG) {\
        track_stack.clear();\
        track_stack_redzone.clear();\
    }\
    if(reg_is_gpr(reg_def)) {\
        assert(reg_resize_to_opsz(reg_def, OPSZ_PTR)-DR_REG_START_GPR >= 0);\
        assert(reg_resize_to_opsz(reg_def, OPSZ_PTR)-DR_REG_START_GPR <  DR_NUM_GPR_REGS);\
        track_reg_gpr[reg_resize_to_opsz(reg_def, OPSZ_PTR)-DR_REG_START_GPR] = (node);\
    } else if(reg_is_simd(reg_def)) {\
        assert(reg_resize_to_xmm(reg_def)-DR_REG_START_XMM >= 0);\
        assert(reg_resize_to_xmm(reg_def)-DR_REG_START_XMM <  DR_NUM_SIMD_VECTOR_REGS);\
        track_reg_simd[reg_resize_to_xmm(reg_def)-DR_REG_START_XMM] = (node);\
    }\
} while(0)

#define GET_TRACKED_REG_NODE(reg_use, p) do {\
    p=NULL;\
    if(reg_is_gpr(reg_use)) {\
        assert(reg_resize_to_opsz(reg_use, OPSZ_PTR)-DR_REG_START_GPR>=0);\
        assert(reg_resize_to_opsz(reg_use, OPSZ_PTR)-DR_REG_START_GPR< DR_NUM_GPR_REGS);\
        p = track_reg_gpr[reg_resize_to_opsz(reg_use, OPSZ_PTR)-DR_REG_START_GPR];\
    } else if(reg_is_simd(reg_use)) {\
        assert(reg_resize_to_xmm(reg_use)-DR_REG_START_XMM>=0);\
        assert(reg_resize_to_xmm(reg_use)-DR_REG_START_XMM< DR_NUM_SIMD_VECTOR_REGS);\
        p = track_reg_simd[reg_resize_to_xmm(reg_use)-DR_REG_START_XMM];\
    }\
} while(0)

#define TRACK_STACK(opnd, node) do {\
    int off = opnd_get_disp(opnd);\
    /* stack offset should be positive */\
    if(off>=0) {\
        int size = opnd_size_in_bytes(opnd_get_size(opnd));\
        int end = off + size;\
        if(track_stack.size()<(size_t)end) {\
            track_stack.resize(end, &entry_node);\
        }\
        for(int i=off; i<end; ++i) {\
            track_stack[i] = (node);\
        }\
    } else {\
        off = -off;\
        int size = opnd_size_in_bytes(opnd_get_size(opnd));\
        int end = off + size;\
        if(track_stack_redzone.size()<(size_t)end) {\
            track_stack_redzone.resize(end, &entry_node);\
        }\
        for(int i=off; i<end; ++i) {\
            track_stack_redzone[i] = (node);\
        }\
    }\
} while(0)

#define GET_TRACKED_STACK_NODE(opnd, p) do {\
    int off = opnd_get_disp(opnd);\
    /* stack offset should be negative */\
    if(off>=0 && (size_t)off<track_stack.size()) {\
        p = track_stack[off];\
    } else if(off<0) {\
        off = -off;\
        if((size_t)off<track_stack_redzone.size()) {\
            p = track_stack_redzone[off];\
        } else {\
            p = &entry_node;\
        }\
    } else {\
        p = &entry_node;\
    }\
} while(0)

#define ADD_EDGE(edge, opnd, src, dst) do {\
    edge = new DFGDirectedLink(opnd, src, dst);\
    (dst)->parents.push_back(edge);\
    (src)->children.push_back(edge);\
    edges.push_back(edge);\
} while(0)

void DFG::forward_analysis() {
    // TODO: track stack values for DFG analysis
    DFGDirectedLink* edge;
    DFGNode* track_reg_gpr[DR_NUM_GPR_REGS];
    DFGNode* track_reg_simd[DR_NUM_SIMD_VECTOR_REGS];
    std::vector<DFGNode*> track_stack;
    std::vector<DFGNode*> track_stack_redzone;
    int func_entry;
    for(int i=0; i<DR_NUM_GPR_REGS; ++i) {
        track_reg_gpr[i] = &entry_node;
    }
    for(int i=0; i<DR_NUM_SIMD_VECTOR_REGS; ++i) {
        track_reg_simd[i] = &entry_node;
    }
    for(auto it=entries.begin(); it!=entries.end(); ++it) {
        // handle non-call operations
        int num_srcs = instr_num_srcs((*it)->ins);
        for(int i=0; i<num_srcs; ++i) {
            opnd_t opnd = instr_get_src((*it)->ins, i);
            if(opnd_is_reg(opnd)) {
                DFGNode* p;
                reg_id_t reg_use = opnd_get_reg(opnd);
                GET_TRACKED_REG_NODE(reg_use, p);
                if(p) ADD_EDGE(edge, opnd, p, *it);
#ifdef DEBUG
                else {
                    dr_fprintf(STDERR, "Warning: skip non genereal-purpose/simd register for dataflow tracking: ");
                    opnd_disassemble(dr_get_current_drcontext(), opnd, STDERR);
                    dr_fprintf(STDERR, "\n");
                }
#endif
            } else if(opnd_is_near_base_disp(opnd) && opnd_num_regs_used(opnd)==1) {
                // check if it is stack memory reference
                reg_id_t reg_use = opnd_get_reg_used(opnd, 0);
                if(reg_use == DR_STACK_REG) {
                    DFGNode* p;
                    GET_TRACKED_STACK_NODE(opnd, p);
                    ADD_EDGE(edge, opnd, p, *it);
                } else {
                    ADD_EDGE(edge, opnd, &entry_node, *it);
                }
            } else {
                ADD_EDGE(edge, opnd, &entry_node, *it);
            }
            // lookup and set the condition list of the edge
            ConditionList_t* condlist;
            if(TrivialOpTable::getTrivialConditionList((*it)->ins, i, &condlist)) {
                memcpy(&(edge->info), &(condlist->first), sizeof(ConditionListInfo_t));
                int n_cond = condlist->second.size();
                for(int j=0; j<n_cond; ++j) {
                    DR_ASSERT(condlist->second[j].backward_idx>=0);
                    DR_ASSERT(condlist->second[j].backward_idx< instr_num_srcs((*it)->ins));
                    DR_ASSERT(condlist->second[j].other_idx>=0);
                    DR_ASSERT(condlist->second[j].other_idx< instr_num_srcs((*it)->ins));
                    ConditionResult condRes;
                    condRes.cond = condlist->second[j].val;
                    condRes.attr = condlist->second[j].attr;
                    condRes.res_opnd = instr_get_dst((*it)->ins, 0);
                    condRes.abs_opnd = instr_get_src((*it)->ins,
                                        condlist->second[j].backward_idx);
                    condRes.id_opnd = instr_get_src((*it)->ins,
                                        condlist->second[j].other_idx);
                    condRes.result = condlist->second[j].res;
                    condRes.propagated = false;
                    if(condlist->second[j].attr==Never) {
                        DR_ASSERT(condlist->second[j].res==INVALID);
                    }
                    edge->condlist.push_back(condRes);
                }
            }
        }
        int num_dsts = instr_num_dsts((*it)->ins);
        for(int i=0; i<num_dsts; ++i) {
            opnd_t opnd = instr_get_dst((*it)->ins, i);
            // we ignore the non-register dst opnd
            if(opnd_is_reg(opnd)) {
                reg_id_t reg_def = opnd_get_reg(opnd);
                TRACK_REG(reg_def, *it);
            } else if(opnd_is_near_base_disp(opnd) && opnd_num_regs_used(opnd)==1) {
                // check if it is stack memory reference
                reg_id_t reg_use = opnd_get_reg_used(opnd, 0);
                if(reg_use == DR_STACK_REG) {
                    TRACK_STACK(opnd, *it);
                }
            }
        }
    }
}

void DFG::recompute() {
    for(auto it=entries.begin(); it!=entries.end(); ++it) {
        (*it)->children.clear();
        (*it)->parents.clear();
    }
    edges.clear();
    forward_analysis();
}

DFG::DFG(instrlist_t* bb) {
    _cost = 0;
    disassembled = false;
    // forward analysis to construct DFG
    // 1) allocate all nodes with control flow
    int func_entry;
    int idx = 0;
    for (instr_t* instr = instrlist_first(bb); instr != NULL; instr = instr_get_next(instr)) {
        if(!instr_is_app(instr)) continue;
        // TODO: continue DFG construction if jump target is statically known
        DFGNode* new_node = new DFGNode(idx, instr);
        ++idx;
        entries.push_back(new_node);
    }
    // 2) def-use analysis to add directed links between all allocated nodes along control flow
    forward_analysis();
    // 3) disassemble for readable info
    disassemble();
}

// mark all parent nodes as nodes within backward slice
void mark_backward(DFGNode* node) {
    SET_BACKWARD_SLICE(node);
    for(auto p=node->parents.begin(); p!=node->parents.end(); ++p) {
        mark_backward((*p)->source);
    }
}

void mark_dead_backward(DFG* dfg) {
    for(auto it=dfg->entries.rbegin(); it!=dfg->entries.rend(); ++it) {
        if(IS_IN_BACKWARD_SLICE(*it)) {
            bool isDead = true;
            // only dead when all children are dead
            for(auto c=(*it)->children.begin(); c!=(*it)->children.end(); ++c) {
                if(!IS_DEAD((*c)->target)) {
                    isDead = false;
                    break;
                }
            }
            if(isDead) {
                SET_DEAD_MARK(*it);
            }
        }
    }
}

#endif