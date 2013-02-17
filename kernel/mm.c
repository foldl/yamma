/*
 *   Copyright (C) 2011 mapandfold@gmail.com
 *
 *   This file is part of yamma.
 *
 *   yamma is free software: you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License as published by
 *   the Free Software Foundation, either version 3 of the License, or
 *   (at your option) any later version.
 *
 *   Foobar is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *   GNU General Public License for more details.
 *
 *   You should have received a copy of the GNU General Public License
 *   along with Foobar.  If not, see <http://www.gnu.org/licenses/>.
*/


#include <stdlib.h>
#include <string.h>
#include "mm.h"
#include "mstr.h"
#include "expr.h"
#include "encoding.h"

#if (GC_METHOD == GC_METHOD_DEF)
//#   define _FORWARD_FREE
#endif

struct XRefObjListMan
{
    XRefObjListMan *next;

    char TypeName[INTERNAL_NAME_LEN + 1];
    const MXRefObjMT *mt;
    int         TypeSize;

#if (GC_METHOD == GC_METHOD_GENERATION)
#   define GENERATION_NUM   3
    MXRefObjHeader* heads[GENERATION_NUM];
#elif (GC_METHOD == GC_METHOD_DEF)
    MXRefObjHeader head_alloc;
    MXRefObjHeader head_free;
    int ListSize_free;
    int MaxListSize_free;
#endif        
    int ListSize_alloc;
    int MaxListSize_alloc;
}; 

struct XRefObjListManList
{
    XRefObjListMan head;
    XRefObjListMan *last;
};

static XRefObjListManList ObjManList = {{0}, &ObjManList.head};

struct MXRefObjType_imp
{
    MXRefObjMT *pMT;
    void       *pTypeMan;
    int         Size;
};

void *YM_malloc(const int size)
{    
    void *p = malloc(size);
    Zero(p, size);
    return p;
}

void *YM_realloc(void *p, const int size)
{
    return realloc(p, size);
}

void YM_free(void *p)
{
    free(p);
}

#if (GC_METHOD == GC_METHOD_GENERATION)

// recycle from the newest generation to eldest one
inline static MXRefObjHeader *recycle_obj(XRefObjListMan *pMan, const bool bPartial = true)
{
    int i;
    int j;
    MXRefObjHeader *h = pMan->heads[0];
    MXRefObjHeader *head = NULL;
    MXRefObjHeader *last = NULL;
    
    // check gen0, hope that we are lucky
    if (h)
    {
        do 
        {
            if (h->_RefCount >= 0)
                h = h->next;
            else
            {
                pMan->heads[0] = h->next;
                return h;
            }
        } while (h != pMan->heads[0]);
    }
    else;

    for (i = 1; i < GENERATION_NUM; i++)
    {
        MXRefObjHeader *t = pMan->heads[i];
        if (t == NULL) continue;
        h = t->next;
        while (h != pMan->heads[i]) 
        {
            if (h->_RefCount >= 0)
            {
                t = h;
                h = h->next;
            }
            else
            {
                // remove h
                t->next = h->next;

                // insert h
                h->next = head;
                head = h;
                if (!last) last = h;

                h = t->next;
            }
        }

        if (h->_RefCount < 0)
        {
            if (t != h)
            {
                pMan->heads[i] = h->next;
                t->next = h->next;
            }
            else
                pMan->heads[i] = NULL;

            // insert h
            h->next = head;
            head = h;
            if (!last) last = h;
        }
        else;

        if (last && bPartial) break;
    }

    // move gen[j] to gen[j + 1]
    if (i == GENERATION_NUM) i = GENERATION_NUM - 1;
    for (j = i - 1; j >= 0; j--)
    {
        if (pMan->heads[j])
        {
            if (pMan->heads[j + 1])
            {
                MXRefObjHeader *t = pMan->heads[j]->next;
                pMan->heads[j]->next = pMan->heads[j + 1]->next;
                pMan->heads[j + 1]->next = t;
            }
            else
                pMan->heads[j + 1] = pMan->heads[j];
            pMan->heads[j] = NULL;
        }
    }

    if (last)
    {
        // move free ones to gen0
        last->next = head;  // cycle it up
        pMan->heads[0] = head->next; // head is allocated here
        return head;
    }
    else
    {
        return NULL;
    }
}

