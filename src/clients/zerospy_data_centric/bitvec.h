#include <stdint.h>
#include <assert.h>
#include <stdlib.h>
#include "dr_api.h"
#include <unordered_map>
/************************************************/
/****************** Bit Vector ******************/
// 4K Page Size (4K * 8B = 32KB)
#define BITVEC_PAGE_SIZE (1<<12)
#define CEIL(a, b) (((a)+(b)-1)/(b))
struct bitvec_t { 
    union {
        uint64_t stat; // static 64 bits small cases
        uint64_t*  dyn; // dynamic allocate memory for bitvec within a page
        uint64_t** dyn_pages; // dynamic allocate memory for bitvec larger than 64
    } data;
    size_t size;
    size_t capacity;
};
typedef bitvec_t* bitref_t;
inline void bitvec_alloc(bitref_t bitref, size_t size) {
    bitref->size = size;
    if(size>BITVEC_PAGE_SIZE*64) {
        bitref->capacity = CEIL(CEIL(size,64) + 1, BITVEC_PAGE_SIZE);
        // guaranttee to be zero
        bitref->data.dyn_pages = (uint64_t**)dr_raw_mem_alloc(bitref->capacity*sizeof(uint64_t*), DR_MEMPROT_READ | DR_MEMPROT_WRITE, NULL);
        assert(bitref->data.dyn_pages!=NULL);
        memset(bitref->data.dyn_pages, 0, bitref->capacity*sizeof(uint64_t*));
    } else if(size>64) {
        /* FIXME i#5: Although upper bound of size/64 is usually enough, 
         * the compiler may generate overflowed memory access at the end 
         * of not-aligned data (maybe for better performance?). Here we 
         * just hot-fix the case by allocating one more capacity to avoid 
         * this kind of overflow. */
        bitref->capacity = CEIL(size,64) + 1;
        // bitref->capacity = (size+63)/64 + 1;
        //assert(bitref->capacity > 0);
        // Only Dynamic Malloc for large cases (>64 Bytes)
        bitref->data.dyn = (uint64_t*)dr_raw_mem_alloc(bitref->capacity*sizeof(uint64_t), DR_MEMPROT_READ | DR_MEMPROT_WRITE, NULL);
        assert(bitref->data.dyn!=NULL);
        memset(bitref->data.dyn, -1, bitref->capacity*sizeof(uint64_t));
        // // TODO: may be slow, use avx
        // memset(bitref->data.dyn, -1, sizeof(uint64_t)*(size+63)/64);
    } else {
        //bitref->capacity = 1;
        bitref->data.stat = -1; // 0xffffffffffffffffLL;
    }
}

inline void bitvec_free(bitref_t bitref) {
    if(bitref->size>64) {
        //free(bitref->data.dyn);
        if(bitref->size>BITVEC_PAGE_SIZE*64) {
            for(size_t i=0; i<bitref->capacity; ++i) {
                if(bitref->data.dyn_pages[i]) {
                    dr_raw_mem_free(bitref->data.dyn_pages[i], BITVEC_PAGE_SIZE*sizeof(uint64_t));
                }
            }
        }
        dr_raw_mem_free(bitref->data.dyn, bitref->capacity*sizeof(uint64_t*));
    }
}

