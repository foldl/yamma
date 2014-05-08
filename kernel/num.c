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

#include <math.h>

#include "num.h"
#include "mm.h"
#include "encoding.h"
#include "sys_sym.h"
#include "big.h"

enum MNumType
{
    ntBig,
    ntBigRational,
    ntMachine
};

struct MTagNum_impl
{
    MNumType Type;
    
    union
    {
        struct 
        {
            MBigNum Denominator;
            MBigNum Big;
        };        
        MReal   MachR;
    };
};

static MXRefObjType Type_MNum = NULL;
//static MHeap        Heap_MNum = NULL;

typedef MTagNum_impl *MNum_impl;

#define to_impl(v) ((MNum_impl)(v))
#define to_interf(v) ((MNum)(v))
#define rel_and_null(v) Num_Release(v); v = NULL;

#define Big_SameQSmall(b, v) (Big_IsInt(b) && ((b)->size == 1) && ((b)->buf[0] == labs(v)) && ((b)->bNeg == ((v) < 0)))

#define try_big_by_op(vr, vop, f) \
do                                \
{                                 \
	if (!MObj_IsUnique(vr))       \
    {                             \
        MBigNum t = vr;           \
        vr = Big_Uniquelize(vr);  \
        Big_Release(t);           \
    }                             \
    f(vr, vop);                   \
} while(false);

#define CONSTANT_INT_CACHE  3

#define Big_AbsBy(b) (b)->bNeg = false

#define release_objs(t)                    \
    do {                                   \
        switch (t1->Type)                  \
        {                                  \
        case ntBigRational:                \
            rel_and_null(t1->Denominator); \
        case ntBig:                        \
            rel_and_null(t1->Big);         \
        default:                           \
            ;                              \
        }                                  \
    } while (false)

static MNum s_NumIntCache[CONSTANT_INT_CACHE * 2 + 1] = {NULL};

MNum Num_Create(MString s)
{
    MNum_impl v = MXRefObj_Create(MTagNum_impl, Type_MNum);
    v->Big = Big_Create(s);
    return to_interf(v);
}

MNum Num_Create(MInt v)
{
    if ((v <= CONSTANT_INT_CACHE) && (v >= -CONSTANT_INT_CACHE))
    {
        XRef_IncRef(s_NumIntCache[v + CONSTANT_INT_CACHE]);
        return s_NumIntCache[v + CONSTANT_INT_CACHE];
    }
    else
        return Num_CreateNoCache(v);
}

MNum Num_CreateU(MUInt v)
{
    MNum_impl r = MXRefObj_Create(MTagNum_impl, Type_MNum);
    r->Big = Big_CreateU(v);
    return to_interf(r);
}

MNum Num_CreateNoCache(MInt v)
{
    MNum_impl r = MXRefObj_Create(MTagNum_impl, Type_MNum);
    r->Big = Big_Create(v);
    return to_interf(r);
}

MNum Num_Create(MReal v)
{
    MNum_impl t = MXRefObj_Create(MTagNum_impl, Type_MNum);
    t->Type = ntMachine;
    t->MachR = v;
    return to_interf(t);
}

static MNum Num_CreateBigX(MBigNum v)
{
    MNum_impl t = MXRefObj_Create(MTagNum_impl, Type_MNum);
    t->Type = ntBig;
    t->Big = v;
    return to_interf(t);
}

MNum Num_Create(MNum a)
{
    XRef_IncRef(a);
    return a;
}

MNum Num_NormalizeRational(MNum_impl a)
{
    if (a->Type != ntBigRational)
        return to_interf(a);

    if (Big_IsZero(a->Big))
    {
        rel_and_null(a->Denominator); 
        a->Type = ntBig;
    }
    else if (Big_IsZero(a->Denominator))
    {
        rel_and_null(a->Denominator); 
        Big_Release(a->Big);
        a->Big = Big_Create(0);
        a->Type = ntBig;
    }
    else if (Big_SameQSmall(a->Denominator, 1))
    {
        rel_and_null(a->Denominator); 
        a->Type = ntBig;
    }
    return to_interf(a);
}

static MNum Num_GCD(const MBigNum v1, const MBigNum v2);

MNum Num_CancelRational(MNum_impl r)
{
    if (r->Type != ntBigRational)
        return to_interf(r);

    MNum_impl gcd = to_impl(Num_GCD(r->Big, r->Denominator));
    if (!Num_SameQSmall(to_interf(gcd), 1))
    {
        MBigNum quot1 = Big_IntDiv(r->Big, gcd->Big);
        Num_Release(r->Big);
        r->Big = quot1;
        quot1 = Big_IntDiv(r->Denominator, gcd->Big);
        Num_Release(r->Denominator);
        r->Denominator = quot1;
    }
    Num_Release(gcd);
    return to_interf(r);
}

MNum Num_CreateRational(const MNum Numerator, const MNum Denominator)
{
    if (Num_IsInt(Numerator) && Num_IsInt(Denominator))
    {
        MNum_impl r = MXRefObj_Create(MTagNum_impl, Type_MNum);
        r->Type = ntBigRational;
        r->Big = to_impl(Numerator)->Big; XRef_IncRef(to_impl(Numerator)->Big);
        r->Denominator = to_impl(Denominator)->Big;   XRef_IncRef(to_impl(Denominator)->Big);
        Num_NormalizeRational(r);
        return to_interf(r);
    }
    else
        return Num_Divide(Numerator, Denominator);
}

static MNum Num_GCD(const MBigNum v1, const MBigNum v2);

MNum Num_CreateRationalX(const MNum Numerator, const MNum Denominator)
{
    if (Num_IsInt(Numerator) && Num_IsInt(Denominator))
    {
        MNum_impl r = MXRefObj_Create(MTagNum_impl, Type_MNum);
        r->Type = ntBigRational;
        r->Big = to_impl(Numerator)->Big; XRef_IncRef(to_impl(Numerator)->Big); Num_Release(Numerator);
        r->Denominator = to_impl(Denominator)->Big;   XRef_IncRef(to_impl(Denominator)->Big); Num_Release(Denominator);
        Num_NormalizeRational(r);
        return to_interf(r);
    }
    else
    {
        MNum r =  Num_Divide(Numerator, Denominator);
        Num_Release(Numerator);
        Num_Release(Denominator);
        return r;
    }
}

