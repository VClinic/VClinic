#ifndef __TRIVIAL_TABLE_H__
#define __TRIVIAL_TABLE_H__

#include "dr_api.h"
#include "drmgr.h"
#include "drreg.h"
#include "drutil.h"
#include "drsyms.h"
#include "drcctlib.h"
#include "drcctlib_defines.h"
#include "trivial_define.h"
#include <vector>
#include <string>

#define SET(a, i) (a)=((a)|(1<<(i)))
#define IS_SET(a, i) ((a)&(1<<(i)))

enum ConditionVal_t {
    ARBITARY=0,
    IS_ZERO,   // Z
    IS_ONE,    // O
    IS_FULL,   // F
    condval_num
};

inline
const char* getConditionValString(ConditionVal_t val) {
    switch(val) {
        case ARBITARY: return "ARBITARY";
        case IS_ZERO: return "IS_ZERO";
        case IS_ONE: return "IS_ONE";
        case IS_FULL: return "IS_FULL";
        default:
            return "Unknown";
    }
}

enum ResultVal_t {
    INVALID=0,  // Never satisfies
    ZERO,
    ONE,
    FULL,
    OTHER,
    UNKNOWN,
    resval_num
};

inline
ResultVal_t convertCVal2Res(ConditionVal_t val) {
    switch(val) {
        case IS_ZERO: return ZERO;
        case IS_ONE: return ONE;
        case IS_FULL: return FULL;
        default:
            return UNKNOWN;
    }
}

inline
ConditionVal_t convertRes2CVal(ResultVal_t res) {
    switch(res) {
        case ZERO: return IS_ZERO;
        case ONE: return IS_ONE;
        case FULL: return IS_FULL;
        case OTHER: 
        case INVALID:
        case UNKNOWN:
        default:
            return ARBITARY;
    }
}

inline
const char* getResultValString(ResultVal_t val) {
    switch(val) {
        case INVALID: return "INVALID";
        case ZERO: return "ZERO";
        case ONE: return "ONE";
        case FULL: return "FULL";
        case OTHER: return "OTHER";
        case UNKNOWN:
        default:
            return "UNKNOWN";
    }
}

inline
const int getResultVal(ResultVal_t res) {
    switch(res) {
        case ZERO: return 0;
        case ONE: return 1;
        case FULL: return -1;
        default:
            assert(0);
    }
    return 0;
}

#define COND_RES_MATCHES(cond, res) ( ((cond)==IS_ZERO && (res)==ZERO) || ((cond)==IS_ONE && (res)==ONE) || ((cond)==IS_FULL && (res)==FULL) )

// Attributes
enum Attribute_t {
    Absorbing,
    Identical,
    Function,
    Never,
    Attribute_Count
};

#define COND_CHECK_ATTR_ABSORBING (-1)
#define COND_CHECK_ATTR_IDENTICAL (1)

typedef instr_t* (*instr_simplify_func_t)(instr_t* ins_orig);

struct Condition_t {
    Attribute_t     attr;
    ConditionVal_t  val;
    ResultVal_t     res;
    instr_simplify_func_t simplify;
    // >=0 indicates the starting index for backward slice
    int backward_idx;
    // other index for identical condition
    int other_idx;
};

template<ResultVal_t res>
instr_t* simplify_to_const(instr_t* ins_orig) {
    switch(res) {
        case ZERO: 
            return XINST_CREATE_load_int(dr_get_current_drcontext(), instr_get_dst(ins_orig, 0), OPND_CREATE_INT32((ushort)0));
        case ONE: 
            return XINST_CREATE_load_int(dr_get_current_drcontext(), instr_get_dst(ins_orig, 0), OPND_CREATE_INT32((ushort)1));
        case FULL: 
            return XINST_CREATE_load_int(dr_get_current_drcontext(), instr_get_dst(ins_orig, 0), OPND_CREATE_INT32((ushort)(-1)));
        default:
            assert(0);
    }
    return NULL;
}

