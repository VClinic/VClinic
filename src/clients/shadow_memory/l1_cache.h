#ifndef L1_CACHE_H
#define L1_CACHE_H

#include <random>
#include "cache_common.h"
#include "vprofile.h"
// #include "L2Cache.h"  // 依赖L2，处理缺失时的请求转发

// L1类型枚举（区分I/D缓存）
enum class L1Type {
    INSTRUCTION,  // L1I
    DATA          // L1D
};

class L1Cache {
public:
    // 构造函数：输入缓存大小（16KB/32KB/64KB，{insert\_element\_16\_}）、L2引用、L1类型
    L1Cache(uint32_t size_kb, L1Type type);

    // 测试初始化
    void print_here(val_info_t *info);
    
    // 核心操作：读/写（返回是否命中）
    void read(val_info_t *info);  // 读操作（如L1I取指、L1D加载）
    // bool write(uint64_t phys_addr, const uint8_t* in_data, uint32_t data_len); // 写操作（仅L1D支持）
    
    // 辅助接口：处理L2的snoop请求（一致性同步，{insert\_element\_17\_}）
    // MESIState handle_snoop(uint64_t phys_addr);  // snoop时返回当前行状态，并可能置为无效

private:
    // 私有成员：缓存大小、L2引用、L1类型、缓存结构（二维数组：组→路→缓存行）
    uint32_t cache_size;
    L1Type l1_type;
    std::vector<std::vector<CacheLine>> cache_sets;  // cache_sets[index][way] = CacheLine
    uint32_t* group_cap;
    
    // 私有方法：核心逻辑封装
    void insert_cacheline(CacheLine new_line, uint32_t group_index);
    uint32_t find_freecacheline(uint32_t group_index);
    bool find_cacheline(uint32_t index, uint64_t tag, uint32_t& way);  // 查找行，返回是否命中
    // void allocate_line(uint64_t phys_addr, uint32_t index);  // 分配新行（处理缺失时的行填充）
    // void evict_line(uint32_t index);  // 驱逐行（LRU算法，需根据L1类型与L2交互）
    void update_lru(uint32_t index, uint32_t way);  // 更新LRU计数器
};

#endif // L1_CACHE_H