MNum Num_Uniquelize(MNum a)
{
    MNum_impl t = (MNum_impl)(a);
    MNum_impl v = MXRefObj_Create(MTagNum_impl, Type_MNum);
    v->Type = t->Type;
    v->MachR = t->MachR;
    if (t->Type == ntBig)
        v->Big = Big_Uniquelize(t->Big);
    else if (t->Type == ntBigRational)
    {
        v->Big = Big_Uniquelize(t->Big);
        v->Denominator = Big_Uniquelize(t->Denominator);
    }
    return to_interf(v);
}

MNum Num_UniquelizeIntPart(MNum a)
{
    MNum_impl t = (MNum_impl)(a);
    MNum_impl r = MXRefObj_Create(MTagNum_impl, Type_MNum);
    r->Type = t->Type;
    switch (t->Type)
    {
    case ntBig:
        r->Big = Big_UniquelizeIntPart(t->Big);
        break;
    case ntBigRational:
        r->Big = Big_IntDiv(t->Big, t->Denominator);
        r->Type = ntBig;
        break;
    default:
        r->MachR = (MInt)(t->MachR);
    }
    return to_interf(r);
}

MNum Num_UniquelizeFractPart(MNum a)
{
    MNum_impl t = (MNum_impl)(a);
    MNum_impl r = MXRefObj_Create(MTagNum_impl, Type_MNum);
    MBigNum temp;

    r->Type = t->Type;
    switch (t->Type)
    {
    case ntBig:
        r->Big = Big_UniquelizeFractPart(t->Big);
        break;
    case ntBigRational:
        temp = Big_Divide(t->Big, t->Denominator);
        r->Big = Big_UniquelizeFractPart(temp);
        Big_Release(temp);
        r->Type = ntBig;
        break;
    default:
        r->MachR = t->MachR - (MInt)(t->MachR);
    }
    return to_interf(r);
}

MNum Num_Round(MNum a)
{
    MNum_impl t = (MNum_impl)(a);
    MNum_impl r = MXRefObj_Create(MTagNum_impl, Type_MNum);
    MBigNum fract;
    
    r->Type = ntBig;
    switch (t->Type)
    {
    case ntBig:
        {
            MBigNum one = Big_Create(1);
            r->Big = Big_UniquelizeIntPart(t->Big);
            fract = Big_UniquelizeFractPart(t->Big);
            one = Big_Create(1);
            Big_AbsBy(fract); 
            Big_ShiftLeftBy(fract, 1);
            // Big_Print(fract);
            if (Big_GTE(fract, one))
                Big_AddBy(r->Big, t->Big->bNeg ? -1 : 1);
            Big_Release(one);
            Big_Release(fract);
        }
        break;
    case ntBigRational:
        r->Big = Big_IntDivMod(t->Big, t->Denominator, &fract);
        Big_AbsBy(fract);
        Big_ShiftLeftBy(fract, 1);
        if (Big_GTE(fract, t->Denominator))
            Big_AddBy(r->Big, t->Big->bNeg ? -1 : 1);
        Big_Release(fract);
        break;
    default:
        r->Big = Big_Create(floor(t->MachR + 0.5));
    }
    
    return to_interf(r);
}

MNum Num_SetFrom(MNum a, const MNum b)
{
    MNum_impl t1 = (MNum_impl)(a);
    const MNum_impl t2 = (const MNum_impl)(b);

    if (t1->Type != t2->Type)
        release_objs(t1);
    else;

    t1->Type = t2->Type;
    switch (t2->Type)
    {
    case ntBigRational:
        if (t2->Denominator != NULL)
            Big_SetFrom(t1->Denominator, t2->Denominator);
        else
            t1->Denominator = Big_Create(t2->Denominator);
    case ntBig:
        if (t2->Big != NULL)
            Big_SetFrom(t1->Big, t2->Big);
        else
            t1->Big = Big_Create(t2->Big);
        break;
    default:
        t1->MachR = t2->MachR;
    }

    return a;
}

inline MReal   Num_ToIEEE754(MNum a)
{
    MNum_impl t = (MNum_impl)(a);
    switch (t->Type)
    {
    case ntBigRational:
        return Big_ToIEEE754(t->Big) / Big_ToIEEE754(t->Denominator);
    case ntBig:
        return Big_ToIEEE754(t->Big);
    default:
        return t->MachR;
    }
}

MBool   Num_IsMachineInt(MNum a)
{
    MNum_impl t = (MNum_impl)(a);
    switch (t->Type)
    {
    case ntBig:
        return Big_IsMachineInt(t->Big);
    default:
        return false;
    }
}

MInt    Num_ToInt(MNum a)
{
    MNum_impl t = (MNum_impl)(a);
    MBigNum temp = NULL;
    MInt r;
    switch (t->Type)
    {
    case ntBigRational:
        temp = Big_IntDiv(t->Big, t->Denominator);
        r = Big_ToInt(temp);
        Big_Release(temp);
        return  r;
    case ntBig:
        return Big_ToInt(t->Big);
    default:
        return (MInt)(t->MachR);
    }
}

MString Num_ToString(MNum a, const MInt Base)
{
    MNum_impl t = (MNum_impl)(a);
#define TEMP_LEN    200
    char tempC[TEMP_LEN + 1];
    int pos;
    MString s;
    MString temp;

    ASSERT(a);

    switch (t->Type)
    {
    case ntBigRational:
        s = MString_NewWithCapacity(TEMP_LEN);
        MString_Cat(s, sym_Rational->Name);
        MString_CatChar(s, '[');

        temp = Big_ToString(t->Big, Base);
        MString_Cat(s, temp);
        MString_Release(temp);

        MString_CatChar(s, ','); //MString_CatChar(s, ' ');

        temp = Big_ToString(t->Denominator, Base);
        MString_Cat(s, temp);
        MString_Release(temp);

        MString_CatChar(s, ']');

        return  s;
    case ntBig:
        return Big_ToString(t->Big, Base);
    default:
        pos = snprintf(tempC, TEMP_LEN, "%f", t->MachR);
        while (pos > 0)
        {
            if (tempC[pos - 1] == '0')
                pos--;
            else
                break;
        }
        return MString_NewCN(tempC, pos);
    }
}