template<int other_idx>
instr_t* simplify_to_mov(instr_t* ins_orig) {
    return XINST_CREATE_move(dr_get_current_drcontext(), instr_get_dst(ins_orig, 0), instr_get_src(ins_orig, other_idx));
}

struct ConditionListInfo_t {
    bool is_float;
    uint8_t latency; /*if 0, indicates it is ignored*/
    uint8_t esize;
    uint8_t size;
    uint8_t check_size; /* some instructions (e.g., vmulsd) only compute with lower x bytes */
};

typedef std::pair<ConditionListInfo_t, std::vector<Condition_t> > ConditionList_t;
typedef std::vector<ConditionList_t> OpConditionMap_t;
typedef std::vector<OpConditionMap_t> OpConditionMapList_t;
typedef std::unordered_map<int, OpConditionMapList_t> TrivialOpMap_t;

typedef std::unordered_map<int, std::string> TrivialOpStrMap_t;

#define TRIVIAL_OPRAND_ANY (-1)
static TrivialOpMap_t trivial_op_table;
static TrivialOpStrMap_t trivial_op_str_table;

typedef struct {
    std::vector<reg_id_t> reg_in;
    reg_id_t reg_out;
    // trivial conditions
    ConditionList_t** condlist;
    // trivial results
} trivial_func_info_t;
static std::unordered_map<int, trivial_func_info_t> trivial_func_table;

// static bool getTargetFuncName(instr_t* instr, app_pc addr, char* name) {
//     drsym_error_t symres;
//     drsym_info_t sym;
//     char file[MAXIMUM_PATH];
//     module_data_t *data = dr_lookup_module(addr);
//     if (data == NULL) {
// #ifdef DEBUG_TRIVIALSPY
//         dr_fprintf(STDOUT, "dr_lookup_module failed: Unknown target: ");
//         instr_disassemble(dr_get_current_drcontext(), instr, STDOUT);
//         dr_fprintf(STDOUT, ", targ addr=%p\n", addr);
// #endif
//         return false;
//     }
//     sym.struct_size = sizeof(sym);
//     sym.name = name;
//     sym.name_size = MAXIMUM_SYMNAME;
//     sym.file = file;
//     sym.file_size = MAXIMUM_PATH;
//     symres = drsym_lookup_address(data->full_path, addr - data->start, &sym,
//                                 DRSYM_DEFAULT_FLAGS);
//     if (symres == DRSYM_SUCCESS || symres == DRSYM_ERROR_LINE_NOT_AVAILABLE) {
//         return true;
//     }
// #ifdef DEBUG_TRIVIALSPY
//     dr_fprintf(STDOUT, "drsym_lookup_address failed: Unknown target: ");
//     instr_disassemble(dr_get_current_drcontext(), instr, STDOUT);
//     dr_fprintf(STDOUT, ", targ addr=%p\n", addr);
//     dr_fprintf(STDOUT, "data->full_path=%s, start=%p, addr=%p, error=%d(%s)\n", data->full_path, data->start, addr, symres,
//         symres==DRSYM_ERROR_LOAD_FAILED?"DRSYM_ERROR_LOAD_FAILED":(symres==DRSYM_ERROR_SYMBOL_NOT_FOUND?"DRSYM_ERROR_SYMBOL_NOT_FOUND":symres==DRSYM_ERROR_RECURSIVE?"DRSYM_ERROR_RECURSIVE":"OTHER"));
// #endif
//     return false;
// }

