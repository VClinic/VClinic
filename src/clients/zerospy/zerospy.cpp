//#include <unordered_map>
#include <map>
#include <vector>
#include <string>
#include <sys/stat.h>
#include <assert.h>
#include <algorithm>

#ifdef DEBUG
#define IF_DEBUG(stat) stat
#else
#define IF_DEBUG(stat)
#endif
// #define USE_TIMER

//#define ZEROSPY_DEBUG
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

#include "dr_api.h"
#include "drmgr.h"
#include "drreg.h"
#include "drutil.h"
#include "drcctlib.h"
#include "dr_tools.h"
#include <sys/time.h>
#include "utils.h"
#include "vprofile.h"
// #ifdef X86
//     #define USE_SIMD
//     #define USE_SSE
//     #if defined(USE_SIMD) || defined(USE_SSE)
//         #include <nmmintrin.h>
//         #include <immintrin.h>
//         #include <emmintrin.h>
//     #endif
// #endif

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

#define MAKE_KEY(ctxt_hndl, elementSize, accessLen) \
    ((((uint64_t)(ctxt_hndl))<<32) | ((((uint64_t)(elementSize))<<8)|(uint64_t)(accessLen)))
#define DECODE_CTXT(key) (context_handle_t)(((key) >> 32) & 0xffffffff)
#define DECODE_ACCESSLEN(key) ((uint8_t)((key) & 0xff))
#define DECODE_ELEMENTSIZE(key) ((uint8_t)(((key)>>8) & 0xff))

#define ENCODE_TO_UPPER(upper, src) ((((uint64_t)upper)<<56) | ((uint64_t)src))
#define DECODE_UPPER(enc) (uint8_t)(((uint64_t)enc>>56) & 0xff)
#define DECODE_SRC(enc) (uint64_t)((uint64_t)enc & 0xffffffffffffffLL)

struct INTRedLog_t{
    uint64_t tot;
    uint64_t red;
    uint64_t fred;
    uint64_t redByteMap;
};

// AVX2
#define MAX_VLEN 4
struct FPRedLog_t{
    uint64_t ftot;
    uint64_t fred;
    //uint64_t redByteMap[MAX_VLEN];
    uint64_t redByteMap;
};

typedef std::map<context_handle_t, INTRedLog_t> INTRedLogMap_t;
typedef std::map<context_handle_t, FPRedLog_t> FPRedLogMap_t;

#define delta 0.01
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
    FPRedLogMap_t* FPRedLogMap;
    INTRedLogMap_t* INTRedLogMap;
    file_t output_file;
    int32_t threadId;
    vector<instr_t*> *instr_clones;
} per_thread_t;

#define IN
#define INOUT
#define OUT

file_t gFlagF;

/******************************************************************
 * Cached trace implementation of Zerospy for acceleration:
 *  1. For each memory load, we only cache the cct, loaded value, 
 * and the statically encoded information (size, isApprox, offset).
 * If we use data-centric mode, we also record the target address.
 *  2. Note that the previous tracing (caching) will not include any
 * arithmetic operations with any changes to arithmetic flags, so we
 * don't need to heavily reserve/restore the states. Even though it 
 * may result in better data locality when we analyze the loaded data
 * on the fly, we detect some existing bugs and lack of SIMD register
 * reservation supports of drreg, we still directly cache these values
 * for further offline/buffer-clearing analysis. And we can also 
 * benifit from frequent spilling to reserve arithmetic flags when 
 * register pressure is high.
 *  3. For sampling, we will analyze and store the cached traces when
 * the cache is full or the sampling flag is changed to inactive. We 
 * will discard the cache when the sampling flag is not active.
 * ***************************************************************/

// #include "detect.h"

file_t gFile;
FILE* gJson;
static void *gLock;
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