static MBigNum Num_ToBig(const MNum_impl a, const MNum_impl ref)
{
    ASSERT(a->Type != ntMachine);
    if (a->Type == ntBig)
    {
        XRef_IncRef(a->Big);
        return a->Big;
    }
    else
        return Big_Divide(a->Big, a->Denominator, 
                ref && (ref->Type == ntBig) ? (MInt)(ceil(Big_Accuracy(ref->Big))) : -1);
}

MNum Num_Add(const MNum a, const MNum b)
{
    const MNum_impl t1 = (const MNum_impl)(a);
    const MNum_impl t2 = (const MNum_impl)(b);
    MNum_impl r = MXRefObj_Create(MTagNum_impl, Type_MNum);

    if ((t1->Type == ntMachine) || (t2->Type == ntMachine))
    {
        r->Type = ntMachine;
        r->MachR = Num_ToIEEE754(a) + Num_ToIEEE754(b);
    }
    else if ((t1->Type == ntBigRational) && (t2->Type == ntBigRational))
    {
        MNum_impl gcd = to_impl(Num_GCD(t1->Denominator, t2->Denominator));
        MNum_impl quot1 = to_impl(Num_CreateBigX(Big_IntDiv(t1->Denominator, gcd->Big)));
        MNum_impl quot2 = to_impl(Num_CreateBigX(Big_IntDiv(t2->Denominator, gcd->Big)));
        
        MBigNum t1Norm = Big_Multiply(t1->Big, quot2->Big);
        MBigNum t2Norm = Big_Multiply(t2->Big, quot1->Big);

        r->Big = Big_Add(t1Norm, t2Norm);
        r->Denominator = Big_Multiply(t1->Denominator, quot2->Big);

        Num_Release(gcd);
        Num_Release(quot1);
        Num_Release(quot2);
        Big_Release(t1Norm);
        Big_Release(t2Norm);

        r->Type = ntBigRational;
        
        Num_CancelRational(r);
        Num_NormalizeRational(r);
    }
    else if ((t1->Type == ntBigRational) && Num_IsInt(b))
    {
        r->Type = ntBigRational;
        r->Denominator = Big_Create(t1->Denominator);
        r->Big = Big_Multiply(t1->Denominator, t2->Big);
        Big_AddBy(r->Big, t1->Big);
    }
    else if ((t2->Type == ntBigRational) && Num_IsInt(a))
    {
        r->Type = ntBigRational;
        r->Denominator = Big_Create(t2->Denominator);
        r->Big = Big_Multiply(t2->Denominator, t1->Big);
        Big_AddBy(r->Big, t2->Big);
    }   
    else
    {
        MBigNum tt1 = Num_ToBig(t1, t2);
        MBigNum tt2 = Num_ToBig(t2, t1);
        r->Big = Big_Add(tt1, tt2);
        Big_Release(tt1);
        Big_Release(tt2);
    }
    return to_interf(r);
}

MNum Num_AddBy(MNum a, const MNum b)
{
    MNum_impl t1 = (const MNum_impl)(a);
    const MNum_impl t2 = (const MNum_impl)(b);

    ASSERT_REF_CNT(a, 0);

    if ((t1->Type == ntMachine) || (t2->Type == ntMachine))
    {
        MReal tt = Num_ToIEEE754(a) + Num_ToIEEE754(b);
        release_objs(t1);
        t1->MachR = tt;
        t1->Type = ntMachine;
    }
    else if ((t1->Type == ntBigRational) && (t2->Type == ntBigRational))
    {
        MNum_impl gcd = to_impl(Num_GCD(t1->Denominator, t2->Denominator));
        MNum_impl quot1 = to_impl(Num_CreateBigX(Big_IntDiv(t1->Denominator, gcd->Big)));
        MNum_impl quot2 = to_impl(Num_CreateBigX(Big_IntDiv(t2->Denominator, gcd->Big)));

        MBigNum t1Norm = Big_Multiply(t1->Big, quot2->Big);
        MBigNum t2Norm = Big_Multiply(t2->Big, quot1->Big);

        Num_Release(t1->Big);
        Num_Release(t1->Denominator);
        t1->Big = Big_Add(t1Norm, t2Norm);
        t1->Denominator = Big_Multiply(t2->Denominator, quot1->Big);

        Num_Release(gcd);
        Num_Release(quot1);
        Num_Release(quot2);
        Big_Release(t1Norm);
        Big_Release(t2Norm);

        Num_CancelRational(t1);
        Num_NormalizeRational(t1);
    }
    else if ((t1->Type == ntBigRational) && Num_IsInt(b))
    {
        MBigNum temp = Big_Multiply(t1->Denominator, t2->Big);
        try_big_by_op(t1->Big, temp, Big_AddBy);
        Big_Release(temp);

        Num_CancelRational(t1);
        Num_NormalizeRational(t1);
    }
    else if ((t2->Type == ntBigRational) && Num_IsInt(a))
    {
        t1->Type = ntBigRational;
        t1->Denominator = Big_Create(t2->Denominator);
        try_big_by_op(t1->Big, t2->Denominator, Big_MultiplyBy);
        try_big_by_op(t1->Big, t2->Big, Big_AddBy);

        Num_CancelRational(t1);
        Num_NormalizeRational(t1);
    }   
    else
    {
        MBigNum tt1 = Num_ToBig(t1, t2);
        MBigNum tt2 = Num_ToBig(t2, t1);
        
        release_objs(t1);

        t1->Type = ntBig;
        Big_AddBy(tt1, tt2);
        t1->Big = tt1;
        Big_Release(tt2);
    }
    return a;
}