class TrivialFuncTable {
    public:
    #include "trivial_func_table_impl.h"
    static void fini() {
        for(auto it=trivial_func_table.begin(); it!=trivial_func_table.end(); ++it) {
            int n_in = it->second.reg_in.size();
            for(int i=0; i<n_in; ++i) {
                delete[] it->second.condlist[i];
            }
            delete[] it->second.condlist;
        }
        trivial_func_table.clear();
    }
//     static int getTrivialFuncTableEntry(instr_t* instr) {
//         if(!instr_is_call(instr)) {
//             return -1;
//         }
//         opnd_t targ = instr_get_target(instr);
//         app_pc addr = 0;
//         if(opnd_is_pc(targ)) {
//             addr = opnd_get_pc(targ);
//         } else if(opnd_is_instr(targ)) {
//             addr = instr_get_app_pc(opnd_get_instr(targ));
//         } else {
// #ifdef DEBUG_TRIVIALSPY
//             dr_fprintf(STDOUT, "Unknown target: ");
//             instr_disassemble(dr_get_current_drcontext(), instr, STDOUT);
//             dr_fprintf(STDOUT, ", targ=");
//             opnd_disassemble(dr_get_current_drcontext(), targ, STDOUT);
//             dr_fprintf(STDOUT, "\n");
// #endif
//             return -1;
//         }
//         char name[MAXIMUM_SYMNAME];
//         if(getTargetFuncName(instr, addr, name)) {
//             return lookup(name);
//         }
//         return -1;
//     }
    static bool getTrivialConditionList(int entry, ConditionList_t*** condlist) {
        auto it = trivial_func_table.find(entry);
        if(it!=trivial_func_table.end()) {
            *condlist = it->second.condlist;
            return true;
        }
        return false;
    }
    // return UNKNOWN when not found
    static ResultVal_t getTrivialResultForCondition(int entry, int trivial_operand, ConditionVal_t val, int8_t* other_idx) {
        assert(trivial_operand>=0);
        assert((size_t)trivial_operand<trivial_func_table[entry].reg_in.size());
        ConditionList_t* condlist = trivial_func_table[entry].condlist[trivial_operand];
        for(size_t j=0; j<condlist->second.size(); ++j) {
            if(condlist->second[j].val==val) {
                *other_idx = condlist->second[j].other_idx;
                return condlist->second[j].res;
            }
        }
        return UNKNOWN;
    }

    static int getAbsorbingResult(int entry) {
        int res = 0;
        int n_in = trivial_func_table[entry].reg_in.size();
        for(int i=0; i<n_in; ++i) {
            ConditionList_t* condlist = trivial_func_table[entry].condlist[i];
            for(size_t j=0; j<condlist->second.size(); ++j) {
                SET(res, condlist->second[j].res);
                DPRINTF("SET result: %d (%d is set)\n", res, condlist->second[j].res);
            }
        }
        return res;
    }

    static Condition_t* getConditionWithVal(int entry, int trivial_operand, ConditionVal_t val) {
        assert(trivial_operand>=0);
        assert((size_t)trivial_operand<trivial_func_table[entry].reg_in.size());
        ConditionList_t* condlist = trivial_func_table[entry].condlist[trivial_operand];
        for(size_t j=0; j<condlist->second.size(); ++j) {
            if(condlist->second[j].val==val) {
                return &condlist->second[j];
            }
        }
        return NULL;
    }
    static reg_id_t getReturnRegister(int entry) {
        auto it = trivial_func_table.find(entry);
        if(it!=trivial_func_table.end()) {
            return it->second.reg_out;
        }
        assert(0 && "Target Function Entry not found!\n");
        return DR_REG_INVALID;
    }
    static std::vector<reg_id_t>& getInputRegisterList(int entry) {
        auto it = trivial_func_table.find(entry);
        if(it!=trivial_func_table.end()) {
            return it->second.reg_in;
        }
        assert(0 && "Target Function Entry not found!\n");
    }
};

struct TrivialOpTable {
    static void initTrivialOpTable() {
        #include "trivial_op_table_impl.h"
    }

    static void finiTrivialOpTable() {
        trivial_op_table.clear();
        trivial_op_str_table.clear();
    }

