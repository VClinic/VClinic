#ifndef L1_CACHE_H
#define L1_CACHE_H

#include <random>
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
    // 构造函数：输入缓存大小（16KB/32KB/64KB）、L2引用、L1类型
    L1Cache(uint32_t size_kb, L1Type type, L2Cache* l2, ShadowPageTable* spt);
    ~L1Cache();
    
    // test
    void print_here(val_info_t *info);
    
    void load(uint64_t addr, int32_t tid);
    void store(uint64_t addr, int32_t tid);
    
private:

    uint32_t cache_size;
    L1Type l1_type;
    std::vector<std::vector<CacheLine>> cache_sets;  // cache_sets[index][way] = CacheLine
    L2Cache* l2;
    ShadowPageTable* spt;
    
    uint32_t insert_cacheline(uint64_t addr, uint64_t tag, uint32_t group_index, int32_t tid);
    
    uint32_t find_lru(uint32_t group_index);
    void update_lru(uint32_t group_index, uint32_t way);
    
    uint32_t find_freecacheline(uint32_t group_index, int32_t tid);
    bool find_cacheline(uint32_t index, uint64_t tag, uint32_t& way);
};

#endif // L1_CACHE_H