MNum Num_AddBy(MNum a, const MSWord b)
{
    MNum_impl t1 = (const MNum_impl)(a);

    ASSERT_REF_CNT(a, 0);

    switch (t1->Type)
    {
    case ntBig:
        Big_AddBy(t1->Big, b);
        break;
    case ntBigRational:
        {
            MBigNum t = Big_Create(b);
            Big_MultiplyBy(t, t1->Denominator);
            Big_AddBy(t1->Big, t);
            Big_Release(t);
        }
        break;
    default:
       t1->MachR += b;
    }
    return a;
}

MNum Num_Subtract(const MNum a, const MNum b)
{
    MNum r;
    Num_NegBy(a);
    r = Num_Add(a, b);
    Num_NegBy(a);
    Num_NegBy(r);
    return to_interf(r);
}

MNum Num_SubtractBy(MNum a, const MNum b)
{
    ASSERT_REF_CNT(a, 0);

    Num_NegBy(a);
    Num_AddBy(a, b);
    Num_NegBy(a);
    return a;
}

MNum Num_Divide(const MNum a, const MNum b, const MInt TargetPrecision)
{
    const MNum_impl t1 = (const MNum_impl)(a);
    const MNum_impl t2 = (const MNum_impl)(b);
    MNum_impl r = MXRefObj_Create(MTagNum_impl, Type_MNum);

    if ((t1->Type == ntMachine) || (t2->Type == ntMachine))
    {
        r->Type = ntMachine;
        r->MachR = Num_ToIEEE754(a) / Num_ToIEEE754(b);
    }
    else if ((t1->Type == ntBigRational) && (t2->Type == ntBigRational))
    {
        r->Type = ntBigRational;
        r->Big = Big_Multiply(t1->Big, t2->Denominator);
        r->Denominator = Big_Multiply(t2->Big, t1->Denominator);
        Num_CancelRational(r);
        Num_NormalizeRational(r);
    }
    else if ((t1->Type == ntBigRational) && Num_IsInt(b))
    {
        r->Type = ntBigRational;
        r->Big = Big_Create(t1->Big);
        r->Denominator = Big_Multiply(t1->Denominator, t2->Big);
        Num_CancelRational(r);
        Num_NormalizeRational(r);
    }
    else if ((t2->Type == ntBigRational) && Num_IsInt(a))
    {
        r->Type = ntBigRational;
        r->Big = Big_Multiply(t2->Denominator, t1->Big);
        r->Denominator = Big_Create(t2->Big);
        Num_CancelRational(r);
        Num_NormalizeRational(r);
    }   
    else
    {
        MBigNum tt1 = Num_ToBig(t1, t2);
        MBigNum tt2 = Num_ToBig(t2, t1);
        r->Big = Big_Divide(tt1, tt2, TargetPrecision);
        Big_Release(tt1);
        Big_Release(tt2);
    }

    return to_interf(r);
}

MNum Num_DivideBy(MNum a, const MNum b, const MInt TargetPrecision)
{
    MNum_impl t1 = (const MNum_impl)(a);
    const MNum_impl t2 = (const MNum_impl)(b);

    ASSERT_REF_CNT(a, 0);
    
    if ((t1->Type == ntMachine) || (t2->Type == ntMachine))
    {
        MReal tt = Num_ToIEEE754(a) / Num_ToIEEE754(b);
        release_objs(t1);
        t1->Type = ntMachine;
        t1->MachR = tt;
    }
    else if ((t1->Type == ntBigRational) && (t2->Type == ntBigRational))
    {
        try_big_by_op(t1->Big, t2->Denominator, Big_MultiplyBy);
        try_big_by_op(t1->Denominator, t2->Big, Big_MultiplyBy);
        Num_CancelRational(t1);
        Num_NormalizeRational(t1);
    }
    else if ((t1->Type == ntBigRational) && Num_IsInt(b))
    {
        try_big_by_op(t1->Denominator, t2->Big, Big_MultiplyBy);
        Num_CancelRational(t1);
        Num_NormalizeRational(t1);
    }
    else if ((t2->Type == ntBigRational) && Num_IsInt(a))
    {
        t1->Type = ntBigRational;
        try_big_by_op(t1->Big, t2->Denominator, Big_MultiplyBy);
        t1->Denominator = Big_Create(t2->Big);
        Num_CancelRational(t1);
        Num_NormalizeRational(t1);
    }   
    else
    {
        MBigNum tt1 = Num_ToBig(t1, t2);
        MBigNum tt2 = Num_ToBig(t2, t1);
        
        release_objs(t1);

        t1->Type = ntBig;
        Big_DivideBy(tt1, tt2);
        t1->Big = tt1;
        Big_Release(tt2);
    }

    return a;
}

MNum Num_Multiply(const MNum a, const MNum b)
{
    const MNum_impl t1 = (const MNum_impl)(a);
    const MNum_impl t2 = (const MNum_impl)(b);
    MNum_impl r = MXRefObj_Create(MTagNum_impl, Type_MNum);

    if ((t1->Type == ntMachine) || (t2->Type == ntMachine))
    {
        r->Type = ntMachine;
        r->MachR = Num_ToIEEE754(a) * Num_ToIEEE754(b);
    }
    else if ((t1->Type == ntBigRational) && (t2->Type == ntBigRational))
    {
        r->Type = ntBigRational;
        r->Big = Big_Multiply(t1->Big, t2->Big);
        r->Denominator = Big_Multiply(t1->Denominator, t2->Denominator);
        Num_CancelRational(r);
        Num_NormalizeRational(r);
    }
    else if ((t1->Type == ntBigRational) && Num_IsInt(b))
    {
        r->Type = ntBigRational;
        r->Big = Big_Multiply(t1->Big, t2->Big);
        r->Denominator = Big_Create(t1->Denominator);
        Num_CancelRational(r);
        Num_NormalizeRational(r);
    }
    else if ((t2->Type == ntBigRational) && Num_IsInt(a))
    {
        r->Type = ntBigRational;
        r->Big = Big_Multiply(t1->Big, t2->Big);
        r->Denominator = Big_Create(t2->Denominator);
        Num_CancelRational(r);
        Num_NormalizeRational(r);
    }   
    else
    {
        MBigNum tt1 = Num_ToBig(t1, t2);
        MBigNum tt2 = Num_ToBig(t2, t1);
        r->Big = Big_Multiply(tt1, tt2);
        Big_Release(tt1);
        Big_Release(tt2);
    }

    return to_interf(r);
}