    static inline __attribute__((always_inline))
    uint8_t getCost(int opcode, uint8_t size) {
        // loopup the cost of the opcode from the trivial table
        TrivialOpMap_t::iterator it = trivial_op_table.find(opcode);
        if(it != trivial_op_table.end()) {
            OpConditionMapList_t& op_cond_map_list = it->second;
            for(size_t i=0; i<op_cond_map_list.size(); ++i) {
                OpConditionMap_t& op_cond_map = op_cond_map_list[i];
                for(size_t i=0; i<op_cond_map.size(); ++i) {
                    ConditionList_t& condlist = op_cond_map[i];
                    if(condlist.first.size==size) {
                        return condlist.first.latency;
                    }
                }
            }
        }
#ifdef DEBUG_WARN_COST
        if(it != trivial_op_table.end()) {
            dr_fprintf(STDERR, "Warning: no cost found for opcode: %d (%s) with size=%d\n", opcode, trivial_op_str_table[opcode].c_str(), size);
        } else {
            dr_fprintf(STDERR, "Warning: no cost found for opcode: %d (%s) with size=%d\n", opcode, "nil", size);
        }
#endif
        return 1;
    }

    static bool isTrivial(instr_t* instr, int trivial_oprand, bool int_only) {
        int opc = instr_get_opcode(instr);
        TrivialOpMap_t::iterator it = trivial_op_table.find(opc);
        if(it == trivial_op_table.end()) {
            return false;
        }
        // now this opcode has entry for trivial condition, further 
        // check for if it is NEVER attribute
        OpConditionMapList_t& op_cond_map_list = it->second;
        for(size_t i=0; i<op_cond_map_list.size(); ++i) {
            OpConditionMap_t& op_cond_map = op_cond_map_list[i];
            if(op_cond_map.size() != (size_t)instr_num_srcs(instr)) {
#ifdef DEBUG
                dr_fprintf(STDERR, "info: op_cond_map size=%d, num srcs=%d\n", op_cond_map.size(), instr_num_srcs(instr));
#endif
                continue;
            }
            if(trivial_oprand==TRIVIAL_OPRAND_ANY) {
                for(size_t i=0; i<op_cond_map.size(); ++i) {
                    int size= opnd_size_in_bytes(opnd_get_size(instr_get_src(instr, i)));
                    ConditionList_t& condlist = op_cond_map[i];
                    if( condlist.first.latency!=0 && 
                        condlist.first.size==size ) {
                        if (!int_only || (int_only && !condlist.first.is_float)) {
                            for(size_t j=0; j<condlist.second.size(); ++j) {
                                //if(condlist.second[j].attr!=Never) {
                                    return true;
                                //}
                            }
                        }
                    }
                    //dr_fprintf(STDERR, "\ti=%d, opnd size=%d, latency=%d, size=%d\n", i, size, condlist.first.latency, condlist.first.size);
                }
            } else {
                assert(trivial_oprand >= 0);
                assert((size_t)trivial_oprand < op_cond_map.size());
                ConditionList_t& condlist = op_cond_map[trivial_oprand];
                if (!int_only || (int_only && !condlist.first.is_float)) {
                    assert(trivial_oprand >= 0);
                    assert(trivial_oprand < instr_num_srcs(instr));
                    int size= opnd_size_in_bytes(opnd_get_size(instr_get_src(instr, trivial_oprand)));
                    if( condlist.first.latency!=0 && 
                        condlist.first.size==size ) {
                        for(size_t j=0; j<condlist.second.size(); ++j) {
                            if(condlist.second[j].attr!=Never) {
                                return true;
                            }
                        }
                    }
                    //dr_fprintf(STDERR, "\ti=%d, opnd size=%d, latency=%d, size=%d\n", trivial_oprand, size, condlist.first.latency, condlist.first.size);
                } else {
                    return false;
                }
            }
        }
#ifdef DEBUG
        char code[100];
        instr_disassemble_to_buffer(dr_get_current_drcontext(), instr, code, 100);
        dr_fprintf(STDERR, "Warning: no trivial condition found for opcode: %d (%s): opnd idx=%d, instr: %s\n", opc, trivial_op_str_table[opc].c_str(), trivial_oprand, code);
#endif
        return false;
    }
    static bool isApproxTrivial(instr_t* instr, int trivial_oprand) {
        int opc = instr_get_opcode(instr);
        TrivialOpMap_t::iterator it = trivial_op_table.find(opc);
        if(it == trivial_op_table.end()) {
            return false;
        }
        // now this opcode has entry for trivial condition, further 
        // check for if it is NEVER attribute
        OpConditionMapList_t& op_cond_map_list = it->second;
        for(size_t i=0; i<op_cond_map_list.size(); ++i) {
            OpConditionMap_t& op_cond_map = op_cond_map_list[i];
            if(op_cond_map.size() != (size_t)instr_num_srcs(instr)) {
                continue;
            }
            if(trivial_oprand==TRIVIAL_OPRAND_ANY) {
                for(size_t i=0; i<op_cond_map.size(); ++i) {
                    int size= opnd_size_in_bytes(opnd_get_size(instr_get_src(instr, i)));
                    ConditionList_t& condlist = op_cond_map[i];
                    if ( condlist.first.is_float && 
                         condlist.first.latency!=0 &&
                         condlist.first.size==size ) {
                        for(size_t j=0; j<condlist.second.size(); ++j) {
                            if(condlist.second[j].attr!=Never) {
                                return true;
                            }
                        }
                    }
                }
            } else {
                assert(trivial_oprand >= 0);
                assert((size_t)trivial_oprand < op_cond_map.size());
                ConditionList_t& condlist = op_cond_map[trivial_oprand];
                int size= opnd_size_in_bytes(opnd_get_size(instr_get_src(instr, trivial_oprand)));
                if ( condlist.first.is_float && 
                     condlist.first.latency!=0 &&
                     condlist.first.size==size ) {
                    for(size_t j=0; j<condlist.second.size(); ++j) {
                        if(condlist.second[j].attr!=Never) {
                            return true;
                        }
                    }
                }
            }
        }
#ifdef DEBUG
        dr_fprintf(STDERR, "Warning: no approx trivial condition found for opcode: %d (%s)\n", opc, trivial_op_str_table[opc].c_str());
#endif
        return false;
    }
    // return if success
    static bool getTrivialConditionList(instr_t* instr, int trivial_oprand, ConditionList_t** condlist) {
        DPRINTF("Enter getTrivialConditionList: instr=");
        IF_DEBUG_TRIVIAL(instr_disassemble(dr_get_current_drcontext(), instr, STDOUT));
        DPRINTF(", trivial_operand=%d\n", trivial_oprand);
        int opc = instr_get_opcode(instr);
        TrivialOpMap_t::iterator it = trivial_op_table.find(opc);
        if(it == trivial_op_table.end()) {
            DPRINTF("Not exist in trivial_op_table. Exit\n");
            return false;
        }
        assert(trivial_oprand>=0);
        assert(trivial_oprand<instr_num_srcs(instr));
        int size= opnd_size_in_bytes(opnd_get_size(instr_get_src(instr, trivial_oprand)));
        // now this opcode has entry for trivial condition
        OpConditionMapList_t& op_cond_map_list = it->second;
        for(size_t i=0; i<op_cond_map_list.size(); ++i) {
            OpConditionMap_t& op_cond_map = op_cond_map_list[i];
            if(op_cond_map.size() == (size_t)instr_num_srcs(instr)) {
                assert(trivial_oprand >= 0);
                assert((size_t)trivial_oprand < op_cond_map.size());
                if( op_cond_map[trivial_oprand].first.latency!=0 &&
                    op_cond_map[trivial_oprand].first.size==size ) {
                    *condlist = &op_cond_map[trivial_oprand];
                    DPRINTF("Exit getTrivialConditionList: Condlist Match Found!\n");
                    return true;
                }
                DPRINTF("latency or size not match: <latency, size>=<%d, %d>, "
                        "expected size=%d\n",
                        op_cond_map[trivial_oprand].first.latency,
                        op_cond_map[trivial_oprand].first.size, size);
            } else {
                DPRINTF("operand number not matched: %d, expected %d\n", op_cond_map.size(), instr_num_srcs(instr));
            }
        }
        DPRINTF("Exit getTrivialConditionList: No Match Found!\n");
        return false;
    }
    // return UNKNOWN when not found
    static ResultVal_t getTrivialResultForCondition(instr_t* instr, int trivial_operand, ConditionVal_t val, int8_t* other_idx) {
        // int func_entry;
        // if((func_entry=TrivialFuncTable::getTrivialFuncTableEntry(instr))!=-1) {
        //     // this operation is a pre-defined trivial function entry, so bypass to the trivial func table
        //     return TrivialFuncTable::getTrivialResultForCondition(func_entry, trivial_operand, val, other_idx);
        // }
        ConditionList_t* condlist;
        if(getTrivialConditionList(instr, trivial_operand, &condlist)) {
            for(size_t j=0; j<condlist->second.size(); ++j) {
                if(condlist->second[j].val==val) {
                    *other_idx = condlist->second[j].other_idx;
                    return condlist->second[j].res;
                }
            }
        }
        return UNKNOWN;
    }

