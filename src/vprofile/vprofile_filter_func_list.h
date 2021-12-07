#ifndef __VPROFILE_FILTER_FUNC_LIST_H__
#define __VPROFILE_FILTER_FUNC_LIST_H__
#include "dr_api.h"

/* TODO: Instruction Filters, including all instr, memory only, mem read only, mem write only */
DR_EXPORT
bool 
VPROFILE_FILTER_ALL_INSTR(instr_t* instr) {
    return true;
}

DR_EXPORT
bool
VPROFILE_FILTER_MEM_ACCESS_INSTR(instr_t *instr)
{
    return (instr_reads_memory(instr) || instr_writes_memory(instr));
}

DR_EXPORT
bool
VPROFILE_FILTER_MEM_READ_INSTR(instr_t *instr)
{
    return instr_reads_memory(instr);
}

DR_EXPORT
bool
VPROFILE_FILTER_MEM_WRITE_INSTR(instr_t *instr)
{
    return instr_writes_memory(instr);
}

/* TODO: Operand Filters, including all opnd, mem only, mem read only, mem write only, reg only, reg read only, reg write only */
DR_EXPORT
bool 
VPROFILE_FILTER_ALL_OPND(opnd_t opnd, vprofile_src_t opmask) {
    return true;
}

#define VPROFILE_DEFAULT_OPND_FILTER VPROFILE_FILTER_ALL_OPND

#endif