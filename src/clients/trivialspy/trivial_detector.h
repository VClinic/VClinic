#ifndef __TRIVIAL_DETECTOR_H__
#define __TRIVIAL_DETECTOR_H__

#include <stdint.h>
#include "trivial_table.h"

static double epsilon = 1e-4;
static int bit_cnt = 0;

/*********************************************************************
 * IEEE floating point format: val = s * 1.m * 2^(e-127)
 * So we only check for e to ensure it is small enough.
 * For approximation, we use <epsilon_e> to indicate the maximum
 * exponent error allowed (e.g., fabs(val) < 2^(-epsilon_e))
 ********************************************************************/
static uint32_t approx_mask_sp[3] = {0};
static uint64_t approx_mask_dp[3] = {0};

#define MASK_OP_NONE 0
#define MASK_OP_OR   0
#define MASK_OP_AND  1

// exp==0 (e<128-2^bitcnt, lower few bits are masked as 0), mantissa==0 (all masked as 0)
#define APPROX_ZERO_SP (0)
// exp==127, mantissa==0 (lower few bits are masked as 0)
#define APPROX_GE_ONE_SP (0x3f800000)
// exp==126, mantissa==(-1) (lower few bits are masked as 1)
#define APPROX_LT_ONE_SP (0x3f7fffff)

// exp==0 (e<1024-2^bitcnt, lower few bits are masked as 0), mantissa==0 (all masked as 0)
#define APPROX_ZERO_DP ((uint64_t)0)
// exp==1023, mantissa==0 (lower few bits are masked as 0)
#define APPROX_GE_ONE_DP ((uint64_t)0x3ff0000000000000LL)
// exp==1022, mantissa==(-1) (lower few bits are masked as 1)
#define APPROX_LT_ONE_DP ((uint64_t)0x3fefffffffffffffLL)

// soft approximate: ~ eps
inline __attribute__((always_inline))
bool set_approx_soft(double eps) { 
    epsilon = eps; 
    return true;
}

// hard approximate: ~ 2^(-bit_cnt)
inline __attribute__((always_inline))
bool set_approx_hard(int _bit_cnt) {
    // automatic hard approximate mask generation
    bit_cnt = _bit_cnt;
    // single precision
    if(bit_cnt<8) {
        // e<128-2^bitcnt, lower few bits are masked as 0
        approx_mask_sp[0] = 0xc07fffff | (((1<<bit_cnt)-1)<<23);
    } else {
        approx_mask_sp[0] = 0xffffffff;
    }
    if(bit_cnt<=22) {
        approx_mask_sp[1] = (~((1<<(23-bit_cnt))-1));
        approx_mask_sp[2] = (  (1<<(23-bit_cnt))-1);
    } else {
        approx_mask_sp[1] = 0xffffffff;
        approx_mask_sp[2] = 0;
    }
    // double precision
    if(bit_cnt<11) {
        // e<128-2^bitcnt, lower few bits are masked as 0
        approx_mask_dp[0] = 0xc00fffffffffffffLL | (((1LL<<bit_cnt)-1LL)<<52);
    } else {
        approx_mask_dp[0] = 0xffffffffffffffffLL;
    }
    if(bit_cnt<=51) {
        approx_mask_dp[1] = (~((1LL<<(51-bit_cnt))-1LL));
        approx_mask_dp[2] = (  (1LL<<(51-bit_cnt))-1LL);
    } else {
        approx_mask_dp[1] = 0xffffffffffffffffLL;
        approx_mask_dp[2] = 0;
    }
    return true;
}

template<class T, int start, int end, int targ>
struct UnrolledTrivialCounter {
    static inline __attribute__((always_inline))
    int run(T* val_ptr) {
        return (val_ptr[start]==(T)targ?1:0) + UnrolledTrivialCounter<T, start+1, end, targ>::run(val_ptr);
    }
};

template<class T, int end, int targ>
struct UnrolledTrivialCounter<T,end,end,targ> {
    static inline __attribute__((always_inline))
    int run(T* val_ptr) {
        return 0;
    }
};

/* maskOp=true, AND; maskOp=false, OR */
template<class T, int start, int end, uint64_t targ, bool maskOp>
struct ApproxUnrolledTrivialCounter {
    static inline __attribute__((always_inline))
    int run_soft(T* val_ptr, double epsilon) {
        return (fabs(val_ptr[start]-(T)targ)<epsilon?1:0) + ApproxUnrolledTrivialCounter<T, start+1, end, targ, maskOp>::run_soft(val_ptr, epsilon);
    }

