#ifndef CACHE_COMMON_H
#define CACHE_COMMON_H

#include <cstdint>
#include <vector>
#include <unordered_map>

#include "page_table.h"

// 1. 文档定义的硬件参数（2.6.1/2.7.1节）
const uint32_t L1_CACHE_SIZE = 64;          // L1 Cache size 64 KB
const uint32_t L2_CACHE_SIZE = 128;         // L2 Cache size 128 KB
const uint32_t CACHE_LINE_SIZE = 64;       // 所有层级行大小均为64字节（{insert\_element\_0\_}、{insert\_element\_1\_}、{insert\_element\_2\_}）
const uint32_t L1_ASSOCIATIVITY = 4;       // L1I/L1D均为4路组相联（{insert\_element\_3\_}、{insert\_element\_4\_}）
const uint32_t L2_ASSOCIATIVITY = 4;       // L2为4路组相联（{insert\_element\_5\_}）
// const uint32_t L3_ASSOCIATIVITY = 8;       // L3默认8路组相联（文档未明确，参考DSU通用设计）
const uint64_t L1_INS_CNT_GATE = 10000;
const uint8_t L1_CONFLICT_MISS_GATE = 1;
const uint64_t L1_CACHE_BUMP_GATE = 200;

// data prefetch
const size_t PREFETCH_CAPACITY = 40;

enum class CacheMissReason {
    COHERENCE,
    CAPACITY,
    CONFLICT
};

// 3. 缓存行结构（存储Tag、MESI状态、数据、脏位等核心信息）
struct CacheLine {
    uint64_t addr = 0;
    uint64_t tag = 0;              // 物理地址的Tag部分（VIPT中L1D用物理Tag）
    ShadowPage* sp = nullptr;
    bool empty = true;
    
    uint32_t lru_counter = 0;      // LRU替换算法计数器 (数越大，越久未访问)

    uint64_t last_miss_ins_cnt = 0; // 上一次缺失时间（指令计数值）
    uint8_t accu_miss_cnt = 0;      // 缺失累计（用于判断是否超过阈值）
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

struct CacheBump {
    uint64_t addr;
    
    uint64_t out_ins_cnt;
    int32_t out_cct;
    CacheMissReason out_reason;

    uint64_t in_ins_cnt;
    int32_t in_cct;
    CacheMissReason in_reason;
};

inline std::string reason_to_string(CacheMissReason reason) {
    switch (reason) {
        case CacheMissReason::COHERENCE: return "一致性缺失";
        case CacheMissReason::CAPACITY:   return "容量缺失";
        case CacheMissReason::CONFLICT:   return "冲突缺失";
        default: return "未知缺失";
    }
}

const std::string COHERENCE_MISS_OPTIMIZE = R"(
Coherence Miss（一致性缺失）

一致性缺失源于多线程并发访问同一缓存行，分为 true sharing（真共享） 和 false sharing（伪共享），且可能由应用程序或内存分配器导致，改进方法需区分类型和来源：

1. True Sharing（真共享）
 - 核心思路：减少共享变量的更新频率或缩小共享范围。
 - 具体方法：通过重构代码，用线程局部变量（thread-local variables）或局部变量替代频繁更新的共享变量，降低多线程对同一缓存行的写冲突；若必须共享，尽量减少对共享变量的访问次数。

2. False Sharing（伪共享）
 - 应用程序导致的伪共享：对数据结构进行 填充（padding），确保不同线程访问的变量处于不同缓存行（避免因缓存行对齐导致无关变量共享缓存行）；或使用按线程私有页面（per-thread private pages）隔离不同线程的数据。
 - 分配器导致的伪共享：切换内存分配器（如将默认的 glibc 分配器替换为 TCMalloc，其内存分配机制可避免不同线程的对象分配到同一缓存行）；或在应用层调整数据结构的对齐方式，强制不同线程的对象分属不同缓存行。


)";

const std::string CAPACITY_MISS_OPTIMIZE = R"(
Conflict Miss（冲突缺失）

冲突缺失源于 N 路组相联缓存中，超过 N 个缓存行映射到同一缓存组，导致频繁替换，可能由应用程序的访问模式或内存分配器的对象布局导致：

1. 应用程序导致的冲突缺失
 - 调整循环顺序：优化嵌套循环的访问顺序，使内存访问模式与缓存组映射规则匹配（如 Kripke 应用中，切换循环顺序可避免连续访问同一缓存组）。
 - 数据结构填充：对数组或结构体进行填充，改变其内存地址，使不同数据映射到不同缓存组，避免集中映射到同一组。
 - 调整对象起始地址：通过代码优化改变数据对象的内存分配起始地址，分散缓存组映射压力。

2. 分配器导致的冲突缺失
 - 切换内存分配器：选择对缓存友好的分配器（如 TCMalloc 相比 glibc 分配器，可通过批量分配和地址对齐避免多个小对象映射到同一缓存组）。
 - 应用层规避：在应用中插入 “伪内存分配” 或调整分配对象的大小，间接改变对象的内存地址，分散缓存组映射。


)";

const std::string CONFLICT_MISS_OPTIMIZE = R"(
Capacity Miss（容量缺失）

容量缺失源于程序的工作集（active data）超过缓存容量，导致缓存无法容纳所有必要数据，需通过优化数据访问范围和模式减少缓存压力：

1. 循环优化
 - 循环融合（Loop Fusion）：将多个独立循环合并为一个，减少跨循环的数据重复加载（如将 “读取数组 Alpha” 和 “计算数组 Beta” 的两个循环融合，避免 Alpha 被逐出缓存后再次加载）。
 - 循环分块（Loop Tiling）：将大数组的访问拆分为小块（块大小适配缓存容量），确保每块数据在处理期间可完全驻留缓存，减少重复加载。

 2. 数组重组（Array Regrouping）
 - 将访问频率高的相关数据重组到连续内存区域，减少缓存行的无效加载（如 IRS 应用中，将多个独立数组的相关元素合并，使一次缓存行加载可覆盖多个所需数据，降低总访问量）。

3. 减少工作集大小
 - 优化数据结构，移除不必要的全局变量或大对象；或通过算法优化，缩小程序运行时的活跃数据规模（如避免在循环中频繁访问超大数组的无关部分）。


)";

#endif // CACHE_COMMON_H