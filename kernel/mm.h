#ifndef mm_h
#define mm_h

#include <stdio.h>

#include "mtype.h"
#include "assert.h"

//#define MEM_VERIFY
//#define GC_METHOD GC_METHOD_GENERATION

#define GC_METHOD_DEF           0
#define GC_METHOD_GENERATION    1

#ifndef GC_METHOD
#   define GC_METHOD GC_METHOD_DEF
#endif

#define OBJ_ATT_STANDALONE  (0x1 << 0)
#define OBJ_VERBOSE         (0x1 << 1)
#define OBJ_UNDER_WATCH     (0x1 << 2)
#define OBJ_LOCKED          (0x1 << 3)

#define OBJ_ATT_USER_START  (4)

typedef void *(*f_YM_malloc)(const int size);
typedef void *(*f_YM_realloc)(void *p, const int size);
typedef void  (*f_YM_free)(void *p);

void *YM_malloc(const int size);
void *YM_realloc(void *p, const int size);
void  YM_free(void *p);

struct MMemMan
{
    f_YM_malloc  YM_malloc;
    f_YM_realloc YM_realloc;
    f_YM_free    YM_free;
};

typedef void * (*f_YM_XRefObjDispose)(void *p);
typedef void * (*f_YM_XRefObjInit)(void *p);
typedef char * (*f_YM_XRefObjDump)(void *p, char *s, const MInt maxlen);

struct MXRefObjMT
{
    f_YM_XRefObjInit    _Init;
    f_YM_XRefObjDispose _Dispose;   
    f_YM_XRefObjDump    _Dump;
};

typedef void * MXRefObjType;

struct MXRefObjHeader
{
#ifdef MEM_VERIFY
    MDword token1;
#endif

    MXRefObjHeader *next;
#if (GC_METHOD == GC_METHOD_DEF)
    MXRefObjHeader *prev_alloc;
#endif
    MXRefObjType Type;
    MDword Generation;
    MDword Attrib;
    MInt   _RefCount;

#ifdef MEM_VERIFY
    MDword token2;
#endif
};

#define XRef_DATA(p) ((char *)(p) + sizeof(MXRefObjHeader))
#define DATA_XRef(p) ((MXRefObjHeader *)((char *)(p) - sizeof(MXRefObjHeader)))
#define XRef_CNT(p)  (DATA_XRef(p)->_RefCount)

#ifdef _DEBUG

#define XRef_IncRef(p)      \
do                          \
{                           \
    void *p_31415936 = (p);                 \
    ASSERT(DATA_XRef(p_31415936)->_RefCount >= 0);\
    DATA_XRef(p_31415936)->_RefCount++;     \
}  while(0 == 1)

// more checks:
// ASSERT((DATA_XRef(p_31415936)->_RefCount) >= 0); 

#if (GC_METHOD == GC_METHOD_GENERATION)

// more checks:
// ASSERT((h_314159->_RefCount) >= 0);                             
// if (h_314159->Attrib & OBJ_VERBOSE) printf("XRef_DecRef: 0x%08X, %d\n", p_31415936, h_314159->_RefCount); 

#define XRef_DecRef(p)  \
do                                                                      \
{                                                                       \
    void *p_31415936 = (p);                                             \
    ASSERT(p_31415936);                                                 \
    do {                                                                \
        MXRefObjHeader *h_314159 = DATA_XRef(p_31415936);               \
        ASSERT(h_314159->_RefCount >= 0);                               \
        h_314159->_RefCount--;                                          \
    } while (false);                                                    \
} while (false)

#elif (GC_METHOD == GC_METHOD_DEF)

// more checks:
// ASSERT((h_314159->_RefCount) >= 0);                             
// if (h_314159->Attrib & OBJ_VERBOSE) printf("XRef_DecRef: 0x%08X, %d\n", p_31415936, h_314159->_RefCount); 

#define XRef_DecRef(p)  \
do                                                                      \
{                                                                       \
    void *p_31415936 = (p);                                             \
    ASSERT(p_31415936);                                                 \
    do {                                                                \
        MXRefObjHeader *h_314159 = DATA_XRef(p_31415936);               \
        ASSERT(h_314159->_RefCount >= 0);                               \
        if (h_314159->_RefCount == 0)                                   \
        {                                                               \
            if ((h_314159->Attrib & OBJ_LOCKED) == 0)                   \
                MXRefObj_Free((MUnknown)(h_314159));                    \
            else                                                        \
                MXRefObj_FreeLocked((MUnknown)(h_314159));              \
        }                                                               \
        else h_314159->_RefCount--;                                     \
    } while (false);                                                    \
} while (false)

#endif      



#else

#define XRef_IncRef(p)      \
do                          \
{                           \
    DATA_XRef(p)->_RefCount++; \
}  while(0 == 1)

#if (GC_METHOD == GC_METHOD_GENERATION)

