#ifndef __VTRACER_DEFINE_H__
#define __VTRACER_DEFINE_H__

#include <string.h>
#include <unistd.h>
#include <assert.h>

#include "dr_api.h"
#include "drreg.h"

#define VTRACER_CLIENT_EXIT_PROCESS_TEMPLATE(client, format, args...)           \
    do {                                                                         \
        char name[MAXIMUM_PATH] = "";                                            \
        gethostname(name + strlen(name), MAXIMUM_PATH - strlen(name));           \
        pid_t pid = getpid();                                                    \
        dr_printf("[vtracer[" client "](%s%d) msg]====" format "\n", name, pid, \
                  ##args);                                                       \
    } while (0);                                                                 \
    dr_exit_process(-1)

#define VTRACER_EXIT_PROCESS(format, args...)                                           \
    VTRACER_CLIENT_EXIT_PROCESS_TEMPLATE("vtracer", format, \
                                          ##args)

#define TLS_SLOT(tls_base, offs) (void **)((byte *)(tls_base) + (offs))
#define BUF_PTR(tls_base, offs) *(byte **)TLS_SLOT(tls_base, offs)

#define MINSERT instrlist_meta_preinsert

// use manual inlined updates
#define RESERVE_AFLAGS(dc, bb, ins) assert(drreg_reserve_aflags (dc, bb, ins)==DRREG_SUCCESS)
#define UNRESERVE_AFLAGS(dc, bb, ins) assert(drreg_unreserve_aflags (dc, bb, ins)==DRREG_SUCCESS)

#define RESERVE_REG(dc, bb, instr, vec, reg) do {\
    if (drreg_reserve_register(dc, bb, instr, vec, &reg) != DRREG_SUCCESS) { \
        VTRACER_EXIT_PROCESS("ERROR @ %s:%d: drreg_reserve_register != DRREG_SUCCESS", __FILE__, __LINE__); \
    } } while(0)
#define UNRESERVE_REG(dc, bb, instr, reg) do { \
    if (drreg_unreserve_register(dc, bb, instr, reg) != DRREG_SUCCESS) { \
        VTRACER_EXIT_PROCESS("ERROR @ %s:%d: drreg_unreserve_register != DRREG_SUCCESS", __FILE__, __LINE__); \
    } } while(0)

#endif

inline void fillUnusedRegEntry(drvector_t* allowed, opnd_t opnd) {
    // the regs used in this instr is not allowd to spill
    for (int i = opnd_num_regs_used(opnd) - 1; i >= 0; i--) {
        reg_id_t reg_used = opnd_get_reg_used(opnd, i);
        drreg_set_vector_entry(allowed, reg_used, false);
        // resize for simd or gpr, mmx regs are not supported!
        // resize for simd may occur error
        if(!reg_is_gpr(reg_used) || reg_is_simd(reg_used) || reg_is_mmx(reg_used)) {
            continue;
        }
        drreg_set_vector_entry(allowed, reg_resize_to_opsz(reg_used, OPSZ_1), false);
        drreg_set_vector_entry(allowed, reg_resize_to_opsz(reg_used, OPSZ_2), false);
        drreg_set_vector_entry(allowed, reg_resize_to_opsz(reg_used, OPSZ_4), false);
        drreg_set_vector_entry(allowed, reg_resize_to_opsz(reg_used, OPSZ_8), false);
    }
}

inline void getUnusedRegEntry(drvector_t* allowed, opnd_t opnd) {
    drreg_init_and_fill_vector(allowed, true);
    // the regs used in this instr is not allowd to spill
    fillUnusedRegEntry(allowed, opnd);
}

inline void getUnusedRegEntryInstr(drvector_t* allowed, instr_t* instr) {
    int num;
    drreg_init_and_fill_vector(allowed, true);
    num = instr_num_srcs(instr);
    for(int j = 0; j < num; j++) {
        fillUnusedRegEntry(allowed, instr_get_src(instr, j));
    }
    num = instr_num_dsts(instr);
    for(int j = 0; j < num; j++) {
        fillUnusedRegEntry(allowed, instr_get_dst(instr, j));
    }
}

inline void getUnusedRegEntryInstrWithoutInit(drvector_t* allowed, instr_t* instr) {
    int num;
    num = instr_num_srcs(instr);
    for(int j = 0; j < num; j++) {
        fillUnusedRegEntry(allowed, instr_get_src(instr, j));
    }
    num = instr_num_dsts(instr);
    for(int j = 0; j < num; j++) {
        fillUnusedRegEntry(allowed, instr_get_dst(instr, j));
    }
}