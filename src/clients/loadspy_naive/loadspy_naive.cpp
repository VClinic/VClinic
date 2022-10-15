// @COPYRIGHT@
// Licensed under MIT license.
// See LICENSE.TXT file in the project root for more information.
// ==============================================================

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <atomic>
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
#include "utils.h"

using namespace std;

#define IS_ACCESS_WITHIN_PAGE_BOUNDARY(accessAddr, accessLen)  (PAGE_OFFSET((accessAddr)) <= (PAGE_OFFSET_MASK - (accessLen)))

/* Other footprint_client settings */
#define MAX_REDUNDANT_CONTEXTS_TO_LOG (1000)
#define MAX_FILE_PATH   (200)
#define MAX_DEPTH 10

#define DECODE_DEAD(data) static_cast<context_handle_t>(((data)  & 0xffffffffffffffff) >> 32 )
#define DECODE_KILL(data) (static_cast<context_handle_t>( (data)  & 0x00000000ffffffff))


#define MAKE_CONTEXT_PAIR(a, b) (((uint64_t)(a) << 32) | ((uint64_t)(b)))

#define delta 0.01

#define WINDOW_ENABLE 1000000
#define WINDOW_DISABLE 100000000

int window_enable;
int window_disable;

// Client Options
#include "droption.h"

 #define LOADSPY_EXIT_PROCESS(format, args...)                                           \
    DRCCTLIB_CLIENT_EXIT_PROCESS_TEMPLATE("loadspy", format, \
                                          ##args)

#ifdef ARM_CCTLIB
#    define OPND_CREATE_CCT_INT OPND_CREATE_INT
#else
#    define OPND_CREATE_CCT_INT OPND_CREATE_INT32
#endif

#ifdef ARM_CCTLIB
#    define OPND_CREATE_IMMEDIATE_INT OPND_CREATE_INT
#else
#    ifdef CCTLIB_64
#        define OPND_CREATE_IMMEDIATE_INT OPND_CREATE_INT64
#    else
#        define OPND_CREATE_IMMEDIATE_INT OPND_CREATE_INT32
#    endif
#endif

bool instr_is_ignorable(instr_t *ins) {
    int opc = instr_get_opcode(ins);
#ifdef AARCH64
    if(instr_is_exclusive_load(ins) || instr_is_exclusive_store(ins)) {
        return false;
    }
#endif
    switch (opc) {
        case OP_nop:
#ifdef X86
	    case OP_nop_modrm:
#endif

#if defined(AARCH64)
        case OP_isb:
        case OP_ld3:
        case OP_ld3r:
#endif
                return true;
        default:
                return false;
    }

    return false;
}

// We only interest in memory loads
bool LOADSPY_FILTER_MEM_ACCESS_INSTR(instr_t *instr) {
    if(!instr_reads_memory(instr)) return false;
    if(instr_is_call(instr)) return false;
    if(instr_is_prefetch(instr)) return false;
    if(instr_is_return(instr)) return false;
    if(instr_is_syscall(instr)) return false;
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
        case OP_xrstor32:
        case OP_xrstor64:
            return false;
        default: return true;
    }
#endif
    return true;
}


/***********************************************
 ******  shadow memory
 ************************************************/
// Each memory byte M has a shadow byte to hold its previous value prevValue
// To maintain the context for a read operation, we store an additional pointer sized variable CONTEXT(M) (actually uint32_t) in the shadow memory for each memory byte M.
typedef struct _mem_node_t {
    uint32_t ctxt;
    uint8_t value;
} mem_node_t;

ConcurrentShadowMemory<mem_node_t> *mem_map;

// ConcurrentShadowMemory<uint32_t> *ctxt_sm;
// ConcurrentShadowMemory<uint8_t> *val_sm;

////////////////////////////////////////////////

struct RedSpyThreadData{
    
    uint64_t bytesLoad;
    
    long long numIns;
    bool sampleFlag;
};

typedef struct _per_thread_t {
    uint64_t bytesLoad;

    unordered_map<uint64_t, uint64_t>* RedMap;
    unordered_map<uint64_t, uint64_t>* ApproxRedMap;

    file_t output_file;
    int32_t threadId;

    vector<opnd_t*> *opnd_clones;
} per_thread_t;

// for metric logging
int redload_metric_id = 0;
int redload_approx_metric_id = 0;

//for statistics result
uint64_t grandTotBytesLoad;
uint64_t grandTotBytesRedLoad;
uint64_t grandTotBytesApproxRedLoad;

enum {
    INSTRACE_TLS_OFFS_BUF_PTR,
    INSTRACE_TLS_OFFS_INTLOG_PTR,
    INSTRACE_TLS_OFFS_FPLOG_PTR,
    INSTRACE_TLS_COUNT, /* total number of TLS slots allocated */
};

static int tls_idx;
static reg_id_t tls_seg;
static uint tls_offs;

file_t gTraceFile;
string g_folder_name;
// static void *gLock;

