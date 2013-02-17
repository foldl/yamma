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
#include <memory.h>
#include <math.h>
#include "big.h"
#include "expr.h"
#include "mm.h"
#include "mstr.h"
#include "sym.h"
#include "sys_sym.h"
#include "msg.h"
#include "encoding.h"

static MXRefObjType Type_MBig = NULL;
static MHeap        Heap_MBig = NULL;

#define ensure_capacity(r, cap)              \
do {                                         \
    if ((r)->capacity < (cap))               \
    {                                        \
        (r)->capacity = (cap) + 5;           \
        (r)->buf = ((MWord *)MHeap_realloc(Heap_MBig, (r)->buf, ((r)->capacity) * sizeof(MWord)));\
    }                                        \
    else;                                    \
} while(false)

MInt  ParseMachineInt(const MString s);
MReal ParseMachineReal(const MString s, const MInt Base = 10);
MInt Big_BaseCmp(const MBigNum a, const MBigNum b);

// TODO: eliminate log function, use a table

// Log[WORDMASK + 1, 10.] = 0.2076205059304601467419
#define base10toWORD(v) ((MInt)ceil(0.2076205059304601467419 * (v)))

// Log[2, 10.] = 3.32192809488736234787
#define base10toBIT(v) ((MInt)ceil(3.32192809488736234787 * (v)))

// Log[10, WORDMASK + 1] = 4.81647993062369912342
#define baseWORDto10(v) ((MInt)ceil(4.81647993062369912342 * (v)))

// Log[10, 2] = 0.301029995663981
#define baseBITto10(v) ((MInt)ceil(0.301029995663981 * (v)))

// Log[N, ORDMASK + 1]
#define baseNtoWORD(n, v) ((MInt)ceil(log10(n) * 0.2076205059304601467419 * (v)))

// Log[N, 2.] = log10(n) / log10(2)
#define baseNtoBIT(n, v) ((MInt)ceil(log10(n) * 3.32192809488736234787 * (v)))

// Log[2, N ] = log10(2) / log10(n)
#define baseBITtoN(n, v) ((MInt)ceil(3.32192809488736234787 / log10(n) * (v)))

// Log[WORDMASK + 1, N]
#define baseWORDtoN(n, v) ((MInt)ceil(4.81647993062369912342 / log10(n) * (v)))

#define bitpreci(big)  ((big)->bitpreci)
#define preci(big)     ((big)->bitpreci / WORDBITS)
#define logic_preci(big)    ((big)->bitpreci > 0 ? min((big)->size, preci(big)) : (big)->size)
#define logic_bitpreci(big) ((big)->bitpreci > 0 ? min((big)->size * WORDBITS, bitpreci(big)) : (big)->size * WORDBITS)
#define bitpreciExp(big) (bitpreci(big) >= 0 ? ((big)->size + (big)->binexp) * WORDBITS - bitpreci(big) : -MAX_PRECISION)
#define is_max_preciExp(p) ((p) == -MAX_PRECISION)
#define update_preci(big, p) \
do {                            \
    int v0000 = ((big)->size + (big)->binexp) * WORDBITS - p;  \
    (big)->bitpreci = min(v0000, (big)->size * WORDBITS);   \
} while (false)

#define minpreci(pre1, pre2) ((pre1 < 0) ? (pre2) : ((pre2 < 0) ? (pre1) : min((pre1), (pre2))))

void Big_Print(const MWord *ps, const int len);

#define big_alloc(size)  ((MWord *)MHeap_malloc(Heap_MBig, (size) * sizeof(MWord)))
#define big_free(p)      MHeap_free(Heap_MBig, p)

#define big_size(a) ((a)->size)
#define big_neg(a) ((a)->bNeg)
#define big_preci(a)    preci(a)
#define is_exact(a)     (bitpreci(a) < 0)

#define Big_GetEpsilon() (5)

static MInt big_WorkingPreci = 32;

int ceiling2(MDword v) 
{
   int n; 
   MDword x = v;
 
   if (x == 0) return (0); 
   n = 1; 
   if ((x >> 16) == 0) {n = n +16; x = x <<16;} 
   if ((x >> 24) == 0) {n = n + 8; x = x << 8;} 
   if ((x >> 28) == 0) {n = n + 4; x = x << 4;} 
   if ((x >> 30) == 0) {n = n + 2; x = x << 2;} 
   n = n - (x >> 31); 
   return (v & (v - 1)) ? 32 - n : 31 - n; 
} 


MInt Big_SetWorkingPrecission(const MInt NewPreci)
{
    MInt r = big_WorkingPreci;
    big_WorkingPreci = NewPreci > 0 ? NewPreci : big_WorkingPreci;
    return r;
}

MInt Big_GetWorkingPrecission(void)
{
    return big_WorkingPreci;
}

MBigNum Big_Create0(void)
{
    MBigNum r = MXRefObj_Create(MTagBigNum, Type_MBig);
    ensure_capacity(r, 1);  
    r->buf[0] = 0;
    r->size = 0;
    r->bitpreci = -1;
    return r;
}

MBigNum Big_Create1(const MInt bitpreci)
{
    MBigNum r = MXRefObj_Create(MTagBigNum, Type_MBig);
    
    MInt prefer = bitpreci / WORDBITS + 1;
    ensure_capacity(r, prefer);
    
    Zero(r->buf, prefer * sizeof(MWord));

    r->bitpreci = bitpreci;
    r->size = prefer;
    r->binexp = 1 - r->size; 
    r->buf[r->size - 1] = 1;

    return r;
}

static MBigNum Big_Trim(MBigNum a)
{
    MInt s = big_size(a);
    if (s > 0)
    {
        MWord *pend = a->buf + s - 1;
        while ((pend >= a->buf) && (*pend-- == 0))
            s--;
    }
    else
        s = 0;

    a->size = max(s, 0);

    return a;
}

void Big_Print(const MWord *ps, const int len)
{
#define SIZE    5000
    MInt pos = 0;
    char r[SIZE + 1] = {'\0'};
    MInt i = len + 1;
    ps += len - 1;
    while (--i)
    {
        MWord vv = *ps--;
        r[pos++] = (char)(Int2Char((vv & 0xF000) >> 12));
        r[pos++] = (char)(Int2Char((vv & 0x0F00) >> 8));
        r[pos++] = (char)(Int2Char((vv & 0x00F0) >> 4));
        r[pos++] = (char)(Int2Char((vv & 0x000F)));
        r[pos++] = ' ';
    }

    r[pos++] = '\n';
    printf(r);
}

void Big_Print(MBigNum a)
{
#define SIZE    5000
    char r[SIZE + 1] = {'\0'};
    MWord *w = a->buf + big_size(a) - 1;
    MInt i = big_size(a) - 1;
    MInt pos = 0;
    if (big_neg(a))
        r[pos++] = '-';
    if (a->size == 0)
        r[pos++] = '0';

    while (i >= 0)
    {
        MWord vv = *w--;
        r[pos++] = (char)(Int2Char((vv & 0xF000) >> 12));
        r[pos++] = (char)(Int2Char((vv & 0x0F00) >> 8));
        r[pos++] = (char)(Int2Char((vv & 0x00F0) >> 4));
        r[pos++] = (char)(Int2Char((vv & 0x000F)));
        r[pos++] = ' ';
        i--;
    }

    if (!is_exact(a))
        r[pos++] = '.';

    {
        char t[20] = {'\0'};
        sprintf(t, "*^%d @ %d\n", a->binexp, bitpreci(a));
        strcat(r, t);
    }
    printf(r);
}

MBigNum Big_EnsureCapacity(MBigNum a, const int cap)
{
    if (a->capacity < cap)
    {
        a->capacity = cap;
        a->buf = (MWord *)MHeap_realloc(Heap_MBig, a->buf, cap * sizeof(MWord));
        Zero(a->buf + a->size, (a->capacity - a->size) * sizeof(a->buf[0]));
    }

    return a;
}

#define big_insert_0(big, n)    \
do                              \
{                               \
    if (big->capacity < big->size + (n))    \
    {                                       \
        big->capacity = big->size + (n);    \
        MWord *newBuf = (MWord *)MHeap_malloc(Heap_MBig, big->capacity * sizeof(MWord)); \
        memcpy(newBuf + (n), big->buf, big->size * sizeof(MWord));\
        if (big->buf) big_free(big->buf);                   \
        big->buf = newBuf;                                  \
    }                                                       \
    else                                                    \
    {                                                       \
        memmove(big->buf + (n), big->buf, big->size * sizeof(MWord));\
        Zero(big->buf, (n) * sizeof(MWord));                         \
    }                                                   \
    big->size += (n);                                   \
    big->binexp -= (n);                                 \
} while (0 == 1)