MNum Num_MultiplyBy(MNum a, const MNum b)
{
    MNum_impl t1 = (const MNum_impl)(a);
    const MNum_impl t2 = (const MNum_impl)(b);

    ASSERT_REF_CNT(a, 0);

    if ((t1->Type == ntMachine) || (t2->Type == ntMachine))
    {
        MReal tt = Num_ToIEEE754(a) * Num_ToIEEE754(b);
        release_objs(t1);
        t1->Type = ntMachine;
        t1->MachR = tt;
    }
    else if ((t1->Type == ntBigRational) && (t2->Type == ntBigRational))
    {
        try_big_by_op(t1->Big, t2->Big, Big_MultiplyBy);
        try_big_by_op(t1->Denominator, t2->Denominator, Big_MultiplyBy);
        Num_CancelRational(t1);
        Num_NormalizeRational(t1);
    }
    else if ((t1->Type == ntBigRational) && Num_IsInt(b))
    {
        try_big_by_op(t1->Big, t2->Big, Big_MultiplyBy);
        Num_CancelRational(t1);
        Num_NormalizeRational(t1);
    }
    else if ((t2->Type == ntBigRational) && Num_IsInt(a))
    {
        t1->Type = ntBigRational;
        try_big_by_op(t1->Big, t2->Big, Big_MultiplyBy);
        t1->Denominator = Big_Create(t2->Denominator);
        Num_CancelRational(t1);
        Num_NormalizeRational(t1);
    }   
    else
    {
        MBigNum tt1 = Num_ToBig(t1, t2);
        MBigNum tt2 = Num_ToBig(t2, t1);
        
        release_objs(t1);

        t1->Type = ntBig;
        Big_MultiplyBy(tt1, tt2);
        t1->Big = tt1;
        Big_Release(tt2);
    }

    return a;
}

MNum Num_IntDivMod(const MNum a, const MNum b, MNum *premainer)
{
    const MNum_impl t1 = (const MNum_impl)(a);
    const MNum_impl t2 = (const MNum_impl)(b);

    ASSERT(Num_IsInt(a) && Num_IsInt(b));

    *premainer = to_interf(MXRefObj_Create(MTagNum_impl, Type_MNum));
    return Num_CreateBigX(Big_IntDivMod(t1->Big, t2->Big, &((*(MNum_impl *)(premainer))->Big)));
}

MNum Num_IntDiv(const MNum a, const MNum b)
{
    const MNum_impl t1 = (const MNum_impl)(a);
    const MNum_impl t2 = (const MNum_impl)(b);

    ASSERT(Num_IsInt(a) && Num_IsInt(b));
    return Num_CreateBigX(Big_IntDiv(t1->Big, t2->Big));
}

MNum Num_IntMod(const MNum a, const MNum b)
{
    const MNum_impl t1 = (const MNum_impl)(a);
    const MNum_impl t2 = (const MNum_impl)(b);
    
    ASSERT(Num_IsInt(a) && Num_IsInt(b));
    return Num_CreateBigX(Big_IntMod(t1->Big, t2->Big));
}

MNum Num_DivMod(const MNum a, const MNum b, MNum *premainer)
{
    const MNum_impl t1 = (const MNum_impl)(a);
    const MNum_impl t2 = (const MNum_impl)(b);

    if ((t1->Type == ntMachine) || (t2->Type == ntMachine))
    {
        MReal r1 = Num_ToIEEE754(a);
        MReal r2 = Num_ToIEEE754(b);
        MReal temp = r1 / r2;
        MInt I = (MInt)(temp);
        MNum r = Num_Create(I);
        *premainer = Num_Create(r1 - I * r2);
        return to_interf(r);
    }
    else
    {
        MBigNum tt1 = Num_ToBig(t1, t2);
        MBigNum tt2 = Num_ToBig(t2, t1);
        MNum r;
        *premainer = to_interf(MXRefObj_Create(MTagNum_impl, Type_MNum));
        ((MNum_impl)*premainer)->Big = NULL;
        r = Num_CreateBigX(Big_DivMod(t1->Big, t2->Big, &((MNum_impl)*premainer)->Big));
        Big_Release(tt1);
        Big_Release(tt2);
        return to_interf(r);
    }
}

MNum Num_Div(const MNum a, const MNum b)
{
    const MNum_impl t1 = (const MNum_impl)(a);
    const MNum_impl t2 = (const MNum_impl)(b);

    if ((t1->Type == ntMachine) || (t2->Type == ntMachine))
    {
        MReal r1 = Num_ToIEEE754(a);
        MReal r2 = Num_ToIEEE754(b);
        MReal temp = r1 / r2;
        MInt I = (MInt)(temp);
        MNum r = Num_Create(I);
        return to_interf(r);
    }
    else
    {
        MBigNum tt1 = Num_ToBig(t1, t2);
        MBigNum tt2 = Num_ToBig(t2, t1);
        MNum r;
        r = Num_CreateBigX(Big_Div(t1->Big, t2->Big));
        Big_Release(tt1);
        Big_Release(tt2);
        return to_interf(r);
    }
}