/****************************************************************************************/
inline __attribute__((always_inline)) uint64_t count_zero_bytemap_int8(uint8_t * addr) {
    return addr[0]==0?1:0;
    // register uint8_t xx = *((uint8_t*)addr);
    // // reduce by bits until byte level
    // // xx = xx | (xx>>1) | (xx>>2) | (xx>>3) | (xx>>4) | (xx>>5) | (xx>>6) | (xx>>7);
    // xx = xx | (xx>>1);
    // xx = xx | (xx>>2);
    // xx = xx | (xx>>4);
    // // now xx is byte level reduced, check if it is zero and mask the unused bits
    // xx = (~xx) & 0x1;
    // return xx;
}
inline __attribute__((always_inline)) uint64_t count_zero_bytemap_int16(uint8_t * addr) {
    register uint16_t xx = *((uint16_t*)addr);
    // reduce by bits until byte level
    // xx = xx | (xx>>1) | (xx>>2) | (xx>>3) | (xx>>4) | (xx>>5) | (xx>>6) | (xx>>7);
    xx = xx | (xx>>1);
    xx = xx | (xx>>2);
    xx = xx | (xx>>4);
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
    // xx = xx | (xx>>1) | (xx>>2) | (xx>>3) | (xx>>4) | (xx>>5) | (xx>>6) | (xx>>7);
    xx = xx | (xx>>1);
    xx = xx | (xx>>2);
    xx = xx | (xx>>4);
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
    // xx = xx | (xx>>1) | (xx>>2) | (xx>>3) | (xx>>4) | (xx>>5) | (xx>>6) | (xx>>7);
    xx = xx | (xx>>1);
    xx = xx | (xx>>2);
    xx = xx | (xx>>4);
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
/*******************************************************************************************/
inline __attribute__((always_inline)) void AddINTRedLog(uint64_t ctxt_hndl, uint64_t accessLen, uint64_t redZero, uint64_t fred, uint64_t redByteMap, per_thread_t* pt) {
    //uint64_t key = MAKE_KEY(ctxt_hndl, 0/*elementSize, not used*/, accessLen);
    INTRedLogMap_t::iterator it = pt->INTRedLogMap->find(ctxt_hndl);
    if(it==pt->INTRedLogMap->end()) {
        INTRedLog_t log_ptr = { ENCODE_TO_UPPER(accessLen, accessLen), redZero, fred, redByteMap };
        (*pt->INTRedLogMap)[ctxt_hndl] = log_ptr;
    } else {
        it->second.tot += accessLen;
        it->second.red += redZero;
        it->second.fred += fred;
        it->second.redByteMap &= redByteMap;
    }
}

static const unsigned char BitCountTable4[] __attribute__ ((aligned(64))) = {
    0, 0, 1, 2
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

template<int start, int end, int step>
struct UnrolledFunctions {
    static inline __attribute__((always_inline)) int getFullyRedundantZeroFP(void* pval) {
        switch(step) {
            case 4:
                return (((*reinterpret_cast<uint32_t*>(((uint8_t*)pval)+start))&0x7fffffffLL)==0?1:0)
                    + UnrolledFunctions<start+step, end, step>::getFullyRedundantZeroFP(pval);
            case 8:
                return (((*reinterpret_cast<uint64_t*>(((uint8_t*)pval)+start))&0x7fffffffffffffffLL)==0?1:0)
                    + UnrolledFunctions<start+step, end, step>::getFullyRedundantZeroFP(pval);
        }
        assert(0);
        return 0;
    }
    static inline __attribute__((always_inline)) void mergeRedByteMapFP(void* redByteMap, void* pval) {
        switch(step) {
            case 4:
                reinterpret_cast<uint32_t*>(redByteMap)[start] |= reinterpret_cast<uint32_t*>(pval)[start];
                break;
            case 8:
                reinterpret_cast<uint64_t*>(redByteMap)[start] |= reinterpret_cast<uint64_t*>(pval)[start];
                break;
            default:
                assert(0 && "Unknown type size!\n");
        }
        UnrolledFunctions<start+step, end, step>::mergeRedByteMapFP(redByteMap, pval);
    }
    static inline __attribute__((always_inline)) void memcpy(void* dst, void* src) {
        switch(step) {
            case 4:
                reinterpret_cast<uint32_t*>(dst)[start] = reinterpret_cast<uint32_t*>(src)[start];
                break;
            case 8:
                reinterpret_cast<uint64_t*>(dst)[start] = reinterpret_cast<uint64_t*>(src)[start];
                break;
            default:
                assert(0 && "Unknown type size!\n");
        }
        UnrolledFunctions<start+step, end, step>::memcpy(dst, src);
    }
    static __attribute__((always_inline)) uint64_t BodyRedNum(uint64_t rmap){
        static_assert(start < end);
        if(step==1)
            return ((start==0) ? (rmap&0x1) : ((rmap>>start)&0x1)) + (UnrolledFunctions<start+step,end,step>::BodyRedNum(rmap));
        else if(step==2)
            return ((start==0) ? BitCountTable4[rmap&0x3] : BitCountTable4[(rmap>>start)&0x3]) + (UnrolledFunctions<start+step,end,step>::BodyRedNum(rmap));
        else if(step==4)
            return ((start==0) ? BitCountTable16[rmap&0xf] : BitCountTable16[(rmap>>start)&0xf]) + (UnrolledFunctions<start+step,end,step>::BodyRedNum(rmap));
        else if(step==8)
            return ((start==0) ? BitCountTable256[rmap&0xff] : BitCountTable256[(rmap>>start)&0xff]) + (UnrolledFunctions<start+step,end,step>::BodyRedNum(rmap));
        return 0;
    }
    static __attribute__((always_inline)) uint64_t BodyRedMapFP(uint8_t* addr){
        if(step==4)
            return count_zero_bytemap_fp((void*)(addr+start)) | (UnrolledFunctions<start+step,end,step>::BodyRedMapFP(addr)<<SP_MAP_SIZE);
        else if(step==8)
            return count_zero_bytemap_dp((void*)(addr+start)) | (UnrolledFunctions<start+step,end,step>::BodyRedMapFP(addr)<<DP_MAP_SIZE);
        else
            assert(0 && "Not Supportted floating size! now only support for FP32 or FP64.");
        return 0;
    }
    static __attribute__((always_inline)) uint64_t BodyHasRedundancy(uint8_t* addr){
        if(step==4)
            return hasRedundancy_fp((void*)(addr+start)) || (UnrolledFunctions<start+step,end,step>::BodyHasRedundancy(addr));
        else if(step==8)
            return hasRedundancy_dp((void*)(addr+start)) || (UnrolledFunctions<start+step,end,step>::BodyHasRedundancy(addr));
        else
            assert(0 && "Not Supportted floating size! now only support for FP32 or FP64.");
        return 0;
    }
};

template<int end, int step>
struct UnrolledFunctions<end, end, step> {
    static inline __attribute__((always_inline)) int getFullyRedundantZeroFP(void* pval) {
        return 0;
    }
    static inline __attribute__((always_inline)) void mergeRedByteMapFP(void* redByteMap, void* pval) {
        return ;
    }
    static inline __attribute__((always_inline)) void memcpy(void* dst, void* src) {
        return ;
    }
    static inline __attribute__((always_inline)) uint64_t BodyRedNum(uint64_t rmap) {
        return 0;
    }
    static __attribute__((always_inline)) uint64_t BodyRedMapFP(uint8_t* addr){
        return 0;
    }
    static __attribute__((always_inline)) uint64_t BodyHasRedundancy(uint8_t* addr){
        return 0;
    }
};

template<int esize, int size>
inline __attribute__((always_inline)) void AddFPRedLog(uint64_t ctxt_hndl, void* pval, per_thread_t* pt) {
    //uint64_t key = MAKE_KEY(ctxt_hndl, esize, size);
    FPRedLogMap_t::iterator it = pt->FPRedLogMap->find(ctxt_hndl);
    bool hasRedundancy = UnrolledFunctions<0, size, esize>::BodyHasRedundancy((uint8_t*)pval);
    if(it==pt->FPRedLogMap->end()) {
        FPRedLog_t log_ptr;
        log_ptr.ftot = ENCODE_TO_UPPER(size, size/esize);
        if(hasRedundancy) {
            uint64_t fred = UnrolledFunctions<0, size, esize>::getFullyRedundantZeroFP(pval);
            log_ptr.fred = ENCODE_TO_UPPER(esize, fred);
            log_ptr.redByteMap = UnrolledFunctions<0,size,esize>::BodyRedMapFP((uint8_t*)pval);
        } else {
            log_ptr.fred = ENCODE_TO_UPPER(esize, 0);
            log_ptr.redByteMap = 0;
        }
        //UnrolledFunctions<0, size, esize>::memcpy(log_ptr.redByteMap, pval);
        (*pt->FPRedLogMap)[ctxt_hndl] = log_ptr;
    } else {
        it->second.ftot += size/esize;
        if(hasRedundancy) {
            it->second.fred += UnrolledFunctions<0, size, esize>::getFullyRedundantZeroFP(pval);
            if(it->second.redByteMap) {
                it->second.redByteMap &= UnrolledFunctions<0,size,esize>::BodyRedMapFP((uint8_t*)pval);
            }
        }
    }
}

template<int accessLen, int elementSize>
inline __attribute__((always_inline))
void CheckAndInsertIntPage_impl(int32_t ctxt_hndl, void* addr, per_thread_t *pt) {
    // update info
    uint8_t* bytes = reinterpret_cast<uint8_t*>(addr);
    if(bytes[accessLen-1]!=0) {
        // the log have already been clear to 0, so we do nothing here and quick return.
        AddINTRedLog(ctxt_hndl, accessLen, 0, 0, 0, pt);
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
        AddINTRedLog(ctxt_hndl, accessLen, redZero, redZero, redByteMap, pt);
    } else {
        // accessLen == elementSize
        uint64_t redByteMap_2 = (~redByteMap) & ((1LL<<accessLen)-1);
        uint64_t redZero = _lzcnt_u64(redByteMap_2) - (64-accessLen);
        AddINTRedLog(ctxt_hndl, accessLen, redZero, redZero==accessLen?1:0, redByteMap, pt);
    }
#else
    uint64_t redZero = UnrolledFunctions<0, accessLen, elementSize>::BodyRedNum(redByteMap);
    if(elementSize==1) {
        AddINTRedLog(ctxt_hndl, accessLen, redZero, redZero, redByteMap, pt);
    } else {
        AddINTRedLog(ctxt_hndl, accessLen, redZero, redZero==accessLen?1:0, redByteMap, pt);
    }
#endif
}

#ifdef X86
template<uint32_t AccessLen, uint32_t ElemLen, bool isApprox, bool enable_sampling>
void CheckNByteValueAfterVGather(int slot, instr_t* instr)
{
    static_assert(ElemLen==4 || ElemLen==8);
    void *drcontext = dr_get_current_drcontext();
    context_handle_t ctxt_hndl = drcctlib_get_context_handle(drcontext, slot);
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
        app_pc addr;
        bool is_write;
        uint32_t pos;
        for( int index=0; instr_compute_address_ex_pos(instr, &mcontext, index, &addr, &is_write, &pos); ++index ) {
            DR_ASSERT(!is_write);
            AddFPRedLog<ElemLen, ElemLen>(ctxt_hndl, addr, pt);
        }
    } else {
        assert(0 && "VGather should be a floating point operation!");
    }
}
#define HANDLE_VGATHER(T, ACCESS_LEN, ELEMENT_LEN, IS_APPROX) do {\
if(op_enable_sampling.get_value()) { \
dr_insert_clean_call(drcontext, bb, ins, (void *)CheckNByteValueAfterVGather<(ACCESS_LEN), (ELEMENT_LEN), (IS_APPROX), true>, false, 2, OPND_CREATE_CCT_INT(slot), OPND_CREATE_INTPTR(ins_clone)); \
} else { \
dr_insert_clean_call(drcontext, bb, ins, (void *)CheckNByteValueAfterVGather<(ACCESS_LEN), (ELEMENT_LEN), (IS_APPROX), false>, false, 2, OPND_CREATE_CCT_INT(slot), OPND_CREATE_INTPTR(ins_clone)); \
} } while(0)
#endif
/***************************************************************************************/
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
/*******************************************************************************************/