static MBigNum Big_UnsignedAddBy(MBigNum a, const MBigNum b)
{
    MWord *p1;
    MWord *p2;
    MDword carry = 0;
    MInt i = 0;
    MInt len;
    MInt expOff = a->binexp - b->binexp;

    if (expOff > 0)
    {
        len = max(a->size + expOff, b->size);
        if (a->capacity < len + 1)
        {
            MWord *pb = a->buf;
            a->buf = big_alloc(len + 1);
            a->capacity = len + 1;
            memcpy(a->buf + expOff, pb, a->size * sizeof(a->buf[0]));
            big_free(pb);
        }
        else
        {
            memmove(a->buf + expOff, a->buf, a->size * sizeof(a->buf[0]));
            Zero(a->buf, expOff * sizeof(a->buf[0]));
        }
               
        a->size += expOff;
        a->binexp = b->binexp; 
        p1 = a->buf;
        p2 = b->buf; 
        len = b->size;
        a->size = max(a->size, len);
    }
    else
    {
        len = max(a->size, b->size - expOff);
        Big_EnsureCapacity(a, len + 1);

        p1 = a->buf - expOff;
        p2 = b->buf;
        a->size = max(a->size, len);
        len = b->size;
    }

    for (i = 0; i < len; i++)
    {
        MDword t = (MDword)(*p1) + (MDword)(*p2++) + carry;
        carry = t >> WORDBITS;
        *p1++ = (MWord)(t & WORDMASK);
    }

    if (carry)
    {
        for (; i < a->size; i++)
        {
            MDword t = (MDword)(*p1) + carry;
            carry = t >> WORDBITS;
            *p1++ = (MWord)(t & WORDMASK);

            if (carry == 0)
                break;
        }

        if (carry)
        {
            *p1++ = (MWord)(carry);
            a->size++;
        }
    }

    return a;
}

// Note: a.base <= b.base
static MBigNum Big_UnsignedSubtractBy(MBigNum a, const MBigNum b)
{
    MWord *p1;
    MWord *p2;
    MInt carry = 0;
    MInt i = 0;
    MInt expOff = a->binexp - b->binexp;
    int len;

    if (expOff > 0)
    {
        len = max(a->size + expOff, b->size);
        if (a->capacity < len)
        {
            MWord *pb = a->buf;
            a->buf = big_alloc(len);
            a->capacity = len + 1;
            memcpy(a->buf + expOff, pb, a->size * sizeof(a->buf[0]));
            big_free(pb);
        }
        else
        {
            memmove(a->buf + expOff, a->buf, a->size * sizeof(a->buf[0]));
            Zero(a->buf, expOff * sizeof(a->buf[0]));
        }
               
        a->size += expOff;
        a->binexp = b->binexp; 
        p1 = a->buf;
        p2 = b->buf; 
        len = b->size;
        a->size = max(a->size, len);
    }
    else
    {
        len = max(a->size, b->size - expOff);
        Big_EnsureCapacity(a, len);

        p1 = a->buf - expOff;
        p2 = b->buf;
        len = b->size;
        a->size = max(a->size, len);
    }

    for (; i < len; i++)
    {
        MInt t = (MInt)(*p1) - (MInt)(*p2++) + carry;
        if (t < 0)
        {
            t += WORDMASK + 1;
            carry = -1;
        }
        else
            carry = 0;
        
        *p1++ = (MWord)(t & WORDMASK);
    }

    if (carry != 0)
    {
        ASSERT((i < a->size));
        for (; i < a->size; i++)
        {
            MInt t = (MInt)(*p1) + carry;
            if (t < 0)
            {
                t += WORDMASK + 1;
                carry = -1;
            }
            else
                carry = 0;
        
            *p1++ = (MWord)(t & WORDMASK);

            if (carry == 0)
                break;
        }
    }
    else;

    return a;
}

static MBigNum Big_UnsignedBaseMutiplyBy(MBigNum a, const MBigNum b)
{
    MWord *p1;
    const MWord *p2;
    MWord *pr;
    MInt carry = 0;
    MInt i;
    MInt j;
    MInt s1 = a->size;
    MInt s2 = b->size;

    p1 = a->buf;

    a->size += b->size - 1;
    a->capacity = a->size + 1;
    a->buf = big_alloc(a->capacity);

    pr = a->buf;
    p2 = b->buf;

    for (i = 0; i < s2; i++)
    {
        MWord *pa = p1;
        MWord *po = pr; pr++;
        MDword b = *p2++;
        carry = 0;
        for (j = 0; j < s1; j++)
        {
            MDword t = (MDword)(*pa++) * b + carry + (MDword)(*po);
            carry = t >> WORDBITS;
            *po++ = (MWord)(t & WORDMASK);
        }
        
        if (carry)
            *po++ = (MWord)carry;
    }

    if (a->buf[a->size])
        a->size++;

    if (p1)
        big_free(p1);

    return a;
}

static MBigNum Big_BaseAddBySmall(MBigNum a, MWord b)
{
    MWord *p;
    MWord *pend;
    MDword carry = b;

    ASSERT(a->binexp <= 0);

    Big_EnsureCapacity(a, a->size + 1);

    p = a->buf - a->binexp;
    pend = a->buf + a->size;

    while (p != pend)
    {
        MDword t = (MDword)(*p) + carry;
        carry = t >> WORDBITS;
        *p++ = (MWord)(t & WORDMASK);
        if (carry == 0)
            return a;
    }

    *p = (MWord)carry;
    a->size++;

    return a;
}

#define add_by_word(p, b, size, carry) \
{                                           \
    MInt i;                                 \
    carry += b;                             \
    for (i = 0; i < size; i++)              \
    {                                       \
        MInt t = *p + carry;                \
        carry = t >> WORDBITS;              \
        *p++ = (MWord)(t & WORDMASK);       \
        if (0 == carry) break;              \
    }                                       \
}

#define subtract_by_word(p, b, size, carry) \
{                                           \
    MInt i;                                 \
    carry -= b;                             \
    for (i = 0; i < size; i++)              \
    {                                       \
        MInt t = *p + carry;                \
        if (t < 0)                          \
        {                                   \
            t += WORDMASK + 1;              \
            carry = -1;                     \
        }                                   \
        else                                \
            carry = 0;                      \
        *p++ = (MWord)(t);                  \
        if (0 == carry) break;              \
    }                                       \
}

#define mult_by_word(p, b, size, carry)     \
{                                           \
    MInt i;                                 \
    for (i = 0; i < size; i++)              \
    {                                       \
        MDword t = *p * b + carry;          \
        carry = t >> WORDBITS;              \
        *p++ = (MWord)(t & WORDMASK);       \
    }                                       \
}

static MBigNum Big_MultiplyBy(MBigNum a, MWord b)
{
    MWord *p = NULL;
    MDword carry = 0;

    Big_Trim(a);
    Big_EnsureCapacity(a, a->size + 1);

    p = a->buf;

    mult_by_word(p, b, a->size, carry);
    
    if (carry)
    {
        *p = (MWord)carry;
        a->size++;
    }

    return a;
}

static MBigNum Big_MultiplyTo(MBigNum a, MWord b, MBigNum r)
{
    MWord *p;
    MWord *o;
    MDword carry = 0;
    MInt i;

    Big_Trim(a);
    Big_EnsureCapacity(r, a->size + 1);
    r->size = a->size;

    p = a->buf;
    o = r->buf;

    for (i = 0; i < a->size; i++)
    {
        MDword t = (MDword)(*p++) * b + carry;
        carry = t >> WORDBITS;
        *o++ = (MWord)(t);
    }
    
    if (carry)
    {
        *o = (MWord)carry;
        r->size++;
    }

    return a;
}

#define div_by_word_full(pa, b, size, remainer)               \
{                                                             \
    MInt i;                                                   \
    MWord *p = pa + (size) - 1;                               \
    MWord *po = p;                                            \
    i = size;                                                 \
    while (i > 0)                                             \
    {                                                         \
        i--;                                                  \
        MDword t = ((remainer << WORDBITS) | (MDword)(*p--)); \
        MWord  q = (MWord)(t / b);                            \
        remainer = t % b;                                     \
        if (q)                                                \
        {                                                     \
            *po-- = q;                                        \
            break;                                            \
        }                                                     \
    }                                                         \
    for (; i > 0; i--)                                        \
    {                                                         \
        MDword t = ((remainer << WORDBITS) | (MDword)(*p--)); \
        *po-- = (MWord)(t / b);                               \
        remainer = t % b;                                     \
    }                                                         \
    for (; po > p; po--)                                      \
    {                                                         \
        a->binexp--;                                          \
        MDword t = (remainer << WORDBITS);                    \
        *po = (MWord)(t / b);                                 \
        remainer = t % b;                                     \
    }                                                         \
}

static MBigNum Big_DivideBySmallFull(MBigNum a, MWord b, MWord *Remainer)
{
    MDword remainer = Remainer ? *Remainer : 0;

    Big_Trim(a);

    div_by_word_full(a->buf, b, a->size, remainer);
    
    if (Remainer)
        *Remainer = (MWord)remainer;
    else if (remainer >= WORDHALF)
    {
        MDword carry = 1;
        MWord *p = a->buf;
        Big_EnsureCapacity(a, a->size + 1);
        add_by_word(p, 0, a->size, carry);
        
        if (carry)
        {
            *p = (MWord)carry;
            a->size++;
        }
    }
    else;

    return a;
}

#define div_by_word(pa, b, size, remainer)                    \
{                                                             \
    MInt i;                                                   \
    MWord *p = pa + (size) - 1;                               \
    for (i = 0; i < size; i++)                                \
    {                                                         \
        MDword t = ((remainer << WORDBITS) | (MDword)(*p));   \
        *p-- = (MWord)(t / b);                                \
        remainer = t % b;                                     \
    }                                                         \
}

