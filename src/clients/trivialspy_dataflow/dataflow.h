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

#define SET_MARKER(node, trivial_idx, cond) (node)->marker = ((trivial_idx) | (cond)<<4)
#define GET_TRIVIAL_IDX(node) ((node)->marker & 0xf)
#define GET_TRIVIAL_COND(node) (ConditionVal_t)((node)->marker >> 4)

#define MEMORY_COST_BOOST 50

int32_t estimate_cost(instr_t* instr) {
    int cost;
    if(TrivialOpTable::isTrivial(instr, TRIVIAL_OPRAND_ANY, false)) {
        int opcode = instr_get_opcode(instr);
        int i = 1, num_srcs = instr_num_srcs(instr);
        int size = opnd_size_in_bytes(opnd_get_size(instr_get_src(instr, 0)));
        while(size==0 && i<num_srcs) {
            size = opnd_size_in_bytes(opnd_get_size(instr_get_src(instr, i)));
            ++i;
        }
        cost = TrivialOpTable::getCost(opcode, size);
    } else {
        cost = 1;
    }

    int mem_count = 0;
    int num_srcs = instr_num_srcs(instr);
    for(int i=0; i<num_srcs; ++i) {
        if(opnd_is_memory_reference(instr_get_src(instr, i))) {
            ++mem_count;
        }
    }
    int num_dsts = instr_num_dsts(instr);
    for(int i=0; i<num_dsts; ++i) {
        if(opnd_is_memory_reference(instr_get_dst(instr, i))) {
            ++mem_count;
        }
    }

    cost += mem_count*MEMORY_COST_BOOST;
    return cost;
}

struct DFGNode {
    int idx;
    instr_t* ins;
    std::list<std::pair<int, DFGNode*> > children;
    std::list<std::pair<int, DFGNode*> > parents;
    // related condlist is cached into this node to avoid redundant lookups
    bool isTrivial;
    ConditionList_t** condlist;
    // trivial marker
    int8_t marker;
    // principle attributes
    int8_t principle;
    std::string* info;
    // result
    ResultVal_t result;
    int8_t other_idx;
    DFGNode() { marker=-1; principle=0; result=UNKNOWN; }
};

void unlink_DFGNode(DFGNode* node) {
    // clear edges
    for(auto c=node->children.begin(); c!=node->children.end(); ++c) {
        (*c).second->parents.remove(std::make_pair((*c).first,node));
    }
    node->children.clear();
    for(auto p=node->parents.begin(); p!=node->parents.end(); ++p) {
        // this instruction will be set to nop, so no def-use relations
        (*p).second->children.remove(std::make_pair((*p).first,node));
    }
    node->parents.clear();
}

// data flow can be represented as a directed linked graph
// each edge is def-use relation
struct DFG {
    // whether need to destroy instructions
    bool need_destroy;
    bool need_free_condlist;
    // the edges inside this list indicates original linear control flow inside a bb
    std::vector<DFGNode*> entries;
    // construct DFG from basic block
    DFG(instrlist_t* bb);
    // clone instructions and graph structure from dfg
    DFG(DFG* dfg, bool deep_copy);
    ~DFG() {
        for(auto it=entries.begin(); it!=entries.end(); ++it) {
            if(*it==NULL) continue;
            
            if(need_destroy) {
                instr_destroy(dr_get_current_drcontext(), (*it)->ins);
            }
            if(need_free_condlist) {
                int num_srcs = instr_num_srcs((*it)->ins);
                if(num_srcs>0) {
                    dr_global_free((*it)->condlist, sizeof(ConditionList_t*)*num_srcs);
                }
            }
            delete (*it);
        }
        entries.clear();
    }
    void recompute();
    void forward_analysis();
    // int locate(DFGNode* node) {
    //     int i = 0;
    //     for(auto pos=entries.begin(); pos!=entries.end(); ++pos, ++i) {
    //         if(*pos == node) {
    //             return i;
    //         }
    //     }
    //     assert(0 && "DFG node not found!\n");
    //     return -1;
    // }
    size_t get_ins_count() { return entries.size(); }
    size_t get_mem_count() {
        size_t mem_count = 0; 
        for(auto it=entries.begin(); it!=entries.end(); ++it) {
            instr_t* instr = (*it)->ins;
            int num_srcs = instr_num_srcs(instr);
            for(int i=0; i<num_srcs; ++i) {
                if(opnd_is_memory_reference(instr_get_src(instr, i))) {
                    ++mem_count;
                }
            }
            int num_dsts = instr_num_dsts(instr);
            for(int i=0; i<num_dsts; ++i) {
                if(opnd_is_memory_reference(instr_get_dst(instr, i))) {
                    ++mem_count;
                }
            }
        }
        return mem_count;
    }
    int trivialCondNum();
    bool isCombinedTrivial();
    void disassemble();
    void print_detail(file_t file);
    void print_summary(file_t file);
    /********************************************
     *              Estimators                  *
     ********************************************/
    int estimate_detail(int principle);
    int estimate();
    int estimate_benifit();
    int estimate_condCost();
    // void prune_to_interest();

    private:
    void _estimate_all();
    int _detail[3];
    int _cost;
    int _benifit;
    int _condCost;
};

int DFG::trivialCondNum() {
    int num = 0;
    for(auto it=entries.begin(); it!=entries.end(); ++it) {
        if((*it)->marker!=-1) {
            ++num;
        }
    }
    return num;
}

bool DFG::isCombinedTrivial() {
    return trivialCondNum()>1;
}