    static inline __attribute__((always_inline))
    int run_hard(T* val_ptr, T mask) {
        if(maskOp) {
            return (((val_ptr[start]&mask)==(T)targ)?1:0) + ApproxUnrolledTrivialCounter<T, start+1, end, targ, maskOp>::run_hard(val_ptr, mask);
        } else {
            return (((val_ptr[start]|mask)==(T)targ)?1:0) + ApproxUnrolledTrivialCounter<T, start+1, end, targ, maskOp>::run_hard(val_ptr, mask);
        }
    }
};

template<class T, int end, uint64_t targ, bool maskOp>
struct ApproxUnrolledTrivialCounter<T, end, end, targ, maskOp> {
    static inline __attribute__((always_inline))
    int run_soft(T* val_ptr, double epsilon) {
        return 0;
    }

    static inline __attribute__((always_inline))
    int run_hard(T* val_ptr, T mask) {
        return 0;
    }
};

template<class T, int num, ConditionVal_t cond>
struct TrivialDetector {
    static inline __attribute__((always_inline))
    bool run(void* val_ptr) {
        int trivial_num = 0;
        if(cond==ConditionVal_t::IS_ZERO) {
            trivial_num = UnrolledTrivialCounter<T, 0, num, 0>::run((T*)val_ptr);
        } else if(cond==ConditionVal_t::IS_ONE) {
            trivial_num = UnrolledTrivialCounter<T, 0, num, 1>::run((T*)val_ptr);
        } else if(cond==ConditionVal_t::IS_FULL) {
            trivial_num = UnrolledTrivialCounter<T, 0, num, -1>::run((T*)val_ptr);
        }
        return trivial_num==num;
    }
};

template<class T, int num, ConditionVal_t cond>
struct ApproxTrivialDetectorSoft {
    static inline __attribute__((always_inline))
    bool run(void* val_ptr) {
        int trivial_num = 0;
        if(cond==ConditionVal_t::IS_ZERO) {
            trivial_num = ApproxUnrolledTrivialCounter<T, 0, num, 0, MASK_OP_NONE>::run_soft((T*)val_ptr, epsilon);
        } else if(cond==ConditionVal_t::IS_ONE) {
            trivial_num = ApproxUnrolledTrivialCounter<T, 0, num, 1, MASK_OP_NONE>::run_soft((T*)val_ptr, epsilon);
        }
        return trivial_num==num;
    }
};

template<int num, ConditionVal_t cond>
struct ApproxTrivialDetectorHard {
    static inline __attribute__((always_inline))
    bool run_single(void* vptr) {
        uint32_t* val_ptr = reinterpret_cast<uint32_t*>(vptr);
        int trivial_num = 0;
        if(cond==ConditionVal_t::IS_ZERO) {
            trivial_num = ApproxUnrolledTrivialCounter<uint32_t, 0, num, APPROX_ZERO_SP, MASK_OP_AND>::run_hard(val_ptr, approx_mask_sp[0]);
        } else if(cond==ConditionVal_t::IS_ONE) {
            // val >= 1 Approximations 
            trivial_num = ApproxUnrolledTrivialCounter<uint32_t, 0, num, APPROX_GE_ONE_SP, MASK_OP_AND>::run_hard(val_ptr, approx_mask_sp[1]);
            // val <  1 Approximations 
            trivial_num+= ApproxUnrolledTrivialCounter<uint32_t, 0, num, APPROX_LT_ONE_SP, MASK_OP_OR>::run_hard(val_ptr, approx_mask_sp[2]);
        }
        return trivial_num==num;
    }

    static inline __attribute__((always_inline))
    bool run_double(void* vptr) {
        uint64_t* val_ptr = reinterpret_cast<uint64_t*>(vptr);
        int trivial_num = 0;
        if(cond==ConditionVal_t::IS_ZERO) {
            trivial_num = ApproxUnrolledTrivialCounter<uint64_t, 0, num, APPROX_ZERO_DP, MASK_OP_AND>::run_hard(val_ptr, approx_mask_dp[0]);
        } else if(cond==ConditionVal_t::IS_ONE) {
            // val >= 1 Approximations 
            trivial_num = ApproxUnrolledTrivialCounter<uint64_t, 0, num, APPROX_GE_ONE_DP, MASK_OP_AND>::run_hard(val_ptr, approx_mask_dp[1]);
            // val <  1 Approximations 
            trivial_num+= ApproxUnrolledTrivialCounter<uint64_t, 0, num, APPROX_LT_ONE_DP, MASK_OP_OR>::run_hard(val_ptr, approx_mask_dp[2]);
        }
        return trivial_num==num;
    }
};

