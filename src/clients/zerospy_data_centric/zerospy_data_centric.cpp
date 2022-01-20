#include <unordered_map>
#include <map>
#include <vector>
#include <list>
#include <string>
#include <sys/stat.h>
#include <assert.h>
#include <algorithm>

#ifdef DEBUG_CHECK
#define IF_DEBUG(stat) stat
#else
#define IF_DEBUG(stat)
#endif

// #define ZEROSPY_DEBUG
// #define DEBUG_CHECK
#define _WERROR

#ifdef TIMING
#include <time.h>
#include <math.h>
uint64_t get_miliseconds() {
    struct timespec spec;
    clock_gettime(CLOCK_REALTIME, &spec);
    return spec.tv_sec*1000 + round(spec.tv_nsec / 1.0e6); // Convert nanoseconds to milliseconds
}
#endif

static void *gLock;

#include "dr_api.h"
#include "drmgr.h"
#include "drreg.h"
#include "drutil.h"
// enable data centric with addr info
#define DRCCTLIB_USE_ADDR
#include "drcctlib.h"
#include "utils.h"
#include "bitvec.h"
#include "vprofile.h"
#include "../cl_include/rapidjson/document.h"
#include "../cl_include/rapidjson/filewritestream.h"
#include "../cl_include/rapidjson/prettywriter.h"

#define WINDOW_ENABLE 1000000
#define WINDOW_DISABLE 100000000
// #define WINDOW_CLEAN 10

int window_enable;
int window_disable;

// Client Options
#include "droption.h"
static droption_t<bool> op_enable_sampling
(DROPTION_SCOPE_CLIENT, "enable_sampling", 0, 0, 64, "Enable Bursty Sampling",
 "Enable bursty sampling for lower overhead with less profiling accuracy.");

static droption_t<bool> op_help
(DROPTION_SCOPE_CLIENT, "help", 0, 0, 64, "Show this help",
 "Show this help.");

static droption_t<int> op_window
(DROPTION_SCOPE_CLIENT, "window", WINDOW_DISABLE, 0, INT32_MAX, "Window size configuration of sampling",
 "Window size of sampling. Only available when sampling is enabled.");

static droption_t<int> op_window_enable
(DROPTION_SCOPE_CLIENT, "window_enable", WINDOW_ENABLE, 0, INT32_MAX, "Window enabled size configuration of sampling",
 "Window enabled size of sampling. Only available when sampling is enabled.");

using namespace std;

