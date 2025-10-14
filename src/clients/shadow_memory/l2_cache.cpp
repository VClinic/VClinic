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