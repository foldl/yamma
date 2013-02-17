
#ifndef mtype_h
#define mtype_h

typedef unsigned short MChar;

typedef void *  MPtr;
typedef int     MInt;
typedef unsigned int MUInt;
typedef double  MReal;
typedef bool    MBool;

typedef struct _dummy_struct_type_ {} *MUnknown;

#define M_MAX_INT   0x7fffffff

#if (defined(_MSC_VER) || defined(__BORLANDC_))

    typedef unsigned __int8    MByte;
    typedef unsigned __int16   MWord;
    typedef   signed __int16   MSWord;  // MS Word?
    typedef unsigned __int32   MDword;

#elif defined(__GNUC__)

#include <stdint.h>
    typedef uint8_t    MByte;
    typedef uint16_t   MWord;
    typedef int32_t    MSWord;  // MS Word?
    typedef uint32_t   MDword;

#endif

#define INTERNAL_NAME_LEN   (100)

#ifndef NULL
#define NULL 0
#endif

#ifndef max
#define min(a,b) (((a) < (b)) ? (a) : (b))
#define max(a,b) (((a) > (b)) ? (a) : (b))
#endif

#define arr_size(s) (sizeof(s) / sizeof(s[0]))
#endif