void *MXRefObj_CreateVoid(const MXRefObjType pt)
{
    XRefObjListMan *pMan = (XRefObjListMan *)pt;
    MXRefObjHeader *h = recycle_obj(pMan);

    if (h)
    {
        h->Generation++;
        if (pMan->mt->_Dispose)
        {
            if (NULL == pMan->mt->_Dispose(XRef_DATA(h)))
                Zero(XRef_DATA(h), pMan->TypeSize);
        }
        else
            Zero(XRef_DATA(h), pMan->TypeSize);

        ASSERT(h->_RefCount == -1);

        h->Attrib = 0;
        h->_RefCount = 0;

#ifdef MEM_VERIFY
        ASSERT(h->token1 == 0xBADABADA);
        ASSERT(h->token2 == 0x12341234);
#endif
    }
    else
    {
#define PRE_ALLOC   (1)
      //  printf("pre..");
        int i;
        MXRefObjHeader *t = 
                (MXRefObjHeader *)YM_malloc(PRE_ALLOC * (pMan->TypeSize + sizeof(MXRefObjHeader)));
        h = t;
        for (i = 0; i < PRE_ALLOC; i++)
        {
            MXRefObjHeader *n = t + 1;
            t->Type = pMan;

#ifdef MEM_VERIFY
            t->token1 = 0xBADABADA;
            t->token2 = 0x12341234;
#endif        
            if (pMan->mt->_Init)
                pMan->mt->_Init(XRef_DATA(t));

#ifdef MEM_VERIFY
            ASSERT(t->token1 == 0xBADABADA);
            ASSERT(t->token2 == 0x12341234);
#endif

            t->next = n;
            t = n;
        }
        t--;
        t->next = h;
        pMan->ListSize_alloc += PRE_ALLOC;
        if (pMan->MaxListSize_alloc < pMan->ListSize_alloc)
            pMan->MaxListSize_alloc = pMan->ListSize_alloc;
//printf("ok..\n");
        ASSERT(pMan->heads[0] == NULL);
        pMan->heads[0] = h->next;
    }

    ASSERT(h->Attrib == 0);

    return XRef_DATA(h);
}
#elif (GC_METHOD == GC_METHOD_DEF)
void *MXRefObj_CreateVoid(const MXRefObjType pt)
{
    XRefObjListMan *pMan = (XRefObjListMan *)pt;
    MXRefObjHeader *h;
    bool bDispose = false;
    if (pMan->ListSize_free > 0) // (pMan->head_free.next)
    {
        h = pMan->head_free.next;
        pMan->head_free.next = h->next;
        pMan->ListSize_free--;
        
        bDispose = true;

#ifndef _FORWARD_FREE
        if (pMan->mt->_Dispose)
        {
            if (NULL == pMan->mt->_Dispose(XRef_DATA(h)))
                Zero(XRef_DATA(h), pMan->TypeSize);
        }
        else
            Zero(XRef_DATA(h), pMan->TypeSize);
#endif

        ASSERT(h->_RefCount == -1);

        h->next = NULL;
        h->Attrib = 0;
        h->_RefCount = 0;

#ifdef MEM_VERIFY
        ASSERT(h->token1 == 0xBADABADA);
        ASSERT(h->token2 == 0x12341234);
#endif
    }
    else
    {
        h = (MXRefObjHeader *)YM_malloc(pMan->TypeSize + sizeof(MXRefObjHeader));
        h->Type = pMan;
#ifdef MEM_VERIFY
        h->token1 = 0xBADABADA;
        h->token2 = 0x12341234;
#endif
    }
//    if (strcmp("MExprList", pMan->TypeName) == 0)
//        printf("new: '%s' @ 0x%08X\n", pMan->TypeName, h);

    h->prev_alloc = &pMan->head_alloc;
    h->next = pMan->head_alloc.next;
    h->next->prev_alloc = h;
    pMan->head_alloc.next = h;

    pMan->ListSize_alloc++;
    if (pMan->MaxListSize_alloc < pMan->ListSize_alloc)
        pMan->MaxListSize_alloc = pMan->ListSize_alloc;
    if (pMan->mt->_Init)
        pMan->mt->_Init(XRef_DATA(h));

#ifdef MEM_VERIFY
    ASSERT(h->token1 == 0xBADABADA);
    ASSERT(h->token2 == 0x12341234);
#endif

    ASSERT(h->Attrib == 0);

    return XRef_DATA(h);
}
#endif     