static inline void AddToRedTable(uint64_t key,  uint16_t value, per_thread_t* pt) __attribute__((always_inline,flatten));
static inline void AddToRedTable(uint64_t key,  uint16_t value, per_thread_t* pt) {
    unordered_map<uint64_t, uint64_t>::iterator it = pt->RedMap->find(key);
    if (it  == pt->RedMap->end()) {
        (*pt->RedMap)[key] = value;
    } else {
        it->second += value;
    }
}

static inline void AddToApproximateRedTable(uint64_t key,  uint16_t value, per_thread_t* pt) __attribute__((always_inline,flatten));
static inline void AddToApproximateRedTable(uint64_t key,  uint16_t value, per_thread_t* pt) {
    unordered_map<uint64_t, uint64_t>::iterator it = pt->ApproxRedMap->find(key);
    if (it  == pt->ApproxRedMap->end()) {
        (*pt->ApproxRedMap)[key] = value;
    } else {
        it->second += value;
    }
}

/***************************************************************************************/
/*********************** memory temporal redundancy functions **************************/
/***************************************************************************************/
template<int start, int end, int incr, bool conditional, bool approx>
struct UnrolledLoop{
    static __attribute__((always_inline)) void Body(function<void (const int)> func){
        func(start); // Real loop body
        UnrolledLoop<start+incr, end, incr, conditional, approx>:: Body(func);   // unroll next iteration
    }
    static __attribute__((always_inline)) void BodySamePage(mem_node_t* mem_node, const context_handle_t handle, per_thread_t *pt){
        if(conditional) {
            // report in RedTable
            if(approx)
                AddToApproximateRedTable(MAKE_CONTEXT_PAIR(mem_node[start].ctxt, handle), 1, pt);
            else
                AddToRedTable(MAKE_CONTEXT_PAIR(mem_node[start].ctxt, handle), 1, pt);
        }
        // Update context
        mem_node[start].ctxt = handle;
        UnrolledLoop<start+incr, end, incr, conditional, approx>:: BodySamePage(mem_node, handle, pt);   // unroll next iteration
    }
    static __attribute__((always_inline)) void BodyStraddlePage(uint64_t addr, const context_handle_t handle, per_thread_t *pt){
        mem_node_t* mem_node = (mem_node_t *)mem_map->GetOrCreateShadowAddress((size_t)addr+start);
        
        if (conditional) {
            // report in RedTable
            if(approx)
                AddToApproximateRedTable(MAKE_CONTEXT_PAIR(mem_node->ctxt, handle), 1, pt);
            else
                AddToRedTable(MAKE_CONTEXT_PAIR(mem_node->ctxt, handle), 1, pt);
        }
        // Update context
        mem_node->ctxt = handle;
        UnrolledLoop<start+incr, end, incr, conditional, approx>:: BodyStraddlePage(addr, handle, pt);   // unroll next iteration
    }
};

template<int end,  int incr, bool conditional, bool approx>
struct UnrolledLoop<end , end , incr, conditional, approx>{
    static __attribute__((always_inline)) void Body(function<void (const int)> func){}
    static __attribute__((always_inline)) void BodySamePage(mem_node_t* mem_node, const context_handle_t handle, per_thread_t *pt){}
    static __attribute__((always_inline)) void BodyStraddlePage(uint64_t addr, const context_handle_t handle, per_thread_t *pt){}
};

template<int start, int end, int incr>
struct UnrolledConjunction{
    static __attribute__((always_inline)) bool Body(function<bool (const int)> func){
        return func(start) && UnrolledConjunction<start+incr, end, incr>:: Body(func);   // unroll next iteration
    }
    static __attribute__((always_inline)) bool BodyContextCheck(mem_node_t* mem_node){
        return (mem_node[0].ctxt == mem_node[start].ctxt) && UnrolledConjunction<start+incr, end, incr>:: BodyContextCheck(mem_node);   // unroll next iteration
    }
};

template<int end,  int incr>
struct UnrolledConjunction<end , end , incr>{
    static __attribute__((always_inline)) bool Body(function<void (const int)> func){
        return true;
    }
    static __attribute__((always_inline)) bool BodyContextCheck(mem_node_t* mem_node){
        return true;
    }
};

template<int start, int end, int incr, class T, bool isApprox>
struct UnrolledSubLoop {
    static __attribute__((always_inline)) bool Body(function<bool (const int)> func){
        return func(start) && UnrolledSubLoop<start + incr, end, incr, T, isApprox>::Body(func);   // unroll next iteration
    }

    static __attribute__((always_inline)) bool BodyISRed(void *addr, mem_node_t* mem_node) {
        bool isRed = false;
        
        isRed = (reinterpret_cast<const T> (mem_node[start].value) == reinterpret_cast<const T *> (addr)[start]);

        return isRed &&
               UnrolledSubLoop<start + incr, end, incr, T, isApprox>::BodyISRed(addr, mem_node);   // unroll next iteration
    }

    static __attribute__((always_inline)) bool BodyStraddleISRed(void *addr, void *val) {
        bool isRed = false;
        mem_node_t* mem_node = (mem_node_t *)mem_map->GetOrCreateShadowAddress((size_t)addr+start);

        isRed = (reinterpret_cast<const T> (mem_node[0].value) == reinterpret_cast<const T *> (val)[start]);

        return isRed &&
               UnrolledSubLoop<start + incr, end, incr, T, isApprox>::BodyStraddleISRed(addr, val);   // unroll next iteration
    }
};

