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
#include <wchar.h>

#include "mstr.h"
#include "mm.h"
#include "encoding.h"

static MXRefObjType Type_MString = NULL;
static MHeap        Heap_MString = NULL;
static MString EmptyStr;

//static int SuggestCapacity(const int len)
//{
//#ifndef _DEBUG
//    if (len < 256)
//        return 256;
//    else
//        return len + 256;
//#else
//    return len;
//#endif
//}

#define make_str_len(r, l)                                      \
    if (r->Capacity < (l))                                      \
    {                                                           \
        if (r->pData) MHeap_free(Heap_MString, r->pData);       \
        r->Capacity = (l);                                      \
        r->pData = (MChar *)MHeap_malloc(Heap_MString, (r->Capacity) << 1); \
    }

MString MString_NewC(const char *s)
{
    if ((s == NULL) || (s[0] == 0))
        return MString_NewS(EmptyStr);
    else
        return MString_NewCN(s, strlen(s));
}

MString MString_NewCN(const char *s, const int Len)
{
    int len = Len;
    if (len <= 0)
        return MString_NewS(EmptyStr);

    MString r = MXRefObj_Create(MTagString, Type_MString);
    r->Len = len;
    make_str_len(r, r->Len);
    for (int i = 0; i < r->Len; i++)
        r->pData[i] = s[i];

    return r;
}

MString MString_CopyCN(MString s, const char *cs, const int len)
{
    s->Len = min(s->Capacity, len);
    for (int i = 0; i < s->Len; i++)
        s->pData[i] = cs[i];
    return s;
}

MString MString_NewWithCapacity(const int Capacity)
{
    MString r = MXRefObj_Create(MTagString, Type_MString);
    r->Len = 0;
    make_str_len(r, Capacity);

    return r;
}

MString MString_NewM(const MChar *s)
{
    return MString_NewMN(s, wcslen((wchar_t *)s));
}

MString MString_NewMN(const MChar *s, const int Len)
{
    MString r = MXRefObj_Create(MTagString, Type_MString);
    r->Len = Len;
    make_str_len(r, r->Len);
    memcpy(r->pData, s, r->Len << 1);

    return r;
}

MString MString_NewS(const MString s)
{
    XRef_IncRef(s);
    return s;
}

MString MString_Unique(const MString s)
{
    MString r = MXRefObj_Create(MTagString, Type_MString);
    r->Len = MString_Len(s);
    make_str_len(r, r->Len);
    memcpy(r->pData, s->pData, r->Len << 1);
    return r;
}

//void    MString_Release(MString s)
//{
//    XRef_DecRef(s);
//}

static int MString_Pos_1(const MChar *s, const int len, const MChar c)
{
    int i = 0;
    for (i = 0; i < len; i++)
        if (c == s[i])
            return i + 1;
    return -1;
}

static int MString_Pos_2(const MChar *s, const int len, const MChar *sub)
{
    int i = 0;
    MDword d = *(MDword *)sub;
    for (i = 0; i < len - 1; i++)
        if (*(MDword *)(s + i) == d)
            return i + 1;
    return -1;
}

static int MString_Pos_3(const MChar *s, const int len, const MChar *sub)
{
    int i = 0;
    MChar c1 = sub[0];
    MChar c2 = sub[1];
    for (i = 0; i < len - 2; i++)
        if ((c1 == s[i]) && (c2 == s[i + 1]) && (c2 == s[i + 2]))
            return i + 1;
    return -1;
}

static int  StrIndex_KMP(const MChar *s, const MChar *t, const int s_len, const int t_len, const int *next) 
{
    int i = 0;
    int j = 0;

    while (i < s_len && j < t_len)
    {
        if ((j == -1) || (s[i] == t[j]))
        {
            i++; j++;
        } 
        else 
            j = next[j];
    }

    if (j >= t_len)  
        return i - j;
    else  
        return -1;
}

static void KMPGet_nextpos(const MChar *t, int *next, const int len)
{    
    int i,j;
    i = 0; 
    j = -1; 
    next[0] = -1;
    while (i < len) 
    {    
        if((j == -1) || (t[i] == t[j]))
        {    
            i++; j++;
            if (t[i] != t[j]) 
                next[i] = j;
            else  
                next[i] = next[j];
        }
        else  
            j = next[j];
    }
}

