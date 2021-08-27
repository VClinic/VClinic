#ifndef __TRIVIAL_TABLE_H__
#define __TRIVIAL_TABLE_H__

#include "dr_api.h"
#include "drmgr.h"
#include "drreg.h"
#include "drutil.h"
#include "drsyms.h"
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
const char* getResultVallString(ResultVal_t val) {
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
#ifdef DEBUG
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

    static bool isAbsorbing(instr_t* instr, int trivial_operand, ConditionVal_t val) {
        ConditionList_t* condlist;
        if(getTrivialConditionList(instr, trivial_operand, &condlist)) {
            for(size_t j=0; j<condlist->second.size(); ++j) {
                if(condlist->second[j].val==val) {
                    return condlist->second[j].attr == Absorbing;
                }
            }
        }
        return false;
    }

    static bool isIdentical(instr_t* instr, int trivial_operand, ConditionVal_t val) {
        ConditionList_t* condlist;
        if(getTrivialConditionList(instr, trivial_operand, &condlist)) {
            for(size_t j=0; j<condlist->second.size(); ++j) {
                if(condlist->second[j].val==val) {
                    return condlist->second[j].attr == Identical;
                }
            }
        }
        return false;
    }

    // return encoded check attr info
    static int8_t getEncodedTrivialCondAttrFromList(ConditionList_t* condlist, ConditionVal_t val) {
        int8_t check = 0;
        for(size_t j=0; j<condlist->second.size(); ++j) {
            if(condlist->second[j].val==val) {
                if(condlist->second[j].attr==Attribute_t::Absorbing) {
                    check = COND_CHECK_ATTR_ABSORBING;
                } else if(condlist->second[j].attr==Attribute_t::Identical) {
                    check = COND_CHECK_ATTR_IDENTICAL;
                } else {
                    // the condition with never attribute will not be checked
                }
                break;
            }
        }
        return check;
    }
};

#if 0
/********************************************************************
 * TrivialFuncTable is generated from pre-defined lists of functions
 * with trivial conditions and corresponding attributes and results.
 * Pre-defined function table format:
 *      begin <func name> : 
 *          <arg no.>; <type> <condition>; <attribute>; <type> <result>;
 *          ...
 *      end
 * Where:
 *      <func name> is the trivial function name,
 *      <arg no.> is the number of input argument of the function,
 *      <type> is the type of the argument/result, which is one of:
 *             double, float, int8, int16, int32, int64, etc.
 *      <condition> is the trivial condition, which is one of
 *             IS_ZERO, IS_FULL, IS_ONE
 *      <attribute> is the attribute of this trivial condition, 
 *          which is one of:
 *             FP_UNSAFE, ABSORBING, IDENTITY, MEMREF, etc.
 *          Note that attribute FUNCTION is automatically added;
 *      <result> is the final result of this function when this 
 *          trivial condition is satisfied, which is one of:
 *              ZERO, ONE, FULL, OTHER, etc.
 *          Note that if result is not set to OTHER, it will enable 
 *          function skipping when trivial condition is satisfied, 
 *          and the pre-defined results are returned, which will 
 *          result in better profiling performance.
 * Table content is the function callback before the wrapped function
 * which is generated from the information in pre-defined function 
 * table.
 * ******************************************************************/
typedef void(*pre_func_cb_t)(void *, void **);
// Note: the list is passed to the call back as *user_data by using drwrap_wrap_ex() if it contains more than one call back
typedef uint32_t func_id_t;

struct func_info_t {
    std::string func_name;
    int arg;
    pre_func_cb_t pre_cb;
    AttributeTable_t atb;
    bool isApprox;
};

typedef std::vector<func_info_t*> func_info_list_t;
typedef std::unordered_map<std::string, func_info_list_t*> trivial_func_table_t;

#define MAX_FUNC_ID 1024
func_id_t cur_func_id = 0;
func_info_t preallocated_func_info[MAX_FUNC_ID];

func_id_t get_func_id(std::string func_name, int arg, pre_func_cb_t callback, AttributeTable_t atb, bool isApprox) {
    assert(cur_func_id < MAX_FUNC_ID);
    preallocated_func_info[cur_func_id].func_name   = func_name;
    preallocated_func_info[cur_func_id].arg         = arg;
    preallocated_func_info[cur_func_id].pre_cb      = callback;
    preallocated_func_info[cur_func_id].atb         = atb;
    preallocated_func_info[cur_func_id].isApprox    = isApprox;
    return cur_func_id++;
}

func_info_t& get_func_info(func_id_t func_id) {
    return preallocated_func_info[func_id];
}

struct funcPreCBData {
    int arg;
    func_id_t func_id;
};

struct TrivialFuncTable{
    static void initFuncTable(const char* extFuncTable);
    static void finiFuncTable();
    // get trivial condition of function <target>, not used
    static int get(instr_t* instr, func_info_list_t &func_info_list);
    // checker
    static bool isTrivial(instr_t* instr, uint64_t filter);
    static bool isApproxTrivial(instr_t* instr, uint64_t filter);
    static int trivialCounts(instr_t* instr, uint64_t filter, uint& trivial, uint& approxTrivial);
};
#endif

#endif