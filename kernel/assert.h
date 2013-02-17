
#ifndef _assert_h

#define _assert_h

#include <stdlib.h>

#ifdef _DEBUG

#define ALWAYS(statement, compare, v) ASSERT(statement compare v)

#ifdef _MSC_VER

#define ASSERT(f) \
do {                    \
    if (!(f))           \
    {                   \
        fprintf(stderr, "%s:%d: ASSERT failed: %s\n", __FILE__, __LINE__, #f);  \
        __asm {int 3}                                                           \
        _exit(-1);                                                              \
    }                                                                           \
} while (false)

#elif (defined(__GNUC__))

#define ASSERT(f) \
do {                    \
    if (!(f))           \
    {                   \
        fprintf(stderr, "%s:%d: ASSERT failed: %s\n", __FILE__, __LINE__, #f);  \
        __asm__ ("int $0x3" );                                                  \
        _exit(-1);                                                              \
    }                                                                           \
} while (false)

#else

#error "unknow compiler"

#endif

#define CASSERT(a)                      \
do {                                    \
    volatile void *p = (a) ? false : true; \
} while (false)

#else

#define ALWAYS(statement, compare, v) statement
#define ASSERT(f) {}

#define CASSERT(a) {}

#endif

#endif