MNum Num_Mod(const MNum a, const MNum b)
{
    const MNum_impl t1 = (const MNum_impl)(a);
    const MNum_impl t2 = (const MNum_impl)(b);

    if ((t1->Type == ntMachine) || (t2->Type == ntMachine))
    {
        MReal r1 = Num_ToIEEE754(a);
        MReal r2 = Num_ToIEEE754(b);
        MReal temp = r1 / r2;
        MInt I = (MInt)(temp);
        return Num_Create(r1 - I * r2);
    }
    else
    {
        MBigNum tt1 = Num_ToBig(t1, t2);
        MBigNum tt2 = Num_ToBig(t2, t1);
        MNum r;
        r = Num_CreateBigX(Big_Mod(t1->Big, t2->Big));
        Big_Release(tt1);
        Big_Release(tt2);
        return to_interf(r);
    }
}

MNum Num_ShiftLeft(const MNum a, const MInt off)
{
    ASSERT(((const MNum_impl)(a))->Type == ntBig);
    MNum r = Num_Uniquelize(a);
    return Num_ShiftLeftBy(r, off);
}

MNum Num_ShiftLeftBy(MNum a, MInt off)
{
    const MNum_impl t = (const MNum_impl)(a);
    ASSERT(ntBig == t->Type);
    Big_ShiftLeftBy(t->Big, off);
    return a;
}

MBool Num_IsZero(MNum a)
{
    const MNum_impl t = (const MNum_impl)(a);
    if (t->Type == ntBig)
        return Big_IsZero(t->Big);
    else if (t->Type == ntMachine)
        return t->MachR == 0.0;
    else
        return false;
}

MBool Num_IsInt(MNum a)
{
    const MNum_impl t = (const MNum_impl)(a);
    return t->Type == ntBig ? Big_IsInt(t->Big) : false;
}

MBool Num_IsReal(MNum a)
{
    const MNum_impl t = (const MNum_impl)(a);
    return t->Type == ntBig ? !Big_IsInt(t->Big) : true;
}

MBool Num_IsMachineReal(MNum a)
{
    const MNum_impl t = (const MNum_impl)(a);
    return t->Type == ntMachine;
}

MBool Num_IsRational(MNum a)
{
    const MNum_impl t = (const MNum_impl)(a);
    return t->Type == ntBigRational;
}

MBool Num_IsNeg(MNum a)
{
    const MNum_impl t = (const MNum_impl)(a);
    switch (t->Type)
    {
    case ntBig:
    case ntBigRational:
        return (t->Big->bNeg);
    case ntMachine:
        return t->MachR < 0.0;
    }
    
    return false;
}

MBool Num_EvenQ(MNum a)
{
    const MNum_impl t = (const MNum_impl)(a);
    if (t->Type == ntBig)
        return Big_EvenQ(t->Big);
    else
        return false;
}

MBool   Num_LT (const MNum a, const MNum b)
{
    return Num_Cmp(a, b) < 0;
}

MBool   Num_LTE(const MNum a, const MNum b)
{
    return Num_Cmp(a, b) <= 0;
}

MBool   Num_E  (const MNum a, const MNum b)
{
    return Num_Cmp(a, b) == 0;
}

MBool   Num_GTE(const MNum a, const MNum b)
{
    return Num_Cmp(a, b) >= 0;
}

MBool   Num_GT (const MNum a, const MNum b)
{
    return Num_Cmp(a, b) > 0;
}

MBool   Num_NE (const MNum a, const MNum b)
{
    return Num_Cmp(a, b) != 0;
}

MInt    Num_Cmp(const MNum a, const MNum b)
{
    const MNum_impl t1 = (const MNum_impl)(a);
    const MNum_impl t2 = (const MNum_impl)(b);

    if ((t1->Type == ntMachine) || (t2->Type == ntMachine))
    {
        MReal t = Num_ToIEEE754(a) - Num_ToIEEE754(b);
        if (t > 0.0)
            return 1;
        else if (t < 0.0)
            return -1;
        else
            return 0;
    }
    else if ((t1->Type == ntBigRational) && (t2->Type == ntBigRational))
    {
        MBigNum tt1 = Big_Multiply(t1->Big, t2->Denominator);
        MBigNum tt2 = Big_Multiply(t2->Big, t1->Denominator);
        MInt r = Big_Cmp(tt1, tt2);
        Big_Release(tt1);
        Big_Release(tt2);
        return r;
    }
    else if ((t1->Type == ntBigRational) && Num_IsInt(b))
    {
        MBigNum tt2 = Big_Multiply(t2->Big, t1->Denominator);
        MInt r = Big_Cmp(t1->Big, tt2);
        Big_Release(tt2);
        return r;
    }
    else if ((t2->Type == ntBigRational) && Num_IsInt(a))
    {
        MBigNum tt1 = Big_Multiply(t1->Big, t2->Denominator);
        MInt r = Big_Cmp(tt1, t2->Big);
        Big_Release(tt1);
        return r;
    }   
    else
    {
        MBigNum tt1 = Num_ToBig(t1, t2);
        MBigNum tt2 = Num_ToBig(t2, t1);
        MInt r = Big_Cmp(tt1, tt2);
        Big_Release(tt1);
        Big_Release(tt2);
        return r;
    }
}

MInt    Num_AbsCmp(const MNum a, const MNum b)
{
    const MNum_impl t1 = (const MNum_impl)(a);
    const MNum_impl t2 = (const MNum_impl)(b);

    if ((t1->Type == ntMachine) || (t2->Type == ntMachine))
    {
        MReal t = fabs(Num_ToIEEE754(a)) - fabs(Num_ToIEEE754(b));
        if (t > 0.0)
            return 1;
        else if (t < 0.0)
            return -1;
        else
            return 0;
    }
    else if ((t1->Type == ntBigRational) && (t2->Type == ntBigRational))
    {
        MBigNum tt1 = Big_Multiply(t1->Big, t2->Denominator);
        MBigNum tt2 = Big_Multiply(t2->Big, t1->Denominator);
        MInt r = Big_AbsCmp(tt1, tt2);
        Big_Release(tt1);
        Big_Release(tt2);
        return r;
    }
    else if ((t1->Type == ntBigRational) && Num_IsInt(b))
    {
        MBigNum tt2 = Big_Multiply(t2->Big, t1->Denominator);
        MInt r = Big_AbsCmp(t1->Big, tt2);
        Big_Release(tt2);
        return r;
    }
    else if ((t2->Type == ntBigRational) && Num_IsInt(a))
    {
        MBigNum tt1 = Big_Multiply(t1->Big, t2->Denominator);
        MInt r = Big_AbsCmp(tt1, t2->Big);
        Big_Release(tt1);
        return r;
    }   
    else
    {
        MBigNum tt1 = Num_ToBig(t1, t2);
        MBigNum tt2 = Num_ToBig(t2, t1);
        MInt r = Big_AbsCmp(tt1, tt2);
        Big_Release(tt1);
        Big_Release(tt2);
        return r;
    }
}