static MBigNum Big_DivideBySmall(MBigNum a, MWord b, MWord *Remainer)
{
    MDword remainer = Remainer ? *Remainer : 0;

    Big_Trim(a);

    div_by_word(a->buf, b, a->size, remainer);
    
    if (Remainer)
        *Remainer = (MWord)remainer;
    else if (remainer >= WORDHALF)
    {
        MDword carry = 1;
        MWord *p = a->buf;
        Big_EnsureCapacity(a, a->size + 1);
        add_by_word(p, 0, a->size, carry);
        
        if (carry)
        {
            *p = (MWord)carry;
            a->size++;
        }
    }
    else;

    return a;
}

static MBigNum Big_IntDivideBy(MBigNum a, MWord b, MWord *Remainer)
{
    MWord *p = a->buf + a->size - 1;
    MDword remainer = Remainer ? *Remainer : 0;
    MInt i;
    MInt len = a->size + a->binexp;
    const int N = min(a->size, len);
    for (i = 0; i < N; i++)
    {
        MDword t = ((remainer << WORDBITS) | (MDword)(*p));
        remainer = t % b;
        *p-- = (MWord)(t / b);
    }

    if (Remainer)
        *Remainer = (MWord)remainer;

    Big_Trim(a);
    return a;
}

static MInt clz_w(MWord x)
{
    MInt n = 0; 
    if (x == 0) return(16); 
    if (x <= 0x00FF) {n += 8; x <<= 8;} 
    if (x <= 0x0FFF) {n += 4; x <<= 4;} 
    if (x <= 0x3FFF) {n += 2; x <<= 2;} 
    if (x <= 0x7FFF) {n += 1;} 
    return n; 
}

static void Big_UnsignedBaseDivMod(MWord *pa, const MWord *pb, const MInt aLen, const MInt bLen,
                                   MWord *pQuotient, MWord *pMod)
{
    MWord *p1;
    const MWord *p2;
    MInt remainer = 0;
    MInt len = aLen - bLen + 1;
    MWord divisor = pb[bLen - 1];
    MWord *pres;

    ASSERT(divisor >= WORDHALF);
    ASSERT(len >= 1);

    pres = pQuotient + len - 1;
    p1 = pa + aLen - 1;
    p2 = pb;
    
    len++;
    while (--len)
    {
        MDword t = (remainer << WORDBITS) | *p1;
        MDword q = t / divisor;
        
        if (q)
        {
            MInt bsize = bLen + 1;
            MDword carry = 0;

            MWord *pr = p1 - bLen + 1;
            const MWord *pi = p2;

            // multiply and subtract
            while (--bsize)
            {
                ASSERT(0xFFFFFFFFUL - (MDword)(*pi) * q >= (MDword)(carry));

                MDword pround = (MDword)(*pi++) * q + carry;
                MInt t;

                if ((MInt)(pround) >= 0)
                {
                    t = (MInt)(*pr) - (MInt)(pround);
                    if (t < 0)
                    {
                        carry = ((MDword)(-t + WORDMASK)) >> WORDBITS;
                        t += carry << WORDBITS;
                    }
                    else
                        carry = 0;
                }
                else
                {
                    MDword abst = pround - (MDword)(*pr);
                    {
                        carry = ((MDword)(abst + WORDMASK)) >> WORDBITS;
                        abst -= carry << WORDBITS;
                        t = -(MInt)(abst);
                    }
                }

                *pr++ = (MWord)(t & WORDMASK);
            }

            remainer -= carry;
            while (remainer < 0)
            {
                q--;
                pr = p1 - bLen + 1;
                pi = p2;
                bsize = bLen + 1;

                carry = 0;
                while (--bsize)
                {
                    MDword t = (MDword)(*pr) + MDword(*pi++) + carry;
                    carry = t >> WORDBITS;
                    *pr++ = (MWord)(t & WORDMASK);
                }

                remainer += carry;
            }

            ASSERT(remainer == 0);
        }
        else;

        ASSERT(remainer == 0);
        remainer = *p1;
        *p1-- = 0;

        *pres-- = (MWord)(q);
    }
    
    // store the last remainer
    *++p1 = (MWord)(remainer);
    
    if (pMod)
        memcpy(pMod, pa, bLen * sizeof(pa[0]));
}

static MBigNum Big_UnsignedDivideBy(MBigNum a, const MBigNum b, const MInt wordpreci)
{
    MDword m;
    MInt len;
    MWord *res;
    MWord *temp;
    MInt tempLen;

    ASSERT(wordpreci > 0);
    ASSERT(b->buf[b->size - 1] != 0);

    // shift b
    m = 1 << clz_w(b->buf[b->size - 1]);
    {
        MDword carry = 0;
        res = b->buf;
        mult_by_word(res, m, b->size, carry);
    }
    Big_MultiplyBy(a, (MWord)(m));

    len = wordpreci + 1;
    res = big_alloc(len);
    tempLen = b->size + len - 1;
    temp = big_alloc(tempLen);
    if (tempLen > a->size)
        memcpy(temp + tempLen - a->size, a->buf, a->size * sizeof(a->buf[0]));
    else
        memcpy(temp, a->buf + (a->size - tempLen), tempLen * sizeof(a->buf[0]));
    a->binexp -= tempLen - a->size;

    if (a->capacity < len)
    {
        a->capacity = len;
        if (a->buf) big_free(a->buf);
        a->buf = NULL;
    }

    a->size = len;
    
    Big_UnsignedBaseDivMod(temp, b->buf, tempLen, b->size, res, NULL);
    
    big_free(temp);

    if (a->buf)
    {
        memcpy(a->buf, res, len * sizeof(a->buf[0]));
        big_free(res);
    }
    else
        a->buf = res;

    Big_DivideBySmall(b, (MWord)(m), NULL);

    return a;
}

static void Big_UnsignedIntDivMod(const MBigNum a, const MBigNum b, 
                                  MBigNum *pQuotient, MBigNum *pMod)
{
    MWord m;
    MInt len;
    MWord *res;
    MWord *modres;
    MWord *temp;
    MDword carry = 0;

    ASSERT(b->buf[b->size - 1] != 0);
    ASSERT(*pQuotient == NULL);
    ASSERT(*pMod == NULL);
    ASSERT(a->size >= b->size);

    // shift b
    m = 1 << clz_w(b->buf[b->size - 1]);
    {
        carry = 0;
        res = b->buf;
        mult_by_word(res, m, b->size, carry);
    }

    // create a "large a"
    temp = (MWord *)MHeap_malloc(Heap_MBig, (a->size + 1) * sizeof(a->buf[0]));
    memcpy(temp, a->buf, (a->size + 1) * sizeof(a->buf[0]));
    {
        carry = 0;
        res = temp;
        mult_by_word(res, m, a->size, carry);
        *res++ = (MWord)(carry);
    }

    len = (carry) ? a->size - b->size + 2 : a->size - b->size + 1;
    res = big_alloc(len);
    modres = big_alloc(b->size);

    if (carry)
        Big_UnsignedBaseDivMod(temp, b->buf, a->size + 1, b->size, res, modres);
    else
        Big_UnsignedBaseDivMod(temp, b->buf, a->size, b->size, res, modres);
    
    big_free(temp);
    
    // create results
    {
        MBigNum r = MXRefObj_Create(MTagBigNum, Type_MBig);
        if (r->buf) big_free(r->buf);
        r->capacity = len;
        r->size = len;
        r->buf = res;
        r->bitpreci = -1;
        Big_Normalize(r);
        *pQuotient = r;
    }
    {
        MBigNum r = MXRefObj_Create(MTagBigNum, Type_MBig);
        if (r->buf) big_free(r->buf);
        r->capacity = b->size;
        r->buf = modres;
        r->size = b->size;
        r->bitpreci = -1;
        Big_Normalize(r);
        Big_DivideBySmall(r, (MWord)(m), NULL);
        *pMod = r;
    }

    Big_DivideBySmall(b, (MWord)(m), NULL);
}

