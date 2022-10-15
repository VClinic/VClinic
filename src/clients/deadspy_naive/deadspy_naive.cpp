// @COPYRIGHT@
// Licensed under MIT license.
// See LICENSE.TXT file in the project root for more information.
// ==============================================================

#define __STDC_FORMAT_MACROS
#include <map>
#include <tr1/unordered_map>
#include <list>
#include <inttypes.h>
#include <sys/syscall.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <exception>
#include <sys/time.h>
#include <sstream>
#include <fstream>
#include <cassert>

#include "dr_api.h"
#include "drmgr.h"
#include "drreg.h"
#include "drutil.h"
#include "drcctlib.h"
#include "utils.h"
// #include "shadow_memory.h"
#include "shadow_memory_lock.h"
#include "dr_tools.h"
using namespace std;
using namespace std::tr1;

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

#if !defined(MAP_ANONYMOUS) && defined(MAP_ANON)
#define MAP_ANONYMOUS MAP_ANON
#endif

// ensures CONTINUOUS_DEADINFO

#define CONTINUOUS_DEADINFO


#define MAX_CCT_PRINT_DEPTH (10)
#define MAX_FILE_PATH   (200)
#ifndef MAX_DEAD_CONTEXTS_TO_LOG
#define MAX_DEAD_CONTEXTS_TO_LOG   (1000)
#endif //MAX_DEAD_CONTEXTS_TO_LOG

// have R, W representative macros
#define READ_ACTION (0)
#define WRITE_ACTION (0xff)

#define ONE_BYTE_READ_ACTION (0)
#define TWO_BYTE_READ_ACTION (0)
#define FOUR_BYTE_READ_ACTION (0)
#define EIGHT_BYTE_READ_ACTION (0)

#define ONE_BYTE_WRITE_ACTION (0xff)
#define TWO_BYTE_WRITE_ACTION (0xffff)
#define FOUR_BYTE_WRITE_ACTION (0xffffffff)
#define EIGHT_BYTE_WRITE_ACTION (0xffffffffffffffff)

#define TLS_SLOT(tls_base, offs) (void **)((byte *)(tls_base) + (offs))
#define BUF_PTR(tls_base, offs) *(byte **)TLS_SLOT(tls_base, offs)

#define MINSERT instrlist_meta_preinsert

// use manual inlined updates
#define RESERVE_AFLAGS(dc, bb, ins) assert(drreg_reserve_aflags (dc, bb, ins)==DRREG_SUCCESS)
#define UNRESERVE_AFLAGS(dc, bb, ins) assert(drreg_unreserve_aflags (dc, bb, ins)==DRREG_SUCCESS)

#define RESERVE_REG(dc, bb, instr, vec, reg) do {\
    if (drreg_reserve_register(dc, bb, instr, vec, &reg) != DRREG_SUCCESS) { \
        VTRACER_EXIT_PROCESS("ERROR @ %s:%d: drreg_reserve_register != DRREG_SUCCESS", __FILE__, __LINE__); \
    } } while(0)
#define UNRESERVE_REG(dc, bb, instr, reg) do { \
    if (drreg_unreserve_register(dc, bb, instr, reg) != DRREG_SUCCESS) { \
        VTRACER_EXIT_PROCESS("ERROR @ %s:%d: drreg_unreserve_register != DRREG_SUCCESS", __FILE__, __LINE__); \
    } } while(0)

// All fwd declarations

struct DeadInfo;
file_t gTraceFile;
std::fstream topnStream;


struct MergedDeadInfo;
struct DeadInfoForPresentation;

// Each memory byte M has a shadow byte to hold its current state STATE(M)
// To maintain the context for a write operation, we store an additional pointer sized variable CONTEXT(M) (actually uint32_t) in the shadow memory for each memory byte M.
typedef struct _mem_node_t {
    uint32_t ctxt;
    uint8_t state;
} mem_node_t;

ConcurrentShadowMemory<mem_node_t> *mem_map;

typedef struct _per_thread_t {
    unordered_map<uint64_t, uint64_t> *DeadMap;
    vector<instr_t*> *instr_clones;
    vector<opnd_t*> *opnd_clones;
    uint64_t totalBytesWrite;
    file_t output_file;
    int32_t threadId;
} per_thread_t;