#define ZEROSPY_PRINTF(format, args...) \
    DRCCTLIB_PRINTF_TEMPLATE("zerospy", format, ##args)
#define ZEROSPY_EXIT_PROCESS(format, args...)                                           \
    DRCCTLIB_CLIENT_EXIT_PROCESS_TEMPLATE("zerospy", format, \
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

// We only interest in memory loads
bool 
VPROFILE_FILTER_OPND(opnd_t opnd, vprofile_src_t opmask) {
    uint32_t user_mask = ANY_DATA_TYPE | MEMORY | READ | BEFORE;
    return ((user_mask & opmask) == opmask);
}

bool
zerospy_filter_read_mem_access_instr(instr_t *instr)
{
    return instr_reads_memory(instr) && !instr_is_prefetch(instr);
}

#define ZEROSPY_FILTER_READ_MEM_ACCESS_INSTR zerospy_filter_read_mem_access_instr

vtrace_t* vtrace;
static string g_folder_name;
static int tls_idx;

template<int size>
struct cache_t {
    void* addr;
    int8_t val[size];
};

struct RedLogs{
    uint64_t red;  // how many byte zero
    bitvec_t redmap; // bitmap logging if a byte is redundant
    bitvec_t accmap; // bitmap logging if a byte is accessed
};

typedef unordered_map<uint64_t, RedLogs> RedLogSizeMap;
typedef unordered_map<uint64_t, RedLogSizeMap> RedLogMap;

struct FPRedLogs{
    uint64_t red;  // how many byte zero
    bitvec_t redmap; // bitmap logging if a byte is redundant
    bitvec_t accmap; // bitmap logging if a byte is accessed
    uint8_t typesz;
};

typedef unordered_map<uint64_t, FPRedLogs> FPRedLogSizeMap;
typedef unordered_map<uint64_t, FPRedLogSizeMap> FPRedLogMap;

#define MINSERT instrlist_meta_preinsert
#define MAKE_OBJID(a, b) (((uint64_t)(a)<<32) | (b))
#define DECODE_TYPE(a) (((uint64_t)(a)&(0xffffffffffffffff))>>32)
#define DECODE_NAME(b) ((uint64_t)(b)&(0x00000000ffffffff))


#define MAKE_APPROX_OBJID(a, b, ts) (((uint64_t)(a)<<32) | ((b)<<8) | (ts))
#define DECODE_APPROX_TYPE(a) (((uint64_t)(a)&(0xffffffffffffffff))>>32)
#define DECODE_APPROX_NAME(b) (((uint64_t)(b)&(0x00000000ffffff00))>>8)
#define DECODE_APPROX_TYPESZ(c) ((uint64_t)(c)&(0x00000000000000ff))

#define MAKE_CNTXT(a, b, c) (((uint64_t)(a)<<32) | ((uint64_t)(b)<<16) | (uint64_t)(c))
#define DECODE_CNTXT(a) (static_cast<ContextHandle_t>((((a)&(0xffffffffffffffff))>>32)))
#define DECODE_ACCLN(b) (((uint64_t)(b)&(0x00000000ffff0000))>>16)
#define DECODE_TYPSZ(c)  ((uint64_t)(c)&(0x000000000000ffff))

#define MAX_OBJS_TO_LOG 100

#define delta 0.01

#define CACHE_LINE_SIZE (64)
#ifndef PAGE_SIZE
#define PAGE_SIZE (4*1024)
#endif
#define MAX_REDUNDANT_CONTEXTS_TO_LOG (1000)
// maximum cct depth to print
#define MAX_DEPTH 10

enum {
    INSTRACE_TLS_OFFS_BUF_PTR,
    INSTRACE_TLS_OFFS_INTLOG_PTR,
    INSTRACE_TLS_OFFS_FPLOG_PTR,
    INSTRACE_TLS_COUNT, /* total number of TLS slots allocated */
};

static reg_id_t tls_seg;
static uint tls_offs;

// 1M
#define MAX_CLONE_INS 1048576
typedef struct _per_thread_t {
    RedLogMap *INTRedMap;
    FPRedLogMap *FPRedMap;
    file_t output_file;
    int32_t threadId;
    vector<instr_t*> *instr_clones;
} per_thread_t;

file_t gFile;
FILE* gJson;
rapidjson::Document gDoc;
rapidjson::Document::AllocatorType &jsonAllocator = gDoc.GetAllocator();
rapidjson::Value metricOverview(rapidjson::kObjectType);
rapidjson::Value totalIntegerRedundantBytes(rapidjson::kObjectType);
rapidjson::Value totalFloatRedundantBytes(rapidjson::kObjectType);
std::map<int32_t, rapidjson::Value> threadDetailedMetricsMap;
#ifndef _WERROR
file_t fwarn;
bool warned=false;
#endif
#ifdef ZEROSPY_DEBUG
file_t gDebug;
#endif

// global metrics
uint64_t grandTotBytesLoad = 0;
uint64_t grandTotBytesRedLoad = 0;
uint64_t grandTotBytesApproxRedLoad = 0;

/*******************************************************************************************/
// TODO: May be further optimized by combining size and data hndl to avoid one more mapping
static inline void AddToRedTable(uint64_t addr, data_handle_t data, uint16_t value, uint16_t total, uint32_t redmap, per_thread_t *pt) __attribute__((always_inline,flatten));
static inline void AddToRedTable(uint64_t addr, data_handle_t data, uint16_t value, uint16_t total, uint32_t redmap, per_thread_t *pt) {
    assert(addr<=(uint64_t)data.end_addr);
    size_t offset = addr-(uint64_t)data.beg_addr;
    size_t size = (uint64_t)data.end_addr - (uint64_t)data.beg_addr;
    uint64_t key = MAKE_OBJID(data.object_type,data.sym_name);
    RedLogMap::iterator it2 = pt->INTRedMap->find(key);
    RedLogSizeMap::iterator it;
    // IF_DEBUG(dr_fprintf(
    //         STDOUT,
    //         "AddToRedTable 1: offset=%ld, total=%d, size=%ld\n", offset, total, size));
    if ( it2  == pt->INTRedMap->end() || (it = it2->second.find(size)) == it2->second.end()) {
        RedLogs log;
        log.red = value;
#ifdef DEBUG_CHECK
        if(offset+total>size) {
            printf("AddToRedTable 1: offset=%ld, total=%d, size=%ld\n", offset, total, size);
            if(data.object_type == DYNAMIC_OBJECT) {
                printf("\n^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^ Dynamic Object: ^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^\n");
                drcctlib_print_backtrace(STDOUT, data.sym_name, true, true, MAX_DEPTH);
            } else {
                printf("\n^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^ Static Object: %s ^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^\n", drcctlib_get_str_from_strpool((uint32_t)data.sym_name));
            }
        }
#endif
        bitvec_alloc(&log.redmap, size);
        bitvec_and(&log.redmap, redmap, offset, total);
        bitvec_alloc(&log.accmap, size);
        bitvec_and(&log.accmap, 0, offset, total);
        (*pt->INTRedMap)[key][size] = log;
    } else {
        assert(it->second.redmap.size==it->second.accmap.size);
        assert(size == it->second.redmap.size);
#ifdef DEBUG_CHECK
        if(offset+total>size) {
            printf("AddToRedTable 2: offset=%ld, total=%d, size=%ld\n", offset, total, size);
            if(data.object_type == DYNAMIC_OBJECT) {
                printf("\n^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^ Dynamic Object: ^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^\n");
                drcctlib_print_backtrace(STDOUT, data.sym_name, true, true, MAX_DEPTH);
            } else {
                printf("\n^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^ Static Object: %s ^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^\n", drcctlib_get_str_from_strpool((uint32_t)data.sym_name));
            }
        }
#endif
        it->second.red += value;
        bitvec_and(&(it->second.redmap), redmap, offset, total);
        bitvec_and(&(it->second.accmap), 0, offset, total);
    }
}

static inline void AddToApproximateRedTable(uint64_t addr, data_handle_t data, uint16_t value, uint16_t total, uint64_t redmap, uint32_t typesz, per_thread_t *pt) __attribute__((always_inline,flatten));
static inline void AddToApproximateRedTable(uint64_t addr, data_handle_t data, uint16_t value, uint16_t total, uint64_t redmap, uint32_t typesz, per_thread_t *pt) {
    // printf("ADDR=%lx, beg_addr=%lx, end_addr=%lx, typesz=%d, index=%ld, size=%ld\n", addr, (uint64_t)data.beg_addr, (uint64_t)data.end_addr, typesz, addr-(uint64_t)data.beg_addr, (uint64_t)data.end_addr - (uint64_t)data.beg_addr);
    assert(addr<=(uint64_t)data.end_addr);
    size_t offset = addr-(uint64_t)data.beg_addr;
    uint64_t key = MAKE_APPROX_OBJID(data.object_type,data.sym_name, typesz);
    FPRedLogMap::iterator it2 = pt->FPRedMap->find(key);
    FPRedLogSizeMap::iterator it;
    // the data size may not aligned with typesz, so use upper bound as the bitvec size
    // Note: not aligned case : struct/class with floating and int.
    size_t size = (uint64_t)data.end_addr - (uint64_t)data.beg_addr;
    if(value > total) {
        dr_fprintf(STDERR, "** Warning AddToApproximateTable : value %d, total %d **\n", value, total);
        assert(0 && "** BUG #0 Detected. Existing **");
    }
    if ( it2  == pt->FPRedMap->end() || (it = it2->second.find(size)) == it2->second.end()) {
        FPRedLogs log;
        log.red = value;
        log.typesz = typesz;
#ifdef DEBUG_CHECK
        if(offset+total>size) {
            printf("AddToApproxRedTable 1: offset=%ld, total=%d, size=%ld\n", offset, total, size);
            if(data.object_type == DYNAMIC_OBJECT) {
                printf("\n^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^ Dynamic Object: ^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^\n");
                drcctlib_print_backtrace(STDOUT, data.sym_name, true, true, MAX_DEPTH);
            } else {
                printf("\n^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^ Static Object: %s ^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^\n", drcctlib_get_str_from_strpool((uint32_t)data.sym_name));
            }
        }
#endif
        bitvec_alloc(&log.redmap, size);
        bitvec_and(&log.redmap, redmap, offset, total);
        bitvec_alloc(&log.accmap, size);
        bitvec_and(&log.accmap, 0, offset, total);
        (*pt->FPRedMap)[key][size] = log;
    } else {
        assert(it->second.redmap.size==it->second.accmap.size);
        assert(size == it->second.redmap.size);
#ifdef DEBUG_CHECK
        if(it->second.typesz != typesz) {
            printf("it->second.typesz=%d typesz=%d\n", it->second.typesz, typesz);
        }
        if(offset+total>size) {
            printf("AddToApproxRedTable 1: offset=%ld, total=%d, size=%ld\n", offset, total, size);
            if(data.object_type == DYNAMIC_OBJECT) {
                printf("\n^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^ Dynamic Object: ^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^\n");
                drcctlib_print_backtrace(STDOUT, data.sym_name, true, true, MAX_DEPTH);
            } else {
                printf("\n^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^ Static Object: %s ^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^\n", drcctlib_get_str_from_strpool((uint32_t)data.sym_name));
            }
        }
#endif
        assert(it->second.typesz == typesz);
        it->second.red += value;
        bitvec_and(&(it->second.redmap), redmap, offset, total);
        bitvec_and(&(it->second.accmap), 0, offset, total);
    }
}
/*******************************************************************************************/
// single floating point zero byte counter
// 32-bit float: |sign|exp|mantissa| = | 1 | 8 | 23 |
// the redmap of single floating point takes up 5 bits (1 bit sign, 1 bit exp, 3 bit mantissa)
#define SP_MAP_SIZE 5
inline __attribute__((always_inline)) uint64_t count_zero_bytemap_fp(void * addr) {
    register uint32_t xx = *((uint32_t*)addr);
    // reduce by bits until byte level
    // | 0 | x0x0 x0x0 | 0x0 x0x0 x0x0 x0x0 x0x0 x0x0 |
    xx = xx | ((xx>>1)&0xffbfffff);
    // | x | 00xx 00xx | 0xx 00xx 00xx 00xx 00xx 00xx |
    xx = xx | ((xx>>2)&0xffdfffff);
    // | x | 0000 xxxx | 000 xxxx 0000 xxxx 0000 xxxx |
    xx = xx | ((xx>>4)&0xfff7ffff);
    // now xx is byte level reduced, check if it is zero and mask the unused bits
    xx = (~xx) & 0x80810101;
    // narrowing
    xx = xx | (xx>>7) | (xx>>14) | (xx>>20) | (xx>>27);
    xx = xx & 0x1f;
    return xx;
}
inline __attribute__((always_inline)) bool hasRedundancy_fp(void * addr) {
    register uint32_t xx = *((uint32_t*)addr);
    return (xx & 0x007f0000)==0;
}
/*******************************************************************************************/
// double floating point zero byte counter
// 64-bit float: |sign|exp|mantissa| = | 1 | 11 | 52 |
// the redmap of single floating point takes up 10 bits (1 bit sign, 2 bit exp, 7 bit mantissa)
#define DP_MAP_SIZE 10
inline __attribute__((always_inline)) uint64_t count_zero_bytemap_dp(void * addr) {
    register uint64_t xx = (static_cast<uint64_t*>(addr))[0];
    // reduce by bits until byte level
    // | 0 | 0x0 x0x0 x0x0 | x0x0 x0x0_x0x0 x0x0_x0x0 x0x0_x0x0 x0x0_x0x0 x0x0_x0x0 x0x0_x0x0 |
    xx = xx | ((xx>>1)&(~0x4008000000000000LL));
    // | x | 0xx 00xx 00xx | 00xx 00xx_00xx 00xx_00xx 00xx_00xx 00xx_00xx 00xx_00xx 00xx_00xx |
    xx = xx | ((xx>>2)&(~0x200c000000000000LL));
    // | x | xxx 0000 xxxx | xxxx 0000_xxxx 0000_xxxx 0000_xxxx 0000_xxxx 0000_xxxx 0000_xxxx |
    xx = xx | ((xx>>4)&(~0x100f000000000000LL));
    // now xx is byte level reduced, check if it is zero and mask the unused bits
    xx = (~xx) & 0x9011010101010101LL;
    // narrowing
    register uint64_t m = xx & 0x1010101010101LL;
    m = m | (m>>7);
    m = m | (m>>14);
    m = m | (m>>28);
    m = m & 0x7f;
    xx = xx | (xx>>9) | (xx>>7);
    xx = (xx >> 45) & 0x380;
    xx = m | xx;
    return xx;
}
inline __attribute__((always_inline)) bool hasRedundancy_dp(void * addr) {
    register uint64_t xx = *((uint64_t*)addr);
    return (xx & 0x000f000000000000LL)==0;
}
/***************************************************************************************/
/*********************** floating point full redundancy functions **********************/
/***************************************************************************************/

#if __BYTE_ORDER == __BIG_ENDIAN
typedef union {
  float f;
  struct {
    uint32_t sign : 1;
    uint32_t exponent : 8;
    uint32_t mantisa : 23;
  } parts;
  struct {
    uint32_t sign : 1;
    uint32_t value : 31;
  } vars;
} float_cast;

typedef union {
  double f;
  struct {
    uint64_t sign : 1;
    uint64_t exponent : 11;
    uint64_t mantisa : 52;
  } parts;
  struct {
    uint64_t sign : 1;
    uint64_t value : 63;
  } vars;
} double_cast;
#elif __BYTE_ORDER == __LITTLE_ENDIAN
typedef union {
  float f;
  struct {
    uint32_t mantisa : 23;
    uint32_t exponent : 8;
    uint32_t sign : 1;
  } parts;
  struct {
    uint32_t value : 31;
    uint32_t sign : 1;
  } vars;
} float_cast;

typedef union {
  double f;
  struct {
    uint64_t mantisa : 52;
    uint64_t exponent : 11;
    uint64_t sign : 1;
  } parts;
  struct {
    uint64_t value : 63;
    uint64_t sign : 1;
  } vars;
} double_cast;
#else
    #error Known Byte Order
#endif

template<int start, int end, int incr>
struct UnrolledConjunctionApprox{
    // if the mantisa is 0, the value of the double/float var must be 0
    static __attribute__((always_inline)) uint64_t BodyZeros(uint8_t* addr){
        if(incr==4)
            return ((*(reinterpret_cast<float_cast*>(&addr[start]))).vars.value==0) + (UnrolledConjunctionApprox<start+incr,end,incr>::BodyZeros(addr));
        else if(incr==8)
            return ((*(reinterpret_cast<double_cast*>(&addr[start]))).vars.value==0) + (UnrolledConjunctionApprox<start+incr,end,incr>::BodyZeros(addr));
        return 0;
    }
    static __attribute__((always_inline)) uint64_t BodyRedMap(uint8_t* addr){
        if(incr==4)
            return count_zero_bytemap_fp((void*)(addr+start)) | (UnrolledConjunctionApprox<start+incr,end,incr>::BodyRedMap(addr)<<SP_MAP_SIZE);
        else if(incr==8)
            return count_zero_bytemap_dp((void*)(addr+start)) | (UnrolledConjunctionApprox<start+incr,end,incr>::BodyRedMap(addr)<<DP_MAP_SIZE);
        else
            assert(0 && "Not Supportted floating size! now only support for FP32 or FP64.");
        return 0;
    }
    static __attribute__((always_inline)) uint64_t BodyHasRedundancy(uint8_t* addr){
        if(incr==4)
            return hasRedundancy_fp((void*)(addr+start)) || (UnrolledConjunctionApprox<start+incr,end,incr>::BodyHasRedundancy(addr));
        else if(incr==8)
            return hasRedundancy_dp((void*)(addr+start)) || (UnrolledConjunctionApprox<start+incr,end,incr>::BodyHasRedundancy(addr));
        else
            assert(0 && "Not Supportted floating size! now only support for FP32 or FP64.");
        return 0;
    }
};

template<int end,  int incr>
struct UnrolledConjunctionApprox<end , end , incr>{
    static __attribute__((always_inline)) uint64_t BodyZeros(uint8_t* addr){
        return 0;
    }
    static __attribute__((always_inline)) uint64_t BodyRedMap(uint8_t* addr){
        return 0;
    }
    static __attribute__((always_inline)) uint64_t BodyHasRedundancy(uint8_t* addr){
        return 0;
    }
};

/****************************************************************************************/
inline __attribute__((always_inline)) uint64_t count_zero_bytemap_int8(uint8_t * addr) {
    register uint8_t xx = *((uint8_t*)addr);
    // reduce by bits until byte level
    xx = xx | (xx>>1) | (xx>>2) | (xx>>3) | (xx>>4) | (xx>>5) | (xx>>6) | (xx>>7);
    // now xx is byte level reduced, check if it is zero and mask the unused bits
    xx = (~xx) & 0x1;
    return xx;
}
inline __attribute__((always_inline)) uint64_t count_zero_bytemap_int16(uint8_t * addr) {
    register uint16_t xx = *((uint16_t*)addr);
    // reduce by bits until byte level
    xx = xx | (xx>>1) | (xx>>2) | (xx>>3) | (xx>>4) | (xx>>5) | (xx>>6) | (xx>>7);
    // now xx is byte level reduced, check if it is zero and mask the unused bits
    xx = (~xx) & 0x101;
    // narrowing
    xx = xx | (xx>>7);
    xx = xx & 0x3;
    return xx;
}
inline __attribute__((always_inline)) uint64_t count_zero_bytemap_int32(uint8_t * addr) {
    register uint32_t xx = *((uint32_t*)addr);
    // reduce by bits until byte level
    xx = xx | (xx>>1) | (xx>>2) | (xx>>3) | (xx>>4) | (xx>>5) | (xx>>6) | (xx>>7);
    // now xx is byte level reduced, check if it is zero and mask the unused bits
    xx = (~xx) & 0x1010101;
    // narrowing
    xx = xx | (xx>>7);
    xx = xx | (xx>>14);
    xx = xx & 0xf;
    return xx;
}
inline __attribute__((always_inline)) uint64_t count_zero_bytemap_int64(uint8_t * addr) {
    register uint64_t xx = *((uint64_t*)addr);
    // reduce by bits until byte level
    xx = xx | (xx>>1) | (xx>>2) | (xx>>3) | (xx>>4) | (xx>>5) | (xx>>6) | (xx>>7);
    // now xx is byte level reduced, check if it is zero and mask the unused bits
    xx = (~xx) & 0x101010101010101LL;
    // narrowing
    xx = xx | (xx>>7);
    xx = xx | (xx>>14);
    xx = xx | (xx>>28);
    xx = xx & 0xff;
    return xx;
}

#ifdef USE_SIMD
uint8_t mask[64] __attribute__((aligned(64))) = {   0x1, 0x1, 0x1, 0x1, 0x1, 0x1, 0x1, 0x1, 
                                                    0x1, 0x1, 0x1, 0x1, 0x1, 0x1, 0x1, 0x1,
                                                    0x1, 0x1, 0x1, 0x1, 0x1, 0x1, 0x1, 0x1, 
                                                    0x1, 0x1, 0x1, 0x1, 0x1, 0x1, 0x1, 0x1,
                                                    0x1, 0x1, 0x1, 0x1, 0x1, 0x1, 0x1, 0x1, 
                                                    0x1, 0x1, 0x1, 0x1, 0x1, 0x1, 0x1, 0x1,
                                                    0x1, 0x1, 0x1, 0x1, 0x1, 0x1, 0x1, 0x1, 
                                                    0x1, 0x1, 0x1, 0x1, 0x1, 0x1, 0x1, 0x1 };

uint8_t mask_shuf[32] __attribute__((aligned(64))) = { 
                                         0x00, 0x08, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 
                                         0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
                                         0x00, 0x08, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 
                                         0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff };

inline __attribute__((always_inline)) uint64_t count_zero_bytemap_int128(uint8_t * addr) {
    uint64_t xx;
    __m128i mmx, tmp;
    // load 128-bit val
    mmx = _mm_loadu_si128((__m128i*)addr);
    // Merge all bits within a byte in parallel
    // 0x
    tmp = _mm_srli_epi64 (mmx, 1);
    mmx = _mm_or_si128(mmx, tmp);
    // 00xx
    tmp = _mm_srli_epi64 (mmx, 2);
    mmx = _mm_or_si128(mmx, tmp);
    // 0000xxxx
    tmp = _mm_srli_epi64 (mmx, 4);
    mmx = _mm_or_si128(mmx, tmp);
    // x = (~x) & broadcast(0x01)
    // the mask is already aligned
    tmp = _mm_load_si128((__m128i*)mask);
    mmx = _mm_andnot_si128(mmx, tmp);
    /* Now SIMD reg_val contains the collected bitmap for each byte, we now
       narrow them into each 64-bit element in this packed SIMD register*/
    // x = (x>>7 | x)
    tmp = _mm_srli_epi64 (mmx, 7);
    mmx = _mm_or_si128(mmx, tmp);
    // x = (x>>14 | x)
    tmp = _mm_srli_epi64 (mmx, 14);
    mmx = _mm_or_si128(mmx, tmp);
    // x = (x>>28 | x)
    tmp = _mm_srli_epi64 (mmx, 28);
    mmx = _mm_or_si128(mmx, tmp);
    /* After narrowed them by 64-bit elementwise merging, the lowest byte of
      each element contains the collected redmap, so we can now narrow them
      by select (bytewise permutation).*/
    // x = permuteb(x, {0,8,...})
    // shuffle: [...clear...] [72:64] [8:0]
    // We directly load the simd mask from memory
    tmp = _mm_load_si128((__m128i*)mask_shuf);
    mmx = _mm_shuffle_epi8(mmx, tmp);
    // now store the lower 16-bits into target (INOUT) register
    union U128I {
        __m128i  v;
        uint16_t e[8];
    } cast;
    cast.v = mmx;
    xx = (uint64_t)cast.e[0];
    return xx;
}

inline __attribute__((always_inline)) uint64_t count_zero_bytemap_int256(uint8_t * addr) {
    uint64_t xx;
    __m256i mmx, tmp;
    // Load data from memory via SIMD instruction
    mmx = _mm256_loadu_si256((__m256i*)addr);
    // Merge all bits within a byte in parallel
    // 0x
    tmp = _mm256_srli_epi64(mmx, 1);
    mmx = _mm256_or_si256(mmx, tmp);
    // 00xx
    tmp = _mm256_srli_epi64(mmx, 2);
    mmx = _mm256_or_si256(mmx, tmp);
    // 0000xxxx
    tmp = _mm256_srli_epi64(mmx, 4);
    mmx = _mm256_or_si256(mmx, tmp);
    // x = (~x) & broadcast(0x01)
    tmp = _mm256_load_si256((__m256i*)mask);
    mmx = _mm256_andnot_si256(mmx, tmp);
    /* Now SIMD reg_val contains the collected bitmap for each byte, we now
       narrow them into each 64-bit element in this packed SIMD register*/
    // x = (x>>7 | x)
    tmp = _mm256_srli_epi64 (mmx, 7);
    mmx = _mm256_or_si256(mmx, tmp);
    // x = (x>>14 | x)
    tmp = _mm256_srli_epi64 (mmx, 14);
    mmx = _mm256_or_si256(mmx, tmp);
    // x = (x>>28 | x)
    tmp = _mm256_srli_epi64 (mmx, 28);
    mmx = _mm256_or_si256(mmx, tmp);
    /* After narrowed them by 64-bit elementwise merging, the lowest byte of
      each element contains the collected redmap, so we can now narrow them
      by select (bytewise permutation).*/
    // x = permuteb(x, {0,8,...})
    // shuffle: [...clear...] [200:192] [136:128] | [...clear...] [72:64] [8:0]
    // We directly load the simd mask from memory
    tmp = _mm256_load_si256((__m256i*)mask_shuf);
    mmx = _mm256_shuffle_epi8(mmx, tmp);
    // As shuffle is performed per lane, so we need further merging
    // 1. permutation to merge two lanes into the first lane: 8 = (10) (00) -> [...] [192:128] [64:0]
    mmx = _mm256_permute4x64_epi64(mmx, 8);
    // 2. shuffle again for narrowing into lower 64-bit value, here we reuse the previously loaded mask in simd scratch register
    mmx = _mm256_shuffle_epi8(mmx, tmp);
    // now store the lower 32-bits into target (INOUT) register
    union U256I {
        __m256i  v;
        uint32_t e[8];
    } cast;
    cast.v = mmx;
    xx = (uint64_t)cast.e[0];
    return xx;
}
#endif

static const unsigned char BitCountTable4[] __attribute__ ((aligned(64))) = {
    0, 0, 1, 2
};

static const unsigned char BitCountTable8[] __attribute__ ((aligned(64))) = {
    0, 0, 0, 0, 1, 1, 2, 3
};

static const unsigned char BitCountTable16[] __attribute__ ((aligned(64))) = {
    0, 0, 0, 0, 0, 0, 0, 0,
    1, 1, 1, 1, 2, 2, 3, 4
};

static const unsigned char BitCountTable256[] __attribute__ ((aligned(64))) = {
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 
    2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 
    2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 
    3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 
    4, 4, 4, 4, 4, 4, 4, 4, 5, 5, 5, 5, 6, 6, 7, 8
};

// accessLen & eleSize are size in bits
template<uint32_t AccessLen, uint32_t EleSize>
struct RedMapString {
    static __attribute__((always_inline)) string getIntRedMapString(uint64_t redmap) {
        // static_assert(AccessLen % EleSize == 0);
        string buff = 
            RedMapString<AccessLen-EleSize, EleSize>::getIntRedMapString(redmap>>(AccessLen-EleSize)) + 
            " , " + RedMapString<EleSize, EleSize>::getIntRedMapString(redmap);
        return buff;
    }
};

template<uint32_t AccessLen>
struct RedMapString <AccessLen, AccessLen> {
    static __attribute__((always_inline)) string getIntRedMapString(uint64_t redmap) {
        string buff = "";
        buff += ((redmap>>(AccessLen-1))&0x1) ? "00 " : "XX ";
        buff += RedMapString<AccessLen-1, AccessLen-1>::getIntRedMapString(redmap>>1);
        return buff;
    }
};

template<>
struct RedMapString <1, 1> {
    static __attribute__((always_inline)) string getIntRedMapString(uint64_t redmap) {
        return string((redmap&0x1) ? "00" : "XX");
    }
};

template<uint32_t n_exp, uint32_t n_man>
inline __attribute__((always_inline)) string __getFpRedMapString(uint64_t redmap) {
    string buff = "";
    const uint32_t signPos = n_exp + n_man;
    buff += RedMapString<1,1>::getIntRedMapString(redmap>>signPos) + " | ";
    buff += RedMapString<n_exp,n_exp>::getIntRedMapString(redmap>>n_man) + " | ";
    buff += RedMapString<n_man,n_man>::getIntRedMapString(redmap);
    return buff;
}

template<uint32_t n_exp, uint32_t n_man>
string getFpRedMapString(uint64_t redmap, uint64_t accessLen) {
    string buff = "";
    uint64_t newAccessLen = accessLen - (n_exp + n_man + 1);
    if(newAccessLen==0) {
        return __getFpRedMapString<n_exp,n_man>(redmap);
    } else {
        return getFpRedMapString<n_exp,n_man>(redmap>>newAccessLen, newAccessLen) + " , " + __getFpRedMapString<n_exp,n_man>(redmap);
    }
    return buff;
}

#define getFpRedMapString_SP(redmap, num) getFpRedMapString<1,3>(redmap, num*5)
#define getFpRedMapString_DP(redmap, num) getFpRedMapString<2,7>(redmap, num*10)

template<int start, int end, int incr>
struct UnrolledConjunction{
    // if the mantisa is 0, the value of the double/float var must be 0
    static __attribute__((always_inline)) uint64_t BodyRedNum(uint64_t rmap){
        // static_assert(start < end);
        if(incr==1)
            return ((start==0) ? (rmap&0x1) : ((rmap>>start)&0x1)) + (UnrolledConjunction<start+incr,end,incr>::BodyRedNum(rmap));
        else if(incr==2)
            return ((start==0) ? BitCountTable8[rmap&0x3] : BitCountTable8[(rmap>>start)&0x3]) + (UnrolledConjunction<start+incr,end,incr>::BodyRedNum(rmap));
        else if(incr==4)
            return ((start==0) ? BitCountTable16[rmap&0xf] : BitCountTable16[(rmap>>start)&0xf]) + (UnrolledConjunction<start+incr,end,incr>::BodyRedNum(rmap));
        else if(incr==8)
            return ((start==0) ? BitCountTable256[rmap&0xff] : BitCountTable256[(rmap>>start)&0xff]) + (UnrolledConjunction<start+incr,end,incr>::BodyRedNum(rmap));
        return 0;
    }
    static __attribute__((always_inline)) uint64_t BodyRedMap(uint8_t* addr){
        // static_assert(start < end);
        if(incr==1)
            return count_zero_bytemap_int8(addr+start) | (UnrolledConjunction<start+incr,end,incr>::BodyRedMap(addr)<<1);
        else if(incr==2)
            return count_zero_bytemap_int16(addr+start) | (UnrolledConjunction<start+incr,end,incr>::BodyRedMap(addr)<<2);
        else if(incr==4)
            return count_zero_bytemap_int32(addr+start) | (UnrolledConjunction<start+incr,end,incr>::BodyRedMap(addr)<<4);
        else if(incr==8)
            return count_zero_bytemap_int64(addr+start) | (UnrolledConjunction<start+incr,end,incr>::BodyRedMap(addr)<<8);
        else
            assert(0 && "Not Supportted integer size! now only support for INT8, INT16, INT32 or INT64.");
        return 0;
    }
    static __attribute__((always_inline)) bool BodyHasRedundancy(uint8_t* addr){
        if(incr==1)
            return (addr[start]==0) || (UnrolledConjunction<start+incr,end,incr>::BodyHasRedundancy(addr));
        else if(incr==2)
            return (((*((uint16_t*)(&addr[start])))&0xff00)==0) || (UnrolledConjunction<start+incr,end,incr>::BodyRedMap(addr));
        else if(incr==4)
            return (((*((uint32_t*)(&addr[start])))&0xff000000)==0) || (UnrolledConjunction<start+incr,end,incr>::BodyRedMap(addr));
        else if(incr==8)
            return (((*((uint64_t*)(&addr[start])))&0xff00000000000000LL)==0) || (UnrolledConjunction<start+incr,end,incr>::BodyRedMap(addr));
        else
            assert(0 && "Not Supportted integer size! now only support for INT8, INT16, INT32 or INT64.");
        return 0;
    }
};

template<int end,  int incr>
struct UnrolledConjunction<end , end , incr>{
    static __attribute__((always_inline)) uint64_t BodyRedNum(uint64_t rmap){
        return 0;
    }
    static __attribute__((always_inline)) uint64_t BodyRedMap(uint8_t* addr){
        return 0;
    }
    static __attribute__((always_inline)) uint64_t BodyHasRedundancy(uint8_t* addr){
        return 0;
    }
};
/*******************************************************************************************/
template<int accessLen, int elementSize>
inline __attribute__((always_inline))
void CheckAndInsertIntPage_impl(void* drcontext, void* addr, void* pval, per_thread_t *pt) {
    // update info
    uint8_t* bytes = reinterpret_cast<uint8_t*>(pval);
    data_handle_t data_hndl =
                drcctlib_get_data_hndl_ignore_stack_data(drcontext, (app_pc)addr);
    if(data_hndl.object_type!=DYNAMIC_OBJECT && data_hndl.object_type!=STATIC_OBJECT) {
        return ;
    }
    if(bytes[accessLen-1]!=0) {
        // the log have already been clear to 0, so we do nothing here and quick return.
        AddToRedTable((uint64_t) addr, data_hndl, 0, accessLen, 0, pt);
        return ;
    }
    uint64_t redByteMap;
    switch(accessLen) {
        case 1:
            redByteMap = count_zero_bytemap_int8(bytes);
            break;
        case 2:
            redByteMap = count_zero_bytemap_int16(bytes);
            break;
        case 4:
            redByteMap = count_zero_bytemap_int32(bytes);
            break;
        case 8:
            redByteMap = count_zero_bytemap_int64(bytes);
            break;
        case 16:
#ifdef USE_SIMD
            redByteMap = count_zero_bytemap_int128(bytes);
#else
            redByteMap = count_zero_bytemap_int64(bytes) |
                        (count_zero_bytemap_int64(bytes+8)<<8);
#endif
            break;
        case 32:
#ifdef USE_SIMD
            redByteMap = count_zero_bytemap_int256(bytes);
#else
            redByteMap = count_zero_bytemap_int64(bytes) |
                        (count_zero_bytemap_int64(bytes+8)<<8) |
                        (count_zero_bytemap_int64(bytes+16)<<16) |
                        (count_zero_bytemap_int64(bytes+24)<<24);
#endif
            break;
        default:
            assert(0 && "UNKNOWN ACCESSLEN!\n");
    }
#ifdef USE_SSE
    if(elementSize==1) {
        uint64_t redZero = _mm_popcnt_u64(redByteMap);
    } else {
        // accessLen == elementSize
        uint64_t redByteMap_2 = (~redByteMap) & ((1LL<<accessLen)-1);
        uint64_t redZero = _lzcnt_u64(redByteMap_2) - (64-accessLen);
    }
    AddToRedTable((uint64_t)addr, data_hndl, redZero, accessLen, redByteMap, pt);
#else
    uint64_t redZero = UnrolledConjunction<0, accessLen, elementSize>::BodyRedNum(redByteMap);
    AddToRedTable((uint64_t)addr, data_hndl, redZero, accessLen, redByteMap, pt);
#endif
}

template<int size, int esize, bool is_float>
void trace_update_cb(val_info_t *info) {
    per_thread_t* pt = (per_thread_t *)drmgr_get_tls_field(dr_get_current_drcontext(), tls_idx);
    void* drcontext = dr_get_current_drcontext();
    void* val = info->val;
    uint64_t addr = info->addr;
    if(is_float) {
        data_handle_t data_hndl =
                drcctlib_get_data_hndl_ignore_stack_data(drcontext, (app_pc)addr);
        if(data_hndl.object_type!=DYNAMIC_OBJECT && data_hndl.object_type!=STATIC_OBJECT) {
            return;
	    }
        uint8_t* bytes = reinterpret_cast<uint8_t*>(val);
        bool hasRedundancy = UnrolledConjunctionApprox<0,size,esize>::BodyHasRedundancy(bytes);
        if(hasRedundancy) {
            uint64_t map = UnrolledConjunctionApprox<0,size,esize>::BodyRedMap(bytes);
            uint32_t zeros = UnrolledConjunctionApprox<0,size,esize>::BodyZeros(bytes);        
            AddToApproximateRedTable(addr, data_hndl, zeros, size/esize, map, esize, pt);
        } else {
            AddToApproximateRedTable(addr,data_hndl, 0, size/esize, 0, esize, pt);
        }
    } else {
        CheckAndInsertIntPage_impl<size, esize>(drcontext, (void*)addr, val, pt);
    }
}

void debug_output_line() {
    dr_fprintf(STDOUT, "--------------\n"); fflush(stdout);
}

void debug_output(void* val) {
    dr_fprintf(STDOUT, "loaded val=%p\n", val); fflush(stdout);
}

#ifdef X86
#define HANDLE_VGATHER(T, ACCESS_LEN, ELEMENT_LEN, IS_APPROX) do {\
if(op_enable_sampling.get_value()) { \
dr_insert_clean_call(drcontext, bb, ins, (void *)ZeroSpyAnalysis<T, (ACCESS_LEN), (ELEMENT_LEN), (IS_APPROX), true>::CheckNByteValueAfterVGather, false, 2, OPND_CREATE_CCT_INT(slot), OPND_CREATE_INTPTR(ins_clone)); \
} else { \
dr_insert_clean_call(drcontext, bb, ins, (void *)ZeroSpyAnalysis<T, (ACCESS_LEN), (ELEMENT_LEN), (IS_APPROX), false>::CheckNByteValueAfterVGather, false, 2, OPND_CREATE_CCT_INT(slot), OPND_CREATE_INTPTR(ins_clone)); \
} } while(0)
#endif

template<class T, uint32_t AccessLen, uint32_t ElemLen, bool isApprox, bool enable_sampling>
struct ZeroSpyAnalysis{
#ifdef X86
    static __attribute__((always_inline)) void CheckNByteValueAfterVGather(int32_t slot, instr_t* instr)
    {
        void *drcontext = dr_get_current_drcontext();
        per_thread_t *pt = (per_thread_t *)drmgr_get_tls_field(drcontext, tls_idx);
        if(enable_sampling) {
            if(!vtracer_get_sampling_state(drcontext)) {
                return ;
            }
        }
        dr_mcontext_t mcontext;
        mcontext.size = sizeof(mcontext);
        mcontext.flags= DR_MC_ALL;
        DR_ASSERT(dr_get_mcontext(drcontext, &mcontext));
#ifdef DEBUG_VGATHER
        printf("\n^^ CheckNByteValueAfterVGather: ");
        disassemble(drcontext, instr_get_app_pc(instr), 1/*sdtout file desc*/);
        printf("\n");
#endif
        if(isApprox) {
            uint32_t zeros=0;
            uint64_t map=0;
            app_pc addr;
            bool is_write;
            uint32_t pos;
            for( int index=0; instr_compute_address_ex_pos(instr, &mcontext, index, &addr, &is_write, &pos); ++index ) {
                DR_ASSERT(!is_write);
                data_handle_t data_hndl = drcctlib_get_data_hndl_ignore_stack_data(drcontext, addr);
                if(data_hndl.object_type!=DYNAMIC_OBJECT && data_hndl.object_type!=STATIC_OBJECT) {
                    continue ;
                }
                T* bytes = reinterpret_cast<T*>(addr);
                uint64_t val = (bytes[0] == 0) ? 1 : 0;
                AddToApproximateRedTable((uint64_t)addr, data_hndl, val, 1, val, sizeof(T), pt);
            }
        } else {
            assert(0 && "VGather should be a floating point operation!");
        }
    }
#endif
};

struct ZerospyInstrument{
#ifdef X86
    static __attribute__((always_inline)) void InstrumentReadValueBeforeVGather(void *drcontext, instrlist_t *bb, instr_t *ins, int32_t slot){
        opnd_t opnd = instr_get_src(ins, 0);
        uint32_t operSize = FloatOperandSizeTable(ins, opnd); // VGather's second operand is the memory operand
        uint32_t refSize = opnd_size_in_bytes(opnd_get_size(instr_get_dst(ins, 0)));
        per_thread_t *pt = (per_thread_t *)drmgr_get_tls_field(drcontext, tls_idx);
        instr_t* ins_clone = instr_clone(drcontext, ins);
        pt->instr_clones->push_back(ins_clone);
#ifdef DEBUG_VGATHER
        printf("^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^\n");
        printf("^^ refSize = %d, operSize = %d\n", refSize, operSize);
        printf("^^ Disassembled Instruction ^^^\n");
        disassemble(drcontext, instr_get_app_pc(ins), STDOUT);
        printf("^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^\n");
#endif
        switch(refSize) {
            case 1:
            case 2: 
            case 4: 
            case 8: 
            case 10: 
#ifdef _WERROR
                    printf("\nERROR: refSize for floating point instruction is too small: %d!\n", refSize);
                    printf("^^ Disassembled Instruction ^^^\n");
                    disassemble(drcontext, instr_get_app_pc(ins), 1/*sdtout file desc*/);
                    printf("^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^\n");
                    fflush(stdout);
                    assert(0 && "memory read floating data with unexptected small size");
#else
                    dr_mutex_lock(gLock);
                    dr_fprintf(fwarn, "\nERROR: refSize for floating point instruction is too small: %d!\n", refSize);
                    dr_fprintf(fwarn, "^^ Disassembled Instruction ^^^\n");
                    disassemble(drcontext, instr_get_app_pc(ins), fwarn);
                    dr_fprintf(fwarn, "^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^\n");
                    warned = true;
                    dr_mutex_unlock(gLock);
                    /* Do nothing, just report warning */
                    break;
#endif
            case 16: {
                switch (operSize) {
                    case 4: HANDLE_VGATHER(float, 16, 4, true); break;
                    case 8: HANDLE_VGATHER(double, 16, 8, true); break;
                    default: assert(0 && "handle large mem read with unexpected operand size\n"); break;
                }
            }break;
            case 32: {
                switch (operSize) {
                    case 4: HANDLE_VGATHER(float, 32, 4, true); break;
                    case 8: HANDLE_VGATHER(double, 32, 8, true); break;
                    default: assert(0 && "handle large mem read with unexpected operand size\n"); break;
                }
            }break;
            default: 
#ifdef _WERROR
                    printf("\nERROR: refSize for floating point instruction is too large: %d!\n", refSize);
                    printf("^^ Disassembled Instruction ^^^\n");
                    disassemble(drcontext, instr_get_app_pc(ins), 1/*sdtout file desc*/);
                    printf("^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^\n");
                    fflush(stdout);
                    assert(0 && "unexpected large memory read\n"); break;
#else
                    dr_mutex_lock(gLock);
                    dr_fprintf(fwarn, "\nERROR: refSize for floating point instruction is too large: %d!\n", refSize);
                    dr_fprintf(fwarn, "^^ Disassembled Instruction ^^^\n");
                    disassemble(drcontext, instr_get_app_pc(ins), fwarn);
                    dr_fprintf(fwarn, "^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^\n");
                    warned = true;
                    dr_mutex_unlock(gLock);
                    /* Do nothing, just report warning */
                    break;
#endif
        }
    }
#endif
};

static void
ThreadOutputFileInit(per_thread_t *pt)
{
    int32_t id = drcctlib_get_thread_id();
    pt->threadId = id;
    char name[MAXIMUM_PATH] = "";
    sprintf(name + strlen(name), "%s/thread-%d.topn.log", g_folder_name.c_str(), id);
    pt->output_file = dr_open_file(name, DR_FILE_WRITE_OVERWRITE | DR_FILE_ALLOW_LARGE);
    DR_ASSERT(pt->output_file != INVALID_FILE);
    if (op_enable_sampling.get_value()) {
        dr_fprintf(pt->output_file, "[ZEROSPY INFO] Sampling Enabled\n");
    }
}

static void
ClientThreadStart(void *drcontext)
{
    per_thread_t *pt = (per_thread_t *)dr_thread_alloc(drcontext, sizeof(per_thread_t));
    if (pt == NULL) {
        ZEROSPY_EXIT_PROCESS("pt == NULL");
    }
    pt->INTRedMap = new RedLogMap();
    pt->FPRedMap = new FPRedLogMap();
    pt->instr_clones = new vector<instr_t*>();
    pt->INTRedMap->rehash(10000000);
    pt->FPRedMap->rehash(10000000);
    drmgr_set_tls_field(drcontext, tls_idx, (void *)pt);
    ThreadOutputFileInit(pt);
}

/*******************************************************************/
/* Output functions */
//#define SKIP_SMALLACCESS
#ifdef SKIP_SMALLACCESS
#define LOGGING_THRESHOLD 100
#endif

// redundant data for a object
struct ObjRedundancy {
    uint64_t objID;
    uint64_t dfreq;
    uint64_t bytes;
};

static inline bool ObjRedundancyCompare(const struct ObjRedundancy &first, const struct ObjRedundancy &second) {
    return first.bytes > second.bytes ? true : false;
}

static inline void PrintSize(file_t gTraceFile, uint64_t size, const char* unit="B") {
    if(size >= (1<<20)) {
        dr_fprintf(gTraceFile, "%lf M%s",(double)size/(double)(1<<20),unit);
    } else if(size >= (1<<10)) {
        dr_fprintf(gTraceFile, "%lf K%s",(double)size/(double)(1<<10),unit);
    } else {
        dr_fprintf(gTraceFile, "%ld %s",size,unit);
    }
}

static inline void PrintJsonSize(char *str, uint64_t size, const char* unit="B") {
    if(size >= (1<<20)) {
        dr_snprintf(str, 64, "%lf M%s", (double)size/(double)(1<<20),unit);
    } else if(size >= (1<<10)) {
        dr_snprintf(str, 64, "%lf K%s", (double)size/(double)(1<<10),unit);
    } else {
        dr_snprintf(str, 64, "%ld %s", size,unit);
    }
}

#define MAX_REDMAP_PRINT_SIZE 128
// only print top 5 redundancy with full redmap to file
#define MAX_PRINT_FULL 5

static uint64_t PrintRedundancyPairs(per_thread_t *pt, uint64_t threadBytesLoad, int threadId, rapidjson::Value &threadDetailedMetrics, rapidjson::Value &threadDetailedDataCentricMetrics) {
    printf("[ZEROSPY INFO] PrintRedundancyPairs for INT...\n");
    vector<ObjRedundancy> tmpList;
    rapidjson::Value integerRedundantInfo(rapidjson::kArrayType);
    file_t gTraceFile = pt->output_file;
    
    uint64_t grandTotalRedundantBytes = 0;
    dr_fprintf(gTraceFile, "\n--------------- Dumping Data Redundancy Info ----------------\n");
    dr_fprintf(gTraceFile, "\n*************** Dump Data from Thread %d ****************\n", threadId);

    int count=0;
    int rep=-1;
    int total = pt->INTRedMap->size();
    tmpList.reserve(total);
    for(RedLogMap::iterator it = pt->INTRedMap->begin(); it != pt->INTRedMap->end(); ++it) {
        ++count;
        if(100 * count / total!=rep) {
            rep = 100 * count / total;
            printf("\r[ZEROSPY INFO] Stage 1 : %3d %% Finish",rep);
            fflush(stdout);
        }
        ObjRedundancy tmp = {(*it).first, 0, 0};
        for(RedLogSizeMap::iterator it2 = it->second.begin(); it2 != it->second.end(); ++it2) {
            tmp.bytes += it2->second.red;
            if(it2->second.red) {
                bitref_t accmap = &(it2->second.accmap);
                bitref_t redmap = &(it2->second.redmap);
                for(size_t i=0;i<accmap->size;++i) {
                    if(!bitvec_at(accmap, i)) {
                        if(bitvec_at(redmap, i)) ++tmp.dfreq;
                    }
                }
            }
        }
        if(tmp.bytes==0) continue;
        
        grandTotalRedundantBytes += tmp.dfreq;
        tmpList.push_back(tmp); 
    }
    printf("\n[ZEROSPY INFO] Stage 1 finish\n");

    __sync_fetch_and_add(&grandTotBytesRedLoad,grandTotalRedundantBytes);
    dr_fprintf(gTraceFile, "\n Total redundant bytes = %f %%\n", grandTotalRedundantBytes * 100.0 / threadBytesLoad);

    if(grandTotalRedundantBytes==0) {
        dr_fprintf(gTraceFile, "\n------------ Dumping Redundancy Info Finish -------------\n");
        return grandTotalRedundantBytes;
    }

    sort(tmpList.begin(), tmpList.end(), ObjRedundancyCompare);

    int objNum = 0;
    rep = -1;
    total = tmpList.size()<MAX_OBJS_TO_LOG?tmpList.size():MAX_OBJS_TO_LOG;
    for(vector<ObjRedundancy>::iterator listIt = tmpList.begin(); listIt != tmpList.end(); ++listIt) {
        if(objNum++ >= MAX_OBJS_TO_LOG) break;
        rapidjson::Value integerRedundantInfoItem(rapidjson::kObjectType);
        if(100 * objNum / total!=rep) {
            rep = 100 * objNum / total;
            printf("\r[ZEROSPY INFO] Stage 2 : %3d %% Finish",rep);
            fflush(stdout);
        }
        if((uint8_t)DECODE_TYPE((*listIt).objID) == DYNAMIC_OBJECT) {
            integerRedundantInfoItem.AddMember("name", rapidjson::Value("Dynamic Object", jsonAllocator), jsonAllocator);
            // std::string cctInfo = drcctlib_get_full_cct_string(DECODE_NAME((*listIt).objID), true, true, MAX_DEPTH);
            // integerRedundantInfoItem.AddMember("CCT Info", rapidjson::Value(cctInfo.c_str(), jsonAllocator), jsonAllocator);
            dr_fprintf(gTraceFile, "\n\n^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^ Dynamic Object: ^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^\n");
            drcctlib_print_backtrace(gTraceFile, DECODE_NAME((*listIt).objID), true, true, MAX_DEPTH);
        } else {
            char str[64];
            dr_snprintf(str, 64, "Static Object: %s", drcctlib_get_str_from_strpool((uint32_t)DECODE_NAME((*listIt).objID)));
            integerRedundantInfoItem.AddMember("name", rapidjson::Value(str, jsonAllocator), jsonAllocator); 
            dr_fprintf(gTraceFile, "\n\n^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^ Static Object: %s ^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^\n", drcctlib_get_str_from_strpool((uint32_t)DECODE_NAME((*listIt).objID)));
        }

        dr_fprintf(gTraceFile, "\n==========================================\n");
        dr_fprintf(gTraceFile, "Redundancy Ratio = %f %% (%ld Bytes, %ld Redundant Load Bytes)\n", (*listIt).dfreq * 100.0 / grandTotalRedundantBytes, (*listIt).dfreq, (*listIt).bytes);
        integerRedundantInfoItem.AddMember("rate", (*listIt).dfreq * 100.0 / grandTotalRedundantBytes, jsonAllocator);

        char str[64];
        dr_snprintf(str, 64, "(%ld Bytes, %ld Redundant Load Bytes)", (*listIt).dfreq, (*listIt).bytes);
        integerRedundantInfoItem.AddMember("Redundancy", rapidjson::Value(str, jsonAllocator), jsonAllocator); 
        
        int dataNum = 0;
        for(RedLogSizeMap::iterator it2 = (*pt->INTRedMap)[(*listIt).objID].begin(); it2 != (*pt->INTRedMap)[(*listIt).objID].end(); ++it2) {
            uint64_t dfreq = 0;
            uint64_t dread = 0;
            uint64_t dsize = it2->first;
            bitref_t accmap = &(it2->second.accmap);
            bitref_t redmap = &(it2->second.redmap);

            assert(accmap->size==dsize);
            assert(accmap->size==redmap->size);

            for(size_t i=0;i<accmap->size;++i) {
                if(!bitvec_at(accmap, i)) {
                    ++dread;
                    if(bitvec_at(redmap, i)) ++dfreq;
                }
            }
                
            dr_fprintf(gTraceFile, "\n\n======= DATA SIZE : ");
            PrintSize(gTraceFile, dsize);
            PrintJsonSize(str, dsize);
            char keyStr[32];
            dr_snprintf(keyStr, 32, "Data Size %d", dataNum);
            integerRedundantInfoItem.AddMember(rapidjson::Value(keyStr, jsonAllocator), rapidjson::Value(str, jsonAllocator), jsonAllocator); 

            dr_fprintf(gTraceFile, "( Not Accessed Data %f %% (%ld Bytes), Redundant Data %f %% (%ld Bytes) )", 
                    (dsize-dread) * 100.0 / dsize, dsize-dread, 
                    dfreq * 100.0 / dsize, dfreq);

            dr_snprintf(str, 64, "%f %% (%ld Bytes)", (dsize-dread) * 100.0 / dsize, dsize-dread);
            dr_snprintf(keyStr, 32, "Not Accessed Data %d", dataNum);
            integerRedundantInfoItem.AddMember(rapidjson::Value(keyStr, jsonAllocator), rapidjson::Value(str, jsonAllocator), jsonAllocator); 

            dr_snprintf(str, 64, "%f %% (%ld Bytes)", dfreq * 100.0 / dsize, dfreq);
            dr_snprintf(keyStr, 32, "Redundant Data %d", dataNum);
            integerRedundantInfoItem.AddMember(rapidjson::Value(keyStr, jsonAllocator), rapidjson::Value(str, jsonAllocator), jsonAllocator); 

            dr_fprintf(gTraceFile, "\n======= Redundant byte map : [0] ");
            uint32_t num=0;
            for(size_t i=0;i<accmap->size;++i) {
                if(!bitvec_at(accmap, i)) {
                    if(bitvec_at(redmap, i)) {
                        dr_fprintf(gTraceFile, "00 ");
                    } else {
                        dr_fprintf(gTraceFile, "XX ");
                    }
                } else {
                    dr_fprintf(gTraceFile, "?? ");
                }
                ++num;
                if(num>MAX_REDMAP_PRINT_SIZE) {
                    dr_fprintf(gTraceFile, "... ");
                    break;
                }
            }
            #if 0
                    if(objNum<=MAX_PRINT_FULL) {
                        char fn[50] = {};
                        sprintf(fn,"%lx.redmap",(*listIt).objID);
                        file_t fp = dr_open(fn,"w");
                        
                        for(size_t i=0;i<accmap->size;++i) {
                            int tmp = 1;
                            if(bitvec_at(accmap, i)) {
                                fwrite(&tmp, sizeof(char), 1, fp);
                            } else {
                                tmp = 0;
                                fwrite(&tmp, sizeof(char), 1, fp);
                            }

                            tmp = 1;
                            if(bitvec_at(redmap, i)) {
                                fwrite(&tmp, sizeof(char), 1, fp);
                            } else {
                                tmp = 0;
                                fwrite(&tmp, sizeof(char), 1, fp);
                            }
                        }
                        dr_snprintf(keyStr, 32, "Redmap %d", dataNum);
                        integerRedundantInfoItem.AddMember(rapidjson::Value(keyStr, jsonAllocator), rapidjson::Value(fn, jsonAllocator), jsonAllocator); 
                    }
            #endif
            dataNum++;
        }
        integerRedundantInfoItem.AddMember("dataNum", dataNum, jsonAllocator); 

#if 0
        if(objNum<=MAX_PRINT_FULL) {
            char fn[50] = {};
            sprintf(fn,"%lx.redmap",(*listIt).objID);
            file_t fp = dr_open(fn,"w");
            if((uint8_t)DECODE_TYPE((*listIt).objID) == DYNAMIC_OBJECT) {
                dr_fprintf(fp, "\n\n^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^ Dynamic Object: ^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^\n");
                PrintFullCallingContext(DECODE_NAME((*listIt).objID)); // segfault might happen if the shadow memory based data centric is used
            } else  
                dr_fprintf(fp, "\n\n^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^ Static Object: %s ^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^\n", GetStringFromStringPool((uint32_t)DECODE_NAME((*listIt).objID)));
            for(size_t i=0;i<accmap->size;++i) {
                if(!bitvec_at(accmap, i)) {
                    if(bitvec_at(redmap, i)) {
                        dr_fprintf(fp, "00 ");
                    } else {
                        dr_fprintf(fp, "XX ");
                    }
                } else {
                    dr_fprintf(fp, "?? ");
                }
            }
        }
#endif
        integerRedundantInfo.PushBack(integerRedundantInfoItem, jsonAllocator);
    }
    printf("\n[ZEROSPY INFO] Stage 2 Finish\n");
    dr_fprintf(gTraceFile, "\n------------ Dumping Redundancy Info Finish -------------\n");
    threadDetailedDataCentricMetrics.AddMember("Integer Redundant Info", integerRedundantInfo, jsonAllocator);
    return grandTotalRedundantBytes;
}

void free_int_logs(per_thread_t* pt) {
    int num=0;
    for(RedLogMap::iterator it = pt->INTRedMap->begin(); it != pt->INTRedMap->end(); ++it) {
        for(RedLogSizeMap::iterator it2 = it->second.begin(); it2 != it->second.end(); ++it2) {
            bitvec_free(&(it2->second.accmap));
            bitvec_free(&(it2->second.redmap));
            ++num;
        }
        it->second.clear();
    }
    delete pt->INTRedMap;
    dr_fprintf(STDOUT, "INTLOG FREED: %d\n", num);
}

void free_fp_logs(per_thread_t* pt) {
    int num=0;
    for(FPRedLogMap::iterator it = pt->FPRedMap->begin(); it != pt->FPRedMap->end(); ++it) {
        for(FPRedLogSizeMap::iterator it2 = it->second.begin(); it2 != it->second.end(); ++it2) {
            bitvec_free(&(it2->second.accmap));
            bitvec_free(&(it2->second.redmap));
            ++num;
        }
        it->second.clear();
    }
    delete pt->FPRedMap;
    dr_fprintf(STDOUT, "FPLOG FREED: %d\n", num);
}

static uint64_t PrintApproximationRedundancyPairs(per_thread_t *pt, uint64_t threadBytesLoad, int threadId, rapidjson::Value &threadDetailedMetrics, rapidjson::Value &threadDetailedDataCentricMetrics) {
    printf("[ZEROSPY INFO] PrintRedundancyPairs for FP...\n");
    vector<ObjRedundancy> tmpList;
    rapidjson::Value floatingPointRedundantInfo(rapidjson::kArrayType);
    file_t gTraceFile = pt->output_file;
    
    uint64_t grandTotalRedundantBytes = 0;
    dr_fprintf(gTraceFile, "\n--------------- Dumping Data Approximation Redundancy Info ----------------\n");
    dr_fprintf(gTraceFile, "\n*************** Dump Data(delta=%.2f%%) from Thread %d ****************\n", delta*100,threadId);

    int count=0;
    int rep=-1;
    int total = pt->FPRedMap->size();
    tmpList.reserve(total);
    for(FPRedLogMap::iterator it = pt->FPRedMap->begin(); it != pt->FPRedMap->end(); ++it) {
        ++count;
        if(100 * count / total!=rep) {
            rep = 100 * count / total;
            printf("\r[ZEROSPY INFO] Stage 1 : %3d %% Finish",rep);
            fflush(stdout);
        }
        ObjRedundancy tmp = {(*it).first, 0, 0};
        for(FPRedLogSizeMap::iterator it2 = it->second.begin(); it2 != it->second.end(); ++it2) {
            tmp.bytes += it2->second.red;
            if(it2->second.red) {
                bitref_t accmap = &(it2->second.accmap);
                bitref_t redmap = &(it2->second.redmap);
                for(size_t i=0;i<accmap->size;++i) {
                    if(!bitvec_at(accmap, i)) {
                        if(bitvec_at(redmap, i)) ++tmp.dfreq;
                    }
                }
            }
        }
        if(tmp.bytes==0) continue;
        grandTotalRedundantBytes += tmp.dfreq;
        tmpList.push_back(tmp); 
    }
    printf("\n[ZEROSPY INFO] Stage 1 Finish\n");

    __sync_fetch_and_add(&grandTotBytesApproxRedLoad,grandTotalRedundantBytes);

    dr_fprintf(gTraceFile, "\n Total redundant bytes = %f %%\n", grandTotalRedundantBytes * 100.0 / threadBytesLoad);

    if(grandTotalRedundantBytes==0) {
        dr_fprintf(gTraceFile, "\n------------ Dumping Approx Redundancy Info Finish -------------\n");
        return grandTotalRedundantBytes;
    }
    sort(tmpList.begin(), tmpList.end(), ObjRedundancyCompare);

    int objNum = 0;
    vector<uint8_t> state;
    for(vector<ObjRedundancy>::iterator listIt = tmpList.begin(); listIt != tmpList.end(); ++listIt) {
        if(objNum++ >= MAX_OBJS_TO_LOG) break;
        rapidjson::Value floatRedundantInfoItem(rapidjson::kObjectType);
        if(100 * objNum / total!=rep) {
            rep = 100 * objNum / total;
            printf("\r[ZEROSPY INFO] Stage 2 : %3d %% Finish",rep);
            fflush(stdout);
        }
        if((uint8_t)DECODE_APPROX_TYPE((*listIt).objID) == DYNAMIC_OBJECT) {
            floatRedundantInfoItem.AddMember("name", rapidjson::Value("Dynamic Object", jsonAllocator), jsonAllocator);
            // std::string cctInfo = drcctlib_get_full_cct_string(DECODE_APPROX_NAME((*listIt).objID), true, true, MAX_DEPTH);
            // floatRedundantInfoItem.AddMember("CCT Info", rapidjson::Value(cctInfo.c_str(), jsonAllocator), jsonAllocator);
            dr_fprintf(gTraceFile, "\n\n^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^ Dynamic Object: ^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^\n");
            drcctlib_print_backtrace(gTraceFile, DECODE_APPROX_NAME((*listIt).objID), true, true, MAX_DEPTH);
        } else {
            char str[64];
            dr_snprintf(str, 64, "Static Object: %s", drcctlib_get_str_from_strpool((uint32_t)DECODE_NAME((*listIt).objID)));
            floatRedundantInfoItem.AddMember("name", rapidjson::Value(str, jsonAllocator), jsonAllocator); 
            dr_fprintf(gTraceFile, "\n\n^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^ Static Object: %s ^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^\n", drcctlib_get_str_from_strpool((uint32_t)DECODE_APPROX_NAME((*listIt).objID)));
        }
        dr_fprintf(gTraceFile, "\n==========================================\n");
        dr_fprintf(gTraceFile, "Redundancy Ratio = %f %% (%ld Bytes, %ld Redundant Load Bytes)\n", (*listIt).dfreq * 100.0 / grandTotalRedundantBytes, (*listIt).dfreq, (*listIt).bytes);
        floatRedundantInfoItem.AddMember("rate", (*listIt).dfreq * 100.0 / grandTotalRedundantBytes, jsonAllocator);

        char str[64];
        dr_snprintf(str, 64, "(%ld Bytes, %ld Redundant Load Bytes)", (*listIt).dfreq, (*listIt).bytes);
        floatRedundantInfoItem.AddMember("Redundancy", rapidjson::Value(str, jsonAllocator), jsonAllocator); 

        int dataNum = 0;
        for(FPRedLogSizeMap::iterator it2 = (*pt->FPRedMap)[(*listIt).objID].begin(); it2 != (*pt->FPRedMap)[(*listIt).objID].end(); ++it2) {
            uint64_t dfreq = 0;
            uint64_t dread = 0;
            uint64_t dsize = it2->first;
            uint8_t  dtype = it2->second.typesz;
            bitref_t accmap = &(it2->second.accmap);
            bitref_t redmap = &(it2->second.redmap);

            assert(accmap->size==dsize);
            assert(accmap->size==redmap->size);

            for(size_t i=0;i<accmap->size;++i) {
                if(!bitvec_at(accmap, i)) {
                    ++dread;
                    if(bitvec_at(redmap, i)) ++dfreq;
                    i += dtype-1; // each loop will increament 1
                }
            }
                
            dr_fprintf(gTraceFile, "\n\n======= DATA SIZE : ");
            PrintSize(gTraceFile, dsize, " Elements");
            PrintJsonSize(str, dsize, " Elements");
            char keyStr[32];
            dr_snprintf(keyStr, 32, "Data Size %d", dataNum);
            floatRedundantInfoItem.AddMember(rapidjson::Value(keyStr, jsonAllocator), rapidjson::Value(str, jsonAllocator), jsonAllocator); 

            dr_fprintf(gTraceFile, "( Not Accessed Data %f %% (%ld Reads), Redundant Data %f %% (%ld Reads) )", 
                    (dsize-dread) * 100.0 / dsize, dsize-dread, 
                    dfreq * 100.0 / dsize, dfreq);

            dr_snprintf(str, 64, "%f %% (%ld Reads)", (dsize-dread) * 100.0 / dsize, dsize-dread);
            dr_snprintf(keyStr, 32, "Not Accessed Data %d", dataNum);
            floatRedundantInfoItem.AddMember(rapidjson::Value(keyStr, jsonAllocator), rapidjson::Value(str, jsonAllocator), jsonAllocator); 

            dr_snprintf(str, 64, "%f %% (%ld Reads)", dfreq * 100.0 / dsize, dfreq);
            dr_snprintf(keyStr, 32, "Redundant Data %d", dataNum);
            floatRedundantInfoItem.AddMember(rapidjson::Value(keyStr, jsonAllocator), rapidjson::Value(str, jsonAllocator), jsonAllocator); 

            dr_fprintf(gTraceFile, "\n======= Redundant byte map : [0] ");
            uint32_t num=0;
            for(size_t i=0;i<accmap->size;++i) {
                if(!bitvec_at(accmap, i)) {
                    if(bitvec_at(redmap, i)) {
                        dr_fprintf(gTraceFile, "00 ");
                    } else {
                        dr_fprintf(gTraceFile, "XX ");
                    }
                } else {
                    dr_fprintf(gTraceFile, "?? ");
                }
                ++num;
                if(num>MAX_REDMAP_PRINT_SIZE) {
                    dr_fprintf(gTraceFile, "... ");
                    break;
                }
            }
            #if 0
                    if(objNum<=MAX_PRINT_FULL) {
                        char fn[50] = {};
                        sprintf(fn,"%lx.redmap",(*listIt).objID);
                        file_t fp = dr_open(fn,"w");
                        
                        for(size_t i=0;i<accmap->size;++i) {
                            int tmp = 1;
                            if(bitvec_at(accmap, i)) {
                                fwrite(&tmp, sizeof(char), 1, fp);
                            } else {
                                tmp = 0;
                                fwrite(&tmp, sizeof(char), 1, fp);
                            }

                            tmp = 1;
                            if(bitvec_at(redmap, i)) {
                                fwrite(&tmp, sizeof(char), 1, fp);
                            } else {
                                tmp = 0;
                                fwrite(&tmp, sizeof(char), 1, fp);
                            }
                        }
                        dr_snprintf(keyStr, 32, "Redmap %d", dataNum);
                        floatRedundantInfoItem.AddMember(rapidjson::Value(keyStr, jsonAllocator), rapidjson::Value(fn, jsonAllocator), jsonAllocator); 
                    }
            #endif
            dataNum++;
        }
        floatRedundantInfoItem.AddMember("dataNum", dataNum, jsonAllocator); 

#if 0
        if(objNum<=MAX_PRINT_FULL) {
            char fn[50] = {};
            sprintf(fn,"%lx.redmap",(*listIt).objID);
            file_t fp = dr_open(fn,"w");
            if((uint8_t)DECODE_APPROX_TYPE((*listIt).objID) == DYNAMIC_OBJECT) {
                dr_fprintf(fp, "\n\n^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^ Dynamic Object: %lx^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^\n",(*listIt).objID);
            } else  
                dr_fprintf(fp, "\n\n^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^ Static Object: %s, %lx ^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^\n", GetStringFromStringPool((uint32_t)DECODE_APPROX_NAME((*listIt).objID)),(*listIt).objID);
            for(size_t i=0;i<accmap->size;++i) {
                if(!bitvec_at(accmap, i)) {
                    if(bitvec_at(redmap, i)) {
                        dr_fprintf(fp, "00 ");
                    } else {
                        dr_fprintf(fp, "XX ");
                    }
                } else {
                    dr_fprintf(fp, "?? ");
                }
            }
        }
#endif
        floatingPointRedundantInfo.PushBack(floatRedundantInfoItem, jsonAllocator);
    }
    printf("\n[ZEROSPY INFO] Stage 2 Finish\n");

    dr_fprintf(gTraceFile, "\n------------ Dumping Approx Redundancy Info Finish -------------\n");

    threadDetailedDataCentricMetrics.AddMember("Floating Point Redundant Info", floatingPointRedundantInfo, jsonAllocator);
    threadDetailedMetrics.AddMember("Data Centric", threadDetailedDataCentricMetrics, jsonAllocator);
    threadDetailedMetricsMap[threadId] = threadDetailedMetrics;
    return grandTotalRedundantBytes;
}
/*******************************************************************/

uint64_t getThreadByteLoad(per_thread_t* pt) {
    uint64_t threadByteLoad = 0;

    for(RedLogMap::iterator it = pt->INTRedMap->begin(); it != pt->INTRedMap->end(); ++it) {
        ObjRedundancy tmp = {(*it).first, 0};
        for(RedLogSizeMap::iterator it2 = it->second.begin(); it2 != it->second.end(); ++it2) {
            threadByteLoad += it2->first;
        }
    }

    for(FPRedLogMap::iterator it = pt->FPRedMap->begin(); it != pt->FPRedMap->end(); ++it) {
        ObjRedundancy tmp = {(*it).first, 0};
        for(FPRedLogSizeMap::iterator it2 = it->second.begin(); it2 != it->second.end(); ++it2) {
            threadByteLoad += it2->first;
        }
    }

    return threadByteLoad;
}

static void
ClientThreadEnd(void *drcontext)
{
#ifdef TIMING
    uint64_t time = get_miliseconds();
#endif
    per_thread_t *pt = (per_thread_t *)drmgr_get_tls_field(drcontext, tls_idx);
    if (pt->INTRedMap->empty() && pt->FPRedMap->empty()) {
        free_int_logs(pt);
        free_fp_logs(pt);
        dr_close_file(pt->output_file);

        for(size_t i=0;i<pt->instr_clones->size();++i) {
            instr_destroy(drcontext, (*pt->instr_clones)[i]);
        }
        delete pt->instr_clones;

#ifdef DEBUG_REUSE
        dr_close_file(pt->log_file);
#endif
        dr_thread_free(drcontext, pt, sizeof(per_thread_t));
        return;
    }
    uint64_t threadByteLoad = getThreadByteLoad(pt);
    __sync_fetch_and_add(&grandTotBytesLoad,threadByteLoad);
    int threadId = pt->threadId;
    rapidjson::Value threadDetailedMetrics(rapidjson::kObjectType);
    rapidjson::Value threadDetailedDataCentricMetrics(rapidjson::kObjectType);
    uint64_t threadRedByteLoadINT = 0;
    uint64_t threadRedByteLoadFP = 0;
    if(threadByteLoad != 0) {
        threadRedByteLoadINT = PrintRedundancyPairs(pt, threadByteLoad, threadId, threadDetailedMetrics, threadDetailedDataCentricMetrics);
        free_int_logs(pt);
        threadRedByteLoadFP = PrintApproximationRedundancyPairs(pt, threadByteLoad, threadId, threadDetailedMetrics, threadDetailedDataCentricMetrics);
        free_fp_logs(pt);
    } else {
        free_int_logs(pt);
        free_fp_logs(pt);
    }
#ifdef TIMING
    time = get_miliseconds() - time;
    printf("Thread %d: Time %ld ms for generating outputs\n", threadId, time);
#endif

    dr_mutex_lock(gLock);
    dr_fprintf(gFile, "\n#THREAD %d Redundant Read:", threadId);
    dr_fprintf(gFile, "\nTotalBytesLoad: %lu ",threadByteLoad);
    if(threadByteLoad != 0) {
        dr_fprintf(gFile, "\nRedundantBytesLoad: %lu %.2f",threadRedByteLoadINT, threadRedByteLoadINT * 100.0/threadByteLoad);
        dr_fprintf(gFile, "\nApproxRedundantBytesLoad: %lu %.2f\n",threadRedByteLoadFP, threadRedByteLoadFP * 100.0/threadByteLoad);
    }
    dr_mutex_unlock(gLock);

    dr_close_file(pt->output_file);

    for(size_t i=0;i<pt->instr_clones->size();++i) {
        instr_destroy(drcontext, (*pt->instr_clones)[i]);
    }
    delete pt->instr_clones;

#ifdef DEBUG_REUSE
    dr_close_file(pt->log_file);
#endif
    dr_thread_free(drcontext, pt, sizeof(per_thread_t));
}

static void
ClientInit(int argc, const char *argv[])
{
    /* Parse options */
    std::string parse_err;
    int last_index;
    if (!droption_parser_t::parse_argv(DROPTION_SCOPE_CLIENT, argc, argv, &parse_err, &last_index)) {
        dr_fprintf(STDERR, "Usage error: %s", parse_err.c_str());
        dr_abort();
    }
    /* Creating result directories */
    
    gDoc.SetObject();

    pid_t pid = getpid();
#ifdef ARM_CCTLIB
    char name[MAXIMUM_PATH] = "arm-";
#else
    char name[MAXIMUM_PATH] = "x86-";
#endif
    gethostname(name + strlen(name), MAXIMUM_PATH - strlen(name));
    sprintf(name + strlen(name), "-%d-zerospy-datacentric", pid);
    g_folder_name.assign(name, strlen(name));
    mkdir(g_folder_name.c_str(), S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH);

    dr_fprintf(STDOUT, "[ZEROSPY INFO] Using Data Centric Zerospy\n");
    dr_fprintf(STDOUT, "[ZEROSPY INFO] Profiling result directory: %s\n", g_folder_name.c_str());

    sprintf(name+strlen(name), "/zerospy.log");
    gFile = dr_open_file(name, DR_FILE_WRITE_OVERWRITE | DR_FILE_ALLOW_LARGE);
    gJson = fopen("report.json", "w");
    DR_ASSERT(gFile != INVALID_FILE);
    DR_ASSERT(gJson != NULL);
    if (op_enable_sampling.get_value()) {
        dr_fprintf(STDOUT, "[ZEROSPY INFO] Sampling Enabled\n");
        dr_fprintf(gFile, "[ZEROSPY INFO] Sampling Enabled\n");
        window_enable = op_window_enable.get_value();
        window_disable= op_window.get_value();
        float rate = (float)window_enable / (float)window_disable;
        dr_fprintf(STDOUT, "[ZEROSPY INFO] Sampling Rate: %.3f, Window Size: %ld\n", rate, window_disable);
        dr_fprintf(gFile,  "[ZEROSPY INFO] Sampling Rate: %.3f, Window Size: %ld\n", rate, window_disable);
    } else {
        dr_fprintf(STDOUT, "[ZEROSPY INFO] Sampling Disabled\n");
        dr_fprintf(gFile, "[ZEROSPY INFO] Sampling Disabled\n");
    }
    if (op_help.get_value()) {
        dr_fprintf(STDOUT, "%s\n", droption_parser_t::usage_long(DROPTION_SCOPE_CLIENT).c_str());
        exit(1);
    }
#ifndef _WERROR
    sprintf(name+strlen(name), ".warn");
    fwarn = dr_open_file(name, DR_FILE_WRITE_OVERWRITE | DR_FILE_ALLOW_LARGE);
    DR_ASSERT(fwarn != INVALID_FILE);
#endif
#ifdef ZEROSPY_DEBUG
    sprintf(name+strlen(name), ".debug");
    gDebug = dr_open_file(name, DR_FILE_WRITE_OVERWRITE | DR_FILE_ALLOW_LARGE);
    DR_ASSERT(gDebug != INVALID_FILE);
#endif
}

static void
ClientExit(void)
{
    dr_fprintf(gFile, "\n#Redundant Read:");
    dr_fprintf(gFile, "\nTotalBytesLoad: %lu \n",grandTotBytesLoad);
    dr_fprintf(gFile, "\nRedundantBytesLoad: %lu %.2f\n",grandTotBytesRedLoad, grandTotBytesRedLoad * 100.0/grandTotBytesLoad);
    dr_fprintf(gFile, "\nApproxRedundantBytesLoad: %lu %.2f\n",grandTotBytesApproxRedLoad, grandTotBytesApproxRedLoad * 100.0/grandTotBytesLoad);

    totalIntegerRedundantBytes.AddMember("rate", grandTotBytesRedLoad * 100.0/grandTotBytesLoad, jsonAllocator);
    totalIntegerRedundantBytes.AddMember("fraction", rapidjson::Value((std::to_string(grandTotBytesRedLoad) + "/" + std::to_string(grandTotBytesLoad)).c_str(), jsonAllocator), jsonAllocator);

    totalFloatRedundantBytes.AddMember("rate", grandTotBytesApproxRedLoad * 100.0/grandTotBytesLoad, jsonAllocator);
    totalFloatRedundantBytes.AddMember("fraction", rapidjson::Value((std::to_string(grandTotBytesApproxRedLoad) + "/" + std::to_string(grandTotBytesLoad)).c_str(), jsonAllocator), jsonAllocator);

    metricOverview.AddMember("Total Integer Redundant Bytes", totalIntegerRedundantBytes, jsonAllocator);
    metricOverview.AddMember("Total Floating Point Redundant Bytes", totalFloatRedundantBytes, jsonAllocator);
    metricOverview.AddMember("Thread Num", threadDetailedMetricsMap.size(), jsonAllocator);
    gDoc.AddMember("Metric Overview", metricOverview, jsonAllocator);

    for(auto &threadMetrics : threadDetailedMetricsMap){
        gDoc.AddMember(rapidjson::Value(("Thread " + std::to_string(threadMetrics.first) + " Detailed Metrics").c_str(), jsonAllocator), threadMetrics.second, jsonAllocator);
    }

    char writeBuffer[127];
    rapidjson::FileWriteStream os(gJson, writeBuffer, sizeof(writeBuffer));

    rapidjson::PrettyWriter<rapidjson::FileWriteStream> writer(os);
    gDoc.Accept(writer);
#ifndef _WERROR
    if(warned) {
        dr_fprintf(gFile, "####################################\n");
        dr_fprintf(gFile, "WARNING: some unexpected instructions are ignored. Please check zerospy.log.warn for detail.\n");
        dr_fprintf(gFile, "####################################\n");
    }
#endif
    dr_close_file(gFile);
    fclose(gJson);
    dr_mutex_destroy(gLock);

    if (!dr_raw_tls_cfree(tls_offs, INSTRACE_TLS_COUNT)) {
        ZEROSPY_EXIT_PROCESS(
            "ERROR: zerospy dr_raw_tls_calloc fail");
    }

    if (!drmgr_unregister_thread_init_event(ClientThreadStart) ||
        !drmgr_unregister_thread_exit_event(ClientThreadEnd) ||
        !drmgr_unregister_tls_field(tls_idx)) {
        printf("ERROR: zerospy failed to unregister in ClientExit");
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
    dr_set_client_name("DynamoRIO Client 'zerospy_data_centric'",
                       "http://dynamorio.org/issues");
    ClientInit(argc, argv);

    if(!vprofile_init(ZEROSPY_FILTER_READ_MEM_ACCESS_INSTR, NULL, NULL, NULL, VPROFILE_COLLECT_DATAOBJ)) {
        ZEROSPY_EXIT_PROCESS("ERROR: zerospy unable to initialize vprofile");
    }

    if(op_enable_sampling.get_value()) {
        vtracer_enable_sampling(op_window_enable.get_value(), op_window.get_value());
    }

    drmgr_priority_t thread_init_pri = { sizeof(thread_init_pri),
                                         "zerospy-thread-init", NULL, NULL,
                                         DRCCTLIB_THREAD_EVENT_PRI + 1 };
    drmgr_priority_t thread_exit_pri = { sizeof(thread_exit_pri),
                                         "zerispy-thread-exit", NULL, NULL,
                                         DRCCTLIB_THREAD_EVENT_PRI + 1 };

    if (   !drmgr_register_thread_init_event_ex(ClientThreadStart, &thread_init_pri) 
        || !drmgr_register_thread_exit_event_ex(ClientThreadEnd, &thread_exit_pri) ) {
        ZEROSPY_EXIT_PROCESS("ERROR: zerospy unable to register events");
    }

    tls_idx = drmgr_register_tls_field();
    if (tls_idx == -1) {
        ZEROSPY_EXIT_PROCESS("ERROR: zerospy drmgr_register_tls_field fail");
    }
    if (!dr_raw_tls_calloc(&tls_seg, &tls_offs, INSTRACE_TLS_COUNT, 0)) {
        ZEROSPY_EXIT_PROCESS(
            "ERROR: zerospy dr_raw_tls_calloc fail");
    }
    gLock = dr_mutex_create();

    dr_register_exit_event(ClientExit);

    vtrace = vprofile_allocate_trace(VPROFILE_TRACE_VAL_ADDR);

    uint32_t opnd_mask = ANY_DATA_TYPE | MEMORY | READ | BEFORE;

    // Tracing Buffer
    // Integer 1 B
    vprofile_register_trace_cb(vtrace, VPROFILE_FILTER_OPND, opnd_mask, INT8, trace_update_cb<1,1,false>);
    // Integer 2 B
    vprofile_register_trace_cb(vtrace, VPROFILE_FILTER_OPND, opnd_mask, INT16, trace_update_cb<2,2,false>);
    // Integer 4 B
    vprofile_register_trace_cb(vtrace, VPROFILE_FILTER_OPND, opnd_mask, INT32, trace_update_cb<4,4,false>);
    // Integer 8 B
    vprofile_register_trace_cb(vtrace, VPROFILE_FILTER_OPND, opnd_mask, INT64, trace_update_cb<8,8,false>);
    // Integer 16 B
    vprofile_register_trace_cb(vtrace, VPROFILE_FILTER_OPND, opnd_mask, INT128, trace_update_cb<16,16,false>);
    // Integer 32 B
    vprofile_register_trace_cb(vtrace, VPROFILE_FILTER_OPND, opnd_mask, INT256, trace_update_cb<32,32,false>);
    // Floating Point Single
    vprofile_register_trace_cb(vtrace, VPROFILE_FILTER_OPND, opnd_mask, SPx1, trace_update_cb<4,4,true>);
    // Floating Point Double
    vprofile_register_trace_cb(vtrace, VPROFILE_FILTER_OPND, opnd_mask, DPx1, trace_update_cb<8,8,true>);
    // Floating Point 4*Single
    vprofile_register_trace_cb(vtrace, VPROFILE_FILTER_OPND, opnd_mask, SPx4, trace_update_cb<16,4,true>);
    // Floating Point 2*Double
    vprofile_register_trace_cb(vtrace, VPROFILE_FILTER_OPND, opnd_mask, DPx2, trace_update_cb<16,8,true>);
    // Floating Point 8*Single
    vprofile_register_trace_cb(vtrace, VPROFILE_FILTER_OPND, opnd_mask, SPx8, trace_update_cb<32,4,true>);
    // Floating Point 4*Double
    vprofile_register_trace_cb(vtrace, VPROFILE_FILTER_OPND, opnd_mask, DPx4, trace_update_cb<32,8,true>);
}

#ifdef __cplusplus
}
#endif