template<int end, int incr, class T, bool isApprox>
struct UnrolledSubLoop<end, end, incr, T, isApprox> {
    static __attribute__((always_inline)) bool Body(function<bool (const int)> func){ return true;}
    static __attribute__((always_inline)) bool BodyISRed(void *addr, mem_node_t* mem_node) { return true; }
    static __attribute__((always_inline)) bool BodyStraddleISRed(void *addr, void *val) { return true; }
};

//T换成uint8即可
template<int start, int end, int incr, class T>
struct UnrolledCopy {
    static __attribute__((always_inline)) void Body(function<void(const int)> func) {
        func(start);
        UnrolledCopy<start + incr, end, incr, T>::Body(func);   // unroll next iteration
    }

    static __attribute__((always_inline)) void BodyCopy(void *addr, mem_node_t* mem_node) {
        mem_node[start].value = *(static_cast<T *>(addr) + start);
        UnrolledCopy<start + incr, end, incr, T>::BodyCopy(addr, mem_node);   // unroll next iteration
    }

    static __attribute__((always_inline)) void BodyStraddleCopy(void *addr, void *val) {
        mem_node_t* mem_node = (mem_node_t *)mem_map->GetOrCreateShadowAddress((size_t)addr+start);

        mem_node->value = *(static_cast<T *>(val) + start);
        UnrolledCopy<start + incr, end, incr, T>::BodyStraddleCopy(addr, val);   // unroll next iteration
    }
};

template<int end, int incr, class T>
struct UnrolledCopy<end, end, incr, T> {
    static __attribute__((always_inline)) void Body(function<void(const int)> func) {}
    static __attribute__((always_inline)) void BodyCopy(void *addr, mem_node_t* mem_node) {}
    static __attribute__((always_inline)) void BodyStraddleCopy(void *addr, void *val) {}
};

// template<int start, int end, int incr, bool conditional, bool approx>
// struct UnrolledLoop{
//     static __attribute__((always_inline)) void Body(function<void (const int)> func){
//         func(start); // Real loop body
//         UnrolledLoop<start+incr, end, incr, conditional, approx>:: Body(func);   // unroll next iteration
//     }
//     static __attribute__((always_inline)) void BodySamePage(context_handle_t * __restrict__ prevIP, const context_handle_t handle, per_thread_t *pt){
//         if(conditional) {
//             // report in RedTable
//             if(approx)
//                 AddToApproximateRedTable(MAKE_CONTEXT_PAIR(prevIP[start], handle), 1, pt);
//             else
//                 AddToRedTable(MAKE_CONTEXT_PAIR(prevIP[start], handle), 1, pt);
//         }
//         // Update context
//         prevIP[start] = handle;
//         UnrolledLoop<start+incr, end, incr, conditional, approx>:: BodySamePage(prevIP, handle, pt);   // unroll next iteration
//     }
//     static __attribute__((always_inline)) void BodyStraddlePage(uint64_t addr, const context_handle_t handle, per_thread_t *pt){
//         mem_node_t* mem_node = (mem_node_t *)mem_map->GetOrCreateShadowAddress((size_t)addr+start);
//         // context_handle_t *prevIP = (context_handle_t *)ctxt_sm->GetOrCreateShadowAddress((size_t)addr+start);
        
//         if (conditional) {
//             // report in RedTable
//             if(approx)
//                 AddToApproximateRedTable(MAKE_CONTEXT_PAIR(mem_node->prevIP[0 /* 0 is correct*/ ], handle), 1, pt);
//             else
//                 AddToRedTable(MAKE_CONTEXT_PAIR(prevIP[0 /* 0 is correct*/ ], handle), 1, pt);
//         }
//         // Update context
//         prevIP[0] = handle;
//         UnrolledLoop<start+incr, end, incr, conditional, approx>:: BodyStraddlePage(addr, handle, pt);   // unroll next iteration
//     }
// };

// template<int end,  int incr, bool conditional, bool approx>
// struct UnrolledLoop<end , end , incr, conditional, approx>{
//     static __attribute__((always_inline)) void Body(function<void (const int)> func){}
//     static __attribute__((always_inline)) void BodySamePage(context_handle_t * __restrict__ prevIP, const context_handle_t handle, per_thread_t *pt){}
//     static __attribute__((always_inline)) void BodyStraddlePage(uint64_t addr, const context_handle_t handle, per_thread_t *pt){}
// };

// template<int start, int end, int incr>
// struct UnrolledConjunction{
//     static __attribute__((always_inline)) bool Body(function<bool (const int)> func){
//         return func(start) && UnrolledConjunction<start+incr, end, incr>:: Body(func);   // unroll next iteration
//     }
//     static __attribute__((always_inline)) bool BodyContextCheck(context_handle_t * __restrict__ prevIP){
//         return (prevIP[0] == prevIP[start]) && UnrolledConjunction<start+incr, end, incr>:: BodyContextCheck(prevIP);   // unroll next iteration
//     }
// };

