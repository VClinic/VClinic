#ifndef __TRIVIAL_DEFINE_H__
#define __TRIVIAL_DEFINE_H__

#include <string.h>
#include <unistd.h>
#include <assert.h>

#include "dr_api.h"
#include "drreg.h"

// #define DEBUG_TRIVIAL
// #define DEBUG_COMBINED

#ifdef DEBUG_TRIVIALSPY
#define DPRINTF(args...) dr_fprintf(STDOUT, args)
#define IF_DEBUG_TRIVIAL(stat...) stat
#else
#define DPRINTF(args...)
#define IF_DEBUG_TRIVIAL(stat...)
#endif

#define TRIVIALSPY_PRINTF_TEMPLATE(client, format, args...)                        \
    do {                                                                         \
        char name[MAXIMUM_PATH] = "";                                            \
        gethostname(name + strlen(name), MAXIMUM_PATH - strlen(name));           \
        pid_t pid = getpid();                                                    \
        dr_printf("[trivialspy[" client "](%s%d) msg]====" format "\n", name, pid, \
                  ##args);                                                       \
    } while (0)

#define TRIVIALSPY_CLIENT_EXIT_PROCESS_TEMPLATE(client, format, args...)           \
    do {                                                                         \
        char name[MAXIMUM_PATH] = "";                                            \
        gethostname(name + strlen(name), MAXIMUM_PATH - strlen(name));           \
        pid_t pid = getpid();                                                    \
        dr_printf("[trivialspy[" client "](%s%d) msg]====" format "\n", name, pid, \
                  ##args);                                                       \
    } while (0);                                                                 \
    dr_exit_process(-1)

#define TRIVIALSPY_PRINTF(format, args...) \
    TRIVIALSPY_PRINTF_TEMPLATE("trivialspy", format, ##args)
#define TRIVIALSPY_EXIT_PROCESS(format, args...)                                           \
    TRIVIALSPY_CLIENT_EXIT_PROCESS_TEMPLATE("trivialspy", format, \
                                          ##args)


#define TLS_SLOT(tls_base, offs) (void **)((byte *)(tls_base) + (offs))
#define BUF_PTR(tls_base, offs) *(byte **)TLS_SLOT(tls_base, offs)

#define MINSERT instrlist_meta_preinsert

#ifdef RESERVE_REG
#undef RESERVE_REG
#undef UNRESERVE_REG
#endif

#define RESERVE_REG(dc, bb, instr, vec, reg) do {\
    if (drreg_reserve_register(dc, bb, instr, vec, &reg) != DRREG_SUCCESS) { \
        TRIVIALSPY_EXIT_PROCESS("ERROR @ %s:%d: drreg_reserve_register != DRREG_SUCCESS", __FILE__, __LINE__); \
    } } while(0)
#define UNRESERVE_REG(dc, bb, instr, reg) do { \
    if (drreg_unreserve_register(dc, bb, instr, reg) != DRREG_SUCCESS) { \
        TRIVIALSPY_EXIT_PROCESS("ERROR @ %s:%d: drreg_unreserve_register != DRREG_SUCCESS", __FILE__, __LINE__); \
    } } while(0)

#endif