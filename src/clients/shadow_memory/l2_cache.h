#ifndef L2_CACHE_H
#define L2_CACHE_H

#include "cache_common.h"
#include "page_table.h"

class L2Cache {
    public:
        // 构造函数：输入L2大小（64KB/128KB/256KB）、L3引用
        L2Cache(uint32_t size_kb, ShadowPageTable* spt);

        void ask_l2(uint64_t addr, int32_t tid);
        void evicted_from_l1(uint64_t addr, int32_t tid);        
    
    private:
        uint32_t cache_size;
        std::vector<std::vector<CacheLine>> cache_sets;  // cache_sets[index][way] = CacheLine
        ShadowPageTable* spt;

        void update_lru(uint32_t group_index, uint32_t way);
        uint32_t find_lru(uint32_t group_index);

        uint32_t find_freecacheline(uint32_t group_index, int32_t tid);
        bool find_cacheline(uint32_t index, uint64_t tag, uint32_t& way);
};

#endif  // L2_CACHE_H