// template<int end,  int incr>
// struct UnrolledConjunction<end , end , incr>{
//     static __attribute__((always_inline)) bool Body(function<void (const int)> func){
//         return true;
//     }
//     static __attribute__((always_inline)) bool BodyContextCheck(context_handle_t * __restrict__ prevIP){
//         return true;
//     }
// };

// template<int start, int end, int incr, class T, bool isApprox>
// struct UnrolledSubLoop {
//     static __attribute__((always_inline)) bool Body(function<bool (const int)> func){
//         return func(start) && UnrolledSubLoop<start + incr, end, incr, T, isApprox>::Body(func);   // unroll next iteration
//     }

//     static __attribute__((always_inline)) bool BodyISRed(void *addr, uint8_t *prev) {
//         bool isRed = false;
//         if (isApprox) {
//             T oldValue = reinterpret_cast<const T *> (prev)[start];
//             T newValue = reinterpret_cast<const T *> (addr)[start];
//             T rate = (newValue - oldValue) / oldValue;
//             isRed = (rate <= delta && rate >= -delta);
//         } else {
//             isRed = (reinterpret_cast<const T *> (prev)[start] == reinterpret_cast<const T *> (addr)[start]);
//         }

//         return isRed &&
//                UnrolledSubLoop<start + incr, end, incr, T, isApprox>::BodyISRed(addr, prev);   // unroll next iteration
//     }

//     static __attribute__((always_inline)) bool BodyStraddleISRed(void *addr, void *val) {
//         bool isRed = false;
//         uint8_t *prev = (uint8_t *)val_sm->GetOrCreateShadowAddress((size_t)addr+start);

//         if (isApprox) {
//             T oldValue = reinterpret_cast<const T *> (prev)[0];
//             T newValue = reinterpret_cast<const T *> (val)[start];
//             T rate = (newValue - oldValue) / oldValue;
//             isRed = (rate <= delta && rate >= -delta);
//         } else {
//             isRed = (reinterpret_cast<const T *> (prev)[0] == reinterpret_cast<const T *> (val)[start]);
//         }

//         return isRed &&
//                UnrolledSubLoop<start + incr, end, incr, T, isApprox>::BodyStraddleISRed(addr, val);   // unroll next iteration
//     }
// };

// template<int end, int incr, class T, bool isApprox>
// struct UnrolledSubLoop<end, end, incr, T, isApprox> {
//     static __attribute__((always_inline)) bool Body(function<bool (const int)> func){ return true;}
//     static __attribute__((always_inline)) bool BodyISRed(void *addr, uint8_t *prev) { return true; }
//     static __attribute__((always_inline)) bool BodyStraddleISRed(void *addr, void *val) { return true; }
// };

// template<int start, int end, int incr, class T>
// struct UnrolledCopy {
//     static __attribute__((always_inline)) void Body(function<void(const int)> func) {
//         func(start);
//         UnrolledCopy<start + incr, end, incr, T>::Body(func);   // unroll next iteration
//     }

//     static __attribute__((always_inline)) void BodyCopy(void *addr, uint8_t *prev) {
//         *((T *) (prev + start)) = *(static_cast<T *>(addr) + start);
//         UnrolledCopy<start + incr, end, incr, T>::BodyCopy(addr, prev);   // unroll next iteration
//     }

//     static __attribute__((always_inline)) void BodyStraddleCopy(void *addr, void *val) {
//         uint8_t *prev = (uint8_t *)val_sm->GetOrCreateShadowAddress((size_t)addr+start);

//         *((T *) (prev)) = *(static_cast<T *>(val) + start);
//         UnrolledCopy<start + incr, end, incr, T>::BodyStraddleCopy(addr, val);   // unroll next iteration
//     }
// };

// template<int end, int incr, class T>
// struct UnrolledCopy<end, end, incr, T> {
//     static __attribute__((always_inline)) void Body(function<void(const int)> func) {}
//     static __attribute__((always_inline)) void BodyCopy(void *addr, uint8_t *prev) {}
//     static __attribute__((always_inline)) void BodyStraddleCopy(void *addr, void *val) {}
// };

template<uint32_t AccessLen, bool isApprox>
struct RedSpyAnalysis{