template<int size, int esize, bool is_float>
void trace_update_cb(val_info_t *info) {
    per_thread_t* pt = (per_thread_t *)drmgr_get_tls_field(dr_get_current_drcontext(), tls_idx);
    if(is_float) {
        AddFPRedLog<esize, size>(info->ctxt_hndl, (void*)info->val, pt);
    } else {
        CheckAndInsertIntPage_impl<size, esize>(info->ctxt_hndl, (void*)info->val, pt);
    }
}

void debug_output_line() {
    dr_fprintf(STDOUT, "--------------\n"); fflush(stdout);
}

void debug_output(void* val) {
    dr_fprintf(STDOUT, "loaded val=%p\n", val); fflush(stdout);
}

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
                printf("\nERROR: refSize for floating point instruction is too small: %d!\n", refSize);
                printf("^^ Disassembled Instruction ^^^\n");
                disassemble(drcontext, instr_get_app_pc(ins), 1/*sdtout file desc*/);
                printf("^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^\n");
                fflush(stdout);
                assert(0 && "memory read floating data with unexptected small size");
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
                printf("\nERROR: refSize for floating point instruction is too large: %d!\n", refSize);
                printf("^^ Disassembled Instruction ^^^\n");
                disassemble(drcontext, instr_get_app_pc(ins), 1/*sdtout file desc*/);
                printf("^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^\n");
                fflush(stdout);
                assert(0 && "unexpected large memory read\n"); break;
        }
    }
