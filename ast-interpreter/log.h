#ifndef __ZYY_LOG__
#define __ZYY_LOG__

#include <string>

// Overall debug flag
extern bool ZYY_DEBUG;

// separated debug flags
extern bool ValueBinding;

extern bool FunctionCall;

extern bool ControlStmt;

extern bool PointerVisit;

extern bool DeclVisit;

extern bool CastVisit;

extern bool HeapState;

extern bool ArrayVisit;

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

#define logp(debug_flags, p) \
    do {\
        if (ZYY_DEBUG && (debug_flags)) { \
            fprintf(stderr, "%s,%d: " "%s is %p\n", \
                    __func__, __LINE__, #p, p), \
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
