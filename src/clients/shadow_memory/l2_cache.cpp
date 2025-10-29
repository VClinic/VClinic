#include <cassert>

#include "l2_cache.h"
#include "cache_common.h"
#include "page_table.h"

// 构造函数：初始化L2缓存组（容量支持64KB/128KB/256KB，4路组相联，文档2.7.1节）
L2Cache::L2Cache(uint32_t size_kb, ShadowPageTable* spt) 
    : cache_size(size_kb * 1024), spt(spt) {
    // 计算总组数：总容量 / (行大小 × 路数)（行大小64字节，文档2.7.1节）
    uint32_t num_sets = cache_size / (CACHE_LINE_SIZE * L2_ASSOCIATIVITY);
    
    // 初始化每组的4个路（默认状态为INVALID）
    cache_sets.resize(num_sets, std::vector<CacheLine>(L2_ASSOCIATIVITY));

    printf(">>>>>>>> L2Cache Init <<<<<<<<\n");
    printf("L2Cache size %dKB\n", size_kb);
    printf("L2Cache cacheline size %dB\n", CACHE_LINE_SIZE);
    printf("L2Cache ASSOCIATIVITY %d\n", L2_ASSOCIATIVITY);
    printf("L2Cache num of group %d\n\n", num_sets);
}

void L2Cache::ask_l2(uint64_t addr, int32_t tid){
    uint64_t tag;
    uint32_t index, offset, way;
    AddressSplitter::split(addr, cache_size, tag, index, offset);

    // find cacheline
    bool in_l2 = find_cacheline(index, tag, way);

    // L2 Hit
    if(in_l2){
        // check sm status
        CacheLine& hit_cacheline = cache_sets[index][way];
        bool sm_status = hit_cacheline.sp->is_dirty(addr, tid);

        // if sm status is clean --> make l2 cacheline invalid & transfer data to l1
        if(sm_status == false){
            hit_cacheline.empty = true;
            return;
        }
        // if sm status is dirty --> treated as l2 miss & transfer data to l1
        else{
            // treated as l2 miss
            return;
        }
    }
    // L2 Miss
    else{
        // l2 miss
        // transfer data to l1
        printf("l2 miss transfer data to l1\n");
        return;
    }
}

void L2Cache::evicted_from_l1(uint64_t addr, int32_t tid){
    uint64_t tag;
    uint32_t index, offset, way;
    AddressSplitter::split(addr, cache_size, tag, index, offset);
    // assert l2 do not have addr (data cache)
    assert(find_cacheline(index, tag, way) == false);

    uint32_t free_cacheline = find_freecacheline(index, tid);
    CacheLine* target_cacheline = nullptr;

    // L2 has free cache line
    if(free_cacheline >= 0){
        target_cacheline = &(cache_sets[index][way]);
        way = free_cacheline;
    }
    // L2 has NO free cache line
    else{
        // find LRU cache line & evict
        way = find_lru(index);
        target_cacheline = &(cache_sets[index][way]);
    }
    
    // fill cacheline
    target_cacheline->addr = addr;
    target_cacheline->tag = tag;
    target_cacheline->sp = spt->get_or_create_page(addr);
    target_cacheline->empty = false;

    update_lru(index, way);
}

/*
    set current lru counter to 0
    set other lru counter within group to +1
*/
void L2Cache::update_lru(uint32_t group_index, uint32_t way){
    for(uint32_t i=0; i < L2_ASSOCIATIVITY; i++){
        if(i != way)
            cache_sets[group_index][i].lru_counter++;
        else
            cache_sets[group_index][i].lru_counter = 0;
    }
}

uint32_t L2Cache::find_lru(uint32_t group_index){
    // default is way-0
    uint32_t target_way = 0;
    uint32_t max_ts = cache_sets[group_index][0].lru_counter;
    for(uint32_t i=1; i < L2_ASSOCIATIVITY; i++){
        if(cache_sets[group_index][i].lru_counter > max_ts){
            max_ts = cache_sets[group_index][i].lru_counter;
            target_way = i;
        }
    }
    return target_way;
}


/*
    return target way if has free cacheline
    return -1 if has no free cacheline
*/
uint32_t L2Cache::find_freecacheline(uint32_t group_index, int32_t tid){
    for(uint32_t i = 0; i < L2_ASSOCIATIVITY; i++){
        if(cache_sets[group_index][i].empty ||  /* or sm status is dirty ? */
            cache_sets[group_index][i].sp->is_dirty(cache_sets[group_index][i].addr, tid))
            return i;
    }
    // no free line
    return -1;
}

/*
    return true & way when found
    return false when not found
*/
bool L2Cache::find_cacheline(uint32_t index, uint64_t tag, uint32_t& way){
    for(uint32_t i = 0; i < L2_ASSOCIATIVITY; i++){
        if(cache_sets[index][i].tag == tag && 
            cache_sets[index][i].empty == false){
            way = i;
            return true;
        }
    }
    return false;
}