#include <algorithm>
#include <iostream>
#include <queue>

#include "l1_cache.h"
#include "l2_cache.h"
#include "vprofile.h"
#include "cache_common.h"
#include "page_table.h"
// /root/VClinic-sample/build/bin64/drrun -t vprofile_memory -- is.A.x

L1Cache::L1Cache(uint32_t size_kb, L1Type type, L2Cache* l2, ShadowPageTable* spt, file_t output_file) 
    : cache_size(size_kb * 1024), l1_type(type), l2(l2), spt(spt), output_file(output_file) {
    print_flag = false;
    // 计算总组数：容量/(行大小×路数)
    uint32_t num_sets = cache_size / (CACHE_LINE_SIZE * L1_ASSOCIATIVITY);
    
    // 初始化每组的4个路（默认均为INVALID EMPTY状态）
    cache_sets.resize(num_sets, std::vector<CacheLine>(L1_ASSOCIATIVITY));

    total_ins_cnt = 0;

    l1_miss_cnt = 0;
    l1_load_miss_cnt = 0;
    l1_store_miss_cnt = 0;
    l1_coherence_miss_cnt = 0;
    l1_capacity_miss_cnt = 0;
    l1_conflict_miss_cnt = 0;

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

bool cct_catch(int32_t cct){
    // srad 272 line 
    // return (cct == 25500) || (cct == 25461) || (cct == 25410) || (cct == 25447);
    // srad 205 line
    // return (cct == 25369);
    // srad 243 line
    // return (cct == 25500);
    // is
    // return (cct == 25454) || (cct == 25455) || (cct == 25456) || (cct == 25460);
    return false;
}

void L1Cache::load(uint64_t addr, int32_t cct, int32_t read_bytes, int32_t tid){
    uint64_t tag;
    uint32_t index, offset, way;
    AddressSplitter::split(addr, cache_size, tag, index, offset);
    // dr_fprintf(output_file, "read addr %lu read bytes %d B\n", addr, read_bytes);
    // printf("read addr %lu read bytes %d B\n", addr, read_bytes);

    // prefetch
    bool prefetched = check_prefetch(addr);

    // need multi cacheline split ?
    if(!prefetched && offset + read_bytes > L1_CACHE_SIZE){
        // printf("need multi cacheline split addr %lu read_bytes %d offset %u\n", addr, read_bytes, offset); fflush(NULL);
        load(addr + (L1_CACHE_SIZE - offset), cct, read_bytes - (L1_CACHE_SIZE - offset), tid);
    }

    if(cct_catch(cct) || print_flag){
        print_flag = true;
        dr_fprintf(output_file, "read addr %lu read bytes %d B\n", addr, read_bytes);
    }

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
            if(!prefetched){
                if(cct_catch(cct)){
                    dr_fprintf(output_file, "cct %d X read dirty addr %lu read bytes %d B\n", cct, addr, read_bytes);
                }
                l1_miss_cnt++;
                l1_load_miss_cnt++;
                l1_coherence_miss_cnt++;
                cct_coherence_miss_map[cct]++;
            }
            // printf("LOAD HIT AND DIRTY\n\n");
            return;
        }
    }
    // L1 Miss
    else{
        // l1d load miss
        if(!prefetched){
            l1_miss_cnt++;
            l1_load_miss_cnt++;
        }
        // dr_fprintf(output_file, "X read miss addr %lu read bytes %d B\n", addr, read_bytes);
        if(!prefetched){
            if(cct_catch(cct)){
                dr_fprintf(output_file, "cct %d X read miss addr %lu read bytes %d B\n", cct, addr, read_bytes);
            }
        }

        // if(!prefetched && cct_catch(cct)){
        //     dr_fprintf(output_file, "prefetch_prefix_deque >>>>>> \n");
        //     for(uint64_t a : prefetch_prefix_deque){
        //         dr_fprintf(output_file, "addr %lu\n", a);
        //     }
        //     dr_fprintf(output_file, "prefetch_prefix_deque <<<<<< \n");
        // }

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
        if(!prefetched){
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
        }
        return;
    }
}