MBool   Num_SameQ(const MNum a, const MNum b)
{
    const MNum_impl t1 = (const MNum_impl)(a);
    const MNum_impl t2 = (const MNum_impl)(b);
    if (t1->Type != t2->Type)
        return false;

    switch (t1->Type)
    {
    case ntBig:
        return Big_SameQ(t1->Big, t2->Big);
    case ntBigRational:
        return Big_SameQ(t1->Big, t2->Big) && Big_SameQ(t1->Denominator, t2->Denominator);
    case ntMachine:
        return Num_ToIEEE754(a) == Num_ToIEEE754(b);
    }

    ASSERT("Num_SameQ" == NULL);
    return false;
}

MBool   Num_SameQSmall(const MNum a, const short v)
{
    const MNum_impl t = (const MNum_impl)(a);
    if (v != 0)        
        return t->Type == ntBig ? 
                Big_SameQSmall(t->Big, v)
                : false;
    else
        return Num_IsZero(a);
}

MBool   Num_UnsameQ(const MNum a, const MNum b)
{
    const MNum_impl t1 = (const MNum_impl)(a);
    const MNum_impl t2 = (const MNum_impl)(b);

    if (t1->Type != t2->Type)
        return true;

    switch (t1->Type)
    {
    case ntBig:
        return Big_UnsameQ(t1->Big, t2->Big);
    case ntBigRational:
        return Big_UnsameQ(t1->Big, t2->Big) || Big_UnsameQ(t1->Denominator, t2->Denominator);
    case ntMachine:
        return Num_ToIEEE754(a) != Num_ToIEEE754(b);
    }
    
    ASSERT("Num_UnsameQ" == NULL);
    return false;
}

MNum Num_Abs(MNum a)
{
    const MNum_impl t1 = (const MNum_impl)(a);
    MNum_impl r = MXRefObj_Create(MTagNum_impl, Type_MNum);
    r->Type = t1->Type;
    if (t1->Type == ntBig)
    {
        r->Big = Big_Uniquelize(t1->Big);
        r->Big->bNeg = false;
    }
    else if (t1->Type == ntBigRational)
    {
        r->Big = Big_Uniquelize(t1->Big);
        r->Big->bNeg = false;
        r->Denominator = Big_Create(t1->Denominator);
    }
    else
        r->MachR = fabs(t1->MachR);
    return to_interf(r);
}

MNum Num_AbsBy(MNum a)
{
    const MNum_impl t1 = (const MNum_impl)(a);

    switch (t1->Type)
    {
    case ntBig:
    case ntBigRational:
        if (!MObj_IsUnique(t1->Big))
        {
            MBigNum t = Big_Uniquelize(t1->Big);
            Big_Release(t1->Big);
            t1->Big = t;
        }
        t1->Big->bNeg = false;
        break;
    case ntMachine:
        t1->MachR = fabs(t1->MachR);
    }

    return a;
}

MNum Num_Neg(MNum a)
{
    const MNum_impl t1 = (const MNum_impl)(a);
    MNum_impl r = MXRefObj_Create(MTagNum_impl, Type_MNum);
    r->Type = t1->Type;

    if (t1->Type == ntBig)
    {
        r->Big = Big_Uniquelize(t1->Big);
        r->Big->bNeg = !t1->Big->bNeg;
    }
    else if (t1->Type == ntBigRational)
    {
        r->Big = Big_Uniquelize(t1->Big);
        r->Big->bNeg = t1->Big->size > 0 ? !t1->Big->bNeg : false;
        r->Denominator = Big_Create(t1->Denominator);
    }
    else
        r->MachR = -(t1->MachR);
    return to_interf(r);
}

MNum Num_NegBy(MNum a)
{
    const MNum_impl t1 = (const MNum_impl)(a);

    switch (t1->Type)
    {
    case ntBig:
    case ntBigRational:
        if (!MObj_IsUnique(t1->Big))
        {
            MBigNum t = Big_Uniquelize(t1->Big);
            Big_Release(t1->Big);
            t1->Big = t;
        }
        t1->Big->bNeg = t1->Big->size > 0 ? !t1->Big->bNeg : false;
        break;
    case ntMachine:
        t1->MachR = -(t1->MachR);
    }

    return a;
}

MReal   Num_Accuracy(MNum a)
{
    const MNum_impl t = (const MNum_impl)(a);
    switch (t->Type)
    {
    case ntBig:
        return Big_Accuracy(t->Big);
    case ntBigRational:
        return -1.0;
    default:
        return MACHINE_ACCURACY;
    }
}

MReal   Num_Precision(MNum a)
{
    const MNum_impl t = (const MNum_impl)(a);
    switch (t->Type)
    {
    case ntBig:
        return Big_Precision(t->Big);
    case ntBigRational:
        return -1.0;
    default:
        MInt expo = (MInt)(*(((MWord *)&t->MachR) + sizeof(MReal) - 1) & 0x7FF) - 1023;
        return pow(2., expo - sizeof(MReal) - 12);
    }
}