#endif
};

#ifdef X86
void InstrumentVGather(void* drcontext, instrlist_t *bb, instr_t* instr, int32_t slot)
{
    // We use instr_compute_address_ex_pos to handle gather (with VSIB addressing)
    ZerospyInstrument::InstrumentReadValueBeforeVGather(drcontext, bb, instr, slot);
}
#endif

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
    } else {
        dr_fprintf(pt->output_file, "[ZEROSPY INFO] Sampling Disabled\n");
    }
}

static void
ClientThreadStart(void *drcontext)
{
    // assert(dr_get_itimer(ITIMER_REAL));
    per_thread_t *pt = (per_thread_t *)dr_thread_alloc(drcontext, sizeof(per_thread_t));
    if (pt == NULL) {
        ZEROSPY_EXIT_PROCESS("pt == NULL");
    }
    pt->INTRedLogMap = new INTRedLogMap_t();
    pt->FPRedLogMap = new FPRedLogMap_t();
    // pt->FPRedLogMap->rehash(10000000);
    // pt->INTRedLogMap->rehash(10000000);
    pt->instr_clones = new vector<instr_t*>();
    drmgr_set_tls_field(drcontext, tls_idx, (void *)pt);
    // init output files
    ThreadOutputFileInit(pt);
}

/*******************************************************************/
/* Output functions */
struct RedundacyData {
    context_handle_t cntxt;
    uint64_t frequency;
    uint64_t all0freq;
    uint64_t ltot;
    uint64_t byteMap;
    uint8_t accessLen;
};

