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

#include <string.h>
#include "expr.h"
#include "mm.h"
#include "mstr.h"
#include "sym.h"
#include "sys_sym.h"
#include "msg.h"
#include "encoding.h"

static MXRefObjType Type_MExpr = NULL;
static MXRefObjType Type_MSequence = NULL;
static MXRefObjType Type_MExprList = NULL;
static MXRefObjType Type_MExprListNode = NULL;
static MXRefObjType Type_MStack = NULL;
static MHeap        Heap_MExpr = NULL;

#define reset_hash(expr) MExpr_InvalidateHash(expr)

#define make_seq_cap(seq, cap)   \
do {                                                                         \
if ((cap) > (seq)->Capacity)                                                 \
{                                                                            \
if ((cap) > SEQ_CACHE_SIZE)                                                  \
{                                                                            \
    if ((seq)->Capacity > SEQ_CACHE_SIZE)                                    \
        MHeap_free(Heap_MExpr, (seq)->pExpr);                                \
                                                                             \
    (seq)->Capacity = (cap);                                                 \
    (seq)->pExpr = (MExpr *)MHeap_malloc(Heap_MExpr, (cap) * sizeof(MExpr)); \
}                                                                            \
else                                                                         \
{                                                                            \
    (seq)->Capacity = SEQ_CACHE_SIZE;                                        \
    (seq)->pExpr = (seq)->ExprCache;                                         \
	memset((seq)->ExprCache, 0, sizeof((seq)->ExprCache));					 \
}                                                                            \
}                                                                            \
else memset((seq)->pExpr, 0, (seq)->Capacity * sizeof((seq)->pExpr[0]));	 \
} while (false)

MExprList *MExprList_Create(void)
{    
    MExprList *r = MXRefObj_Create(MExprList, Type_MExprList);
    r->Last = &r->Head;
    return r;
}

MExprList *MExprList_Mirror(MExprList *Mirror)
{
    MExprList *r = MXRefObj_Create(MExprList, Type_MExprList);
    r->Mirror = Mirror;
    r->Len = Mirror->Len;
    r->Last = Mirror->Last;        
    r->Head.next = Mirror->Last;    // use Head.next to track Mirror
    return r;
}

MExprList *MExprList_Push(MExprList *l, MExpr expr)
{
    ASSERT(l->Mirror == NULL);

    MExprListNode *n = MXRefObj_Create(MExprListNode, Type_MExprListNode);
    n->Expr = expr; XRef_IncRef(expr);
    n->prev = l->Last;
    l->Last->next = n; 
    l->Last = n;
    l->Len++;
    return l;
}

MExprList *MExprList_PushOrphan(MExprList *l, MExpr expr)
{
    MExprListNode *n = MXRefObj_Create(MExprListNode, Type_MExprListNode);
    n->Expr = expr;
    if (l->Mirror == NULL)
    {
        n->prev = l->Last;
        l->Last->next = n; 
        l->Last = n;
        l->Len++;
    }
    else
    {
        n->prev = l->Last;
        l->Last = n;
        l->Len++;
    }
    return l;
}

MExpr MExprList_Top(MExprList *l)
{
    return l->Len > 0 ? l->Last->Expr : NULL;
}

MExpr MExprList_Pop(MExprList *l)
{
    ASSERT(l->Mirror == NULL);
    if (l->Len > 0)
    {
        MExprListNode *n = l->Last;
        MExpr r = n->Expr;
        l->Last = n->prev;
        l->Last->next = NULL;
        l->Len--;
        XRef_DecRef(n);
        return r;
    }
    
    return NULL;
}

MExpr MExprList_PopOrphan(MExprList *l)
{
    if (l->Len > 0)
    {
        if (l->Mirror == NULL)
        {
            MExprListNode *n = l->Last;
            MExpr r = n->Expr;
            l->Last = n->prev;
            l->Last->next = NULL;
            l->Len--;
            n->Expr = NULL;     // n->Expr should not be freed by "n"
            XRef_DecRef(n);
            return r;
        }
        else
        {
            MExpr r = l->Last->Expr;
            l->Len--;
            if (l->Last != l->Head.next)
            {
                MExprListNode *n = l->Last;
                n->Expr = NULL;
                l->Last = l->Last->prev;
                XRef_DecRef(n);
            }
            else
            {
                l->Last = l->Last->prev;
                l->Head.next = l->Last;
            }
            return r;
        }        
    }
    
    return NULL;
}

void MExprList_Clear(MExprList *l)
{
    ASSERT(l->Mirror == NULL);
    while (l->Len > 0)
    {
        MExprListNode *n = l->Last;
        l->Last = n->prev;
        l->Last->next = NULL;
        l->Len--;
        n->Expr = NULL;
        XRef_DecRef(n);
    }
}