struct DeadInfo {
    void* firstIP;
    void* secondIP;
    uint64_t count;
};

// key for accessing TLS storage in the threads. initialized once in main()
static int tls_idx;
static string g_folder_name;

enum {
    INSTRACE_TLS_OFFS_BUF_PTR,
    INSTRACE_TLS_OFFS_INTLOG_PTR,
    INSTRACE_TLS_OFFS_FPLOG_PTR,
    INSTRACE_TLS_COUNT, /* total number of TLS slots allocated */
};

static reg_id_t tls_seg;
static uint tls_offs;

struct{
    char dummy1[128];
    string topNLogFileName;
    char dummy2[128];
} DeadSpyGlobals;

// Client Options
#include "droption.h"

static droption_t<bool> KnobTopN
(DROPTION_SCOPE_CLIENT, "d", 0, 0, 1000, "Max Top Contexts to log",
 "How Many Top Contexts To Log.");

 #define DEADSPY_EXIT_PROCESS(format, args...)                                           \
    DRCCTLIB_CLIENT_EXIT_PROCESS_TEMPLATE("deadspy", format, \
                                          ##args)

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

bool
DEADSPY_FILTER_MEM_ACCESS_INSTR(instr_t *instr) {
    if(!(instr_reads_memory(instr) || instr_writes_memory(instr)) || instr_is_prefetch(instr)) return false;
#ifdef X86
    if(instr_is_xsave(instr)) {
        return false;
    }

    switch(instr_get_opcode(instr)) {
        case OP_xrstor32:
        case OP_xrstor64:
            return false;
        default: return true;
    }
#endif
    return true;
}

inline bool DeadInfoComparer(const DeadInfo& first, const DeadInfo& second);

#ifdef GATHER_STATS
FILE* statsFile;
#endif //end GATHER_STATS

// global metrics
// static void *gLock;
uint64_t grandTotBytesWrites = 0;
uint64_t gTotalDead = 0;
#ifdef MULTI_THREADED
uint64_t gTotalMTDead = 0;
#endif // end MULTI_THREADED

// make 64bit hash from 2 32bit deltas from
// remove lower 3 bits so that when we need more than 4 GB HASH still continues to work

#if 0
#define CONTEXT_HASH_128BITS_TO_64BITS(curCtxt, oldCtxt, hashVar)  \
{\
uint64_t key = (uint64_t) (((void**)oldCtxt) - gPreAllocatedContextBuffer); \
hashVar = key << 32;\
key = (uint64_t) (((void**)curCtxt) - gPreAllocatedContextBuffer); \
hashVar |= key;\
}

#else

#define CONTEXT_HASH_128BITS_TO_64BITS(curCtxt, oldCtxt, hashVar)  \
{\
uint64_t key = (uint64_t) (oldCtxt); \
hashVar = key << 32;\
key = (uint64_t) (curCtxt); \
hashVar |= key;\
}

#endif


#define OLD_CTXT (*lastIP)
// defined in cct lib: #define CUR_CTXT_INDEX (&(tData->gCurrentIPNode[tData->curSlotNo]))

#define DECLARE_HASHVAR(name) uint64_t name=0

void REPORT_DEAD(per_thread_t *pt, uint32_t curCtxt, uint32_t lastCtxt, uint64_t hashVar, int size) {
    CONTEXT_HASH_128BITS_TO_64BITS(curCtxt, lastCtxt,hashVar);
    unordered_map<uint64_t, uint64_t>::iterator DeadMapIt;
    if ( (DeadMapIt = (*pt->DeadMap).find(hashVar))  == (*pt->DeadMap).end()) {
        (*pt->DeadMap).insert(std::pair<uint64_t, uint64_t>(hashVar,size));
    } else {
        (DeadMapIt->second) += size;
    }
}

#define REPORT_IF_DEAD(pt, mask, curCtxt, lastCtxt, hashVar) do {if (state & (mask)){ \
REPORT_DEAD(pt, curCtxt, lastCtxt,hashVar, 1);\
}}while(0)

