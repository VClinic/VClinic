// @COPYRIGHT@
// Licensed under MIT license.
// See LICENSE.TXT file in the project root for more information.
// ==============================================================

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <malloc.h>
#include <iostream>
#include <unistd.h>
#include <assert.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sstream>
#include <functional>
#include <unordered_set>
#include <vector>
#include <unordered_map>
#include <algorithm>
#include <list>
#ifdef X86
#include <xmmintrin.h>
#include <immintrin.h>
#endif

#include "dr_api.h"
#include "drmgr.h"
#include "drreg.h"
#include "drutil.h"
#include "drcctlib.h"
// #include "shadow_memory.h"
#include "shadow_memory_lock.h"
#include "drcctlib_hpcviewer_format.h"
#include "dr_tools.h"
#include "vprofile.h"

using namespace std;

#define IS_ACCESS_WITHIN_PAGE_BOUNDARY(accessAddr, accessLen)  (PAGE_OFFSET((accessAddr)) <= (PAGE_OFFSET_MASK - (accessLen)))

/* Other footprint_client settings */
#define MAX_REDUNDANT_CONTEXTS_TO_LOG (1000)
#define MAX_FILE_PATH   (200)
#define MAX_DEPTH 10

#define ENCODE_ADDRESS_AND_ACCESS_LEN(addr, len) ( (addr) | (((uint64_t)(len)) << 48))
#define DECODE_ADDRESS(addrAndLen) ( (addrAndLen) & ((1L<<48) - 1))
#define DECODE_ACCESS_LEN(addrAndLen) ( (addrAndLen) >> 48)


#define MAX_WRITE_OP_LENGTH (512)
#define MAX_WRITE_OPS_IN_INS (8)
#define MAX_REG_LENGTH (64)

#ifdef X86
    #define MAX_SIMD_LENGTH (64)
#else
    #define MAX_SIMD_LENGTH (16) //Q0-Q31 128bits
#endif
#define MAX_SIMD_REGS (32)

#define MAX_ALIAS_REGS (16)  //EAX, EBX, ECX, EDX, EBP, EDI, ESI, ESP, R8-R15
#define MAX_ALIAS_REG_SIZE (8) //RAX is 64bits
#define MAX_ALIAS_TYPE (3) //(RAX, EAX, AX),(AH),(AL)

#ifdef AARCH64
    // 64 BIT: X0-X30 XSP XZR
    // 32 BIT: W0-W30 WSP WZR
    #define MAX_GENERAL_REGS (33)
#endif

#ifdef X86
//different register group
enum AliasReg {
    ALIAS_REG_A = 0, //RAX, EAX, AX, AH, or AL
    ALIAS_REG_B,
    ALIAS_REG_C,
    ALIAS_REG_D,
    ALIAS_REG_BP,
    ALIAS_REG_DI,
    ALIAS_REG_SI,
    ALIAS_REG_SP,
    ALIAS_REG_R8,
    ALIAS_REG_R9,
    ALIAS_REG_R10,
    ALIAS_REG_R11,
    ALIAS_REG_R12,
    ALIAS_REG_R13,
    ALIAS_REG_R14,
    ALIAS_REG_R15};

//alias type, generic, high byte or low byte

enum AliasGroup{
    ALIAS_GENERIC=0, // RAX, EAX, or AX
    ALIAS_HIGH_BYTE, //AH
    ALIAS_LOW_BYTE // AL
};

#if __BYTE_ORDER == __LITTLE_ENDIAN
    //alias begin bytes for different types
    #define ALIAS_BYTES_INDEX_64 (0)
    #define ALIAS_BYTES_INDEX_32 (0)
    #define ALIAS_BYTES_INDEX_16 (0)
    #define ALIAS_BYTES_INDEX_8_L (0)
    #define ALIAS_BYTES_INDEX_8_H (1)

#elif __BYTE_ORDER == __BIG_ENDIAN

    #define ALIAS_BYTES_INDEX_64 (0)
    #define ALIAS_BYTES_INDEX_32 (4)
    #define ALIAS_BYTES_INDEX_16 (6)
    #define ALIAS_BYTES_INDEX_8_L (7)
    #define ALIAS_BYTES_INDEX_8_H (6)

#else

#error "unknown endianness"

#endif
#endif


#define WINDOW_ENABLE 1000000
#define WINDOW_DISABLE 100000000

int window_enable;
int window_disable;

#define DECODE_DEAD(data) static_cast<context_handle_t>(((data)  & 0xffffffffffffffff) >> 32 )
#define DECODE_KILL(data) (static_cast<context_handle_t>( (data)  & 0x00000000ffffffff))


#define MAKE_CONTEXT_PAIR(a, b) (((uint64_t)(a) << 32) | ((uint64_t)(b)))

#define delta 0.01


struct AddrValPair{
    uint8_t value[MAX_WRITE_OP_LENGTH];
} __attribute__((aligned(16)));

struct LargeReg{
    uint8_t value[MAX_SIMD_LENGTH];
} __attribute__((aligned(32)));

// Client Options
#include "droption.h"
static droption_t<bool> op_enable_sampling
(DROPTION_SCOPE_CLIENT, "enable_sampling", 0, 0, 64, "Enable Bursty Sampling",
 "Enable bursty sampling for lower overhead with less profiling accuracy.");

static droption_t<int> op_window
(DROPTION_SCOPE_CLIENT, "window", WINDOW_DISABLE, 0, INT32_MAX, "Window size configuration of sampling",
 "Window size of sampling. Only available when sampling is enabled.");

