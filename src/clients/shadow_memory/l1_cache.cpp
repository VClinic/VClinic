#include <algorithm>
#include <iostream>

#include "l1_cache.h"
#include "l2_cache.h"
#include "vprofile.h"
#include "cache_common.h"
#include "page_table.h"
// /root/VClinic-sample/build/bin64/drrun -t vprofile_memory -- is.A.x

L1Cache::L1Cache(uint32_t size_kb, L1Type type, L2Cache* l2, ShadowPageTable* spt, file_t output_file) 
    : cache_size(size_kb * 1024), l1_type(type), l2(l2), spt(spt), output_file(output_file) {
    // 计算总组数：容量/(行大小×路数)
    uint32_t num_sets = cache_size / (CACHE_LINE_SIZE * L1_ASSOCIATIVITY);
    
    // 初始化每组的4个路（默认均为INVALID EMPTY状态）
    cache_sets.resize(num_sets, std::vector<CacheLine>(L1_ASSOCIATIVITY));

    total_ins_cnt = 0;

    l1_miss_cnt = 0;
    l1_load_miss_cnt = 0;
    l1_coherence_miss_cnt = 0;
    l1_capacity_miss_cnt = 0;
    l1_conflict_miss_cnt = 0;

    prev_addr = 0;
    diff = 0;
    l1_prefetch_miss_cnt = 0;

    printf(">>>>>>>> L1Cache Init <<<<<<<<\n");
    printf("L1Cache size %dKB\n", size_kb);
    printf("L1Cache cacheline size %dB\n", CACHE_LINE_SIZE);
    printf("L1Cache ASSOCIATIVITY %d\n", L1_ASSOCIATIVITY);
    printf("L1Cache num of group %d\n\n", num_sets);
}