MBigNum Big_Create(MString s)
{
    static MString sAccPrecMark = MString_NewC("`");
    static MString sExponentMark = MString_NewC("*^");
    static MString sBaseMark = MString_NewC("^^");

    MBigNum r = Big_Create0();

    int pos;
    int base = 10;
    MString value;
    int tenexp = 0;

    {
        pos = MString_Pos(s, sExponentMark);
        if (pos > 0)
        {
            MString t = MString_Delete(s, pos + 1);
            tenexp = ParseMachineInt(t);
            MString_Release(t);          
            
            value = MString_Copy(s, 1, pos - 1);
        }
        else
            value = MString_NewS(s);
    }

    {
        pos = MString_Pos(value, sAccPrecMark);
        if (pos > 0)
        {
            MString t;
            if ((pos < MString_Len(value)) 
                && (MString_GetAt(value, pos + 1) == MString_GetAt(sAccPrecMark, 1)))
                t = MString_Delete(value, pos + 1);
            else
                t = MString_Delete(value, pos);

            r->bitpreci = ParseMachineInt(t);
            MString_Release(t);          
            
            t = value;
            value = MString_Copy(value, 1, pos - 1);
            MString_Release(t);
        }
        else;
    }

    pos = MString_Pos(value, sBaseMark);
    if (pos > 0)
    {
        MString t = MString_Copy(value, 1, pos - 1);
        base = ParseMachineInt(t);
        MString_Release(t);

        t = value;
        value = MString_Delete(value, pos + 1);
        MString_Release(t);
    }

    if (MString_Len(value) > 0)
    {
        MInt fract;
        MInt intlen;

        pos = 1;
        if (MString_GetAt(value, 1) == '-')
        {
            pos++;
            r->bNeg = true;
        }

        for (; pos <= MString_Len(value); pos++)
        {
            if (MString_GetAt(value, pos) == '.')
            {
                if (r->bitpreci < 0)
                    r->bitpreci = max(MString_Len(value) - pos, 16);
                pos++;
                break;
            }

            Big_MultiplyBy(r, base);
            Big_BaseAddBySmall(r, Char2Int(MString_GetAt(value, pos)));
        }

        Big_Trim(r);
        intlen = r->size;
        
        fract = MString_Len(value) - pos + 1;
        if (fract > 0)
        {
            // our Big is as precious as a IEEE double, at least
            MBigNum unit = Big_Create1(max(base10toBIT(r->bitpreci), IEEE_FACT_WORD_BITS) + 2 * WORDBITS);
            r->bitpreci = unit->bitpreci - (WORDBITS * 2);
            MBigNum t = Big_Uniquelize(unit);
            for (; pos <= MString_Len(value); pos++)
            {
                Big_DivideBySmall(unit, base, NULL);
                Big_MultiplyTo(unit, Char2Int(MString_GetAt(value, pos)), t);
                Big_UnsignedAddBy(r, t);
            }
            
            Big_Release(t);
            Big_Release(unit);
        }
        else if (r->bitpreci > 0)
            r->bitpreci = base10toWORD(r->bitpreci) * WORDBITS;
        r->binexp = intlen - r->size;
    }

    // *10^n
    if (tenexp > 0)
    {
        while (tenexp >= 4)
        {
            Big_MultiplyBy(r, 10000);
            tenexp -= 4;
        }
        switch (tenexp) 
        {
        case 3:
            Big_MultiplyBy(r, 1000);
    	    break;
        case 2:
            Big_MultiplyBy(r, 100);
    	    break;
        case 1:
            Big_MultiplyBy(r, 10);
    	    break;
        }
    }
    else if (tenexp < 0)
    {
        int inc = base10toWORD(-tenexp) + sizeof(double);
        int c = 1;
        ensure_capacity(r, r->size + inc);
        memmove(r->buf + inc, r->buf, r->size * sizeof(r->buf[0]));
        memset(r->buf, 0, inc * sizeof(r->buf[0]));
        r->size += inc;
        r->binexp -= inc;
        r->bitpreci = r->bitpreci > 0 ? (r->bitpreci + inc * WORDBITS) : r->size * WORDBITS;
        while (tenexp <= -4)
        {
            Big_DivideBySmall(r, 10000, NULL);
            tenexp += 4;
            c++;
        }
        switch (tenexp) 
        {
        case -3:
            Big_DivideBySmall(r, 1000, NULL);
    	    break;
        case -2:
            Big_DivideBySmall(r, 100, NULL);
    	    break;
        case -1:
            Big_DivideBySmall(r, 10, NULL);
    	    break;
        } 
        r->bitpreci = min(r->bitpreci, (r->size - 1) * WORDBITS - c);
    }
    
    Big_Normalize(r);
    MString_Release(value);
    return r;
}

MBigNum Big_Create(MInt v)
{
    MBigNum r = MXRefObj_Create(MTagBigNum, Type_MBig);
    CASSERT(sizeof(MWord) * 2 >= sizeof(MInt));

    ensure_capacity(r, 2);
    
    r->size = 2;
    r->bNeg = v < 0; v = labs(v);
    r->bitpreci = -1;
    r->binexp = 0;
    r->buf[0] = v & WORDMASK;
    r->buf[1] = (v >> WORDBITS);
    Big_Normalize(r);
    return r;
}

MBigNum Big_CreateU(MUInt v)
{
    MBigNum r = MXRefObj_Create(MTagBigNum, Type_MBig);
    CASSERT(sizeof(MWord) * 2 >= sizeof(MUInt));

    ensure_capacity(r, 2);
    
    r->size = 2;
    r->bitpreci = -1;
    r->binexp = 0;
    r->buf[0] = v & WORDMASK;
    r->buf[1] = (v >> WORDBITS);
    Big_Normalize(r);
    return r;
}

MBigNum Big_ShiftLeftBy(MBigNum a, MInt off);

MBigNum Big_Create(MReal v)
{
    MBigNum r = MXRefObj_Create(MTagBigNum, Type_MBig);
    MInt i;
    const MWord *p = (MWord *)&v;
    r->size = sizeof(MReal) / sizeof(r->buf[0]);
    
    if (((p[r->size - 1] & 0x7FF) >> 4) == 0x7FF)
    {
        Big_Release(r);
        return Big_Create0();
    }

    ensure_capacity(r, r->size);
    r->bitpreci = r->size * WORDBITS;

    for (i = 0; i < r->size; i++)
        r->buf[i] = p[i];
    i--;
    r->buf[i] &= 0x000F;

    r->bNeg = (p[i] & 0x8000) != 0;
    if (p[i] & 0x7FF0)
    {
        r->buf[i] |= 0x0010;
        i = ((p[i] & 0x7FF0) >> 4) - IEEE_EXPO_BIAS - IEEE_FACT_BITS;
        r->binexp = i / WORDBITS;
        Big_ShiftLeftBy(r, i % WORDBITS);
    }
    else
    {
        i = 1 - IEEE_EXPO_BIAS - IEEE_FACT_BITS;
        r->binexp = i / WORDBITS;
        Big_ShiftLeftBy(r, i % WORDBITS);
    }
    
    Big_Normalize(r);
    return r;
}

MBigNum Big_Create(MBigNum a)
{
    XRef_IncRef(a);
    return a;
}

MBigNum Big_UniquelizeIntPart(MBigNum a)
{
    MBigNum r = MXRefObj_Create(MTagBigNum, Type_MBig);

    MInt s = a->size + a->binexp;
    r->size = max(s, 0);
    ensure_capacity(r, r->size);

    if (r->size > 0)
    {
        s = min(r->size, a->size);
        Zero(r->buf, (r->size - s) * sizeof(MWord));
        memcpy(r->buf + r->size - s, a->buf + a->size - s, s * sizeof(MWord));
    }
    
	r->bNeg = a->bNeg;
    r->bitpreci = -1;
    Big_Normalize(r);
    return r;
}

MBigNum Big_UniquelizeFractPart(MBigNum a)
{
    MBigNum r = MXRefObj_Create(MTagBigNum, Type_MBig);

    MInt s = - a->binexp;
    r->size = min(max(s, 0), a->size);
    if (r->size == 0)
        return r;

    r->bitpreci = max(a->bitpreci - (a->size - r->size) * WORDBITS, 0);
    
    ensure_capacity(r, r->size);
    memcpy(r->buf, a->buf, r->size * sizeof(MWord));
    r->binexp = a->binexp;
    
    Big_Normalize(r);
    return r;
}

MBigNum Big_Uniquelize(MBigNum a)
{
    MBigNum r = MXRefObj_Create(MTagBigNum, Type_MBig);
    MWord *b;
    MInt cap; ASSERT(r->buf || (r->capacity == 0));
    ensure_capacity(r, a->size);
    b = r->buf;
    cap = r->capacity;
    memcpy(r->buf, a->buf, a->size * sizeof(MWord));
    memcpy(r, a, sizeof(MTagBigNum));
    r->buf = b;
    r->capacity = cap;

    return r;
}

MBigNum Big_SetFrom(MBigNum a, const MBigNum b)
{
    MWord *p;
    MInt cap;
    if (a->capacity < b->size)
    {
        if (a->buf)
            big_free(a->buf);

        a->capacity = b->size;
        a->buf = big_alloc(a->capacity);
    }
    memcpy(a->buf, b->buf, b->size * sizeof(a->buf[0]));
    p = a->buf;
    cap = a->capacity;
    memcpy(a, b, sizeof(*a));
    a->buf = p;
    a->capacity = cap;
    return a;
}

static MDword Big_TestBitRounding(const MBigNum a, const int preci, const int bitpreci)
{
    const MWord *p = a->buf + a->size - bitpreci / WORDBITS - 1;
    const MWord *pend = a->buf + a->size - preci - 1;
    if (p >= a->buf)
    {
        MWord w = *p++;
        MWord mask = 1 << (15 - (bitpreci % WORDBITS));
        MDword carry = (w + mask) >> WORDBITS;
        if (carry == 0)
            return 0;

        while (p != pend)
        {
            MDword t = (MDword)(*p++) + carry;
            carry = t >> WORDBITS;
            if (carry == 0)
                return 0;
        }

        return carry;
    }
    else
        return 0;
}

