#include "log.h"
file_t dFile = INVALID_FILE;
int gLevel = LOG_LV::ALWAYS;

const char* level2str(int level) {
    switch(level) {
        case ALWAYS: return "[ALWAYS]";
        case SUMMARY: return "[SUMMARY]";
        case DETAIL: return "[DETAIL]";
        case EVERYTHING: return "[EVERYTHING]";
        default:
            return "[Unknown]";
    }
    return "";
}