void MExprList_ReleaseMirror(MExprList *l, f_OnUnmirror OnUnmirror)
{
    ASSERT(l->Mirror != NULL);

    if (OnUnmirror)
        while ((l->Last != l->Head.next) && (l->Last))
        {
            MExprListNode *n = l->Last;
            l->Last = n->prev;
            OnUnmirror(l, n->Expr);
            n->Expr = NULL;     // n->Expr should not be freed by "n"
            XRef_DecRef(n);
        }
    else
        while ((l->Last != l->Head.next) && (l->Last))
        {
            MExprListNode *n = l->Last;
            l->Last = n->prev;
            n->Expr = NULL;     // n->Expr should not be freed by "n"
            XRef_DecRef(n);
        }

    memset(l, 0, sizeof(MExprList));
    XRef_DecRef(l);
}

MStack *MStack_Create(int Capacity)
{
    MStack *r = MXRefObj_Create(MStack, Type_MStack);
    r->cap = Capacity;
    r->p = (MStackEle *)MHeap_malloc(Heap_MExpr, r->cap * sizeof(r->p[0]));
    r->top = -1;
    r->mt = -1;
    return r;
}

MStack * MStack_PushOrphan(MStack *s, MInt Tag, MExpr Expr)
{
    register int i;
    i = s->top - s->mt;
    s->top++;
    if (i < s->cap)
    {
        register MStackEle *pe = s->p + i;
        pe->t = Tag;
        pe->e = Expr;
        return s;
    }
    else
    {
        register MStackEle *pe;
        s->cap += 64;   // programmers should estimate proper stack size
        s->p = (MStackEle *)MHeap_realloc(Heap_MExpr, s->p, s->cap * sizeof(s->p[0]));
        pe = s->p + i;
        pe->t = Tag;
        pe->e = Expr;
        return s;
    }
}

MStackEle *MStack_Top0(MStack *s)
{
    if (s->top < 0) return NULL;

    if (s->m == NULL)
    {
        return &s->p[s->top - s->mt - 1];
    }
    else
    {
        int i = s->top;
        ASSERT(s->top == s->mt);
        while (true)
        {
            if (s->m == NULL)
            {
                if (i < 0) return NULL;
                return &s->p[i];
            }
            else if (i > s->mt)
            {
                return &s->p[i - s->mt - 1];
            }
            else
                s = s->m;
        }
    }    
}

MExpr   MStack_TopExpr(MStack *Stack)
{
    MStackEle *pe = MStack_Top0(Stack);
    return pe == NULL ? NULL : pe->e;
}

MInt    MStack_TopTag(MStack *s)
{
    MStackEle *pe = MStack_Top0(s);
    return pe == NULL ? NULL : pe->t;
}

MExpr   MStack_Top(MStack *Stack, MInt *Tag)
{
    MStackEle *pe = MStack_Top0(Stack);
    if (pe)
    {
        *Tag = pe->t;
        return pe->e;
    }
    else
    {
        *Tag = -1;
        return NULL;
    }
}

MInt    MStack_PopOrphan2(MStack *s)
{
    MInt r;
    int i;
    if (s->top < 0) return -1;

    if (s->m == NULL)
    {
        s->top--;
        r = s->p[s->top - s->mt].t;
        return r;
    }
    else
    {
        MStack *m = s;
        i = m->top;
        ASSERT(m->top == m->mt);
        
        while (true)
        {
            if (m->m == NULL)
            {
                if (i < 0) return -1;
                r = m->p[i].t;
                break;
            }
            else if (i > m->mt)
            {
                r = m->p[i - m->mt - 1].t;
                break;
            }
            else
                m = m->m;
        }
        s->top--;
        s->mt--;
        return r;
    }
}

MExpr   MStack_PopOrphan(MStack *s, MInt *Tag)
{
    MExpr r;
    int i;
    if (s->top < 0) return NULL;

    if (s->m == NULL)
    {
        s->top--;
        r = s->p[s->top - s->mt].e;
        *Tag = s->p[s->top - s->mt].t;
        return r;
    }
    else
    {
        MStack *m = s;
        i = m->top;
        ASSERT(m->top == m->mt);
        
        while (true)
        {
            if (m->m == NULL)
            {
                if (i < 0) return NULL;
                r = m->p[i].e;
                *Tag = m->p[i].t;
                break;
            }
            else if (i > m->mt)
            {
                r = m->p[i - m->mt - 1].e;
                *Tag = m->p[i - m->mt - 1].t;
                break;
            }
            else
                m = m->m;
        }
        s->top--;
        s->mt--;
        return r;
    }
}