typedef bool (*trivial_detector_t)(void*);

#define ENCODE_TEMPLATE_PARA(is_float, size, esize, val) (((is_float)?1LL:0LL) | (uint64_t)(size)<<1 | (uint64_t)(esize)<<16 | (uint64_t)(val)<<28)

/* <is_float:0,1>, <esize:0(1),1(2),2(4),3(8)>, <size:0(1),1(2),2(4),3(8),4(16),5(32)>, <val:0(IS_ZERO),1(IS_ONE),2(IS_FULL)> */
#define get_idx_is_float(is_float) ((is_float)?1:0)
#define get_idx_val(val) ((int)(val)-(int)IS_ZERO)
// esize/size must be exp of 2 (only has single bit set)
#define check_size_valid(size) (__builtin_popcount(size)==1)
#define get_idx_size(size) (__builtin_ctz(size))

#define get_detector_from_table(is_float, esize, size, val) _detector_table[get_idx_is_float(is_float)][get_idx_size(esize)][get_idx_size(size)][get_idx_val(val)]

static trivial_detector_t _detector_table[2][4][6][3][3/*3 mode: trivial, soft, hard*/] = {
    { /*is_float=0 enter*/
    { /* esize=1 enter */
    { /* size=1 enter */
    /*is_float=0, esize=1, size=1, val=IS_ZERO*/
    { TrivialDetector<uint8_t, 1, IS_ZERO>::run, NULL, NULL },
    /*is_float=0, esize=1, size=1, val=IS_ONE*/
    { TrivialDetector<uint8_t, 1, IS_ONE>::run, NULL, NULL },
    /*is_float=0, esize=1, size=1, val=IS_FULL*/
    { TrivialDetector<uint8_t, 1, IS_FULL>::run, NULL, NULL }
    /* size=1 exit */ },
    { /* size=2 enter */
    /*is_float=0, esize=1, size=2, val=IS_ZERO*/
    { TrivialDetector<uint8_t, 2, IS_ZERO>::run, NULL, NULL },
    /*is_float=0, esize=1, size=2, val=IS_ONE*/
    { TrivialDetector<uint8_t, 2, IS_ONE>::run, NULL, NULL },
    /*is_float=0, esize=1, size=2, val=IS_FULL*/
    { TrivialDetector<uint8_t, 2, IS_FULL>::run, NULL, NULL }
    /* size=2 exit */},
    { /* size=4 enter */
    /*is_float=0, esize=1, size=4, val=IS_ZERO*/
    { TrivialDetector<uint8_t, 4, IS_ZERO>::run, NULL, NULL },
    /*is_float=0, esize=1, size=4, val=IS_ONE*/
    { TrivialDetector<uint8_t, 4, IS_ONE>::run, NULL, NULL },
    /*is_float=0, esize=1, size=4, val=IS_FULL*/
    { TrivialDetector<uint8_t, 4, IS_FULL>::run, NULL, NULL }
    /* size=4 exit */},
    { /* size=8 enter */
    /*is_float=0, esize=1, size=8, val=IS_ZERO*/
    { TrivialDetector<uint8_t, 8, IS_ZERO>::run, NULL, NULL },
    /*is_float=0, esize=1, size=8, val=IS_ONE*/
    { TrivialDetector<uint8_t, 8, IS_ONE>::run, NULL, NULL },
    /*is_float=0, esize=1, size=8, val=IS_FULL*/
    { TrivialDetector<uint8_t, 8, IS_FULL>::run, NULL, NULL }
    /* size=8 exit */},
    { /* size=16 enter */
    /*is_float=0, esize=1, size=16, val=IS_ZERO*/
    { TrivialDetector<uint8_t, 16, IS_ZERO>::run, NULL, NULL },
    /*is_float=0, esize=1, size=16, val=IS_ONE*/
    { TrivialDetector<uint8_t, 16, IS_ONE>::run, NULL, NULL },
    /*is_float=0, esize=1, size=16, val=IS_FULL*/
    { TrivialDetector<uint8_t, 16, IS_FULL>::run, NULL, NULL }
    /* size=16 exit */},
    { /* size=32 enter */
    /*is_float=0, esize=1, size=32, val=IS_ZERO*/
    { TrivialDetector<uint8_t, 32, IS_ZERO>::run, NULL, NULL },
    /*is_float=0, esize=1, size=32, val=IS_ONE*/
    { TrivialDetector<uint8_t, 32, IS_ONE>::run, NULL, NULL },
    /*is_float=0, esize=1, size=32, val=IS_FULL*/
    { TrivialDetector<uint8_t, 32, IS_FULL>::run, NULL, NULL }
    /* size=32 exit */}
    /* esize=1 exit */},
    { /* esize=2 enter */
    { /* size=1 enter */
    /*is_float=0, esize=2, size=1, val=* */
    { NULL, NULL, NULL }, { NULL, NULL, NULL }, { NULL, NULL, NULL }
    /* size=1 exit */},
    { /* size=2 enter */
    /*is_float=0, esize=2, size=2, val=IS_ZERO*/
    { TrivialDetector<uint16_t, 1, IS_ZERO>::run, NULL, NULL },
    /*is_float=0, esize=2, size=2, val=IS_ONE*/
    { TrivialDetector<uint16_t, 1, IS_ONE>::run, NULL, NULL },
    /*is_float=0, esize=2, size=2, val=IS_FULL*/
    { TrivialDetector<uint16_t, 1, IS_FULL>::run, NULL, NULL }
    /* size=2 exit */},
    { /* size=4 enter */
    /*is_float=0, esize=2, size=4, val=IS_ZERO*/
    { TrivialDetector<uint16_t, 2, IS_ZERO>::run, NULL, NULL },
    /*is_float=0, esize=2, size=4, val=IS_ONE*/
    { TrivialDetector<uint16_t, 2, IS_ONE>::run, NULL, NULL },
    /*is_float=0, esize=2, size=4, val=IS_FULL*/
    { TrivialDetector<uint16_t, 2, IS_FULL>::run, NULL, NULL }
    /* size=4 exit */},
    { /* size=8 enter */
    /*is_float=0, esize=2, size=8, val=IS_ZERO*/
    { TrivialDetector<uint16_t, 4, IS_ZERO>::run, NULL, NULL },
    /*is_float=0, esize=2, size=8, val=IS_ONE*/
    { TrivialDetector<uint16_t, 4, IS_ONE>::run, NULL, NULL },
    /*is_float=0, esize=2, size=8, val=IS_FULL*/
    { TrivialDetector<uint16_t, 4, IS_FULL>::run, NULL, NULL }
    /* size=8 exit */},
    { /* size=16 enter */
    /*is_float=0, esize=2, size=16, val=IS_ZERO*/
    { TrivialDetector<uint16_t, 8, IS_ZERO>::run, NULL, NULL },
    /*is_float=0, esize=2, size=16, val=IS_ONE*/
    { TrivialDetector<uint16_t, 8, IS_ONE>::run, NULL, NULL },
    /*is_float=0, esize=2, size=16, val=IS_FULL*/
    { TrivialDetector<uint16_t, 8, IS_FULL>::run, NULL, NULL }
    /* size=16 exit */},
    { /* size=32 enter */
    /*is_float=0, esize=2, size=32, val=IS_ZERO*/
    { TrivialDetector<uint16_t, 16, IS_ZERO>::run, NULL, NULL },
    /*is_float=0, esize=2, size=32, val=IS_ONE*/
    { TrivialDetector<uint16_t, 16, IS_ONE>::run, NULL, NULL },
    /*is_float=0, esize=2, size=32, val=IS_FULL*/
    { TrivialDetector<uint16_t, 16, IS_FULL>::run, NULL, NULL }
    /* size=32 exit */}
    /* esize=2 exit */ },
    { /* esize=4 enter */
    { /* size=1 enter */
    /*is_float=0, esize=4, size=1, val=* */
    { NULL, NULL, NULL }, { NULL, NULL, NULL }, { NULL, NULL, NULL }
    /* size=1 exit */},
    { /* size=2 enter */
    /*is_float=0, esize=4, size=2, val=* */
    { NULL, NULL, NULL }, { NULL, NULL, NULL }, { NULL, NULL, NULL }
    /* size=2 exit */},
    { /* size=4 enter */
    /*is_float=0, esize=4, size=4, val=IS_ZERO*/
    { TrivialDetector<uint32_t, 1, IS_ZERO>::run, NULL, NULL },
    /*is_float=0, esize=4, size=4, val=IS_ONE*/
    { TrivialDetector<uint32_t, 1, IS_ONE>::run, NULL, NULL },
    /*is_float=0, esize=4, size=4, val=IS_FULL*/
    { TrivialDetector<uint32_t, 1, IS_FULL>::run, NULL, NULL }
    /* size=4 exit */},
    { /* size=8 enter */
    /*is_float=0, esize=4, size=8, val=IS_ZERO*/
    { TrivialDetector<uint32_t, 2, IS_ZERO>::run, NULL, NULL },
    /*is_float=0, esize=4, size=8, val=IS_ONE*/
    { TrivialDetector<uint32_t, 2, IS_ONE>::run, NULL, NULL },
    /*is_float=0, esize=4, size=8, val=IS_FULL*/
    { TrivialDetector<uint32_t, 2, IS_FULL>::run, NULL, NULL }
    /* size=8 exit */},
    { /* size=16 enter */
    /*is_float=0, esize=4, size=16, val=IS_ZERO*/
    { TrivialDetector<uint32_t, 4, IS_ZERO>::run, NULL, NULL },
    /*is_float=0, esize=4, size=16, val=IS_ONE*/
    { TrivialDetector<uint32_t, 4, IS_ONE>::run, NULL, NULL },
    /*is_float=0, esize=4, size=16, val=IS_FULL*/
    { TrivialDetector<uint32_t, 4, IS_FULL>::run, NULL, NULL }
    /* size=16 exit */},
    { /* size=32 enter */
    /*is_float=0, esize=4, size=32, val=IS_ZERO*/
    { TrivialDetector<uint32_t, 8, IS_ZERO>::run, NULL, NULL },
    /*is_float=0, esize=4, size=32, val=IS_ONE*/
    { TrivialDetector<uint32_t, 8, IS_ONE>::run, NULL, NULL },
    /*is_float=0, esize=4, size=32, val=IS_FULL*/
    { TrivialDetector<uint32_t, 8, IS_FULL>::run, NULL, NULL }
    /* size=32 exit */}
    /* esize=4 exit */ },
    { /* esize=8 enter */
    { /* size=1 enter */
    /*is_float=0, esize=8, size=1, val=* */
    { NULL, NULL, NULL }, { NULL, NULL, NULL }, { NULL, NULL, NULL }
    /* size=1 exit */},
    { /* size=2 enter */
    /*is_float=0, esize=8, size=2, val=* */
    { NULL, NULL, NULL }, { NULL, NULL, NULL }, { NULL, NULL, NULL }
    /* size=2 exit */},
    { /* size=4 enter */
    /*is_float=0, esize=8, size=4, val=* */
    { NULL, NULL, NULL }, { NULL, NULL, NULL }, { NULL, NULL, NULL }
    /* size=4 exit */},
    { /* size=8 enter */
    /*is_float=0, esize=8, size=8, val=IS_ZERO*/
    { TrivialDetector<uint64_t, 1, IS_ZERO>::run, NULL, NULL },
    /*is_float=0, esize=8, size=8, val=IS_ONE*/
    { TrivialDetector<uint64_t, 1, IS_ONE>::run, NULL, NULL },
    /*is_float=0, esize=8, size=8, val=IS_FULL*/
    { TrivialDetector<uint64_t, 1, IS_FULL>::run, NULL, NULL }
    /* size=8 exit */},
    { /* size=16 enter */
    /*is_float=0, esize=8, size=16, val=IS_ZERO*/
    { TrivialDetector<uint64_t, 2, IS_ZERO>::run, NULL, NULL },
    /*is_float=0, esize=8, size=16, val=IS_ONE*/
    { TrivialDetector<uint64_t, 2, IS_ONE>::run, NULL, NULL },
    /*is_float=0, esize=8, size=16, val=IS_FULL*/
    { TrivialDetector<uint64_t, 2, IS_FULL>::run, NULL, NULL }
    /* size=16 exit */},
    { /* size=32 enter */
    /*is_float=0, esize=8, size=32, val=IS_ZERO*/
    { TrivialDetector<uint64_t, 4, IS_ZERO>::run, NULL, NULL },
    /*is_float=0, esize=8, size=32, val=IS_ONE*/
    { TrivialDetector<uint64_t, 4, IS_ONE>::run, NULL, NULL },
    /*is_float=0, esize=8, size=32, val=IS_FULL*/
    { TrivialDetector<uint64_t, 4, IS_FULL>::run, NULL, NULL }
    /* size=32 exit */}
    /* esize=8 exit */ }
    /* is_float=0 exit */},
    { /* is_float=1 enter */
    { /* esize=1 enter */
    { /* size=1 enter */
    /*is_float=1, esize=1, size=1, val=* */
    { NULL, NULL, NULL }, { NULL, NULL, NULL }, { NULL, NULL, NULL }
    /* size=1 exit */},
    { /* size=2 enter */
    /*is_float=1, esize=1, size=2, val=* */
    { NULL, NULL, NULL }, { NULL, NULL, NULL }, { NULL, NULL, NULL }
    /* size=2 exit */},
    { /* size=4 enter */
    /*is_float=1, esize=1, size=4, val=* */
    { NULL, NULL, NULL }, { NULL, NULL, NULL }, { NULL, NULL, NULL }
    /* size=4 exit */},
    { /* size=8 enter */
    /*is_float=1, esize=1, size=8, val=* */
    { NULL, NULL, NULL }, { NULL, NULL, NULL }, { NULL, NULL, NULL }
    /* size=8 exit */},
    { /* size=16 enter */
    /*is_float=1, esize=1, size=16, val=* */
    { NULL, NULL, NULL }, { NULL, NULL, NULL }, { NULL, NULL, NULL }
    /* size=16 exit */},
    { /* size=32 enter */
    /*is_float=1, esize=1, size=32, val=* */
    { NULL, NULL, NULL }, { NULL, NULL, NULL }, { NULL, NULL, NULL }
    /* size=32 exit */}
    /* esize=1 exit */},
    { /* esize=2 enter */
    { /* size=1 enter */
    /*is_float=1, esize=2, size=1, val=* */
    { NULL, NULL, NULL }, { NULL, NULL, NULL }, { NULL, NULL, NULL }
    /* size=1 exit */},
    { /* size=2 enter */
    /*is_float=1, esize=2, size=2, val=* */
    { NULL, NULL, NULL }, { NULL, NULL, NULL }, { NULL, NULL, NULL }
    /* size=2 exit */},
    { /* size=4 enter */
    /*is_float=1, esize=2, size=4, val=* */
    { NULL, NULL, NULL }, { NULL, NULL, NULL }, { NULL, NULL, NULL }
    /* size=4 exit */},
    { /* size=8 enter */
    /*is_float=1, esize=2, size=8, val=* */
    { NULL, NULL, NULL }, { NULL, NULL, NULL }, { NULL, NULL, NULL }
    /* size=8 exit */},
    { /* size=16 enter */
    /*is_float=1, esize=2, size=16, val=* */
    { NULL, NULL, NULL }, { NULL, NULL, NULL }, { NULL, NULL, NULL }
    /* size=16 exit */},
    { /* size=32 enter */
    /*is_float=1, esize=2, size=32, val=* */
    { NULL, NULL, NULL }, { NULL, NULL, NULL }, { NULL, NULL, NULL }
    /* size=32 exit */}
    /* esize=2 exit */ },
    { /* esize=4 enter */
    { /* size=1 enter */
    /*is_float=1, esize=4, size=1, val=* */
    { NULL, NULL, NULL }, { NULL, NULL, NULL }, { NULL, NULL, NULL }
    /* size=1 exit */},
    { /* size=2 enter */
    /*is_float=1, esize=4, size=2, val=* */
    { NULL, NULL, NULL }, { NULL, NULL, NULL }, { NULL, NULL, NULL }
    /* size=2 exit */},
    { /* size=4 enter */
    /*is_float=1, esize=4, size=4, val=IS_ZERO*/
    { TrivialDetector<float, 1, IS_ZERO>::run, ApproxTrivialDetectorSoft<float, 1, IS_ZERO>::run, ApproxTrivialDetectorHard<1, IS_ZERO>::run_single },
    /*is_float=1, esize=4, size=4, val=IS_ONE*/
    { TrivialDetector<float, 1, IS_ONE>::run, ApproxTrivialDetectorSoft<float, 1, IS_ONE>::run, ApproxTrivialDetectorHard<1, IS_ONE>::run_single },
    /*is_float=1, esize=4, size=4, val=IS_FULL*/
    { TrivialDetector<float, 1, IS_FULL>::run, ApproxTrivialDetectorSoft<float, 1, IS_FULL>::run, ApproxTrivialDetectorHard<1, IS_FULL>::run_single }
    /* size=4 exit */},
    { /* size=8 enter */
    /*is_float=1, esize=4, size=8, val=IS_ZERO*/
    { TrivialDetector<float, 2, IS_ZERO>::run, ApproxTrivialDetectorSoft<float, 2, IS_ZERO>::run, ApproxTrivialDetectorHard<2, IS_ZERO>::run_single },
    /*is_float=1, esize=4, size=8, val=IS_ONE*/
    { TrivialDetector<float, 2, IS_ONE>::run, ApproxTrivialDetectorSoft<float, 2, IS_ONE>::run, ApproxTrivialDetectorHard<2, IS_ONE>::run_single },
    /*is_float=1, esize=4, size=8, val=IS_FULL*/
    { TrivialDetector<float, 2, IS_FULL>::run, ApproxTrivialDetectorSoft<float, 2, IS_FULL>::run, ApproxTrivialDetectorHard<2, IS_FULL>::run_single }
    /* size=8 exit */},
    { /* size=16 enter */
    /*is_float=1, esize=4, size=16, val=IS_ZERO*/
    { TrivialDetector<float, 4, IS_ZERO>::run, ApproxTrivialDetectorSoft<float, 4, IS_ZERO>::run, ApproxTrivialDetectorHard<4, IS_ZERO>::run_single },
    /*is_float=1, esize=4, size=16, val=IS_ONE*/
    { TrivialDetector<float, 4, IS_ONE>::run, ApproxTrivialDetectorSoft<float, 4, IS_ONE>::run, ApproxTrivialDetectorHard<4, IS_ONE>::run_single },
    /*is_float=1, esize=4, size=16, val=IS_FULL*/
    { TrivialDetector<float, 4, IS_FULL>::run, ApproxTrivialDetectorSoft<float, 4, IS_FULL>::run, ApproxTrivialDetectorHard<4, IS_FULL>::run_single }
    /* size=16 exit */},
    { /* size=32 enter */
    /*is_float=1, esize=4, size=32, val=IS_ZERO*/
    { TrivialDetector<float, 8, IS_ZERO>::run, ApproxTrivialDetectorSoft<float, 8, IS_ZERO>::run, ApproxTrivialDetectorHard<8, IS_ZERO>::run_single },
    /*is_float=1, esize=4, size=32, val=IS_ONE*/
    { TrivialDetector<float, 8, IS_ONE>::run, ApproxTrivialDetectorSoft<float, 8, IS_ONE>::run, ApproxTrivialDetectorHard<8, IS_ONE>::run_single },
    /*is_float=1, esize=4, size=32, val=IS_FULL*/
    { TrivialDetector<float, 8, IS_FULL>::run, ApproxTrivialDetectorSoft<float, 8, IS_FULL>::run, ApproxTrivialDetectorHard<8, IS_FULL>::run_single }
    /* size=32 exit */}
    /* esize=4 exit */ },
    { /* esize=8 enter */
    { /* size=1 enter */
    /*is_float=1, esize=8, size=1, val=* */
    { NULL, NULL, NULL }, { NULL, NULL, NULL }, { NULL, NULL, NULL }
    /* size=1 exit */},
    { /* size=2 enter */
    /*is_float=1, esize=8, size=2, val=* */
    { NULL, NULL, NULL }, { NULL, NULL, NULL }, { NULL, NULL, NULL }
    /* size=2 exit */},
    { /* size=4 enter */
    /*is_float=1, esize=8, size=4, val=* */
    { NULL, NULL, NULL }, { NULL, NULL, NULL }, { NULL, NULL, NULL }
    /* size=4 exit */},
    { /* size=8 enter */
    /*is_float=1, esize=8, size=8, val=IS_ZERO*/
    { TrivialDetector<double, 1, IS_ZERO>::run, ApproxTrivialDetectorSoft<double, 1, IS_ZERO>::run, ApproxTrivialDetectorHard<1, IS_ZERO>::run_double },
    /*is_float=1, esize=8, size=8, val=IS_ONE*/
    { TrivialDetector<double, 1, IS_ONE>::run, ApproxTrivialDetectorSoft<double, 1, IS_ONE>::run, ApproxTrivialDetectorHard<1, IS_ONE>::run_double },
    /*is_float=1, esize=8, size=8, val=IS_FULL*/
    { TrivialDetector<double, 1, IS_FULL>::run, ApproxTrivialDetectorSoft<double, 1, IS_FULL>::run, ApproxTrivialDetectorHard<1, IS_FULL>::run_double }
    /* size=8 exit */},
    { /* size=16 enter */
    /*is_float=1, esize=8, size=16, val=IS_ZERO*/
    { TrivialDetector<double, 2, IS_ZERO>::run, ApproxTrivialDetectorSoft<double, 2, IS_ZERO>::run, ApproxTrivialDetectorHard<2, IS_ZERO>::run_double },
    /*is_float=1, esize=8, size=16, val=IS_ONE*/
    { TrivialDetector<double, 2, IS_ONE>::run, ApproxTrivialDetectorSoft<double, 2, IS_ONE>::run, ApproxTrivialDetectorHard<2, IS_ONE>::run_double },
    /*is_float=1, esize=8, size=16, val=IS_FULL*/
    { TrivialDetector<double, 2, IS_FULL>::run, ApproxTrivialDetectorSoft<double, 2, IS_FULL>::run, ApproxTrivialDetectorHard<2, IS_FULL>::run_double }
    /* size=16 exit */},
    { /* size=32 enter */
    /*is_float=1, esize=8, size=32, val=IS_ZERO*/
    { TrivialDetector<double, 4, IS_ZERO>::run, ApproxTrivialDetectorSoft<double, 4, IS_ZERO>::run, ApproxTrivialDetectorHard<4, IS_ZERO>::run_double },
    /*is_float=1, esize=8, size=32, val=IS_ONE*/
    { TrivialDetector<double, 4, IS_ONE>::run, ApproxTrivialDetectorSoft<double, 4, IS_ONE>::run, ApproxTrivialDetectorHard<4, IS_ONE>::run_double },
    /*is_float=1, esize=8, size=32, val=IS_FULL*/
    { TrivialDetector<double, 4, IS_FULL>::run, ApproxTrivialDetectorSoft<double, 4, IS_FULL>::run, ApproxTrivialDetectorHard<4, IS_FULL>::run_double }
    /* size=32 exit */}
    /* esize=8 exit */ }
    /* is_float=1 exit */ }
};

