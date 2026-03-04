#ifndef L1_CACHE_H
#define L1_CACHE_H

#include <unordered_set>
#include <string>
#include <random>
#include <deque>

#include "cache_common.h"
#include "vprofile.h"
#include "l2_cache.h"  // 依赖L2，处理缺失时的请求转发

// L1类型枚举（区分I/D缓存）
enum class L1Type {
    INSTRUCTION,  // L1I
    DATA          // L1D
};

class L1Cache {
public:
    bool print_flag;
    // 构造函数：输入缓存大小（16KB/32KB/64KB）、L2引用、L1类型
    L1Cache(uint32_t size_kb, L1Type type, L2Cache* l2, ShadowPageTable* spt, file_t);
    ~L1Cache();

    // test
    void print_here(val_info_t *info);
    
    void load(uint64_t addr, int32_t cct, int32_t read_bytes, int32_t tid);
    void store(uint64_t addr, int32_t cct, int32_t write_bytes, int32_t tid);

    // void load(uint64_t addr, int32_t cct, int32_t tid);
    // void store(uint64_t addr, int32_t cct, int32_t tid);

    void print_result(int64_t, int64_t total_store_cnt, int64_t memory_ins_cnt, int32_t tid);
    
private:

    uint32_t cache_size;
    L1Type l1_type;
    std::vector<std::vector<CacheLine>> cache_sets;  // cache_sets[index][way] = CacheLine
    L2Cache* l2;
    ShadowPageTable* spt;

    uint64_t total_ins_cnt;

    int64_t l1_miss_cnt;
    int64_t l1_load_miss_cnt;
    int64_t l1_store_miss_cnt;
    int64_t l1_coherence_miss_cnt;
    int64_t l1_capacity_miss_cnt;
    int64_t l1_conflict_miss_cnt;
    
    // std::unordered_map<uint64_t, int64_t> pc_l1_miss_map;
    std::unordered_map<int32_t, int64_t> cct_coherence_miss_map;
    std::unordered_map<int32_t, int64_t> cct_capacity_miss_map;
    std::unordered_map<int32_t, int64_t> cct_conflict_miss_map;

    std::unordered_map<uint64_t, CacheBump> cache_bump_map;
    std::vector<CacheBump> cache_bump_list;

    std::deque<uint64_t> prefetch_prefix_deque;
    std::unordered_map<uint64_t, int32_t> prefetch_prefix_map;
    // output result file_t
    file_t output_file;

    uint32_t insert_cacheline(uint64_t addr, uint64_t tag, uint32_t group_index, int32_t tid, uint64_t* swap_out_addr);
    
    // uint32_t find_lru(uint32_t group_index);
    // void update_lru(uint32_t group_index, uint32_t way);
    
    int32_t find_freecacheline(uint32_t group_index, int32_t tid);
    bool find_cacheline(uint32_t index, uint64_t tag, uint32_t& way);

    // prefetch 
    bool check_prefetch(uint64_t new_addr);

    void print_total_info(int64_t total_load_cnt, int64_t total_store_cnt, int64_t memory_ins_cnt);
    void print_miss_cnt_topk(int topk, std::unordered_map<int32_t, int64_t>& cct_miss_map, CacheMissReason cache_reason, int32_t tid);
    void print_cache_bump(int topk, int num_print_event);
    void print_cache_bump_fast(int topk, int num_print_event);
    // void print_cache_bump(int topk);

    void clean_steal_cache_bump_map();
};

#endif // L1_CACHE_H