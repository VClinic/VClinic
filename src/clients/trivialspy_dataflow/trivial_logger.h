#ifndef __TRIVIAL_LOGGER_H__
#define __TRIVIAL_LOGGER_H__

#include <stdint.h>
#include <vector>
#include <unordered_map>

#include "trace.h"
#include "trivial_define.h"
#include "dataflow.h"

/**************************************************************************************************
 * To avoid calculating hash/reallocation while logging with data structures like unordered_map, we
 * use data structures like page maping, where key (like page id) is (ctxt_hndl<<2 | opnd_idx). 
 * For lower memory overhead, we use page size of 1024 elements (s.t. the actual size is ~40KB).
 * Empirically, we want align at OS-level page to hold the logged data, which can be further tuned.
 **************************************************************************************************/
struct __TrivialCondLog {
    uint64_t opnd_id;
    uint64_t trivialNum[Attribute_t::Attribute_Count];
};

// In X86, the maximum oprand number of a instruction is 3
// Log the triviality as the operation level
struct TrivialOpCondLog {
    uint64_t totalNum;
    // 4 operands at maximum
    __TrivialCondLog oprand[4];
};

#define GET_OPIDX(opid) ((opid)&0x3)

#define GET_PAGEID(key) ((key)>>10)
#define GET_PAGE_CHID(key) ((key)&((1<<10)-1))
#define TRIVIALLOG_PAGE_SIZE 1024
// ~ 2^23 = 8M entries for each
#define TRIVIAL_LOG_PAGE_NUM ((((size_t)CONTEXT_HANDLE_MAX)+TRIVIALLOG_PAGE_SIZE-1)/TRIVIALLOG_PAGE_SIZE)

#define DECODE_CTXT(page_id, page_chid) ((page_id)<<10 | (page_chid))

struct log_space_t {
    int tls_idx;
    uint tls_offs;
    reg_id_t tls_seg;
    void *gLock;
    int thread_id_max;
    uint64_t totalCost;
    uint64_t trivial_metrics[5];
    uint64_t soft_approx_metrics[5];
    uint64_t hard_approx_metrics[5];
    uint64_t* bb_ref;
};

static log_space_t global_log_space;

enum {
    RAW_LOG_STATIC_LOG_NUM,
    RAW_LOG_COUNTS
};

struct TrivialLog {
    uint64_t total;
    uint64_t trivial;
    uint64_t soft_approx;
    uint64_t hard_approx;
    int8_t temp_tot;
    int8_t temp_tri;
    int8_t temp_sft;
    int8_t temp_hrd;
};

struct per_thread_log_t {
    TrivialLog* log;
    trivial_entry_list_t* trivial_entry_list;
    combined_trivial_entry_list_t* comb_telist;
    uint64_t totalCost;
    uint64_t* bb_ref;
    int threadId;
};

static int _bb_idx = -1;
static int get_bb_idx() {
    return __sync_add_and_fetch(&_bb_idx, 1);
}

static void update_global_bb_ref_from_tls(void* drcontext) {
    per_thread_log_t *pt = (per_thread_log_t *)drmgr_get_tls_field(drcontext, global_log_space.tls_idx);
    for(int i=0; i<MAX_DFGLOG_NUM; ++i) {
        if(pt->bb_ref[i]) {
            __sync_fetch_and_add(&global_log_space.bb_ref[i], pt->bb_ref[i]);
        }
    }
}