        static __attribute__((always_inline)) void CheckNByteValueAfterRead(void* addr, uint8_t *val, uint32_t curCtxtHandle, per_thread_t *pt){
        mem_node_t* mem_node = (mem_node_t *)mem_map->GetOrCreateShadowAddress((size_t)addr);
        
        // bool isRedundantRead = IsReadRedundant((void*)val, prevValue);
        bool isRedundantRead = false;
        
        const bool isAccessWithinPageBoundary = IS_ACCESS_WITHIN_PAGE_BOUNDARY( (uint64_t)addr, AccessLen);
        if(isAccessWithinPageBoundary) {
            isRedundantRead = UnrolledSubLoop<0, AccessLen, 1, uint8_t, isApprox>::BodyISRed((void*)val, mem_node);
            UnrolledCopy<0, AccessLen, 1, uint8_t>::BodyCopy((void*)val, mem_node);
        } else {
            isRedundantRead = UnrolledSubLoop<0, AccessLen, 1, uint8_t, isApprox>::BodyStraddleISRed((void*)addr, (void*)val);
            UnrolledCopy<0, AccessLen, 1, uint8_t>::BodyStraddleCopy((void*)addr, (void*)val);
        }

        if(isRedundantRead) {
            // detected redundancy
            if(isAccessWithinPageBoundary) {
                // All from same ctxt?
                if (UnrolledConjunction<0, AccessLen, 1>::BodyContextCheck(mem_node)) {
                    // report in RedTable
                    if(isApprox)
                        AddToApproximateRedTable(MAKE_CONTEXT_PAIR(mem_node[0].ctxt, curCtxtHandle), AccessLen, pt);
                    else
                        AddToRedTable(MAKE_CONTEXT_PAIR(mem_node[0].ctxt, curCtxtHandle), AccessLen, pt);
                    // Update context
                    UnrolledLoop<0, AccessLen, 1, false, /* redundancy is updated outside*/ isApprox>::BodySamePage(mem_node, curCtxtHandle, pt);
                } else {
                    // different contexts
                    UnrolledLoop<0, AccessLen, 1, true, /* redundancy is updated inside*/ isApprox>::BodySamePage(mem_node, curCtxtHandle, pt);
                }
            } else {
                // Read across a 64-K page boundary
                // First byte is on this page though
                if(isApprox)
                    AddToApproximateRedTable(MAKE_CONTEXT_PAIR(mem_node[0].ctxt, curCtxtHandle), 1, pt);
                else
                    AddToRedTable(MAKE_CONTEXT_PAIR(mem_node[0].ctxt, curCtxtHandle), 1, pt);
                // Update context
                mem_node[0].ctxt = curCtxtHandle;
                
                // Remaining bytes [1..AccessLen] somewhere will across a 64-K page boundary
                UnrolledLoop<1, AccessLen, 1, true, /* update redundancy */ isApprox>::BodyStraddlePage( (uint64_t) addr, curCtxtHandle, pt);
            }
        } else {
            // No redundancy.
            // Just update contexts
            if(isAccessWithinPageBoundary) {
                // Update context
                UnrolledLoop<0, AccessLen, 1, false, /* not redundant*/ isApprox>::BodySamePage(mem_node, curCtxtHandle, pt);
            } else {
                // Read across a 64-K page boundary
                // Update context
                mem_node[0].ctxt = curCtxtHandle;
                
                // Remaining bytes [1..AccessLen] somewhere will across a 64-K page boundary
                UnrolledLoop<1, AccessLen, 1, false, /* not redundant*/ isApprox>::BodyStraddlePage( (uint64_t) addr, curCtxtHandle, pt);
            }
        }
    }
};


// static inline void CheckAfterLargeRead(void* addr, uint8_t *val, uint32_t accessLen, uint32_t curCtxtHandle, per_thread_t *pt){
//     context_handle_t * __restrict__ prevIP = (context_handle_t *)ctxt_sm->GetOrCreateShadowAddress((size_t)addr);
//     uint8_t* prevValue = val_sm->GetOrCreateShadowAddress((size_t)addr);
    
//     // This assumes that a large read cannot straddle a page boundary -- strong assumption, but lets go with it for now.
//     // tuple<uint8_t[SHADOW_PAGE_SIZE], context_handle_t[SHADOW_PAGE_SIZE]> &tt = sm.GetOrCreateShadowBaseAddress((uint64_t)addr);
//     if(memcmp(prevValue, (void*)val, accessLen) == 0){
//         // redundant
//         for(uint32_t index = 0 ; index < accessLen; index++){
//             // report in RedTable
//             AddToRedTable(MAKE_CONTEXT_PAIR(prevIP[index], curCtxtHandle), 1, pt);
//             // Update context
//             prevIP[index] = curCtxtHandle;
//         }
//     }else{
//         // Not redundant
//         for(uint32_t index = 0 ; index < accessLen; index++){
//             // Update context
//             prevIP[index] = curCtxtHandle;
//         }
//     }
//     // update prevValue
//     memcpy(prevValue,(void*)val,accessLen);
// }

template<uint32_t AccessLen, uint32_t ElemLen, bool isApprox, bool is_write>
void CheckNByteValue(int slot, opnd_t *opnd)
{
    void *drcontext = dr_get_current_drcontext();
    context_handle_t ctxt_hndl = drcctlib_get_context_handle(drcontext, slot);
    per_thread_t *pt = (per_thread_t *)drmgr_get_tls_field(drcontext, tls_idx);
    
    dr_mcontext_t mcontext;
    mcontext.size = sizeof(mcontext);
    mcontext.flags= DR_MC_ALL;
    DR_ASSERT(dr_get_mcontext(drcontext, &mcontext));
    assert(opnd_is_memory_reference(*opnd));
    assert(!is_write);
    app_pc addr = opnd_compute_address(*opnd, &mcontext);
    pt->bytesLoad += AccessLen;
    RedSpyAnalysis<AccessLen, isApprox>::CheckNByteValueAfterRead(addr, (uint8_t*)addr, ctxt_hndl, pt);
}