void    MStack_Release(MStack *s)
{
    ASSERT(s->m == NULL);
    if (s->cap)
    {
        MHeap_free(Heap_MExpr, s->p);
        s->cap = 0;
    }
    XRef_DecRef(s);    
}

MStack *MStack_Mirror(MStack *Stack)
{
    MStack *r = MXRefObj_Create(MStack, Type_MStack);
    r->m = Stack;
    r->mt = Stack->top;        
    r->top = Stack->top;
    return r;
}

void    MStack_ReleaseMirror(MStack *s, f_OnStackUnmirror OnUnmirror)
{
    ASSERT(s->m != NULL);
    int i = 0;
    if (s->cap)
    {
        if (OnUnmirror)
        {
            for (; i < s->top - s->mt; i++)
            {
                OnUnmirror(s, s->p[i].t, s->p[i].e);
            }
free_all:
            MHeap_free(Heap_MExpr, s->p);
            s->cap = 0;
        }
        else
            goto free_all;
    }
    else;

    memset(s, 0, sizeof(*s));
    XRef_DecRef(s);    
}


MExprWatch *MExprWatch_Create()
{
    return MExprList_Create(); 
}

#define set_field_watch(n, v) (n)->prev = (MExprListNode *)(void *)(v)
#define get_field_watch(n)    ((MDword)((n)->prev))

MExprWatch *MExprWatch_Watch(MExprWatch *Watch, MExpr expr)
{
    MExprListNode *n = MXRefObj_Create(MExprListNode, Type_MExprListNode);
    n->Expr = expr; 
    set_field_watch(n, MObj_GetGen(expr));
    n->next = Watch->Head.next;
    Watch->Head.next = n;
    Watch->Len++;
    return Watch;
}

MExpr MExprWatch_ReleaseWatch(MExprWatch *Watch, bool &bWatched)
{
    if (Watch->Head.next)
    {
        MExprListNode *n = Watch->Head.next;
        MExpr r = n->Expr;
        bWatched = MObj_GetGen(r) == get_field_watch(n);
        Watch->Head.next = n->next;
        n->next = NULL;
        n->Expr = NULL;
        XRef_DecRef(n);
        Watch->Len--;
        return r;
    }
    else
        return NULL;
}

//void       MExprList_Release(MExprList *l)
//{
//    XRef_DecRef(l);
//}

MSequence *MSequence_FromExprList(MExprList *l, bool bRev)
{
    int i;
    MSequence *s = MXRefObj_Create(MSequence, Type_MSequence);
    s->Len = l->Len;
    make_seq_cap(s, l->Len);

    MExprListNode *n = l->Head.next;
    if (bRev)
    {
        for (i = s->Len - 1; i >= 0; i--)
        {
            s->pExpr[i] = n->Expr; XRef_IncRef(n->Expr);
            n = n->next;
        }
    }
    else
    {
        for (i = 0; i < s->Len; i++)
        {
            s->pExpr[i] = n->Expr; XRef_IncRef(n->Expr);
            n = n->next;
        }
    }

    return s;
}

MSequence *MSequence_Create(const MInt len)
{
    MSequence *s = MXRefObj_Create(MSequence, Type_MSequence);
    s->Len = len;

    ASSERT(len >= 0);

    make_seq_cap(s, len);
    
    return s;
}

//#define use_shadow

MSequence *MSequence_Uniquelize(MSequence *s)
{
    MInt len = s->Len;
    MSequence *ns;
#ifdef use_shadow
    ns = s->Shadow;
    if (ns)
    {
        ASSERT(MObj_RefCnt(ns) >= -1);
        ASSERT(ns->Len == s->Len);
        if (MObj_RefCnt(ns) == -1)
        {
            MExpr *p = s->pExpr;
            MObj_SetRefCnt(ns, 0);
            for (--len; len >= 0; --len)
            {
                if (ns->pExpr[len]) XRef_DecRef(ns->pExpr[len]);
                ns->pExpr[len] = p[len]; XRef_IncRef(p[len]);
            }
            return ns;
        }
        else
            MObj_Unlock(ns);
    }
#endif
    // create new
    ns = MSequence_Create(len);

#ifdef use_shadow    
    s->Shadow = ns;
    MObj_Lock(ns);
#endif

    {
        MExpr *p = s->pExpr;
        for (--len; len >= 0; --len)
        {
            ns->pExpr[len] = p[len];
            XRef_IncRef(p[len]);
        }
        return ns;
    }
}

