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

void L1Cache::read(val_info_t* info){
    // MEMORY INS
    // printf("l1 got memory addr %ld\n", info->addr);
    uint64_t tag;
    uint32_t index, offset, way;
    AddressSplitter::split(info->addr, cache_size, tag, index, offset);
    // printf("l1 got %lu %u %u\n", tag, index, offset);

    // 1. find cache line in l1-cache
    if (find_cacheline(index, tag, way)) {
        // update_lru(index, way);
        // printf("L1 Cache READ HIT !\n");
        return;
    }

    // 2. cache miss (read miss)
    CacheLine new_line; new_line.tag = tag; new_line.state = MESIState::EXCLUSIVE; new_line.lru_counter++;
    // MESIState l2_state = l2_cache.provide_line(phys_addr, new_line.data);  // 向L2请求行数据

    // 3. allocate new cache line
    insert_cacheline(new_line, index);

    // 4. inclusive or exclusive ...
    if(l1_type == L1Type::DATA){
        // TODO: MESI state from L2
        // TODO: exclusive cacheline in L2
    }else{
        // TODO: MESI state from L2
    }
}

void L1Cache::insert_cacheline(CacheLine new_line, uint32_t group_index){
    // 1. find_free_cacheline
    // 2. need to evict old line
    // 3. insert new_line to free_cacheline
    uint32_t way = find_freecacheline(group_index);
    
    if(group_cap[group_index] == L1_ASSOCIATIVITY){
        // TODO: evict old line;
        // printf("L1 Cache READ EVICT !\n");
    }else{
        group_cap[group_index]++;
    }
    

    cache_sets[group_index][way] = new_line;
    assert(group_cap[group_index] <= L1_ASSOCIATIVITY && group_cap[group_index] >= 0);
}

uint32_t L1Cache::find_freecacheline(uint32_t group_index){
    // 1. if there is idle cache_line, then return
    // 2. else use PRR to evict
    if(group_cap[group_index] < L1_ASSOCIATIVITY){
        // exit idle cache line
        // printf("L1 Cache READ IDLE !\n");
        for(uint32_t i = 0; i < L1_ASSOCIATIVITY; i++){
            if(cache_sets[group_index][i].tag == 0 && cache_sets[group_index][i].state == MESIState::INVALID)
                return i;
        }
    }
    // cache line full
    return std::rand() % L1_ASSOCIATIVITY;
}


bool L1Cache::find_cacheline(uint32_t index, uint64_t tag, uint32_t& way){
    for(uint32_t i = 0; i < L1_ASSOCIATIVITY; i++){
        if(cache_sets[index][i].state != MESIState::INVALID && cache_sets[index][i].tag == tag){
            way = i;
            return true;
        }
    }
    return false;
}

void L1Cache::update_lru(uint32_t index, uint32_t way){
    cache_sets[index][way].lru_counter++;
}