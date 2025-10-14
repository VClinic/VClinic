#ifndef L1_CACHE_H
#define L1_CACHE_H

#include "cache_common.h"

class L2Cache {
    public:
        // 构造函数：输入L2大小（64KB/128KB/256KB，{insert\_element\_24\_}）、L3引用
        L2Cache(uint32_t size_kb);
        
        // 1. 向L1提供行数据（L1缺失时调用）
        // MESIState provide_line(uint64_t phys_addr, uint8_t* out_data);
        
        // 2. 接收L1驱逐的行（L1驱逐时调用，{insert\_element\_25\_}）
        // void accept_evicted_line(uint64_t phys_addr, const uint8_t* in_data, MESIState state);
        
        // 3. 释放行（L1D加载后调用，严格互斥要求，{insert\_element\_26\_}）
        // void release_line(uint64_t phys_addr);
    
    private:
        // 私有成员：缓存大小、L2引用、L1类型、缓存结构（二维数组：组→路→缓存行）
        uint32_t cache_size;
        std::vector<std::vector<CacheLine>> cache_sets;  // cache_sets[index][way] = CacheLine
        // 私有逻辑：与L3交互（L2缺失时从L3加载，驱逐时写回L3）
        // bool load_from_l3(uint64_t phys_addr, CacheLine& line);
        // void write_to_l3(uint64_t phys_addr, const CacheLine& line);
};

#endif