#define copy_on_write()   \
do                                                                                      \
{                                                                                       \
    ASSERT_REF_CNT(s, 0);                                                               \
    if (s->Holder != NULL)                                                              \
    {                                                                                   \
        MSequence *holder = s->Holder;                                                  \
        MInt i;                                                                         \
                                                                                        \
        s->Holder = NULL;                                                               \
        s->Capacity = s->Len;                                                           \
                                                                                        \
        if (s->Len > SEQ_CACHE_SIZE)                                                    \
            s->pExpr = (MExpr *)MHeap_malloc(Heap_MExpr, s->Capacity * sizeof(MExpr));  \
        else                                                                            \
            s->pExpr = s->ExprCache;                                                    \
        for (i = 0; i < s->Len; i++)                                                    \
        {                                                                               \
            s->pExpr[i] = holder->pExpr[i];                                             \
            XRef_IncRef(holder->pExpr[i]);                                              \
        }                                                                               \
                                                                                        \
        MSequence_Release(holder);                                                      \
    }                                                                                   \
    else;                                                                               \
} while (0)

MSequence *MSequence_SetFrom(MSequence *s, MExprList *l, const int Start, const int Len)
{
    int i;
    MExprListNode *n = l->Head.next;

    ASSERT_REF_CNT(s, 0);
    ASSERT(s->Holder == NULL);

    for (i = Start; (i < Start + Len) && (i < s->Capacity); i++)
    {
        s->pExpr[i] = n->Expr; XRef_IncRef(n->Expr);
        n = n->next;
    }
    return s;
}

MSequence *MSequence_OverwriteSetFrom(MSequence *destination, const MSequence *source, 
                                      const int DestStart, const int SourceStart, const int Len)
{
    int i;

    ASSERT_REF_CNT(destination, 0);
    ASSERT(destination->Holder == NULL);
    ASSERT(DestStart >= 0);
    ASSERT(SourceStart >= 0);
    ASSERT(Len >= 0);
    ASSERT(DestStart + Len <= MSequence_Len(destination));
    ASSERT(SourceStart + Len <= MSequence_Len(source));

    memcpy(destination->pExpr + DestStart, source->pExpr + SourceStart, Len * sizeof(source->pExpr[0]));
    for (i = 0; i < Len; i++)
        XRef_IncRef(source->pExpr[i + SourceStart]);
    return destination;
}

// it should be ensured that: RefCount[s] == 0
MSequence *MSequence_SetAt(MSequence *s, const MInt i, MExpr expr)
{
    copy_on_write();

    if ((i >= 0) && (i < s->Len))
    {
        XRef_IncRef(expr);
        if (s->pExpr[i])
            XRef_DecRef(s->pExpr[i]);
        s->pExpr[i] = expr;
    }
    return s;
}

// it should be ensured that: RefCount[s] == 0
MSequence *MSequence_SetAtX(MSequence *s, const MInt i, MExpr expr)
{
    copy_on_write();

    if (s->pExpr[i])
        XRef_DecRef(s->pExpr[i]);
    s->pExpr[i] = expr;
    return s;
}

MSequence *MSequence_UniqueCopy(MSequence *s, const int Start, const int Len)
{
    if ((Start != 0) || (Len != s->Len))
    {
        MSequence *r = MXRefObj_Create(MSequence, Type_MSequence);
        MInt i;
        r->Len = min(s->Len - Start, Len);
        make_seq_cap(r, r->Len);

        memcpy(r->pExpr, s->pExpr + Start, (r->Len) * sizeof(MExpr));
        for (i = 0; i < r->Len; i++)
            XRef_IncRef(r->pExpr[i]);
        return r;
    }
    else
    {
        MSequence *r = MXRefObj_Create(MSequence, Type_MSequence);
        MInt i;
        r->Len = s->Len;
        make_seq_cap(r, r->Len);

        memcpy(r->pExpr, s->pExpr, (r->Len) * sizeof(MExpr));
        for (i = 0; i < r->Len; i++)
            XRef_IncRef(r->pExpr[i]);
        return r;
    }
}

MSequence *MSequence_Copy(MSequence *s, const int Start, const int Len)
{
    if ((Start != 0) || (Len != s->Len))
    {
        MSequence *r = MXRefObj_Create(MSequence, Type_MSequence);
        if (r->Capacity > SEQ_CACHE_SIZE)
            MHeap_free(Heap_MExpr, r->pExpr);

        r->Capacity = 0;
        r->Len = min(s->Len - Start, Len);
        r->Holder = s;
        r->pExpr = s->pExpr + Start;
        XRef_IncRef(s);
        return r;
    }
    else
    {
        XRef_IncRef(s);
        return s;
    }
}

