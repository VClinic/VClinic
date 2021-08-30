#ifndef __VPROFILE_FILTER_FUNC_LIST_H__
#define __VPROFILE_FILTER_FUNC_LIST_H__
#include "dr_api.h"

/* TODO: Instruction Filters, including all instr, memory only, mem read only, mem write only */
bool VPROFILE_FILTER_ALL_INSTR(instr_t* instr) {
    return true;
}

/* TODO: Operand Filters, including all opnd, mem only, mem read only, mem write only, reg only, reg read only, reg write only */
bool VPROFILE_FILTER_ALL_OPND(opnd_t opnd) {
    return true;
}

#endif