void DFG::_estimate_all() {
    _cost = 0;
    _benifit = 0;
    _condCost = 0;
    _detail[0] = 0;
    _detail[1] = 0;
    _detail[2] = 0;
    for(auto it=entries.begin(); it!=entries.end(); ++it) {
        int cost = estimate_cost((*it)->ins);
        _cost += cost;
        if(IS_DEAD(*it)) {
            // we conservatively do not fully eliminate memory writes
            if(instr_writes_memory((*it)->ins)) {
                cost -= MEMORY_COST_BOOST;
            }
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
        if((*it)->marker!=-1) {
            assert(GET_TRIVIAL_IDX(*it) >= 0);
            assert(GET_TRIVIAL_IDX(*it) < instr_num_srcs((*it)->ins));
            if(opnd_is_memory_reference(instr_get_src((*it)->ins,GET_TRIVIAL_IDX(*it)))) {
                _condCost += MEMORY_COST_BOOST;
            }
            _condCost += 2;
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
    int cost = 0;
    for(auto it=entries.begin(); it!=entries.end(); ++it) {
        cost += estimate_cost((*it)->ins);
    }
    return cost;
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

// void DFG::prune_to_interest() {
//     std::vector<DFGNode*> interest;
//     interest.reserve(entries.size());
//     for(auto it=entries.begin(); it!=entries.end(); ++it) {
//         if((*it)->marker!=-1 || (*it)->principle!=0 || (*it)->result!=UNKNOWN) {
//             interest.push_back(*it);
//         } else {
//             free_DFGNode(*it, need_destroy);
//         }
//     }
//     entries.clear();
//     entries = interest;
// }

#ifdef MAX_STRING_BUF_SIZE
#define MAX_STRING_BUF_SIZE_BK MAX_STRING_BUF_SIZE
#undef MAX_STRING_BUF_SIZE_BK
#endif
#define MAX_STRING_BUF_SIZE 100
std::string getStaticInstrInfo(void* drcontext, instr_t* ins) {
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
        return std::string(rptr);
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
    return std::string(rptr);
}
#undef MAX_STRING_BUF_SIZE
#ifdef MAX_STRING_BUF_SIZE_BK
#define MAX_STRING_BUF_SIZE MAX_STRING_BUF_SIZE_BK
#undef MAX_STRING_BUF_SIZE_BK
#endif

void print_DFGNode(void* drcontext, file_t file, DFGNode* node) {
    instr_disassemble(drcontext, node->ins, file);
    dr_fprintf(file, ", children={ ");
    std::list<DFGNode*>::iterator pos;
    for(auto it=node->children.begin(); it!=node->children.end(); ++it) {
        dr_fprintf(file, "<%d, %d> ", it->first, it->second->idx);
    }
    dr_fprintf(file, "}, parents={ ");
    for(auto it=node->parents.begin(); it!=node->parents.end(); ++it) {
        dr_fprintf(file, "<%d, %d> ", it->first, it->second->idx);
    }
    dr_fprintf(file, "}, marker=<%d,%s>, principle=<bw=%d,heavy=%d,propagated=%d>", 
        GET_TRIVIAL_IDX(node), getConditionValString(GET_TRIVIAL_COND(node)), 
        IS_IN_BACKWARD_SLICE(node), IS_HEAVY(node), IS_PROPAGATED(node));
    dr_fprintf(file, ", isTrivial=%d ", node->isTrivial);
    if(node->marker!=-1) {
        dr_fprintf(file, "<<< Singular");
    }
    dr_fprintf(file, "\n");
}

void DFG::disassemble() {
    int n = 0;
    void* drcontext = dr_get_current_drcontext();
    for(auto it=entries.begin(); it!=entries.end(); ++it, ++n) {
        if(!(*it)->info) {
            (*it)->info = new std::string(getStaticInstrInfo(drcontext, (*it)->ins));
        }
    }
}

void DFG::print_detail(file_t file) {
    void* drcontext = dr_get_current_drcontext();
    int i=0;
    for(auto it=entries.begin(); it!=entries.end(); ++it, ++i) {
        dr_fprintf(file, "[%d] ", i);
        print_DFGNode(drcontext, file, (*it));
    }
}

void DFG::print_summary(file_t file) {
    int n = 0;
    void* drcontext = dr_get_current_drcontext();
    char buf[50];
    for(auto it=entries.begin(); it!=entries.end(); ++it, ++n) {
        if((*it)->marker!=-1) {
            assert(GET_TRIVIAL_IDX(*it) >= 0);
            assert(GET_TRIVIAL_IDX(*it) < instr_num_srcs((*it)->ins));
            size_t step = opnd_disassemble_to_buffer(
              drcontext, instr_get_src((*it)->ins, GET_TRIVIAL_IDX(*it)), buf,
              50);
            assert(step < 50);
            dr_fprintf(file, ">>> SINGULAR TRIVIAL: opnd=%s, cond=%s\n", 
                buf, getConditionValString(GET_TRIVIAL_COND(*it)));
        }
        std::string res = "";
        if(IS_DEAD(*it)) { res += "[D]"; }
        if(IS_HEAVY(*it)) { res += "[H]"; }
        if(IS_PROPAGATED(*it)) { res += "[P]"; }
        if(IS_IN_BACKWARD_SLICE(*it)) { res += "[B]"; }
        dr_fprintf(file, "\t[%d] %s %s\n", n, (*it)->info!=NULL?(*it)->info->c_str():"<no info>", res.c_str());
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

void DFG::forward_analysis() {
    // TODO: track stack values for DFG analysis
    DFGNode* track_reg_gpr[DR_NUM_GPR_REGS] = {0};
    DFGNode* track_reg_simd[DR_NUM_SIMD_VECTOR_REGS] = {0};
    std::vector<DFGNode*> track_stack;
    std::vector<DFGNode*> track_stack_redzone;
    for(auto it=entries.begin(); it!=entries.end(); ++it) {
        int num_srcs = instr_num_srcs((*it)->ins);
        for(int i=0; i<num_srcs; ++i) {
            opnd_t opnd = instr_get_src((*it)->ins, i);
            if(opnd_is_reg(opnd)) {
                reg_id_t reg_use = opnd_get_reg(opnd);
                DFGNode* p = NULL;
                if(reg_is_gpr(reg_use)) {
                    p = track_reg_gpr[reg_resize_to_opsz(reg_use, OPSZ_PTR)-DR_REG_START_GPR];
                } else if(reg_is_simd(reg_use)) {
                    p = track_reg_simd[reg_resize_to_xmm(reg_use)-DR_REG_START_XMM];
                }
                if(p) {
                    (*it)->parents.push_back(std::make_pair(i, p));
                    p->children.push_back(std::make_pair(i, *it));
                }
            } else if(opnd_is_near_base_disp(opnd) && opnd_num_regs_used(opnd)==1) {
                // check if it is stack memory reference
                reg_id_t reg_use = opnd_get_reg_used(opnd, 0);
                if(reg_use == DR_STACK_REG) {
                    int off = opnd_get_disp(opnd);
                    // stack offset should be negative
                    if(off>=0 && (size_t)off<track_stack.size()) {
                        DFGNode* p = track_stack[off];
                        if(p) {
                            (*it)->parents.push_back(std::make_pair(i, p));
                            p->children.push_back(std::make_pair(i, *it));
                        }
                    } else if(off<0) {
                        off = -off;
                        if((size_t)off<track_stack_redzone.size()) {
                            DFGNode* p = track_stack_redzone[off];
                            if(p) {
                                (*it)->parents.push_back(std::make_pair(i, p));
                                p->children.push_back(std::make_pair(i, *it));
                            }
                        }
                    }
                }
            }
        }
        int num_dsts = instr_num_dsts((*it)->ins);
        for(int i=0; i<num_dsts; ++i) {
            opnd_t opnd = instr_get_dst((*it)->ins, i);
            // we ignore the non-register dst opnd
            if(opnd_is_reg(opnd)) {
                reg_id_t reg_def = opnd_get_reg(opnd);
                // we naively lose track of stack when stack reg is updated.
                // TODO: stack reg may be tracked to enumurate common update cases: call, sub, add, ...
                if(reg_def == DR_STACK_REG) {
                    track_stack.clear();
                }
                if(reg_is_gpr(reg_def)) {
                    track_reg_gpr[reg_resize_to_opsz(reg_def, OPSZ_PTR)-DR_REG_START_GPR] = (*it);
                } else if(reg_is_simd(reg_def)) {
                    track_reg_simd[reg_resize_to_xmm(reg_def)-DR_REG_START_XMM] = (*it);
                }
            } else if(opnd_is_near_base_disp(opnd) && opnd_num_regs_used(opnd)==1) {
                // check if it is stack memory reference
                reg_id_t reg_use = opnd_get_reg_used(opnd, 0);
                if(reg_use == DR_STACK_REG) {
                    int off = opnd_get_disp(opnd);
                    // stack offset should be positive
                    if(off>=0) {
                        int size = opnd_size_in_bytes(opnd_get_size(opnd));
                        int end = off + size;
                        if(track_stack.size()<(size_t)end) {
                            track_stack.resize(end, 0);
                        }
                        for(int i=off; i<end; ++i) {
                            track_stack[i] = (*it);
                        }
                    } else {
                        off = -off;
                        int size = opnd_size_in_bytes(opnd_get_size(opnd));
                        int end = off + size;
                        if(track_stack_redzone.size()<(size_t)end) {
                            track_stack_redzone.resize(end, 0);
                        }
                        for(int i=off; i<end; ++i) {
                            track_stack_redzone[i] = (*it);
                        }
                    }
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
    forward_analysis();
}

DFG::DFG(DFG* dfg, bool deep_copy) {
    _cost = 0;
    need_destroy = deep_copy;
    need_free_condlist = false;
    if(deep_copy) {
        for(auto it=dfg->entries.begin(); it!=dfg->entries.end(); ++it) {
            DFGNode* node_clone = new DFGNode();
            node_clone->idx = (*it)->idx;
            node_clone->ins = instr_clone(dr_get_current_drcontext(), (*it)->ins);
            node_clone->isTrivial = (*it)->isTrivial;
            node_clone->condlist = (*it)->condlist;
            node_clone->info = (*it)->info;
            entries.push_back(node_clone);
        }
    } else {
        for(auto it=dfg->entries.begin(); it!=dfg->entries.end(); ++it) {
            DFGNode* node_clone = new DFGNode();
            node_clone->idx = (*it)->idx;
            node_clone->ins = (*it)->ins;
            node_clone->isTrivial = (*it)->isTrivial;
            node_clone->condlist = (*it)->condlist;
            node_clone->info = (*it)->info;
            entries.push_back(node_clone);
        }
    }
    forward_analysis();
}

DFG::DFG(instrlist_t* bb) {
    _cost = 0;
    need_destroy = false;
    need_free_condlist = true;
    // forward analysis to construct DFG
    // 1) allocate all nodes with control flow
    int idx = 0;
    for (instr_t* instr = instrlist_first(bb); instr != NULL; instr = instr_get_next(instr)) {
        if(!instr_is_app(instr)) continue;
        // TODO: continue DFG construction if jump target is statically known
        DFGNode* new_node = new DFGNode();
        new_node->idx = idx; ++idx;
        new_node->ins = instr;
        new_node->isTrivial = false;
        new_node->info = NULL;
        int num_srcs = instr_num_srcs(instr);
        // only further check triviality when it has source operands
        if(num_srcs>0) {
            new_node->condlist = (ConditionList_t**)dr_global_alloc(sizeof(ConditionList_t*)*num_srcs);
            for(int i=0; i<num_srcs; ++i) {
                if(TrivialOpTable::getTrivialConditionList(instr, i, &(new_node->condlist[i]))) {
                    new_node->isTrivial = true;
                } else {
                    new_node->condlist[i] = NULL;
                }
            }
        }
#ifdef DEBUG_TRIVIAL
        int opcode_check = instr_get_opcode(instr);
        if(trivial_op_table.find(opcode_check) != trivial_op_table.end()) {
            dr_fprintf(STDERR, "Trivial found for opcode: %d (%s): isTrivial=%d, instr=", opcode_check, trivial_op_str_table[opcode_check].c_str(), new_node->isTrivial);
        } else {
            dr_fprintf(STDERR, "Trivial not found for opcode: %d (%s): isTrivial=%d, instr=", opcode_check, "nil", new_node->isTrivial);
        }
        instr_disassemble(dr_get_current_drcontext(), new_node->ins, STDOUT);
        dr_fprintf(STDOUT, "\n");
#endif
        entries.push_back(new_node);
    }
    // 2) def-use analysis to add directed links between all allocated nodes along control flow
    forward_analysis();
}

// mark all parent nodes as nodes within backward slice
void mark_backward(DFGNode* node) {
    SET_BACKWARD_SLICE(node);
    for(auto p=node->parents.begin(); p!=node->parents.end(); ++p) {
        mark_backward(p->second);
    }
}

void mark_backward_if_absorbing(DFGNode* entry, Condition_t* cond) {
    if(cond && cond->attr==Absorbing) {
        for(auto p=entry->parents.begin(); p!=entry->parents.end(); ++p) {
            if(p->first==cond->backward_idx) {
                mark_backward(p->second);
            }
        }
    }
}

void mark_dead_backward(DFG* dfg) {
    for(auto it=dfg->entries.rbegin(); it!=dfg->entries.rend(); ++it) {
        if(IS_IN_BACKWARD_SLICE(*it)) {
            bool isDead = true;
            // only dead when all children are dead
            for(auto c=(*it)->children.begin(); c!=(*it)->children.end(); ++c) {
                if(!IS_DEAD(c->second)) {
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

typedef struct {
    // trivial operand
    opnd_t opnd;
    int size;
    // trivial condition
    // ConditionVal_t cond;
    int esize;
    // is floating
    bool is_float;
    // entry node
    instr_t* entry;
} trivial_entry_t;

typedef struct {
    trivial_entry_t trivial_entry;
    // DFG log ctxt
    int DFG_ctxt;
} individual_trivial_entry_t;

#define MAXIMUM_COMBINED_VARS 4

typedef struct {
    std::list<trivial_entry_t> trivial_entries;
    // DFG log ctxt
    int DFG_ctxt;
} combined_trivial_entry_t;

// list of trivial detection entries needed for trace schedule
typedef std::list<individual_trivial_entry_t*> trivial_entry_list_t;
typedef std::list<combined_trivial_entry_t*> combined_trivial_entry_list_t;

void telist_clear(trivial_entry_list_t* telist) {
    for(auto it=telist->begin(); it!=telist->end(); ++it) {
        delete (*it);
    }
    telist->clear();
}

void comb_telist_clear(combined_trivial_entry_list_t* comb_telist) {
    for(auto it=comb_telist->begin(); it!=comb_telist->end(); ++it) {
        delete (*it);
    }
    comb_telist->clear();
}

struct trivial_operand_t {
    opnd_t opnd;
    ConditionVal_t cond;
    int insert_pt;
};

struct DFGLog {
    // registered triviality function
    int8_t combinedNum;
    union {
        ConditionVal_t val;
        // trivialNum = trivialFuncs[mode](val)
        //trivial_func_t* trivialFuncs;
        // trivialNum = trivialCombineFuncs[combIdx][mode](val);
        trivial_func_t** trivialCombineFuncs;
    };
    // overall cost diff gains
    int32_t benifit;
    // detailed metrics for each principle
    int32_t absChainedCost;
    int32_t bwd_slice_cost;
    int32_t heavyCost;
    int bb_idx;
    DFG* detail;
};

static inline bool DFGLogCompare(const struct DFGLog &first, const struct DFGLog &second) {
    return first.benifit > second.benifit ? true : false;
}

void print_DFGLog(file_t file, DFGLog* log) {
    dr_fprintf(file, "======= DFGLog from Thread %d =======\n", dr_get_thread_id(dr_get_current_drcontext()));
    dr_fprintf(file, "benifit: %d\n", log->benifit);
    dr_fprintf(file, "absChainedCost: %d\n", log->absChainedCost);
    dr_fprintf(file, "bwd_slice_cost: %d\n", log->bwd_slice_cost);
    dr_fprintf(file, "heavyCost: %d\n", log->heavyCost);
    dr_fprintf(file, "Combined Trivial Num: %d\n", log->combinedNum);
    dr_fprintf(file, "==> detailed info: \n");
    log->detail->print_summary(file);
    dr_fprintf(file, "=====================================\n");
}

// 4M
#define MAX_DFGLOG_NUM (1<<22)

static int DFGLogList_curr = -1;
static DFGLog DFGLogList[MAX_DFGLOG_NUM];

DFGLog* getDFGLog(int idx) {
    return &DFGLogList[idx];
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
            uint64_t boost =  bb_ref[DFGLogList[i].bb_idx];
            totalDFGCost += DFGLogList[i].detail->estimate() * boost;
            benifit += DFGLogList[i].benifit * boost;
            chained += DFGLogList[i].absChainedCost * boost;
            bwdslice += DFGLogList[i].bwd_slice_cost * boost;
            heavy += DFGLogList[i].heavyCost * boost;
            if(DFGLogList[i].combinedNum) {
                combined += DFGLogList[i].benifit * boost;
            }
        }
        dr_fprintf(file, "Potential Benifit: %.3lf (%ld / %ld)\n", 100.0*(double)benifit/(double)totalDFGCost, benifit, totalDFGCost);
        dr_fprintf(file, "Chained Rate: %.3lf (%ld / %ld)\n", 100.0*(double)chained/(double)benifit, chained, benifit);
        dr_fprintf(file, "Backward Slice Rate: %.3lf (%ld / %ld)\n", 100.0*(double)bwdslice/(double)benifit, bwdslice, benifit);
        dr_fprintf(file, "Heavy Instruction Rate: %.3lf (%ld / %ld)\n", 100.0*(double)heavy/(double)benifit, heavy, benifit);
        dr_fprintf(file, "Combined Trivial Rate: %.3lf (%ld / %ld)\n", 100.0*(double)combined/(double)benifit, combined, benifit);
        dr_fprintf(file, "----------- Detailed DFG Statistics -------------\n\n");
        if(enable_sort) {
            for(int i=0; i<=DFGLogList_curr; ++i) {
                uint64_t boost =  bb_ref[DFGLogList[i].bb_idx];
                DFGLogList[i].benifit *= boost;
                DFGLogList[i].absChainedCost *= boost;
                DFGLogList[i].bwd_slice_cost *= boost;
                DFGLogList[i].heavyCost *= boost;
            }
            std::sort(DFGLogList, DFGLogList+DFGLogList_curr+1, DFGLogCompare);
        }
        for(int i=0; i<=DFGLogList_curr; ++i) {
            print_DFGLog(file, &DFGLogList[i]);
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
        if(DFGLogList[i].combinedNum) {
            dr_global_free(DFGLogList[i].trivialCombineFuncs, DFGLogList[i].combinedNum*sizeof(trivial_func_t*));
        }
        delete DFGLogList[i].detail;
    }
}

void fillDFGLog(int DFG_ctxt, DFG* annotated, ConditionVal_t val, int size, int esize, bool is_float) {
    DFGLog* log = getDFGLog(DFG_ctxt);
    // annotated->prune_to_interest();
    annotated->disassemble();
    log->detail = annotated;
    log->benifit = annotated->estimate_benifit();
    log->absChainedCost = annotated->estimate_detail(PROPAGATED);
    log->bwd_slice_cost = annotated->estimate_detail(IN_BACKWARD_SLICE);
    log->heavyCost = annotated->estimate_detail(HEAVY);
    log->combinedNum = 0;
    // register handlers
    log->val = val;
    //log->trivialFuncs = TrivialDetectorTable::get(val, size, esize, is_float);
    // assert(log->trivialFuncs!=NULL);
    // assert(log->trivialFuncs[0]!=NULL);
    // assert(!is_float || (log->trivialFuncs[1]!=NULL && log->trivialFuncs[2]!=NULL));
}

bool trivialPropagate(DFG* annotated, std::list<DFGNode*>* chain_entries, std::vector<int>* propagated_instr_index) {
    while(!chain_entries->empty()) {
        DFGNode* node = chain_entries->front();
        ConditionVal_t val = convertRes2CVal(node->result);
        for(auto c=node->children.begin(); c!=node->children.end(); ++c) {
            // if the result is marked as OTHER, check the other_idx opnd is constant or not
            if(c->second->result==OTHER) {
                assert(c->second->other_idx >= 0);
                assert(c->second->other_idx < instr_num_srcs(c->second->ins));
                opnd_t other_opnd = instr_get_src(c->second->ins, c->second->other_idx);
                if(opnd_is_immed(other_opnd)) {
                    ptr_int_t immed_val = opnd_get_immed_int(other_opnd);
                    if(val==IS_ZERO && immed_val==0) {
                        c->second->result = ZERO;
                    } else if(val==IS_ONE && immed_val==1) {
                        c->second->result = ONE;
                    } else if(val==IS_FULL && immed_val==(ptr_int_t)(-1)) {
                        c->second->result = FULL;
                    }
                } else {
                    for(auto p=c->second->parents.begin(); p!=c->second->parents.end(); ++p) {
                        // select other operand
                        if(p->first == c->second->other_idx) {
                            // identical result
                            c->second->result = p->second->result;
                            break;
                        }
                    }
                }
                continue;
            }
            // if the result is not unknown, it must be already propagated
            if(c->second->result!=UNKNOWN) {
                continue;
            }
            // propagate if it is register copy (mov) instruction and the result is determined
            if(instr_is_reg_copy(c->second->ins) && node->result!=OTHER) {
                // propagated, so can be safely marked as dead
                c->second->result = node->result;
                SET_PROPAGATED(c->second);
                SET_DEAD_MARK(c->second);
                chain_entries->push_back(c->second);
                continue;
            }
            // check for trivial condition
            c->second->result = TrivialOpTable::getTrivialResultForCondition(c->second->ins, c->first, val, &(c->second->other_idx));
            if(c->second->result == INVALID) {
                DPRINTF("--> trivialPropagate: SKIP CHILD: ");
                IF_DEBUG_TRIVIAL(instr_disassemble(dr_get_current_drcontext(), c->second->ins, STDOUT));
                DPRINTF(", propagated result %s (as cond %s) is result in INVALID!\n", getResultVallString(node->result), getConditionValString(val));
                return false;
            } else if(c->second->result != UNKNOWN) {
                if(propagated_instr_index) {
                    SET((*propagated_instr_index)[c->second->idx], val);
                }
                // mark as absorbing transitivity
                SET_PROPAGATED(c->second);
                SET_DEAD_MARK(c->second);
                // backward slice marking if absorbing
                mark_backward_if_absorbing(c->second,
                                           TrivialOpTable::getConditionWithVal(
                                               c->second->ins, c->first, val));
                chain_entries->push_back(c->second);
            }
        }
        // pop out processed entry
        chain_entries->pop_front();
    }
    // all triviality propagated, mark dead backward slices
    mark_dead_backward(annotated);
    return true;
}

bool trivial_cond_marking(DFG* annotated, DFGNode* entry, int i, Condition_t cond, std::vector<int>* propagated_instr_index, int threshold) {
    ConditionVal_t val = cond.val;
    ResultVal_t res = cond.res;
    Attribute_t attr= cond.attr;
    assert(i >= 0);
    assert(i < instr_num_srcs(entry->ins));
    opnd_t ref = instr_get_src(entry->ins, i);
    SET_MARKER(entry, i, val);
    assert(GET_TRIVIAL_IDX(entry) >= 0);
    assert(GET_TRIVIAL_IDX(entry) < instr_num_srcs(entry->ins));
    // Principle 1: heavy instruction
    if(estimate_cost(entry->ins) >= threshold) {
        SET_HEAVY(entry);
    }
    // Principle 2: absorbing transitivity
    entry->result = res;
    entry->other_idx = cond.other_idx;
    std::list<DFGNode*> chain_entries;
    if(opnd_is_reg(ref) && !entry->parents.empty()) {
        // multiple instructions may share the same trivial condition
        // check backward
        for(auto p=entry->parents.begin(); p!=entry->parents.end(); ++p) {
            // found a parent defines this trivial condition, mark all his trivial children
            if(p->first == i) {
                DFGNode* pnode = p->second;
                for(auto c=pnode->children.begin(); c!=pnode->children.end(); ++c) {
                    c->second->result = TrivialOpTable::getTrivialResultForCondition(c->second->ins, c->first, val, &(c->second->other_idx));
                    if(c->second->result == INVALID) {
                        DPRINTF("--> trivial_cond_marking: SKIP CHILD: ");
                        IF_DEBUG_TRIVIAL(instr_disassemble(dr_get_current_drcontext(), c->second->ins, STDOUT));
                        DPRINTF(", cond[%d](%s) is result in INVALID!\n", i, getConditionValString(val));
                        return false;
                    } else if(c->second->result != UNKNOWN) {
                        // update when needed
                        if(propagated_instr_index) {
                            SET((*propagated_instr_index)[c->second->idx], val);
                        }
                        SET_PROPAGATED(c->second);
                        SET_DEAD_MARK(c->second);
                        mark_backward_if_absorbing(
                            c->second, TrivialOpTable::getConditionWithVal(
                                           c->second->ins, c->first, val));
                        chain_entries.push_back(c->second);
                    }
                }
            }
        }
    } else {
        SET_PROPAGATED(entry);
        SET_DEAD_MARK(entry);
        mark_backward_if_absorbing(entry, &cond);
        chain_entries.push_back(entry);
    }
    // Principle 2 & 3: absorbing transitivity & large backward slice
    if (!trivialPropagate(annotated, &chain_entries,
                            propagated_instr_index)) {
        DPRINTF("--> trivial_cond_marking: PROPAGATE FAILED\n");
        IF_DEBUG_TRIVIAL(annotated->print_detail(STDOUT));
        return false;
    }
    return true;
}

void storeIndividualDFGLog(trivial_entry_list_t* trivial_entry_list, DFG* annotated, instr_t* entry, opnd_t ref, ConditionVal_t val, int size, int esize, bool is_float) {
    // log this singular trivial condition and annotated dfg
    individual_trivial_entry_t *te = new individual_trivial_entry_t();
    te->trivial_entry.opnd = ref;
    te->trivial_entry.size = size;
    // te->trivial_entry.cond = val;
    te->trivial_entry.esize = esize;
    te->trivial_entry.is_float = is_float;
    te->trivial_entry.entry = entry;
    trivial_entry_list->push_back(te);
    // allocate a DFGLog and fill in
    int DFG_ctxt = allocateDFGList();
    te->DFG_ctxt = DFG_ctxt;
    fillDFGLog(DFG_ctxt, annotated, val, size, esize, is_float);
}

void discoverSingularTrivialEntries(DFG* dfg, trivial_entry_list_t* trivial_entry_list, int threshold) {
    int idx = 0;
    std::vector<int> propagated_instr_index;
    propagated_instr_index.resize(dfg->entries.size(), 0);
    DFG* cloned_dfg = NULL;
    // check all nodes in dfg to discover singular trivial entry
    for(auto it=dfg->entries.begin(); it!=dfg->entries.end(); ++it, ++idx) {
        DFGNode* node = (*it);
        instr_t* instr = node->ins;
        // For each trivial instruction, check for three principles
        if(node->isTrivial) {
            int32_t mycost = estimate_cost(instr);
            int num_srcs = instr_num_srcs(instr);
            for(int i=0; i<num_srcs; ++i) {
                opnd_t ref = instr_get_src(instr, i);
                // only handle configurable register/memory operand
                if (!opnd_is_reg(ref) && !opnd_is_memory_reference(ref)) {
                    continue;
                }
                if(opnd_is_reg(ref) && opnd_get_reg(ref)==DR_STACK_REG) {
                    // stack reg is always non-trivial
                    continue;
                }
                ConditionList_t* condlist = node->condlist[i];
                if(condlist) {
                    // for each trivial condition
                    for(size_t j=0; j<condlist->second.size(); ++j) {
                        ConditionVal_t val = condlist->second[j].val;
                        // skip if it can be propageted by previous trivial instruction
                        if(IS_SET(propagated_instr_index[idx], val)) {
                            continue;
                        }
                        SET(propagated_instr_index[idx], val);
                        // clone a dfg for annotation and logging
                        DFG* annotated;
                        if(cloned_dfg) {
                            annotated = new DFG(cloned_dfg, false);
                        } else {
                            annotated = new DFG(dfg, true);
                        }
                        DFGNode* entry = annotated->entries[idx];
                        if(trivial_cond_marking(annotated, entry, i, condlist->second[j], &propagated_instr_index, threshold) &&
                           annotated->estimate_benifit() > 0) {
                            IF_DEBUG_TRIVIAL(
                               instr_disassemble(dr_get_current_drcontext(), node->ins, STDOUT);
                               dr_fprintf(STDOUT, ": i=%d, val=%s, check size=%d, esize=%d, is_float=%d\n", i, getConditionValString(val), condlist->first.check_size, condlist->first.esize, condlist->first.is_float);
                            );
                            storeIndividualDFGLog(trivial_entry_list, annotated, node->ins, ref, val, condlist->first.check_size, condlist->first.esize, condlist->first.is_float);
                            cloned_dfg = annotated;
                        } else {
                            DPRINTF("--> discoverSingularTrivialEntries: trivial_cond_marking failed or benifit is negative: %d\n", annotated->estimate_benifit());
                            delete annotated;
                        }
                    }
                }
            }
        }
    }
}

struct trivial_root_t {
    int i;
    int esize;
    int size;
    Condition_t cond;
    bool is_float;
    DFGNode* node;
};

bool operator== (const trivial_root_t& first, const trivial_root_t& second) {
    return (first.i==second.i) && (first.esize==second.esize) &&
     (first.cond.attr==second.cond.attr) && (first.cond.val==second.cond.val) &&
     (first.cond.res==second.cond.res) && (first.is_float==second.is_float) &&
     (first.node==second.node);
}

bool trivial_root_discovery(DFGNode* trivialNode, int i, ConditionVal_t val, std::list<trivial_root_t>* roots) {
    bool discovered = false;
    // backward
    for(auto p=trivialNode->parents.begin(); p!=trivialNode->parents.end(); ++p) {
        // find parent node defines this value
        if(p->first==i && p->second->isTrivial) {
            // query which condition should be satisfied to generate the trivial result
            instr_t* instr = p->second->ins;
            int num_srcs = instr_num_srcs(instr);
            for(int i=0; i<num_srcs; ++i) {
                ConditionList_t* condlist = p->second->condlist[i];
                if(condlist) {
                    opnd_t ref = instr_get_src(instr, i);
                    // for each trivial condition
                    for(size_t j=0; j<condlist->second.size(); ++j) {
                        if(COND_RES_MATCHES(val, condlist->second[j].res)) {
                            discovered = true;
                            if(!trivial_root_discovery(p->second, i, condlist->second[j].val, roots)) {
                                trivial_root_t tr = {i, condlist->first.esize, condlist->first.check_size, condlist->second[j], condlist->first.is_float, p->second};
                                roots->push_back(tr);
                                return true;
                            }
                        }
                    }
                }
            }
        }
    }
    return discovered;
}

bool tryAbsBreakPtFwd(DFGNode* node, ConditionVal_t val, int max_chain, std::list<trivial_root_t>* pts) {
    DPRINTF("--> Enter tryAbsBreakPtFwd: max_chain=%d, val=%s, node ins=", max_chain, getConditionValString(val));
    IF_DEBUG_TRIVIAL(instr_disassemble(dr_get_current_drcontext(), node->ins, STDOUT));
    DPRINTF("\n");
    if(max_chain>0) {
        IF_DEBUG_TRIVIAL(int cidx = 0);
        for(auto c=node->children.begin(); c!=node->children.end(); ++c IF_DEBUG_TRIVIAL(, ++cidx)) {
            int ci = (*c).first;
            DFGNode* cnode = (*c).second;

            DPRINTF("--> child node [%d](in %d) to src %d ins: ", cidx, node->children.size(), ci);
            IF_DEBUG_TRIVIAL(instr_disassemble(dr_get_current_drcontext(), cnode->ins, STDOUT));
            DPRINTF("\n");

            if(!cnode->isTrivial) {
                DPRINTF("--> SKIP not trivial!\n");
                continue;
            }
            ConditionList_t* ccl = cnode->condlist[ci];
            if(ccl) {
                for(size_t cj=0; cj<ccl->second.size(); ++cj) {
                    DPRINTF("--> Check condlist[%d]: val %s (attr=%d, res=%s), expected %s\n", cj, getConditionValString(ccl->second[cj].val), ccl->second[cj].attr, getResultVallString(ccl->second[cj].res), getConditionValString(val));
                    if(ccl->second[cj].val == val) {
                        switch(ccl->second[cj].attr) {
                            case Never:
                                return false;
                            case Absorbing:
                                DPRINTF("--> Check passed. Absorbing Breakpoint matched.\n");
                                return true;
                            case Function:
                                if( ccl->second[cj].res==ZERO ||
                                    ccl->second[cj].res==ONE ||
                                    ccl->second[cj].res==FULL) {
                                    DPRINTF("--> Check passed. Absorbing Breakpoint matched (Function).\n");
                                    return true;
                                }
                            default:
                                continue;
                        }
                    }
                }
            }
            // chained breakpoint
            int num_srcs = instr_num_srcs(cnode->ins);
            for(int i=0; i<num_srcs; ++i) {
                if(i==ci) continue;
                ccl = cnode->condlist[i];
                if (ccl) {
                    for(size_t cj=0; cj<ccl->second.size(); ++cj) {
                        if(ccl->second[cj].res==OTHER || ccl->second[cj].attr==Identical) {
                            if(tryAbsBreakPtFwd(cnode, val, max_chain-1, pts)) {
                                trivial_root_t tr = {i, ccl->first.esize, ccl->first.check_size, ccl->second[cj], ccl->first.is_float, cnode};
                                pts->push_back(tr);
                                return true;
                            }
                        }
                    }
                }
            }
        }
    }
    DPRINTF("--> Exit tryAbsBreakPtFwd: not found.\n");
    return false;
}

void fillCombinedDFGLog(int DFG_ctxt, DFG* annotated, std::list<trivial_root_t>* roots) {
    DFGLog* log = getDFGLog(DFG_ctxt);
    // annotated->prune_to_interest();
    annotated->disassemble();
    log->detail = annotated;
    log->benifit = annotated->estimate_benifit();
    log->absChainedCost = annotated->estimate_detail(PROPAGATED);
    log->bwd_slice_cost = annotated->estimate_detail(IN_BACKWARD_SLICE);
    log->heavyCost = annotated->estimate_detail(HEAVY);
    // register handlers
    log->combinedNum = roots->size();
    log->trivialCombineFuncs = (trivial_func_t**)dr_global_alloc(log->combinedNum*sizeof(trivial_func_t*));
    int ri = 0;
    for(auto rit=roots->begin(); rit!=roots->end(); ++rit, ++ri) {
        log->trivialCombineFuncs[ri] = TrivialDetectorTable::get((*rit).cond.val, (*rit).size, (*rit).esize, (*rit).is_float);
        assert(log->trivialCombineFuncs[ri]!=NULL);
        assert(log->trivialCombineFuncs[ri][0]!=NULL);
        assert(!(*rit).is_float || (log->trivialCombineFuncs[ri][1]!=NULL && log->trivialCombineFuncs[ri][2]!=NULL));
    }
}

DFG* tryStoreCombinedDFGLog(DFG* dfg, bool deep_copy, std::list<trivial_root_t>* roots, int threshold, combined_trivial_entry_list_t* trivial_entry_list, std::vector<int>* propagated_instr_index) {
    DFG* annotated = new DFG(dfg, deep_copy);
    for(auto rit=roots->begin(); rit!=roots->end(); ++rit) {
        DFGNode* entry = annotated->entries[(*rit).node->idx];
        DPRINTF("--> trivial cond marking for node: ");
        IF_DEBUG_TRIVIAL(print_DFGNode(dr_get_current_drcontext(), STDOUT, entry));
        if(!trivial_cond_marking(annotated, entry, (*rit).i, (*rit).cond, NULL, threshold)) {
            DPRINTF("--> trivial_cond_marking failed!\n");
            delete annotated;
            return NULL;
        }
    }
    if(annotated) {
        if(annotated->estimate_benifit() <= 0) {
            DPRINTF("--> tryStoreCombinedDFGLog: benifit not positive: %d\n", annotated->estimate_benifit());
            IF_DEBUG_TRIVIAL(annotated->print_detail(STDOUT));
            delete annotated;
            return NULL;
        }
        // log this singular trivial condition and annotated dfg
        combined_trivial_entry_t *cte = new combined_trivial_entry_t();
        for(auto rit=roots->begin(); rit!=roots->end(); ++rit) {
            trivial_entry_t te;
            assert((*rit).i >= 0);
            assert((*rit).i < instr_num_srcs((*rit).node->ins));
            te.opnd = instr_get_src((*rit).node->ins, (*rit).i);
            // te.cond = (*rit).cond.val;
            te.size = (*rit).size;
            te.esize = (*rit).esize;
            te.is_float = (*rit).is_float;
            te.entry = (*rit).node->ins;
            cte->trivial_entries.push_back(te);
        }
        trivial_entry_list->push_back(cte);
        // allocate a DFGLog and fill in
        int DFG_ctxt = allocateDFGList();
        cte->DFG_ctxt = DFG_ctxt;
        fillCombinedDFGLog(DFG_ctxt, annotated, roots);
    }
    return annotated;
}

// Principle 4: Singular Trivial Condition can link two absorbing instruction I1, I2 together, where
// I1 can generate condition to validate absorbing I2. The STC can be chained max_chain times at maximum
// Discover combined trivial condition from backward combination from identical trivial condition
void discoverCombinedTrivial(DFG* dfg, combined_trivial_entry_list_t* comb_telist, int max_chain, int threshold) {
    int idx = 0;
    std::list<trivial_root_t> roots;
    std::list<trivial_root_t> self_roots;
    std::vector<int> propagated_instr_index;
    DFG* cloned_dfg = NULL;
    // check all nodes in dfg to discover singular trivial entry
    for(auto it=dfg->entries.begin(); it!=dfg->entries.end(); ++it, ++idx) {
        ConditionList_t* condlist;
        DFGNode* node = (*it);
        instr_t* instr = node->ins;
        int num_srcs = instr_num_srcs(instr);
        DPRINTF("--------------------------\n");
        DPRINTF("Handling node [%d]: ", idx);
        IF_DEBUG_TRIVIAL(instr_disassemble(dr_get_current_drcontext(), instr, STDOUT));
        DPRINTF("\n");
        if(num_srcs!=2 || !node->isTrivial) {
            DPRINTF("SKIP node: num_srcs=%d, isTrivial=%d\n", num_srcs, node->isTrivial);
            continue;
        }
        // For each trivial instruction, check for three principles
        int32_t mycost = estimate_cost(instr);
        for(int i=0; i<num_srcs; ++i) {
            opnd_t ref = instr_get_src(instr, i);
            IF_DEBUG_TRIVIAL(char buf[50]);
            IF_DEBUG_TRIVIAL(opnd_disassemble_to_buffer(dr_get_current_drcontext(), ref, buf, 50));
            // only handle configurable register/memory operand
            if (!opnd_is_reg(ref) && !opnd_is_memory_reference(ref)) {
                DPRINTF("SKIP src %d (%s): is_reg=%d, is_memory=%d\n", i, buf, opnd_is_reg(ref), opnd_is_memory_reference(ref));
                continue;
            }
            condlist = node->condlist[i];
            if (condlist) {
                // for each trivial condition
                for(size_t j=0; j<condlist->second.size(); ++j) {
                    ConditionVal_t val = condlist->second[j].val;
                    // skip if it is not identical
                    if (condlist->second[j].res!=OTHER && condlist->second[j].attr != Identical) {
                        DPRINTF("SKIP cond[%d] for src[%d](%s): attr=%d, res=%s!\n", j, i, buf, condlist->second[j].attr, getResultVallString(condlist->second[j].res));
                        continue;
                    }
                    DPRINTF("^^ Handling opnd[%d](%s) with cond: %s\n", i, buf, getConditionValString(val));
                    self_roots.clear();
                    if(!trivial_root_discovery(node, i, condlist->second[j].val, &self_roots)) {
                        DPRINTF("backward analysis for trivial operand [%d](%s) root cause not found. Add self\n", i, buf);
                        trivial_root_t tr = {i, condlist->first.esize, condlist->first.check_size, condlist->second[j], condlist->first.is_float, node};
                        self_roots.push_back(tr);
                    } else {
                        self_roots.unique();
                        DPRINTF("=> trivial root found: num=%d\n", self_roots.size());
                        IF_DEBUG_TRIVIAL( int ridx=0;
                        for(auto ri = self_roots.begin(); ri!=self_roots.end(); ++ri, ++ridx) {
                            DPRINTF("=> trivial root[%d]: ", ridx);
                            IF_DEBUG_TRIVIAL(instr_disassemble(dr_get_current_drcontext(), (*ri).node->ins, STDOUT));
                            DPRINTF("\n");
                        } );
                        IF_DEBUG_TRIVIAL(assert(self_roots.size()==1));
                    }
                    int other_idx = condlist->second[j].other_idx;
                    for(auto p=node->parents.begin(); p!=node->parents.end(); ++p) {
                        // skip if this parent generates the trivial condition for this check
                        if((*p).first!=other_idx) {
                            DPRINTF("SKIP SRC (%d) not matched for OTHER_IDX (%d)\n", (*p).first, other_idx);
                            continue;
                        }
                        DPRINTF("Checking parent: ");
                        IF_DEBUG_TRIVIAL(instr_disassemble(dr_get_current_drcontext(), (*p).second->ins, STDOUT));
                        DPRINTF("\n");
                        int resMask = TrivialOpTable::getAbsorbingResult((*p).second->ins);
                        DPRINTF("resMask=%x: ZERO=%d, ONE=%d, FULL=%d\n", resMask, IS_SET(resMask, ZERO)?1:0, IS_SET(resMask, ONE)?1:0, IS_SET(resMask, FULL)?1:0);
                        roots.assign(self_roots.begin(), self_roots.end());
                        if( val==IS_ZERO && (IS_SET(resMask, ZERO) && tryAbsBreakPtFwd(node, IS_ZERO, max_chain, &roots)) ) {
                            IF_DEBUG_TRIVIAL(bool check =) trivial_root_discovery(node, other_idx, IS_ZERO, &roots);
                            IF_DEBUG_TRIVIAL(assert(check && "trivial root discovery failed!\n"));
                            DPRINTF("Absorbing Breakpoint Found (IS_ZERO)\n");
                            DFG* template_dfg;
                            if(cloned_dfg) template_dfg = cloned_dfg;
                            else          template_dfg = dfg;
                            cloned_dfg = tryStoreCombinedDFGLog(template_dfg, cloned_dfg==NULL, &roots, threshold, comb_telist, &propagated_instr_index);
                        }
                        roots.assign(self_roots.begin(), self_roots.end());
                        if( val==IS_ONE && (IS_SET(resMask, ONE) && tryAbsBreakPtFwd(node, IS_ONE, max_chain, &roots)) ) {
                            IF_DEBUG_TRIVIAL(bool check =) trivial_root_discovery(node, other_idx, IS_ONE, &roots);
                            IF_DEBUG_TRIVIAL(assert(check && "trivial root discovery failed!\n"));
                            DFG* template_dfg;
                            if(cloned_dfg) template_dfg = cloned_dfg;
                            else          template_dfg = dfg;
                            cloned_dfg = tryStoreCombinedDFGLog(template_dfg, cloned_dfg==NULL, &roots, threshold, comb_telist, &propagated_instr_index);
                        }
                        roots.assign(self_roots.begin(), self_roots.end());
                        if( val==IS_FULL && (IS_SET(resMask, FULL) && tryAbsBreakPtFwd(node, IS_FULL, max_chain, &roots)) ) {
                            IF_DEBUG_TRIVIAL(bool check =) trivial_root_discovery(node, other_idx, IS_FULL, &roots);
                            IF_DEBUG_TRIVIAL(assert(check && "trivial root discovery failed!\n"));
                            DFG* template_dfg;
                            if(cloned_dfg) template_dfg = cloned_dfg;
                            else          template_dfg = dfg;
                            cloned_dfg = tryStoreCombinedDFGLog(template_dfg, cloned_dfg==NULL, &roots, threshold, comb_telist, &propagated_instr_index);
                        }
                    }
                }
            } else {
                DPRINTF("SKIP src %d (%s): condlist is null!\n", i, buf);
            }
        }
    }
}

#endif