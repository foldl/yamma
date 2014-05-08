
#ifndef _assert_h

#define _assert_h

#include <stdlib.h>

#ifdef _DEBUG

#define ALWAYS(statement, compare, v) ASSERT(statement compare v)

#ifdef _MSC_VER
    #define raise_int   __asm {int 3}
#endif

#if (defined(__GNUC__) && defined(__MSVCRT__))
    #define raise_int   __asm__ ("int $0x3" );
#endif

#ifndef raise_int
    #define raise_int
#endif

#define ASSERT(f) \
do {                    \
    if (!(f))           \
    {                   \
        fprintf(stderr, "%s:%d: ASSERT failed: %s\n", __FILE__, __LINE__, #f);  \
        raise_int                                                               \
        exit(-1);                                                              \
    }                                                                           \
} while (false)

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

