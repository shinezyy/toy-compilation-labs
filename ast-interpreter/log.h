#ifndef __ZYY_LOG__
#define __ZYY_LOG__

#include <string>

#define ZYY_DEBUG false

#define log(format,...) \
    do {\
        if (ZYY_DEBUG) { \
            fprintf(stderr, "%s,%d: " format, \
                    __func__, __LINE__, ## __VA_ARGS__), \
                    fflush(stderr); \
        }\
    } while(0)


#define log_var(x) \
    do {\
        if (ZYY_DEBUG) { \
            fprintf(stderr, "%s,%d: " "%s is %d\n", \
                    __func__, __LINE__, #x, x), \
                    fflush(stderr); \
        }\
    } while(0)

// short:
#define log_var_s(x) \
    do {\
        if (ZYY_DEBUG) { \
            fprintf(stderr, "%s is %d\n", \
                    #x, x), \
                    fflush(stderr); \
        }\
    } while(0)


#define log_var_cond(b, x) \
    do {\
        if (ZYY_DEBUG && b) { \
            fprintf(stderr, "%s,%d: " "%s is %d\n", \
                   __func__, __LINE__, #x, x), \
                fflush(stderr); \
        }\
    } while(0)


#endif
