// 简化示例实现；工程中应把错误处理、对齐、内存回收等完善
#ifndef PAGE_TABLE
#define PAGE_TABLE

#include <atomic>
#include <mutex>
#include <vector>
#include <memory>
#include <cstring>

#define MAX_THREADS 8

enum class COHERENCE_MISS_REASON {
    CLEAN,
    TURE_SHARING,
    FALSE_SHARING
};

// 配置常量
constexpr size_t PAGE_SIZE = 4096;
constexpr size_t CACHE_LINE = 64;
constexpr size_t LINES_PER_PAGE = PAGE_SIZE / CACHE_LINE;

// 页表层次位宽
constexpr int OFFSET_BITS = 12; // 4KB
constexpr int L3_BITS = 9;
constexpr int L2_BITS = 9;
constexpr int L1_BITS = 9;


// struct CacheLine {
//     size_t Lasttid;
//     uint8_t LastOP;
// };

// ---- 每个叶子页 ----
class ShadowPage {
private:
    std::mutex page_lock;

    // Dirty 掩码：一个 bit per cache line
    // std::atomic<uint64_t> thread_dirty_bitmap[MAX_THREADS];

    // Dirty 掩码：一个 bit per cache line's Byte
    std::atomic<uint64_t> thread_dirty_bitmap_perline[MAX_THREADS][LINES_PER_PAGE];

    uint8_t data[LINES_PER_PAGE];
public:
    ShadowPage() {
        // for(size_t i=0; i < MAX_THREADS; i++)   thread_dirty_bitmap[i].store(0);
        for(size_t i=0; i < MAX_THREADS; i++){
            for(size_t j=0; j < LINES_PER_PAGE; j++)    thread_dirty_bitmap_perline[i][j].store(0);
        }
        std::memset(data, 0, LINES_PER_PAGE);
    }
    
    // return if cache of addr is dirty
    COHERENCE_MISS_REASON is_dirty(uint64_t addr, int32_t tid, uint32_t offset);

    // set cache of addr to clean. return if cache of addr is dirty
    void read(uint64_t addr, int32_t tid);

    // set cache of addt to dirty for all threads ;return if cache of addr is dirty
    void write(uint64_t addr, int32_t tid, uint32_t offset);
};

// ---- 页表节点 ----
struct PTNode {
    std::vector<std::atomic<void*>> entries;
    PTNode(size_t n): entries(n) {
        for (auto &e : entries) e.store(nullptr);
    }
}; 

// 顶层页表封装
class ShadowPageTable {
    static constexpr size_t L1_SIZE = (1ULL << L1_BITS);
    static constexpr size_t L2_SIZE = (1ULL << L2_BITS);
    static constexpr size_t L3_SIZE = (1ULL << L3_BITS);

    std::unique_ptr<PTNode> l1;
public:
    ShadowPageTable() {
        l1.reset(new PTNode(L1_SIZE));
    }

    ~ShadowPageTable() {
    }

    // return page hadler for addr, return nullptr if no page was found
    ShadowPage* find_page(uint64_t addr);

    // return page hadler for addr, new page will be created if no page was found
    ShadowPage* get_or_create_page(uint64_t addr);
};


#endif  // PAGE_TABLE