MExpr      MSequence_GetAt(MSequence *s, const MInt i)
{
    if ((i >= 0) && (i < s->Len))
        return s->pExpr[i];
    else
        return NULL;
}

MSequence *MSequence_InitStatic(MSequence *s, const MInt len)
{
    memset(s, 0, sizeof(*s));
    ASSERT(len <= SEQ_CACHE_SIZE);
    s->Len = len;
    s->Capacity = SEQ_CACHE_SIZE;
    s->pExpr = s->ExprCache;
    return s;
}

void       MSequence_ReleaseStatic(MSequence *s)
{
    int i;
    for (i = 0; i < s->Len; i++)
    {
        if (s->pExpr[i])
            MExpr_Release(s->pExpr[i]);
    }
    memset(s, 0, sizeof(*s));
}

void MSequence_Shorten(MSequence *s, const int new_len)
{
    ASSERT_REF_CNT(s, 0);
    s->Len = min(s->Len, new_len);
}

//void       MSequence_Release(MSequence *s)
//{
//    XRef_DecRef(s);
//}

#define CONSTANT_INT_CACHE  10

static MExpr s_ComplexUnit = NULL;
static MExpr s_ExprIntCache[CONSTANT_INT_CACHE * 2 + 1] = {NULL};

MExpr MExpr_CreateMachineInt(const MInt v)
{
    if ((v <= CONSTANT_INT_CACHE) && (v >= -CONSTANT_INT_CACHE))
    {
        XRef_IncRef(s_ExprIntCache[v + CONSTANT_INT_CACHE]);
        return s_ExprIntCache[v + CONSTANT_INT_CACHE];
    }
    else
    {
        MExpr s = MXRefObj_Create(MTagExpr, Type_MExpr);
        s->Type = etNum;
        s->Num = Num_Create(v);
        return s;
    }
}

MExpr MExpr_CreateMachineUInt(const MUInt v)
{
    MExpr s = MXRefObj_Create(MTagExpr, Type_MExpr);
    s->Type = etNum;
    s->Num = Num_CreateU(v);
    return s;
}

MExpr MExpr_CreateMachineReal(const MReal v)
{
    MExpr s = MXRefObj_Create(MTagExpr, Type_MExpr);
    s->Type = etNum;
    s->Num = Num_Create(v);
    return s;
}

MExpr MExpr_CreateNum(const MNum n)
{
    MExpr s = MXRefObj_Create(MTagExpr, Type_MExpr);
    s->Type = etNum;
    s->Num = n; XRef_IncRef(n);
    return s;
}

MExpr MExpr_CreateNumX(const MNum n)
{
    MExpr s = MXRefObj_Create(MTagExpr, Type_MExpr);
    s->Type = etNum;
    s->Num = n;
    return s;
}

MExpr MExpr_CreateString(const MString v)
{
    MExpr s = MXRefObj_Create(MTagExpr, Type_MExpr);
    s->Type = etString;
    s->Str = MString_NewS(v);
    return s;
}

MExpr MExpr_CreateStringX(const MString v)
{
    MExpr s = MXRefObj_Create(MTagExpr, Type_MExpr);
    s->Type = etString;
    s->Str = v;
    return s;
}

//MExpr MExpr_CreateBool(const MBool v)
//{
//    MExpr s = MXRefObj_Create(MTagExpr, Type_MExpr);
//    s->Type = etBool;
//    s->Bool = v;
//    return s;
//}

MExpr MExpr_CreateRational(const MNum Numerator, const MNum Denominator)
{
    MExpr s = MXRefObj_Create(MTagExpr, Type_MExpr);
    s->Type = etNum;
    s->Num = Num_CreateRational(Numerator, Denominator);
    return s;
}

MExpr MExpr_CreateRationalX(const MNum Numerator, const MNum Denominator)
{
    MExpr s = MXRefObj_Create(MTagExpr, Type_MExpr);
    s->Type = etNum;
    s->Num = Num_CreateRationalX(Numerator, Denominator);
    return s;
}

MExpr MExpr_CreateComplex(const MNum real, const MNum image)
{
    MExpr s = MXRefObj_Create(MTagExpr, Type_MExpr);
    s->Type = etComplex;
    s->Complex.Imag = image; XRef_IncRef(image);
    s->Complex.Real = real; XRef_IncRef(real);
    return s;
}

MExpr MExpr_CreateComplexX(const MNum real, const MNum image)
{
    MExpr s = MXRefObj_Create(MTagExpr, Type_MExpr);
    s->Type = etComplex;
    s->Complex.Imag = image;
    s->Complex.Real = real;
    return s;
}

MExpr MExpr_CreateComplexUnit()
{
    XRef_IncRef(s_ComplexUnit);
    return s_ComplexUnit;
}

