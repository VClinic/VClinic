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