// Analysis routines to update the shadow memory for different size READs and WRITEs


void Record1ByteMemRead(void* addr) {
    mem_node_t *ptr = mem_map->GetShadowAddress((size_t)addr);
    if(ptr) {
        ptr->state = ONE_BYTE_READ_ACTION;
    }
    // else err!
}

void Record1ByteMemWrite(per_thread_t *pt, void* addr, const uint32_t curCtxtHandle) {
    mem_node_t *ptr = mem_map->GetOrCreateShadowAddress((size_t)addr);
    if(ptr->state == ONE_BYTE_WRITE_ACTION) {
        uint64_t myhash = 0;
        REPORT_DEAD(pt, curCtxtHandle, ptr->ctxt, myhash, 1);
    } else {
        ptr->state = ONE_BYTE_WRITE_ACTION;
    }
    // dead context was replaced by killing context
    ptr->ctxt = curCtxtHandle;
}

void RecordNByteMemRead(void* addr, int size) {
    for(int i = 0; i < size; i++) {
        Record1ByteMemRead(((char*)addr) + i);
    }
} 

void RecordNByteMemWrite(per_thread_t *pt, void* addr, const uint32_t curCtxtHandle, int size) {
    for(int i = 0; i < size; i++) {
        Record1ByteMemWrite(pt, ((char*)addr) + i, curCtxtHandle);
    }
}

template<uint32_t AccessLen, uint32_t ElemLen, bool is_write>
void CheckNByteValue(int slot, opnd_t* opnd)
{
    void *drcontext = dr_get_current_drcontext();
    context_handle_t ctxt_hndl = drcctlib_get_context_handle(drcontext, slot);
    per_thread_t *pt = (per_thread_t *)drmgr_get_tls_field(drcontext, tls_idx);

    dr_mcontext_t mcontext;
    mcontext.size = sizeof(mcontext);
    mcontext.flags= DR_MC_ALL;
    DR_ASSERT(dr_get_mcontext(drcontext, &mcontext));

    app_pc addr = opnd_compute_address(*opnd, &mcontext);
    
    if(is_write) {
        pt->totalBytesWrite += ElemLen;
        RecordNByteMemWrite(pt, addr, ctxt_hndl, ElemLen); 
    } else {
        RecordNByteMemRead(addr, ElemLen);
    }
}