MExpr MExpr_CreateSymbol(const MSymbol s)
{
    if (s->SymExpr)
    {   
        XRef_IncRef(s->SymExpr);
        return s->SymExpr;
    }

    s->SymExpr = MXRefObj_Create(MTagExpr, Type_MExpr);
    s->SymExpr->Type = etSymbol;
    s->SymExpr->Symbol = s; XRef_IncRef(s);
    XRef_IncRef(s->SymExpr);
    return s->SymExpr;
}

MExpr MExpr_CreateCons(const MExpr x, const MExpr y)
{
    MExpr r = MXRefObj_Create(MTagExpr, Type_MExpr);
    r->Type = etCons;
    r->Cons.Car = x; XRef_IncRef(x);
    r->Cons.Cdr = y; XRef_IncRef(y);
    return r;
}

MExpr MExpr_CreateConsNX(const MExpr x, const MExpr y)
{
    MExpr r = MXRefObj_Create(MTagExpr, Type_MExpr);
    r->Type = etCons;
    r->Cons.Car = x; XRef_IncRef(x);
    r->Cons.Cdr = y;
    return r;
}

MExpr MExpr_CreateConsXX(const MExpr x, const MExpr y)
{
    MExpr r = MXRefObj_Create(MTagExpr, Type_MExpr);
    r->Type = etCons;
    r->Cons.Car = x;
    r->Cons.Cdr = y;
    return r;
}

MExpr MExpr_CreateEmptySeq()
{
    static MExpr r = MExpr_CreateHeadSeqX(sym_Sequence, MSequence_Create(0));
    XRef_IncRef(r);
    return r;
}

MExpr MExpr_ReplaceCar(const MExpr r, const MExpr car)
{
    ASSERT(r->Type = etCons);
    XRef_DecRef(r->Cons.Car);
    r->Cons.Car = car; XRef_IncRef(car);
    return r;
}

MExpr MExpr_ReplaceCdr(const MExpr r, const MExpr cdr)
{
    ASSERT(r->Type = etCons);
    XRef_DecRef(r->Cons.Cdr);
    r->Cons.Cdr = cdr; XRef_IncRef(cdr);
    return r;
}

MExpr MExpr_CreateHeadSeq(const MExpr h, MSequence *s)
{
    MExpr r = MXRefObj_Create(MTagExpr, Type_MExpr);
    r->Type = etHeadSeq;
    r->HeadSeq.Head = h; XRef_IncRef(h);
    r->HeadSeq.pSequence = s; XRef_IncRef(s);
    return r;
}

MExpr MExpr_CreateHeadSeqX(const MExpr h, MSequence *s)
{
    MExpr r = MXRefObj_Create(MTagExpr, Type_MExpr);
    r->Type = etHeadSeq;
    r->HeadSeq.Head = h; XRef_IncRef(h);
    r->HeadSeq.pSequence = s;
    return r;
}

MExpr MExpr_CreateHeadXSeqX(const MExpr h, MSequence *s)
{
    MExpr r = MXRefObj_Create(MTagExpr, Type_MExpr);
    r->Type = etHeadSeq;
    r->HeadSeq.Head = h;
    r->HeadSeq.pSequence = s;
    return r;
}

MExpr MExpr_CreateHeadSeq(const MSymbol h, MSequence *s)
{
    MExpr r = MXRefObj_Create(MTagExpr, Type_MExpr);
    r->Type = etHeadSeq;
    r->HeadSeq.Head = MExpr_CreateSymbol(h);
    r->HeadSeq.pSequence = s; XRef_IncRef(s);
    return r;
}

MExpr MExpr_CreateHeadSeqX(const MSymbol h, MSequence *s)
{
    MExpr r = MXRefObj_Create(MTagExpr, Type_MExpr);
    r->Type = etHeadSeq;
    r->HeadSeq.Head = MExpr_CreateSymbol(h);
    r->HeadSeq.pSequence = s;
    return r;
}

MExpr MExpr_CreateHeadSeqFromExprList(const MExpr h, MExprList *s, bool bRev)
{
    MSequence *m = MSequence_FromExprList(s, bRev);
    MExpr r = MExpr_CreateHeadSeq(h, m);
    MSequence_Release(m);
    return r;
}

MExpr MExpr_CreateHeadSeqFromExprList(const MSymbol h, MExprList *s, bool bRev)
{
    MExpr t = MExpr_CreateSymbol(h);
    MExpr r = MExpr_CreateHeadSeqFromExprList(t, s, bRev);
    MExpr_Release(t);
    return r;
}