static droption_t<int> op_window_enable
(DROPTION_SCOPE_CLIENT, "window_enable", WINDOW_ENABLE, 0, INT32_MAX, "Window enabled size configuration of sampling",
 "Window enabled size of sampling. Only available when sampling is enabled.");

 #define REDSPY_EXIT_PROCESS(format, args...)                                           \
    DRCCTLIB_CLIENT_EXIT_PROCESS_TEMPLATE("redspy", format, \
                                          ##args)

// We only interest in memory stores
bool 
VPROFILE_FILTER_OPND(opnd_t opnd, vprofile_src_t opmask) {
    uint32_t mask1 = (ANY_DATA_TYPE | GPR_REGISTER | SIMD_REGISTER | WRITE | AFTER);
    uint32_t mask2 = (ANY_DATA_TYPE | MEMORY | WRITE | BEFORE | AFTER);
    return ((mask1 & opmask) == opmask) || ((mask2 &opmask) == opmask);
}

bool REDSPY_FILTER_MEM_ACCESS_INSTR(instr_t *instr) {
    if(instr_is_call(instr)) return false;
    if(instr_is_prefetch(instr)) return false;
    if(instr_is_return(instr)) return false;
    if(instr_is_syscall(instr)) return false;
    if(instr_is_cti(instr)) return false;
    if(instr_is_ubr(instr) || instr_is_cbr(instr)) return false;
#ifdef X86
    // Certain instructions should not be approximated
    if(instr_is_xsave(instr)) {
        return false;
    }

    switch(instr_get_opcode(instr)) {
        case OP_fxsave32:
        case OP_fxsave64:
        case OP_fxrstor32:
        case OP_fxrstor64:
        case OP_fldenv:
        case OP_fnstenv:
        case OP_fnsave:
        case OP_fldcw:
        case OP_fnstcw:
            return false;
        default: return true;
    }
#endif
    return true;
}

/***********************************************
 ******  shadow memory
 ************************************************/
// To maintain the context for a write operation, we store an additional pointer sized variable CONTEXT(M) (actually uint32_t) in the shadow memory for each memory byte M.
ConcurrentShadowMemory<uint32_t> *ctxt_sm;

typedef struct _per_thread_t {
    uint64_t bytesWritten;

    uint8_t readBufferSlotIndex;
    uint8_t slotNum;
    AddrValPair buffer[MAX_WRITE_OPS_IN_INS];

    struct LargeReg simdValue[MAX_SIMD_REGS];
    uint32_t simdCtxt[MAX_SIMD_REGS];

#ifdef X86
    uint32_t regCtxt[DR_REG_LAST_ENUM];
    uint8_t regValue[DR_REG_LAST_ENUM][MAX_REG_LENGTH];

    uint8_t aliasValue[MAX_ALIAS_REGS][MAX_ALIAS_REG_SIZE];
    uint32_t aliasCtxt[MAX_ALIAS_REGS][MAX_ALIAS_TYPE];
#else
    uint32_t regCtxt[MAX_GENERAL_REGS];
    uint8_t regValue[MAX_GENERAL_REGS][MAX_REG_LENGTH];
#endif

    unordered_map<uint64_t, uint64_t>* RedMap;
    unordered_map<uint64_t, uint64_t>* ApproxRedMap;

    file_t output_file;
    int32_t threadId;
} per_thread_t;

file_t gTraceFile;
string g_folder_name;
// static void *gLock;

// for metric logging
int redspy_metric_id = 0;

//for statistics result
uint64_t grandTotBytesWritten;
uint64_t grandTotBytesRedWritten;
uint64_t grandTotBytesApproxRedWritten;

vtrace_t* vtrace;

enum {
    INSTRACE_TLS_OFFS_BUF_PTR,
    INSTRACE_TLS_OFFS_INTLOG_PTR,
    INSTRACE_TLS_OFFS_FPLOG_PTR,
    INSTRACE_TLS_COUNT, /* total number of TLS slots allocated */
};

static int tls_idx;
static reg_id_t tls_seg;
static uint tls_offs;

static inline void AddToRedTable(uint64_t key,  uint16_t value, per_thread_t* pt) __attribute__((always_inline,flatten));
static inline void AddToRedTable(uint64_t key,  uint16_t value, per_thread_t* pt) {
    unordered_map<uint64_t, uint64_t>::iterator it = pt->RedMap->find(key);
    if ( it  == pt->RedMap->end()) {
        (*pt->RedMap)[key] = value;
    } else {
        it->second += value;
    }
}

static inline void AddToApproximateRedTable(uint64_t key,  uint16_t value, per_thread_t* pt) __attribute__((always_inline,flatten));
static inline void AddToApproximateRedTable(uint64_t key,  uint16_t value, per_thread_t* pt) {
    unordered_map<uint64_t, uint64_t>::iterator it = pt->ApproxRedMap->find(key);
    if ( it  == pt->ApproxRedMap->end()) {
        (*pt->ApproxRedMap)[key] = value;
    } else {
        it->second += value;
    }
}


template<int start, int end, int incr, bool conditional, bool approx>
struct UnrolledLoop{
    static __attribute__((always_inline)) void Body(function<void (const int)> func){
        func(start); // Real loop body
        UnrolledLoop<start+incr, end, incr, conditional, approx>:: Body(func);   // unroll next iteration
    }
    static __attribute__((always_inline)) void BodySamePage(context_handle_t * __restrict__ prevIP, const context_handle_t handle, per_thread_t *pt){
        if(conditional) {
            // report in RedTable
            if(approx)
                AddToApproximateRedTable(MAKE_CONTEXT_PAIR(prevIP[start], handle), 1, pt);
            else
                AddToRedTable(MAKE_CONTEXT_PAIR(prevIP[start], handle), 1, pt);
        }
        // Update context
        prevIP[start] = handle;
        UnrolledLoop<start+incr, end, incr, conditional, approx>:: BodySamePage(prevIP, handle, pt);   // unroll next iteration
    }
    static __attribute__((always_inline)) void BodyStraddlePage(uint64_t addr, const context_handle_t handle, per_thread_t *pt){
        context_handle_t *prevIP = (context_handle_t *)ctxt_sm->GetOrCreateShadowAddress((size_t)addr+start);
        
        if (conditional) {
            // report in RedTable
            if(approx)
                AddToApproximateRedTable(MAKE_CONTEXT_PAIR(prevIP[0 /* 0 is correct*/ ], handle), 1, pt);
            else
                AddToRedTable(MAKE_CONTEXT_PAIR(prevIP[0 /* 0 is correct*/ ], handle), 1, pt);
        }
        // Update context
        prevIP[0] = handle;
        UnrolledLoop<start+incr, end, incr, conditional, approx>:: BodyStraddlePage(addr, handle, pt);   // unroll next iteration
    }
};

template<int end,  int incr, bool conditional, bool approx>
struct UnrolledLoop<end , end , incr, conditional, approx>{
    static __attribute__((always_inline)) void Body(function<void (const int)> func){}
    static __attribute__((always_inline)) void BodySamePage(context_handle_t * __restrict__ prevIP, const context_handle_t handle, per_thread_t *pt){}
    static __attribute__((always_inline)) void BodyStraddlePage(uint64_t addr, const context_handle_t handle, per_thread_t *pt){}
};

template<int start, int end, int incr>
struct UnrolledConjunction{
    static __attribute__((always_inline)) bool Body(function<bool (const int)> func){
        return func(start) && UnrolledConjunction<start+incr, end, incr>:: Body(func);   // unroll next iteration
    }
    static __attribute__((always_inline)) bool BodyContextCheck(context_handle_t * __restrict__ prevIP){
        return (prevIP[0] == prevIP[start]) && UnrolledConjunction<start+incr, end, incr>:: BodyContextCheck(prevIP);   // unroll next iteration
    }
};

template<int end,  int incr>
struct UnrolledConjunction<end , end , incr>{
    static __attribute__((always_inline)) bool Body(function<void (const int)> func){
        return true;
    }
    static __attribute__((always_inline)) bool BodyContextCheck(context_handle_t * __restrict__ prevIP){
        return true;
    }
};

template<int start, int end, int incr, class T>
struct UnrolledCopy {
    static __attribute__((always_inline)) void Body(function<void(const int)> func) {
        func(start);
        UnrolledCopy<start + incr, end, incr, T>::Body(func);   // unroll next iteration
    }

    static __attribute__((always_inline)) void BodyCopy(void *addr, uint8_t *prev) {
        *((T *) (prev + start)) = *(static_cast<T *>(addr) + start);
        UnrolledCopy<start + incr, end, incr, T>::BodyCopy(addr, prev);   // unroll next iteration
    }
};

template<int end, int incr, class T>
struct UnrolledCopy<end, end, incr, T> {
    static __attribute__((always_inline)) void Body(function<void(const int)> func) {}

    static __attribute__((always_inline)) void BodyCopy(void *addr, uint8_t *prev) {}
};

template<int start, int end, int incr, class T, bool isApprox>
struct UnrolledSubLoop {
    static __attribute__((always_inline)) bool Body(function<bool (const int)> func){
        return func(start) && UnrolledSubLoop<start + incr, end, incr, T, isApprox>::Body(func);   // unroll next iteration
    }

    static __attribute__((always_inline)) bool BodyISRed(void *addr, uint8_t *prev) {
        bool isRed = false;
        if (isApprox) {
            T oldValue = reinterpret_cast<const T *> (prev)[start];
            T newValue = reinterpret_cast<const T *> (addr)[start];
            T rate = (newValue - oldValue) / oldValue;
            isRed = (rate <= delta && rate >= -delta);
        } else {
            isRed = (reinterpret_cast<const T *> (prev)[start] == reinterpret_cast<const T *> (addr)[start]);
        }

        return isRed &&
               UnrolledSubLoop<start + incr, end, incr, T, isApprox>::BodyISRed(addr, prev);   // unroll next iteration
    }
};

template<int end, int incr, class T, bool isApprox>
struct UnrolledSubLoop<end, end, incr, T, isApprox> {
    static __attribute__((always_inline)) bool Body(function<bool (const int)> func){ return true;}
    static __attribute__((always_inline)) bool BodyISRed(void *addr, uint8_t *prev) { return true; }
};

/*********************************************************************************/
/*                              register analysis                                */
/*********************************************************************************/
#ifdef X86
/****************  handleing align registers ****************/
template<class T, AliasGroup aliasGroup>
struct HandleAliasRegisters{
    static __attribute__((always_inline)) void CheckUpdateGenericAlias(uint8_t regId, T value, context_handle_t curCtxtHandle, per_thread_t* pt) {
        //alias begin bytes for different types
        #if __BYTE_ORDER == __LITTLE_ENDIAN
            uint8_t byteOffset = aliasGroup == ALIAS_HIGH_BYTE ? 1 : 0;
        #else
            #error "unknown endianness"
        #endif

        
        T * where = (T *)(&pt->aliasValue[regId][byteOffset]);
        
        if (*where == value) {
            AddToRedTable(MAKE_CONTEXT_PAIR(pt->aliasCtxt[regId][aliasGroup], curCtxtHandle), sizeof(T), pt);
        }else {
            *where = value;
        }
        pt->aliasCtxt[regId][ALIAS_GENERIC] = curCtxtHandle;
        if(aliasGroup == ALIAS_GENERIC){
            pt->aliasCtxt[regId][ALIAS_HIGH_BYTE] = curCtxtHandle;
            pt->aliasCtxt[regId][ALIAS_LOW_BYTE] = curCtxtHandle;
        } else {
            pt->aliasCtxt[regId][aliasGroup] = curCtxtHandle;
        }
    }
};

/****************  handleing general registers ****************/
template<class T, uint8_t len>
struct HandleGeneralRegisters{
    static __attribute__((always_inline)) void CheckValues(T value, reg_id_t reg, context_handle_t curCtxtHandle, per_thread_t* pt) {
        T * regBefore = (T *)(&pt->regValue[reg][0]);
        
        if (* regBefore == value ) {
            AddToRedTable(MAKE_CONTEXT_PAIR(pt->regCtxt[reg],curCtxtHandle),sizeof(T),pt);
        }else
            * regBefore = value;
        pt->regCtxt[reg] = curCtxtHandle;
    }
};

//lenInt64: 1(X87), 2(XMM), 4(YMM), 8(ZMM)
template<uint8_t lenInt64>
struct HandleSpecialRegisters{

    //check the MM_x part registers in X87
    static __attribute__((always_inline)) void CheckRegValues(uint8_t *val, reg_id_t regID, context_handle_t curCtxtHandle, per_thread_t *pt){
        if(lenInt64 == 1){
            uint64_t *oldValue = (uint64_t*)&(pt->regValue[regID][0]);
            if(*oldValue == *(uint64_t*)val)
                AddToRedTable(MAKE_CONTEXT_PAIR(pt->regCtxt[regID],curCtxtHandle),8,pt);
            else
                *oldValue = *(uint64_t*)val;
        
            pt->regCtxt[regID] = curCtxtHandle;
        }else if(lenInt64 == 2){
            
            uint64_t *oldValue1 = (uint64_t*)&(pt->simdValue[regID].value);
            uint64_t *oldValue2 = (uint64_t*)&(pt->simdValue[regID].value[8]);
            if(*oldValue1 == ((uint64_t*)val)[0] && *oldValue2 == ((uint64_t*)val)[1])
            AddToRedTable(MAKE_CONTEXT_PAIR(pt->simdCtxt[regID],curCtxtHandle),16,pt);
            else{
                *oldValue1 = ((uint64_t*)val)[0];
                *oldValue2 = ((uint64_t*)val)[1];
            }
            pt->simdCtxt[regID] = curCtxtHandle;
            
        }else{
            
            uint64_t *oldValue;
            bool isRedundant = true;
            for(int i = 0,j = 0; i < lenInt64; ++i, j += 8){
                oldValue = (uint64_t*)&(pt->simdValue[regID].value[j]);
                if(*oldValue != ((uint64_t*)val)[i]){
                    isRedundant = false;
                    *oldValue = ((uint64_t*)val)[i];
                }
            }
            
            if(isRedundant)
                AddToRedTable(MAKE_CONTEXT_PAIR(pt->simdCtxt[regID],curCtxtHandle),lenInt64*8,pt);
            
            pt->simdCtxt[regID] = curCtxtHandle;
        }
    }
};

/****************  handleing registers approximation  ****************/
//static void Check10BytesReg(PIN_REGISTER* regRef, REG reg, uint32_t opaqueHandle, THREADID threadId)__attribute__((always_inline));
static void Check10BytesReg(uint8_t *val, reg_id_t reg, context_handle_t curCtxtHandle, per_thread_t *pt){
    uint64_t * upperOld = (uint64_t*)&(pt->regValue[reg][2]);
    uint64_t * upperNew = (uint64_t*)&(val[2]);
    
    uint16_t * lowOld = (uint16_t*)&(pt->regValue[reg][0]);
    uint16_t * lowNew = (uint16_t*)(val);
    
    if((*lowOld & 0xfff0) == (*lowNew & 0xfff0) && *upperNew == *upperOld){
        AddToApproximateRedTable(MAKE_CONTEXT_PAIR(pt->regCtxt[reg],curCtxtHandle),10,pt);
        *lowOld = *lowNew;
    }else
        memcpy(&pt->regValue[reg][0], val, 10);
    pt->regCtxt[reg] = curCtxtHandle;
}

//approximate general registers
template<class T, bool isAlias>
struct ApproxGeneralRegisters{
    
    static __attribute__((always_inline)) void CheckValues(uint8_t *val, reg_id_t reg, context_handle_t curCtxtHandle, per_thread_t *pt){
        if(isAlias){
            uint8_t byteOffset = 0;
            
            T newValue = *(T*)val;
            
            T oldValue = *((T*)(&pt->aliasValue[reg][byteOffset]));
            T rate = (newValue - oldValue)/oldValue;
            if( rate <= delta && rate >= -delta ){
                AddToApproximateRedTable(MAKE_CONTEXT_PAIR(pt->aliasCtxt[reg][ALIAS_GENERIC],curCtxtHandle),sizeof(T),pt);
            }
            if( newValue != oldValue)
                *((T*)(&pt->aliasValue[reg][byteOffset])) = newValue;
            
            pt->aliasCtxt[reg][ALIAS_GENERIC] = curCtxtHandle;
            pt->aliasCtxt[reg][ALIAS_HIGH_BYTE] = curCtxtHandle;
            pt->aliasCtxt[reg][ALIAS_LOW_BYTE] = curCtxtHandle;
            
        }else{
            T newValue = *(T*)val;
            
            T oldValue = *((T*)(&pt->regValue[reg][0]));
            T rate = (newValue - oldValue)/oldValue;
            if(rate <= delta && rate >= -delta) {
                AddToApproximateRedTable(MAKE_CONTEXT_PAIR(pt->regCtxt[reg],curCtxtHandle),sizeof(T),pt);
            }
            if(newValue != oldValue)
                *((T*)(&pt->regValue[reg][0])) = newValue;
            pt->regCtxt[reg] = curCtxtHandle;
        }
    }
};

//approximate SIMD registers, simdType:0(XMM), 1(YMM), 2(ZMM)
template<class T, uint32_t AccessLen>
struct ApproxLargeRegisters{
    
    static __attribute__((always_inline)) void CheckValues(uint8_t *val, reg_id_t regID, context_handle_t curCtxtHandle, per_thread_t *pt){
        if(UnrolledSubLoop<0, AccessLen/sizeof(T), 1, T, true>::BodyISRed((void*)val, &(pt->simdValue[regID].value[0]))) {
            AddToApproximateRedTable(MAKE_CONTEXT_PAIR(pt->simdCtxt[regID],curCtxtHandle),AccessLen,pt);
        } else {
            UnrolledCopy<0, AccessLen/sizeof(T), 1, T>::BodyCopy(val, &(pt->simdValue[regID].value[0]));
        }

        pt->simdCtxt[regID] = curCtxtHandle;
    }
};

static inline uint32_t GetAliasIDs(reg_id_t reg){
    uint8_t regGroup = 0;
    uint8_t byteInd = 0;
    uint8_t type = 0;
    switch (reg) {
        case DR_REG_RAX: regGroup = ALIAS_REG_A; byteInd = ALIAS_BYTES_INDEX_64; type = ALIAS_GENERIC; break;
        case DR_REG_EAX: regGroup = ALIAS_REG_A; byteInd = ALIAS_BYTES_INDEX_32; type = ALIAS_GENERIC; break;
        case DR_REG_AX: regGroup = ALIAS_REG_A; byteInd = ALIAS_BYTES_INDEX_16; type = ALIAS_GENERIC; break;
        case DR_REG_AH: regGroup = ALIAS_REG_A; byteInd = ALIAS_BYTES_INDEX_8_H; type = ALIAS_HIGH_BYTE; break;
        case DR_REG_AL: regGroup = ALIAS_REG_A; byteInd = ALIAS_BYTES_INDEX_8_L; type = ALIAS_LOW_BYTE; break;
            
        case DR_REG_RBX: regGroup = ALIAS_REG_B; byteInd = ALIAS_BYTES_INDEX_64; type = ALIAS_GENERIC; break;
        case DR_REG_EBX: regGroup = ALIAS_REG_B; byteInd = ALIAS_BYTES_INDEX_32; type = ALIAS_GENERIC; break;
        case DR_REG_BX: regGroup = ALIAS_REG_B; byteInd = ALIAS_BYTES_INDEX_16; type = ALIAS_GENERIC; break;
        case DR_REG_BH: regGroup = ALIAS_REG_B; byteInd = ALIAS_BYTES_INDEX_8_H; type = ALIAS_HIGH_BYTE; break;
        case DR_REG_BL: regGroup = ALIAS_REG_B; byteInd = ALIAS_BYTES_INDEX_8_L; type = ALIAS_LOW_BYTE; break;
            
        case DR_REG_RCX: regGroup = ALIAS_REG_C; byteInd = ALIAS_BYTES_INDEX_64; type = ALIAS_GENERIC; break;
        case DR_REG_ECX: regGroup = ALIAS_REG_C; byteInd = ALIAS_BYTES_INDEX_32; type = ALIAS_GENERIC; break;
        case DR_REG_CX: regGroup = ALIAS_REG_C; byteInd = ALIAS_BYTES_INDEX_16; type = ALIAS_GENERIC; break;
        case DR_REG_CH: regGroup = ALIAS_REG_C; byteInd = ALIAS_BYTES_INDEX_8_H; type = ALIAS_HIGH_BYTE; break;
        case DR_REG_CL: regGroup = ALIAS_REG_C; byteInd = ALIAS_BYTES_INDEX_8_L; type = ALIAS_LOW_BYTE; break;
            
        case DR_REG_RDX: regGroup = ALIAS_REG_D; byteInd = ALIAS_BYTES_INDEX_64; type = ALIAS_GENERIC; break;
        case DR_REG_EDX: regGroup = ALIAS_REG_D; byteInd = ALIAS_BYTES_INDEX_32; type = ALIAS_GENERIC; break;
        case DR_REG_DX: regGroup = ALIAS_REG_D; byteInd = ALIAS_BYTES_INDEX_16; type = ALIAS_GENERIC; break;
        case DR_REG_DH: regGroup = ALIAS_REG_D; byteInd = ALIAS_BYTES_INDEX_8_H; type = ALIAS_HIGH_BYTE; break;
        case DR_REG_DL: regGroup = ALIAS_REG_D; byteInd = ALIAS_BYTES_INDEX_8_L; type = ALIAS_LOW_BYTE; break;
            
        case DR_REG_RBP: regGroup = ALIAS_REG_BP; byteInd = ALIAS_BYTES_INDEX_64; type = ALIAS_GENERIC; break;
        case DR_REG_EBP: regGroup = ALIAS_REG_BP; byteInd = ALIAS_BYTES_INDEX_32; type = ALIAS_GENERIC; break;
        case DR_REG_BP: regGroup = ALIAS_REG_BP; byteInd = ALIAS_BYTES_INDEX_16; type = ALIAS_GENERIC; break;
        case DR_REG_BPL: regGroup = ALIAS_REG_BP; byteInd = ALIAS_BYTES_INDEX_8_L; type = ALIAS_GENERIC; break;
            
        case DR_REG_RDI: regGroup = ALIAS_REG_DI; byteInd = ALIAS_BYTES_INDEX_64; type = ALIAS_GENERIC; break;
        case DR_REG_EDI: regGroup = ALIAS_REG_DI; byteInd = ALIAS_BYTES_INDEX_32; type = ALIAS_GENERIC; break;
        case DR_REG_DI: regGroup = ALIAS_REG_DI; byteInd = ALIAS_BYTES_INDEX_16; type = ALIAS_GENERIC; break;
        case DR_REG_DIL: regGroup = ALIAS_REG_DI; byteInd = ALIAS_BYTES_INDEX_8_L; type = ALIAS_GENERIC; break;
            
        case DR_REG_RSI: regGroup = ALIAS_REG_SI; byteInd = ALIAS_BYTES_INDEX_64; type = ALIAS_GENERIC; break;
        case DR_REG_ESI: regGroup = ALIAS_REG_SI; byteInd = ALIAS_BYTES_INDEX_32; type = ALIAS_GENERIC; break;
        case DR_REG_SI: regGroup = ALIAS_REG_SI; byteInd = ALIAS_BYTES_INDEX_16; type = ALIAS_GENERIC; break;
        case DR_REG_SIL: regGroup = ALIAS_REG_SI; byteInd = ALIAS_BYTES_INDEX_8_L; type = ALIAS_GENERIC; break;
            
        case DR_REG_RSP: regGroup = ALIAS_REG_SP; byteInd = ALIAS_BYTES_INDEX_64; type = ALIAS_GENERIC; break;
        case DR_REG_ESP: regGroup = ALIAS_REG_SP; byteInd = ALIAS_BYTES_INDEX_32; type = ALIAS_GENERIC; break;
        case DR_REG_SP: regGroup = ALIAS_REG_SP; byteInd = ALIAS_BYTES_INDEX_16; type = ALIAS_GENERIC; break;
        case DR_REG_SPL: regGroup = ALIAS_REG_SP; byteInd = ALIAS_BYTES_INDEX_8_L; type = ALIAS_GENERIC; break;
            
        case DR_REG_R8: regGroup = ALIAS_REG_R8; byteInd = ALIAS_BYTES_INDEX_64; type = ALIAS_GENERIC; break;
        case DR_REG_R8D: regGroup = ALIAS_REG_R8; byteInd = ALIAS_BYTES_INDEX_32; type = ALIAS_GENERIC; break;
        case DR_REG_R8W: regGroup = ALIAS_REG_R8; byteInd = ALIAS_BYTES_INDEX_16; type = ALIAS_GENERIC; break;
        case DR_REG_R8L: regGroup = ALIAS_REG_R8; byteInd = ALIAS_BYTES_INDEX_8_L; type = ALIAS_GENERIC; break;
            
        case DR_REG_R9: regGroup = ALIAS_REG_R9; byteInd = ALIAS_BYTES_INDEX_64; type = ALIAS_GENERIC; break;
        case DR_REG_R9D: regGroup = ALIAS_REG_R9; byteInd = ALIAS_BYTES_INDEX_32; type = ALIAS_GENERIC; break;
        case DR_REG_R9W: regGroup = ALIAS_REG_R9; byteInd = ALIAS_BYTES_INDEX_16; type = ALIAS_GENERIC; break;
        case DR_REG_R9L: regGroup = ALIAS_REG_R9; byteInd = ALIAS_BYTES_INDEX_8_L; type = ALIAS_GENERIC; break;

        case DR_REG_R10: regGroup = ALIAS_REG_R10; byteInd = ALIAS_BYTES_INDEX_64; type = ALIAS_GENERIC; break;
        case DR_REG_R10D: regGroup = ALIAS_REG_R10; byteInd = ALIAS_BYTES_INDEX_32; type = ALIAS_GENERIC; break;
        case DR_REG_R10W: regGroup = ALIAS_REG_R10; byteInd = ALIAS_BYTES_INDEX_16; type = ALIAS_GENERIC; break;
        case DR_REG_R10L: regGroup = ALIAS_REG_R10; byteInd = ALIAS_BYTES_INDEX_8_L; type = ALIAS_GENERIC; break;

        case DR_REG_R11: regGroup = ALIAS_REG_R11; byteInd = ALIAS_BYTES_INDEX_64; type = ALIAS_GENERIC; break;
        case DR_REG_R11D: regGroup = ALIAS_REG_R11; byteInd = ALIAS_BYTES_INDEX_32; type = ALIAS_GENERIC; break;
        case DR_REG_R11W: regGroup = ALIAS_REG_R11; byteInd = ALIAS_BYTES_INDEX_16; type = ALIAS_GENERIC; break;
        case DR_REG_R11L: regGroup = ALIAS_REG_R11; byteInd = ALIAS_BYTES_INDEX_8_L; type = ALIAS_GENERIC; break;

        case DR_REG_R12: regGroup = ALIAS_REG_R12; byteInd = ALIAS_BYTES_INDEX_64; type = ALIAS_GENERIC; break;
        case DR_REG_R12D: regGroup = ALIAS_REG_R12; byteInd = ALIAS_BYTES_INDEX_32; type = ALIAS_GENERIC; break;
        case DR_REG_R12W: regGroup = ALIAS_REG_R12; byteInd = ALIAS_BYTES_INDEX_16; type = ALIAS_GENERIC; break;
        case DR_REG_R12L: regGroup = ALIAS_REG_R12; byteInd = ALIAS_BYTES_INDEX_8_L; type = ALIAS_GENERIC; break;

        case DR_REG_R13: regGroup = ALIAS_REG_R13; byteInd = ALIAS_BYTES_INDEX_64; type = ALIAS_GENERIC; break;
        case DR_REG_R13D: regGroup = ALIAS_REG_R13; byteInd = ALIAS_BYTES_INDEX_32; type = ALIAS_GENERIC; break;
        case DR_REG_R13W: regGroup = ALIAS_REG_R13; byteInd = ALIAS_BYTES_INDEX_16; type = ALIAS_GENERIC; break;
        case DR_REG_R13L: regGroup = ALIAS_REG_R13; byteInd = ALIAS_BYTES_INDEX_8_L; type = ALIAS_GENERIC; break;

        case DR_REG_R14: regGroup = ALIAS_REG_R14; byteInd = ALIAS_BYTES_INDEX_64; type = ALIAS_GENERIC; break;
        case DR_REG_R14D: regGroup = ALIAS_REG_R14; byteInd = ALIAS_BYTES_INDEX_32; type = ALIAS_GENERIC; break;
        case DR_REG_R14W: regGroup = ALIAS_REG_R14; byteInd = ALIAS_BYTES_INDEX_16; type = ALIAS_GENERIC; break;
        case DR_REG_R14L: regGroup = ALIAS_REG_R14; byteInd = ALIAS_BYTES_INDEX_8_L; type = ALIAS_GENERIC; break;

        case DR_REG_R15: regGroup = ALIAS_REG_R15; byteInd = ALIAS_BYTES_INDEX_64; type = ALIAS_GENERIC; break;
        case DR_REG_R15D: regGroup = ALIAS_REG_R15; byteInd = ALIAS_BYTES_INDEX_32; type = ALIAS_GENERIC; break;
        case DR_REG_R15W: regGroup = ALIAS_REG_R15; byteInd = ALIAS_BYTES_INDEX_16; type = ALIAS_GENERIC; break;
        case DR_REG_R15L: regGroup = ALIAS_REG_R15; byteInd = ALIAS_BYTES_INDEX_8_L; type = ALIAS_GENERIC; break;

        default: assert(0 && "not alias registers! should not reach here!"); break;
    }
    uint32_t aliasGroupByteType = ((uint32_t)regGroup << 16) | ((uint32_t)byteInd << 8) | ((uint32_t)type);
    return aliasGroupByteType;
}

inline bool REG_is_Lower8(reg_id_t reg){
    switch(reg){
        case DR_REG_AL:
        case DR_REG_BL:
        case DR_REG_CL:
        case DR_REG_DL:
        case DR_REG_BPL:
        case DR_REG_DIL:
        case DR_REG_SIL:
        case DR_REG_SPL:
        case DR_REG_R8L:
        case DR_REG_R9L:
        case DR_REG_R10L:
        case DR_REG_R11L:
        case DR_REG_R12L:
        case DR_REG_R13L:
        case DR_REG_R14L:
        case DR_REG_R15L:
            return true;
        default: return false;
    }
}

inline bool RegHasAlias(reg_id_t reg){
    switch(reg){
        case DR_REG_RAX:
        case DR_REG_RBX:
        case DR_REG_RCX:
        case DR_REG_RDX:
        case DR_REG_EAX:
        case DR_REG_EBX:
        case DR_REG_ECX:
        case DR_REG_EDX:
        case DR_REG_AX:
        case DR_REG_BX:
        case DR_REG_CX:
        case DR_REG_DX:
        case DR_REG_AH:
        case DR_REG_BH:
        case DR_REG_CH:
        case DR_REG_DH:
        case DR_REG_AL:
        case DR_REG_BL:
        case DR_REG_CL:
        case DR_REG_DL:
        case DR_REG_RBP:
        case DR_REG_EBP:
        case DR_REG_BP:
        case DR_REG_BPL:
        case DR_REG_RDI:
        case DR_REG_EDI:
        case DR_REG_DI:
        case DR_REG_DIL:
        case DR_REG_RSI:
        case DR_REG_ESI:
        case DR_REG_SI:
        case DR_REG_SIL:
        case DR_REG_RSP:
        case DR_REG_ESP:
        case DR_REG_SP:
        case DR_REG_SPL:
        case DR_REG_R8:
        case DR_REG_R8D:
        case DR_REG_R8W:
        case DR_REG_R8L:
        case DR_REG_R9:
        case DR_REG_R9D:
        case DR_REG_R9W:
        case DR_REG_R9L:
        case DR_REG_R10:
        case DR_REG_R10D:
        case DR_REG_R10W:
        case DR_REG_R10L:
        case DR_REG_R11:
        case DR_REG_R11D:
        case DR_REG_R11W:
        case DR_REG_R11L:
        case DR_REG_R12:
        case DR_REG_R12D:
        case DR_REG_R12W:
        case DR_REG_R12L:
        case DR_REG_R13:
        case DR_REG_R13D:
        case DR_REG_R13W:
        case DR_REG_R13L:
        case DR_REG_R14:
        case DR_REG_R14D:
        case DR_REG_R14W:
        case DR_REG_R14L:
        case DR_REG_R15:
        case DR_REG_R15D:
        case DR_REG_R15W:
        case DR_REG_R15L:
            return true;
        default: return false;
    }
}
#else
/****************  handleing general registers ****************/
template<class T, uint8_t len>
struct HandleGeneralRegisters{
    static __attribute__((always_inline)) void CheckValues(T value, reg_id_t reg, context_handle_t curCtxtHandle, per_thread_t* pt) {
        T * regBefore = (T *)(&pt->regValue[reg][0]);
        
        if (* regBefore == value ) {
            AddToRedTable(MAKE_CONTEXT_PAIR(pt->regCtxt[reg],curCtxtHandle),sizeof(T),pt);
        }else
            * regBefore = value;
        pt->regCtxt[reg] = curCtxtHandle;
    }
};

// handleLargeRegisters

/****************  handleing registers approximation  ****************/
//approximate general registers
template<class T>
struct ApproxGeneralRegisters{
    
    static __attribute__((always_inline)) void CheckValues(uint8_t *val, reg_id_t reg, context_handle_t curCtxtHandle, per_thread_t *pt){
        T newValue = *(T*)val;
        
        T oldValue = *((T*)(&pt->regValue[reg][0]));
        T rate = (newValue - oldValue)/oldValue;
        if(rate <= delta && rate >= -delta) {
            AddToApproximateRedTable(MAKE_CONTEXT_PAIR(pt->regCtxt[reg],curCtxtHandle),sizeof(T),pt);
        }
        if(newValue != oldValue)
            *((T*)(&pt->regValue[reg][0])) = newValue;
        pt->regCtxt[reg] = curCtxtHandle;
    }
};

//approximate SIMD registers, simdType:0(XMM), 1(YMM), 2(ZMM)
template<class T, uint32_t AccessLen>
struct ApproxLargeRegisters{
    
    static __attribute__((always_inline)) void CheckValues(uint8_t *val, reg_id_t regID, context_handle_t curCtxtHandle, per_thread_t *pt){
        if(UnrolledSubLoop<0, AccessLen/sizeof(T), 1, T, true>::BodyISRed((void*)val, &(pt->simdValue[regID].value[0]))) {
            AddToApproximateRedTable(MAKE_CONTEXT_PAIR(pt->simdCtxt[regID],curCtxtHandle),AccessLen,pt);
        } else {
            UnrolledCopy<0, AccessLen/sizeof(T), 1, T>::BodyCopy(val, &(pt->simdValue[regID].value[0]));
        }

        pt->simdCtxt[regID] = curCtxtHandle;
    }
};
#endif

/***************************************************************************************/
/*********************** memory temporal redundancy functions **************************/
/***************************************************************************************/
template<class T, uint32_t AccessLen, uint32_t bufferOffset, bool isApprox>
struct RedSpyAnalysis{
    static __attribute__((always_inline)) bool IsWriteRedundant(uint8_t* val, per_thread_t *pt){
        AddrValPair * avPair = & pt->buffer[bufferOffset];
        
        return UnrolledSubLoop<0, AccessLen/sizeof(T), 1, T, isApprox>::BodyISRed(val, avPair->value);
    }
    
    static __attribute__((always_inline)) void RecordNByteValueBeforeWrite(uint8_t* val, per_thread_t *pt){
        AddrValPair * avPair = & pt->buffer[bufferOffset];
        if(AccessLen >= 16) {
            UnrolledCopy<0, AccessLen/sizeof(T), 1, T>::BodyCopy(val, avPair->value);
        } else if(AccessLen == 10){
            memcpy(avPair->value, val, AccessLen);
        } else
            *((T*)(avPair->value)) = *(static_cast<T*>((void*)val));
    }
    
    static __attribute__((always_inline)) void CheckNByteValueAfterWrite(void *addr, uint8_t *val, uint32_t curCtxtHandle, per_thread_t *pt){
        bool isRedundantWrite = IsWriteRedundant(val, pt);
        
        context_handle_t * __restrict__ prevIP = (context_handle_t *)ctxt_sm->GetOrCreateShadowAddress((size_t)addr);
        // if(isRedundantWrite) {
        //     dr_fprintf(STDOUT, "prevIP = %d, curIP = %d\n", *prevIP, curCtxtHandle);
        // }
        const bool isAccessWithinPageBoundary = IS_ACCESS_WITHIN_PAGE_BOUNDARY( (uint64_t)addr, AccessLen);
        if(isRedundantWrite) {
            // detected redundancy
            if(isAccessWithinPageBoundary) {
                // All from same ctxt?
                if (UnrolledConjunction<0, AccessLen, 1>::BodyContextCheck(prevIP)) {
                    // report in RedTable
                    if(isApprox)
                        AddToApproximateRedTable(MAKE_CONTEXT_PAIR(prevIP[0], curCtxtHandle), AccessLen, pt);
                    else
                        AddToRedTable(MAKE_CONTEXT_PAIR(prevIP[0], curCtxtHandle), AccessLen, pt);
                    // Update context
                    UnrolledLoop<0, AccessLen, 1, false, /* redundancy is updated outside*/ isApprox>::BodySamePage(prevIP, curCtxtHandle, pt);
                } else {
                    // different contexts
                    UnrolledLoop<0, AccessLen, 1, true, /* redundancy is updated inside*/ isApprox>::BodySamePage(prevIP, curCtxtHandle, pt);
                }
            } else {
                // Write across a 64-K page boundary
                // First byte is on this page though
                if(isApprox)
                    AddToApproximateRedTable(MAKE_CONTEXT_PAIR(prevIP[0], curCtxtHandle), 1, pt);
                else
                    AddToRedTable(MAKE_CONTEXT_PAIR(prevIP[0], curCtxtHandle), 1, pt);
                // Update context
                prevIP[0] = curCtxtHandle;
                
                // Remaining bytes [1..AccessLen] somewhere will across a 64-K page boundary
                UnrolledLoop<1, AccessLen, 1, true, /* update redundancy */ isApprox>::BodyStraddlePage( (uint64_t) addr, curCtxtHandle, pt);
            }
        } else {
            // No redundancy.
            // Just update contexts
            if(isAccessWithinPageBoundary) {
                // Update context
                UnrolledLoop<0, AccessLen, 1, false, /* not redundant*/ isApprox>::BodySamePage(prevIP, curCtxtHandle, pt);
            } else {
                // Write across a 64-K page boundary
                // Update context
                prevIP[0] = curCtxtHandle;
                
                // Remaining bytes [1..AccessLen] somewhere will across a 64-K page boundary
                UnrolledLoop<1, AccessLen, 1, false, /* not redundant*/ isApprox>::BodyStraddlePage( (uint64_t) addr, curCtxtHandle, pt);
            }
        }
    }
    static __attribute__((always_inline)) void ApproxCheckAfterWrite(void *addr, uint8_t *val, uint32_t curCtxtHandle, per_thread_t *pt){
        bool isRedundantWrite = IsWriteRedundant(val, pt);
        
        uint32_t const interv = sizeof(T);
        context_handle_t * __restrict__ prevIP = (context_handle_t *)ctxt_sm->GetOrCreateShadowAddress((size_t)addr);
        
        if(isRedundantWrite){
            for(uint32_t index = 0 ; index < AccessLen; index+=interv){
                prevIP = (context_handle_t*)(context_handle_t *)ctxt_sm->GetOrCreateShadowAddress((size_t)addr + index);
                // report in RedTable
                AddToApproximateRedTable(MAKE_CONTEXT_PAIR(prevIP[0 /* 0 is correct*/ ], curCtxtHandle), interv, pt);
                // Update context
                prevIP[0] = curCtxtHandle;
            }
        }else{
            for(uint32_t index = 0 ; index < AccessLen; index+=interv){
                prevIP = (context_handle_t*)(context_handle_t *)ctxt_sm->GetOrCreateShadowAddress((size_t)addr + index);
                // Update context
                prevIP[0] = curCtxtHandle;
            }
        }
    }
};


static inline void RecordValueBeforeLargeWrite(uint8_t *val, uint32_t accessLen,  uint32_t bufferOffset, per_thread_t *pt){
    // 这里的addr实际上传的是val info t的val
    memcpy(& (pt->buffer[bufferOffset].value), (void*)val, accessLen);
}

static inline void CheckAfterLargeWrite(void *addr, uint8_t *val, uint32_t accessLen,  uint32_t bufferOffset, uint32_t curCtxtHandle, per_thread_t *pt){
    context_handle_t * __restrict__ prevIP = (context_handle_t *)ctxt_sm->GetOrCreateShadowAddress((size_t)addr);
    if(memcmp( & (pt->buffer[bufferOffset].value), val, accessLen) == 0){
        // redundant
        for(uint32_t index = 0 ; index < accessLen; index++){
            prevIP = (context_handle_t*)(context_handle_t *)ctxt_sm->GetOrCreateShadowAddress((size_t)addr + index);
            // report in RedTable
            AddToRedTable(MAKE_CONTEXT_PAIR(prevIP[0 /* 0 is correct*/ ], curCtxtHandle), 1, pt);
            // Update context
            prevIP[0] = curCtxtHandle;
        }
    }else{
        // Not redundant
        for(uint32_t index = 0 ; index < accessLen; index++){
            prevIP = (context_handle_t*)(context_handle_t *)ctxt_sm->GetOrCreateShadowAddress((size_t)addr + index);
            // Update context
            prevIP[0] = curCtxtHandle;
        }
    }
}

template<uint32_t readBufferSlotIndex, bool is_before>
void InstrumentReadValueBeforeAndAfterWriting(int size, int esize, bool is_float, void *addr, uint8_t* val, uint32_t curCtxtHandle, per_thread_t *pt) {
    if(is_before) {
        if(is_float) {
            switch(size) {
                case 1:
                case 2: DR_ASSERT_MSG(false, "trace_update_cb: memory write floating data with unexptected small size.\n");
                case 4: RedSpyAnalysis<float, 4, readBufferSlotIndex,true>::RecordNByteValueBeforeWrite(val, pt); break;
                case 8: RedSpyAnalysis<double, 8, readBufferSlotIndex,true>::RecordNByteValueBeforeWrite(val, pt); break;
                case 10: RedSpyAnalysis<uint8_t, 10, readBufferSlotIndex,true>::RecordNByteValueBeforeWrite(val, pt); break;
                case 16: {
                    switch(esize) {
                        case 4: RedSpyAnalysis<float, 16, readBufferSlotIndex,true>::RecordNByteValueBeforeWrite(val, pt); break;
                        case 8: RedSpyAnalysis<double, 16, readBufferSlotIndex,true>::RecordNByteValueBeforeWrite(val, pt); break;
                        default: DR_ASSERT_MSG(false, "InstrumentReadValueBeforeAndAfterWriting: handle large mem write with large operand size\n"); break;
                    }
                }break;
                case 32: {
                    switch(esize) {
                        case 4: RedSpyAnalysis<float, 32, readBufferSlotIndex,true>::RecordNByteValueBeforeWrite(val, pt); break;
                        case 8: RedSpyAnalysis<double, 32, readBufferSlotIndex,true>::RecordNByteValueBeforeWrite(val, pt); break;
                        default: DR_ASSERT_MSG(false, "InstrumentReadValueBeforeAndAfterWriting: handle large mem write with large operand size\n"); break;
                    }
                }break;
                default: DR_ASSERT_MSG(false, "InstrumentReadValueBeforeAndAfterWriting: Unexpected large memory writes\n"); break;
            }
        } else {
            switch(size) {
                case 1: RedSpyAnalysis<uint8_t, 1, readBufferSlotIndex,false>::RecordNByteValueBeforeWrite(val, pt); break;
                case 2: RedSpyAnalysis<uint16_t, 2, readBufferSlotIndex,false>::RecordNByteValueBeforeWrite(val, pt); break;
                case 4: RedSpyAnalysis<uint32_t, 4, readBufferSlotIndex,false>::RecordNByteValueBeforeWrite(val, pt); break;
                case 8: RedSpyAnalysis<uint64_t, 8, readBufferSlotIndex,false>::RecordNByteValueBeforeWrite(val, pt); break;
                default: {
                    RecordValueBeforeLargeWrite(val, size, readBufferSlotIndex, pt); break;
                }
            }
        }
    } else {
        if(is_float) {
            switch(size) {
                case 1:
                case 2: DR_ASSERT_MSG(false, "trace_update_cb: memory write floating data with unexptected small size.\n");
                case 4: RedSpyAnalysis<float, 4, readBufferSlotIndex,true>::ApproxCheckAfterWrite(addr, val, curCtxtHandle, pt); break;
                case 8: RedSpyAnalysis<double, 8, readBufferSlotIndex,true>::ApproxCheckAfterWrite(addr, val, curCtxtHandle, pt); break;
                case 10: RedSpyAnalysis<uint8_t, 10, readBufferSlotIndex,true>::ApproxCheckAfterWrite(addr, val, curCtxtHandle, pt); break;
                case 16: {
                    switch(esize) {
                        case 4: RedSpyAnalysis<float, 16, readBufferSlotIndex,true>::ApproxCheckAfterWrite(addr, val, curCtxtHandle, pt); break;
                        case 8: RedSpyAnalysis<double, 16, readBufferSlotIndex,true>::ApproxCheckAfterWrite(addr, val, curCtxtHandle, pt); break;
                        default: DR_ASSERT_MSG(false, "InstrumentReadValueBeforeAndAfterWriting: handle large mem write with large operand size\n"); break;
                    }
                }break;
                case 32: {
                    switch(esize) {
                        case 4: RedSpyAnalysis<float, 32, readBufferSlotIndex,true>::ApproxCheckAfterWrite(addr, val, curCtxtHandle, pt); break;
                        case 8: RedSpyAnalysis<double, 32, readBufferSlotIndex,true>::ApproxCheckAfterWrite(addr, val, curCtxtHandle, pt); break;
                        default: DR_ASSERT_MSG(false, "InstrumentReadValueBeforeAndAfterWriting: handle large mem write with large operand size\n"); break;
                    }
                }break;
                default: DR_ASSERT_MSG(false, "InstrumentReadValueBeforeAndAfterWriting: Unexpected large memory writes\n"); break;
            }
        } else {
            switch(size) {
                case 1: RedSpyAnalysis<uint8_t, 1, readBufferSlotIndex,false>::CheckNByteValueAfterWrite(addr, val, curCtxtHandle, pt); break;
                case 2: RedSpyAnalysis<uint16_t, 2, readBufferSlotIndex,false>::CheckNByteValueAfterWrite(addr, val, curCtxtHandle, pt); break;
                case 4: RedSpyAnalysis<uint32_t, 4, readBufferSlotIndex,false>::CheckNByteValueAfterWrite(addr, val, curCtxtHandle, pt); break;
                case 8: RedSpyAnalysis<uint64_t, 8, readBufferSlotIndex,false>::CheckNByteValueAfterWrite(addr, val, curCtxtHandle, pt); break;
                default: {
                    CheckAfterLargeWrite(addr, val, size, readBufferSlotIndex, curCtxtHandle, pt); break;
                }
            }
        }
    }
}

#ifdef X86
void InstrumentReg(int size, int esize, bool is_float, void *addr, uint8_t* val, uint32_t cct, per_thread_t *pt) {
    reg_id_t *reg = (reg_id_t*) &addr;
    // dr_fprintf(STDOUT, "For REG %s\n", get_register_name(*reg));
    if(RegHasAlias(*reg)) {
        uint32_t aliasIDs = GetAliasIDs(*reg);
        uint8_t regId = static_cast<uint8_t>(((aliasIDs)  & 0x00ffffff) >> 16 );
        if(is_float) {
            switch(size) {
                case 1: DR_ASSERT_MSG(false, "trace_update_cb: Unexptected small floating size.\n"); break;
                case 2: break;
                case 4: ApproxGeneralRegisters<float, true>::CheckValues(val, regId, cct, pt); break;
                case 8: ApproxGeneralRegisters<double, true>::CheckValues(val, regId, cct, pt); break;
                default: break;
            }
        } else {
            switch(size) {
                case 8: HandleAliasRegisters<uint64_t, ALIAS_GENERIC>::CheckUpdateGenericAlias(regId, *(uint64_t*)val, cct, pt); break;
                case 4: HandleAliasRegisters<uint32_t, ALIAS_GENERIC>::CheckUpdateGenericAlias(regId, *(uint32_t*)val, cct, pt); break;
                case 2: HandleAliasRegisters<uint16_t, ALIAS_GENERIC>::CheckUpdateGenericAlias(regId, *(uint16_t*)val, cct, pt); break;
                case 1: if(REG_is_Lower8(*reg)) {
                    HandleAliasRegisters<uint8_t, ALIAS_LOW_BYTE>::CheckUpdateGenericAlias(regId, *(uint8_t*)val, cct, pt);
                } else {
                    HandleAliasRegisters<uint8_t, ALIAS_HIGH_BYTE>::CheckUpdateGenericAlias(regId, *(uint8_t*)val, cct, pt);
                }break;
                default: break;
            }
        }
    } else {
        if(is_float) {
            switch(size) {
                case 1: DR_ASSERT_MSG(false, "trace_update_cb: Unexptected small floating size.\n"); break;
                case 2: break;
                case 4: ApproxGeneralRegisters<float, false>::CheckValues(val, *reg, cct, pt); break;
                case 8: ApproxGeneralRegisters<double, false>::CheckValues(val, *reg, cct, pt); break;
                case 10: Check10BytesReg(val, *reg, cct, pt); break;
                case 16: {
                    switch(esize) {
                        case 4: ApproxLargeRegisters<float, 16>::CheckValues(val, *reg - DR_REG_XMM0, cct, pt); break;
                        case 8: ApproxLargeRegisters<double, 16>::CheckValues(val, *reg - DR_REG_XMM0, cct, pt); break;
                        default: DR_ASSERT_MSG(false, "trace_update_cb: handle large reg with large operand size\n"); break;
                    }
                }break;
                case 32: {
                    switch(esize) {
                        case 4: ApproxLargeRegisters<float, 32>::CheckValues(val, *reg - DR_REG_YMM0, cct, pt); break;
                        case 8: ApproxLargeRegisters<double, 32>::CheckValues(val, *reg - DR_REG_YMM0, cct, pt); break;
                        default: DR_ASSERT_MSG(false, "trace_update_cb: handle large reg with large operand size\n"); break;
                    }
                }break;
                case 64: {
                    switch(esize) {
                        case 4: ApproxLargeRegisters<float, 64>::CheckValues(val, *reg - DR_REG_ZMM0, cct, pt); break;
                        case 8: ApproxLargeRegisters<double, 64>::CheckValues(val, *reg - DR_REG_ZMM0, cct, pt); break;
                        default: DR_ASSERT_MSG(false, "trace_update_cb: handle large reg with large operand size\n"); break;
                    }
                }break;
                default: DR_ASSERT_MSG(false, "trace_update_cb: not recoganized register size for floating instruction!\n"); break;
            }
        } else {
            switch(size) {
                case 1: HandleGeneralRegisters<uint8_t, 1>::CheckValues(*val, *reg, cct, pt); break;
                case 2: HandleGeneralRegisters<uint16_t, 1>::CheckValues(*(uint16_t*)val, *reg, cct, pt); break;
                case 4: HandleGeneralRegisters<uint32_t, 1>::CheckValues(*(uint32_t*)val, *reg, cct, pt); break;
                case 8: HandleGeneralRegisters<uint64_t, 1>::CheckValues(*(uint64_t*)val, *reg, cct, pt); break;
                case 16: HandleSpecialRegisters<2>::CheckRegValues(val, *reg - DR_REG_XMM0, cct, pt);break;
                case 32: HandleSpecialRegisters<4>::CheckRegValues(val, *reg - DR_REG_YMM0, cct, pt);break;
                case 64: HandleSpecialRegisters<8>::CheckRegValues(val, *reg - DR_REG_ZMM0, cct, pt);break;
            }
        }
    }
}
#else
void InstrumentReg(int size, int esize, bool is_float, void *addr, uint8_t* val, uint32_t cct, per_thread_t *pt) {
    reg_id_t *reg = (reg_id_t*) &addr;
    bool is_gpr = reg_is_gpr(*reg);
    if(is_gpr) {
        if(is_float) {
            switch(size) {
                case 4: ApproxGeneralRegisters<float>::CheckValues(val, *reg - DR_REG_W0, cct, pt); break;
                case 8: ApproxGeneralRegisters<double>::CheckValues(val, *reg - DR_REG_X0, cct, pt); break;
                default: DR_ASSERT_MSG(false, "trace_update_cb: not recoganized gp register size for floating instruction!\n"); break;
            }
        } else {
            switch(size) {
                case 4: HandleGeneralRegisters<uint32_t, 1>::CheckValues(*(uint32_t*)val, *reg - DR_REG_W0, cct, pt); break;
                case 8: HandleGeneralRegisters<uint64_t, 1>::CheckValues(*(uint64_t*)val, *reg - DR_REG_X0, cct, pt); break;
                default: DR_ASSERT_MSG(false, "trace_update_cb: not recoganized gp register size for integer instruction!\n"); break;
            }
        }
    } else {
        if(is_float) {
            switch(size) {
                case 1: ApproxGeneralRegisters<uint8_t>::CheckValues(val, *reg - DR_REG_B0, cct, pt); break;
                case 2: ApproxGeneralRegisters<uint16_t>::CheckValues(val, *reg - DR_REG_H0, cct, pt); break;
                case 4: ApproxGeneralRegisters<float>::CheckValues(val, *reg - DR_REG_S0, cct, pt); break;
                case 8: ApproxGeneralRegisters<double>::CheckValues(val, *reg - DR_REG_D0, cct, pt); break;
                // case 10: Check10BytesReg(val, *reg, cct, pt); break;
                case 16: {
                    switch(esize) {
                        case 2: ApproxLargeRegisters<uint16_t, 16>::CheckValues(val, *reg - DR_REG_Q0, cct, pt); break;
                        case 4: ApproxLargeRegisters<float, 16>::CheckValues(val, *reg - DR_REG_Q0, cct, pt); break;
                        case 8: ApproxLargeRegisters<double, 16>::CheckValues(val, *reg - DR_REG_Q0, cct, pt); break;
                        default: DR_ASSERT_MSG(false, "trace_update_cb: handle large reg with large operand size\n"); break;
                    }
                }break;
                default: DR_ASSERT_MSG(false, "trace_update_cb: not recoganized simd register size for floating instruction!\n"); break;
            }
        } else {
            switch(size) {
                case 1: HandleGeneralRegisters<uint8_t, 1>::CheckValues(*val, *reg - DR_REG_B0, cct, pt); break;
                case 2: HandleGeneralRegisters<uint16_t, 1>::CheckValues(*(uint16_t*)val, *reg - DR_REG_H0, cct, pt); break;
                case 4: HandleGeneralRegisters<uint32_t, 1>::CheckValues(*(uint32_t*)val, *reg - DR_REG_S0, cct, pt); break;
                case 8: HandleGeneralRegisters<uint64_t, 1>::CheckValues(*(uint64_t*)val, *reg - DR_REG_D0, cct, pt); break;
                default: DR_ASSERT_MSG(false, "trace_update_cb: not recoganized simd register size for integer instruction!\n"); break;
            }
        }
    }
}
#endif

// template<int size, int esize, bool is_float>
void trace_update_cb(val_info_t *info) {
    per_thread_t* pt = (per_thread_t *)drmgr_get_tls_field(dr_get_current_drcontext(), tls_idx);
    void* addr = (void*)info->addr;
    int32_t cct = info->ctxt_hndl;
    uint32_t type = info->type;
    uint8_t *val = (uint8_t*)info->val;
    int size = info->size;
    int esize = info->esize;
    bool is_float = info->is_float;

    // skip unknown ins's op
    if(esize == 0) return;

    if((TEST_OPND_MASK(type, GPR_REGISTER) || TEST_OPND_MASK(type, SIMD_REGISTER))) {
        pt->bytesWritten += size;
        InstrumentReg(size, esize, is_float, addr, val, cct, pt);
    } else {
        if(TEST_OPND_MASK(type, AFTER)) {
            pt->bytesWritten += size;
            switch(pt->readBufferSlotIndex) {
                case 0: InstrumentReadValueBeforeAndAfterWriting<0, false>(size, esize, is_float, addr, val, cct, pt); break;
                case 1: InstrumentReadValueBeforeAndAfterWriting<1, false>(size, esize, is_float, addr, val, cct, pt); break;
                case 2: InstrumentReadValueBeforeAndAfterWriting<2, false>(size, esize, is_float, addr, val, cct, pt); break;
                case 3: InstrumentReadValueBeforeAndAfterWriting<3, false>(size, esize, is_float, addr, val, cct, pt); break;
                case 4: InstrumentReadValueBeforeAndAfterWriting<4, false>(size, esize, is_float, addr, val, cct, pt); break;
                case 5: InstrumentReadValueBeforeAndAfterWriting<5, false>(size, esize, is_float, addr, val, cct, pt); break;
                case 6: InstrumentReadValueBeforeAndAfterWriting<6, false>(size, esize, is_float, addr, val, cct, pt); break;
                case 7: InstrumentReadValueBeforeAndAfterWriting<7, false>(size, esize, is_float, addr, val, cct, pt); break;
                default: DR_ASSERT_MSG(false, "trace_update_cb: readBufferSlotIndex overflow!\n"); break;
            }
            pt->readBufferSlotIndex++;
            if(pt->readBufferSlotIndex == pt->slotNum) {
                pt->readBufferSlotIndex = 0;
                pt->slotNum = 0;
            }
        } else {
            switch(pt->slotNum) {
                case 0: InstrumentReadValueBeforeAndAfterWriting<0, true>(size, esize, is_float, addr, val, cct, pt); break;
                case 1: InstrumentReadValueBeforeAndAfterWriting<1, true>(size, esize, is_float, addr, val, cct, pt); break;
                case 2: InstrumentReadValueBeforeAndAfterWriting<2, true>(size, esize, is_float, addr, val, cct, pt); break;
                case 3: InstrumentReadValueBeforeAndAfterWriting<3, true>(size, esize, is_float, addr, val, cct, pt); break;
                case 4: InstrumentReadValueBeforeAndAfterWriting<4, true>(size, esize, is_float, addr, val, cct, pt); break;
                case 5: InstrumentReadValueBeforeAndAfterWriting<5, true>(size, esize, is_float, addr, val, cct, pt); break;
                case 6: InstrumentReadValueBeforeAndAfterWriting<6, true>(size, esize, is_float, addr, val, cct, pt); break;
                case 7: InstrumentReadValueBeforeAndAfterWriting<7, true>(size, esize, is_float, addr, val, cct, pt); break;
                default: DR_ASSERT_MSG(false, "trace_update_cb: readBufferSlotIndex overflow!\n"); break;
            }
            pt->slotNum++;
        }
    }
}

struct RedundacyData {
    context_handle_t dead;
    context_handle_t kill;
    uint64_t frequency;
};

static inline bool RedundacyCompare(const struct RedundacyData &first, const struct RedundacyData &second) {
    return first.frequency > second.frequency ? true : false;
}

static void PrintRedundancyPairs(per_thread_t *pt) {
    vector<RedundacyData> tmpList;
    vector<RedundacyData>::iterator tmpIt;

    uint64_t grandTotalRedundantBytes = 0;
    dr_fprintf(pt->output_file, "*************** Dump Data from Thread %d ****************\n", pt->threadId);
    
#ifdef MERGING
    for (unordered_map<uint64_t, uint64_t>::iterator it = pt->RedMap->begin(); it != pt->RedMap->end(); ++it) {
        context_handle_t dead = DECODE_DEAD((*it).first);
        context_handle_t kill = DECODE_KILL((*it).first);

        for(tmpIt = tmpList.begin();tmpIt != tmpList.end(); ++tmpIt){
            if(dead == 0 || ((*tmpIt).dead) == 0){
                continue;
            }
            if (!drcctlib_have_same_caller_prefix(dead,(*tmpIt).dead)) {
                continue;
            }
            if (!drcctlib_have_same_caller_prefix(kill,(*tmpIt).kill)) {
                continue;
            }
            bool ct1 = drcctlib_have_same_source_line(dead,(*tmpIt).dead);
            bool ct2 = drcctlib_have_same_source_line(kill,(*tmpIt).kill);
            if(ct1 && ct2){
                  (*tmpIt).frequency += (*it).second;
                  grandTotalRedundantBytes += (*it).second;
                  break;
            }
        }
        if(tmpIt == tmpList.end()){
             RedundacyData tmp = { dead, kill, (*it).second};
             tmpList.push_back(tmp);
             grandTotalRedundantBytes += tmp.frequency;
        }
    }
#else
    for (unordered_map<uint64_t, uint64_t>::iterator it = pt->RedMap->begin(); it != pt->RedMap->end(); ++it) {
        RedundacyData tmp = { DECODE_DEAD ((*it).first), DECODE_KILL((*it).first), (*it).second};
        tmpList.push_back(tmp);
        grandTotalRedundantBytes += tmp.frequency;
    }
#endif

    __sync_fetch_and_add(&grandTotBytesRedWritten, grandTotalRedundantBytes);
    
    dr_fprintf(pt->output_file, "\n Total redundant bytes = %f %%\n", grandTotalRedundantBytes * 100.0 / pt->bytesWritten);
    
    sort(tmpList.begin(), tmpList.end(), RedundacyCompare);
    int cntxtNum = 0;
    for (vector<RedundacyData>::iterator listIt = tmpList.begin(); listIt != tmpList.end(); ++listIt) {
        if (cntxtNum < MAX_REDUNDANT_CONTEXTS_TO_LOG) {
            dr_fprintf(pt->output_file, "\n======= (%f) %% ======\n", (*listIt).frequency * 100.0 / grandTotalRedundantBytes);
            if ((*listIt).dead == 0) {
                dr_fprintf(pt->output_file, "\n Prepopulated with  by OS\n");
            } else {
                drcctlib_print_backtrace(pt->output_file, (*listIt).dead, true, true, MAX_DEPTH);
            }
            dr_fprintf(pt->output_file, "\n---------------------Redundantly written by---------------------------\n");
            drcctlib_print_backtrace(pt->output_file, (*listIt).kill, true, true, MAX_DEPTH);
        }
        else {
            break;
        }
        cntxtNum++;
    }
}

static void PrintApproximationRedundancyPairs(per_thread_t *pt) {
    vector<RedundacyData> tmpList;
    vector<RedundacyData>::iterator tmpIt;
    
    uint64_t grandTotalRedundantBytes = 0;
    dr_fprintf(pt->output_file, "*************** Dump Data(delta=%.2f%%) from Thread %d ****************\n", delta*100,pt->threadId);
    
#ifdef MERGING
    for (unordered_map<uint64_t, uint64_t>::iterator it = pt->ApproxRedMap->begin(); it != pt->ApproxRedMap->end(); ++it) {
        context_handle_t dead = DECODE_DEAD((*it).first);
        context_handle_t kill = DECODE_KILL((*it).first);
        
        for(tmpIt = tmpList.begin();tmpIt != tmpList.end(); ++tmpIt){
            if(dead == 0 || ((*tmpIt).dead) == 0){
                continue;
            }
            if (!drcctlib_have_same_caller_prefix(dead,(*tmpIt).dead)) {
                continue;
            }
            if (!drcctlib_have_same_caller_prefix(kill,(*tmpIt).kill)) {
                continue;
            }
            bool ct1 = drcctlib_have_same_source_line(dead,(*tmpIt).dead);
            bool ct2 = drcctlib_have_same_source_line(kill,(*tmpIt).kill);
            if(ct1 && ct2){
                (*tmpIt).frequency += (*it).second;
                grandTotalRedundantBytes += (*it).second;
                grandTotalRedundantIns += 1;
                break;
            }
        }
        if(tmpIt == tmpList.end()){
            RedundacyData tmp = { dead, kill, (*it).second};
            tmpList.push_back(tmp);
            grandTotalRedundantBytes += tmp.frequency;
        }
    }
#else
    for (unordered_map<uint64_t, uint64_t>::iterator it = pt->ApproxRedMap->begin(); it != pt->ApproxRedMap->end(); ++it) {
        RedundacyData tmp = { DECODE_DEAD ((*it).first), DECODE_KILL((*it).first), (*it).second};
        tmpList.push_back(tmp);
        grandTotalRedundantBytes += tmp.frequency;
    }
#endif

    __sync_fetch_and_add(&grandTotBytesApproxRedWritten, grandTotalRedundantBytes);

    dr_fprintf(pt->output_file, "\n Total redundant bytes = %f %%\n", grandTotalRedundantBytes * 100.0 / pt->bytesWritten);
    
    sort(tmpList.begin(), tmpList.end(), RedundacyCompare);
    int cntxtNum = 0;
    for (vector<RedundacyData>::iterator listIt = tmpList.begin(); listIt != tmpList.end(); ++listIt) {
        if (cntxtNum < MAX_REDUNDANT_CONTEXTS_TO_LOG) {
            dr_fprintf(pt->output_file, "\n======= (%f) %% ======\n", (*listIt).frequency * 100.0 / grandTotalRedundantBytes);
            if ((*listIt).dead == 0) {
                dr_fprintf(pt->output_file, "\n Prepopulated with  by OS\n");
            } else {
                drcctlib_print_backtrace(pt->output_file, (*listIt).dead, true, true, MAX_DEPTH);
            }
            dr_fprintf(pt->output_file, "\n---------------------Redundantly written by---------------------------\n");
            drcctlib_print_backtrace(pt->output_file, (*listIt).kill, true, true, MAX_DEPTH);
        }
        else {
            break;
        }
        cntxtNum++;
    }
}

static void HPCRunRedundancyPairs(per_thread_t *pt) {
    vector<RedundacyData> tmpList;
    vector<RedundacyData>::iterator tmpIt;
    
    for (unordered_map<uint64_t, uint64_t>::iterator it = pt->RedMap->begin(); it != pt->RedMap->end(); ++it) {
        RedundacyData tmp = { DECODE_DEAD ((*it).first), DECODE_KILL((*it).first), (*it).second};
        tmpList.push_back(tmp);
    }
    
    sort(tmpList.begin(), tmpList.end(), RedundacyCompare);
    vector<HPCRunCCT_t*> HPCRunNodes;
    int cntxtNum = 0;
    for (vector<RedundacyData>::iterator listIt = tmpList.begin(); listIt != tmpList.end(); ++listIt) {
        if (cntxtNum < MAX_REDUNDANT_CONTEXTS_TO_LOG) {
            HPCRunCCT_t *HPCRunNode = new HPCRunCCT_t();
            HPCRunNode->ctxt_hndl_list.push_back((*listIt).dead);
            HPCRunNode->ctxt_hndl_list.push_back((*listIt).kill);
            HPCRunNode->metric_list.push_back((*listIt).frequency);
            HPCRunNodes.push_back(HPCRunNode);
        }
        else {
            break;
        }
        cntxtNum++;
    }
    void *drcontext = dr_get_current_drcontext();
    build_thread_custom_cct_hpurun_format(HPCRunNodes, drcontext);
    write_thread_custom_cct_hpurun_format(drcontext);
}

static void
ClientThreadEnd(void *drcontext)
{
    per_thread_t *pt = (per_thread_t *)drmgr_get_tls_field(drcontext, tls_idx);
    dr_fprintf(gTraceFile, "Thread %d ThreadTotalBytesWritten = %u\n", pt->threadId, pt->bytesWritten);
    dr_fprintf(pt->output_file, "ThreadTotalBytesWritten = %u\n", pt->bytesWritten);
    __sync_fetch_and_add(&grandTotBytesWritten,pt->bytesWritten);

    // output the CCT for hpcviewer format
    // HPCRunRedundancyPairs(pt);
    // HPCRunApproxRedundancyPairs(pt);

    PrintRedundancyPairs(pt);
    PrintApproximationRedundancyPairs(pt);

    dr_close_file(pt->output_file);
    delete pt->RedMap;
    delete pt->ApproxRedMap;
    dr_thread_free(drcontext, pt, sizeof(per_thread_t));
}

static void
ThreadOutputFileInit(per_thread_t *pt)
{
    int32_t id = drcctlib_get_thread_id();
    pt->threadId = id;
    char name[MAX_FILE_PATH] = "";
    sprintf(name + strlen(name), "%s/thread-%d.topn.log", g_folder_name.c_str(), id);
    pt->output_file = dr_open_file(name, DR_FILE_WRITE_OVERWRITE | DR_FILE_ALLOW_LARGE);
    DR_ASSERT(pt->output_file != INVALID_FILE);
    if (op_enable_sampling.get_value()) {
        dr_fprintf(pt->output_file, "[REDSPY INFO] Sampling Enabled\n");
    } else {
        dr_fprintf(pt->output_file, "[REDSPY INFO] Sampling Disabled\n");
    }
}

static void
ClientThreadStart(void *drcontext)
{
    per_thread_t *pt = (per_thread_t *)dr_thread_alloc(drcontext, sizeof(per_thread_t));
    if (pt == NULL) {
        REDSPY_EXIT_PROCESS("pt == NULL");
    }
    pt->bytesWritten = 0;
    pt->RedMap = new unordered_map<uint64_t, uint64_t>();
    pt->ApproxRedMap = new unordered_map<uint64_t, uint64_t>();
    pt->readBufferSlotIndex = 0;
    pt->slotNum = 0;
    drmgr_set_tls_field(drcontext, tls_idx, (void *)pt);
    ThreadOutputFileInit(pt);
}

static void
ClientExit(void)
{
    dr_fprintf(gTraceFile, "\n#Redundant Write:");
    dr_fprintf(gTraceFile, "\nTotalBytesWritten: %lu \n",grandTotBytesWritten);
    dr_fprintf(gTraceFile, "\nRedundantBytesWritten: %lu %.2f\n",grandTotBytesRedWritten, grandTotBytesRedWritten * 100.0/grandTotBytesWritten);
    dr_fprintf(gTraceFile, "\nApproxRedundantBytesWritten: %lu %.2f\n",grandTotBytesApproxRedWritten, grandTotBytesApproxRedWritten * 100.0/grandTotBytesWritten);

    delete ctxt_sm;
    dr_close_file(gTraceFile);

    if (!dr_raw_tls_cfree(tls_offs, INSTRACE_TLS_COUNT)) {
        REDSPY_EXIT_PROCESS(
            "ERROR: redspy dr_raw_tls_calloc fail");
    }

    dr_mutex_destroy(gLock);
    if (!drmgr_unregister_thread_init_event(ClientThreadStart) ||
        !drmgr_unregister_thread_exit_event(ClientThreadEnd) ||
        !drmgr_unregister_tls_field(tls_idx)) {
        printf("ERROR: redspy failed to unregister in ClientExit");
        fflush(stdout);
        exit(-1);
    }
    vprofile_unregister_trace(vtrace);
    vprofile_exit();
}

static void ClientInit(int argc, const char* argv[]) {
    /* Parse options */
    std::string parse_err;
    int last_index;
    if (!droption_parser_t::parse_argv(DROPTION_SCOPE_CLIENT, argc, argv, &parse_err, &last_index)) {
        dr_fprintf(STDERR, "Usage error: %s", parse_err.c_str());
        dr_abort();
    }

    ctxt_sm = new ConcurrentShadowMemory<uint32_t>();

    // Create output file
    #ifdef ARM_CCTLIB
    char name[MAX_FILE_PATH] = "arm-";
#else
    char name[MAX_FILE_PATH] = "x86-";
#endif
    pid_t pid = getpid();
    
    gethostname(name + strlen(name), MAX_FILE_PATH - strlen(name));
    sprintf(name + strlen(name), "-%d-redspy", pid);
    g_folder_name.assign(name, strlen(name));
    mkdir(g_folder_name.c_str(), S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH);

    dr_fprintf(STDOUT, "[REDSPY INFO] Profiling result directory: %s\n", g_folder_name.c_str());

    sprintf(name+strlen(name), "/redspy.log");
    gTraceFile = dr_open_file(name, DR_FILE_WRITE_OVERWRITE | DR_FILE_ALLOW_LARGE);

    // print the arguments passed
    dr_fprintf(gTraceFile, "\n");
    
    for(int i = 0 ; i < argc; i++) {
        dr_fprintf(gTraceFile, "%s ", argv[i]);
    }
    
    dr_fprintf(gTraceFile, "\n");
}

#ifdef __cplusplus
extern "C" {
#endif

DR_EXPORT void
dr_client_main(client_id_t id, int argc, const char *argv[])
{
    dr_set_client_name("DynamoRIO Client 'redspy'",
                       "http://dynamorio.org/issues");
    ClientInit(argc, argv);

    if(!vprofile_init(REDSPY_FILTER_MEM_ACCESS_INSTR, NULL, NULL, NULL,
                     VPROFILE_COLLECT_CCT)) {
        REDSPY_EXIT_PROCESS("ERROR: redspy unable to initialize vprofile");
    }

    if(op_enable_sampling.get_value()) {
        vtracer_enable_sampling(op_window_enable.get_value(), op_window.get_value());
    }

    drmgr_priority_t thread_init_pri = { sizeof(thread_init_pri),
                                         "redspy-thread-init", NULL, NULL,
                                         DRCCTLIB_THREAD_EVENT_PRI + 1 };
    drmgr_priority_t thread_exit_pri = { sizeof(thread_exit_pri),
                                         "redspy-thread-exit", NULL, NULL,
                                         DRCCTLIB_THREAD_EVENT_PRI - 1 };

    if (   !drmgr_register_thread_init_event_ex(ClientThreadStart, &thread_init_pri) 
        || !drmgr_register_thread_exit_event_ex(ClientThreadEnd, &thread_exit_pri) ) {
        REDSPY_EXIT_PROCESS("ERROR: redspy unable to register events");
    }

    tls_idx = drmgr_register_tls_field();
    if (tls_idx == -1) {
        REDSPY_EXIT_PROCESS("ERROR: redspy drmgr_register_tls_field fail");
    }
    if (!dr_raw_tls_calloc(&tls_seg, &tls_offs, INSTRACE_TLS_COUNT, 0)) {
        REDSPY_EXIT_PROCESS(
            "ERROR: redspy dr_raw_tls_calloc fail");
    }
    gLock = dr_mutex_create();

    vtrace = vprofile_allocate_trace(VPROFILE_TRACE_VAL_CCT_ADDR | VPROFILE_TRACE_BEFORE_WRITE | VPROFILE_TRACE_STRICTLY_ORDERED);

    uint32_t opnd_mask = (ANY_DATA_TYPE | GPR_REGISTER | SIMD_REGISTER | MEMORY | WRITE | BEFORE | AFTER);

    // Tracing Buffer
    vprofile_register_trace_cb(vtrace, VPROFILE_FILTER_OPND, opnd_mask, ANY, trace_update_cb);

    dr_register_exit_event(ClientExit);
}

#ifdef __cplusplus
}
#endif