void MXRefObj_Info(void *p, FILE *f)
{
    MXRefObjHeader *h = DATA_XRef(p);
    fprintf(f, "'%s' @ 0x%8p, RefCount = %d\n", ((XRefObjListMan *)h->Type)->TypeName, p, h->_RefCount);
}

static void MXRef_DumpType(FILE *file, XRefObjListMan *pMan);

bool MXRefObj_Verify(void *p)
{
    ASSERT((p) != NULL);

    {        
#ifdef MEM_VERIFY
        ASSERT(DATA_XRef(p)->token1 == 0xBADABADA);
        ASSERT(DATA_XRef(p)->token2 == 0x12341234);
#endif
        ASSERT(DATA_XRef(p)->Type);
    }
    
    if (DATA_XRef(p)->_RefCount < 0)
    {
        printf("!!! '%s' @ 0x%8p, 0x%8p, %d\n", 
                ((XRefObjListMan *)DATA_XRef(p)->Type)->TypeName, 
                DATA_XRef(p), p, DATA_XRef(p)->_RefCount);
        MXRef_DumpType(stdout, ((XRefObjListMan *)DATA_XRef(p)->Type));
//            if (strcmp("MExpr", ((XRefObjListMan *)DATA_XRef(p)->Type)->TypeName) == 0)
//            {
//                char t2[200 + 1] = {0};
//                MString s2 = MExpr_ToFullForm(MExpr(p));
//                int l1 = MStringToASCII(s2, (MByte *)t2, sizeof(t2) - 1);
//                printf("expr: %s\n", t2);
//            }
        return false;
    }

    return true;
}

void MXRefObj_FreeLocked(MUnknown p)
{
    MXRefObjHeader *h = (MXRefObjHeader *)(p);
    XRefObjListMan *pMan = (XRefObjListMan *)h->Type;
    h->_RefCount = -1;
    if (NULL == pMan->mt->_Dispose(XRef_DATA(h)))
        Zero(XRef_DATA(h), pMan->TypeSize);
}

#if (GC_METHOD == GC_METHOD_DEF)
void MXRefObj_Free(MUnknown p)
{
    MXRefObjHeader *h = (MXRefObjHeader *)(p);
    XRefObjListMan *pMan = (XRefObjListMan *)h->Type;
    h->_RefCount = -1;
      
//            if (strcmp("MExprList", ((XRefObjListMan *)DATA_XRef(p)->Type)->TypeName) == 0)
//            {
//                printf("Release '%s' @ 0x%08X, %d\n", ((XRefObjListMan *)DATA_XRef(p)->Type)->TypeName, DATA_XRef(p), DATA_XRef(p)->_RefCount);
//                MXRef_DumpType(stdout, ((XRefObjListMan *)DATA_XRef(p)->Type));
//            }

    h->prev_alloc->next = h->next;
    h->next->prev_alloc = h->prev_alloc;

    h->next = pMan->head_free.next;
    pMan->head_free.next = h;

    pMan->ListSize_alloc--;
    pMan->ListSize_free++;
    if (pMan->MaxListSize_free < pMan->ListSize_free)
        pMan->MaxListSize_free = pMan->ListSize_free;

#ifdef _FORWARD_FREE
    if (pMan->mt->_Dispose)
    {
        if (NULL == pMan->mt->_Dispose(XRef_DATA(h)))
            Zero(XRef_DATA(h), pMan->TypeSize);
    }
    else
        Zero(XRef_DATA(h), pMan->TypeSize);
#endif
}