static MString MExpr_Dispose(MExpr s)
{
    switch (s->Type)
    {
    case etNum:         Num_Release(s->Num); break;
    case etString:      MString_Release(s->Str); break;
    case etComplex:     
        if (s->Complex.Imag)
            Num_Release(s->Complex.Imag); 
        if (s->Complex.Real)
            Num_Release(s->Complex.Real); 
        break;
    case etCons:     
        if (s->Cons.Car) MExpr_Release(s->Cons.Car); 
        if (s->Cons.Cdr) MExpr_Release(s->Cons.Cdr);  
        break;
    case etSymbol:      MSymbol_Release(s->Symbol); break;
    case etHeadSeq:     MExpr_Release(s->HeadSeq.Head); MSequence_Release(s->HeadSeq.pSequence); break;
    default:
            ASSERT("no default" == NULL);
    }

    return NULL;
}

MExpr MExpr_SetTo(MExpr dest, MExpr s)
{
    MExpr_Dispose(dest);
    memcpy(dest, s, sizeof(*dest));
    switch (s->Type)
    {
    case etNum:         XRef_IncRef(s->Num); break;
    case etString:      XRef_IncRef(s->Str);  break;
    case etComplex:     
        XRef_IncRef(s->Complex.Imag); 
        XRef_IncRef(s->Complex.Real); 
        break;
    case etCons:     
        XRef_IncRef(s->Cons.Car); 
        XRef_IncRef(s->Cons.Cdr); 
        break;
    case etSymbol:      
        XRef_IncRef(s->Symbol); 
        break;
    case etHeadSeq:     
        XRef_IncRef(s->HeadSeq.Head); 
        XRef_IncRef(s->HeadSeq.pSequence); 
        break;
    default:
        ASSERT("no default" == NULL);
    }
    return dest;
}

extern MHash BIF_Hash0(MExpr s);
MHash MExpr_Hash(MExpr s)
{
    if (s->Hash == 0)
        s->Hash = BIF_Hash0(s);
    return s->Hash;
}


static MSequence *MSequence_Dispose(MSequence *s)
{
#ifdef use_shadow    
    if (s->Shadow)
    {
        MSequence *t = s->Shadow;
        MObj_Unlock(t);
        ASSERT(MObj_RefCnt(t) >= -1);
        if (MObj_RefCnt(t) == -1)
        {
            MObj_SetRefCnt(t, 0);
            MSequence_Release(t);
        }
        else;
        s->Shadow = NULL;
    }
    else;
#endif
    if (s->Holder)
    {
        XRef_DecRef(s->Holder);
        s->Holder = NULL;
        s->Capacity = 0;
        s->Len = 0;

        return NULL;
    }
    else
    {
        if (s->pExpr)
        {
            int i;
            for (i = 0; i < s->Len; i++)
                if (s->pExpr[i])
                    XRef_DecRef(s->pExpr[i]);
            Zero(s->pExpr, s->Len * sizeof(s->pExpr[0]));
        }
/* 
        if (s->pPackedInt)
        {
            MHeap_free(Heap_MExpr, s->pPackedInt);
            s->pPackedInt = NULL;
        }
        if (s->pPackedReal)
        {
            MHeap_free(Heap_MExpr, s->pPackedReal);
            s->pPackedReal = NULL;
        }
*/
        return s;
    }
}

static MExprList *MExprList_Dispose(MExprList *p)
{
    if (p->Head.next)
        XRef_DecRef(p->Head.next);
    return NULL;
}

/*
static MStack *MStack_Dispose(MStack *p)
{
    return NULL;
}
*/

static MExprListNode *MExprListNode_Dispose(MExprListNode *p)
{
    if (p->Expr)
        XRef_DecRef(p->Expr);
    if (p->next)
        XRef_DecRef(p->next);
    return NULL;
}

static char *MExpr_DumpObj(MExpr expr, char *s, const MInt maxlen)
{
    MString s2 = MExpr_ToFullForm(expr);
    s[maxlen - 1] = '\0';
    MStringToASCII(s2, (MByte *)s, maxlen - 2);
    MString_Release(s2);
    return s;
}

static char *MSequence_DumpObj(MSequence *seq, char *s, const MInt maxlen)
{
    int i;
    int k = 0;
    s[k++] = '[';
    for (i = 0; i < seq->Len; i++)
    {
        MExpr e = seq->pExpr[i];
        MExpr_DumpObj(e, s + k, maxlen - 2 - k);
        k += strlen(s + k);
        s[k++] = ',';
        if (k > maxlen - 1)
            break;
    }
    if (seq->Len > 0) k--;
    s[k++] = ']';
    s[k] = '\0';
    return s;
}