template<bool is_write>
void Instrument_impl(void *drcontext, instrlist_t *bb, instr_t *instr, int32_t slot, int size, int esize, bool is_float, int pos)
{
    per_thread_t *pt = (per_thread_t *)drmgr_get_tls_field(drcontext, tls_idx);
    opnd_t *opnd = new opnd_t(instr_get_src(instr, pos));
    pt->opnd_clones->push_back(opnd);

    if(!is_float) {
        switch(size) {
            case 1: dr_insert_clean_call(drcontext, bb, instr, (void*)CheckNByteValue<1,1,false,is_write>, false, 2, OPND_CREATE_CCT_INT(slot), OPND_CREATE_INTPTR(opnd));break;
            case 2: dr_insert_clean_call(drcontext, bb, instr, (void*)CheckNByteValue<2,2,false,is_write>, false, 2, OPND_CREATE_CCT_INT(slot), OPND_CREATE_INTPTR(opnd));break;
            case 4: dr_insert_clean_call(drcontext, bb, instr, (void*)CheckNByteValue<4,4,false,is_write>, false, 2, OPND_CREATE_CCT_INT(slot), OPND_CREATE_INTPTR(opnd));break;
            case 8: dr_insert_clean_call(drcontext, bb, instr, (void*)CheckNByteValue<8,8,false,is_write>, false, 2, OPND_CREATE_CCT_INT(slot), OPND_CREATE_INTPTR(opnd));break;
            case 16: dr_insert_clean_call(drcontext, bb, instr, (void*)CheckNByteValue<16,16,false,is_write>, false, 2, OPND_CREATE_CCT_INT(slot), OPND_CREATE_INTPTR(opnd));break;
            case 32: dr_insert_clean_call(drcontext, bb, instr, (void*)CheckNByteValue<32,32,false,is_write>, false, 2, OPND_CREATE_CCT_INT(slot), OPND_CREATE_INTPTR(opnd));break;
            case 64: dr_insert_clean_call(drcontext, bb, instr, (void*)CheckNByteValue<64,64,false,is_write>, false, 2, OPND_CREATE_CCT_INT(slot), OPND_CREATE_INTPTR(opnd));break;
            default: {
                printf("\nERROR: size for instruction is too large: %d!\n", size);
                printf("^^ Disassembled Instruction ^^^\n");
                disassemble(drcontext, instr_get_app_pc(instr), 1/*sdtout file desc*/);
                printf("^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^\n");
                fflush(stdout);
                assert(0 && "unexpected large size\n"); break;
            }
        }
    } else {
        switch(size) {
            case 2: break;
            case 4: dr_insert_clean_call(drcontext, bb, instr, (void*)CheckNByteValue<4,4,true,is_write>, false, 2, OPND_CREATE_CCT_INT(slot), OPND_CREATE_INTPTR(opnd));break;
            case 8: dr_insert_clean_call(drcontext, bb, instr, (void*)CheckNByteValue<8,8,true,is_write>, false, 2, OPND_CREATE_CCT_INT(slot), OPND_CREATE_INTPTR(opnd));break;
            // 128-bit, AVX, SSE
            case 16: {
                switch(esize) {
                    case 4: dr_insert_clean_call(drcontext, bb, instr, (void*)CheckNByteValue<16,4,true,is_write>, false, 2, OPND_CREATE_CCT_INT(slot), OPND_CREATE_INTPTR(opnd));break;
                    case 8: dr_insert_clean_call(drcontext, bb, instr, (void*)CheckNByteValue<16,8,true,is_write>, false, 2, OPND_CREATE_CCT_INT(slot), OPND_CREATE_INTPTR(opnd));break;
                    default: {
                        dr_fprintf(STDERR, "esize=%d\n", esize);
                        assert(0 && "handle large mem read with unexpected operand size\n");
                    }break;
                }
            }break;
            case 28: break;
            // 256-bit, AVX2
            case 32: {
                switch(esize) {
                    case 4: dr_insert_clean_call(drcontext, bb, instr, (void*)CheckNByteValue<32,4,true,is_write>, false, 2, OPND_CREATE_CCT_INT(slot), OPND_CREATE_INTPTR(opnd));break;
                    case 8: dr_insert_clean_call(drcontext, bb, instr, (void*)CheckNByteValue<32,8,true,is_write>, false, 2, OPND_CREATE_CCT_INT(slot), OPND_CREATE_INTPTR(opnd));break;
                    default: assert(0 && "handle large mem read with unexpected operand size\n"); break;
                }
            }break;
            // 512-bit, AVX512
            case 64: {
                switch(esize) {
                    case 4: dr_insert_clean_call(drcontext, bb, instr, (void*)CheckNByteValue<64,4,true,is_write>, false, 2, OPND_CREATE_CCT_INT(slot), OPND_CREATE_INTPTR(opnd));break;
                    case 8: dr_insert_clean_call(drcontext, bb, instr, (void*)CheckNByteValue<64,8,true,is_write>, false, 2, OPND_CREATE_CCT_INT(slot), OPND_CREATE_INTPTR(opnd));break;
                    default: assert(0 && "handle large mem read with unexpected operand size\n"); break;
                }
            }break;
            default: {
                printf("\nERROR: size for instruction is too large: %d!\n", size);
                printf("^^ Disassembled Instruction ^^^\n");
                disassemble(drcontext, instr_get_app_pc(instr), 1/*sdtout file desc*/);
                printf("^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^\n");
                fflush(stdout);
                assert(0 && "unexpected large size\n"); break;
            }
        }
    }
}

