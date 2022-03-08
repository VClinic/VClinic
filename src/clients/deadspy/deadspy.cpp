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

#include "dr_api.h"
#include "drmgr.h"
#include "drreg.h"
#include "drutil.h"
#include "drcctlib.h"
// #include "shadow_memory.h"
#include "shadow_memory_lock.h"
#include "dr_tools.h"
#include "vprofile.h"
using namespace std;
using namespace std::tr1;

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
vtrace_t* vtrace;
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

#define WINDOW_ENABLE 1000000
#define WINDOW_DISABLE 100000000

int window_enable;
int window_disable;

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

static droption_t<bool> KnobTopN
(DROPTION_SCOPE_CLIENT, "d", 0, 0, 1000, "Max Top Contexts to log",
 "How Many Top Contexts To Log.");

 #define DEADSPY_EXIT_PROCESS(format, args...)                                           \
    DRCCTLIB_CLIENT_EXIT_PROCESS_TEMPLATE("deadspy", format, \
                                          ##args)

bool 
VPROFILE_FILTER_OPND(opnd_t opnd, vprofile_src_t opmask) {
    // uint32_t user_mask1 = (ANY_DATA_TYPE | MEMORY | READ | BEFORE);
    // uint32_t user_mask2 = (ANY_DATA_TYPE | MEMORY | WRITE | AFTER);
    // return ((user_mask1 & opmask) == opmask) || ((user_mask2 & opmask) == opmask);
    uint32_t user_mask = (ANY_DATA_TYPE | MEMORY | READ | WRITE | BEFORE);
    return ((user_mask & opmask) == opmask);
}

bool
DEADSPY_FILTER_MEM_ACCESS_INSTR(instr_t *instr) {
    if(!VPROFILE_FILTER_MEM_ACCESS_INSTR(instr) || instr_is_prefetch(instr)) return false;
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

// cache_t_ctxt_no_info
void trace_update_cb(val_info_t *info) {
    // no need to esize?
    int size = info->size;
    uint64_t addr = info->addr;
    int32_t cct = info->ctxt_hndl;
    uint32_t type = info->type;
    bool is_write = ((type & WRITE) == WRITE);

    per_thread_t* pt = (per_thread_t *)drmgr_get_tls_field(dr_get_current_drcontext(), tls_idx);

    if(is_write) {
        pt->totalBytesWrite += size;
    }

    switch(size) {
        case 1: (is_write) ? Record1ByteMemWrite(pt, (void*)addr, cct) : Record1ByteMemRead((void*)addr); break;
        default: (is_write) ? RecordNByteMemWrite(pt, (void*)addr, cct, size) : RecordNByteMemRead((void*)addr, size); break;
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
    if (op_enable_sampling.get_value()) {
        dr_fprintf(pt->output_file, "[DEADSPY INFO] Sampling Enabled\n");
    } else {
        dr_fprintf(pt->output_file, "[DEADSPY INFO] Sampling Disabled\n");
    }
}

static void
ClientThreadStart(void *drcontext)
{
    per_thread_t *pt = (per_thread_t *)dr_thread_alloc(drcontext, sizeof(per_thread_t));
    if (pt == NULL) {
        DEADSPY_EXIT_PROCESS("pt == NULL");
    }
    pt->totalBytesWrite = 0;
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
    vprofile_unregister_trace(vtrace);
    vprofile_exit();
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

    if(!vprofile_init(DEADSPY_FILTER_MEM_ACCESS_INSTR, NULL, NULL, NULL,
                     VPROFILE_COLLECT_CCT)) {
        DEADSPY_EXIT_PROCESS("ERROR: deadspy unable to initialize vprofile");
    }

    if(op_enable_sampling.get_value()) {
        vtracer_enable_sampling(op_window_enable.get_value(), op_window.get_value());
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

    dr_register_exit_event(ClientExit);

    // maybe we can set trace before write!
    uint32_t trace_flag = (VPROFILE_TRACE_CCT_ADDR | VPROFILE_TRACE_STRICTLY_ORDERED | VPROFILE_TRACE_BEFORE_WRITE);

    vtrace = vprofile_allocate_trace(trace_flag);

    // We only interest in memory loads
    uint32_t opnd_mask = (ANY_DATA_TYPE | MEMORY | READ | BEFORE | WRITE /*| AFTER*/);

    // Tracing Buffer
    vprofile_register_trace_cb(vtrace, VPROFILE_FILTER_OPND, opnd_mask, ANY, trace_update_cb);
}

#ifdef __cplusplus
}
#endif