MNum Num_Power(MNum a, MNum b)
{
    const MNum_impl t1 = (const MNum_impl)(a);
    MNum_impl t2 = (MNum_impl)(b);

    if (Num_IsInt(b) && (t1->Type == ntBig || t1->Type == ntBigRational))
    {
        switch (t1->Type)
        {
        case ntBig:
            if (!t2->Big->bNeg)
                return Num_CreateBigX(Big_Power(t1->Big, t2->Big));
            else
            {
                MNum r;
                t2->Big->bNeg = false;
                r = Num_CreateRationalX(
                       Num_Create(1), 
                       Num_CreateBigX(Big_Power(t1->Big, t2->Big)));
                t2->Big->bNeg = true;
                Num_NormalizeRational(to_impl(r));
                return r;
            }
            break;
        case ntBigRational:
            if (!t2->Big->bNeg)
            {
                MNum r = Num_CreateRationalX(
                       Num_CreateBigX(Big_Power(t1->Big, t2->Big)), 
                       Num_CreateBigX(Big_Power(t1->Denominator, t2->Big)));
                Num_NormalizeRational(to_impl(r));
                return r;
            }
            else
            {
                MNum r;
                t2->Big->bNeg = false;
                r = Num_CreateRationalX(
                       Num_CreateBigX(Big_Power(t1->Denominator, t2->Big)),
                       Num_CreateBigX(Big_Power(t1->Big, t2->Big)));
                t2->Big->bNeg = true;
                Num_NormalizeRational(to_impl(r));
                return r;
            }
            break;
        default:
            ;
        }
        
        ASSERT("Num_Power: should not goto here!" == NULL);
        return NULL;
    }
    else
        return Num_Create(pow(Num_ToIEEE754(a), Num_ToIEEE754(b)));
}

MNum Num_Sqrt(MNum a, MInt precision)
{
    const MNum_impl t1 = (const MNum_impl)(a);
    if (t1->Type == ntBig)
        return Num_CreateBigX(Big_Sqrt(t1->Big, precision));
    else
        return Num_Create(sqrt(Num_ToIEEE754(a)));
}

static MNum Num_GCD_uint_uint(const MNum v1, const MNum v2)
{
    if (Num_IsZero(v2))
        return Num_Create(v1);

    if (Num_SameQSmall(v2, 1))
        return Num_Create(v2);

    MNum a = Num_Uniquelize(v2);
    MNum b = Num_IntMod(v1, v2);
    while (!Num_IsZero(b))
    {        
        MNum t = Num_IntMod(a, b);
        Num_Release(a);
        a = b;
        b = t;
    }
    Num_Release(b);
    return a;
}

static MNum Num_GCD(const MBigNum v1, const MBigNum v2)
{
    MTagNum_impl n1;
    MTagNum_impl n2;

    n1.Type = ntBig;
    n1.Big = v1;
    n2.Type = ntBig;
    n2.Big = v2;

    return Num_GCD(to_interf(&n1), to_interf(&n2));
}

MNum Num_GCD(const MNum v1, const MNum v2)
{
    MNum n1 = Num_Uniquelize(v1);
    MNum n2 = Num_Uniquelize(v2);

    ASSERT(Num_IsInt(n1));
    ASSERT(Num_IsInt(n2));

    Num_AbsBy(n1);
    Num_AbsBy(n1);

    MNum r = Num_GCD_uint_uint(n1, n2);

    Num_Release(n1);
    Num_Release(n2);

    return r;
}

/*
static MNum Num_LCM_uint_uint(const MNum v1, const MNum v2)
{
    MNum gcd = Num_GCD_uint_uint(v1, v2);

    if (!Num_IsZero(gcd))
    {
        MNum t1 = Num_IntDiv(v1, gcd);
        Num_Release(gcd);
        return Num_MultiplyBy(t1, v2);
    }
    else
        return gcd;
}
*/

MNum Num_LCM(const MNum v1, const MNum v2)
{
    MNum gcd = Num_GCD(v1, v2);
    if (!Num_IsZero(gcd))
    {
        MNum t1 = Num_IntDiv(v1, gcd);
        Num_Release(gcd);
        return Num_MultiplyBy(t1, v2);
    }
    else
        return gcd;
}

MHash Num_Hash(MNum a)
{
    // when compiling with -O3 in gcc 4.4.1, things are massed up.
    // MReal f = Num_ToIEEE754(a);
    // MHash r = *(MHash *)(&f);
    // return r;
    MHash h = 0xdeadbeef;
    const unsigned int m = 0x7fd652ad;
    const int s = 16;
    CASSERT(sizeof(MReal) >= 2 * sizeof(MHash));
    union
    {
        MReal f;
        MHash r[2];
    };
    f = Num_ToIEEE754(a);
    	

	h += r[0];
    h *= m;
    h ^= h >> s;
    h += r[1];
    h *= m;
    h ^= h >> s;

    h *= m;
	h ^= h >> 10;
	h *= m;
	h ^= h >> 17;

    return h;
}

// ----------------------------------------

static MNum MNum_Dispose(MNum s)
{
    MNum_impl v = (MNum_impl)(s);
    switch (v->Type)
    {
    case ntBigRational:
        Big_Release(v->Denominator);
    case ntBig:
        Big_Release(v->Big);
    default:
        ;
    }
    v->Type = ntBig;
    return s;
}

static char *MNum_DumpObj(MNum v, char *s, const MInt maxlen)
{
    MString s2 = Num_ToString(v);
    s[maxlen - 1] = '\0';
    MStringToASCII(s2, (MByte *)s, maxlen - 2);
    MString_Release(s2);
    return s;
}

static MXRefObjMT MNum_MT = {NULL, (f_YM_XRefObjDispose)MNum_Dispose, (f_YM_XRefObjDump)MNum_DumpObj}; // (f_YM_XRefObjDump)MNum_DumpObj

void Num_Init()
{
    MInt i;

    Type_MNum = MXRefObj_Register(&MNum_MT, sizeof(MTagNum_impl), "MTagNum_impl");
//    Heap_MNum = MHeap_NewHeap("MNum", 100000);

    for (i = -CONSTANT_INT_CACHE; i <= CONSTANT_INT_CACHE; i++)
        s_NumIntCache[i + CONSTANT_INT_CACHE] = Num_CreateNoCache(i);
}

void Num_DumpHeap(FILE *file)
{
//    MHeap_Dump(Heap_MNum, file);
}