static void MXRef_DumpType(FILE *file, XRefObjListMan *pMan)
{
    fprintf(file, "TypeName: %s\n", pMan->TypeName);
    fprintf(file, "TypeSize: %d\n", pMan->TypeSize);
    fprintf(file, "ListSize_alloc   : %d\n", pMan->ListSize_alloc);
    fprintf(file, "MaxListSize_alloc: %d\n", pMan->MaxListSize_alloc);
#ifndef DUMP_EACH_OBJ
    if (pMan->mt->_Dump)
    {
        MXRefObjHeader* p = pMan->head_alloc.next;
        for (int i = 0; i < pMan->ListSize_alloc; i++)
        {
            char s[1025];
            if (pMan->mt->_Dump)
            {
                fprintf(file, "\t %2d @ 0x%8p: %s\n", p->_RefCount, 
                        p, pMan->mt->_Dump(XRef_DATA(p), s, 1024));
            }
            else
                fprintf(file, "\t %2d @ 0x%8p\n", p->_RefCount, p);
            
            p = p->next;
        }
    }
#endif
    fprintf(file, "ListSize_free    : %d\n", pMan->ListSize_free);
    fprintf(file, "MaxListSize_free : %d\n", pMan->MaxListSize_free);
#ifdef DUMP_EACH_OBJ
    {
        MXRefObjHeader* p = pMan->head_free.next;
        MDword i;
        for (i = 0; i < pMan->ListSize_free; i++)
        {
            fprintf(file, "\t @ 0x%8p\n", p);
            p = p->next;
        }
    }
#endif
}

#elif (GC_METHOD == GC_METHOD_GENERATION)
static void MXRef_DumpType(FILE *file, XRefObjListMan *pMan)
{
    fprintf(file, "TypeName: %s\n", pMan->TypeName);
    fprintf(file, "TypeSize: %d\n", pMan->TypeSize);
    fprintf(file, "ListSize_alloc   : %d\n", pMan->ListSize_alloc);
    fprintf(file, "MaxListSize_alloc: %d\n", pMan->MaxListSize_alloc);
#ifdef DUMP_EACH_OBJ
    if (pMan->mt->_Dump)
    {
        int i;
        for (i = 0; i < GENERATION_NUM; i++)
        {
            int total = 0;
            int freed = 0;
            fprintf(file, "================= Generation %d ================== \n", i);
            MXRefObjHeader* p = pMan->heads[i];
            if (p == NULL) continue;
            do
            {
                total++;
                char s[1030];

                if (p->_RefCount >= 0)
                {
                    if (pMan->mt->_Dump)
                    {
                        fprintf(file, "\t %2d @ 0x%8p: %s\n", p->_RefCount, 
                                p, pMan->mt->_Dump(XRef_DATA(p), s, 1024));
                    }
                    else
                        fprintf(file, "\t %2d @ 0x%8p\n", p->_RefCount, p);
                }
                else
                {
                    fprintf(file, "\t %2d @ 0x%8p\n", p->_RefCount, p);
                    freed++;
                }
                p = p->next;
            } while (p != pMan->heads[i]);

            fprintf(file, "Total: %d\n", total);
            fprintf(file, "Freed: %d\n", freed);
        }
        
    }
#else
    {
        int i;
        for (i = 0; i < GENERATION_NUM; i++)
        {
            int total = 0;
            int freed = 0;
            fprintf(file, "================= Generation %d ================== \n", i);
            MXRefObjHeader* p = pMan->heads[i];
            if (p == NULL) continue;
            do
            {
                total++;
                if (p->_RefCount < 0) freed++;
                p = p->next;
            } while (p != pMan->heads[i]);

            fprintf(file, "Total: %d\n", total);
            fprintf(file, "Freed: %d\n", freed);
        }
    }
#endif
}

#endif

void MXRef_DumpType(FILE *file, MXRefObjType t)
{
    MXRef_DumpType(file, (XRefObjListMan *)t);
}

void MXRef_Dump(FILE *file)
{
    fprintf(file, "========= MXRef_Dump =========\n");
    XRefObjListMan *p = ObjManList.head.next;
    while (p != NULL)
    {
        MXRef_DumpType(file, p);
        p = p->next;
        fprintf(file, "-------------------------\n");
    }
}

