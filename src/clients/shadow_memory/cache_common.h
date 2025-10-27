#ifndef CACHE_COMMON_H
#define CACHE_COMMON_H

#include <cstdint>
#include <vector>
#include <unordered_map>

// 1. 文档定义的硬件参数（2.6.1/2.7.1节）
const uint32_t L1_CACHE_SIZE = 64;          // L1 Cache size 64 KB
const uint32_t L2_CACHE_SIZE = 128;         // L2 Cache size 128 KB
const uint32_t CACHE_LINE_SIZE = 64;       // 所有层级行大小均为64字节（{insert\_element\_0\_}、{insert\_element\_1\_}、{insert\_element\_2\_}）
const uint32_t L1_ASSOCIATIVITY = 4;       // L1I/L1D均为4路组相联（{insert\_element\_3\_}、{insert\_element\_4\_}）
const uint32_t L2_ASSOCIATIVITY = 4;       // L2为4路组相联（{insert\_element\_5\_}）
// const uint32_t L3_ASSOCIATIVITY = 8;       // L3默认8路组相联（文档未明确，参考DSU通用设计）

// 2. MESI状态（2.6.2.5节，{insert\_element\_6\_}~{insert\_element\_7\_}）
enum class MESIState {
    INVALID,    // I：无效（{insert\_element\_8\_}）
    SHARED,     // S：共享干净（{insert\_element\_9\_}）
    EXCLUSIVE,  // E：独占干净（{insert\_element\_10\_}）
    MODIFIED    // M：已修改（{insert\_element\_11\_}）
};

// 3. 缓存行结构（存储Tag、MESI状态、数据、脏位等核心信息）
struct CacheLine {
    uint64_t tag = 0;              // 物理地址的Tag部分（VIPT中L1D用物理Tag，{insert\_element\_12\_}）
    MESIState state = MESIState::INVALID;  // 行状态
    bool is_dirty = false;         // 脏位（仅M状态为true，{insert\_element\_13\_}）
    // uint8_t data[CACHE_LINE_SIZE] = {0};  // 行数据
    uint32_t lru_counter = 0;      // LRU替换算法计数器 (数越大，越久未访问)
};

// 4. 地址拆分工具（将物理地址拆分为Tag、Index、Offset，适配不同缓存大小）
struct AddressSplitter {
    // 输入：物理地址、缓存大小 → 输出：Tag、Index、Offset
    static void split(uint64_t phys_addr, uint32_t cache_size, 
                      uint64_t& tag, uint32_t& index, uint32_t& offset) {
        offset = phys_addr % CACHE_LINE_SIZE;  // 低6位（64字节行）
        uint32_t num_sets = cache_size / (CACHE_LINE_SIZE * L1_ASSOCIATIVITY);  // 总组数=容量/(行大小×路数)
        index = (phys_addr / CACHE_LINE_SIZE) % num_sets;  // Index = 行号 % 总组数
        tag = phys_addr / (CACHE_LINE_SIZE * num_sets);    // Tag = 物理地址 / (行大小×总组数)
    }
};

#endif // CACHE_COMMON_H