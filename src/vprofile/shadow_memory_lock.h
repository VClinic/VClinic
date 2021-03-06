/* 
 *  Copyright (c) 2020-2021 Xuhpclab. All rights reserved.
 *  Licensed under the MIT License.
 *  See LICENSE file for more information.
 */

#ifndef __SHADOW_MEMORY__
#define __SHADOW_MEMORY__

#include <stdint.h>
#include <atomic>
#include <tuple>
#include <dr_api.h>

#define SHADOW_MEM_PRINTF(_FORMAT, _ARGS...)                        \
    do {                                                          \
        dr_printf("[shadow_memory msg]====" _FORMAT "\n", ##_ARGS); \
    } while (0)

#define SHADOW_MEM_EXIT_PROCESS(_FORMAT, _ARGS...)                  \
    do {                                                          \
        dr_printf("[shadow_memory msg]====" _FORMAT "\n", ##_ARGS); \
    } while (0);                                                  \
    dr_exit_process(-1)

#define SHADOW_MEMORY_TEST 0

// unit size
#define PTR_SIZE (sizeof(void *))
// 2 level page table
// 1MB
#define LEVEL_1_PAGE_TABLE_BITS (20)
#define LEVEL_1_PAGE_TABLE_ENTRIES (1 << LEVEL_1_PAGE_TABLE_BITS)
#define LEVEL_1_PAGE_TABLE_SIZE (LEVEL_1_PAGE_TABLE_ENTRIES * PTR_SIZE)
// 4KB
#define LEVEL_2_PAGE_TABLE_BITS (12)
#define LEVEL_2_PAGE_TABLE_ENTRIES (1 << LEVEL_2_PAGE_TABLE_BITS)
#define LEVEL_2_PAGE_TABLE_SIZE (LEVEL_2_PAGE_TABLE_ENTRIES * PTR_SIZE)
// 64KB
#define PAGE_OFFSET_BITS (16LL)
#define PAGE_OFFSET(addr) (addr & 0xFFFF)
#define PAGE_OFFSET_MASK (0xFFFF)
#define SHADOW_PAGE_SIZE (1 << PAGE_OFFSET_BITS)

#define LEVEL_1_PAGE_TABLE_SLOT(addr) \
    ((((uint64_t)addr) >> (LEVEL_2_PAGE_TABLE_BITS + PAGE_OFFSET_BITS)) & 0xfffff)
#define LEVEL_2_PAGE_TABLE_SLOT(addr) ((((uint64_t)addr) >> (PAGE_OFFSET_BITS)) & 0xFFF)

static void *gLock;

using namespace std;

template <class T> class ConcurrentShadowMemory {
    // All fwd declarations
    atomic<atomic<T *> *> *pageDirectory;
    // Given a address generated by the program, returns the corresponding shadow address
    // FLOORED to  SHADOW_PAGE_SIZE If the shadow page does not exist a new one is MMAPed
public:
    inline ConcurrentShadowMemory()
    {
        atomic<atomic<T *> *> *nullPd = 0;
        pageDirectory = (atomic<atomic<T *> *> *)dr_raw_mem_alloc(
            LEVEL_1_PAGE_TABLE_SIZE, DR_MEMPROT_READ | DR_MEMPROT_WRITE, NULL);

        if (pageDirectory == nullPd) {
            SHADOW_MEM_EXIT_PROCESS("dr_raw_mem_alloc fail pageDirectory");
        }
    }

    inline ~ConcurrentShadowMemory()
    {
        for (uint64_t i = 0; i < LEVEL_1_PAGE_TABLE_ENTRIES; i++) {
            atomic<T *> *l1Page;
            if ((l1Page = pageDirectory[i].load(memory_order_relaxed)) != 0) {
                for (uint64_t j = 0; j < LEVEL_2_PAGE_TABLE_ENTRIES; j++) {
                    T *l2Page;
                    if ((l2Page = l1Page[j].load(memory_order_relaxed)) != 0) {
                        dr_raw_mem_free(l2Page, SHADOW_PAGE_SIZE * sizeof(T));
                    }
                }
                dr_raw_mem_free(l1Page, LEVEL_2_PAGE_TABLE_SIZE);
            }
        }
        dr_raw_mem_free(pageDirectory, LEVEL_1_PAGE_TABLE_SIZE);
    }

    inline T *
    GetOrCreateShadowBaseAddress(const size_t address)
    {
        atomic<atomic<T *> *> *l1Ptr = &pageDirectory[LEVEL_1_PAGE_TABLE_SLOT(address)];
        atomic<T *> *v1;
        if ((v1 = l1Ptr->load(memory_order_consume)) == 0) {
            atomic<T *> *nullL1pg = 0;
            atomic<T *> *l1pg = (atomic<T *> *)dr_raw_mem_alloc(
                LEVEL_2_PAGE_TABLE_SIZE, DR_MEMPROT_READ | DR_MEMPROT_WRITE, NULL);
            if (l1pg == nullL1pg) {
                SHADOW_MEM_EXIT_PROCESS("dr_raw_mem_alloc fail l1pg");
            }

            if (!l1Ptr->compare_exchange_strong(nullL1pg, l1pg, memory_order_acq_rel,
                                                memory_order_relaxed)) {
                dr_raw_mem_free(l1pg, LEVEL_2_PAGE_TABLE_SIZE);
                v1 = l1Ptr->load(memory_order_consume);
            } else {
                v1 = l1pg;
            }
        }

        atomic<T *> *l2Ptr = &v1[LEVEL_2_PAGE_TABLE_SLOT(address)];
        T *v2;
        if ((v2 = l2Ptr->load(memory_order_consume)) == 0) {
            T *nullVal = 0;
            dr_mutex_lock(gLock);
            T *l2pg = (T *)dr_raw_mem_alloc(SHADOW_PAGE_SIZE * sizeof(T),
                                            DR_MEMPROT_READ | DR_MEMPROT_WRITE, NULL);
            dr_mutex_unlock(gLock);
            if (l2pg == nullVal) {
                SHADOW_MEM_EXIT_PROCESS("dr_raw_mem_alloc fail l2pg");
            }
            if (!l2Ptr->compare_exchange_strong(nullVal, l2pg, memory_order_acq_rel,
                                                memory_order_relaxed)) {
                dr_raw_mem_free(l2pg, SHADOW_PAGE_SIZE * sizeof(T));
                v2 = l2Ptr->load(memory_order_consume);
            } else {
                v2 = l2pg;
            }
        }
        return v2;
    }

    inline T *
    GetOrCreateShadowAddress(const size_t address)
    {
        T *shadowPage = GetOrCreateShadowBaseAddress(address);
        return &(shadowPage[PAGE_OFFSET((uint64_t)address)]);
    }

    inline T *
    GetShadowBaseAddress(const size_t address)
    {
        atomic<atomic<T *> *> *l1Ptr = &pageDirectory[LEVEL_1_PAGE_TABLE_SLOT(address)];
        atomic<T *> *v1;
        if ((v1 = l1Ptr->load(memory_order_consume)) == 0) {
            return NULL;
        }

        atomic<T *> *l2Ptr = &v1[LEVEL_2_PAGE_TABLE_SLOT(address)];
        T *v2;
        if ((v2 = l2Ptr->load(memory_order_consume)) == 0) {
            return NULL;
        }
        return v2;
    }

    inline T *
    GetShadowAddress(const size_t address)
    {
        T *shadowPage = GetShadowBaseAddress(address);
        if (shadowPage == NULL) {
            return NULL;
        }
        return &(shadowPage[PAGE_OFFSET((uint64_t)address)]);
    }
};

template <class T> class TlsShadowMemory {
    T *** pageDirectory;
public:
    inline TlsShadowMemory()
    {
        pageDirectory = (T ***)dr_raw_mem_alloc(
            LEVEL_1_PAGE_TABLE_SIZE, DR_MEMPROT_READ | DR_MEMPROT_WRITE, NULL);

        if (pageDirectory == NULL) {
            SHADOW_MEM_EXIT_PROCESS("TlsShadowMemory dr_raw_mem_alloc fail pageDirectory");
        }
    }

    inline ~TlsShadowMemory()
    {
        for (uint64_t i = 0; i < LEVEL_1_PAGE_TABLE_ENTRIES; i++) {
            T ** l1Page = pageDirectory[i];
            if (l1Page != NULL) {
                for (uint64_t j = 0; j < LEVEL_2_PAGE_TABLE_ENTRIES; j++) {
                    T *l2Page = l1Page[j];
                    if (l2Page != 0) {
                        dr_raw_mem_free(l2Page, SHADOW_PAGE_SIZE * sizeof(T));
                    }
                }
                dr_raw_mem_free(l1Page, LEVEL_2_PAGE_TABLE_SIZE);
            }
        }
        dr_raw_mem_free(pageDirectory, LEVEL_1_PAGE_TABLE_SIZE);
    }

    inline T *
    GetOrCreateShadowBaseAddress(const size_t address)
    {
        T ** l1pg = pageDirectory[LEVEL_1_PAGE_TABLE_SLOT(address)];
        if (l1pg == NULL) {
            l1pg = (T **)dr_raw_mem_alloc(
                LEVEL_2_PAGE_TABLE_SIZE, DR_MEMPROT_READ | DR_MEMPROT_WRITE, NULL);
            if (l1pg == NULL) {
                SHADOW_MEM_EXIT_PROCESS("TlsShadowMemory dr_raw_mem_alloc fail l1pg");
            }
            pageDirectory[LEVEL_1_PAGE_TABLE_SLOT(address)] = l1pg;
        }
        T* l2pg = l1pg[LEVEL_2_PAGE_TABLE_SLOT(address)];
        if (l2pg == NULL) {
            l2pg = (T *)dr_raw_mem_alloc(
                SHADOW_PAGE_SIZE * sizeof(T), DR_MEMPROT_READ | DR_MEMPROT_WRITE, NULL);
            if (l2pg == NULL) {
                SHADOW_MEM_EXIT_PROCESS("TlsShadowMemory dr_raw_mem_alloc fail l2pg");
            }
            l1pg[LEVEL_2_PAGE_TABLE_SLOT(address)] = l2pg;
        }
        return l2pg;
    }

    inline T *
    GetOrCreateShadowAddress(const size_t address)
    {
        T *shadowPage = GetOrCreateShadowBaseAddress(address);
        return &(shadowPage[PAGE_OFFSET((uint64_t)address)]);
    }

    inline T *
    GetShadowBaseAddress(const size_t address)
    {
        if (pageDirectory[LEVEL_1_PAGE_TABLE_SLOT(address)] == NULL) {
            return NULL;
        }
        return pageDirectory[LEVEL_1_PAGE_TABLE_SLOT(address)][LEVEL_2_PAGE_TABLE_SLOT(address)];
    }

    inline T *
    GetShadowAddress(const size_t address)
    {
        T *shadowPage = GetShadowBaseAddress(address);
        if (shadowPage == NULL) {
            return NULL;
        }
        return &(shadowPage[PAGE_OFFSET((uint64_t)address)]);
    }
};


#endif // __SHADOW_MEMORY__