void
InstrumentInsCallback(void *drcontext, instr_instrument_msg_t *instrument_msg)
{
    instrlist_t *bb = instrument_msg->bb;
    instr_t *instr = instrument_msg->instr;
    int32_t slot = instrument_msg->slot;

    if(!instr_is_app(instr) || instr_is_ignorable(instr)) return;

    // to-do:vgather & scatter

    per_thread_t *pt = (per_thread_t *)drmgr_get_tls_field(drcontext, tls_idx);
    // instr_t* ins_clone = instr_clone(drcontext, instr);
    // pt->instr_clones->push_back(ins_clone);

    int num = instr_num_srcs(instr);
    for(int j = 0; j < num; j++) {
        opnd_t opnd = instr_get_src(instr, j);
        int size = opnd_size_in_bytes(opnd_get_size(opnd));
        if(size != 0 && opnd_is_memory_reference(opnd)) {
            bool is_float = opnd_is_floating(instr, opnd);
            int esize = is_float ? FloatOperandSizeTable(instr, opnd) : IntegerOperandSizeTable(instr, opnd);
            if(!esize) continue;
            Instrument_impl<false>(drcontext, bb, instr, slot, size, esize, is_float, j);
        }
    }
}

/**********************************************************************************/
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

    int threadId = pt->threadId;

    uint64_t grandTotalRedundantBytes = 0;
    dr_fprintf(pt->output_file, "*************** Dump Data from Thread %d ****************\n", threadId);
    
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
    
    __sync_fetch_and_add(&grandTotBytesRedLoad,grandTotalRedundantBytes);
    
    dr_fprintf(pt->output_file, "\n Total redundant bytes = %f %%\n", grandTotalRedundantBytes * 100.0 / pt->bytesLoad);
    
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
            dr_fprintf(pt->output_file, "\n---------------------Redundant load with---------------------------\n");
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

    int threadId = pt->threadId;
    
    uint64_t grandTotalRedundantBytes = 0;
    dr_fprintf(pt->output_file, "*************** Dump Data(delta=%.2f%%) from Thread %d ****************\n", delta*100,threadId);
    
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
                // grandTotalRedundantIns += 1;???
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
    
    __sync_fetch_and_add(&grandTotBytesApproxRedLoad,grandTotalRedundantBytes);
    
    dr_fprintf(pt->output_file, "\n Total redundant bytes = %f %%\n", grandTotalRedundantBytes * 100.0 / pt->bytesLoad);
    
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
            dr_fprintf(pt->output_file, "\n---------------------Redundant load with---------------------------\n");
            drcctlib_print_backtrace(pt->output_file, (*listIt).kill, true, true, MAX_DEPTH);
        }
        else {
            break;
        }
        cntxtNum++;
    }
}