// two stage rounding: rounding at bitpreci, then according to newDecimal
static MBigNum Big_RoundingDecimal(MBigNum a, const int preci, const int bitpreci)
{
    MInt off = a->size - preci - 1;  // pos before last effective word
    if ((off >= 0) && (off < a->size))
    {
        MDword w = a->buf[off] + Big_TestBitRounding(a, preci, bitpreci);
        if (w >= WORDHALF)
        {
            MWord *p;
            MWord *pend;
            MDword carry = 1;
            Zero(a->buf, off * sizeof(a->buf[0]));
            Big_EnsureCapacity(a, a->size + 1);
            p = a->buf + off + 1;

            pend = a->buf + a->size;

            while (p < pend)
            {
                MDword t = (MDword)(*p) + carry;
                carry = t >> WORDBITS;
                *p++ = (MWord)(t & WORDMASK);
                if (carry == 0)
                    return a;
            }

            if (carry)
            {
                *p = (MWord)carry;
                a->size++;
            }
        }
        else;
    }
    else;

    return a;
}

MReal   Big_ToReal(MBigNum a);
MInt    Big_ToInt(MBigNum a);

MString Big_BaseToString(MBigNum a, const MInt Base, MInt fractMaxLen = -1)
{
    MString r;
    
    MString intpart;
    MString fractpart;
    MString basepart;
    MWord carry = 0;
    MInt baseBits;

    ASSERT((Base >= 2) && (Base <= 36));
    r = 0;
    baseBits = ceiling2(Base);

    if (Base != 10)
    {
        char tempC[100 + 1];
        basepart = MString_NewCN(tempC, _snprintf(tempC, 100, "%d^^", Base));
    }
    else
        basepart = MString_NewC("");

    // integer part
    {
        MBigNum tempV = Big_UniquelizeIntPart(a);
        intpart = MString_NewWithCapacity((int)(tempV->size * WORDBITS * log(Base)) + 15);
        if (tempV->size < 8)
        {
            while (!Big_IsZero(tempV))
            {
                MWord remainer = 0;
                Big_IntDivideBy(tempV, Base, &remainer);
                MString_CatChar(intpart, Int2Char(remainer));
            }
            Big_Release(tempV);
            
            if (MString_Len(intpart) == 0)
                MString_CatChar(intpart, '0');
            if (a->bNeg)
                MString_CatChar(intpart, '-');
            MString_Reverse(intpart);
        }
        else
        {
            // let's enlarge the base to MWord to reduce the calling of Big_IntDivideBy
            MWord largeB = Base;
            int index = 1;
            int i;
            char s[WORDBITS];
            while (((largeB * Base) & WORDMASK) / largeB == Base)
            {
                largeB *= Base;
                index++;
            }            
            
            while (!Big_IsZero(tempV))
            {
                MWord remainer = 0;
                Big_IntDivideBy(tempV, largeB, &remainer);

                for (i = 0; i < index; i++)
                {
                    MWord t = remainer % Base;
                    remainer /= Base;
                    s[i] = Int2Char(t);
                }
                MString_CatCN(intpart, s, index);
            }
            Big_Release(tempV);
            
            // there maybe "0" at the lowest digits (high-est digits in value)
            // trim 0
            while ((MString_Len(intpart) > 0) && (MString_GetAt(intpart, MString_Len(intpart)) == '0'))
                intpart->Len--;

            if (MString_Len(intpart) == 0)
                MString_CatChar(intpart, '0');
            if (a->bNeg)
                MString_CatChar(intpart, '-');
            MString_Reverse(intpart);
        }
    }

    // fraction part
    if (a->binexp < 0)
    {
        MBigNum tempV = Big_UniquelizeFractPart(a);
        MInt rounding = max(preci(tempV) - 1, 1);
        fractpart = MString_NewWithCapacity(baseBITtoN(Base, bitpreci(tempV)) + 3);
        MString_CatChar(fractpart, '.');

        //Big_Print(tempV);

        Big_MultiplyBy(tempV, Base);
        Big_RoundingDecimal(tempV, rounding, bitpreci(tempV));
        tempV->bitpreci -= baseBits * 2; // bitpreci is 1 step forward
        
        {
            MInt pos = -tempV->binexp;
            if ((pos >= 1) && (pos < tempV->size))
            {
                MInt v =  tempV->buf[pos];
                
                // carry from the highest fract bit
                if (tempV->buf[pos] >= Base)
                {
                    carry = 1;
                    tempV->buf[pos] -= Base;
                    v -= Base;
                }
                else;

                MString_CatChar(fractpart, Int2Char(v));
                tempV->size = pos;
                tempV->buf[pos] = 0;    
            }
            else
                MString_CatChar(fractpart, '0');
        }

        while (tempV->bitpreci > WORDBITSONEHALF)
        {
            Big_MultiplyBy(tempV, Base);
            Big_RoundingDecimal(tempV, rounding, bitpreci(tempV));
            tempV->bitpreci -= baseBits;

            MInt pos = -tempV->binexp;
            if ((pos >= 1) && (pos < tempV->size))
            {
                MInt v =  tempV->buf[pos];
                MString_CatChar(fractpart, Int2Char(v));
                tempV->size = pos;
                tempV->buf[pos] = 0;    
            }
            else
                MString_CatChar(fractpart, '0');
        }

        // trim 0
        while ((MString_Len(fractpart) > 0) && (MString_GetAt(fractpart, MString_Len(fractpart)) == '0'))
            fractpart->Len--;

        Big_Release(tempV);
    }
    else if (Big_IsInt(a))
        fractpart = MString_NewC("");
    else
        fractpart = MString_NewC(".");

    if (carry)
    {
        MBigNum tempV = Big_UniquelizeIntPart(a);
        MString_SetLen(intpart, 0);
        Big_BaseAddBySmall(tempV, carry);
        while (!Big_IsZero(tempV))
        {
            MWord remainer = 0;
            Big_IntDivideBy(tempV, Base, &remainer);
            MString_CatChar(intpart, Int2Char(remainer));
        }
        Big_Release(tempV);
       
        if (a->bNeg)
            MString_CatChar(intpart, '-');
        MString_Reverse(intpart);
    }
    else;

    r = MString_NewWithCapacity(MString_Len(basepart) + MString_Len(intpart) + MString_Len(fractpart));
    MString_Cat(MString_Cat(MString_Cat(r, basepart), intpart), fractpart);
    MString_Release(basepart);
    MString_Release(intpart);
    MString_Release(fractpart);

    return r;
}

MBool   Big_IsMachineInt(MBigNum a)
{
    MBool r = Big_IsInt(a);
    if (r)
    {
        if (a->size > ((int)sizeof(MInt)) / (int)(sizeof(MWord)))
            return false;
        else if (a->size == sizeof(MInt) / sizeof(MWord))
            r &= (0 == (a->buf[sizeof(MInt) / sizeof(MWord) - 1] & (1 << (WORDBITS - 1))));
        else;
    }
    return r;
}

MInt    Big_ToInt(MBigNum a)
{
    MInt r = 0;
    MWord *p = a->buf - a->binexp;    
    MInt s = a->size + a->binexp;
    
    if (s > 0)
    {
        if (p >= a->buf)
        {
            r = p[0];
        }

        s--;
        p++;

        if ((s > 0) && (p >= a->buf))
            r |= (p[0] << WORDBITS);
    }
    else;

    return a->bNeg ? -r : r;
}

/************************************************************************
Double precision binary floating-point format
    Sign bit: 1 bit
    Exponent width: 11 bits
    Significant precision: 52 bits (53 implicit)

Exponent encoding
    Emin (0x001) = -1022
    Emax (0x7fe) = 1023
    Exponent bias (0x3ff) = 1023

The true exponent = written exponent - exponent bias

The exponents 0x000 and 0x7ff have a special meaning:
    0x000 is used to represent zero and denormals.
    0x7ff is used to represent infinity and NaNs.

All bit patterns are valid encoding.

The entire double precision number is described by:

(-1)^sign * 2 ^ (exponent - 1023) * 1.mantissa

For Denormalized numbers the exponent is (-1022) (the minimum exponent for a 
normalized number¡ªit is not (-1023) because normalised numbers have a leading 
1 digit before the binary point and denormalized numbers do not). As before, 
both infinity and zero are signed.

Double precision examples
   3    2    1    0    <---- Index
0x3ff0 0000 0000 0000   = 1
0x3ff0 0000 0000 0001   = 1.0000000000000002220446049250313080847263336181640625, the next higher number > 1
0x3ff0 0000 0000 0002   = 1.000000000000000444089209850062616169452667236328125
0x4000 0000 0000 0000   = 2
0xc000 0000 0000 0000   = ¨C2
0x7fef ffff ffff ffff   ¡Ö 1.7976931348623157 x 10308 (Max Double)
0x0000 0000 0000 0000   = 0
0x8000 0000 0000 0000   = ¨C0
0x7ff0 0000 0000 0000   = Infinity
0xfff0 0000 0000 0000   = -Infinity
0x3fd5 5555 5555 5555   ¡Ö 1/3
************************************************************************/
MReal   Big_ToIEEE754(MBigNum a)
{
    CASSERT(sizeof(MReal) == 8);
#define IEEE_WORD_LEN (((int)sizeof(MReal)) / (int)sizeof(MWord))

    union
    {
        MWord mimic[IEEE_WORD_LEN];
        MReal r;
    };

    Zero(mimic, sizeof(mimic));

#define shl(v, off) (((off) > 0) ? ((v) << (off)) : ((v) >> (off)))
#define shr(v, off) (((off) > 0) ? ((v) >> (off)) : ((v) << (off)))

    if (a->size > 0)
    {
        MWord buf[IEEE_WORD_LEN] = {0};
        MInt pos = min(a->size - 1, 3);
        const MWord *p = a->buf + a->size - 1 - pos - 1;
        const MInt shift = clz_w(*(a->buf + a->size - 1));
        const MDword Multi = 1 << (shift + 1);
        MWord remain = 0;
        MDword carry = 0;

        carry = (MDword(*p++) * Multi + carry) >> WORDBITS;
        while (pos >= 0)
        {
            MDword t = MDword(*p++) * Multi + carry;            
            buf[pos] = MWord(t & 0xFFFF);
            carry = t >> WORDBITS;
            pos--;
        }

        for (pos = 0; pos < IEEE_WORD_LEN; pos++)
        {
            mimic[IEEE_WORD_LEN - 1 - pos] = remain | (buf[pos] >> 12);
            remain = (buf[pos] << 4);
        }

        // exponent
        {
            MInt expo = (a->binexp + a->size - 1) * WORDBITS + 16 - shift - 1;
            expo += 1023;
            if (expo > 0)
            {
                if (expo < 0x7FF)
                    mimic[IEEE_WORD_LEN - 1] = (expo << 4) | (mimic[IEEE_WORD_LEN - 1] & 0xF);
                else
                {
                    // +inf
                    mimic[IEEE_WORD_LEN - 1] = 0x7FF0;
                    Zero(mimic, sizeof(mimic) - sizeof(mimic[0]));
                }
            }
            else
            {
                // 0
                Zero(mimic, sizeof(mimic));
            }
        }

        // neg?
        if (a->bNeg)
            mimic[IEEE_WORD_LEN - 1] |= 0x8000;
    }

    return r;
}