MXRefObjType MXRefObj_Register(const MXRefObjMT *pmt, const int size, const char *name)
{
    XRefObjListMan *pMan = (XRefObjListMan *)YM_malloc(sizeof(XRefObjListMan));
    ObjManList.last->next = pMan;
    ObjManList.last = pMan;

    strcpy(pMan->TypeName, name);
    pMan->mt = pmt;
    pMan->TypeSize = size;

#if (GC_METHOD == GC_METHOD_DEF)    
    pMan->head_alloc.next = &pMan->head_alloc;
    pMan->head_alloc.prev_alloc = &pMan->head_alloc;
#elif (GC_METHOD == GC_METHOD_GENERATION)    
#endif

    return (MXRefObjType)pMan;
}

//void MXRefObj_ShareHolder(const void *target, void *p)
//{
//    MXRefObjHeader *h = DATA_XRef(p);
//    if (h->_Holder != NULL)
//        ((MXRefObjHeader *)(h->_Holder))->_RefCount--;
//    h->_Holder = DATA_XRef(target);
//    XRef_IncRef(target);
//}

void MXRefObj_GC(const MDword Option)
{
}

#if (GC_METHOD == GC_METHOD_DEF)    
MObj_WatchDog MWatchDog_InitWatchDog(MObj_WatchDog Buf, MInt BufSize)
{
    XRefObjListMan *pMan = (XRefObjListMan *)(Buf);
    ASSERT(BufSize >= (MInt)(sizeof(XRefObjListMan)));
    memset(Buf, 0, sizeof(XRefObjListMan));
    pMan->head_alloc.next = &pMan->head_alloc;
    pMan->head_alloc.prev_alloc = &pMan->head_alloc;
    return Buf;
}

bool MWatchDog_Watch(MObj_WatchDog Dog, void *Obj)
{
    MXRefObjHeader *h = DATA_XRef(Obj);
    XRefObjListMan *pMan = (XRefObjListMan *)(Dog);
    if (MObj_HasAttrib(Obj, OBJ_UNDER_WATCH)) return false;
    MObj_SetAttrib(Obj, OBJ_UNDER_WATCH);

    // get h off of alloc list
    h->prev_alloc->next = h->next;
    h->next->prev_alloc = h->prev_alloc; 

    // insert h to Dog
    h->prev_alloc = &pMan->head_alloc;
    h->next = pMan->head_alloc.next;
    h->next->prev_alloc = h;
    pMan->head_alloc.next = h;

    return true;
}

inline static void MWatchDog_ReleaseAll(MObj_WatchDog Dog)
{
    XRefObjListMan *pMan = (XRefObjListMan *)(Dog);
    MXRefObjHeader *first = pMan->head_alloc.next;
    MXRefObjHeader *last = pMan->head_alloc.prev_alloc;
    if (first != &pMan->head_alloc)
    {
        pMan = (XRefObjListMan *)first->Type;

        // restore to type manager 
        first->prev_alloc = &pMan->head_alloc;
        last->next = pMan->head_alloc.next;
        pMan->head_alloc.next->prev_alloc = last;
        pMan->head_alloc.next = first;
    }
}

void MWatchDog_TraverseAndRelease(MObj_WatchDog Dog, f_OnWatchedObj OnWatchedObj, void *param)
{
    XRefObjListMan *pMan = (XRefObjListMan *)(Dog);
    MXRefObjHeader *h = pMan->head_alloc.next;
    while (h != &pMan->head_alloc)
    {
        OnWatchedObj(XRef_DATA(h), param);
        MObj_ClearAttrib(XRef_DATA(h), OBJ_UNDER_WATCH);
        h = h->next;
    }
    
    MWatchDog_ReleaseAll(Dog);
}
#elif (GC_METHOD == GC_METHOD_GENERATION)    
MObj_WatchDog MWatchDog_InitWatchDog(MObj_WatchDog Buf, MInt BufSize)
{
    ASSERT(BufSize >= (MInt)(sizeof(XRefObjListMan)));
    memset(Buf, 0, sizeof(XRefObjListMan));
    return Buf;
}