struct ApproxRedundacyData {
    context_handle_t cntxt;
    uint64_t all0freq;
    uint64_t ftot;
    uint64_t byteMap;
    uint8_t accessLen;
    uint8_t size;
};

static inline bool RedundacyCompare(const struct RedundacyData &first, const struct RedundacyData &second) {
    return first.frequency > second.frequency ? true : false;
}
static inline bool ApproxRedundacyCompare(const struct ApproxRedundacyData &first, const struct ApproxRedundacyData &second) {
    return first.all0freq > second.all0freq ? true : false;
}
//#define SKIP_SMALLACCESS
#ifdef SKIP_SMALLACCESS
#define LOGGING_THRESHOLD 100
#endif

static uint64_t PrintRedundancyPairs(per_thread_t *pt, uint64_t threadBytesLoad, int threadId) 
{
    vector<RedundacyData> tmpList;
    vector<RedundacyData>::iterator tmpIt;

    file_t gTraceFile = pt->output_file;
    
    uint64_t grandTotalRedundantBytes = 0;
    tmpList.reserve(pt->INTRedLogMap->size());
    dr_fprintf(STDOUT, "Dumping INTEGER Redundancy Info...\n");
    uint64_t count = 0, rep = -1, num = 0;
    for(auto it=pt->INTRedLogMap->begin(); it!=pt->INTRedLogMap->end(); ++it) {
        ++count;
        if(100 * count / (pt->INTRedLogMap->size())!=rep) {
            rep = 100 * count / (pt->INTRedLogMap->size());
            dr_fprintf(STDOUT, "\r%ld%%  Finish",rep); fflush(stdout);
        }
        uint64_t tot = DECODE_SRC(it->second.tot);
        uint8_t accessLen = DECODE_UPPER(it->second.tot);
        RedundacyData tmp = { it->first, it->second.red,        it->second.fred,
                              tot,       it->second.redByteMap, accessLen };
        tmpList.push_back(tmp);
        grandTotalRedundantBytes += tmp.frequency;
        ++num;
    }
    dr_fprintf(STDOUT, "\r100%%  Finish, Total num = %ld\n", count); fflush(stdout);
    if(count == 0) {
        dr_fprintf(STDOUT, "Warning: No valid profiling data is logged!\n");
        return 0;
    }

    dr_fprintf(gTraceFile, "\n--------------- Dumping INTEGER Redundancy Info ----------------\n");
    dr_fprintf(gTraceFile, "\n*************** Dump Data from Thread %d ****************\n", threadId);
    
    __sync_fetch_and_add(&grandTotBytesRedLoad,grandTotalRedundantBytes);
    dr_fprintf(STDOUT, "Extracted Raw data, now sorting...\n"); fflush(stdout);
    
    dr_fprintf(gTraceFile, "\n Total redundant bytes = %f %%\n", grandTotalRedundantBytes * 100.0 / threadBytesLoad);
    dr_fprintf(gTraceFile, "\n INFO : Total redundant bytes = %f %% (%ld / %ld) \n", grandTotalRedundantBytes * 100.0 / threadBytesLoad, grandTotalRedundantBytes, threadBytesLoad);

    if(grandTotalRedundantBytes==0) {
        dr_fprintf(gTraceFile, "\n------------ Dumping INTEGER Redundancy Info Finish -------------\n");
        dr_fprintf(STDOUT, "INTEGER Report dumped\n"); fflush(stdout);
        return grandTotalRedundantBytes;
    }
    
#ifdef ENABLE_FILTER_BEFORE_SORT
#define FILTER_THESHOLD 1000
    dr_fprintf(gTraceFile, "\n Filter out small redundancies according to the predefined threshold: %.2lf %%\n", 100.0/(double)FILTER_THESHOLD);
    vector<RedundacyData> tmpList2;
    tmpList2 = move(tmpList);
    tmpList.clear(); // make sure it is empty
    tmpList.reserve(tmpList2.size());
    for(tmpIt = tmpList2.begin();tmpIt != tmpList2.end(); ++tmpIt) {
        if(tmpIt->frequency * FILTER_THESHOLD > tmpIt->ltot) {
            tmpList.push_back(*tmpIt);
        }
    }
    dr_fprintf(gTraceFile, " Remained Redundancies: %ld (%.2lf %%)\n", tmpList.size(), (double)tmpList.size()/(double)tmpList2.size());
    tmpList2.clear();
#undef FILTER_THRESHOLD
#endif

    sort(tmpList.begin(), tmpList.end(), RedundacyCompare);
    dr_fprintf(STDOUT, "Sorted, Now generating reports...\n"); fflush(stdout);
    int cntxtNum = 0;
    for (vector<RedundacyData>::iterator listIt = tmpList.begin(); listIt != tmpList.end(); ++listIt) {
        if (cntxtNum < MAX_REDUNDANT_CONTEXTS_TO_LOG) {
            dr_fprintf(gTraceFile, "\n\n======= (%f) %% of total Redundant, with local redundant %f %% (%ld Bytes / %ld Bytes) ======\n", 
                (*listIt).frequency * 100.0 / grandTotalRedundantBytes,
                (*listIt).frequency * 100.0 / (*listIt).ltot,
                (*listIt).frequency,(*listIt).ltot);

            dr_fprintf(gTraceFile, "\n\n======= with All Zero Redundant %f %% (%ld / %ld) ======\n", 
                (*listIt).all0freq * (*listIt).accessLen * 100.0 / (*listIt).ltot,
                (*listIt).all0freq,(*listIt).ltot/(*listIt).accessLen);

            dr_fprintf(gTraceFile, "\n======= Redundant byte map : [0] ");

            dr_fprintf(gTraceFile, " [AccessLen=%d] =======\n", (*listIt).accessLen);

            dr_fprintf(gTraceFile, "\n---------------------Redundant load with---------------------------\n");
            drcctlib_print_backtrace(gTraceFile, (*listIt).cntxt, true, true, MAX_DEPTH);
        }
        else {
            break;
        }
        cntxtNum++;
    }
    dr_fprintf(gTraceFile, "\n------------ Dumping INTEGER Redundancy Info Finish -------------\n");
    dr_fprintf(STDOUT, "INTEGER Report dumped\n"); fflush(stdout);
    return grandTotalRedundantBytes;
}