MString Big_ToString(MBigNum a, const MInt Base)
{
    MBigNum b = NULL;
    MString exp = NULL;
    MString r;
    int Exp = 0;

    //Big_Print(a);

    // need a ^*... to make the result beautiful?
    // Condition: decimal point is too far from the effecient part
    if (!Big_IsInt(a))
    {
        char tempC[100 + 1];
        int distance = abs(a->binexp + a->size);
        // printf("distance: %d, bitpreci: %d, binexp: %d\n", distance, bitpreci(a), a->binexp);
        if (distance > max(preci(a) >> 2, 2))
        {
            b = Big_Uniquelize(a);
            
            // Step1. / 10000 when > 1 --> (-10000, 10000)
            {
                MBigNum tenth = Big_Create(10000);
                while (Big_AbsCmp(b, tenth) > 0)
                {
                    Big_DivideBySmallFull(b, 10000, NULL);
                    Big_Normalize(b);
                    Exp += 4;
                }
                Big_Release(tenth);
            }

            // Step2, * 10000 when < 1 --> (-10000, -1000) or (1000, 10000)
            {
                MBigNum one = Big_Create(1);
                while (Big_AbsCmp(b, one) < 0)
                {
                    Big_MultiplyBy(b, 10000);
                    Big_Normalize(b);
                    Exp -= 4;
                }
                Big_Release(one);
            }
            
            // Step3. / 10 when > 1 --> (-10, 10)
            {
                MBigNum ten = Big_Create(10);
                while (Big_AbsCmp(b, ten) >= 0)
                {
                    Big_DivideBySmall(b, 10, NULL);
                    Big_Normalize(b);
                    Exp++;
                }
                Big_Release(ten);
            }

            // Big_Print(b);

            exp = MString_NewCN(tempC, _snprintf(tempC, 100, "*^%d", Exp));
        }
        else;
    }

    if (b) 
    {
        r = Big_BaseToString(b, Base);
        Big_Release(b);
        MString_Cat(r, exp);
        MString_Release(exp);
    }
    else
        r = Big_BaseToString(a, Base);

    return r;
}

MBigNum Big_Add(MBigNum a, MBigNum b);
MBigNum Big_AddBy(MBigNum a, MBigNum b);
MBigNum Big_Subtract(MBigNum a, MBigNum b);
MBigNum Big_SubtractBy(MBigNum a, MBigNum b);
MBigNum Big_Divide(MBigNum a, MBigNum b);
MBigNum Big_DivideBy(MBigNum a, MBigNum b);
MBigNum Big_Multiply(MBigNum a, MBigNum b);
MBigNum Big_MultiplyBy(MBigNum a, MBigNum b);

MBigNum Big_Power(MBigNum a, MBigNum b);
MBigNum Big_Sqrt(MBigNum a, MInt precision);

MBigNum Big_ShiftLeftBy(MBigNum a, MInt off);

MBigNum Big_Normalize(MBigNum a)
{
    if (a->size > 0)
    {
        MWord *w = a->buf;
        MInt i = 0;
        
        if (bitpreci(a) >= 0)
        {
            while ((*w++ == 0) && (i < a->size)) i++;

            if (i > 0)
            {
                memmove(a->buf, a->buf + i, (a->size - i) * sizeof(MWord));
                a->size -= i;
                a->binexp += i;
            }
        }
        else if (a->binexp > 0)
        {
            // insert zeroes
            big_insert_0(a, a->binexp);
            a->binexp = 0;
        }

        w = a->buf + a->size - 1;
        i = 0;
        while ((*w-- == 0) && (i < a->size)) i++;
        if (i > 0)
        {
            a->size -= i;
            if (a->bitpreci > 0)
            {
                a->bitpreci -= i * WORDBITS;
                if (a->bitpreci < 0)
                    a->bitpreci = 0;
            }
        }
    }
    else;

    if (a->size <= 0)
    {
        a->size = 0;
        a->binexp = 0;
        a->bNeg = false;
    }

    // truncting
    if (a->bitpreci >= 0)
    {
        // TODO: how to handle this easily?
#define over_precision 1

        MInt maxSize = (a->bitpreci + WORDBITS - 1) / WORDBITS + over_precision;
        if (a->size > maxSize)
        {
            Big_RoundingDecimal(a, maxSize, bitpreci(a));
            memmove(a->buf, a->buf + a->size - maxSize, maxSize * sizeof(a->buf[0]));
            a->binexp += a->size - maxSize;
            a->size = maxSize;
        }
    }

    return a;
}

MBigNum Big_Add(const MBigNum a, const MBigNum b)
{
    MBigNum r = Big_Uniquelize(a);
    Big_AddBy(r, b);
    return r;
}

MBigNum Big_AddBy(MBigNum a, const MBigNum b)
{
    int preciExpo = max(bitpreciExp(a), bitpreciExp(b));

    if (a->bNeg ^ b->bNeg)
    {
        MInt cmp = Big_AbsCmp(a, b);
        if (cmp >= 0)
            Big_UnsignedSubtractBy(a, b);
        else
        {
            MBigNum t = Big_Uniquelize(b);
            
            Big_UnsignedSubtractBy(t, a); 
            Big_SetFrom(a, t);
            a->bNeg = b->bNeg;

            Big_Release(t);
        }
    }
    else
        Big_UnsignedAddBy(a, b);

    if (!is_max_preciExp(preciExpo))
        update_preci(a, preciExpo);

    Big_Normalize(a); 
    return a;
}

MBigNum Big_AddBy(MBigNum a, MSWord b)
{
    MWord *p;
    MDword carry = 0;

    if (b == 0)
        return a;

    if (a->binexp != 0)
        goto complex_case;
    else;

    if (a->bNeg)
    {
        if (b < 0)
		{
			b = -b;
            goto add_case;
		}
        else
            goto complex_case;
    }
    else
    {
        if (b > 0)
            goto add_case;
        else 
        {
            if ((a->size >= 2) 
                || ((a->size == 1) && (a->buf[0] >= (-b))))
            {
                MInt carry = 0;
                p = a->buf;
                b = -b;
                subtract_by_word(p, b, a->size, carry);
                ASSERT(carry == 0);
                goto add_case_done;
            }
            else
                goto complex_case;
        }
    }

add_case:
    p = a->buf;
    add_by_word(p, b, a->size, carry);
    if (carry)
    {
        Big_EnsureCapacity(a, a->size + 1);
        a->buf[a->size] = (MWord)(carry);
        a->size++;
    }

add_case_done:
    Big_Normalize(a);
    return a;

complex_case:
    {
        MBigNum t = Big_Create(b);
        Big_AddBy(a, t);
        Big_Release(t);
        return a;
    }
}

MBigNum Big_Subtract(const MBigNum a, const MBigNum b)
{
    MBigNum r = Big_Uniquelize(a);
    Big_SubtractBy(r, b);
    return r;
}

MBigNum Big_SubtractBy(MBigNum a, const MBigNum b)
{
    a->bNeg = !a->bNeg;
    Big_AddBy(a, b);
    if (!Big_IsZero(a))
        a->bNeg = !a->bNeg;
    return a;
}

MBigNum Big_Divide(const MBigNum a, const MBigNum b, MInt TargetPrecision)
{
    MBigNum r = Big_Uniquelize(a);
    Big_DivideBy(r, b, TargetPrecision);
    return r;
}