void L1Cache::store(uint64_t addr, int32_t cct, int32_t write_bytes, int32_t tid){
    uint64_t tag;
    uint32_t index, offset, way;
    AddressSplitter::split(addr, cache_size, tag, index, offset);
    // printf("tid %d write at %u %u addr %lu\n", tid, index, way, addr);
    // printf("write at group %u offset %u\n", index, offset);

    if(cct_catch(cct) || print_flag){
        print_flag = true;
        dr_fprintf(output_file, "write addr %lu write bytes %d B\n", addr, write_bytes);
    }

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
            l1_store_miss_cnt++;
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
        l1_store_miss_cnt++;
        if(cct_catch(cct)){
            dr_fprintf(output_file, "cct %d X write miss addr %lu write bytes %d B\n", cct, addr, write_bytes);
        }
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

bool L1Cache::check_prefetch(uint64_t new_addr){
    bool found = false;
    // 检查 i = 1..8 的 16 个候选值
    std::vector<int64_t> diff_list({-1,-2,-4,2,4});
    for(int64_t diff : diff_list){
        uint64_t a = (int64_t)new_addr + diff;
        uint64_t b = (int64_t)new_addr + diff + diff;
        uint64_t c = (int64_t)new_addr + diff + diff + diff;

        // 在 hash set 中查询
        if (prefetch_prefix_map.find(a) != prefetch_prefix_map.end() &&
            prefetch_prefix_map.find(b) != prefetch_prefix_map.end() &&
            prefetch_prefix_map.find(c) != prefetch_prefix_map.end() ) {
            found = true;
            break;
        }
    }

    // 维护固定容量队列
    if (prefetch_prefix_deque.size() == PREFETCH_CAPACITY) {
        // 队满：弹出队首，同时从 set 中删除
        uint64_t front = prefetch_prefix_deque.front();
        prefetch_prefix_deque.pop_front();
        auto iter = prefetch_prefix_map.find(front);
        if(--(iter->second) == 0)
            prefetch_prefix_map.erase(iter);
    }

    // 插入新地址到队尾，并放入 set
    prefetch_prefix_deque.push_back(new_addr);
    prefetch_prefix_map[new_addr]++;

    return found;
}

void L1Cache::print_total_info(){
    printf("L1Cache Total l1_miss_cnt: %ld\n", l1_miss_cnt);
    printf("L1Cache Total l1_load_miss_cnt: %ld\n", l1_load_miss_cnt);
    printf("L1Cache Total l1_store_miss_cnt: %ld\n", l1_store_miss_cnt);
    printf("L1Cache coherence miss cnt: %ld\n", l1_coherence_miss_cnt);
    printf("L1Cache capacity miss cnt: %ld\n", l1_capacity_miss_cnt);
    printf("L1Cache conflict miss cnt: %ld\n\n", l1_conflict_miss_cnt);

    dr_fprintf(output_file, "L1Cache Total l1_miss_cnt: %ld\n", l1_miss_cnt);
    dr_fprintf(output_file, "L1Cache Total l1_load_miss_cnt: %ld\n", l1_load_miss_cnt);
    dr_fprintf(output_file, "L1Cache Total l1_store_miss_cnt: %ld\n", l1_store_miss_cnt);
    dr_fprintf(output_file, "L1Cache coherence miss cnt: %ld\n", l1_coherence_miss_cnt);
    dr_fprintf(output_file, "L1Cache capacity miss cnt: %ld\n", l1_capacity_miss_cnt);
    dr_fprintf(output_file, "L1Cache conflict miss cnt: %ld\n\n", l1_conflict_miss_cnt);

    printf("<<< Total info done <<<\n");
}

std::unordered_map<int32_t, int64_t>
merge_context_by_call_path(const std::unordered_map<int32_t, int64_t>& cct_cnt)
{
    // 先把原 map 拷贝到一个 vector 中，方便顺序遍历
    std::vector<std::pair<int32_t, int64_t>> entries;
    entries.reserve(cct_cnt.size());
    for (const auto& kv : cct_cnt) {
        entries.emplace_back(kv.first, kv.second);
    }

    std::unordered_map<int32_t, int64_t> merged;

    // merged 中的每个 key 是一个“代表 context”，其 value 是所有等价 context 的计数和
    for (const auto& kv : entries) {
        int32_t ctxt = kv.first;
        int64_t count = kv.second;

        bool merged_to_existing = false;

        // 在线性扫描 merged，寻找一个等价的代表 key
        for (auto& rep_kv : merged) {
            int32_t rep_ctxt = rep_kv.first;
            if (drcctlib_have_same_source_line((context_handle_t)ctxt, (context_handle_t)rep_ctxt)) {
                // 找到等价的调用路径，累加计数后标记已合并
                rep_kv.second += count;
                merged_to_existing = true;
                break;
            }
        }

        // 如果没找到等价 key，就把当前 ctxt 作为新的代表插入
        if (!merged_to_existing) {
            merged.emplace(ctxt, count);
        }
    }

    return merged;
}

std::unordered_map<int32_t, int64_t>
merge_context_by_call_path_fast(std::unordered_map<int32_t, int64_t>& cct_cnt)
{
    // const size_t total_size = cct_cnt.size();
    std::unordered_map<int32_t/*cct_hndl*/, int64_t/*count*/> merged;
    // std::unordered_map<int32_t/*line_no*/, int32_t/*cct_hndl*/> ctxt_to_rep;

    for(auto kv = cct_cnt.begin(); kv != cct_cnt.end(); ){
        const int32_t curr_cct_hndl = kv->first;
        const int64_t curr_count = kv->second;
        kv = cct_cnt.erase(kv);

        if (curr_count < 10){
            continue;
        }

        // 缓存命中，直接累加
        // inner_context_t *ctxt = ctxt_get_from_ctxt_hndl((context_handle_t) curr_cct_hndl);
        // int line_no = ctxt->line_no; ctxt_free(ctxt);
        // auto ctxt_iter = ctxt_to_rep.find(line_no);
        // if (ctxt_iter != ctxt_to_rep.end()) {
        //     merged[ctxt_iter->second] += curr_count;
        //     continue;
        // }

        // 缓存未命中，查找等价代表
        int32_t found_rep = -1;
        for (const auto& rep_kv : merged) {
            if (drcctlib_have_same_source_line((context_handle_t)curr_cct_hndl, (context_handle_t)rep_kv.first)) {
                found_rep = rep_kv.first;
                break;
            }
        }
        
        if (found_rep != -1) {  // found
            merged[found_rep] += curr_count;
            // ctxt_to_rep.emplace(line_no, found_rep);
        } else {
            merged.emplace(curr_cct_hndl, curr_count);
            // ctxt_to_rep.emplace(line_no, curr_cct_hndl);
        }
    }

    return merged;
}

void sort_topk_miss_cnt(int topk, std::unordered_map<int32_t, int64_t>& cct_miss_map, std::vector<std::pair<int32_t, int64_t>>& out){
    if (topk == 0 || cct_miss_map.empty()) {
        out.clear();
        return;
    }

    using Pair = std::pair<int32_t, int64_t>;
    // 按值从小到大排（小顶堆），堆顶是当前最小的那个
    auto cmp = [](const Pair& a, const Pair& b) {
        return a.second > b.second; // value 小的优先
    };

    std::priority_queue<Pair, std::vector<Pair>, decltype(cmp)> minHeap(cmp);

    for (const auto& kv : cct_miss_map) {
        if (minHeap.size() < (size_t)topk) {
            minHeap.push(kv);
        } else if (kv.second > minHeap.top().second) {
            minHeap.pop();
            minHeap.push(kv);
        }
    }

    out.resize(minHeap.size());
    for (int i = (int)minHeap.size() - 1; i >= 0; --i) {
        out[i] = minHeap.top();
        minHeap.pop();
    }
    // out 此时是按 value 降序
}

void L1Cache::print_miss_cnt_topk(int topk, std::unordered_map<int32_t, int64_t>& cct_miss_map, CacheMissReason cache_reason){

    // 将相同的上下文调用栈数量合并
    std::unordered_map<int32_t, int64_t> merged_cct_miss_map = merge_context_by_call_path_fast(cct_miss_map);
    assert(cct_miss_map.size() == 0);


    // 排序并获得Topk cct，其余丢掉
    std::vector<std::pair<int32_t, int64_t>> sorted_topk_cct_miss_map;
    if (topk > (int)merged_cct_miss_map.size()) {
        topk = (int)merged_cct_miss_map.size(); // 防止 k 超过元素数量
        dr_fprintf(output_file, "由于该类型总计数量小于topk，故将topk调整为 %d\n", topk);
    }
    sort_topk_miss_cnt(topk, merged_cct_miss_map, sorted_topk_cct_miss_map);   merged_cct_miss_map.clear();


    dr_fprintf(output_file, "\n统计缓存缺失类型 %s 的前 %d 条记录的cct值以及其缺失数量:\n",
        reason_to_string(cache_reason).c_str(), topk);

    for(std::pair<int32_t, int64_t>& p : sorted_topk_cct_miss_map){
        dr_fprintf(output_file, "cct %d : miss cnt %ld %.2f%% \n", p.first, p.second, (((float)p.second)/l1_miss_cnt)*100);
    }

    dr_fprintf(output_file, "\n给出上述记录cct索引到的backtrace位置\n");
    for(std::pair<int32_t, int64_t>& p : sorted_topk_cct_miss_map){
        dr_fprintf(output_file, "cct %d : miss cnt %ld %.2f%% \n", p.first, p.second, (((float)p.second)/l1_miss_cnt)*100);
        drcctlib_print_backtrace(output_file, p.first, false, true, 50 /*MAX_CCT_DEPTH*/);
    }
    dr_fprintf(output_file, "\n\n");
}


void sort_topk_cache_bump(int topk, std::unordered_map<uint64_t, std::vector<CacheBump>>& cache_bump_map, std::vector<std::pair<uint64_t, std::vector<CacheBump>>>& out){
    if (topk == 0 || cache_bump_map.empty()) {
        out.clear();
        return;
    }

    using Pair = std::pair<uint64_t, std::vector<CacheBump>>;
    // 按值从小到大排（小顶堆），堆顶是当前最小的那个
    auto cmp = [](const Pair& a, const Pair& b) {
        return a.second.size() > b.second.size(); // value 小的优先
    };

    std::priority_queue<Pair, std::vector<Pair>, decltype(cmp)> minHeap(cmp);

    for (const auto& kv : cache_bump_map) {
        if (minHeap.size() < (size_t)topk) {
            minHeap.push(std::move(kv));
        } else if (kv.second.size() > minHeap.top().second.size()) {
            minHeap.pop();
            minHeap.push(std::move(kv));
        }
    }

    out.resize(minHeap.size());
    for (int i = (int)minHeap.size() - 1; i >= 0; --i) {
        out[i] = minHeap.top();
        minHeap.pop();
    }
    // out 此时是按 value 降序
}

void L1Cache::print_cache_bump(int topk, int num_print_event){
    dr_fprintf(output_file, "给出前 %d 次缓存颠簸地址记录\n", topk);

    // 1. 使用unordered_map统计每个addr出现的次数
    printf("print cache bump list size %ld\n", cache_bump_list.size());
    std::unordered_map<uint64_t, std::vector<CacheBump>> addr_count;
    int cnt = 0;
    for(const auto& iter : cache_bump_list){
        addr_count[iter.addr].push_back(iter);
        cnt++;
        if((cnt % 10000) == 0)
            printf("%d\n", cnt);
    }
    // for (auto iter = cache_bump_list.begin(); iter != cache_bump_list.end(); iter++) {
    //     // printf("%lu\n", iter->addr);
    //     addr_count[iter->addr].push_back(*iter);
    //     // iter = cache_bump_list.erase(iter);
    // }
    // assert(cache_bump_list.size() == 0);
    printf("first done\n");

    std::vector<std::pair<uint64_t, std::vector<CacheBump>>> sorted_results;
    if (topk > (int)addr_count.size()) {
        topk = (int)addr_count.size(); // 防止 k 超过元素数量
        dr_fprintf(output_file, "由于该类型总计数量小于topk，故将topk调整为 ", topk);
    }
    sort_topk_cache_bump(topk, addr_count, sorted_results);
    addr_count.clear();
    printf("second done\n");
    
    // 2. 将统计结果转移到vector中以便排序
    // std::vector<std::pair<uint64_t, std::vector<CacheBump>>> sorted_results;
    // for (const auto& pair : addr_count) {
    //     sorted_results.push_back(pair);
    // }
    
    // 3. 按出现次数从高到低排序
    // std::sort(sorted_results.begin(), sorted_results.end(),
    //           [](const std::pair<uint64_t, std::vector<CacheBump>>& a, 
    //              const std::pair<uint64_t, std::vector<CacheBump>>& b) {
    //               return a.second.size() > b.second.size();  // 按次数降序排列
    //           });
    
    // 4. 输出结果
    dr_fprintf(output_file, "CacheBump addr出现次数统计（从高到低）：");

    for (const auto& result : sorted_results) {
        dr_fprintf(output_file, "addr %ld : cache bump cnt %d\n", result.first, result.second.size());
    }

    for (const auto& result : sorted_results) {
        dr_fprintf(output_file, "addr %ld : cache bump cnt %d\n", result.first, result.second.size());
        dr_fprintf(output_file, "对于地址 %lu 处发生的缓存颠簸事件详细信息(最多输出 %d 条事件):\n", result.first, num_print_event);
        int print_event_cnt = 1;
        for(CacheBump cb : result.second){
            if(print_event_cnt++ > num_print_event)
                break;
            dr_fprintf(output_file, "颠簸记录 >>>>>> \n");
            dr_fprintf(output_file, "该地址在cct %d 处被换出，换出原因 <%s> ，在cct %d 处被换入，换入原因 <%s>\n", 
                cb.out_cct, reason_to_string(cb.out_reason).c_str(), cb.in_cct, reason_to_string(cb.in_reason).c_str());

            dr_fprintf(output_file, "换出 backtrace \n");
            drcctlib_print_backtrace(output_file, cb.out_cct, false, true, 50 /*MAX_CCT_DEPTH*/);
            dr_fprintf(output_file, "换入 backtrace \n");
            drcctlib_print_backtrace(output_file, cb.in_cct, false, true, 50 /*MAX_CCT_DEPTH*/);
        }
    }
}

void L1Cache::print_cache_bump_fast(int topk, int num_print_event){
    dr_fprintf(output_file, "给出前 %d 次缓存颠簸地址记录\n", topk);

    // 先对原数组按addr排序（关键：排序后相同addr连续）
    std::sort(cache_bump_list.begin(), cache_bump_list.end(),
          [](const CacheBump& a, const CacheBump& b) {
              return a.addr < b.addr;
          });

    // 1. 使用unordered_map统计每个addr出现的次数
    std::unordered_map<uint64_t, std::vector<CacheBump>> addr_count;
    // 步骤2：遍历排序后的数组，批量分组，无哈希、无冲突、无多次扩容
    auto begin = cache_bump_list.begin();
    const auto end = cache_bump_list.end();
    while (begin != end) {
        const uint64_t curr_addr = begin->addr;
        // 找到第一个addr != curr_addr的位置，批量获取所有相同addr的元素
        auto split = std::partition_point(begin, end,
            [curr_addr](const CacheBump& bump) {
                return bump.addr == curr_addr;
            });
        // 一次性插入所有相同addr的元素，vector只扩容1次！
        addr_count[curr_addr].assign(begin, split);
        begin = split;
    }
    printf("缓存颠簸记录地址总数: %ld\n缓存颠簸记录总条数: %ld\n", addr_count.size(), cache_bump_list.size());
    dr_fprintf(output_file, "缓存颠簸记录地址总数: %ld\n缓存颠簸记录总条数: %ld\n", addr_count.size(), cache_bump_list.size());
    cache_bump_list.clear();

    std::vector<std::pair<uint64_t, std::vector<CacheBump>>> sorted_results;
    if (topk > (int)addr_count.size()) {
        topk = (int)addr_count.size(); // 防止 k 超过元素数量
        dr_fprintf(output_file, "由于该类型总计数量小于topk，故将topk调整为 ", topk);
    }
    sort_topk_cache_bump(topk, addr_count, sorted_results);
    addr_count.clear();
    
    // 2. 将统计结果转移到vector中以便排序
    // std::vector<std::pair<uint64_t, std::vector<CacheBump>>> sorted_results;
    // for (const auto& pair : addr_count) {
    //     sorted_results.push_back(pair);
    // }
    
    // 3. 按出现次数从高到低排序
    // std::sort(sorted_results.begin(), sorted_results.end(),
    //           [](const std::pair<uint64_t, std::vector<CacheBump>>& a, 
    //              const std::pair<uint64_t, std::vector<CacheBump>>& b) {
    //               return a.second.size() > b.second.size();  // 按次数降序排列
    //           });
    
    // 4. 输出结果
    dr_fprintf(output_file, "CacheBump addr出现次数统计（从高到低）：\n");

    for (const auto& result : sorted_results) {
        dr_fprintf(output_file, "addr %ld : cache bump cnt %d\n", result.first, result.second.size());
    }

    for (const auto& result : sorted_results) {
        dr_fprintf(output_file, "\n\n");
        dr_fprintf(output_file, "addr %ld : cache bump cnt %d\n", result.first, result.second.size());
        dr_fprintf(output_file, "对于地址 %lu 处发生的缓存颠簸事件详细信息(最多输出 %d 条事件):\n", result.first, num_print_event);
        int print_event_cnt = 1;
        for(CacheBump cb : result.second){
            if(print_event_cnt++ > num_print_event)
                break;
            dr_fprintf(output_file, "颠簸记录 >>>>>> \n");
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
    
    print_miss_cnt_topk(10, cct_coherence_miss_map, CacheMissReason::COHERENCE);    dr_fprintf(output_file, COHERENCE_MISS_OPTIMIZE.c_str()); 
    print_miss_cnt_topk(20, cct_capacity_miss_map, CacheMissReason::CAPACITY);      dr_fprintf(output_file, CAPACITY_MISS_OPTIMIZE.c_str()); 
    print_miss_cnt_topk(10, cct_conflict_miss_map, CacheMissReason::CONFLICT);      dr_fprintf(output_file, CONFLICT_MISS_OPTIMIZE.c_str()); 

    print_cache_bump_fast(10, 3);
}