int MString_Pos(const MString s, const MString sub, int Start)
{    
    if ((MString_Len(s) < 1) || (Start < 1) || (Start > MString_Len(s)))
        return -1;

    Start--;

    if (MString_Len(sub) < 1)
        return 1;

    if (MString_Len(s) - Start < MString_Len(sub))
        return -1;

    switch (MString_Len(sub))
    {
    case 1:
        return MString_Pos_1(s->pData + Start, MString_Len(s) - Start, sub->pData[0]) + Start;
    case 2:
        return MString_Pos_2(s->pData + Start, MString_Len(s) - Start, sub->pData) + Start;
    case 3:
        return MString_Pos_3(s->pData + Start, MString_Len(s) - Start, sub->pData) + Start;
    default:
        int *next = (int *)YM_malloc((MString_Len(sub) + 1)* sizeof(int));
        KMPGet_nextpos(sub->pData, next, MString_Len(sub));
        int r = StrIndex_KMP(s->pData + Start, sub->pData, MString_Len(s) - Start, MString_Len(sub), next);
        YM_free(next);
        return r + 1 + Start;
    }
}

/*
http://en.wikipedia.org/wiki/Levenshtein_distance

Levenshtein distance is a metric for measuring the amount of difference between two sequences 
(i.e., the so called edit distance). 

The Levenshtein distance between two strings is given by the minimum number of operations needed 
to transform one string into the other, where an operation is an insertion, deletion, or 
substitution of a single character.

int LevenshteinDistance(char s[1..m], char t[1..n])
   // d is a table with m+1 rows and n+1 columns
   declare int d[0..m, 0..n]
 
   for i from 0 to m
       d[i, 0] := i
   for j from 0 to n
       d[0, j] := j
 
   for i from 1 to m
       for j from 1 to n
       {
           if s[i] = t[j] then cost := 0
                          else cost := 1
           d[i, j] := minimum(
                                d[i-1, j] + 1,      // deletion
                                d[i, j-1] + 1,      // insertion
                                d[i-1, j-1] + cost  // substitution
                            )
       }
 
   return d[m, n]
   
Example:
	    k	i	t	t	e	n
	0	1	2	3	4	5	6
s	1	1	2	3	4	5	6
i	2	2	1	2	3	4	5
t	3	3	2	1	2	3	4
t	4	4	3	2	1	2	3
i	5	5	4	3	2	2	3
n	6	6	5	4	3	3	2
g	7	7	6	5	4	4	3

*/

MString MString_Delete(MString s, const int Len)
{
    MString r = MXRefObj_Create(MTagString, Type_MString);
    if (r->Capacity > 0)
    {
        MHeap_free(Heap_MString, r->pData);
        r->Capacity = 0;
    }
    r->Len = MString_Len(s) - Len;
    if (r->Len > 0)
    {
        r->Holder = s;
        r->pData = s->pData + Len;
        XRef_IncRef(s);
    }
    else
        r->Len = 0;
    
    return r;
}

MString MString_Copy(const MString s, const int Start, const int Len)
{
    MString r = MXRefObj_Create(MTagString, Type_MString);
    if (r->Capacity > 0)
    {
        MHeap_free(Heap_MString, r->pData);
        r->Capacity = 0;
    }
    r->Len = min(MString_Len(s) - Start + 1, Len);
    r->Holder = s;
    r->pData = s->pData + Start - 1;
    XRef_IncRef(s);
    return r;
}

MString MString_Join(const MString s, const MString cat)
{
    MString r = MXRefObj_Create(MTagString, Type_MString);
    r->Len = MString_Len(s) + MString_Len(cat);
    make_str_len(r, r->Len);
    memcpy(r->pData, s->pData, s->Len << 1);
    memcpy(r->pData + s->Len, cat->pData, cat->Len << 1);
    return r;
}