#define XRef_DecRef(p)  \
do                             \
{                              \
    DATA_XRef(p)->_RefCount--; \
} while (false)

#elif (GC_METHOD == GC_METHOD_DEF)

#define XRef_DecRef(p)  \
do                                                                      \
{                                                                       \
    void *p_31415936 = (p);                                             \
    {                                                                   \
        MXRefObjHeader *h_314159 = DATA_XRef(p_31415936);               \
        if (h_314159->_RefCount == 0)                                   \
        {                                                               \
            if ((h_314159->Attrib & OBJ_LOCKED) == 0)                   \
                MXRefObj_Free((MUnknown)(h_314159));                    \
            else                                                        \
                MXRefObj_FreeLocked((MUnknown)(h_314159));              \
        }                                                               \
        else h_314159->_RefCount--;                                     \
    }                                                                   \
} while (false)

#endif

#endif

// don't call these functions directly, because you don't know the type of "punknown".
void MXRefObj_Free(MUnknown punknown);
bool MXRefObj_Verify(void *p);
void MXRefObj_FreeLocked(MUnknown p);

void        *MXRefObj_CreateVoid(const MXRefObjType pt);
MXRefObjType MXRefObj_Register(const MXRefObjMT *pmt, const int size, const char *name);

#define GC_COLLECT_FREE     1
#define GC_FREE_FREE_LIST   2
void MXRefObj_GC(const MDword Option);

#define MXRefObj_Create(T, pt) ((T *)(MXRefObj_CreateVoid(pt)))

void MXRef_Dump(FILE *file);
void MXRef_DumpType(FILE *file, MXRefObjType t);
void MXRefObj_Info(void *p, FILE *f);

typedef void *MHeap;

void *MHeap_malloc(MHeap Heap, const int size);
void *MHeap_realloc(MHeap Heap, void *p, const int size);
void  MHeap_free(MHeap Heap, void *p);

MHeap MHeap_NewHeap(const char *name, const int HeapSize);
void  MHeap_Dump(MHeap Heap, FILE *file);

// break on an address
//    if ((int)(p) == 0xB42614) __asm {int 3} 
#define Zero(p, size)                       \
do {                                        \
    memset((p), 0, (size));                 \
} while (false)

#define ASSERT_REF_CNT(obj, n) ASSERT(DATA_XRef(obj)->_RefCount == n)

#define MObj_GetGen(obj)     ((const MDword)(DATA_XRef(obj)->Generation))
#define MObj_RefCnt(obj)     (DATA_XRef(obj)->_RefCount)
#define MObj_SetRefCnt(obj, n)     (DATA_XRef(obj)->_RefCount = (n))
#define MObj_IsUnique(obj)   (DATA_XRef(obj)->_RefCount == 0)
#define MObj_SetAttrib(obj, att)   (DATA_XRef(obj)->Attrib |= (att))
#define MObj_ClearAttrib(obj, att) (DATA_XRef(obj)->Attrib &= ~(att))
#define MObj_HasAttrib(obj, att)   ((DATA_XRef(obj)->Attrib & (att)) != 0)
#define MObj_HasAll(obj, att)      ((DATA_XRef(obj)->Attrib & (att)) == (att))
#define MObj_Lock(obj) MObj_SetAttrib(obj, OBJ_LOCKED)
#define MObj_Unlock(obj) MObj_ClearAttrib(obj, OBJ_LOCKED)

// ===================================
// Here, a object watchdog is presented
// Watchdog introduction:
//      1. create a list of objects, which may have some special properties
//      2. these objects can be released normally
//      3. after things are done, it's possible to perform some actions (cleanup, etc) on 
//          these objects that have not been released.
// ===================================
#define WATCHDOG_SIZE 180
typedef void *MObj_WatchDog;
typedef void(*f_OnWatchedObj)(void *Obj, void *param);
MObj_WatchDog MWatchDog_InitWatchDog(MObj_WatchDog Buf, MInt BufSize);
bool          MWatchDog_Watch(MObj_WatchDog Dog, void *Obj);
void          MWatchDog_TraverseAndRelease(MObj_WatchDog Dog, f_OnWatchedObj OnWatchedObj, void *param);

// ===================================
// Here, a object pool is presented
// pool introduction:
//      1. when objects are freed, it is *not* freed, but add to recyclebin obj list, and Dispose will not be called
//      2. these objects can be restored from bin, and Init will not called, either.
//      3. after things are done, we can *free* these objs as usual.
// ===================================
#define OBJ_POOL_SIZE 100
typedef void *MObj_ObjPool;
MObj_ObjPool MObjPool_Init(MObj_ObjPool Buf, MInt BufSize, void * RefObj);
bool         MObjPool_Watch(MObj_ObjPool Pool, void *Obj);
void        *MObjPool_GetObj(MObj_ObjPool Pool);
void         MObjPool_TraverseAndRelease(MObj_ObjPool Pool, f_OnWatchedObj OnWatchedObj, void *param);

#endif