L1Cache::~L1Cache(){
    printf(">>>>>>>> L1Cache Delete <<<<<<<<\n");
    
    delete l2;
    // delete spt;
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

void L1Cache::load(uint64_t addr, int32_t cct, int32_t read_bytes, int32_t tid){
    uint64_t tag;
    uint32_t index, offset, way;
    AddressSplitter::split(addr, cache_size, tag, index, offset);
    // dr_fprintf(output_file, "read addr %lu read bytes %d B\n", addr, read_bytes);

    // printf("tid %d read     addr %lu\n", tid, addr);
    // total ins cnt ++
    total_ins_cnt++;

    // find in L1
    bool in_l1 = find_cacheline(index, tag, way);

    // printf("tid %d read  at %u %u addr %lu\n", tid, index, way, addr);

    // L1 Hit
    if(in_l1){
        // check addr status in shadow memory
        CacheLine& hit_cacheline = cache_sets[index][way];
        // printf("addr %lu l1 hit, try to see sm is_dirty tid %d\n", addr, tid);
        bool sm_status = hit_cacheline.sp->is_dirty(addr, tid);
        
        // if sm status clean
        // update LRU
        // update sm status
        if(sm_status == false){
            // printf("addr %lu l1 hit, sm clean update lru\n", addr);
            // update_lru(index, way);
            // printf("addr %lu l1 hit, try to sp->read\n", addr);
            hit_cacheline.sp->read(addr, tid);
            // printf("addr %lu l1 hit, done sp->read\n", addr);
            // printf("LOAD HIT AND CLEAN\n\n");
            return;
        }

        // if sm status dirty
        // addr invalid (l1 cache miss)
        // addr new insert
        // update sm status
        // treated as l1d load miss (read allocation)
        else{
            // printf("addr %lu l1 hit, sm dirty update lru\n", addr);
            // update_lru(index, way);
            // printf("addr %lu l1 hit, try to sp->read\n", addr);
            hit_cacheline.sp->read(addr, tid);
            // printf("addr %lu l1 hit, done sp->read\n", addr);
            l1_miss_cnt++;
            l1_load_miss_cnt++;
            l1_coherence_miss_cnt++;
            cct_coherence_miss_map[cct]++;
            // printf("LOAD HIT AND DIRTY\n\n");
            return;
        }
    }
    // L1 Miss
    else{
        // l1d load miss
        l1_miss_cnt++;
        l1_load_miss_cnt++;
        // dr_fprintf(output_file, "X read miss addr %lu read bytes %d B\n", addr, read_bytes);
        int32_t cur_diff = addr > prev_addr ? (addr - prev_addr) : -(prev_addr - addr);
        prev_addr = addr;
        if (diff == cur_diff){
            l1_prefetch_miss_cnt++;
        }else{
            diff = cur_diff;
        }
        // printf("LOAD MISS\n\n");
        // Ask L2
        // printf("addr %lu l1 miss ask l2\n", addr);
        // l2->ask_l2(addr, tid);

        // insert cache line
        // update sm status (read)
        uint64_t swap_out_addr;
        way = insert_cacheline(addr, tag, index, tid, &swap_out_addr);
        // update_lru(index, way);
        CacheLine& target_cacheline = cache_sets[index][way];
        target_cacheline.sp->read(addr, tid);

        // process Capacity MISS or Conflict MISS ...
        if(total_ins_cnt < target_cacheline.last_miss_ins_cnt)      // in case overflow (uint64_t)
            target_cacheline.last_miss_ins_cnt = total_ins_cnt;
        if(total_ins_cnt - target_cacheline.last_miss_ins_cnt > L1_INS_CNT_GATE)
            target_cacheline.accu_miss_cnt = 0;     // or can choose faded with time accu_miss_cnt--;
        else
            target_cacheline.accu_miss_cnt++;
        if(target_cacheline.accu_miss_cnt > L1_CONFLICT_MISS_GATE){
            target_cacheline.accu_miss_cnt--;   // in case overflow
            // conflict miss
            l1_conflict_miss_cnt++;
            cct_conflict_miss_map[cct]++;
            // swap out cb
            CacheBump cb;
            cb.addr = swap_out_addr; cb.out_ins_cnt = total_ins_cnt; cb.out_cct = cct; cb.out_reason = CacheMissReason::CONFLICT;
            cache_bump_map[swap_out_addr] = cb;
            // swap in cb
            if(cache_bump_map.find(addr) != cache_bump_map.end()){
                if(total_ins_cnt - cache_bump_map[addr].out_ins_cnt < L1_CACHE_BUMP_GATE){
                    CacheBump& cb_swap_in = cache_bump_map[addr];
                    cb_swap_in.in_ins_cnt = total_ins_cnt; cb_swap_in.in_cct = cct; cb_swap_in.in_reason = CacheMissReason::CONFLICT;
                    cache_bump_list.push_back(cache_bump_map[addr]);
                    cache_bump_map.erase(addr);
                }else
                    cache_bump_map.erase(addr);
            }
        }else{
            // capacity miss
            l1_capacity_miss_cnt++;
            cct_capacity_miss_map[cct]++;
            // swap out cb
            CacheBump cb;
            cb.addr = swap_out_addr; cb.out_ins_cnt = total_ins_cnt; cb.out_cct = cct; cb.out_reason = CacheMissReason::CAPACITY;
            cache_bump_map[swap_out_addr] = cb;
            // swap in cb
            if(cache_bump_map.find(addr) != cache_bump_map.end()){
                if(total_ins_cnt - cache_bump_map[addr].out_ins_cnt < L1_CACHE_BUMP_GATE){
                    CacheBump& cb_swap_in = cache_bump_map[addr];
                    cb_swap_in.in_ins_cnt = total_ins_cnt; cb_swap_in.in_cct = cct; cb_swap_in.in_reason = CacheMissReason::CAPACITY;
                    cache_bump_list.push_back(cache_bump_map[addr]);
                    cache_bump_map.erase(addr);
                }else
                    cache_bump_map.erase(addr);
            }
        }
        clean_steal_cache_bump_map();
        return;
    }
}

void L1Cache::store(uint64_t addr, int32_t cct, int32_t tid){
    uint64_t tag;
    uint32_t index, offset, way;
    AddressSplitter::split(addr, cache_size, tag, index, offset);
    // printf("tid %d write at %u %u addr %lu\n", tid, index, way, addr);
    // printf("write at group %u offset %u\n", index, offset);

    // total ins cnt ++
    total_ins_cnt++;

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
            // update_lru(index, way);
            hit_cacheline.sp->write(addr, tid);
            return;
        }
        // if sm status dirty
        // addr invalid (l1 cache miss)
        // add new insert
        // update sm status
        // treated as l1 store miss (write allocation)
        else{
            // update_lru(index, way);
            hit_cacheline.sp->write(addr, tid);
            l1_miss_cnt++;
            l1_coherence_miss_cnt++;
            cct_coherence_miss_map[cct]++;
            // printf("HIT AND DIRTY\n");
            return;
        }
    }
    // L1 Miss
    else{
        // l1 store miss
        l1_miss_cnt++;
        // Ask L2
        // l2->ask_l2(addr, tid);

        // insert cache line
        // update sm status (write)
        uint64_t swap_out_addr;
        way = insert_cacheline(addr, tag, index, tid, &swap_out_addr);
        // update_lru(index, way);
        CacheLine& target_cacheline = cache_sets[index][way];
        target_cacheline.sp->write(addr, tid);

        // process Capacity MISS or Conflict MISS ...
        if(total_ins_cnt < target_cacheline.last_miss_ins_cnt)      // in case overflow (uint64_t)
            target_cacheline.last_miss_ins_cnt = total_ins_cnt;
        if(total_ins_cnt - target_cacheline.last_miss_ins_cnt > L1_INS_CNT_GATE)
            target_cacheline.accu_miss_cnt = 0;     // or can choose faded with time accu_miss_cnt--;
        else
            target_cacheline.accu_miss_cnt++;
        if(target_cacheline.accu_miss_cnt > L1_CONFLICT_MISS_GATE){
            target_cacheline.accu_miss_cnt--;   // in case overflow
            // conflict miss
            l1_conflict_miss_cnt++;
            cct_conflict_miss_map[cct]++;
            // swap out cb
            CacheBump cb;
            cb.addr = swap_out_addr; cb.out_ins_cnt = total_ins_cnt; cb.out_cct = cct; cb.out_reason = CacheMissReason::CONFLICT;
            cache_bump_map[swap_out_addr] = cb;
            // swap in cb
            if(cache_bump_map.find(addr) != cache_bump_map.end()){
                if(total_ins_cnt - cache_bump_map[addr].out_ins_cnt < L1_CACHE_BUMP_GATE){
                    CacheBump& cb_swap_in = cache_bump_map[addr];
                    cb_swap_in.in_ins_cnt = total_ins_cnt; cb_swap_in.in_cct = cct; cb_swap_in.in_reason = CacheMissReason::CONFLICT;
                    cache_bump_list.push_back(cache_bump_map[addr]);
                    cache_bump_map.erase(addr);
                }else
                    cache_bump_map.erase(addr);
            }
        }else{
            // capacity miss
            l1_capacity_miss_cnt++;
            cct_capacity_miss_map[cct]++;
            // swap out cb
            CacheBump cb;
            cb.addr = swap_out_addr; cb.out_ins_cnt = total_ins_cnt; cb.out_cct = cct; cb.out_reason = CacheMissReason::CAPACITY;
            cache_bump_map[swap_out_addr] = cb;
            // swap in cb
            if(cache_bump_map.find(addr) != cache_bump_map.end()){
                if(total_ins_cnt - cache_bump_map[addr].out_ins_cnt < L1_CACHE_BUMP_GATE){
                    CacheBump& cb_swap_in = cache_bump_map[addr];
                    cb_swap_in.in_ins_cnt = total_ins_cnt; cb_swap_in.in_cct = cct; cb_swap_in.in_reason = CacheMissReason::CAPACITY;
                    cache_bump_list.push_back(cache_bump_map[addr]);
                    cache_bump_map.erase(addr);
                }else
                    cache_bump_map.erase(addr);
            }
        }
        clean_steal_cache_bump_map();

        return;
    }
}