static uint64_t PrintApproximationRedundancyPairs(per_thread_t *pt, uint64_t threadBytesLoad, int threadId) 
{
    vector<ApproxRedundacyData> tmpList;
    vector<ApproxRedundacyData>::iterator tmpIt;
    tmpList.reserve(pt->FPRedLogMap->size());
    
    file_t gTraceFile = pt->output_file;
    uint64_t grandTotalRedundantBytes = 0;

    dr_fprintf(STDOUT, "Dumping FLOATING POINT Redundancy Info...\n");
    uint64_t count = 0, rep = -1, num = 0;
    for (auto it=pt->FPRedLogMap->begin(); it!=pt->FPRedLogMap->end(); ++it) {
        ++count;
        if(100 * count / (pt->FPRedLogMap->size())!=rep) {
            rep = 100 * count / (pt->FPRedLogMap->size());
            dr_fprintf(STDOUT, "\r%ld%%  Finish",rep); fflush(stdout);
        }
        uint8_t accessLen = DECODE_UPPER(it->second.ftot);
        uint8_t elementSize = DECODE_UPPER(it->second.fred);
        uint64_t redByteMap = it->second.redByteMap;
        ApproxRedundacyData tmp = { it->first,
                                    DECODE_SRC(it->second.fred),
                                    DECODE_SRC(it->second.ftot),
                                    redByteMap,
                                    accessLen,
                                    elementSize };
        tmpList.push_back(tmp);
        grandTotalRedundantBytes += DECODE_SRC(it->second.fred) * accessLen;
        ++num;
    }
    dr_fprintf(STDOUT, "\r100%%  Finish, Total num = %ld\n", count); fflush(stdout);
    if(count == 0) {
        dr_fprintf(STDOUT, "Warning: No valid profiling data is logged!\n");
        return 0;
    }

    dr_fprintf(gTraceFile, "\n--------------- Dumping Approximation Redundancy Info ----------------\n");
    dr_fprintf(gTraceFile, "\n*************** Dump Data(delta=%.2f%%) from Thread %d ****************\n", delta*100,threadId);
    
    __sync_fetch_and_add(&grandTotBytesApproxRedLoad,grandTotalRedundantBytes);
    
    dr_fprintf(gTraceFile, "\n Total redundant bytes = %f %%\n", grandTotalRedundantBytes * 100.0 / threadBytesLoad);
    dr_fprintf(gTraceFile, "\n INFO : Total redundant bytes = %f %% (%ld / %ld) \n", grandTotalRedundantBytes * 100.0 / threadBytesLoad, grandTotalRedundantBytes, threadBytesLoad);

    if(grandTotalRedundantBytes==0) {
        dr_fprintf(gTraceFile, "\n------------ Dumping Approximation Redundancy Info Finish -------------\n");
        printf("Floating Point Report dumped\n");
        return 0;
    }
    
#ifdef ENABLE_FILTER_BEFORE_SORT
#define FILTER_THESHOLD 1000
    dr_fprintf(gTraceFile, "\n Filter out small redundancies according to the predefined threshold: %.2lf %%\n", 100.0/(double)FILTER_THESHOLD);
    // pt->FPRedMap->clear();
    vector<ApproxRedundacyData> tmpList2;
    tmpList2 = move(tmpList);
    tmpList.clear(); // make sure it is empty
    tmpList.reserve(tmpList2.size());
    for(tmpIt = tmpList2.begin();tmpIt != tmpList2.end(); ++tmpIt) {
        if(tmpIt->all0freq * FILTER_THESHOLD > tmpIt->ftot) {
            tmpList.push_back(*tmpIt);
        }
    }
    dr_fprintf(gTraceFile, " Remained Redundancies: %ld (%.2lf %%)\n", tmpList.size(), (double)tmpList.size()/(double)tmpList2.size());
    tmpList2.clear();
#undef FILTER_THRESHOLD
#endif

    sort(tmpList.begin(), tmpList.end(), ApproxRedundacyCompare);
    int cntxtNum = 0;
    for (vector<ApproxRedundacyData>::iterator listIt = tmpList.begin(); listIt != tmpList.end(); ++listIt) {
        if (cntxtNum < MAX_REDUNDANT_CONTEXTS_TO_LOG) {
            dr_fprintf(gTraceFile, "\n======= (%f) %% of total Redundant, with local redundant %f %% (%ld Zeros / %ld Reads) ======\n",
                (*listIt).all0freq * 100.0 / grandTotalRedundantBytes,
                (*listIt).all0freq * 100.0 / (*listIt).ftot,
                (*listIt).all0freq,(*listIt).ftot);
            
            dr_fprintf(gTraceFile, "\n======= Redundant byte map : [ sign | exponent | mantissa ] ========\n");
            if((*listIt).size==4) {
                dr_fprintf(gTraceFile, "%s", getFpRedMapString_SP((*listIt).byteMap, (*listIt).accessLen/4).c_str());
            } else {
                dr_fprintf(gTraceFile, "%s", getFpRedMapString_DP((*listIt).byteMap, (*listIt).accessLen/8).c_str());
            }
            dr_fprintf(gTraceFile, "\n===== [AccessLen=%d, typesize=%d] =======\n", (*listIt).accessLen, (*listIt).size);
            dr_fprintf(gTraceFile, "\n---------------------Redundant load with---------------------------\n");
            drcctlib_print_backtrace(gTraceFile, (*listIt).cntxt, true, true, MAX_DEPTH);
        }
        else {
            break;
        }
        cntxtNum++;
    }
    dr_fprintf(gTraceFile, "\n------------ Dumping Approximation Redundancy Info Finish -------------\n");
    printf("Floating Point Report dumped\n");
    fflush(stdout);
    return grandTotalRedundantBytes;
}
/*******************************************************************/
static uint64_t getThreadByteLoad(per_thread_t *pt) {
    register uint64_t x = 0;
    for (auto it=pt->INTRedLogMap->begin(); it!=pt->INTRedLogMap->end(); ++it) {
        x += DECODE_SRC(it->second.tot);
    }
    for (auto it=pt->FPRedLogMap->begin(); it!=pt->FPRedLogMap->end(); ++it) {
        x += DECODE_SRC(it->second.ftot) * DECODE_UPPER(it->second.ftot);
    }
    return x;
}
/*******************************************************************/