template<bool is_write>
void Instrument_impl(void *drcontext, instrlist_t *bb, instr_t *instr, int32_t slot, int size, int esize, bool is_float, int pos)
{
    per_thread_t *pt = (per_thread_t *)drmgr_get_tls_field(drcontext, tls_idx);
    opnd_t *opnd;
    if(!is_write) opnd = new opnd_t(instr_get_src(instr, pos));
    else opnd = new opnd_t(instr_get_dst(instr, pos));
    pt->opnd_clones->push_back(opnd);

    if(!is_float) {
        switch(size) {
            case 1: dr_insert_clean_call(drcontext, bb, instr, (void*)CheckNByteValue<1,1,is_write>, false, 2, OPND_CREATE_CCT_INT(slot), OPND_CREATE_INTPTR(opnd));break;
            case 2: dr_insert_clean_call(drcontext, bb, instr, (void*)CheckNByteValue<2,2,is_write>, false, 2, OPND_CREATE_CCT_INT(slot), OPND_CREATE_INTPTR(opnd));break;
            case 4: dr_insert_clean_call(drcontext, bb, instr, (void*)CheckNByteValue<4,4,is_write>, false, 2, OPND_CREATE_CCT_INT(slot), OPND_CREATE_INTPTR(opnd));break;
            case 8: dr_insert_clean_call(drcontext, bb, instr, (void*)CheckNByteValue<8,8,is_write>, false, 2, OPND_CREATE_CCT_INT(slot), OPND_CREATE_INTPTR(opnd));break;
            case 16: dr_insert_clean_call(drcontext, bb, instr, (void*)CheckNByteValue<16,16,is_write>, false, 2, OPND_CREATE_CCT_INT(slot), OPND_CREATE_INTPTR(opnd));break;
            case 32: dr_insert_clean_call(drcontext, bb, instr, (void*)CheckNByteValue<32,32,is_write>, false, 2, OPND_CREATE_CCT_INT(slot), OPND_CREATE_INTPTR(opnd));break;
            case 64: dr_insert_clean_call(drcontext, bb, instr, (void*)CheckNByteValue<64,64,is_write>, false, 2, OPND_CREATE_CCT_INT(slot), OPND_CREATE_INTPTR(opnd));break;
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
            case 2: dr_insert_clean_call(drcontext, bb, instr, (void*)CheckNByteValue<2,2,is_write>, false, 2, OPND_CREATE_CCT_INT(slot), OPND_CREATE_INTPTR(opnd));break;
            case 4: dr_insert_clean_call(drcontext, bb, instr, (void*)CheckNByteValue<4,4,is_write>, false, 2, OPND_CREATE_CCT_INT(slot), OPND_CREATE_INTPTR(opnd));break;
            case 8: dr_insert_clean_call(drcontext, bb, instr, (void*)CheckNByteValue<8,8,is_write>, false, 2, OPND_CREATE_CCT_INT(slot), OPND_CREATE_INTPTR(opnd));break;
            // 128-bit, AVX, SSE
            case 16: {
                switch(esize) {
                    case 4: dr_insert_clean_call(drcontext, bb, instr, (void*)CheckNByteValue<16,4,is_write>, false, 2, OPND_CREATE_CCT_INT(slot), OPND_CREATE_INTPTR(opnd));break;
                    case 8: dr_insert_clean_call(drcontext, bb, instr, (void*)CheckNByteValue<16,8,is_write>, false, 2, OPND_CREATE_CCT_INT(slot), OPND_CREATE_INTPTR(opnd));break;
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
                    case 4: dr_insert_clean_call(drcontext, bb, instr, (void*)CheckNByteValue<32,4,is_write>, false, 2, OPND_CREATE_CCT_INT(slot), OPND_CREATE_INTPTR(opnd));break;
                    case 8: dr_insert_clean_call(drcontext, bb, instr, (void*)CheckNByteValue<32,8,is_write>, false, 2, OPND_CREATE_CCT_INT(slot), OPND_CREATE_INTPTR(opnd));break;
                    default: assert(0 && "handle large mem read with unexpected operand size\n"); break;
                }
            }break;
            // 512-bit, AVX512
            case 64: {
                switch(esize) {
                    case 4: dr_insert_clean_call(drcontext, bb, instr, (void*)CheckNByteValue<64,4,is_write>, false, 2, OPND_CREATE_CCT_INT(slot), OPND_CREATE_INTPTR(opnd));break;
                    case 8: dr_insert_clean_call(drcontext, bb, instr, (void*)CheckNByteValue<64,8,is_write>, false, 2, OPND_CREATE_CCT_INT(slot), OPND_CREATE_INTPTR(opnd));break;
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
    
    num = instr_num_dsts(instr);
    for(int j = 0; j < num; j++) {
        opnd_t opnd = instr_get_dst(instr, j);
        int size = opnd_size_in_bytes(opnd_get_size(opnd));
        if(size != 0 && opnd_is_memory_reference(opnd)) {
            bool is_float = opnd_is_floating(instr, opnd);
            int esize = is_float ? FloatOperandSizeTable(instr, opnd) : IntegerOperandSizeTable(instr, opnd);
            if(!esize) continue;
            Instrument_impl<true>(drcontext, bb, instr, slot, size, esize, is_float, j);
        }
    }
}

// When we make System calls we need to update the shadow regions with the effect of the system call
// TODO: handle other system calls. Currently only SYS_write is handled.

// VOID SyscallEntry(THREADID threadIndex, CONTEXT* ctxt, SYSCALL_STANDARD std,
//                   VOID* v) {
//     ADDRINT number = PIN_GetSyscallNumber(ctxt, std);

//     switch(number) {
//     case SYS_write: {
//         char* bufStart = (char*) PIN_GetSyscallArgument(ctxt, std, 1);
//         char* bufEnd = bufStart
//                        + (size_t) PIN_GetSyscallArgument(ctxt, std, 2);
// #ifdef DEBUG
//         printf("\n WRITE %p - %p\n", bufStart, bufEnd);
// #endif //end DEBUG

//         while(bufStart < bufEnd)
//             Record1ByteMemRead(bufStart++);
//     }
//     break;

//     default:
//         break;//NOP
//     }
// }



struct MergedDeadInfo {
    uint32_t context1;
    uint32_t context2;

    bool operator==(const MergedDeadInfo&   x) const {
        if(this->context1 == x.context1 && this->context2 == x.context2)
            return true;

        return false;
    }

    bool operator<(const MergedDeadInfo& x) const {
        if((this->context1 < x.context1) ||
                (this->context1 == x.context1 && this->context2 < x.context2))
            return true;

        return false;
    }
};

struct DeadInfoForPresentation {
    const MergedDeadInfo* pMergedDeadInfo;
    uint64_t count;
};

inline bool MergedDeadInfoComparer(const DeadInfoForPresentation& first, const DeadInfoForPresentation&  second) {
    return first.count > second.count ? true : false;
}


// Prints the complete calling context including the line nunbers and the context's contribution, given a DeadInfo
inline void PrintIPAndCallingContexts(per_thread_t *pt, const DeadInfoForPresentation& di) {
    dr_fprintf(pt->output_file, "\n%" PRIu64 " = %e", di.count, di.count * 100.0 / pt->totalBytesWrite);
    dr_fprintf(pt->output_file, "\n-------------------------------------------------------\n");
    drcctlib_print_backtrace(pt->output_file, di.pMergedDeadInfo->context1, true, true, MAX_CCT_PRINT_DEPTH);
    dr_fprintf(pt->output_file, "\n***********************\n");
    drcctlib_print_backtrace(pt->output_file, di.pMergedDeadInfo->context2, true, true, MAX_CCT_PRINT_DEPTH);
    dr_fprintf(pt->output_file, "\n-------------------------------------------------------\n");
}

static void
ClientThreadEnd(void *drcontext)
{
    per_thread_t *pt = (per_thread_t *)drmgr_get_tls_field(drcontext, tls_idx);
    dr_fprintf(gTraceFile, "Thread %d ThreadTotalBytesWrite = %u\n", pt->threadId, pt->totalBytesWrite);
    dr_fprintf(pt->output_file, "ThreadTotalBytesWrite = %u\n", pt->totalBytesWrite);
    __sync_fetch_and_add(&grandTotBytesWrites,pt->totalBytesWrite);

    unordered_map<uint64_t, uint64_t>::iterator mapIt = (*pt->DeadMap).begin();
    map<MergedDeadInfo, uint64_t> mergedDeadInfoMap;

    for(; mapIt != (*pt->DeadMap).end(); mapIt++) {
        MergedDeadInfo tmpMergedDeadInfo;
        uint64_t hash = mapIt->first;
        uint32_t ctxt1 = (hash >> 32);
        uint32_t ctxt2 = (hash & 0xffffffff);
        tmpMergedDeadInfo.context1 = ctxt1;
        tmpMergedDeadInfo.context2 = ctxt2;
        map<MergedDeadInfo, uint64_t>::iterator tmpIt;

        if((tmpIt = mergedDeadInfoMap.find(tmpMergedDeadInfo)) == mergedDeadInfoMap.end()) {
            mergedDeadInfoMap[tmpMergedDeadInfo] = mapIt->second;
        } else {
            tmpIt->second  += mapIt->second;
        }
    }

    // clear dead map now
    delete pt->DeadMap;
    map<MergedDeadInfo, uint64_t>::iterator it = mergedDeadInfoMap.begin();
    list<DeadInfoForPresentation> deadList;

    for(; it != mergedDeadInfoMap.end(); it ++) {
        DeadInfoForPresentation deadInfoForPresentation;
        deadInfoForPresentation.pMergedDeadInfo = &(it->first);
        deadInfoForPresentation.count = it->second;
        deadList.push_back(deadInfoForPresentation);
    }

    deadList.sort(MergedDeadInfoComparer);
    //present and delete all
    list<DeadInfoForPresentation>::iterator dipIter = deadList.begin();
    dr_mutex_lock(gLock);
    uint64_t deads = 0;

    for(; dipIter != deadList.end(); dipIter++) {
        // Print just first MAX_DEAD_CONTEXTS_TO_LOG contexts
        if(deads < MAX_DEAD_CONTEXTS_TO_LOG) {
            try {
                PrintIPAndCallingContexts(pt, *dipIter);
            } catch(...) {
                dr_fprintf(gTraceFile, "\nexcept");
            }
        } else {
            // print only dead count
#ifdef PRINT_ALL_CTXT
            dr_fprintf(gTraceFile, "\nCTXT_DEAD_CNT:%lu = %e", dipIter->count, dipIter->count * 100.0 / grandTotBytesWrites);
#endif                //end PRINT_ALL_CTXT
        }

        gTotalDead += dipIter->count ;
        deads++;
    }
#ifdef TESTING_BYTES
    PrintInstructionBreakdown();
#endif //end TESTING_BYTES
#ifdef GATHER_STATS
    PrintStats(deadList, deads);
#endif //end GATHER_STATS
    mergedDeadInfoMap.clear();
    deadList.clear();
    dr_mutex_unlock(gLock);
    
    delete pt->instr_clones;
    delete pt->opnd_clones;
    dr_thread_free(drcontext, pt, sizeof(per_thread_t));
}

// Initialized the needed data structures before launching the target program
static void 
ClientInit(int argc, const char* argv[]) {
    /* Parse options */
    std::string parse_err;
    int last_index;
    if (!droption_parser_t::parse_argv(DROPTION_SCOPE_CLIENT, argc, argv, &parse_err, &last_index)) {
        dr_fprintf(STDERR, "Usage error: %s", parse_err.c_str());
        dr_abort();
    }

    mem_map = new ConcurrentShadowMemory<mem_node_t>();

    // Create output file
#ifdef ARM_CCTLIB
    char name[MAX_FILE_PATH] = "arm-";
#else
    char name[MAX_FILE_PATH] = "x86-";
#endif

    gethostname(name + strlen(name), MAX_FILE_PATH - strlen(name));
    pid_t pid = getpid();
    sprintf(name + strlen(name), "-%d-deadspy", pid);
    g_folder_name.assign(name, strlen(name));
    mkdir(g_folder_name.c_str(), S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH);

    dr_fprintf(STDOUT, "[DEADSPY INFO] Profiling result directory: %s\n", g_folder_name.c_str());

    sprintf(name+strlen(name), "/deadspy.log");
    gTraceFile = dr_open_file(name, DR_FILE_WRITE_OVERWRITE | DR_FILE_ALLOW_LARGE);
    DR_ASSERT(gTraceFile != INVALID_FILE);
    // print the arguments passed
    dr_fprintf(gTraceFile, "\n");

    for(int i = 0 ; i < argc; i++) {
        dr_fprintf(gTraceFile, "%s ", argv[i]);
    }

    dr_fprintf(gTraceFile, "\n");
    if(KnobTopN.get_value()) {
       topnStream.open (string(name) + ".topn", std::fstream::out | std::fstream::trunc);
       topnStream<<"\n";
       for(int i = 0 ; i < argc; i++) {
        topnStream << argv[i] << " ";
       }
       topnStream<<"\n";
    }

#ifdef GATHER_STATS
    string statFileName(name);
    statFileName += ".stats";
    statsFile = fopen(statFileName.c_str() , "w");
    fprintf(statsFile, "\n");

    for(int i = 0 ; i < argc; i++) {
        fprintf(statsFile, "%s ", argv[i]);
    }

    fprintf(statsFile, "\n");
#endif //end   GATHER_STATS
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
        DEADSPY_EXIT_PROCESS("pt == NULL");
    }
    pt->totalBytesWrite = 0;
    pt->instr_clones = new vector<instr_t*>();
    pt->opnd_clones = new vector<opnd_t*>();
    pt->DeadMap = new unordered_map<uint64_t, uint64_t>();
    drmgr_set_tls_field(drcontext, tls_idx, (void *)pt);
    ThreadOutputFileInit(pt);
}

static void
ClientExit(void)
{
    dr_fprintf(gTraceFile, "\n#deads");
    dr_fprintf(gTraceFile, "\nGrandTotalWrites = %" PRIu64, grandTotBytesWrites);
    dr_fprintf(gTraceFile, "\nGrandTotalDead = %" PRIu64 " = %e%%", gTotalDead, gTotalDead * 100.0 / grandTotBytesWrites);
#ifdef MULTI_THREADED
    dr_fprintf(gTraceFile, "\nGrandTotalMTDead = %" PRIu64 " = %e%%", gTotalMTDead, gTotalMTDead * 100.0 / grandTotBytesWrites);
#endif // end MULTI_THREADED
    dr_fprintf(gTraceFile, "\n#eof");
    delete mem_map;
    dr_close_file(gTraceFile);
    // if(KnobTopN.get_value())
    //     topnStream.close();
    if (!dr_raw_tls_cfree(tls_offs, INSTRACE_TLS_COUNT)) {
        DEADSPY_EXIT_PROCESS(
            "ERROR: deadspy dr_raw_tls_calloc fail");
    }

    dr_mutex_destroy(gLock);
    if (!drmgr_unregister_thread_init_event(ClientThreadStart) ||
        !drmgr_unregister_thread_exit_event(ClientThreadEnd) ||
        !drmgr_unregister_tls_field(tls_idx)) {
        printf("ERROR: deadspy failed to unregister in ClientExit");
        fflush(stdout);
        exit(-1);
    }

    drcctlib_exit();
    drutil_exit();
    drreg_exit();
    drmgr_exit();
}

#ifdef __cplusplus
extern "C" {
#endif

DR_EXPORT void
dr_client_main(client_id_t id, int argc, const char *argv[])
{
    dr_set_client_name("DynamoRIO Client 'deadspy'",
                       "http://dynamorio.org/issues");
    ClientInit(argc, argv);

    if (!drmgr_init()) {
        DEADSPY_EXIT_PROCESS("ERROR: deadspy unable to initialize drmgr");
    }

    drreg_options_t ops = { sizeof(ops), 4 /*max slots needed*/, false };
    if (drreg_init(&ops) != DRREG_SUCCESS) {
        DR_ASSERT_MSG(false, "ERROR: vtracer unable to initialize drreg");
    }
    if (!drutil_init()) {
        DR_ASSERT_MSG(false, "ERROR: vtracer unable to initialize drutil");
    }

    drmgr_priority_t thread_init_pri = { sizeof(thread_init_pri),
                                         "deadspy-thread-init", NULL, NULL,
                                         DRCCTLIB_THREAD_EVENT_PRI + 1 };
    drmgr_priority_t thread_exit_pri = { sizeof(thread_exit_pri),
                                         "deadspy-thread-exit", NULL, NULL,
                                         DRCCTLIB_THREAD_EVENT_PRI - 1 };

    if (   !drmgr_register_thread_init_event_ex(ClientThreadStart, &thread_init_pri) 
        || !drmgr_register_thread_exit_event_ex(ClientThreadEnd, &thread_exit_pri) ) {
        DEADSPY_EXIT_PROCESS("ERROR: deadspy unable to register events");
    }

    tls_idx = drmgr_register_tls_field();
    if (tls_idx == -1) {
        DEADSPY_EXIT_PROCESS("ERROR: deadspy drmgr_register_tls_field fail");
    }
    if (!dr_raw_tls_calloc(&tls_seg, &tls_offs, INSTRACE_TLS_COUNT, 0)) {
        DEADSPY_EXIT_PROCESS(
            "ERROR: deadspy dr_raw_tls_calloc fail");
    }
    gLock = dr_mutex_create();

    drcctlib_init_ex(DEADSPY_FILTER_MEM_ACCESS_INSTR, INVALID_FILE, InstrumentInsCallback, NULL, NULL, DRCCTLIB_DEFAULT);

    dr_register_exit_event(ClientExit);
}

#ifdef __cplusplus
}
#endif