static inline __attribute__((always_inline))
void TrivialLoggerThreadInit(void* drcontext, bool enable_approx_soft, bool enable_approx_hard, bool no_trace) {
    per_thread_log_t* pt = (per_thread_log_t *)dr_thread_alloc(drcontext, sizeof(per_thread_log_t));
    pt->totalCost = 0;
    pt->trivial_entry_list = new trivial_entry_list_t();
    pt->comb_telist = new combined_trivial_entry_list_t();
    if(!no_trace) {
        pt->log = (TrivialLog*)dr_raw_mem_alloc(MAX_DFGLOG_NUM*sizeof(TrivialLog), DR_MEMPROT_READ | DR_MEMPROT_WRITE, NULL);
        pt->bb_ref = 0;
    } else {
        pt->log = 0;
        pt->bb_ref = (uint64_t*)dr_raw_mem_alloc(MAX_DFGLOG_NUM*sizeof(uint64_t), DR_MEMPROT_READ | DR_MEMPROT_WRITE, NULL);
    }
    // store the thread id for final report
    pt->threadId = dr_atomic_add32_return_sum(&global_log_space.thread_id_max, 1);
    drmgr_set_tls_field(drcontext, global_log_space.tls_idx, (void *)pt);
}

static inline __attribute__((always_inline))
void TrivialLoggerThreadFini(void* drcontext) {
    per_thread_log_t *pt = (per_thread_log_t *)drmgr_get_tls_field(drcontext, global_log_space.tls_idx);
    telist_clear(pt->trivial_entry_list);
    comb_telist_clear(pt->comb_telist);
    delete pt->trivial_entry_list;
    delete pt->comb_telist;
    if(pt->log) {
        dr_raw_mem_free(pt->log, MAX_DFGLOG_NUM*sizeof(TrivialLog));
    }
    if(pt->bb_ref) {
        update_global_bb_ref_from_tls(drcontext);
        dr_raw_mem_free(pt->bb_ref, MAX_DFGLOG_NUM*sizeof(uint64_t));
    }
    // free thread data
    dr_thread_free(drcontext, pt, sizeof(per_thread_log_t));
}

static inline __attribute__((always_inline))
int getThreadId() {
    per_thread_log_t *pt = (per_thread_log_t *)drmgr_get_tls_field(dr_get_current_drcontext(), global_log_space.tls_idx);
    return pt->threadId;
}

static inline __attribute__((always_inline))
void accumulateGlobalMetric(void* drcontext) {
    per_thread_log_t *pt = (per_thread_log_t *)drmgr_get_tls_field(drcontext, global_log_space.tls_idx);
    __sync_fetch_and_add(&global_log_space.totalCost, pt->totalCost);
}

bool TrivialLoggerInit(bool no_trace) {
    int tls_idx;
    uint tls_offs;
    reg_id_t tls_seg;

    /* allocate raw TLS so we can access it from the code cache */
    if (!dr_raw_tls_calloc(&tls_seg, &tls_offs, RAW_LOG_COUNTS, 0))
        return false;

    tls_idx = drmgr_register_tls_field();
    if (tls_idx == -1)
        return false;

    global_log_space.tls_idx = tls_idx;
    global_log_space.tls_offs= tls_offs;
    global_log_space.tls_seg = tls_seg;
    global_log_space.thread_id_max = -1;
    memset(global_log_space.trivial_metrics, 0, 4*sizeof(uint64_t));
    memset(global_log_space.soft_approx_metrics, 0, 4*sizeof(uint64_t));
    memset(global_log_space.hard_approx_metrics, 0, 4*sizeof(uint64_t));

    if(no_trace) {
        global_log_space.bb_ref = (uint64_t*)dr_raw_mem_alloc(MAX_DFGLOG_NUM*sizeof(uint64_t), DR_MEMPROT_READ | DR_MEMPROT_WRITE, NULL);
    } else {
        global_log_space.bb_ref = 0;
    }

    global_log_space.gLock = dr_mutex_create();
    return true;
}

bool TrivialLoggerFini() {
    dr_mutex_destroy(global_log_space.gLock);
    if (!drmgr_unregister_tls_field(global_log_space.tls_idx) ||
        !dr_raw_tls_cfree(global_log_space.tls_offs, RAW_LOG_COUNTS))
        return false;
    if(global_log_space.bb_ref) {
        dr_raw_mem_free(global_log_space.bb_ref, MAX_DFGLOG_NUM*sizeof(uint64_t));
    }
    return true;
}

#endif