bool MWatchDog_Watch(MObj_WatchDog Dog, void *Obj)
{
    MXRefObjHeader *h = DATA_XRef(Obj);
    XRefObjListMan *pMan = (XRefObjListMan *)h->Type;
    MXRefObjHeader *t = h->next;
    int i;
    if (MObj_HasAttrib(Obj, OBJ_UNDER_WATCH)) return false;
    MObj_SetAttrib(Obj, OBJ_UNDER_WATCH);

    return true;

    // get h off of alloc list
    // O(n), bad
    while (t->next != h) t = t->next;
    t->next = h->next;

    for (i = 0; i < GENERATION_NUM; i++)
    {
        if (pMan->heads[i] == h)
        {
            pMan->heads[i] = t != h ? h->next : NULL;
            break;
        }
    }

    // insert h to Dog
    pMan = (XRefObjListMan *)(Dog);
    h->next = pMan->heads[0];
    pMan->heads[0] = h;
    if (pMan->heads[1] == NULL) pMan->heads[1] = h;

    return true;
}

inline static void MWatchDog_ReleaseAll(MObj_WatchDog Dog)
{
    MXRefObjHeader *h = ((XRefObjListMan *)(Dog))->heads[0];
    MXRefObjHeader *t = ((XRefObjListMan *)(Dog))->heads[1];
    if (h)
    {
        XRefObjListMan *pMan = (XRefObjListMan *)h->Type;
        if (pMan->heads[0])
        {
            t->next = pMan->heads[0]->next;
            pMan->heads[0]->next = h;
        }
        else
        {
            pMan->heads[0] = h;
            t->next = h;
        } 
    }
}

void MWatchDog_TraverseAndRelease(MObj_WatchDog Dog, f_OnWatchedObj OnWatchedObj, void *param)
{
    MXRefObjHeader *h = ((XRefObjListMan *)(Dog))->heads[0];
    while (h)
    {
        OnWatchedObj(XRef_DATA(h), param);
        MObj_ClearAttrib(XRef_DATA(h), OBJ_UNDER_WATCH);
        h = h->next;
    };
    
    MWatchDog_ReleaseAll(Dog);
}
#endif

#if (GC_METHOD == GC_METHOD_DEF)    
struct MObj_ObjPool_impl
{
    XRefObjListMan *pRef;
    XRefObjListMan PseudoMan;
    MXRefObjMT     PseudoMT;
};

MObj_ObjPool MObjPool_Init(MObj_ObjPool Buf, MInt BufSize, void *RefObj)
{
    MObj_ObjPool_impl *pMan = (MObj_ObjPool_impl *)(Buf);
    ASSERT(BufSize >= (MInt)(sizeof(MObj_ObjPool_impl)));

    // objsize & mt is set to 0, so, during free/create, obj content will not be changed
    memset(Buf, 0, sizeof(MObj_ObjPool_impl));
    pMan->PseudoMan.mt = &pMan->PseudoMT;

    pMan->pRef = (XRefObjListMan *)(((MXRefObjHeader *)RefObj)->Type);          

    pMan->PseudoMan.head_alloc.next = &pMan->PseudoMan.head_alloc;
    pMan->PseudoMan.head_alloc.prev_alloc = &pMan->PseudoMan.head_alloc;
    return Buf;
}

bool MObjPool_Watch(MObj_ObjPool Pool, void *Obj)
{
    MXRefObjHeader *h = DATA_XRef(Obj);
    MObj_ObjPool_impl *pMan = (MObj_ObjPool_impl *)(Pool);
    if (pMan->pRef != (XRefObjListMan *)(h->Type)) return false;
    h->Type = &pMan->PseudoMan;
    return true;
}

void *MObjPool_GetObj(MObj_ObjPool Pool)
{
    MObj_ObjPool_impl *pMan = (MObj_ObjPool_impl *)(Pool);

    if (pMan->PseudoMan.ListSize_free > 0) 
        return MXRefObj_CreateVoid(&pMan->PseudoMan);
    else
    {
        void *r = MXRefObj_CreateVoid(pMan->pRef);
        MObjPool_Watch(Pool, r);
        return r;
    }
}

// let's move h to alloc list of original type man
#define restore_obj_and_rel(h, pMan)                        \
        h->Type = pMan;                                     \
        h->prev_alloc = &pMan->head_alloc;                  \
        h->next = pMan->head_alloc.next;                    \
        h->next->prev_alloc = h;                            \
        pMan->head_alloc.next = h;                          \
        pMan->ListSize_alloc++;                             \
        if (pMan->MaxListSize_alloc < pMan->ListSize_alloc) \
            pMan->MaxListSize_alloc = pMan->ListSize_alloc; \
        XRef_DecRef(XRef_DATA(h));

