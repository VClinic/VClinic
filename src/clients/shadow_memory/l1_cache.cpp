#include "l1_cache.h"
#include "l2_cache.h"
#include "vprofile.h"
#include "cache_common.h"
#include "page_table.h"
// /root/VClinic-sample/build/bin64/drrun -t vprofile_memory -- is.A.x

L1Cache::L1Cache(uint32_t size_kb, L1Type type, L2Cache* l2, ShadowPageTable* spt) 
    : cache_size(size_kb * 1024), l1_type(type), l2(l2), spt(spt) {
    // 计算总组数：容量/(行大小×路数)
    uint32_t num_sets = cache_size / (CACHE_LINE_SIZE * L1_ASSOCIATIVITY);
    
    // 初始化每组的4个路（默认均为INVALID EMPTY状态）
    cache_sets.resize(num_sets, std::vector<CacheLine>(L1_ASSOCIATIVITY));

    printf(">>>>>>>> L1Cache Init <<<<<<<<\n");
    printf("L1Cache size %dKB\n", size_kb);
    printf("L1Cache cacheline size %dB\n", CACHE_LINE_SIZE);
    printf("L1Cache ASSOCIATIVITY %d\n", L1_ASSOCIATIVITY);
    printf("L1Cache num of group %d\n\n", num_sets);
}

L1Cache::~L1Cache(){
    delete l2;
    delete spt;
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

void L1Cache::load(uint64_t addr, int32_t tid){
    uint64_t tag;
    uint32_t index, offset, way;
    AddressSplitter::split(addr, cache_size, tag, index, offset);

    // find in L1
    bool in_l1 = find_cacheline(index, tag, way);

    // L1 Hit
    if(in_l1){
        // check addr status in shadow memory
        CacheLine& hit_cacheline = cache_sets[index][way];
        bool sm_status = hit_cacheline.sp->is_dirty(addr, tid);
        
        // if sm status clean
        // update LRU
        // update sm status
        if(sm_status == false){
            update_lru(index, way);
            hit_cacheline.sp->read(addr, tid);
            return;
        }

        // if sm status dirty
        // addr invalid (l1 cache miss)
        // addr new insert
        // update sm status
        // treated as l1d load miss (read allocation)
        else{
            update_lru(index, way);
            hit_cacheline.sp->read(addr, tid);
            return;
        }
    }
    // L1 Miss
    else{
        // l1d load miss
        // Ask L2
        printf("l1 miss as l2\n");
        l2->ask_l2(addr, tid);

        // insert cache line
        // update sm status (read)
        way = insert_cacheline(addr, tag, index, tid);
        update_lru(index, way);
        CacheLine& target_cacheline = cache_sets[index][way];
        printf("l1 try to sp->read\n");
        target_cacheline.sp->read(addr, tid);
        printf("l1 done to sp->read\n");

        return;
    }
}

void L1Cache::store(uint64_t addr, int32_t tid){
    uint64_t tag;
    uint32_t index, offset, way;
    AddressSplitter::split(addr, cache_size, tag, index, offset);

    // find in L1
    bool in_l1 = find_cacheline(index, tag, way);

    // L1 Hit
    if(in_l1){
        // check sm status 
        CacheLine& hit_cacheline = cache_sets[index][way];
        bool sm_status = hit_cacheline.sp->is_dirty(addr, tid);

        // if sm status clean
        // update LRU
        // update sm status
        if(sm_status == false){
            update_lru(index, way);
            hit_cacheline.sp->write(addr, tid);
            return;
        }
        // if sm status dirty
        // addr invalid (l1 cache miss)
        // add new insert
        // update sm status
        // treated as l1 store miss (write allocation)
        else{
            update_lru(index, way);
            hit_cacheline.sp->write(addr, tid);
            return;
        }
    }
    // L1 Miss
    else{
        // l1 store miss
        // Ask L2
        l2->ask_l2(addr, tid);

        // insert cache line
        // update sm status (write)
        way = insert_cacheline(addr, tag, index, tid);
        update_lru(index, way);
        CacheLine& target_cacheline = cache_sets[index][way];
        target_cacheline.sp->write(addr, tid);
        return;
    }
}

/*
    return free line way if group has free line and insert
    return evicted line way if group has no free line and evict and insert
*/
uint32_t L1Cache::insert_cacheline(uint64_t addr, uint64_t tag, uint32_t group_index, int32_t tid){
    uint32_t free_line_way = find_freecacheline(group_index, tid);
    CacheLine* new_line = nullptr;

    // Free line exist
    if(free_line_way >= 0){
        new_line = &(cache_sets[group_index][free_line_way]);
    }
    // No free line, need evict
    else{
        // use LRU to fine evicted_line
        uint32_t evicted_way = find_lru(group_index);
        free_line_way = evicted_way;
        
        // check evicted_line sm status
        new_line = &(cache_sets[group_index][evicted_way]);
        bool sm_status = new_line->sp->is_dirty(addr, tid);
        
        // if sm status clean --> evict to L2
        if(sm_status == false){
            l2->evicted_from_l1(addr, tid);
        }
        // if sm status dirty --> invalid
        else{
            // logic done no simulation
        }
    }
    // fill new cache line
    new_line->addr = addr;
    new_line->tag = tag;
    new_line->sp = spt->get_or_create_page(addr);
    new_line->empty = false;

    return free_line_way;
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

/*
    return target way if has free cacheline
    return -1 if has no free cacheline
*/
uint32_t L1Cache::find_freecacheline(uint32_t group_index, int32_t tid){
    for(uint32_t i = 0; i < L1_ASSOCIATIVITY; i++){
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
bool L1Cache::find_cacheline(uint32_t index, uint64_t tag, uint32_t& way){
    for(uint32_t i = 0; i < L1_ASSOCIATIVITY; i++){
        if(cache_sets[index][i].tag == tag && 
            cache_sets[index][i].empty == false){
            way = i;
            return true;
        }
    }
    return false;
}