static void
ClientThreadEnd(void *drcontext)
{
#ifdef TIMING
    uint64_t time = get_miliseconds();
#endif
    per_thread_t *pt = (per_thread_t *)drmgr_get_tls_field(drcontext, tls_idx);
    uint64_t threadByteLoad = getThreadByteLoad(pt);
    if(threadByteLoad!=0) 
    {
        __sync_fetch_and_add(&grandTotBytesLoad,threadByteLoad);
        int32_t threadId = pt->threadId;
        uint64_t threadRedByteLoadINT = PrintRedundancyPairs(pt, threadByteLoad, threadId);
        uint64_t threadRedByteLoadFP = PrintApproximationRedundancyPairs(pt, threadByteLoad, threadId);
#ifdef TIMING
        time = get_miliseconds() - time;
        printf("Thread %d: Time %ld ms for generating outputs\n", threadId, time);
#endif

        dr_mutex_lock(gLock);
        dr_fprintf(gFile, "\n#THREAD %d Redundant Read:", threadId);
        dr_fprintf(gFile, "\nTotalBytesLoad: %lu ",threadByteLoad);
        dr_fprintf(gFile, "\nRedundantBytesLoad: %lu %.2f",threadRedByteLoadINT, threadRedByteLoadINT * 100.0/threadByteLoad);
        dr_fprintf(gFile, "\nApproxRedundantBytesLoad: %lu %.2f\n",threadRedByteLoadFP, threadRedByteLoadFP * 100.0/threadByteLoad);

        dr_mutex_unlock(gLock);
    }
    dr_close_file(pt->output_file);
    for(size_t i=0;i<pt->instr_clones->size();++i) {
        instr_destroy(drcontext, (*pt->instr_clones)[i]);
    }
    delete pt->instr_clones;
    delete pt->INTRedLogMap;
    delete pt->FPRedLogMap;
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

    pid_t pid = getpid();
#ifdef ARM_CCTLIB
    char name[MAXIMUM_PATH] = "arm-";
#else
    char name[MAXIMUM_PATH] = "x86-";
#endif
    gethostname(name + strlen(name), MAXIMUM_PATH - strlen(name));
    sprintf(name + strlen(name), "-%d-zerospy", pid);
    g_folder_name.assign(name, strlen(name));
    mkdir(g_folder_name.c_str(), S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH);

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
    if (dr_using_all_private_caches()) {
        dr_fprintf(STDOUT, "[ZEROSPY INFO] Thread Private is enabled.\n");
        dr_fprintf(gFile,  "[ZEROSPY INFO] Thread Private is enabled.\n");
    } else {
        dr_fprintf(STDOUT, "[ZEROSPY INFO] Thread Private is disabled.\n");
        dr_fprintf(gFile,  "[ZEROSPY INFO] Thread Private is disabled.\n");
    }
    if (op_help.get_value()) {
        dr_fprintf(STDOUT, "%s\n", droption_parser_t::usage_long(DROPTION_SCOPE_CLIENT).c_str());
        exit(1);
    }
#ifdef ZEROSPY_DEBUG
    sprintf(name+strlen(name), ".debug");
    gDebug = dr_open_file(name, DR_FILE_WRITE_OVERWRITE | DR_FILE_ALLOW_LARGE);
    DR_ASSERT(gDebug != INVALID_FILE);
#endif
    gFlagF = dr_open_file("debug.log", DR_FILE_WRITE_OVERWRITE | DR_FILE_ALLOW_LARGE);
    DR_ASSERT(gFlagF != INVALID_FILE);
}