static MBigNum Big_Int2Float(MBigNum a, int wordpreci)
{
    ASSERT(Big_IsInt(a));
    wordpreci = max(a->size, wordpreci);    
    a->binexp = a->size - wordpreci;
    a->bitpreci = wordpreci * WORDBITS;
    if (a->binexp == 0)
        return a;
    
    if (a->capacity < wordpreci)
    {
        a->capacity = wordpreci;
        MWord *p = big_alloc(a->capacity);
        memcpy(p - a->binexp, a->buf, a->size * sizeof(a->buf[0]));
        if (a->buf) big_free(a->buf);
        a->buf = p;
    }
    else
    {
        memmove(a->buf - a->binexp, a->buf, a->size * sizeof(a->buf[0]));
        Zero(a->buf, (-a->binexp) * sizeof(a->buf[0]));
    }
   
    a->size = wordpreci;

    return a;
}

MBigNum Big_DivideBy(MBigNum a, const MBigNum b, MInt TargetPrecision)
{
    if (Big_IsZero(b))
        return a;

    TargetPrecision = TargetPrecision < 0 ? base10toWORD(Big_GetWorkingPrecission()) : base10toWORD(TargetPrecision);

    if (b->size == 1)
    {
        if (Big_IsInt(a))
            Big_Int2Float(a, max(a->size - b->size + 2, Big_IsInt(b) ? TargetPrecision : preci(b)) + 1);
        else
            bitpreci(a) = min(bitpreci(a), bitpreci(b));

        Big_DivideBySmall(a, b->buf[0], NULL);
    }
    else
    {
        MInt precir;
        
        if (Big_IsInt(a))
        {
            if (Big_IsInt(b))
                precir = max(a->size - b->size + 2, TargetPrecision);
            else
                precir = big_preci(b);
        }
        else
        {
            if (Big_IsInt(b))
                precir = big_preci(a);
            else
                precir = min(big_preci(a), big_preci(b));
        }
        
        a->binexp -= b->binexp;
        a->bNeg ^= b->bNeg;
        a->bitpreci = precir * WORDBITS;
        Big_UnsignedDivideBy(a, b, precir);        
    }

    Big_Normalize(a);
    return a;
}

MBigNum Big_IntDivMod(const MBigNum a, const MBigNum b, MBigNum *premainer)
{
    ASSERT(Big_IsInt(a));
    ASSERT(Big_IsInt(b));
    ASSERT(*premainer == NULL);

    if (Big_IsZero(b))
        return Big_Create(a);

    if (Big_AbsCmp(a, b) < 0)
    {
        *premainer = Big_Create(a);
        return Big_Create0();
    }
    else
    {
        if (b->size > 1)
        {
            MBigNum r = NULL;
            Big_UnsignedIntDivMod(a, b, &r, premainer);
            r->bNeg = a->bNeg ^ b->bNeg;
            (*premainer)->bNeg = a->bNeg;

            Big_Normalize(r);
            Big_Normalize(*premainer);
            return r;
        }
        else
        {
            MWord remainer = 0;
            MBigNum r = Big_Uniquelize(a);
            Big_DivideBySmall(r, b->buf[0], &remainer);
            *premainer = Big_Create(remainer);
            r->bNeg = a->bNeg ^ b->bNeg;
            (*premainer)->bNeg = a->bNeg;

            Big_Normalize(r);
            Big_Normalize(*premainer);
            return r;
        }
    }
}

MBigNum Big_IntDiv(const MBigNum a, const MBigNum b)
{
    MBigNum pmod = NULL;
    MBigNum r = Big_IntDivMod(a, b, &pmod);
    Big_Release(pmod);
    return r;
}

MBigNum Big_IntMod(const MBigNum a, const MBigNum b)
{
    MBigNum pmod = NULL;
    MBigNum r = Big_IntDivMod(a, b, &pmod);
    Big_Release(r);
    return pmod;
}

MBigNum Big_DivMod(const MBigNum a, const MBigNum b, MBigNum *premainer)
{
    if (Big_IsZero(b))
    {
        *premainer = Big_Create(a);
        return Big_Create0();
    }
    
    if (Big_IsInt(a) && Big_IsInt(b))
        return Big_IntDivMod(a, b, premainer);

    if (Big_AbsCmp(a, b) < 0)
    {
        *premainer = Big_Create(a);
        return Big_Create0();
    }
    else
    {
        MBigNum whole = Big_Divide(a, b, baseBITto10(max(a->bitpreci, b->bitpreci)) + 1);
        MBigNum r = Big_UniquelizeIntPart(whole);
        *premainer = Big_MultiplyBy(r, b);
        Big_Release(whole);
        return r;
    }
}

MBigNum Big_Div(const MBigNum a, const MBigNum b)
{
    if (Big_IsZero(b))
        return Big_Create(a);
    
    if (Big_IsInt(a) && Big_IsInt(b))
        return Big_IntDiv(a, b);

    if (Big_AbsCmp(a, b) < 0)
    {
        return Big_Create0();
    }
    else
    {
        MBigNum whole = Big_Divide(a, b, baseBITto10(max(a->bitpreci, b->bitpreci)) + 1);
        MBigNum r = Big_UniquelizeIntPart(whole);
        Big_Release(whole);
        return r;
    }
}

MBigNum Big_Mod(const MBigNum a, const MBigNum b)
{
    if (Big_IsZero(b))
        return Big_Create0();
    
    if (Big_IsInt(a) && Big_IsInt(b))
        return Big_IntMod(a, b);

    if (Big_AbsCmp(a, b) < 0)
        return Big_Create(a);
    else
    {
        MBigNum whole = Big_Divide(a, b, baseBITto10(max(a->bitpreci, b->bitpreci)) + 1);
        MBigNum r = Big_UniquelizeFractPart(whole);
        Big_Release(whole);
        return Big_MultiplyBy(r, b);
    }
}

MBigNum Big_Multiply(const MBigNum a, const MBigNum b)
{
    if (Big_IsZero(a) || Big_IsZero(b))
        return Big_Create0();
    else
    {
        MBigNum r = Big_Uniquelize(a);
        Big_MultiplyBy(r, b);
        return r;
    }
}

MBigNum Big_MultiplyBy(MBigNum a, const MBigNum b)
{
    Big_UnsignedBaseMutiplyBy(a, b);
    a->binexp += b->binexp;
    a->bNeg = a->bNeg ^ b->bNeg;
    if (bitpreci(a) > 0)
    {
        a->bitpreci = minpreci(a->bitpreci, b->bitpreci);
       // a->bitpreci -= (a->bitpreci >= 1 ? 1 : 0);
       // a->bitpreci -= (a->bitpreci >= 1 ? 1 : 0);
    }
    else
        a->bitpreci = minpreci(a->bitpreci, b->bitpreci);
    Big_Normalize(a);
    return a;
}

MBigNum Big_ShiftLeftBy(MBigNum a, MInt off)
{
    bool bIsInt = Big_IsInt(a);
    if (Big_IsZero(a))
        return a;

    if (bIsInt && (off > 0))
    {
        big_insert_0(a, off / WORDBITS);
        a->binexp = 0;
    }
    else
        a->binexp += off / WORDBITS;
    off = off % WORDBITS;
    if (off < 0)
    {
        off += WORDBITS;
        a->binexp -= 1;
    }

    // now, ((off >= 0) && (off < WORDBITS));

    if (off)
    {
        MInt i = a->size;
        MWord *p = a->buf + a->size - 1;
        MDword carry = 0;
        if (off > clz_w(a->buf[a->size - 1]))
        {
            Big_EnsureCapacity(a, a->size + 1);
            p = a->buf + a->size - 1;
            a->size++;
        }
        
        // TODO: optimize: if first carry == 0, no need to memmove at last.
        for (; i > 0; i--)
        {
            MDword t = (MDword(*p) << off) | (carry << WORDBITS);
            carry = t & WORDMASK;
            t = t >> WORDBITS;
            if (t)
                *(p + 1) = (MWord)(t);
            p--;
        }
        p++;
        *p = (MWord)(carry);
        if ((carry == 0) && (a->size > 0) && !bIsInt)
        {
            a->binexp++;
            memmove(a->buf, a->buf + 1, (a->size - 1) * sizeof(a->buf[0]));
            a->size--;
        }
    }

    if (bIsInt && (a->binexp < 0))
    {
        memmove(a->buf, a->buf - a->binexp, (a->size + a->binexp) * sizeof(a->buf[0]));
        a->size += a->binexp;
        a->binexp = 0;
    }

    return a;
}

MBigNum Big_ShiftLeft(const MBigNum a, const MInt off)
{
    MBigNum r = Big_Uniquelize(a);
    Big_ShiftLeftBy(r, off);
    return r;
}

// Note:
// Return: 1/-1 if the 2 buffers only differ in the least signaficant Word by "1"
inline int __fastcall reversecmp_w0(
        const MWord * buf1,
        const MWord * buf2,
        size_t count,
        MInt carry
        )
{
    const MDword *pd1 = (const MDword *)(buf1 - 1);
    const MDword *pd2 = (const MDword *)(buf2 - 1);
    MInt r = carry;
    if (r == 0)
    {
        while ((count >= 3) && (*pd1 == *pd2))
        {
            count -= 2;
            pd1--;
            pd2--;
        }
    }
    else;

    buf1 = (const MWord *)(pd1); buf1++;
    buf2 = (const MWord *)(pd2); buf2++;

    while (count && (abs(r) <= 1))
    {
        r = (r << WORDBITS) + (*buf1--) - (*buf2--);
        count--;
    }
    return (count != 0) ? r << 1 : r;
}