/*
    return free line way if group has free line and insert
    return evicted line way if group has no free line and evict and insert
*/
uint32_t L1Cache::insert_cacheline(uint64_t addr, uint64_t tag, uint32_t group_index, int32_t tid,
                                    uint64_t* swap_out_addr){
    int32_t free_line_way = find_freecacheline(group_index, tid);
    assert(free_line_way < (int32_t)L1_ASSOCIATIVITY && free_line_way >= -1);
    CacheLine* new_line = nullptr;

    // Free line exist
    if(free_line_way >= 0){
        // printf("got free line %d\n", free_line_way);
        new_line = &(cache_sets[group_index][free_line_way]);
    }
    // No free line, need evict
    else{
        // use LRU to fine evicted_line
        // uint32_t evicted_way = find_lru(group_index);
        uint32_t evicted_way = rand() % L1_ASSOCIATIVITY;
        free_line_way = evicted_way;
        // printf("got evicted line %d\n", free_line_way);
        
        // check evicted_line sm status
        new_line = &(cache_sets[group_index][evicted_way]);
        // printf("addr %lu insert_cacheline check sm is_dirty\n", addr);
        bool sm_status = new_line->sp->is_dirty(addr, tid);
        
        // if sm status clean --> evict to L2
        if(sm_status == false){
            // printf("addr %lu l1 evict to l2\n", addr);
            // l2->evicted_from_l1(addr, tid);
            // printf("addr %lu l1 evict to l2 done \n", addr);
        }
        // if sm status dirty --> invalid
        else{
            // logic done no simulation
        }
    }
    // fill new cache line
    *swap_out_addr = new_line->addr;
    new_line->addr = addr;
    new_line->tag = tag;
    new_line->sp = spt->get_or_create_page(addr);
    new_line->empty = false;
    assert(free_line_way < (int32_t)L1_ASSOCIATIVITY && free_line_way >= 0);

    return (uint32_t)free_line_way;
}

