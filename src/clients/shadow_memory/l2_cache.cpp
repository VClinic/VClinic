#include "l2_cache.h"
#include "cache_common.h"

// 构造函数：初始化L2缓存组（容量支持64KB/128KB/256KB，4路组相联，文档2.7.1节）
L2Cache::L2Cache(uint32_t size_kb) 
    : cache_size(size_kb * 1024) {
    // 计算总组数：总容量 / (行大小 × 路数)（行大小64字节，文档2.7.1节）
    uint32_t num_sets = cache_size / (CACHE_LINE_SIZE * L2_ASSOCIATIVITY);
    // 初始化每组的4个路（默认状态为INVALID）
    cache_sets.resize(num_sets, std::vector<CacheLine>(L2_ASSOCIATIVITY));
}

void L2Cache::ask_l2(uint64_t addr){
    uint64_t tag;
    uint32_t index, offset, way;
    AddressSplitter::split(addr, cache_size, tag, index, offset);

    // find cacheline

    // L2 Hit
    if(){
        // check sm status

        // if sm status is clean --> make l2 cacheline invalid & transfer data to l1
        return;

        // if sm status is dirty --> treated as l2 miss & transfer data to l1
        return;
    }
    // L2 Miss
    else{
        // transfer data to l1
        return;
    }
}

void L2Cache::evicted_from_l1(uint64 addr){
    // assert l2 do not have addr (data cache)

    // L2 has free cache line
    if(){

    }
    // L2 has NO free cache line
    else{
        // find LRU cache line & evict

    }
}