inline int __fastcall reversecmp_w(
        const MWord * buf1,
        const MWord * buf2,
        size_t count,
        int off
        )
{
    int carry = 0;
    
    if (off == -1)
    {
        carry = - (*buf2--);
        count--;
    }
    else if (off == 1)
    {
        carry = *buf1--;
        count--;
    }
    else;

    return reversecmp_w0(buf1, buf2, count, carry);
}

MInt    Big_Cmp(const MBigNum a, const MBigNum b)
{
    if (a->bNeg ^ b->bNeg)
        return a->bNeg ? -1 : 1;
    else
        return a->bNeg ? -Big_AbsCmp(a, b) : Big_AbsCmp(a, b);
}

MInt    Big_AbsCmp(const MBigNum a, const MBigNum b)
{
#define round_to(a, preci)    \
            ((!Big_IsInt(a)) && (a->size > (preci)) && \
             ((MDword)(a->buf[a->size - (preci) - 1]) + Big_TestBitRounding(a, (preci), bitpreci(a)) >= \
                ((MDword)(1) + WORDMASK)) ? 1 : 0)

#define round_preci(a)    \
    ((!Big_IsInt(a)) && (a->size > preci(a)) && (a->buf[a->size - preci(a) - 1] >= WORDHALF) ? 1 : 0)

#define check_remaining(a, sign)            \
    MWord *p = a->buf + a->size - len - 1;  \
    while (--i >= 0)                        \
    {                                       \
        if (*p)                             \
        {                                   \
            r = sign;                       \
            break;                          \
        };                                  \
        p--;                                \
    }
   
    int r = 0;
    int len;
    int int1 = a->size + a->binexp;
    int int2 = b->size + b->binexp;
    if (a->size < 1)
        return -b->size;
    if (b->size < 1)
        return a->size;

    // now, both size > 0
    r = int1 - int2;
    if (r > 1)
    {
        return r;
    }
    else if (r < -1)
    {
        return r;
    } 
    else;

    // now highest WORD are aligned by (int1 - int2)
    const int preci_a = logic_preci(a);
    const int preci_b = logic_preci(b);
    int i;
    len = min(preci_a, preci_b);
    if (len)
    {
        r = reversecmp_w(a->buf + a->size - 1, b->buf + b->size - 1, len, r);
        switch (r)
        {
        case 0:
            i = preci_a - preci_b; 
            if (i > 0)
            {
                check_remaining(a, 1);
                if (r == 0)
                {
                     r = round_to(b, bitpreci(b)) ? 1 : 0;
                }
            }
            else if (i < 0)
            {
                i = -i;
                check_remaining(b, -1);
                if (r == 0)
                {
                     r = round_to(b, bitpreci(b)) ? -1 : 0;
                }
            }
            else;
            break;
        case 1:
            // a - b == 1
            if (preci_a < preci_b)
                r = round_to(b, preci_a - (int1 - int2)) ? 0 : r;
            else;
            break;
        case -1:
            // a - b == -1
            if (preci_b < preci_a)
                r = round_to(a, preci_b - (int2 - int1)) ? 0 : r;
            else;
            break;
        default:
            ;
        }
    }
    else if (r == 0)
        r = logic_bitpreci(a) - logic_bitpreci(b);

    return r;
}

MBool   Big_LT (const MBigNum a, const MBigNum b)
{
    if (a->bNeg ^ b->bNeg)
        return a->bNeg;
    else
        return (Big_AbsCmp(a, b) < 0) ^ a->bNeg;
}

MBool   Big_LTE(const MBigNum a, const MBigNum b)
{
    if (a->bNeg ^ b->bNeg)
        return a->bNeg;
    else
        return (Big_AbsCmp(a, b) <= 0) ^ a->bNeg;
}
MBool   Big_E  (const MBigNum a, const MBigNum b)
{
    if (a->bNeg ^ b->bNeg)
        return false;
    else
        return Big_AbsCmp(a, b) == 0;
}

MBool   Big_GTE(const MBigNum a, const MBigNum b)
{
    if (a->bNeg ^ b->bNeg)
        return !a->bNeg;
    else
        return (Big_AbsCmp(a, b) >= 0) ^ a->bNeg;
}

MBool   Big_GT (const MBigNum a, const MBigNum b)
{
    if (a->bNeg ^ b->bNeg)
        return !a->bNeg;
    else
        return (Big_AbsCmp(a, b) > 0) ^ a->bNeg;
}

MBool   Big_NE (const MBigNum a, const MBigNum b)
{
    return !Big_E(a, b);
}


MBool   Big_SameQ(const MBigNum a, const MBigNum b)
{
    MBool r = (a->binexp == b->binexp) && (a->bNeg == b->bNeg)
                && (a->size == b->size) && (a->bitpreci == b->bitpreci);
    return r ? memcmp(a->buf, b->buf, a->size * sizeof(a->buf[0])) == 0 : r;
}

MBool   Big_UnsameQ(const MBigNum a, const MBigNum b)
{
    MBool r = (a->binexp != b->binexp) || (a->bNeg != b->bNeg)
                || (a->size != b->size) || (a->bitpreci != b->bitpreci);
    return r ? r : memcmp(a->buf, b->buf, a->size * sizeof(a->buf[0])) != 0;    
}

MReal   Big_Accuracy(MBigNum a)
{
    // Log[10, 2] = 0.301029995663981
    // #define baseBITto10(v) ((MInt)ceil(0.301029995663981 * (v)))
    return a->bitpreci > 0 ? 0.301029995663981 * a->bitpreci : -1.;
}

MReal   Big_Precision(MBigNum a)
{
    return pow(65536., a->binexp - 1);
}

#define base_normal(p, size)            \
{                                       \
    p += size - 1;                      \
    while ((*p == 0) && (size > 0))     \
    {                                   \
        p--; size--;                    \
    }                                   \
}

MBigNum Big_Power(MBigNum a, MBigNum b)
{
    if (Big_IsInt(b))
    {
        if (Big_IsZero(a))
            return Big_Create(1);

        if (!Big_IsZero(b))
        {
            bool bNeg = b->bNeg;
            MBigNum t = Big_UniquelizeIntPart(b);            
            MBigNum v = Big_Create(a);
            MBigNum r = Big_Uniquelize(v);
            MWord *p = t->buf;
            MInt carry = 0;
            t->bNeg = false;
            subtract_by_word(p, 1, t->size, carry);
            p = t->buf;
            base_normal(p, t->size);
            while (!Big_IsZero(t))
            {
                if (t->buf[0] & 0x0001)
                {
                    Big_MultiplyBy(r, v);
                    p = t->buf;
                    subtract_by_word(p, 1, t->size, carry);
                    p = t->buf;
                    base_normal(p, t->size);
                }
                else
                {
                    MBigNum tt = Big_Multiply(v, v);
                    Big_Release(v);
                    v = tt;

                    // t /= 2
                    Big_ShiftLeftBy(t, -1);
                }
            }
            Big_Release(t);
            Big_Release(v);
            if (bNeg)
            {
                MBigNum one = Big_Create1((r->size + 1) * WORDBITS);
                t = Big_Divide(one, r, -1);
                Big_Release(r);
                r = t;
            }
            return r;
        }
        else
            return Big_Create(1);
    }
    else
    {
        // use machine double currently
        return Big_Create(pow(Big_ToIEEE754(a), Big_ToIEEE754(b)));
    }
}

MBigNum Big_Sqrt(MBigNum a, MInt precision)
{
    // TODO:
    return Big_Create(sqrt(Big_ToIEEE754(a)));
}

MBool Big_EvenQ(MBigNum a)
{
    if (!Big_IsInt(a))
        return false;
    if (Big_IsZero(a))
        return true;
    return 0 == (a->buf[0] & 0x0001);
}

///////// ------------------------------------
#define _BIG_FREE_HEAP

static MBigNum MBig_Dispose(MBigNum s)
{
#ifdef _BIG_FREE_HEAP
    if (s->buf)
        big_free(s->buf);
    return NULL;
#else
    MWord *t = s->buf;
    MInt cap = s->capacity;
    Zero(t, cap * sizeof(s->buf[0]));
    Zero(s, sizeof(*s));
    s->buf = t;
    s->capacity = cap;
    return s;
#endif
}

static char *MBig_DumpObj(MBigNum v, char *s, const MInt maxlen)
{
    MString s2 = Big_ToString(v);
    s[maxlen - 1] = '\0';
    MStringToASCII(s2, (MByte *)s, maxlen - 2);
    MString_Release(s2);
    return s;
}

static MXRefObjMT MBig_MT = {NULL, (f_YM_XRefObjDispose)MBig_Dispose, (f_YM_XRefObjDump)MBig_DumpObj}; // (f_YM_XRefObjDump)MBig_DumpObj

void Big_Init()
{
    // do for 32 bit
    CASSERT(sizeof(MWord) * 2 == sizeof(MDword));
    CASSERT(sizeof(MWord) >= 2);
    
    Type_MBig = MXRefObj_Register(&MBig_MT, sizeof(MTagBigNum), "MBigNum");
    Heap_MBig = MHeap_NewHeap("MBig", 10000000);
}

void Big_DumpHeap(FILE *file)
{
    MHeap_Dump(Heap_MBig, file);
}