inline void bitvec_and(bitref_t bitref, uint64_t val, size_t offset, size_t size) {
    if(bitref->size>BITVEC_PAGE_SIZE*64) {
        size_t bytePos = offset / 64;
        size_t bitPos = offset % 64;
        size_t rest = 64-bitPos;
        size_t pagePos = bytePos / BITVEC_PAGE_SIZE;
        size_t pageIdx = bytePos % BITVEC_PAGE_SIZE;
#ifdef DEBUG
        assert(pagePos<bitref->capacity);
#endif
        if(rest<size) {
            size_t pagePosP1 = (bytePos+1) / BITVEC_PAGE_SIZE;
            size_t pageIdxP1 = (bytePos+1) % BITVEC_PAGE_SIZE;
#ifdef DEBUG
            if(pagePosP1>=bitref->capacity) {
                printf("bitPos=%ld, bytePos=%ld, capacity=%ld, rest=%ld, size=%ld\n", bitPos, bytePos, bitref->capacity, rest, size);
                fflush(stdout);
            }
#endif 
            assert(pagePosP1<bitref->capacity);
            if(bitref->data.dyn_pages[pagePosP1]==NULL) {
                bitref->data.dyn_pages[pagePosP1] = (uint64_t*)dr_raw_mem_alloc(BITVEC_PAGE_SIZE*sizeof(uint64_t), DR_MEMPROT_READ | DR_MEMPROT_WRITE, NULL);
                assert(bitref->data.dyn_pages[pagePosP1]!=NULL);
                memset(bitref->data.dyn_pages[pagePosP1], -1, BITVEC_PAGE_SIZE*sizeof(uint64_t));
            }
            register uint64_t mask = (0x1LL << (size-rest)) - 1;
            mask = ~mask;
            bitref->data.dyn_pages[pagePosP1][pageIdxP1] &= ((val>>rest)|mask);
            size = rest;
        }
        if(bitref->data.dyn_pages[pagePos]==NULL) {
            bitref->data.dyn_pages[pagePos] = (uint64_t*)dr_raw_mem_alloc(BITVEC_PAGE_SIZE*sizeof(uint64_t), DR_MEMPROT_READ | DR_MEMPROT_WRITE, NULL);
            assert(bitref->data.dyn_pages[pagePos]!=NULL);
            memset(bitref->data.dyn_pages[pagePos], -1, BITVEC_PAGE_SIZE*sizeof(uint64_t));
        }
        register uint64_t mask = (0x1LL << size) - 1;
        mask = mask << bitPos;
        mask = ~mask;
        bitref->data.dyn_pages[pagePos][pageIdx] &= ((val<<bitPos)|mask);
    } else if(bitref->size>64) {
        size_t bytePos = offset / 64;
        size_t bitPos = offset % 64;
        size_t rest = 64-bitPos;
        if(rest<size) {
            register uint64_t mask = (0x1LL << (size-rest)) - 1;
            mask = ~mask;
            bitref->data.dyn[bytePos+1] &= ((val>>rest)|mask);
            size = rest;
        }
        register uint64_t mask = (0x1LL << size) - 1;
        mask = mask << bitPos;
        mask = ~mask;
        bitref->data.dyn[bytePos] &= ((val<<bitPos)|mask);
    } else {
        assert(offset<64);
        register uint64_t mask = (0x1LL << size) - 1;
        mask = mask << offset;
        mask = ~mask;
        bitref->data.stat &= ((val<<offset)|mask);
    }
}

inline bool bitvec_at(bitref_t bitref, size_t pos) {
    if(bitref->size>BITVEC_PAGE_SIZE*64) {
        size_t bytePos = pos / 64;
        size_t bitPos = pos % 64;
        size_t pagePos = bytePos / BITVEC_PAGE_SIZE;
        size_t pageIdx = bytePos % BITVEC_PAGE_SIZE;
#ifdef DEBUG
        assert(pagePos<bitref->capacity);
#endif
        if(bitref->data.dyn_pages[pagePos]==NULL) {
            return true;
        } else {
            if(bitPos!=0) {
                return (bitref->data.dyn_pages[pagePos][pageIdx] & (0x1LL << bitPos))!=0 ? true : false;
            } else {
                return (bitref->data.dyn_pages[pagePos][pageIdx] & (0x1LL))!=0 ? true : false;
            }
        }
    } else if(bitref->size>64) {
        size_t bytePos = pos / 64;
        size_t bitPos = pos % 64;
        if(bitPos!=0) {
            return (bitref->data.dyn[bytePos] & (0x1LL << bitPos))!=0 ? true : false;
        } else {
            return (bitref->data.dyn[bytePos] & (0x1LL))!=0 ? true : false;
        }
    } else {
        return (bitref->data.stat & (0x1LL << pos))!=0 ? true : false;
    }
}
/************************************************/