#include <random>
#include <ctime>
#include "l1_cache.h"
#include "vprofile.h"

L1Cache::L1Cache(uint32_t size_kb, L1Type type) 
    : cache_size(size_kb * 1024), l1_type(type) {
    // 计算总组数：容量/(行大小×路数)
    uint32_t num_sets = cache_size / (CACHE_LINE_SIZE * L1_ASSOCIATIVITY);
    group_cap = (uint32_t *)malloc(num_sets * sizeof(uint32_t));
    memset(group_cap, 0, sizeof(uint32_t) * num_sets);
    
    // 初始化每组的4个路（默认均为INVALID状态）
    cache_sets.resize(num_sets, std::vector<CacheLine>(L1_ASSOCIATIVITY));

    // random seed PRR 
    std::srand(std::time(nullptr));
    printf("L1Cache init ...\n");
}


void L1Cache::print_here(val_info_t *info){
    if ((info->type & vprofile_src_t::MEMORY_READ) || 
        (info->type & vprofile_src_t::MEMORY_WRITE)){
        // printf("l1d got memory addr %ld\n", info->addr);
        int a= 0;
        for(a = 0; a < 100; a++){
            a += 1;
        }
    }
}

void L1Cache::load(uint64_t addr){
    uint64_t tag;
    uint32_t index, offset, way;
    AddressSplitter::split(addr, cache_size, tag, index, offset);

    // find in L1
    bool in_l1 = find_cacheline(index, tag, way);

    // L1 Hit
    if(in_l1){
        bool sm_status;
        // check addr status in shadow memory
        
        // if sm status clean
        // update LRU
        // update sm status
        return;

        // if sm status dirty
        // addr invalid (l1 cache miss)
        // addr new insert
        // update sm status
        // treated as l1d load miss (read allocation)
        return;
    }
    // L1 Miss
    else{
        // l1d load miss
        // Ask L2

        // insert cache line
        // update sm status (read)
        
        return;
    }
}

void L1Cache::store(uint64_t addr){
    uint64_t tag;
    uint32_t index, offset, way;
    AddressSplitter::split(addr, cache_size, tag, index, offset);

    // find in L1
    bool in_l1 = find_cacheline(index, tag, way);

    // L1 Hit
    if(in_l1){
        // check sm status 

        // if sm status clean
        // update LRU
        // update sm status
        return;

        // if sm status dirty
        // addr invalid (l1 cache miss)
        // add new insert
        // update sm status
        // treated as l1 store miss (write allocation)
        return;
    }
    // L1 Miss
    else{
        // l1 store miss
        // Ask L2

        // insert cache line
        // update sm status (write)
        return;
    }
}

void L1Cache::insert_cacheline(CacheLine new_line, uint32_t group_index){
    uint32_t free_line_way = find_freecacheline(group_index);

    // Free line exist
    if(free_line_way >= 0){
        cache_sets[group_index][free_line_way] = new_line;
        group_cap[group_index]++;
    }
    // No free line, need evict
    else{
        // use LRU to fine evicted_line
        uint32_t evicted_way = find_lru(group_index);
        // check evicted_line sm status
        // if sm status clean --> evict to L2
        // if sm status dirty --> invalid
    }

    assert(group_cap[group_index] <= L1_ASSOCIATIVITY && group_cap[group_index] >= 0);
}

uint32_t L1Cache::find_lru(uint32_t group_index){
    // default is way-0
    uint32_t target_way = 0;
    uint32_t max_ts = cache_sets[group_index][0].lru_counter;
    for(uint32_t i=1; i < L1_ASSOCIATIVITY; i++){
        if(cache_sets[group_index][i].lru_counter > max_ts){
            max_ts = cache_sets[group_index][i].lru_counter;
            target_way = i;
        }
    }
    return target_way;
}


/*
    set current lru counter to 0
    set other lru counter within group to +1
*/
void L1Cache::update_lru(uint32_t group_index, uint32_t way){
    for(uint32_t i=0; i < L1_ASSOCIATIVITY; i++){
        if(i != way)
            cache_sets[group_index][i].lru_counter++;
        else
        cache_sets[group_index][i].lru_counter = 0;
    }
}

uint32_t L1Cache::find_freecacheline(uint32_t group_index){
    // 1. if there is idle cache_line, then return
    if(group_cap[group_index] < L1_ASSOCIATIVITY){
        // exit idle cache line
        for(uint32_t i = 0; i < L1_ASSOCIATIVITY; i++){
            if(cache_sets[group_index][i].tag == 0 /* or sm status is dirty ? */)
                return i;
        }
    }
    // no free line
    return -1;
}


/*
    return true & way when found
    return false when not found
*/
bool L1Cache::find_cacheline(uint32_t index, uint64_t tag, uint32_t& way){
    for(uint32_t i = 0; i < L1_ASSOCIATIVITY; i++){
        if(cache_sets[index][i].tag == tag){
            way = i;
            return true;
        }
    }
    return false;
}