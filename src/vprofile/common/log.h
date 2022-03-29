#ifndef __VPROFILE_LOG_H__
#define __VPROFILE_LOG_H__

#include "dr_api.h"
extern file_t dFile;

enum LOG_LV {
    ALWAYS=0,
    SUMMARY,
    DETAIL,
    EVERYTHING
};

const char* level2str(int level);

extern int gLevel;

#define LOG_INIT(level) do {\
    gLevel=level;\
    dFile=dr_open_file("vprofile.log", DR_FILE_WRITE_OVERWRITE | DR_FILE_ALLOW_LARGE);\
    DR_ASSERT(dFile!=INVALID_FILE);\
} while(0)
#define LOG(prefix, level, format, args...) \
    if(level<=gLevel) dr_fprintf(dFile, "%s" prefix format, level2str(level), ##args)
#define LOG_FINI() dr_close_file(dFile)

#endif