    static int getAbsorbingResult(instr_t* instr) {
        // int func_entry;
        // if((func_entry=TrivialFuncTable::getTrivialFuncTableEntry(instr))!=-1) {
        //     // this operation is a pre-defined trivial function entry, so bypass to the trivial func table
        //     return TrivialFuncTable::getAbsorbingResult(func_entry);
        // }
        int res = 0;
        ConditionList_t* condlist;
        for(int i=0; i<instr_num_srcs(instr); ++i) {
            if(getTrivialConditionList(instr, i, &condlist)) {
                for(size_t j=0; j<condlist->second.size(); ++j) {
                    SET(res, condlist->second[j].res);
                    DPRINTF("SET result: %d (%d is set)\n", res, condlist->second[j].res);
                }
            }
        }
        return res;
    }

    static Condition_t* getConditionWithVal(instr_t* instr, int trivial_operand, ConditionVal_t val) {
        // int func_entry;
        // if((func_entry=TrivialFuncTable::getTrivialFuncTableEntry(instr))!=-1) {
        //     // this operation is a pre-defined trivial function entry, so bypass to the trivial func table
        //     return TrivialFuncTable::getConditionWithVal(func_entry, trivial_operand, val);
        // }
        ConditionList_t* condlist;
        if(getTrivialConditionList(instr, trivial_operand, &condlist)) {
            for(size_t j=0; j<condlist->second.size(); ++j) {
                if(condlist->second[j].val==val) {
                    return &condlist->second[j];
                }
            }
        }
        return NULL;
    }
};

/**********************************************************************************/
// May be better quantized by benchmarking the cycle costs of each math function
#define MATH_FUNC_COST_BOOST 5000
#define MEMORY_COST_BOOST 50

int32_t estimate_cost(instr_t* instr) {
    int cost; //, func_entry;
    // if((func_entry=TrivialFuncTable::getTrivialFuncTableEntry(instr))!=-1) {
    //     cost = MATH_FUNC_COST_BOOST;
    // } else {
    {
        int opcode = instr_get_opcode(instr);
        int i = 1, num_srcs = instr_num_srcs(instr);
        if(num_srcs>0) {
            int size = opnd_size_in_bytes(opnd_get_size(instr_get_src(instr, 0)));
            while(size==0 && i<num_srcs) {
                size = opnd_size_in_bytes(opnd_get_size(instr_get_src(instr, i)));
                ++i;
            }
            cost = TrivialOpTable::getCost(opcode, size);
        } else {
            cost = 1;
        }
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

#endif