void MObjPool_TraverseAndRelease(MObj_ObjPool Pool, f_OnWatchedObj OnWatchedObj, void *param)
{
    XRefObjListMan *pPseudoMan = &((MObj_ObjPool_impl *)Pool)->PseudoMan;
    XRefObjListMan *pMan = ((MObj_ObjPool_impl *)Pool)->pRef;

    // 1. check what's in alloc list
    MXRefObjHeader *h = pPseudoMan->head_alloc.next;
    while (h != &pPseudoMan->head_alloc)
    {
        MXRefObjHeader *hn = h->next;
        OnWatchedObj(XRef_DATA(h), param);
        restore_obj_and_rel(h, pMan);
        h = hn;
    }

    // 2. check what's in freed list
    h = pPseudoMan->head_free.next;
    while (h)
    {
        MXRefObjHeader *hn = h->next;
        OnWatchedObj(XRef_DATA(h), param);

        h->_RefCount = 0;   // free it at next DecRef
        restore_obj_and_rel(h, pMan);
        h = hn;
    }
}
#endif

struct MHeap_block
{
    MHeap_block *p_next;
    MHeap_block *p_pre;

    MDword  len;
    MDword  capacity;
    MByte   data[1];
};

struct MHeap_imp
{
    char TypeName[INTERNAL_NAME_LEN + 1];
    MDword Capacity;
    void *Heap;
    MHeap_block first_free;
    MHeap_block first_alloc;
    MDword Used;
    MDword MaxSize;
    MDword MaxSize80;
    MDword BlockNum;
};

#define get_head(pdata) (MHeap_block *)((int)pdata - ((int)&((MHeap_block *)0)->data[0] - (int)&((MHeap_block *)0)->p_next))
#define get_capacity(byte_size) ((int(byte_size) - (int)(&((MHeap_block *)0)->data[0])) / sizeof(DWORD))
#define get_byte_size(capacity) (capacity * sizeof(DWORD) + (DWORD)(&((MHeap_block *)0)->data[0]))

MHeap MHeap_NewHeap(const char *name, const int HeapSize)
{
    MHeap_imp *p = (MHeap_imp *)YM_malloc(sizeof(MHeap_imp));
    strcpy(p->TypeName, name);
    p->MaxSize = HeapSize;
    p->MaxSize80 = MDword(HeapSize * 0.8);
    return p;
}

void *MHeap_malloc(MHeap Heap, const int size)
{
    ((MHeap_imp *)Heap)->Used += size + sizeof(MDword);
    ((MHeap_imp *)Heap)->BlockNum++;
    MDword *p = (MDword *)YM_malloc(size + sizeof(MDword));
    *p = size;
    return p + 1;
}

void *MHeap_realloc(MHeap Heap, void *p, const int size)
{
    if (p)
    {
        MInt oldSize;
        MDword *pD = (MDword *)p; pD--;
        oldSize = *pD;
        ((MHeap_imp *)Heap)->Used += size - oldSize;
        pD = (MDword *)YM_realloc(pD, size + sizeof(MDword));
        *pD = size;
        if (size > oldSize)
            Zero(((MByte *)(pD + 1)) + oldSize, size - oldSize);
        return pD + 1;
    }
    else
        return MHeap_malloc(Heap, size);
}

void MHeap_free(MHeap Heap, void *p)
{
    MDword *pD = (MDword *)p; pD--;
    ((MHeap_imp *)Heap)->Used -= *pD + sizeof(MDword);
    ((MHeap_imp *)Heap)->BlockNum--;
    YM_free(pD);
}

void MHeap_Dump(MHeap Heap, FILE *file)
{
    MHeap_imp *p = ((MHeap_imp *)Heap);
    fprintf(file, "Heap: %s\n", p->TypeName);
    fprintf(file, "\tMaxSize : %d\n", p->MaxSize);
    fprintf(file, "\tUsed    : %d\n", p->Used);
    fprintf(file, "\tBlockNum: %d\n", p->BlockNum);
}

