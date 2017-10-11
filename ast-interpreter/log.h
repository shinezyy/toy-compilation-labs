#ifndef __ZYY_LOG__
#define __ZYY_LOG__

#include <string>

// Overall debug flag
#define ZYY_DEBUG true

// separated debug flags
#define ValueBinding false

#define FunctionCall false

#define ControlStmt false

#define PointerVisit true

#define log(debug_flags, format,...) \
    do {\
        if (ZYY_DEBUG && (debug_flags)) { \
            fprintf(stderr, "%s,%d: " format, \
                    __func__, __LINE__, ## __VA_ARGS__), \
                    fflush(stderr); \
        }\
    } while(false)

#define logs(debug_flags, str) \
    do {\
        if (ZYY_DEBUG && (debug_flags)) { \
            fprintf(stderr, "%s,%d: " str, \
                    __func__, __LINE__), \
                    fflush(stderr); \
        }\
    } while(false)

#define log_var(debug_flags, x) \
    do {\
        if (ZYY_DEBUG && (debug_flags)) { \
            fprintf(stderr, "%s,%d: " "%s is %d\n", \
                    __func__, __LINE__, #x, x), \
                    fflush(stderr); \
        }\
    } while(false)

// short:
#define log_var_s(debug_flags, x) \
    do {\
        if (ZYY_DEBUG && (debug_flags)) { \
            fprintf(stderr, "%s is %d\n", \
                    #x, x), \
                    fflush(stderr); \
        }\
    } while(false)


#define log_var_cond(debug_flags, condition, x) \
    do {\
        if (ZYY_DEBUG && (debug_flags) && (condition)) { \
            fprintf(stderr, "%s,%d: " "%s is %d\n", \
                   __func__, __LINE__, #x, x), \
                fflush(stderr); \
        }\
    } while(false)


#endif