struct TrivialDetectorTable {
    inline __attribute__((always_inline))
    static trivial_detector_t* get(ConditionVal_t val, int size, int esize, bool is_float) {
#ifdef DEBUG
        DPRINTF("Query: val=%s, size=%d, esize=%d, is_float=%d\n", getConditionValString(val), size, esize, is_float);
        DPRINTF("==> [is_float=%d][esize=%d][size=%d][val=%d]\n", get_idx_is_float(is_float), get_idx_size(esize), get_idx_size(size), get_idx_val(val));
        assert(check_size_valid(size));
        assert(check_size_valid(esize));
        assert(get_idx_size(esize)<4);
        assert(get_idx_size(size)<6);
#endif
        static_assert(get_idx_val(IS_ZERO)==0);
        static_assert(get_idx_val(IS_ONE)==1);
        static_assert(get_idx_val(IS_FULL)==2);
        return get_detector_from_table(is_float, esize, size, val);
    }
};

enum {
    TRIVIAL_DETECTOR_EXACT=0,
    TRIVIAL_DETECTOR_APPROX_SOFT=1,
    TRIVIAL_DETECTOR_APPROX_HARD=2
};

trivial_detector_t getTrivialDetector(ConditionVal_t val, ConditionListInfo_t info, int mode) {
    DR_ASSERT(mode==TRIVIAL_DETECTOR_EXACT || mode==TRIVIAL_DETECTOR_APPROX_SOFT || mode==TRIVIAL_DETECTOR_APPROX_HARD);
    return TrivialDetectorTable::get(val, info.size, info.esize, info.is_float)[mode];
}

#endif