static void HPCRunRedundancyPairs(per_thread_t *pt) {
    int threadId = pt->threadId;
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

static void HPCRunApproxRedundancyPairs(per_thread_t *pt) {
    int threadId = pt->threadId;

    vector<RedundacyData> tmpList;
    vector<RedundacyData>::iterator tmpIt;
    
    for (unordered_map<uint64_t, uint64_t>::iterator it = pt->ApproxRedMap->begin(); it != pt->ApproxRedMap->end(); ++it) {
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
            // HPCRunNode->metric_id = redload_approx_metric_id;
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
    dr_fprintf(gTraceFile, "Thread %d ThreadTotalBytesLoad = %u\n", pt->threadId, pt->bytesLoad);
    dr_fprintf(pt->output_file, "ThreadTotalBytesLoad = %u\n", pt->bytesLoad);
    __sync_fetch_and_add(&grandTotBytesLoad,pt->bytesLoad);

    // output the CCT for hpcviewer format
    // HPCRunRedundancyPairs(pt);
    // HPCRunApproxRedundancyPairs(pt);

    PrintRedundancyPairs(pt);
    PrintApproximationRedundancyPairs(pt);

    dr_close_file(pt->output_file);
    delete pt->opnd_clones;
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
}

static void
ClientThreadStart(void *drcontext)
{
    per_thread_t *pt = (per_thread_t *)dr_thread_alloc(drcontext, sizeof(per_thread_t));
    if (pt == NULL) {
        LOADSPY_EXIT_PROCESS("pt == NULL");
    }
    pt->bytesLoad = 0;
    pt->opnd_clones = new vector<opnd_t*>();
    pt->RedMap = new unordered_map<uint64_t, uint64_t>();
    pt->ApproxRedMap = new unordered_map<uint64_t, uint64_t>();
    drmgr_set_tls_field(drcontext, tls_idx, (void *)pt);
    ThreadOutputFileInit(pt);
}

static void
ClientExit(void)
{
    dr_fprintf(gTraceFile, "\n#Redundant Read:");
    dr_fprintf(gTraceFile, "\nTotalBytesLoad: %lu \n",grandTotBytesLoad);
    dr_fprintf(gTraceFile, "\nRedundantBytesLoad: %lu %.2f\n",grandTotBytesRedLoad, grandTotBytesRedLoad * 100.0/grandTotBytesLoad);
    dr_fprintf(gTraceFile, "\nApproxRedundantBytesLoad: %lu %.2f\n",grandTotBytesApproxRedLoad, grandTotBytesApproxRedLoad * 100.0/grandTotBytesLoad);

    delete mem_map;
    // delete ctxt_sm;
    // delete val_sm;
    dr_close_file(gTraceFile);
    // if(KnobTopN.get_value())
    //     topnStream.close();
    if (!dr_raw_tls_cfree(tls_offs, INSTRACE_TLS_COUNT)) {
        LOADSPY_EXIT_PROCESS(
            "ERROR: loadspy dr_raw_tls_calloc fail");
    }

    dr_mutex_destroy(gLock);
    if (!drmgr_unregister_thread_init_event(ClientThreadStart) ||
        !drmgr_unregister_thread_exit_event(ClientThreadEnd) ||
        !drmgr_unregister_tls_field(tls_idx)) {
        printf("ERROR: loadspy failed to unregister in ClientExit");
        fflush(stdout);
        exit(-1);
    }
    drcctlib_exit();
    drutil_exit();
    drreg_exit();
    drmgr_exit();
}

// Initialized the needed data structures before launching the target program
static void ClientInit(int argc, const char* argv[]) {
    /* Parse options */
    std::string parse_err;
    int last_index;
    if (!droption_parser_t::parse_argv(DROPTION_SCOPE_CLIENT, argc, argv, &parse_err, &last_index)) {
        dr_fprintf(STDERR, "Usage error: %s", parse_err.c_str());
        dr_abort();
    }

    mem_map = new ConcurrentShadowMemory<mem_node_t>();
    // ctxt_sm = new ConcurrentShadowMemory<uint32_t>();
    // val_sm = new ConcurrentShadowMemory<uint8_t>();
    // Create output file
    #ifdef ARM_CCTLIB
    char name[MAX_FILE_PATH] = "arm-";
#else
    char name[MAX_FILE_PATH] = "x86-";
#endif
    pid_t pid = getpid();

    // char hpc_name[MAX_FILE_PATH] = "";
    // sprintf(hpc_name + strlen(hpc_name), "%s-%d", dr_get_application_name(), pid);
    // hpcrun_format_init(hpc_name, false);
    // hpcrun_create_metric("RED_LOAD");
    // hpcrun_create_metric("RED_LOAD_APPROX");  

    gethostname(name + strlen(name), MAXIMUM_PATH - strlen(name));
    sprintf(name + strlen(name), "-%d-loadspy", pid);
    g_folder_name.assign(name, strlen(name));
    mkdir(g_folder_name.c_str(), S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH);

    dr_fprintf(STDOUT, "[LOADSPY INFO] Profiling result directory: %s\n", g_folder_name.c_str());

    sprintf(name+strlen(name), "/loadspy.log");
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
    dr_set_client_name("DynamoRIO Client 'loadspy'",
                       "http://dynamorio.org/issues");
    ClientInit(argc, argv);

    if (!drmgr_init()) {
        LOADSPY_EXIT_PROCESS("ERROR: zerospy unable to initialize drmgr");
    }
    drreg_options_t ops = { sizeof(ops), 4 /*max slots needed*/, false };
    if (drreg_init(&ops) != DRREG_SUCCESS) {
        DR_ASSERT_MSG(false, "ERROR: vtracer unable to initialize drreg");
    }
    if (!drutil_init()) {
        DR_ASSERT_MSG(false, "ERROR: vtracer unable to initialize drutil");
    }

    drmgr_priority_t thread_init_pri = { sizeof(thread_init_pri),
                                         "loadspy-thread-init", NULL, NULL,
                                         DRCCTLIB_THREAD_EVENT_PRI + 1 };
    drmgr_priority_t thread_exit_pri = { sizeof(thread_exit_pri),
                                         "loadspy-thread-exit", NULL, NULL,
                                         DRCCTLIB_THREAD_EVENT_PRI - 1 };

    if (   !drmgr_register_thread_init_event_ex(ClientThreadStart, &thread_init_pri) 
        || !drmgr_register_thread_exit_event_ex(ClientThreadEnd, &thread_exit_pri) ) {
        LOADSPY_EXIT_PROCESS("ERROR: loadspy unable to register events");
    }

    tls_idx = drmgr_register_tls_field();
    if (tls_idx == -1) {
        LOADSPY_EXIT_PROCESS("ERROR: loadspy drmgr_register_tls_field fail");
    }
    if (!dr_raw_tls_calloc(&tls_seg, &tls_offs, INSTRACE_TLS_COUNT, 0)) {
        LOADSPY_EXIT_PROCESS(
            "ERROR: loadspy dr_raw_tls_calloc fail");
    }
    gLock = dr_mutex_create();

    drcctlib_init_ex(LOADSPY_FILTER_MEM_ACCESS_INSTR, INVALID_FILE, InstrumentInsCallback, NULL, NULL, DRCCTLIB_DEFAULT);

    dr_register_exit_event(ClientExit);
}

#ifdef __cplusplus
}
#endif