static void MString_CopyOnWrite(MString th, int cap = -1)
{
    MString holder = th->Holder;
    MChar *old = th->pData;
    th->Capacity = cap = max(cap, th->Len);
    th->pData = (MChar *)MHeap_malloc(Heap_MString, (th->Capacity) << 1);
    memcpy(th->pData, old, th->Len * sizeof(MChar));
    th->Holder = NULL;
    MString_Release(holder);
}

// s should be *unique*
MString MString_Cat(MString s, const MString cat)
{
    if (s->Holder) MString_CopyOnWrite(s, s->Len + cat->Len);
    if (s->Capacity < s->Len + cat->Len)
    {
        s->pData = (MChar *)MHeap_realloc(Heap_MString, s->pData, (s->Len + cat->Len) * sizeof(MChar));
        s->Capacity = s->Len + cat->Len;
    }
    memcpy(s->pData + s->Len, cat->pData, cat->Len << 1);
    s->Len += cat->Len;
    return s;
}

MString MString_CatChar(MString s, const MChar cat)
{
    ASSERT(s->Holder == NULL);  // this is a short-hand function
    if (s->Capacity < s->Len + 1)
    {
        s->Capacity = s->Len + 5;
        s->pData = (MChar *)MHeap_realloc(Heap_MString, s->pData, s->Capacity * sizeof(MChar));
    }
    s->pData[s->Len] = cat;
    s->Len += 1;
    return s;
}

MString MString_CatCN(MString s, const char *cz, const int n)
{
    int i;
    ASSERT(s->Holder == NULL);  // this is a short-hand function
    if (s->Capacity < s->Len + n)
    {
        s->Capacity = s->Len + n + 5;
        s->pData = (MChar *)MHeap_realloc(Heap_MString, s->pData, s->Capacity * sizeof(MChar));
    }
    for (i = 0; i < n; i++)
        s->pData[s->Len + i] = cz[i];
    s->Len += n;
    return s;
}

MInt MString_Compare(const MString s1, const MString s2)
{
    if (s1->Len > s2->Len)
        return 1;
    if (s1->Len < s2->Len)
        return -1;
    return memcmp(s1->pData, s2->pData, s1->Len * sizeof(s1->pData[0]));
}

MBool MString_SameQ(const MString s1, const MString s2)
{
    if (s1->Len != s2->Len)
        return false;
    return memcmp(s1->pData, s2->pData, s1->Len * sizeof(s1->pData[0])) == 0;
}

MString MString_Reverse(MString s)
{
    MInt i;
    for (i = 0; i < s->Len / 2; i++)
    {
        MChar t = s->pData[i];
        s->pData[i] = s->pData[s->Len - i - 1];
        s->pData[s->Len - i - 1] = t;
    }
    return s;
}

MString MString_SetLen(MString s1, const int Len)
{
    s1->Len = min(s1->Capacity, Len);
    return s1;
}

MInt MString_Dump(MString s, MChar *t, const int size)
{
    MInt r = min(size, MString_Len(s));
    memcpy(t, s->pData, size << 1);
    return r;
}

const MChar * MString_GetData(const MString s)
{
    return s->pData;
}

static MString MString_Dispose(MString s)
{
    if (s->Holder)
    {
        XRef_DecRef(s->Holder);
        s->Holder = NULL;
        s->pData = NULL;
        s->Capacity = 0;
        s->Len = 0;
    }
    else;

    return s;
}

static char *MString_DumpObj(MString str, char *s, const MInt maxlen)
{
    s[maxlen - 1] = '\0';
    MStringToASCII(str, (MByte *)s, maxlen - 2);
    return s;
}

f_YM_XRefObjInit f = NULL;
static MXRefObjMT MString_MT = {NULL, (f_YM_XRefObjDispose)MString_Dispose, NULL}; // (f_YM_XRefObjDump)MString_DumpObj

void MString_Init(void)
{
    Type_MString = MXRefObj_Register(&MString_MT, sizeof(MTagString), "MString");
    Heap_MString = MHeap_NewHeap("MString", 10000000);
    EmptyStr = MXRefObj_Create(MTagString, Type_MString);
    EmptyStr->Len = 0;
    EmptyStr->Capacity = 0;
    XRef_IncRef(EmptyStr);
}

void MString_DumpHeap(FILE *file)
{
    MHeap_Dump(Heap_MString, file);
}
