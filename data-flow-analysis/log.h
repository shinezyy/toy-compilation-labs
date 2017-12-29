#ifndef __LOG_HPP__
#define __LOG_HPP__

#include <cstdio>
#include <cerrno>
#include <cstring>

// #define LOG_DEBUG

enum log_level { DEBUG = 0, INFO, WARNING, ERROR };

extern enum log_level this_log_level;

extern const char *log_level_str[];

extern bool DEBUG_ALL;

extern bool FuncVisit;

extern bool PhiVisit;

extern bool PtrSet;

extern bool EnvDebug;

extern bool LdStVisit;


#ifdef LOG_DEBUG
    #define log_it(fmt, level_str, ...) \
        fprintf(stderr, "[%s:%u] %s: " fmt  "\n", __FILE__, __LINE__, \
                level_str, ##__VA_ARGS__);
#else
    #define log_it(fmt, level_str, ...) \
        fprintf(stderr, "%s: " fmt " [%s]\n", level_str, ##__VA_ARGS__, __FUNCTION__);
#endif

#define log_it_s(fmt, level_str) \
        fprintf(stderr, "%s: " fmt " [%s]\n", level_str, __FUNCTION__);

#define log_it_raw(fmt, ...) \
        fprintf(stderr, fmt , ##__VA_ARGS__);

#define log_it_raw_s(fmt) \
        fprintf(stderr, fmt);

#define log(level, debug_flags, fmt, ...) \
    do { \
        if (DEBUG_ALL && (debug_flags)) {\
            if ((level) < this_log_level) \
            break; \
            log_it(fmt, log_level_str[level], ##__VA_ARGS__); \
        }\
    } while (false)

#define logs(level, debug_flags, fmt) \
    do { \
        if (DEBUG_ALL && (debug_flags)) {\
            if ((level) < this_log_level) \
            break; \
            log_it_s(fmt, log_level_str[level]); \
        }\
    } while (false)

#define log_raw(level, debug_flags, fmt, ...) \
    do { \
        if (DEBUG_ALL && (debug_flags)) {\
            if ((level) < this_log_level) \
            break; \
            log_it_raw(fmt, ##__VA_ARGS__); \
        }\
    } while (false)

#endif