static MXRefObjMT MExprList_MT = {NULL, (f_YM_XRefObjDispose)MExprList_Dispose, NULL};
static MXRefObjMT MStack_MT = {NULL, NULL, NULL};
static MXRefObjMT MExprListNode_MT = {NULL, (f_YM_XRefObjDispose)MExprListNode_Dispose, NULL};
static MXRefObjMT MExpr_MT = {NULL, (f_YM_XRefObjDispose)MExpr_Dispose, (f_YM_XRefObjDump)MExpr_DumpObj};
static MXRefObjMT MSequence_MT = {NULL, (f_YM_XRefObjDispose)MSequence_Dispose, (f_YM_XRefObjDump)MSequence_DumpObj};

void MExpr_Init(void)
{
    Type_MExpr = MXRefObj_Register(&MExpr_MT, sizeof(MTagExpr), "MExpr");
    Type_MSequence = MXRefObj_Register(&MSequence_MT, sizeof(MSequence), "MSequence");
    Type_MExprList = MXRefObj_Register(&MExprList_MT, sizeof(MExprList), "MExprList");
    Type_MStack = MXRefObj_Register(&MStack_MT, sizeof(MStack), "MStack");
    Type_MExprListNode = MXRefObj_Register(&MExprListNode_MT, sizeof(MExprListNode), "MExprListNode");
    Heap_MExpr = MHeap_NewHeap("MSequence", 20000000);
    
    //
    {
        MInt i;

        for (i = -CONSTANT_INT_CACHE; i <= CONSTANT_INT_CACHE; i++)
        {
            s_ExprIntCache[i + CONSTANT_INT_CACHE] = MXRefObj_Create(MTagExpr, Type_MExpr);
            s_ExprIntCache[i + CONSTANT_INT_CACHE]->Type = etNum;
            s_ExprIntCache[i + CONSTANT_INT_CACHE]->Num = Num_Create(i);
        }

        s_ComplexUnit = MExpr_CreateComplex(s_ExprIntCache[0 + CONSTANT_INT_CACHE]->Num, 
                                            s_ExprIntCache[1 + CONSTANT_INT_CACHE]->Num);
    }
}

void MExpr_DumpHeap(FILE *file)
{
    MHeap_Dump(Heap_MExpr, file);
}

static MString MExpr_ToFullForm(MString s, MExpr expr)
{
    static MString Null = MString_NewC("<<Null>>");
    MString t = NULL;
    MSequence *ss;

//    if (s->Len > 100)
//        return s;
    if (NULL == expr)
    {
        MString_Cat(s, Null);
        return s;
    }

    switch (expr->Type)
    {
    case etNum:
        t = Num_ToString(expr->Num);
        MString_Cat(s, t);
        MString_Release(t);
        break;
    case etString:
        MString_CatChar(s, '"');
        MString_Cat(s, expr->Str);
        MString_CatChar(s, '"');
        break;
    case etComplex:
        MString_Cat(s, sym_Complex->Name);
        MString_CatChar(s, '[');
        
        t = Num_ToString(expr->Complex.Real);
        MString_Cat(s, t);
        MString_Release(t);

        MString_CatChar(s, ','); MString_CatChar(s, ' ');

        t = Num_ToString(expr->Complex.Imag);
        MString_Cat(s, t);
        MString_Release(t);

        MString_CatChar(s, ']');
        break;
    case etSymbol:
        MString_Cat(s, expr->Symbol->Name);
        break;
    case etCons:
        MString_Cat(s, sym_Cons->Name);
        MString_CatChar(s, '[');
        MExpr_ToFullForm(s, expr->Cons.Car);
        MString_CatChar(s, ','); MString_CatChar(s, ' ');
        MExpr_ToFullForm(s, expr->Cons.Cdr);
        MString_CatChar(s, ']');
        break;
    case etHeadSeq:
        MExpr_ToFullForm(s, expr->HeadSeq.Head);
        MString_CatChar(s, '[');
        
        ss = expr->HeadSeq.pSequence;
        if (ss->Len > 0)
            MExpr_ToFullForm(s, ss->pExpr[0]);
        if (ss->Len > 1)
        {
            int i;
            for (i = 1; i < ss->Len; i++)
            {
                MString_CatChar(s, ',');
                MExpr_ToFullForm(s, ss->pExpr[i]);
            }
        }
        MString_CatChar(s, ']');
        break;
    default:
        ASSERT("no default" == NULL);
    }
    return s;
}

MString MExpr_ToFullForm(MExpr expr)
{
    MString s = MString_NewWithCapacity(1024);
    return MExpr_ToFullForm(s, expr);
}

void MExprList_DumpType(FILE *f)
{
    MXRef_DumpType(f, Type_MExprList);
}

