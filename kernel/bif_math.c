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
#include "bif.h"
#include "sys_sym.h"
#include "sys_msg.h"
#include "eval.h"
#include "msg.h"
#include "num.h"
#include <math.h>

def_bif(EvenQ)
{
    verify_arity_with_r(Factorial, 1);
    r = MSequence_EleAt(seq, 0);
    if (r->Type != etNum)
        return MExpr_CreateSymbol(sym_False);
    return Num_EvenQ(r->Num) ? MExpr_CreateSymbol(sym_True) : MExpr_CreateSymbol(sym_False);
}

def_bif(OddQ)
{
    verify_arity_with_r(Factorial, 1);
    r = MSequence_EleAt(seq, 0);
    if (r->Type != etNum)
        return MExpr_CreateSymbol(sym_False);
    return Num_EvenQ(r->Num) ? MExpr_CreateSymbol(sym_False) : MExpr_CreateSymbol(sym_True);
}

def_bif(Factorial)
{
    MNum v;
    MNum n;
    MNum backn;
    MNum one;
    MDword shift = 0;
    verify_arity_with_r(Factorial, 1);
    r = MSequence_EleAt(seq, 0);
    if ((r->Type != etNum) || !Num_IsInt(r->Num))
    {
        // return Gamma[n + 1]
        MSequence *ps = MSequence_Create(1);
        MSequence *plusps = MSequence_Create(2);
        MSequence_SetAt (plusps, 0, r);
        MSequence_SetAtX(plusps, 1, MExpr_CreateMachineInt(1));
        MSequence_SetAtX(ps, 0, MExpr_CreateHeadSeqX(sym_Plus, plusps));
        return MExpr_CreateHeadSeqX(sym_Gamma, ps);
    }
    if (Num_IsNeg(r->Num))
        return MExpr_CreateSymbol(sym_ComplexInfinity);

    if (Num_IsZero(r->Num))
        return MExpr_CreateMachineInt(1);

    one = Num_Create(1);
    v = Num_CreateNoCache(1);
    n = Num_Uniquelize(r->Num);
    backn = Num_Uniquelize(r->Num);
    while (!Num_SameQSmall(n, 1))
    {
        Num_SetFrom(backn, n);
        while (Num_EvenQ(backn))
        {
            Num_ShiftLeftBy(backn, -1);
            shift++;
            if (shift >= 0x7FFFFFFF) goto over_flow;
        }
        Num_MultiplyBy(v, backn);
        Num_SubtractBy(n, one);
    }

    if (shift)
        Num_ShiftLeftBy(v, shift);

    Num_Release(backn);
    Num_Release(one);
    Num_Release(n);
    return MExpr_CreateNumX(v);

over_flow:
    Num_Release(backn);
    Num_Release(one);
    Num_Release(n);
    return MExpr_CreateSymbol(sym_Overflow);
}

def_bif(Factorial2)
{
    MNum v;
    MNum n;
    MNum backn;
    MNum step;
    MDword shift = 0;
    verify_arity_with_r(Factorial, 1);
    r = MSequence_EleAt(seq, 0);
    if ((r->Type != etNum) || !Num_IsInt(r->Num) || Num_IsNeg(r->Num))
        return MExpr_CreateHeadSeqX(sym_Factorial2, seq);

    if (Num_IsZero(r->Num) || Num_SameQSmall(r->Num, 1))
        return MExpr_CreateMachineInt(1);

    step = Num_Create(2);
    v = Num_CreateNoCache(1);
    n = Num_Uniquelize(r->Num);
    backn = Num_Uniquelize(r->Num);
    while (!Num_SameQSmall(n, 1) && !Num_IsZero(n))
    {
        Num_SetFrom(backn, n);
        while (Num_EvenQ(backn))
        {
            Num_ShiftLeftBy(backn, -1);
            shift++;
            if (shift >= 0x7FFFFFFF) goto over_flow;
        }
        Num_MultiplyBy(v, backn);
        Num_SubtractBy(n, step);
    }

    if (shift)
        Num_ShiftLeftBy(v, shift);

    Num_Release(backn);
    Num_Release(step);
    Num_Release(n);
    return MExpr_CreateNumX(v);

over_flow:
    Num_Release(backn);
    Num_Release(step);
    Num_Release(n);
    return MExpr_CreateSymbol(sym_Overflow);
}

def_bif(Quotient)
{
	verify_arity_with_r(Quotient, 2);
	if (!check_arg_types("NN", seq, sym_Quotient, false, _CONTEXT))
		return MExpr_CreateHeadSeq(sym_Quotient, seq);
	return MExpr_CreateNumX(Num_Div(MSequence_EleAt(seq, 0)->Num, MSequence_EleAt(seq, 1)->Num));
}

def_bif(Mod)
{
	verify_arity_with_r(Quotient, 2);
	if (!check_arg_types("NN", seq, sym_Mod, false, _CONTEXT))
		return MExpr_CreateHeadSeq(sym_Mod, seq);
	return MExpr_CreateNumX(Num_Mod(MSequence_EleAt(seq, 0)->Num, MSequence_EleAt(seq, 1)->Num));
}

def_bif(QuotientRemainder)
{
	verify_arity_with_r(Quotient, 2);
	if (!check_arg_types("NN", seq, sym_QuotientRemainder, false, _CONTEXT))
		return MExpr_CreateHeadSeq(sym_QuotientRemainder, seq);
    else
    {
        MNum remainder = NULL;
        MNum quo =  Num_DivMod(MSequence_EleAt(seq, 0)->Num, 
                               MSequence_EleAt(seq, 1)->Num, 
                               &remainder);
        MSequence *ss = MSequence_Create(2);
        MSequence_SetAtX(ss, 0, MExpr_CreateNumX(quo));
        MSequence_SetAtX(ss, 1, MExpr_CreateNumX(remainder));
	    return MExpr_CreateHeadSeqX(sym_List, ss);
    }
}

def_bif(Ceiling)
{
	MNum n;
	MNum intpart;
	MNum fract;
	verify_arity_with_r(Quotient, 1);
	if (!check_arg_types("N", seq, sym_Mod, false, _CONTEXT))
		return MExpr_CreateHeadSeq(sym_Mod, seq);
	n = MSequence_EleAt(seq, 0)->Num;
	intpart = Num_UniquelizeIntPart(n);
	fract = Num_UniquelizeFractPart(n);
	if (!Num_IsZero(fract) && !Num_IsNeg(n))
			Num_AddBy(intpart, 1);
	Num_Release(fract);
	return MExpr_CreateNumX(intpart);
}

def_bif(Floor)
{
	MNum n;
	MNum intpart;
	MNum fract;
	verify_arity_with_r(Quotient, 1);
	if (!check_arg_types("N", seq, sym_Mod, false, _CONTEXT))
		return MExpr_CreateHeadSeq(sym_Mod, seq);
	n = MSequence_EleAt(seq, 0)->Num;
	intpart = Num_UniquelizeIntPart(n);
	fract = Num_UniquelizeFractPart(n);
	if (!Num_IsZero(fract) && Num_IsNeg(n))
        Num_AddBy(intpart, -1);
	Num_Release(fract);
	return MExpr_CreateNumX(intpart);
}

def_bif(Round)
{
	MNum n;
	verify_arity_with_r(Quotient, 1);
	if (!check_arg_types("N", seq, sym_Mod, false, _CONTEXT))
		return MExpr_CreateHeadSeq(sym_Mod, seq);
	n = MSequence_EleAt(seq, 0)->Num;
	return MExpr_CreateNumX(Num_Round(n));
}