// uint32_t L1Cache::find_lru(uint32_t group_index){
//     // default is way-0
//     uint32_t target_way = 0;
//     uint32_t max_ts = cache_sets[group_index][0].lru_counter;
//     for(uint32_t i=1; i < L1_ASSOCIATIVITY; i++){
//         if(cache_sets[group_index][i].lru_counter > max_ts){
//             max_ts = cache_sets[group_index][i].lru_counter;
//             target_way = i;
//         }
//     }
//     return target_way;
// }


/*
    set current lru counter to 0
    set other lru counter within group to +1
*/
// void L1Cache::update_lru(uint32_t group_index, uint32_t way){
//     for(uint32_t i=0; i < L1_ASSOCIATIVITY; i++){
//         if(i != way)
//             cache_sets[group_index][i].lru_counter++;
//         else
//             cache_sets[group_index][i].lru_counter = 0;
//     }
// }

/*
    return target way if has free cacheline
    return -1 if has no free cacheline
*/
int32_t L1Cache::find_freecacheline(uint32_t group_index, int32_t tid){
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

void L1Cache::print_total_info(){
    printf("L1Cache Total l1_miss_cnt: %ld\n", l1_miss_cnt);
    printf("L1Cache Total l1_load_miss_cnt: %ld\n", l1_load_miss_cnt);
    printf("L1Cache coherence miss cnt: %ld\n", l1_coherence_miss_cnt);
    printf("L1Cache capacity miss cnt: %ld\n", l1_capacity_miss_cnt);
    printf("L1Cache conflict miss cnt: %ld\n\n", l1_conflict_miss_cnt);
    printf("L1Cache prefetch miss cnt: %ld\n\n", l1_prefetch_miss_cnt);

    dr_fprintf(output_file, "L1Cache Total l1_miss_cnt: %ld\n", l1_miss_cnt);
    dr_fprintf(output_file, "L1Cache Total l1_load_miss_cnt: %ld\n", l1_load_miss_cnt);
    dr_fprintf(output_file, "L1Cache coherence miss cnt: %ld\n", l1_coherence_miss_cnt);
    dr_fprintf(output_file, "L1Cache capacity miss cnt: %ld\n", l1_capacity_miss_cnt);
    dr_fprintf(output_file, "L1Cache conflict miss cnt: %ld\n\n", l1_conflict_miss_cnt);
    dr_fprintf(output_file, "L1Cache prefetch miss cnt: %ld\n\n", l1_prefetch_miss_cnt);

    std::unordered_map<uint64_t, int> addr_count;
    for (const auto& bump : cache_bump_list) {
        addr_count[bump.addr]++;
    }
    printf("缓存颠簸记录地址总数: %ld\n缓存颠簸记录总条数: %ld\n", addr_count.size(), cache_bump_list.size());
    dr_fprintf(output_file, "缓存颠簸记录地址总数: %ld\n缓存颠簸记录总条数: %ld\n", addr_count.size(), cache_bump_list.size());
}

void L1Cache::print_miss_cnt_topk(int topk, std::unordered_map<int32_t, int64_t>& cct_miss_map, CacheMissReason cache_reason){
    dr_fprintf(output_file, "\n统计缓存缺失类型 %s 的前 %d 条记录的cct值以及其缺失数量:\n",
        reason_to_string(cache_reason).c_str(), topk);
    std::vector<std::pair<int32_t, int64_t>> pairs(cct_miss_map.begin(), cct_miss_map.end());

    sort(pairs.begin(), pairs.end(), [](const std::pair<int32_t, int64_t>& a, const std::pair<int32_t, int64_t>& b) {
        return a.second > b.second; // 根据值降序排序
    });

    if (topk > (int)pairs.size()) {
        topk = (int)pairs.size(); // 防止 k 超过元素数量
        dr_fprintf(output_file, "由于该类型总计数量小于topk，故将topk调整为 %d\n", topk);
    }

    for(std::pair<int32_t, int64_t>& p : std::vector<std::pair<int32_t, int64_t>>(pairs.begin(), pairs.begin() + topk)){
        dr_fprintf(output_file, "cct %d : miss cnt %ld\n", p.first, p.second);
    }

    dr_fprintf(output_file, "\n给出上述记录cct索引到的backtrace位置");
    for(std::pair<int32_t, int64_t>& p : std::vector<std::pair<int32_t, int64_t>>(pairs.begin(), pairs.begin() + topk)){
        dr_fprintf(output_file, "cct %d : miss cnt %ld\n", p.first, p.second);
        drcctlib_print_backtrace(output_file, p.first, false, true, 50 /*MAX_CCT_DEPTH*/);
    }
    dr_fprintf(output_file, "\n\n");
}

void L1Cache::print_cache_bump(int topk){
    dr_fprintf(output_file, "给出前 %d 次缓存颠簸地址记录\n", topk);

    // 1. 使用unordered_map统计每个addr出现的次数
    std::unordered_map<uint64_t, std::vector<CacheBump>> addr_count;
    
    for (const auto& bump : cache_bump_list) {
        addr_count[bump.addr].push_back(bump);
    }
    
    // 2. 将统计结果转移到vector中以便排序
    std::vector<std::pair<uint64_t, std::vector<CacheBump>>> sorted_results;
    for (const auto& pair : addr_count) {
        sorted_results.push_back(pair);
    }
    
    // 3. 按出现次数从高到低排序
    std::sort(sorted_results.begin(), sorted_results.end(),
              [](const std::pair<uint64_t, std::vector<CacheBump>>& a, 
                 const std::pair<uint64_t, std::vector<CacheBump>>& b) {
                  return a.second.size() > b.second.size();  // 按次数降序排列
              });
    
    // 4. 输出结果
    dr_fprintf(output_file, "CacheBump addr出现次数统计（从高到低）：");
    if (topk > (int)sorted_results.size()) {
        topk = (int)sorted_results.size(); // 防止 k 超过元素数量
        dr_fprintf(output_file, "由于该类型总计数量小于topk，故将topk调整为 ", topk);
    }
    for (const auto& result : std::vector<std::pair<uint64_t, std::vector<CacheBump>>>(sorted_results.begin(), sorted_results.begin()+topk)) {
        dr_fprintf(output_file, "addr %ld : cache bump cnt %d\n", result.first, result.second.size());
    }

    for (const auto& result : std::vector<std::pair<uint64_t, std::vector<CacheBump>>>(sorted_results.begin(), sorted_results.begin()+topk)) {
        dr_fprintf(output_file, "addr %ld : cache bump cnt %d\n", result.first, result.second.size());
        dr_fprintf(output_file, "对于地址 %lu 处发生的缓存颠簸事件详细信息:\n", result.first);
        for(CacheBump cb : result.second){
            dr_fprintf(output_file, "颠簸记录 >>> \n");
            dr_fprintf(output_file, "该地址在cct %d 处被换出，换出原因 <%s> ，在cct %d 处被换入，换入原因 <%s>\n", 
                cb.out_cct, reason_to_string(cb.out_reason).c_str(), cb.in_cct, reason_to_string(cb.in_reason).c_str());

            dr_fprintf(output_file, "换出 backtrace \n");
            drcctlib_print_backtrace(output_file, cb.out_cct, false, true, 50 /*MAX_CCT_DEPTH*/);
            dr_fprintf(output_file, "换入 backtrace \n");
            drcctlib_print_backtrace(output_file, cb.in_cct, false, true, 50 /*MAX_CCT_DEPTH*/);
        }
    }
}

void L1Cache::clean_steal_cache_bump_map(){
    for (auto it = cache_bump_map.begin(); it != cache_bump_map.end(); ) {
        if (total_ins_cnt - it->second.out_ins_cnt >= L1_CACHE_BUMP_GATE)
            it = cache_bump_map.erase(it); 
        else
            ++it;
    }
}

void L1Cache::print_result(){
    print_total_info();
    dr_fprintf(output_file, "容量缺失 topk \n", l1_miss_cnt);
    print_miss_cnt_topk(10, cct_coherence_miss_map, CacheMissReason::COHERENCE);
    print_miss_cnt_topk(20, cct_capacity_miss_map, CacheMissReason::CAPACITY);
    print_miss_cnt_topk(10, cct_conflict_miss_map, CacheMissReason::CONFLICT);
    print_cache_bump(10);
}