static void
ClientExit(void)
{
    dr_fprintf(gFile, "\n#Redundant Read:");
    dr_fprintf(gFile, "\nTotalBytesLoad: %lu \n",grandTotBytesLoad);
    dr_fprintf(gFile, "\nRedundantBytesLoad: %lu %.2f\n",grandTotBytesRedLoad, grandTotBytesRedLoad * 100.0/grandTotBytesLoad);
    dr_fprintf(gFile, "\nApproxRedundantBytesLoad: %lu %.2f\n",grandTotBytesApproxRedLoad, grandTotBytesApproxRedLoad * 100.0/grandTotBytesLoad);

#ifndef _WERROR
    if(warned) {
        dr_fprintf(gFile, "####################################\n");
        dr_fprintf(gFile, "WARNING: some unexpected instructions are ignored. Please check zerospy.log.warn for detail.\n");
        dr_fprintf(gFile, "####################################\n");
    }
#endif
    dr_close_file(gFlagF);
    dr_close_file(gFile);
    fclose(gJson);
    if (!dr_raw_tls_cfree(tls_offs, INSTRACE_TLS_COUNT)) {
        ZEROSPY_EXIT_PROCESS(
            "ERROR: zerospy dr_raw_tls_calloc fail");
    }

    dr_mutex_destroy(gLock);
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
    dr_set_client_name("DynamoRIO Client 'zerospy'",
                       "http://dynamorio.org/issues");
    ClientInit(argc, argv);

    if(!vprofile_init(ZEROSPY_FILTER_READ_MEM_ACCESS_INSTR, NULL, NULL, NULL, VPROFILE_COLLECT_CCT)) {
        ZEROSPY_EXIT_PROCESS("ERROR: zerospy unable to initialize vprofile");
    }

    if(op_enable_sampling.get_value()) {
        vtracer_enable_sampling(op_window_enable.get_value(), op_window.get_value());
    }

    drmgr_priority_t thread_init_pri = { sizeof(thread_init_pri),
                                         "zerospy-thread-init", NULL, NULL,
                                         DRCCTLIB_THREAD_EVENT_PRI + 1 };
    drmgr_priority_t thread_exit_pri = { sizeof(thread_exit_pri),
                                         "zerospy-thread-exit", NULL, NULL,
                                         DRCCTLIB_THREAD_EVENT_PRI - 1 };

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

    vtrace = vprofile_allocate_trace(VPROFILE_TRACE_VAL_CCT);

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