#include "page_table.h"
#include <iostream>
#include <exception>
// 索引掩码函数
inline uint64_t page_base_of(uint64_t addr) { return addr & ~(uint64_t)(PAGE_SIZE - 1); }
inline uint64_t offset_in_page(uint64_t addr) { return addr & (PAGE_SIZE - 1); }
inline size_t line_index_of(uint64_t addr) { return (offset_in_page(addr) / CACHE_LINE); }

bool ShadowPage::is_dirty(uint64_t addr, int32_t tid) {
    try{
        size_t li = line_index_of(addr);
        uint64_t mask = (1ULL << li);
        return (thread_dirty_bitmap[tid].load(std::memory_order_acquire) & mask) != 0;
    }catch (const std::exception& e) { 
        // 打印异常信息：e.what() 返回描述性字符串
        std::cerr << "捕获到标准异常：" << e.what() << std::endl;
    }
    exit(0);
}

bool ShadowPage::read(uint64_t addr, int32_t tid) {
    size_t li = line_index_of(addr);
    uint64_t mask = (1ULL << li);
    uint64_t dirty = thread_dirty_bitmap[tid].fetch_and(~mask, std::memory_order_acquire);
    return (dirty & mask) != 0;
}

bool ShadowPage::write(uint64_t addr, int32_t tid) {
    size_t li = line_index_of(addr);
    uint64_t mask = (1ULL << li);
    uint64_t dirty = 0;
    for (int i = 0; i < MAX_THREADS; i++) {
        if (i == tid) {
            dirty = thread_dirty_bitmap[tid].load();
        } else {
            thread_dirty_bitmap[i].fetch_or(mask, std::memory_order_acquire);
        }
    }
    return (dirty & mask) != 0;
}

ShadowPage* ShadowPageTable::find_page(uint64_t addr) {
    size_t i1 = (addr >> (OFFSET_BITS + L2_BITS + L3_BITS)) & ((1ULL<<L1_BITS)-1);
    size_t i2 = (addr >> (OFFSET_BITS + L3_BITS)) & ((1ULL<<L2_BITS)-1);
    size_t i3 = (addr >> OFFSET_BITS) & ((1ULL<<L3_BITS)-1);
    void* p2 = l1->entries[i1].load(std::memory_order_acquire);
    if (!p2) return nullptr;
    PTNode* node2 = static_cast<PTNode*>(p2);

    void* p3 = node2->entries[i2].load(std::memory_order_acquire);
    if (!p3) return nullptr;
    PTNode* node3 = static_cast<PTNode*>(p3);

    void* leaf = node3->entries[i3].load(std::memory_order_acquire);
    return static_cast<ShadowPage*>(leaf);
}


ShadowPage* ShadowPageTable::get_or_create_page(uint64_t addr) {
    size_t i1 = (addr >> (OFFSET_BITS + L2_BITS + L3_BITS)) & ((1ULL<<L1_BITS)-1);
    size_t i2 = (addr >> (OFFSET_BITS + L3_BITS)) & ((1ULL<<L2_BITS)-1);
    size_t i3 = (addr >> OFFSET_BITS) & ((1ULL<<L3_BITS)-1);

    PTNode* node2 = static_cast<PTNode*>(l1->entries[i1].load(std::memory_order_acquire));
    if (!node2) {
        PTNode* expect = nullptr;
        PTNode* newnode = new PTNode(L2_SIZE);
        if (!l1->entries[i1].compare_exchange_strong((void*&)expect, (void*)newnode)) {
            delete newnode;
            node2 = static_cast<PTNode*>(l1->entries[i1].load(std::memory_order_acquire));
        } else {
            node2 = newnode;
        }
    }

    // 创建 L3
    PTNode* node3 = static_cast<PTNode*>(node2->entries[i2].load(std::memory_order_acquire));
    if (!node3) {
        PTNode* expect = nullptr;
        PTNode* newnode = new PTNode(L3_SIZE);
        if (!node2->entries[i2].compare_exchange_strong((void*&)expect, (void*)newnode)) {
            delete newnode;
            node3 = static_cast<PTNode*>(node2->entries[i2].load(std::memory_order_acquire));
        } else {
            node3 = newnode;
        }
    }

    // 创建叶子 ShadowPage
    ShadowPage* leaf = static_cast<ShadowPage*>(node3->entries[i3].load(std::memory_order_acquire));
    if (!leaf) {
        ShadowPage* expect = nullptr;
        ShadowPage* newleaf = new ShadowPage();
        if (!node3->entries[i3].compare_exchange_strong((void*&)expect, (void*)newleaf)) {
            delete newleaf;
            leaf = static_cast<ShadowPage*>(node3->entries[i3].load(std::memory_order_acquire));
        } else {
            leaf = newleaf;
        }
    }
    return leaf;
}