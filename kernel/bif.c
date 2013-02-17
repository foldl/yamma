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
#include "sym.h"
#include "encoding.h"
#include "parser.h"
#include "matcher.h"
#include <math.h>

#define return_with_bif_seqX(bif, seq)      \
do {                                        \
	MExpr r = BIF_##bif(seq, _CONTEXT);     \
    MSequence_Release(seq);                 \
    return r;                               \
} while (0)

MExpr verify_arity_fun(MSequence *seq, MSymbol s, const MInt arity, _MCONTEXT_)
{
    if (MSequence_Len(seq) != arity)
    {
        MSequence *ss = MSequence_Create(3);
        MSequence_SetAtX(ss, 0, MExpr_CreateSymbol(s));
        MSequence_SetAtX(ss, 1, MExpr_CreateMachineInt(MSequence_Len(seq)));
        MSequence_SetAtX(ss, 2, MExpr_CreateMachineInt(arity));
        BIF_Message(s, msg_Generalargrx, ss, _Context);
        MSequence_Release(ss);
        return MExpr_CreateHeadSeq(s, seq);
    }
    else
        return NULL;
}

MExpr BIF_Plus2_1(MExpr expr1, MExpr expr2);
MExpr BIF_Times2_1(MExpr expr1, MExpr expr2);
//static MExpr BIF_Message(MSymbol sym, MExpr expr);

extern MString K_Input(MString prompt, MString init);
extern void K_Output(int port, MString msg);

/*
static MExpr ZeroFixup_Times(MExpr expr)
{
    if ((MSequence_Len(expr->HeadSeq.pSequence) > 0)
        && (is_exact_zero(MSequence_EleAt(expr->HeadSeq.pSequence, 0))))
    {
        MExpr r = MSequence_EleAt(expr->HeadSeq.pSequence, 0);
        XRef_IncRef(r);
        MExpr_Release(expr);
        return r;
    }
    else
        return expr;
}
*/

static MExpr ZeroFixup_Complex(MExpr expr)
{
    if (is_num_exact_zero(expr->Complex.Imag))
    {
        MExpr r = MExpr_CreateNum(expr->Complex.Real);
        MExpr_Release(expr);
        return r;
    }
    else
        return expr;
}

bool IsExprSameQ(MExpr expr1, MExpr expr2)
{
    bool r = false;
    int i;
    if (expr1 == expr2) return true;
//    if (MExpr_Hash(expr1) != MExpr_Hash(expr2))
//        return false;
    if (expr1->Type != expr2->Type)
        return false;
    switch (expr1->Type)
    {
    case etNum:          return Num_SameQ(expr1->Num, expr2->Num); break;
    case etString:       return MString_SameQ(expr1->Str, expr2->Str); break;
    case etComplex:      return Num_SameQ(expr1->Complex.Real, expr2->Complex.Real) &&
                                Num_SameQ(expr1->Complex.Imag, expr2->Complex.Imag); break;
    case etSymbol:       return expr1->Symbol == expr2->Symbol; break;                         
#if 0
    case etSymbol:       return (expr1->Symbol->Context == expr2->Symbol->Context) &&
                                MString_SameQ(expr1->Symbol->Name, expr2->Symbol->Name); break;
#endif
    case etCons:         return IsExprSameQ(expr1->Cons.Car, expr2->Cons.Car) &&
                                IsExprSameQ(expr1->Cons.Cdr, expr2->Cons.Cdr); 
                         break;
    case etHeadSeq:     r = (MSequence_Len(expr1->HeadSeq.pSequence) == MSequence_Len(expr2->HeadSeq.pSequence))
                            && IsExprSameQ(expr1->HeadSeq.Head, expr2->HeadSeq.Head);
                        if (!r)
                            return r;
                        for (i = 0; i < MSequence_Len(expr1->HeadSeq.pSequence); i++)
                        {
                            r = IsExprSameQ(expr1->HeadSeq.pSequence->pExpr[i],
                                            expr2->HeadSeq.pSequence->pExpr[i]);
                            if (!r)
                                return r;
                        }
                        break;
     default:
            ASSERT("no default" == NULL);
    }

    return r;
}

static bool IsExprUnsameQ(MExpr expr1, MExpr expr2)
{
    bool r = false;
    int i;
//    if (MExpr_Hash(expr1) != MExpr_Hash(expr2))
//        return true;
    if (expr1->Type != expr2->Type)
        return true;
    switch (expr1->Type)
    {
    case etNum:          return Num_UnsameQ(expr1->Num, expr2->Num); break;
    case etString:       return !MString_SameQ(expr1->Str, expr2->Str); break;
    case etComplex:      return Num_UnsameQ(expr1->Complex.Real, expr2->Complex.Real) ||
                                Num_UnsameQ(expr1->Complex.Imag, expr2->Complex.Imag); break;
    case etCons:         return IsExprUnsameQ(expr1->Cons.Car, expr2->Cons.Car) ||
                                IsExprUnsameQ(expr1->Cons.Cdr, expr2->Cons.Cdr); 
                         break;
    case etSymbol:       return (expr1->Symbol->Context != expr2->Symbol->Context) ||
                                !MString_SameQ(expr1->Symbol->Name, expr2->Symbol->Name); break;
    case etHeadSeq:     r = (MSequence_Len(expr1->HeadSeq.pSequence) != MSequence_Len(expr2->HeadSeq.pSequence))
                            || IsExprUnsameQ(expr1->HeadSeq.Head, expr2->HeadSeq.Head);
                        if (r)
                            return r;
                        for (i = 0; i < MSequence_Len(expr1->HeadSeq.pSequence); i++)
                        {
                            r = IsExprUnsameQ(expr1->HeadSeq.pSequence->pExpr[i],
                                              expr2->HeadSeq.pSequence->pExpr[i]);
                            if (r)
                                return r;
                        }
                        break;
    default:
            ASSERT("no default" == NULL);
    }

    return r;
}

bool IsSeqSameQ(MSequence *s1, MSequence *s2, const int from, const int to)
{
    int i;
    if (s1 == s2) return true;
    for (i = from; i < to; i++)
    {
        if (!IsExprSameQ(s1->pExpr[i], s2->pExpr[i]))
            return false;
    }
    return true;
}

void BIF_Message1(MSymbol sym, MExpr expr, MExpr p1, _MCONTEXT_);
void BIF_Message2(MSymbol sym, MExpr expr, MExpr p1, MExpr p2, _MCONTEXT_);
void BIF_Message3(MSymbol sym, MExpr expr, MExpr p1, MExpr p2, MExpr p3, _MCONTEXT_);
void BIF_Message4(MSymbol sym, MExpr expr, MExpr p1, MExpr p2, MExpr p3, MExpr p4, _MCONTEXT_);

bool IsExprNegInfinity(const MExpr t)
{
    if ((t->Type == etHeadSeq) && is_header(t, sym_Times) && (2 == MSequence_Len(t->HeadSeq.pSequence))
            && is_sym(MSequence_EleAt(t->HeadSeq.pSequence, 0), sym_Infinity))
    {
        MExpr neg1 = MSequence_EleAt(t->HeadSeq.pSequence, 0);
        if ((neg1->Type == etNum) && (Num_SameQSmall(neg1->Num, -1)))
            return true;  
    }
      
    return false;
}

MInt ToIntWithClipInfinity(MExpr expr)
{
    if (expr->Type == etNum)
        return Num_ToInt(expr->Num);
    else if (is_sym(expr, sym_Infinity))
        return M_MAX_INT;
    else if (IsExprNegInfinity(expr))
        return -M_MAX_INT;
    else
        return 0;
}

// types:
// i:       machine integer
// I:       any integer
// f:       machine real
// F:       any real
// N:       number
// T:       number, or Infinity, or -Infinity
// R:       rational
// c:       etComplex
// C:       T | R | c 
// S:       string
// M:       symbol
// O:       cons
// A:       atom, (non headseq)
// X:       head seq
// L:       List
// .:       anything
// *:       zero or more args (only allowed to appear as the last char in types)
// +:       1 or more args    (only allowed to appear as the last char in types)
// {III}:           A list with 3 integers
// {{III}{III}}:    A 2 * 3 integer matrix
// {II*}:           A list with 2 integers and zero or more elements
inline bool check_arg_type(const char type, const MExpr t, const MSymbol sym, 
						   const MBool bReport, const int OutterPos context_param)
{
#define report(need)    \
do {                \
if (bReport)        \
{                   \
    MExpr pos = MExpr_CreateMachineInt(OutterPos + 1);      \
    MExpr sneed = MExpr_CreateStringX(MString_NewC(need));  \
    MExpr symexpr = MExpr_CreateSymbol(sym);  \
    BIF_Message4(sym, msg_Generalargtyp, symexpr, pos, t, sneed, _CONTEXT); \
    MExpr_Release(pos);                                 \
    MExpr_Release(sneed);                               \
    MExpr_Release(symexpr);                             \
}\
} while (false)

	switch (type)
    {
    case 'i': if ((t->Type != etNum) || !Num_IsMachineInt(t->Num))
              {
                  report("MachineInteger");
                  return false;
              }
              break;
    case 'I': if ((t->Type != etNum) || !Num_IsInt(t->Num))
              {
                  report("Integer");
                  return false;
              }
              break;
    case 'R': if ((t->Type != etNum) || !Num_IsRational(t->Num))
              {
                  report("Rational");
                  return false;
              }
              break;
    case 'f': if ((t->Type != etNum) || !Num_IsMachineReal(t->Num))
              {
                  report("MachineReal");
                  return false;
              }
              break;
    case 'F': if ((t->Type != etNum) || !Num_IsReal(t->Num))
              {
                  report("MachineReal");
                  return false;
              }
              break;
    case 'N': if (t->Type != etNum)
              {
                  report("Number");
                  return false;
              }
              break;
    case 'T': if ((t->Type != etNum) && !is_sym(t, sym_Infinity) && !IsExprNegInfinity(t))
              {
                  report("Number");
                  return false;
              }
              break;
    case 'A': if (t->Type == etHeadSeq)
              {
                  report("Atom");
                  return false;
              }
              break;
    case 'c': if (t->Type != etComplex)
              {
                  report("Complex[a, b]");
                  return false;
              }
              break;
    case 'C': if (!((t->Type == etComplex) || (t->Type == etNum) || is_sym(t, sym_Infinity) || IsExprNegInfinity(t)))
              {
                  report("Complex");
                  return false;
              }
              break;
    case 'S': if (t->Type != etString)
              {
                  report("String");
                  return false;
              }
              break;
    case 'M': if (t->Type != etSymbol) 
              {
                  report("Symbol");
                  return false;
              }
              break;
    case 'O': if (t->Type != etCons) 
              {
                  report("Cons");
                  return false;
              }
              break;
    case 'X': if (t->Type != etHeadSeq)
              {
                  report("S-Expr");
                  return false;
              }
              break;
    case 'L': if ((t->Type != etHeadSeq) || !is_header(t, sym_List))
              {
                  report("{...}");
                  return false;
              }
              break;
    case '.': return true;
	default:  return false;		
    }

	return true;
}

bool check_arg_types_inner(const char *types, MSequence *seq, const MSymbol sym, 
						   const MBool bReport, const int Level, const int OutterPos, int *TypeLen context_param)
{
    int i = 0;
    char c = '\0';
	char lc = '\0';
    int len = 0;

    while (lc = c, c = *types++)
    {
        MExpr t;
        switch (c)
        {
        case '*': 
            if (i > MSequence_Len(seq))
            {
                if (bReport)
                {
                    MExpr symexpr = MExpr_CreateSymbol(sym);
                    MExpr num = MExpr_CreateMachineInt(MSequence_Len(seq));
                    BIF_Message2(sym, msg_Generalargmore, symexpr, num, _CONTEXT);
                    MExpr_Release(symexpr);
                    MExpr_Release(num);
                }
                return false;
            }
            else if (i < MSequence_Len(seq))           
            {
				int j = i;
				for (; j < MSequence_Len(seq); j++)
					if (!check_arg_type(lc, MSequence_EleAt(seq, j), sym, bReport, Level == 0 ? j : OutterPos, _CONTEXT))
						return false;
                return true;
			}
            else
                return true;
            break;
        case '+': 
            if (i >= MSequence_Len(seq))
            {
                if (bReport)
                {
                    MExpr symexpr = MExpr_CreateSymbol(sym);
                    MExpr num = MExpr_CreateMachineInt(MSequence_Len(seq));
                    BIF_Message2(sym, msg_Generalargmore, symexpr, num, _CONTEXT);
                    MExpr_Release(symexpr);
                    MExpr_Release(num);
                }
                return false;
            }
            else
            {
				int j = i;
				for (; j < MSequence_Len(seq); j++)
					if (!check_arg_type(lc, MSequence_EleAt(seq, j), sym, bReport, Level == 0
                                ? j : OutterPos, _CONTEXT))
						return false;
                return true;
			}
            break;
        case '}':
            *TypeLen = i;
            goto check_number;
            break;
        case '{':
            if (i >= MSequence_Len(seq)) return false;
            t = MSequence_EleAt(seq, i);

            if (!check_arg_type('L', t, sym, bReport, Level == 0 ? i : OutterPos, _CONTEXT))
                return false;                  
            if (!check_arg_types_inner(types, t->HeadSeq.pSequence, sym, bReport, Level + 1, 
                                 Level == 0 ? i : OutterPos,
                                 &len, _CONTEXT))
                return false;
            types += len;   
            c = *types++;
            ASSERT(c == '}');
            break;
        default:
            if (i >= MSequence_Len(seq)) return false;
            t = MSequence_EleAt(seq, i);

            if (!check_arg_type(c, t, sym, bReport, Level == 0 ? i : OutterPos, _CONTEXT))
                return false;;
        }
        i++;
    }

check_number:
    if ((i != MSequence_Len(seq)) && bReport)
        verify_arity_fun(seq, sym, i, _CONTEXT);
    else;
    
    return i == MSequence_Len(seq);
}

bool check_arg_types(const char *types, MSequence *seq, const MSymbol sym, const MBool bReport context_param)
{
	return check_arg_types_inner(types, seq, sym, bReport, 0, 0, NULL, _CONTEXT);
}

MExpr BIF_Plus2(MExpr expr1, MExpr expr2, _MCONTEXT_)
{
    MSequence *s;
    int i;
    MNum re, im;

    if (expr1->Type == expr2->Type)
    {
        switch (expr1->Type)
        {
        case etNum:
            return MExpr_CreateNumX(Num_Add(expr1->Num, expr2->Num));
        case etComplex:
            re = Num_Add(expr1->Complex.Real, expr2->Complex.Real);
            im = Num_Add(expr1->Complex.Imag, expr2->Complex.Imag);

            return ZeroFixup_Complex(MExpr_CreateComplexX(re, im));
        case etSymbol:
            if (MString_SameQ(expr1->Symbol->Name, expr2->Symbol->Name))
            {
                s = MSequence_Create(2);
                MSequence_SetAtX(s, 0, MExpr_CreateMachineInt(2));
                MSequence_SetAt(s,  1, expr1);
                return_with_bif_seqX(Times, s);
            }
            return NULL;
            break;
        case etHeadSeq:
            if (is_header(expr1, sym_Times) && is_header(expr2, sym_Times))
            {
                if ((MSequence_Len(expr1->HeadSeq.pSequence) == MSequence_Len(expr2->HeadSeq.pSequence))
                    && is_inZ(expr1->HeadSeq.pSequence->pExpr[0]) && is_inZ(expr2->HeadSeq.pSequence->pExpr[0])
                    && IsSeqSameQ(expr1->HeadSeq.pSequence, expr2->HeadSeq.pSequence, 
                                  1, MSequence_Len(expr1->HeadSeq.pSequence)))
                {
                    MSequence tags;
                    MSequence *s;
                    MExpr t;

                    s = MSequence_InitStatic(&tags, 2);
                    MSequence_SetAt(s, 0, expr1->HeadSeq.pSequence->pExpr[0]);
                    MSequence_SetAt(s, 1, expr2->HeadSeq.pSequence->pExpr[0]);
                    t = BIF_Plus(s, _Context);
                    MSequence_ReleaseStatic(s);
                    if (!is_exact_zero(t))
                    {
                        MSequence *s = MSequence_Create(MSequence_Len(expr1->HeadSeq.pSequence));
                        MSequence_SetAtX(s, 0, t);
                        for (i = 1; i < MSequence_Len(s); i++)
                            MSequence_SetAt(s, i, expr1->HeadSeq.pSequence->pExpr[i]);
                        return_with_bif_seqX(Times, s);
                    }
                    else
                        return t;
                }
            }
            else;

            if (IsExprSameQ(expr1, expr2))
            {
                MSequence *s = MSequence_Create(2);
                MSequence_SetAtX(s, 0, MExpr_CreateMachineInt(2));
                MSequence_SetAt(s, 1, expr1);
                return_with_bif_seqX(Times, s);
            }
            break;
        default:
            ASSERT("BIF_Plus2 no default" == NULL);
        }
    }

    if (expr2->Type == etHeadSeq)
    {
        if ((2 == MSequence_Len(expr2->HeadSeq.pSequence))
            && is_inZ(expr2->HeadSeq.pSequence->pExpr[0])
            && is_header(expr2, sym_Times)
            && IsExprSameQ(expr1, expr2->HeadSeq.pSequence->pExpr[1]))
        {
            MSequence *s = MSequence_Create(2);
            MExpr t;
            MNum n;
            
            switch (expr2->HeadSeq.pSequence->pExpr[0]->Type)
            {
            case etNum:
                n = Num_Uniquelize(expr2->HeadSeq.pSequence->pExpr[0]->Num);
                Num_AddBy(n, 1);
                t = MExpr_CreateNumX(n);
                if (is_num_exact_zero(n))
                {
                    MSequence_Release(s);
                    return t;
                }
                break;
            case etComplex:
                n = Num_Uniquelize(expr2->HeadSeq.pSequence->pExpr[0]->Complex.Real);
                Num_AddBy(n, 1);
                t = ZeroFixup_Complex(MExpr_CreateComplex(n, expr2->HeadSeq.pSequence->pExpr[0]->Complex.Imag));
                Num_Release(n);
                break;
            default:
                ASSERT("BIF_Plus2 no default" == NULL);
                t = NULL;
            }

            MSequence_SetAtX(s, 0, t);
            MSequence_SetAt(s, 1, expr1);
            return_with_bif_seqX(Times, s);
        }
    }

    return NULL;
}

MExpr BIF_Plus2_1(MExpr expr1, MExpr expr2, _MCONTEXT_)
{
    MExpr r = BIF_Plus2(expr1, expr2, _Context);
    if (r == NULL)
    {
        MSequence *s = MSequence_Create(2);
        MSequence_SetAt(s, 0, expr1);
        MSequence_SetAt(s, 1, expr2);
        r = MExpr_CreateHeadSeq(sym_Plus, s);
        MSequence_Release(s);
    }
    return r;
}

// TODO: optimize: lazy creation of seq
static inline MInt BIF_ListFun2OverSeq(MSequence *seq, const MInt readPos, 
                                       MExpr (*fun)(MExpr, MExpr context_param) context_param)
{
    MInt i;
    MExpr t;
    MInt r = MSequence_Len(seq);
    MBool bRunAgain = true;
    while (bRunAgain)
    {
        bRunAgain = false;
        for (i = 0; i < r - 1; i++)
        {
            MInt k;
            for (k = i + 1; k < r; k++)
            {
                t = fun(MSequence_EleAt(seq, i), MSequence_EleAt(seq, k), _Context);
                if (t)
                    break;
            }

            if (t)
            {
                MInt j;

                MExpr_Release(MSequence_EleAt(seq, i));
                MExpr_Release(MSequence_EleAt(seq, k));
                MSequence_EleAt(seq, i) = NULL;
                MSequence_EleAt(seq, k) = NULL;

                if (r > 2)
                {
                    for (j = 0; j < r; j++)
                    {
                        if (MSequence_EleAt(seq, j))
                        {
                            if (Eval_ExprCmp(t, MSequence_EleAt(seq, j)) <= 0)
                                break;
                        }
                
                    }

                    if (j < i)
                    {
                        memmove(seq->pExpr + j + 1, seq->pExpr + j, (i - j) * sizeof(*(seq->pExpr)));
                        seq->pExpr[j] = t;
                        memmove(seq->pExpr + k, seq->pExpr + k + 1, (r - k - 1) * sizeof(*(seq->pExpr)));
                    }
                    else if (j < k)
                    {
                        memmove(seq->pExpr + i, seq->pExpr + i + 1, (j - i - 1) * sizeof(*(seq->pExpr)));
                        seq->pExpr[j - 1] = t;
                        memmove(seq->pExpr + k, seq->pExpr + k + 1, (r - k - 1) * sizeof(*(seq->pExpr)));
                    }
                    else    // j > k
                    {
                        memmove(seq->pExpr + i, seq->pExpr + i + 1, (k - i - 1) * sizeof(*(seq->pExpr)));
                        memmove(seq->pExpr + k - 1, seq->pExpr + k + 1, (j - k - 1) * sizeof(*(seq->pExpr)));
                        seq->pExpr[j - 2] = t;
                        memmove(seq->pExpr + j - 1, seq->pExpr + j, (r - j) * sizeof(*(seq->pExpr)));
                    }

                    r--;
                    MSequence_EleAt(seq, r) = NULL; // this element is *copied* ahead

                    bRunAgain = true;
                
                }
                else
                {
                    MSequence_EleAt(seq, 0) = t;
                    r = 1;
                }

                break;
            }
            else;
        }
    }

    return r;
}

def_bif(Plus)
{
//    return MExpr_CreateHeadSeq(sym_Plus, seq);
//    return MExpr_CreateMachineInt(-3);

    MNum n = NULL;
    MExpr vc = NULL;
    bool bFoundNum = false;
    bool bFoundComplex = false;
    int i = 0;
    MExpr *p = seq->pExpr;
    MExpr r = NULL;

    if (MSequence_Len(seq) == 0)
        return MExpr_CreateMachineInt(0);

    if ((*p)->Type == etNum)
    {
        n = Num_Uniquelize((*p)->Num);
        p++;
        i = 1;
        bFoundNum = true;
    }
    else;

    for (; i < MSequence_Len(seq); i++, p++)
    {
        if ((*p)->Type == etNum)
        {
            Num_AddBy(n, (*p)->Num);
            bFoundNum = true;
        }
        else
            break;
    }

    if ((i < MSequence_Len(seq)) && ((*p)->Type == etComplex) && (NULL == n))
        n = Num_Create(0);

    for (; i < MSequence_Len(seq); i++, p++)
    {
        if ((*p)->Type == etComplex)
        {
            MSequence *s;
            MNum re, im;

            bFoundComplex = true;
            s = MSequence_Create(2);

            if (vc == NULL)
            {
                if (n)
                    vc = MExpr_CreateComplexX(n, Num_Create(0));
                else
                    vc = MExpr_CreateComplexX(Num_Create(0),
                                              Num_Create(0));
            }

            re = Num_Add(vc->Complex.Real, (*p)->Complex.Real);
            im = Num_Add(vc->Complex.Imag, (*p)->Complex.Imag);

            MExpr_Release(vc);
            vc = ZeroFixup_Complex(MExpr_CreateComplexX(re, im));
        }
        else
            break;
    }

    if (vc)
        r = vc;
    else if (n)
    {
        if (!Num_IsZero(n) || (i == MSequence_Len(seq)))
            r = MExpr_CreateNumX(n);
        else
            Num_Release(n);
    }

    if (i < MSequence_Len(seq))
    {
        MSequence *s = NULL;
        MInt end;
        if (i > 0)
        {
            if (r)
            {
                i--;
                s = MSequence_UniqueCopy(seq, i, MSequence_Len(seq) - i);
                MSequence_SetAt(s, 0, r);
                i = 1;
            }
            else
            {
                s = MSequence_UniqueCopy(seq, i, MSequence_Len(seq) - i);
                i = 0;
            }  
        }
        else
        {
            s = MSequence_UniqueCopy(seq, i, MSequence_Len(seq) - i);
            i = 0;
        }
        
        end = BIF_ListFun2OverSeq(s, i, BIF_Plus2, _Context);

        if (end > 1)
        {
            MSequence *rs = MSequence_Copy(s, 0, end);
            r = MExpr_CreateHeadSeqX(sym_Plus, rs);
        }
        else
        {
            r = MSequence_EleAt(s, 0);
            XRef_IncRef(r);
        }
        MSequence_Release(s);
    }
    else;

    return r ? r : MExpr_CreateMachineInt(0);
}

static MExpr BIF_Times2_num(MExpr expr1, MExpr expr2)
{
    if ((expr1->Type == etComplex) || (expr2->Type == etComplex))
    {
        //   (a + b j) * (c + d j)
        // = [ac - bd] + [ad + bc] j
        // = [ac - bd] + {(a + b) (c + d) - ac - bd}
        MNum a, b, c, d;
        MNum ac, bd;
        MNum SumAB, SumCD, Imag;
        MExpr r;

        if (expr1->Type == etNum)
        {
            a = Num_Create(expr1->Num);
            b = Num_Create(0);
        }
        else
        {
            a = Num_Create(expr1->Complex.Real);
            b = Num_Create(expr1->Complex.Imag);
        }

        if (expr2->Type == etNum)
        {
            a = Num_Create(expr2->Num);
            b = Num_Create(0);
        }
        else
        {
            c = Num_Create(expr2->Complex.Real);
            d = Num_Create(expr2->Complex.Imag);
        }

        ac = Num_Multiply(a, c);
        bd = Num_Multiply(b, d);
        SumAB = Num_Add(a, b);
        SumCD = Num_Add(c, d);
        Imag = Num_Multiply(SumAB, SumCD);
        Num_SubtractBy(Imag, ac);
        Num_SubtractBy(Imag, bd);
    
        Num_SubtractBy(ac, bd);

        r = MExpr_CreateComplex(ac, Imag);

        Num_Release(a); Num_Release(b); Num_Release(c); Num_Release(d); 
        Num_Release(ac); Num_Release(bd);
        Num_Release(SumAB); Num_Release(SumAB); Num_Release(Imag);
        return r;
    }
    else
        return MExpr_CreateNumX(Num_Multiply(expr1->Num, expr2->Num));
}

static MExpr BIF_Times2(MExpr expr1, MExpr expr2, _MCONTEXT_)
{
    MSequence *s;
    MExpr r;
    int i;

    if (is_exact_n(expr1, 1))
    {
        XRef_IncRef(expr2);
        return expr2;
    }

    if (expr1->Type == expr2->Type)
    {
        switch (expr1->Type)
        {
        case etNum:
            return MExpr_CreateNumX(Num_Multiply(expr1->Num, expr2->Num));
        case etSymbol:
            if (MString_SameQ(expr1->Symbol->Name, expr2->Symbol->Name))
            {
                s = MSequence_Create(2);
                MSequence_SetAt(s,  0, expr1);
                MSequence_SetAtX(s, 1, MExpr_CreateMachineInt(2));
                return_with_bif_seqX(Power, s);
            }
            return NULL;
            break;
        case etHeadSeq:
            if (is_header(expr1, sym_Power) && is_header(expr2, sym_Power))
            {
                if ((MSequence_Len(expr1->HeadSeq.pSequence) == MSequence_Len(expr2->HeadSeq.pSequence)) 
                        && (MSequence_Len(expr1->HeadSeq.pSequence) > 1))
                {
                    if  (IsSeqSameQ(expr1->HeadSeq.pSequence, expr2->HeadSeq.pSequence, 
                                0, MSequence_Len(expr1->HeadSeq.pSequence) - 1)
                        && is_inZ(MSequence_EleAt(expr1->HeadSeq.pSequence, 
                                                  MSequence_Len(expr1->HeadSeq.pSequence) - 1))
                        && is_inZ(MSequence_EleAt(expr2->HeadSeq.pSequence, 
                                                  MSequence_Len(expr1->HeadSeq.pSequence) - 1)))
                    {
                        MSequence *s = MSequence_UniqueCopy(expr1->HeadSeq.pSequence, 0, 
                                                            MSequence_Len(expr1->HeadSeq.pSequence));

                        r = BIF_Plus2(MSequence_EleAt(expr1->HeadSeq.pSequence, 
                                                      MSequence_Len(expr1->HeadSeq.pSequence) - 1),
                                      MSequence_EleAt(expr2->HeadSeq.pSequence, 
                                                      MSequence_Len(expr1->HeadSeq.pSequence) - 1), _Context);
                        MSequence_SetAtX(s, MSequence_Len(expr1->HeadSeq.pSequence) - 1, r);
                        return_with_bif_seqX(Power, s);
                    }
                    else;

                    if  (IsSeqSameQ(expr1->HeadSeq.pSequence, expr2->HeadSeq.pSequence, 1, 
                                MSequence_Len(expr1->HeadSeq.pSequence))
                        && is_inZ(MSequence_EleAt(expr1->HeadSeq.pSequence, 0))
                        && is_inZ(MSequence_EleAt(expr2->HeadSeq.pSequence, 0)))
                    {
                        MSequence *s = MSequence_Create(MSequence_Len(expr1->HeadSeq.pSequence));

                        r = BIF_Times2_num(MSequence_EleAt(expr1->HeadSeq.pSequence, 0),
                                           MSequence_EleAt(expr2->HeadSeq.pSequence, 0));
                        MSequence_SetAtX(s, 0, r);
                        for (i = 1; i < MSequence_Len(s); i++)
                            MSequence_SetAt(s, i, expr1->HeadSeq.pSequence->pExpr[i]);

                        return_with_bif_seqX(Power, s);
                    }
                    else;
                }
                

            }
            else;
            return NULL;
            break;
        default:
            ASSERT("BIF_Times2 no default" == NULL);
        }
    }

    if (expr2->Type == etHeadSeq)
    {
        if ((2 == MSequence_Len(expr2->HeadSeq.pSequence))
            && is_header(expr2, sym_Power)
            && IsExprSameQ(expr1, expr2->HeadSeq.pSequence->pExpr[0]))
        {
            MSequence *s = MSequence_Create(2);

            MSequence_SetAtX(s, 0, MExpr_CreateMachineInt(1));
            MSequence_SetAt (s, 1, expr2->HeadSeq.pSequence->pExpr[1]);
            r = BIF_Plus(s, _Context);

            MSequence_SetAt(s, 0, expr1);
            MSequence_SetAtX(s, 1, r);
            return_with_bif_seqX(Power, s);
        }
    }

    return NULL;
}

MExpr BIF_Times2_1(MExpr expr1, MExpr expr2, _MCONTEXT_)
{
    MSequence *s;
    MExpr r = BIF_Times2(expr1, expr2, _Context);
    if (r == NULL)
    {
        s = MSequence_Create(2);
        MSequence_SetAt(s, 0, expr1);
        MSequence_SetAt(s, 1, expr2);
        r = MExpr_CreateHeadSeqX(sym_Times, s);
    }
    return r;
}

def_bif(Times)
{
    MNum n = NULL;
    MExpr vc = NULL;
    bool bFoundComplex = false;
    int i = 0;
    MExpr *p = seq->pExpr;
    MExpr r = NULL;

    if (MSequence_Len(seq) == 0)
        return MExpr_CreateMachineInt(1);

    if ((*p)->Type == etNum)
    {
        i = 1;
        n = Num_Uniquelize((*p)->Num);
        p++;
    }
    else;

    for (; i < MSequence_Len(seq); i++, p++)
    {
        if ((*p)->Type == etNum)
            Num_MultiplyBy(n, (*p)->Num);
        else
            break;
    }

    for (; i < MSequence_Len(seq); i++, p++)
    {
        if ((*p)->Type == etComplex)
        {
            MNum ac, bd, bc, ad;

            bFoundComplex = true;

            if (vc == NULL)
            {
                if (n)
                    vc = MExpr_CreateComplexX(n,
                                              Num_Create(0));
                else
                    vc = MExpr_CreateComplexX(Num_Create(1),
                                              Num_Create(0));
                
            }

            // TODO: optimize, reduce a multiply
            ac = Num_Multiply(vc->Complex.Real, (*p)->Complex.Real);
            bd = Num_Multiply(vc->Complex.Imag, (*p)->Complex.Imag);
            bc = Num_Multiply(vc->Complex.Real, (*p)->Complex.Imag);
            ad = Num_Multiply(vc->Complex.Imag, (*p)->Complex.Real);

            MExpr_Release(vc);
            vc = ZeroFixup_Complex(MExpr_CreateComplexX(Num_Subtract(ac, bd), Num_Add(bc, ad)));
            Num_Release(ac);
            Num_Release(bd);
            Num_Release(bc);
            Num_Release(ad);

        }
        else
            break;
    }

    if (bFoundComplex)
        r = vc;
    else if (n)
    {
        if (Num_IsZero(n))
            return MExpr_CreateNumX(n);
        else if ((!Num_SameQSmall(n, 1)) || (i == MSequence_Len(seq)))
            r = MExpr_CreateNumX(n);
        else
            Num_Release(n);
    }
    else;

    if (i < MSequence_Len(seq))
    {
        MSequence *s = NULL;
        MInt end;
        if (i > 0)
        {
            if (r)
            {
                i--;
                s = MSequence_UniqueCopy(seq, i, MSequence_Len(seq) - i);
                MSequence_SetAt(s, 0, r);
                i = 1;
            }
            else
            {
                s = MSequence_UniqueCopy(seq, i, MSequence_Len(seq) - i);
                i = 0;
            }  
        }
        else
        {
            s = MSequence_UniqueCopy(seq, i, MSequence_Len(seq) - i);
            i = 0;
        }
        
        end = BIF_ListFun2OverSeq(s, i, BIF_Times2, _Context);

        if (end > 1)
        {
            MSequence *rs = MSequence_Copy(s, 0, end);
            r = MExpr_CreateHeadSeqX(sym_Times, rs);
        }
        else
        {
            r = MSequence_EleAt(s, 0);
            XRef_IncRef(r);
        }
        MSequence_Release(s);
    }
    return r;
}

def_bif(Power)
{
    MNum n = NULL;
    MInt i;

    for (i = MSequence_Len(seq) - 1; i >= 0; i--)
    {
        if (MSequence_EleAt(seq, i)->Type == etNum)
        {
            if (n == NULL)
                n = Num_Create(MSequence_EleAt(seq, i)->Num);
            else
            {
                MNum t = n;
                n = Num_Power(MSequence_EleAt(seq, i)->Num, t);
                Num_Release(t);
            }
        }
        else
            break;
    }

    if (NULL == n)
        return MExpr_CreateHeadSeq(sym_Power, seq);

    if (i >= 0)
    {
        if (!Num_IsZero(n))
        {
            MInt j;
            MSequence *s;
            s = MSequence_Create(i + 2);
            for (j = 0; j <= i; j++)
                MSequence_SetAt(s, j, MSequence_EleAt(seq, j));
            MSequence_SetAtX(s, i + 1, MExpr_CreateNumX(n));
            return MExpr_CreateHeadSeqX(sym_Power, s);
        }
        else
        {
            Num_Release(n);
            return MExpr_CreateMachineInt(1);
        }
    }
    else
        return MExpr_CreateNumX(n);
}

static MExpr bif_crement(MSequence *seq, MSymbol sym, bool bInc, bool bPre, _MCONTEXT_)
{
    MSymbol s;
    MExpr r;
    if (!check_arg_types("M", seq, sym, true, _CONTEXT))
        return MExpr_CreateHeadSeq(sym, seq);
    s = MSequence_EleAt(seq, 0)->Symbol;
    r = s->Immediate;
    
    if (r == NULL)
    {
        // it's reasonable to treat s as "0" here         
        s->Immediate = MExpr_CreateMachineInt(bInc ? 1 : -1);
        r = s->Immediate;
        if (bPre)
        {
            XRef_IncRef(r);
            return r;
        }
        else
            return MExpr_CreateMachineInt(0);
    }
    else;

    switch (r->Type)
    {
    case etNum:
        if (MObj_IsUnique(r) && MObj_IsUnique(r->Num))
        {
            if (bPre)
            {
                Num_AddBy(r->Num, bInc ? 1 : -1);
                XRef_IncRef(r);
            }
            else
                s->Immediate = MExpr_CreateNumX(Num_AddBy(Num_Uniquelize(r->Num), bInc ? 1 : -1));
            return r;
        }
        else; // go on, no break
    case etComplex:
        {
            MExpr one = MExpr_CreateMachineInt(bInc ? 1 : -1),
            r = BIF_Plus2(s->Immediate, one, _Context);
            MExpr_Release(one);
            if (bPre)
            {
                MExpr_Release(s->Immediate);
                s->Immediate = r;
                XRef_IncRef(r);
            }
            else
            {
                one = r;
                r = s->Immediate;
                s->Immediate = one;
            }
            return r;
        }
    default:
        return MExpr_CreateHeadSeq(sym, seq);
    }
}

def_bif(Increment)
{
    return bif_crement(seq, sym_Increment, true, false, _Context);
}

def_bif(Decrement)
{
    return bif_crement(seq, sym_Decrement, false, false, _Context);
}

def_bif(PreIncrement)
{
    return bif_crement(seq, sym_PreIncrement, true, true, _Context);
}

def_bif(PreDecrement)
{
    return bif_crement(seq, sym_PreDecrement, false, true, _Context);
}

def_bif(AddTo)
{
    MSymbol s;
    MExpr r = MSequence_EleAt(seq, 1);
    if (!check_arg_types("MC", seq, sym_AddTo, true, _CONTEXT))
        return MExpr_CreateHeadSeq(sym_AddTo, seq);
    s = MSequence_EleAt(seq, 0)->Symbol;
    
    if (s->Immediate == NULL)
    {
        s->Immediate = r;
        XRef_IncRef(r);     // it's reasonable to treat s as "0" here 
    }
    else
    {
        switch (s->Immediate->Type)
        {
        case etNum:
            if (r->Type == etNum)
            {
                if (MObj_IsUnique(s->Immediate) && MObj_IsUnique(s->Immediate->Num))
                {
                    Num_AddBy(s->Immediate->Num, r->Num);
                    r = s->Immediate;
                    goto bif_out;
                }
                else; // go on 
            }
            else; // go on           
        case etComplex:
            {
                MSequence ss;
                MSequence *ps = MSequence_InitStatic(&ss, 2);
                MSequence_EleAt(ps, 0) = s->Immediate;
                MSequence_EleAt(ps, 1) =  r;
                r = BIF_Plus(ps, _Context);
                MSequence_EleAt(ps, 1) = NULL;
                MSequence_ReleaseStatic(ps);
                s->Immediate = r;
            }
            break;
        default:
            return MExpr_CreateHeadSeq(sym_AddTo, seq);
        }
    }

bif_out:    
    XRef_IncRef(r);
    return r;
}

def_bif(SubtractFrom)
{
    MSequence *ps;
    MExpr r;
    MSequence ss;
    if (!check_arg_types("MC", seq, sym_SubtractFrom, true, _CONTEXT))
        return MExpr_CreateHeadSeq(sym_SubtractFrom, seq);
    ps = MSequence_InitStatic(&ss, 2);
    MSequence_EleAt(ps, 0) = MSequence_EleAt(seq, 0);
    MSequence_EleAt(ps, 1) = MExpr_CreateNumX(Num_Neg(MSequence_EleAt(seq, 1)->Num));
    r = BIF_AddTo(ps, _Context);
    MSequence_EleAt(ps, 0) = NULL;
    MSequence_ReleaseStatic(ps);
    return r;
}

def_bif(TimesBy)
{
    MSymbol s;
    MExpr r = MSequence_EleAt(seq, 1);
    if (!check_arg_types("MC", seq, sym_TimesBy, true, _CONTEXT))
        return MExpr_CreateHeadSeq(sym_TimesBy, seq);
    s = MSequence_EleAt(seq, 0)->Symbol;
    
    if (s->Immediate == NULL)
    {
        s->Immediate = r;
        XRef_IncRef(r);     // it's reasonable to treat s as "1" here 
    }
    else
    {
        switch (s->Immediate->Type)
        {
        case etNum:
            if (r->Type == etNum)
            {
                if (MObj_IsUnique(s->Immediate) && MObj_IsUnique(s->Immediate->Num))
                {
                    Num_MultiplyBy(s->Immediate->Num, r->Num);
                    r = s->Immediate;
                    goto bif_out;
                }
                else; // go on 
            }
            else; // go on           
        case etComplex:
            {
                MSequence ss;
                MSequence *ps = MSequence_InitStatic(&ss, 2);
                MSequence_EleAt(ps, 0) = s->Immediate;
                MSequence_EleAt(ps, 1) =  r;
                r = BIF_Times(ps, _Context);
                MSequence_EleAt(ps, 1) = NULL;
                MSequence_ReleaseStatic(ps);
                s->Immediate = r;
            }
            break;
        default:
            return MExpr_CreateHeadSeq(sym_TimesBy, seq);
        }
    }

bif_out:    
    XRef_IncRef(r);
    return r;
}

def_bif(DivideBy)
{
    MSequence *ps;
    MExpr r;
    MNum one;
    MSequence ss;
    if (!check_arg_types("MC", seq, sym_DivideBy, true, _CONTEXT))
        return MExpr_CreateHeadSeq(sym_DivideBy, seq);
    one = Num_Create(1);
    ps = MSequence_InitStatic(&ss, 2);
    MSequence_EleAt(ps, 0) = MSequence_EleAt(seq, 0);
    MSequence_EleAt(ps, 0) = MExpr_CreateNumX(Num_CreateRational(one, 
                                                                 MSequence_EleAt(seq, 1)->Num));
    r = BIF_TimesBy(ps, _Context);
    MSequence_EleAt(ps, 0) = NULL;
    MSequence_ReleaseStatic(ps);
    Num_Release(one);
    return r;
}

def_bif(ToExpression)
{
    MString input = NULL;
    MTokener *tokener;
    MExpr r = NULL;
    if (MSequence_Len(seq) == 1)
    {
        if (!check_arg_types("S", seq, sym_ToExpression, true, _CONTEXT))
            return MExpr_CreateHeadSeq(sym_ToExpression, seq);

        input = MSequence_EleAt(seq, 0)->Str;
        tokener = Token_NewFromMemory(input);
        r = Parse_FromTokener(tokener);
        Token_Delete(tokener);

        return r;
    }
    else if (MSequence_Len(seq) == 2)
    {
        MSymbol s;
        if (!check_arg_types("SM", seq, sym_ToExpression, true, _CONTEXT))
            return MExpr_CreateHeadSeq(sym_ToExpression, seq);
        
        s = MSequence_EleAt(seq, 1)->Symbol;
        if (s != sym_InputForm)
            return MExpr_CreateSymbol(sym_DollarFailed);

        input = MSequence_EleAt(seq, 0)->Str;
        tokener = Token_NewFromMemory(input);
        r = Parse_FromTokener(tokener);
        Token_Delete(tokener);

        return r;
    }
    else if (MSequence_Len(seq) == 3)
    {
        MSymbol s;
        MSequence *ps;
        if (!check_arg_types("SM.", seq, sym_ToExpression, true, _CONTEXT))
            return MExpr_CreateHeadSeq(sym_ToExpression, seq);
        
        s = MSequence_EleAt(seq, 1)->Symbol;
        if (s != sym_InputForm)
            return MExpr_CreateSymbol(sym_DollarFailed);

        input = MSequence_EleAt(seq, 0)->Str;
        tokener = Token_NewFromMemory(input);
        r = Parse_FromTokener(tokener);
        Token_Delete(tokener);
        ps = MSequence_Create(1);
        MSequence_SetAtX(ps, 0, r);
        return MExpr_CreateHeadSeqX(MSequence_EleAt(seq, 2), ps);
    }
    else
        return MExpr_CreateHeadSeq(sym_ToExpression, seq);
}

def_bif(ToString)
{
    if (MSequence_Len(seq) == 1)
    {
        return MExpr_CreateStringX(MExpr_ToFullForm(MSequence_EleAt(seq, 0)));
    }
    else if (MSequence_Len(seq) >= 2)
    {
        // not implemented
        return MExpr_CreateHeadSeq(sym_ToString, seq);
    }
    else
        return MExpr_CreateHeadSeq(sym_ToString, seq);
}

def_bif(StringForm)
{
    if (MSequence_Len(seq) == 1)
    {
        MExpr r = MSequence_EleAt(seq, 0);
        XRef_IncRef(r);
        return r;
    }
    else if (MSequence_Len(seq) == 0)
        return MExpr_CreateStringX(MString_NewC(""));
    else if (MSequence_EleAt(seq, 0)->Type != etString)
        return MExpr_CreateStringX(MString_NewC(""));

    MString format = MSequence_EleAt(seq, 0)->Str;
    MString r = MString_NewWithCapacity(1000);
    MInt i = 0;
    const MInt len = MString_Len(format);
    const MChar *p = MString_GetData(format);
    
    MSequence *mark = MSequence_Create(MSequence_Len(seq));

    // `n`
    // `.`
    // ``
    while (i < len)
    {
        MChar c = *p;
        if (c != '`')
        {
            MString_Cat(r, MString_Copy(format, i + 1, 1));
            i++; p++;
            continue;
        }
        else
        {
            i++; p++; 
            if (i >= len)
                break;

            c = *p;
            if (c == '.')
            {
                MString_Cat(r, MString_Copy(format, i, 1));
                i += 2; p += 2;
                continue;
            }
            else if (c == '`')
            {
                int cur = 1;
                while (cur < MSequence_Len(mark))
                {
                    if (NULL == MSequence_EleAt(mark, cur))
                        break;
                    else
                        cur++;
                }
                
                if ((cur < MSequence_Len(mark)) && (NULL != MSequence_EleAt(mark, cur)))
                {
                    MString_Cat(r, MExpr_ToFullForm(MSequence_EleAt(seq, cur)));
                    MSequence_SetAtX(mark, cur, MExpr_CreateSymbol(sym_Null));
                }
                else
                {
                    MString si = MString_Copy(format , i, 2);
                    MString_Cat(r, si);
                    MString_Release(si);
                }

                i++; p++;
                continue;
            }
            else if (IsNumChar(c))
            {
                int pos = i;
                MString si;
                int index;

                while (i < len)
                {
                    if (*p == '`')
                        break;
                    i++; p++;
                }
                
                si = MString_Copy(format , pos + 1, i - pos);
                index = ParseMachineInt(si);
                MString_Release(si);
                if (index < MSequence_Len(seq))
                {
                    MString_Cat(r, MExpr_ToFullForm(MSequence_EleAt(seq, index)));
                    MSequence_SetAtX(mark, index, MExpr_CreateSymbol(sym_Null));
                }
                else
                {
                    si = MString_Copy(format , pos, i - pos + 2);
                    MString_Cat(r, si);
                    MString_Release(si);
                }
                i++; p++;
            }
        }
    }

    MSequence_Release(mark);

    return MExpr_CreateStringX(r);
}

void BIF_Message(MSymbol sym, MExpr expr, MSequence *seq, _MCONTEXT_)
{
    MExpr r = MSymbol_QueryMsg(expr);
    MString msg = (r != NULL) && (r->Type == etString) ? r->Str : NULL;

    if (msg)
    {
        MSequence *s = MSequence_Create(MSequence_Len(seq) + 1);
        int i;

        MSequence_SetAt(s, 0, r);
        for (i = 0; i < MSequence_Len(seq); i++)
            MSequence_SetAt(s, i + 1, MSequence_EleAt(seq, i));

        MExpr res = BIF_StringForm(s, _Context);
        if (res->Type == etString)
        {
            Msg_Emit(sym->Name);
            Msg_Emit("::");
            Msg_Emit(expr->HeadSeq.pSequence->pExpr[0]->Str);
            Msg_Emit(": ");
            Msg_EmitLn(res->Str);
        }
        else
            Msg_EmitLn("Message::internal: StringFrom does not return a String.");
        MSequence_Release(s);
        MExpr_Release(res);
    }
}

void BIF_Message1(MSymbol sym, MExpr expr, MExpr p1, _MCONTEXT_)
{
    MSequence ts;
    MSequence *s = MSequence_InitStatic(&ts, 1);
    MSequence_SetAt(s, 0, p1);
    BIF_Message(sym, expr, s, _Context);
    MSequence_ReleaseStatic(s);
}

void BIF_Message2(MSymbol sym, MExpr expr, MExpr p1, MExpr p2, _MCONTEXT_)
{
    MSequence ts;
    MSequence *s = MSequence_InitStatic(&ts, 2);
    MSequence_SetAt(s, 0, p1);
    MSequence_SetAt(s, 1, p2);
    BIF_Message(sym, expr, s, _Context);
    MSequence_ReleaseStatic(s);
}

void BIF_Message3(MSymbol sym, MExpr expr, MExpr p1, MExpr p2, MExpr p3, _MCONTEXT_)
{
    MSequence ts;
    MSequence *s = MSequence_InitStatic(&ts, 3);
    MSequence_SetAt(s, 0, p1);
    MSequence_SetAt(s, 1, p2);
    MSequence_SetAt(s, 2, p3);
    BIF_Message(sym, expr, s, _Context);
    MSequence_ReleaseStatic(s);
}

void BIF_Message4(MSymbol sym, MExpr expr, MExpr p1, MExpr p2, MExpr p3, MExpr p4, _MCONTEXT_)
{
    MSequence *s = MSequence_Create(4);
    MSequence_SetAt(s, 0, p1);
    MSequence_SetAt(s, 1, p2);
    MSequence_SetAt(s, 2, p3);
    MSequence_SetAt(s, 3, p4);
    BIF_Message(sym, expr, s, _Context);
    MSequence_Release(s);
}

/*
static MExpr BIF_Message(MSymbol sym, MExpr expr)
{
    MExpr r = MSymbol_QueryMsg(expr);
    MString msg = r->Type == etString ? r->Str : NULL;

    if (msg)
    {
        Msg_Emit(sym->Name);
        Msg_Emit("::");
        Msg_Emit(expr->HeadSeq.pSequence->pExpr[0]->Str);
        Msg_Emit(": ");
        Msg_EmitLn(msg);
    }

    return NULL;
}
*/
def_bif(Message)
{
    static MString sNoMessage = MString_NewC("-- Message text not found --");

    if ((MSequence_Len(seq) > 0) 
        && (seq->pExpr[0]->Type == etHeadSeq) 
        && (is_header(seq->pExpr[0], sym_MessageName))
        && (   ((MSequence_Len(seq->pExpr[0]->HeadSeq.pSequence) == 2)
                && (seq->pExpr[0]->HeadSeq.pSequence->pExpr[0]->Type == etSymbol)
                && (seq->pExpr[0]->HeadSeq.pSequence->pExpr[1]->Type == etString))
            || ((MSequence_Len(seq->pExpr[0]->HeadSeq.pSequence) == 3)
                && (seq->pExpr[0]->HeadSeq.pSequence->pExpr[0]->Type == etSymbol)
                && (seq->pExpr[0]->HeadSeq.pSequence->pExpr[1]->Type == etString)
                && (seq->pExpr[0]->HeadSeq.pSequence->pExpr[2]->Type == etString))))
    {
        MString lang = MSequence_Len(seq->pExpr[0]->HeadSeq.pSequence) == 2 ?
            MString_NewC("") : MString_NewS(seq->pExpr[0]->HeadSeq.pSequence->pExpr[2]->Str);

        MExpr r = MSymbol_QueryMsg(seq->pExpr[0]->HeadSeq.pSequence->pExpr[0]->Symbol,
                                   seq->pExpr[0]->HeadSeq.pSequence->pExpr[1]->Str,
                                   lang);
        MString msg = NULL;
        if (r == NULL)
            r = MSymbol_QueryMsg(sym_General,
                                 seq->pExpr[0]->HeadSeq.pSequence->pExpr[1]->Str,
                                 lang);
        else;
        MString_Release(lang);
        if (r == NULL)
            msg = sNoMessage;
        else if (r->Type == etString)
            msg = r->Str;
        else;

        if (msg)
        {
            MSequence *s = MSequence_UniqueCopy(seq, 0, MSequence_Len(seq));
            MSequence_SetAtX(s, 0, MExpr_CreateString(msg));
            MExpr res = BIF_StringForm(s, _Context);
            if (res->Type == etString)
            {
                Msg_Emit(seq->pExpr[0]->HeadSeq.pSequence->pExpr[0]->Symbol->Name);
                Msg_Emit("::");
                Msg_Emit(seq->pExpr[0]->HeadSeq.pSequence->pExpr[1]->Str);
                Msg_Emit(": ");
                Msg_EmitLn(res->Str);
            }
            else
                Msg_EmitLn("Message::internal: StringFrom does not return a String.");
            MExpr_Release(res);
        }
    }
    else
        BIF_Message(sym_Message, msg_MessageName, seq, _Context);

    return MExpr_CreateSymbol(sym_Null);
}

def_bif(Print)
{
    MInt i;
    MString r = MString_NewWithCapacity(1024);
    for (i = 0; i < MSequence_Len(seq); i++)
    {
        if (MSequence_EleAt(seq, i)->Type != etString)
        {
            MString s2 = MExpr_ToFullForm(MSequence_EleAt(seq, i));
            MString_Cat(r, s2);
            MString_Release(s2);
        }
        else
            MString_Cat(r, MSequence_EleAt(seq, i)->Str);
    }

    MString_CatChar(r, '\n');
    K_Output(0, r);
    MString_Release(r);
    return MExpr_CreateSymbol(sym_Null);
}

void BIF_Print0(MExpr expr, _MCONTEXT_)
{
    MSequence *ss = MSequence_Create(1);
    MSequence_SetAt(ss, 0, expr);
    MExpr_Release(BIF_Print(ss, _Context));
    MSequence_Release(ss);
}

def_bif(Complex)
{
    verify_arity_with_r(Complex, 2);

    if (is_exact_zero(MSequence_EleAt(seq, 1)))
    {
        MExpr r = MSequence_EleAt(seq, 0);
        XRef_IncRef(r);
        return r;
    }
    else
    {
        if ((MSequence_EleAt(seq, 0)->Type == etNum)
            && (MSequence_EleAt(seq, 1)->Type == etNum))
            return MExpr_CreateComplex(MSequence_EleAt(seq, 0)->Num, MSequence_EleAt(seq, 1)->Num);
        else
            return MExpr_CreateHeadSeq(sym_Complex, seq);
    }
}

def_bif(Rational)
{
    verify_arity_with_r(Rational, 2);

    if ((MSequence_EleAt(seq, 0)->Type == etNum)
        && (MSequence_EleAt(seq, 1)->Type == etNum))
        return MExpr_CreateRational(MSequence_EleAt(seq, 0)->Num, MSequence_EleAt(seq, 1)->Num);
    else
        return MExpr_CreateHeadSeq(sym_Complex, seq);
}

bool BIF_SameQ(MExpr expr1, MExpr expr2)
{
    return IsExprSameQ(expr1, expr2);
}

bool BIF_UnsameQ(MExpr expr1, MExpr expr2)
{
    return IsExprUnsameQ(expr1, expr2);
}

static bool BIF_Equal(MExpr expr1, MExpr expr2)
{
    bool r = false;
    int i;
    
    if ((expr1->Type == etNum) && (expr2->Type == etNum))
        return Num_E(expr1->Num, expr2->Num);
    else;

//    if (MExpr_Hash(expr1) != MExpr_Hash(expr2))
//        return false;

    if (expr1->Type != expr2->Type)
        return false;
    
    switch (expr1->Type)
    {
    case etString:       return MString_SameQ(expr1->Str, expr2->Str); break;
    case etComplex:      return Num_E(expr1->Complex.Real, expr2->Complex.Real) &&
                                Num_E(expr1->Complex.Imag, expr2->Complex.Imag); break;
    case etCons:         return BIF_Equal(expr1->Cons.Car, expr2->Cons.Car) &&
                                BIF_Equal(expr1->Cons.Cdr, expr2->Cons.Cdr); 
                         break;
    case etSymbol:       return (expr1->Symbol->Context == expr2->Symbol->Context) &&
                                MString_SameQ(expr1->Symbol->Name, expr2->Symbol->Name); break;
    case etHeadSeq:     r = (MSequence_Len(expr1->HeadSeq.pSequence) == MSequence_Len(expr2->HeadSeq.pSequence))
                            && BIF_Equal(expr1->HeadSeq.Head, expr2->HeadSeq.Head);
                        if (!r)
                            return r;
                        for (i = 0; i < MSequence_Len(expr1->HeadSeq.pSequence); i++)
                        {
                            r = BIF_Equal(expr1->HeadSeq.pSequence->pExpr[i],
                                          expr2->HeadSeq.pSequence->pExpr[i]);
                            if (!r)
                                return r;
                        }
                        break;
    default:
            ASSERT("no default" == NULL);
    }

    return r;
}

static bool  BIF_LessEqual(MExpr expr1, MExpr expr2)
{
    ASSERT(expr1->Type == etNum);
    ASSERT(expr2->Type == etNum);
    return Num_LTE(expr1->Num, expr2->Num);
}

static bool  BIF_Greater(MExpr expr1, MExpr expr2)
{
    ASSERT(expr1->Type == etNum);
    ASSERT(expr2->Type == etNum);
    return Num_GT(expr1->Num, expr2->Num);
}

static bool  BIF_GreaterEqual(MExpr expr1, MExpr expr2)
{
    ASSERT(expr1->Type == etNum);
    ASSERT(expr2->Type == etNum);
    return Num_GTE(expr1->Num, expr2->Num);
}

static bool BIF_Less(MExpr expr1, MExpr expr2)
{
    ASSERT(expr1->Type == etNum);
    ASSERT(expr2->Type == etNum);
    return Num_LT(expr1->Num, expr2->Num);
}

def_bif(Less)
{
    int i;

    if (MSequence_Len(seq) <= 1)
        return MExpr_CreateSymbol(sym_True);

    if (!is_int_real(MSequence_EleAt(seq, 0)))
        return MExpr_CreateHeadSeq(sym_Less, seq);

    for (i = 1; i < MSequence_Len(seq); i++)
    {
        if (is_int_real(MSequence_EleAt(seq, i)))
        {
            if (!BIF_Less(MSequence_EleAt(seq, i - 1), MSequence_EleAt(seq, i)))
                return MExpr_CreateSymbol(sym_False);
        }
        else
            return MExpr_CreateHeadSeqX(sym_Less, MSequence_Copy(seq, i - 1, MSequence_Len(seq) - i + 1));
    }

    return MExpr_CreateSymbol(sym_True);
}

def_bif(LessEqual)
{
    int i;

    if (MSequence_Len(seq) <= 1)
        return MExpr_CreateSymbol(sym_True);

    if (!is_int_real(MSequence_EleAt(seq, 0)))
        return MExpr_CreateHeadSeq(sym_LessEqual, seq);

    for (i = 1; i < MSequence_Len(seq); i++)
    {
        if (is_int_real(MSequence_EleAt(seq, i)))
        {
            if (!BIF_LessEqual(MSequence_EleAt(seq, i - 1), MSequence_EleAt(seq, i)))
                return MExpr_CreateSymbol(sym_False);
        }
        else
            return MExpr_CreateHeadSeqX(sym_LessEqual, MSequence_Copy(seq, i - 1, MSequence_Len(seq) - i + 1));
    }

    return MExpr_CreateSymbol(sym_True);
}

def_bif(Greater)
{
    int i;

    if (MSequence_Len(seq) <= 1)
        return MExpr_CreateSymbol(sym_True);

    if (!is_int_real(MSequence_EleAt(seq, 0)))
        return MExpr_CreateHeadSeq(sym_Greater, seq);

    for (i = 1; i < MSequence_Len(seq); i++)
    {
        if (is_int_real(MSequence_EleAt(seq, i)))
        {
            if (!BIF_Greater(MSequence_EleAt(seq, i - 1), MSequence_EleAt(seq, i)))
                return MExpr_CreateSymbol(sym_False);
        }
        else
            return MExpr_CreateHeadSeqX(sym_Greater, MSequence_Copy(seq, i - 1, MSequence_Len(seq) - i + 1));
    }

    return MExpr_CreateSymbol(sym_True);
}

def_bif(GreaterEqual)
{
    int i;

    if (MSequence_Len(seq) <= 1)
        return MExpr_CreateSymbol(sym_True);

    if (!is_int_real(MSequence_EleAt(seq, 0)))
        return MExpr_CreateHeadSeq(sym_GreaterEqual, seq);

    for (i = 1; i < MSequence_Len(seq); i++)
    {
        if (is_int_real(MSequence_EleAt(seq, i)))
        {
            if (!BIF_GreaterEqual(MSequence_EleAt(seq, i - 1), MSequence_EleAt(seq, i)))
                return MExpr_CreateSymbol(sym_False);
        }
        else
            return MExpr_CreateHeadSeqX(sym_GreaterEqual, MSequence_Copy(seq, i - 1, MSequence_Len(seq) - i + 1));
    }

    return MExpr_CreateSymbol(sym_True);
}

def_bif(SameQ)
{
    int i;

    if (MSequence_Len(seq) <= 1)
        return MExpr_CreateSymbol(sym_True);

    for (i = 0; i < MSequence_Len(seq) - 1; i++)
        if (!BIF_SameQ(MSequence_EleAt(seq, i), MSequence_EleAt(seq, i + 1)))
            return MExpr_CreateSymbol(sym_False);

    return MExpr_CreateSymbol(sym_True);
}

def_bif(UnsameQ)
{
    int i;

    if (MSequence_Len(seq) <= 1)
        return MExpr_CreateSymbol(sym_True);

    if (MSequence_Len(seq) == 2)
        return BIF_UnsameQ(MSequence_EleAt(seq, 0), MSequence_EleAt(seq, 1)) ?
                MExpr_CreateSymbol(sym_True) : MExpr_CreateSymbol(sym_False);
    else
    {
        for (i = 0; i < MSequence_Len(seq) - 1; i++)
        {
            int j;
            for (j = i + 1; j < MSequence_Len(seq); j++)
                if (BIF_SameQ(MSequence_EleAt(seq, i), MSequence_EleAt(seq, j)))
                    return MExpr_CreateSymbol(sym_False);
        }

        return MExpr_CreateSymbol(sym_True);
    }
}

def_bif(Order)
{
    MExpr r = verify_arity(seq, sym_Order, 2);
    MInt cmp;
    if (r)
        return r;

    cmp = Eval_ExprCmp(MSequence_EleAt(seq, 0), MSequence_EleAt(seq, 1));
    if (cmp > 0)
        return MExpr_CreateMachineInt(-1);
    else if (cmp < 0)
        return MExpr_CreateMachineInt(1);
    else
        return MExpr_CreateMachineInt(0);
}

def_bif(Equal)
{
    int i;

    if (MSequence_Len(seq) <= 1)
        return MExpr_CreateSymbol(sym_True);

    for (i = 0; i < MSequence_Len(seq) - 1; i++)
        if (!BIF_Equal(MSequence_EleAt(seq, i), MSequence_EleAt(seq, i + 1)))
            return MExpr_CreateSymbol(sym_False);

    return MExpr_CreateSymbol(sym_True);
}

def_bif(Unequal)
{
    int i;

    if (MSequence_Len(seq) <= 1)
        return MExpr_CreateSymbol(sym_True);

    for (i = 0; i < MSequence_Len(seq) - 1; i++)
    {
        int j;
        for (j = i + 1; j < MSequence_Len(seq); j++)
            if (BIF_Equal(MSequence_EleAt(seq, i), MSequence_EleAt(seq, j)))
                return MExpr_CreateSymbol(sym_False);
    }

    return MExpr_CreateSymbol(sym_True);
}

MSymbol BIF_Set_getsym(MExpr target)
{
    if (target->Type == etSymbol)
        return target->Symbol;
    if (target->Type == etHeadSeq)
    {
        MExpr t = target->HeadSeq.Head;
        while (t->HeadSeq.Head->Type == etHeadSeq)
            t = t->HeadSeq.Head->HeadSeq.Head;
        return t->Type == etSymbol ? t->Symbol : NULL;
    }
    else
        return NULL;
}

static void on_hash_ele(MExprList *lst, MExpr item)
{
    MSequence *s1 = MSequence_Create(1);
    MSequence *s2 = MSequence_Create(2);
    MSequence_SetAt (s1, 0, MSequence_EleAt(item->HeadSeq.pSequence, 0));
    MSequence_SetAtX(s2, 0, MExpr_CreateHeadSeqX(sym_HoldPattern, s1));
    MSequence_SetAt (s2, 1, MSequence_EleAt(item->HeadSeq.pSequence, 1));
    MExprList_PushOrphan(lst, MExpr_CreateHeadSeqX(sym_RuleDelayed, s2));
}

def_bif(DownValues)
{
    MExprList *lst;
    MSequence *s;
    MSymbol sym;
    if (!check_arg_types("M", seq, sym_DownValues, true, _CONTEXT))
        return MExpr_CreateHeadSeq(sym_DownValues, seq);
    else;

    sym = MSequence_EleAt(seq, 0)->Symbol;
    lst = MExprList_Create();
    if (sym->ExactDownValues != NULL)
        MHashTable_Traverse(sym->ExactDownValues, (f_HashTraverse)(on_hash_ele), lst);
    else;

    if (sym->PatternDownValues != NULL)
    {
        int i;
        for (i = 0; i < MSequence_Len(sym->PatternDownValues); i++)
            MExprList_Append(lst, MSequence_EleAt(sym->PatternDownValues, i));
    }
    else;

    s = MSequence_FromExprList(lst);
    MExprList_Release(lst);
    return MExpr_CreateHeadSeqX(sym_List, s);
}

def_bif(Unset)
{
    MExpr r = verify_arity(seq, sym_Unset, 1);
    if (r)
        return r;
    r = MSequence_EleAt(seq, 0);
    if ((r->Type == etSymbol) && (r->Symbol->Immediate))
    {
        MExpr_Release(r->Symbol->Immediate);
        r->Symbol->Immediate = NULL;
    }
    else if ((r->Type == etHeadSeq) && is_header(r, sym_List))
    {
        MSequence *s = r->HeadSeq.pSequence;
        MInt i;
        for (i = 0; i < MSequence_Len(s); i++)
        {
            MExpr t = MSequence_EleAt(s, i);
            if ((t->Type == etSymbol) && (t->Symbol->Immediate))
            {
                MExpr_Release(t->Symbol->Immediate);
                t->Symbol->Immediate = NULL;
            }
        }
    }
    else;

    return MExpr_CreateSymbol(sym_Null);
}

#define handle_body(body, r) \
    if (is_sym_header_sym(body, sym_Slot))                                                                                   \
    {                                                                                                                \
        if (MSequence_Len(body->HeadSeq.pSequence) == 0)                                                             \
        {                                                                                                            \
            r = MSequence_EleAt(para, 0);                                                                            \
            XRef_IncRef(r);                                                                                          \
        }                                                                                                            \
        else if (MSequence_Len(body->HeadSeq.pSequence) == 1)                                                        \
        {                                                                                                            \
            MExpr v = MSequence_EleAt(body->HeadSeq.pSequence, 0);                                                   \
            if (is_int(v))                                                                                           \
            {                                                                                                        \
                MInt n = to_int(v);                                                                                  \
                                                                                                                     \
                if (n > 0)                                                                                           \
                {                                                                                                    \
                    if (n <= MSequence_Len(para))                                                                    \
                    {                                                                                                \
                        r = MSequence_EleAt(para, n - 1);                                                            \
                        XRef_IncRef(r);                                                                              \
                    }                                                                                                \
                }                                                                                                    \
                else if (n < 0)                                                                                      \
                {                                                                                                    \
                    if (-n <= MSequence_Len(para))                                                                   \
                    {                                                                                                \
                        r = MSequence_EleAt(para, MSequence_Len(para) + n);                                          \
                        XRef_IncRef(r);                                                                              \
                    }                                                                                                \
                }                                                                                                    \
                else                                                                                                 \
                {                                                                                                    \
                    XRef_IncRef(Self);                                                                               \
                    r = Self;                                                                                        \
                }                                                                                                    \
            }                                                                                                        \
            else;                                                                                                    \
        }                                                                                                            \
        else;                                                                                                        \
    }                                                                                                                \
    else if (is_sym_header_sym(body, sym_SlotSequence))                                                                      \
    {                                                                                                                \
        if (MSequence_Len(body->HeadSeq.pSequence) == 0)                                                             \
            r = MExpr_CreateHeadSeq(sym_Sequence, para);                                                             \
        else if (MSequence_Len(body->HeadSeq.pSequence) == 1)                                                        \
        {                                                                                                            \
            if (is_int(MSequence_EleAt(body->HeadSeq.pSequence, 0)))                                                 \
            {                                                                                                        \
                MInt n = to_int(MSequence_EleAt(body->HeadSeq.pSequence, 0));                                        \
                if (n > 0)                                                                                           \
                {                                                                                                    \
                    if (n <= MSequence_Len(para))                                                                    \
                        r = MExpr_CreateHeadSeqX(sym_Sequence, MSequence_Copy(para, n - 1, MSequence_Len(para)));    \
                }                                                                                                    \
                else if (n < 0)                                                                                      \
                {                                                                                                    \
                    if (-n <= MSequence_Len(para))                                                                   \
                        r = MExpr_CreateHeadSeqX(sym_Sequence, MSequence_Copy(para, 0, -n));                         \
                }                                                                                                    \
                else;                                                                                                \
            }                                                                                                        \
        }                                                                                                            \
        else;                                                                                                        \
    }

#define is_slot_pure_fun(fun)  (is_sym_header_sym((fun), sym_Function) && (MSequence_Len((fun)->HeadSeq.pSequence) == 1))

static MExpr PureFunSubstitute(MExpr body, MSequence *para, MExpr Self)
{
    MExpr r = NULL;
    if (MSequence_Len(para) < 1)
    {
        return NULL;
    }

    if (body->Type == etHeadSeq)
    {
        if (body->HeadSeq.Head->Type == etSymbol)
        {
            if (is_slot_pure_fun(body))
            {
                XRef_IncRef(body);
                return body;
            }
            else;

            handle_body(body, r);
        }
        else;
        
        if (r == NULL)
        {
            MSequence *news = NULL;
            MInt i;
            MExpr t;

            for (i = 0; i < MSequence_Len(body->HeadSeq.pSequence); i++)
            {
                t = MSequence_EleAt(body->HeadSeq.pSequence, i);
                if (t->Type == etHeadSeq)
                {
                    r = NULL;
                    if (t->HeadSeq.Head->Type == etSymbol)
                    {
                        if (!is_slot_pure_fun(t))
                        {
                            handle_body(t, r)
                            else
                                r = PureFunSubstitute(t, para, Self);
                        }
                        else;
                    }
                    else
                        r = PureFunSubstitute(t, para, Self);
                    
                    if (r)
                    {
                        if (news == NULL)
                            news = MSequence_Uniquelize(body->HeadSeq.pSequence);
                        MSequence_SetAtX(news, i, r);
                    }
                    else;
                }  
            }

            r = body->HeadSeq.Head;
            t = NULL;
            if (r->Type == etHeadSeq)
            {
                if (r->HeadSeq.Head->Type == etSymbol) 
                {
                    if (!is_slot_pure_fun(r))
                    {
                        handle_body(r, t)
                        else
                            t = PureFunSubstitute(r, para, Self);
                    }
                    else;
                }
                else
                    t = PureFunSubstitute(r, para, Self);
            }
            else;

            if (t)
            {
                if (news)
                    return MExpr_CreateHeadXSeqX(t, news);
                else
                {
                    r = MExpr_CreateHeadSeq(t, body->HeadSeq.pSequence);
                    MExpr_Release(t);
                    return r;
                }
            }
            else
            {
                if (news)
                    return MExpr_CreateHeadSeqX(body->HeadSeq.Head, news);
                else
                    return NULL;
            }
        }
    }
    else
    {
        XRef_IncRef(body);
        return body;
    }

    return r;
}

static MExpr FunctionReplaceAll(MExpr expr, MSymbol from, MExpr to);

static MSequence *fun_replace_seq(MSequence *seq, MSymbol from, MExpr to, int i)
{
    MExpr newe = NULL;
    MSequence *s = NULL;
    const int L = MSequence_Len(seq);
    for (; i < L; i++)
    {
        MExpr olde = MSequence_EleAt(seq, i);            
        switch (olde->Type)
        {
        case etSymbol:
            if (olde->Symbol == from)
            {
                newe = to;
                XRef_IncRef(to);
            }
            break; 
        case etHeadSeq:
            newe = FunctionReplaceAll(olde, from, to);
            if (newe == olde)
            {
                MExpr_Release(newe);
                newe = NULL;
            }
            else;
        default:
            ;
        }
        if (newe) break;
    }

    if (newe)
    {
        s = MSequence_Create(L);
        if (i > 0) MSequence_OverwriteSetFrom(s, seq, 0, 0, i);
        MSequence_SetAtX(s, i, newe); i++;

        for (; i < L; i++)
        {
            MExpr olde = MSequence_EleAt(seq, i);     
            switch (olde->Type)
            {
            case etSymbol:
                if (olde->Symbol == from)
                    MSequence_SetAt(s, i, to);
                else
                    MSequence_SetAt(s, i, olde);
                break; 
            case etHeadSeq:
                MSequence_SetAtX(s, i, FunctionReplaceAll(olde, from, to));
                break;
            default:
                MSequence_SetAt(s, i, olde);
            }
        }
    }

    return s;
}

static MExpr FunctionReplaceAll(MExpr expr, MSymbol from, MExpr to)
{
    switch (expr->Type)
    {
    case etSymbol:
        if (expr->Symbol == from)
        {
            XRef_IncRef(to);
            return to;
        }
        break;
    case etHeadSeq:
        {
            MExpr h;
            MSequence *s;
            MInt i = 0;
            MExpr newe;
            MExpr olde;

            if (is_header(expr, sym_Function))
            {
                // CAUTION: parameter name collision
                // TODO: optimize: Hash param list?
                if (MSequence_Len(expr->HeadSeq.pSequence) == 2)
                {
                    MExpr arg = get_seq_i(expr, 0);
                    if ((arg->Type == etHeadSeq) && (is_header(arg, sym_List)))
                    {
                        for (i = 0; i < MSequence_Len(get_seq(arg)); i++)
                        {
                            if ((get_seq_i(arg, i)->Type == etSymbol)
                                    && (get_seq_i(arg, i)->Symbol == from))
                                goto no_match;
                        }
                    }
                    i = 1;
                }
            }

            // handle head
            olde = expr->HeadSeq.Head;
            h = NULL;
            if (olde->Type == etSymbol)
            {
                if (olde->Symbol == from)
                {
                    h = to;
                    XRef_IncRef(to);
                }
            }
            else if (olde->Type == etHeadSeq)
                h = FunctionReplaceAll(olde, from, to);
            
            s = fun_replace_seq(expr->HeadSeq.pSequence, from, to, i);

            if (s)
                return h ? MExpr_CreateHeadXSeqX(h, s) : MExpr_CreateHeadSeqX(expr->HeadSeq.Head, s);
            else
            {
                if (h)
                {
                    newe = MExpr_CreateHeadSeq(h, expr->HeadSeq.pSequence);
                    MExpr_Release(h);
                    return newe;
                }
                else;
            }
        }
        break;
    default:
        ;
    }

no_match:
    XRef_IncRef(expr);
    return expr;
}

static MExpr FunctionReplaceAll2(MExpr expr, MSymbol from, MExpr to)
{
    switch (expr->Type)
    {
    case etSymbol:
        if (expr->Symbol == from) return MExpr_SetTo(expr, to);
        break;
    case etHeadSeq:
        {
            MExpr h;
            MSequence *s;
            MInt i = 0;
            bool bDirty = false;

            if (is_header(expr, sym_Function))
            {
                // CAUTION: parameter name collision
                // TODO: optimize: Hash param list?
                if (MSequence_Len(expr->HeadSeq.pSequence) == 2)
                {
                    MExpr arg = get_seq_i(expr, 0);
                    if ((arg->Type == etHeadSeq) && (is_header(arg, sym_List)))
                    {
                        for (i = 0; i < MSequence_Len(get_seq(arg)); i++)
                            if ((get_seq_i(arg, i)->Type == etSymbol)
                                    && (get_seq_i(arg, i)->Symbol == from))
                                goto no_match;
                    }
                    i = 1;
                }
            }

            {
                MExpr olde = expr->HeadSeq.Head;
                if (olde->Type == etSymbol)
                {
                    if (olde->Symbol == from)
                    {
                        MExpr_Release(olde);
                        expr->HeadSeq.Head = to;
                        bDirty = true;
                    }
                }
                else if (olde->Type == etHeadSeq)
                {
                    if (MObj_IsUnique(olde))
                    {
                        if (FunctionReplaceAll2(olde, from, to)) bDirty = true;
                    }
                    else
                    {
                        h = FunctionReplaceAll(olde, from, to);
                        MExpr_Release(olde);
                        expr->HeadSeq.Head = h;
                        bDirty = true;
                    }
                }
            }

            if (MObj_IsUnique(expr->HeadSeq.pSequence))
            {
                MSequence *s = expr->HeadSeq.pSequence;
                for (; i < MSequence_Len(get_seq(expr)); i++)
                {
                    MExpr olde = get_seq_i(expr, i);            
                    switch (olde->Type)
                    {
                    case etSymbol:
                        if (olde->Symbol == from) 
                        {
                            MSequence_SetAt(s, i, to);
                            bDirty = true;
                        }
                        break; 
                    case etHeadSeq:
                        if (MObj_IsUnique(olde))
                        {
                            if (FunctionReplaceAll2(olde, from, to)) bDirty = true;
                        }
                        else
                        {
                            h = FunctionReplaceAll(olde, from, to);
                            MSequence_SetAtX(s, i, h);
                            bDirty = true;
                        }
                        break;
                    default:
                        ;
                    }
                }
            }
            else
            {
                s = fun_replace_seq(expr->HeadSeq.pSequence, from, to, i);
                if (s)
                {
                    MExpr_Release(expr->HeadSeq.pSequence);
                    expr->HeadSeq.pSequence = s;
                    bDirty = true;
                }
                else;
            }

            return bDirty ? expr : NULL;
        }
        break;
    default:
        ;
    }

no_match:
    return NULL;
}

// TODO: optimize: create a template for pure function, then fill para into the template ==> result?
static MExpr BIF_CallPureFun(MExpr expr)
{
    MExpr fun = expr->HeadSeq.Head;
    MSequence *para = expr->HeadSeq.pSequence;

    if (MSequence_Len(fun->HeadSeq.pSequence) == 1)
    {
        MExpr body = MSequence_EleAt(fun->HeadSeq.pSequence, 0);
        MExpr r = PureFunSubstitute(body, para, fun);
        if (NULL == r)
        {
    //        r = body;
    //        XRef_IncRef(r);
        }
        return r;
    }
    else if (MSequence_Len(fun->HeadSeq.pSequence) == 2)
    {
        MInt i;
        MExpr arg = MSequence_EleAt(fun->HeadSeq.pSequence, 0);
        MExpr body = MSequence_EleAt(fun->HeadSeq.pSequence, 1);

        if ((arg->Type == etHeadSeq) && (is_header(arg, sym_List)))
        {
            MExpr newBody;

            if (MSequence_Len(get_seq(arg)) > MSequence_Len(para))
                goto quit;

            /*
            printf("%d, %d, %d, %d\n", MObj_RefCnt(expr),
                                       MObj_RefCnt(fun),
                                       MObj_RefCnt(fun->HeadSeq.pSequence),
                                       MObj_RefCnt(body));
                                       */
            if (MObj_IsUnique(expr) && MObj_IsUnique(fun) 
                    && MObj_IsUnique(fun->HeadSeq.pSequence) 
                    && MObj_IsUnique(body))
            {
               // printf("hit ..\n");
                newBody = body;
                for (i = 0; i < MSequence_Len(get_seq(arg)); i++)
                {
                    if (get_seq_i(arg, i)->Type == etSymbol)
                        FunctionReplaceAll2(body, get_seq_i(arg, i)->Symbol, MSequence_EleAt(para, i));
                    else
                    {
                        // it's impossible for us to rollback
                        XRef_IncRef(body);
                        goto quit;
                    }
                }
                XRef_IncRef(body);
            }
            else
            {
                newBody = body; XRef_IncRef(body);
                for (i = 0; i < MSequence_Len(get_seq(arg)); i++)
                {
                    if (get_seq_i(arg, i)->Type == etSymbol)
                    {
                        newBody = FunctionReplaceAll(body, get_seq_i(arg, i)->Symbol, MSequence_EleAt(para, i));
                        MExpr_Release(body);
                        body = newBody;
                    }
                    else
                    {
                        MExpr_Release(body);
                        goto quit;
                    }
                }
            }

            return body;
        }
    }
    else;

quit:
    return NULL;
}

MExpr BIF_CallFun(MExpr expr, _MCONTEXT_)
{
    MExpr f = expr->HeadSeq.Head;
    MSymbol sym;
    MSequence *s = NULL;
    MExpr r = NULL;

    if ((f->Type == etHeadSeq) && is_header(f, sym_Function))
        return BIF_CallPureFun(expr);

    sym = BIF_Set_getsym(f);

    if (sym == NULL)
        return NULL;

    if (sym->ExactDownValues != NULL)
    {
        s = MSequence_Create(2);
        MSequence_SetAt(s, 0, expr);

        MExpr t = MExpr_CreateHeadSeq(sym_Rule, s);
        MExpr def = (MExpr)MHashTable_Find(sym->ExactDownValues, t);
        MExpr_Release(t);
        if (def)
        {
            r = MSequence_EleAt(def->HeadSeq.pSequence, 1);
            XRef_IncRef(r);            
            goto quit;
        }

        t = MExpr_CreateHeadSeq(sym_RuleDelayed, s);
        def = (MExpr)MHashTable_Find(sym->ExactDownValues, t);
        MExpr_Release(t);
        if (def)
        {
            r = MSequence_EleAt(def->HeadSeq.pSequence, 1);
            XRef_IncRef(r);            
            goto quit;
        }       
    }
    else;

    if (sym->PatternDownValues)
    {
    }

quit:
    
    if (s) MSequence_Release(s); 
    return r;
}

static MExpr BIF_Inequality(MExpr expr1, MSymbol op, MExpr expr2, _MCONTEXT_)
{
    MExpr r;
    MSequence *s = MSequence_Create(2);
    MSequence_SetAt(s, 0, expr1);
    MSequence_SetAt(s, 1, expr2);

    if (op == sym_SameQ)
        r = BIF_SameQ(s, _Context);
    else if (op == sym_UnsameQ)
        r = BIF_UnsameQ(s, _Context);
    else if (op == sym_Equal)
        r = BIF_Equal(s, _Context);
    else if (op == sym_Unequal)
        r = BIF_Unequal(s, _Context);
    else if (op == sym_Less)
        r = BIF_Less(s, _Context);
    else if (op == sym_LessEqual)
        r = BIF_LessEqual(s, _Context);
    else if (op == sym_Greater)
        r = BIF_Greater(s, _Context);
    else if (op == sym_GreaterEqual)
        r = BIF_GreaterEqual(s, _Context);
    else
        r = MExpr_CreateHeadSeq(op, s);
    MSequence_Release(s);
    return r;
}

def_bif(Inequality)
{
    MInt len = MSequence_Len(seq);
    MInt i;
    MExprList *elist;
    MSequence *ps;

    if ((len < 3) || ((len & 0x1) == 0))
    {
        MSequence *ss = MSequence_Create(1);
        MSequence_SetAtX(ss, 0, MExpr_CreateMachineInt(len));
        BIF_Message(sym_Inequality, msg_Inequalityineq, ss, _Context);
        MSequence_Release(ss);
        return MExpr_CreateHeadSeq(sym_Inequality, seq);
    }

    elist = MExprList_Create();
    for (i = 0; i < len - 2; i += 2)
    {
        MExpr r = NULL;
        MExpr p1 = MSequence_EleAt(seq, i + 0);
        MExpr op = MSequence_EleAt(seq, i + 1);
        MExpr p2 = MSequence_EleAt(seq, i + 2);    
        if (op->Type == etSymbol)
        {
            r = BIF_Inequality(p1, op->Symbol, p2, _Context);
            if (is_false(r))
            {
                MExprList_Release(elist);
                return r;
            }
            else if (is_true(r))
            {
                MExpr_Release(r);
                continue;
            }
        }
        else
        {
            MSequence *para = MSequence_Copy(seq, i, 3);
            r = MExpr_CreateHeadSeqX(sym_Inequality, para);
        }

        MExprList_Append(elist, r);
    }

    if (MExprList_Len(elist) > 1)
    {
        ps = MSequence_FromExprList(elist);
        MExprList_Release(elist);
    
        return MExpr_CreateHeadSeqX(sym_And, ps);
    }
    else
    {
        MExpr t = MExprList_Pop(elist);
        MExprList_Release(elist);
    
        return t ? t : MExpr_CreateSymbol(sym_True);
    }
}

MExpr BIF_GetHead(MExpr para)
{
    switch (para->Type)
    {
    case etNum:
        return Num_IsInt(para->Num) ? MExpr_CreateSymbol(sym_Integer) : 
                (Num_IsRational(para->Num) ? MExpr_CreateSymbol(sym_Rational) : MExpr_CreateSymbol(sym_Real));
    case etComplex:
        return MExpr_CreateSymbol(sym_Complex);
    case etString:
        return MExpr_CreateSymbol(sym_String);
    case etSymbol:
        return MExpr_CreateSymbol(sym_Symbol);
    case etCons:
        return MExpr_CreateSymbol(sym_Cons);
    case etHeadSeq:
        XRef_IncRef(para->HeadSeq.Head);
        return para->HeadSeq.Head;
    default:
        ASSERT("BIF_GetHead(MExpr para): unknown type" == NULL);
        return NULL;
    }
}

def_bif(Head)
{
    MExpr r = verify_arity(seq, sym_Head, 1);
    if (r)
        return r;
    return BIF_GetHead(MSequence_EleAt(seq, 0));
}

def_bif(Length)
{
    MExpr r = verify_arity(seq, sym_Length, 1);
    if (r)
        return r;
    r = MSequence_EleAt(seq, 0);
    return MExpr_CreateMachineInt(r->Type == etHeadSeq ? MSequence_Len(r->HeadSeq.pSequence) : 0);
}

def_bif(AtomQ)
{
    MExpr r = verify_arity(seq, sym_AtomQ, 1);
    if (r)
        return r;
    r = MSequence_EleAt(seq, 0);
    return r->Type != etHeadSeq ? MExpr_CreateSymbol(sym_True) : MExpr_CreateSymbol(sym_False);
}

def_bif(If)
{
    MExpr cond;
    MExpr then = NULL;
    MExpr orelse = NULL;
    if ((MSequence_Len(seq) == 0) || (MSequence_Len(seq) > 3))
        return MExpr_CreateHeadSeq(sym_If, seq);

    cond = MSequence_EleAt(seq, 0);
    if (MSequence_Len(seq) > 1)
        then = MSequence_EleAt(seq, 1);
    if (MSequence_Len(seq) > 2)
        orelse = MSequence_EleAt(seq, 2);

    if (is_true(cond))
    {
        if (then)
        {
            XRef_IncRef(then);
            return then;
        }
        else
            return MExpr_CreateSymbol(sym_Null);
    }

    if (is_false(cond))
    {
        if (orelse)
        {
            XRef_IncRef(orelse);
            return orelse;
        }
        else
            return MExpr_CreateSymbol(sym_Null);
    }

    return MExpr_CreateHeadSeq(sym_If, seq);
}

def_bif(Not)
{
    MExpr r = verify_arity(seq, sym_Not, 1);
    if (r)
        return r;
    r = MSequence_EleAt(seq, 0);
    if (is_true(r))
        return MExpr_CreateSymbol(sym_False);
    else if (is_false(r))
        return MExpr_CreateSymbol(sym_True);
    else
    {
        XRef_IncRef(r);
        return r;
    }
}

def_bif(Evaluate)
{
    MExpr r = verify_arity(seq, sym_Evaluate, 1);
    if (r)
        return r;

    r = MSequence_EleAt(seq, 0);
    if ((r->Type == etHeadSeq) && (is_header(r, sym_Evaluate)))
        return BIF_Evaluate(r->HeadSeq.pSequence, _Context);
    else
    {
        XRef_IncRef(r);
        return r;
    }
}

static void BIF_Flatten_Header_1(MExprList *stack, MSequence *seq, MExpr header)
{
    MInt i;
    for (i = 0; i < MSequence_Len(seq); i++)
    {
        MExpr t = MSequence_EleAt(seq, i);
        if ((t->Type != etHeadSeq) || (IsExprUnsameQ(t->HeadSeq.Head, header)))
            MExprList_Push(stack, t);
        else
            BIF_Flatten_Header_1(stack, t->HeadSeq.pSequence, header);
    }
}

static MBool BIF_Flatten_Header(MExprList *stack, MSequence *seq, MExpr header)
{
    MInt i;
    MBool r = false;
    for (i = 0; i < MSequence_Len(seq); i++)
    {
        MExpr t = MSequence_EleAt(seq, i);
        if ((t->Type == etHeadSeq) && (IsExprSameQ(t->HeadSeq.Head, header)))
        {
            MInt j;
            for (j = 0; j < i; j++)
                MExprList_Push(stack, MSequence_EleAt(seq, j));
            r = true;
            BIF_Flatten_Header_1(stack, t->HeadSeq.pSequence, header);
            i++;
            break;
        }    
    }

    for (; i < MSequence_Len(seq); i++)
    {
        MExpr t = MSequence_EleAt(seq, i);
        if ((t->Type != etHeadSeq) || (IsExprUnsameQ(t->HeadSeq.Head, header)))
            MExprList_Push(stack, t);
        else
        {
            BIF_Flatten_Header_1(stack, t->HeadSeq.pSequence, header);
        }    
    }
    return r;
}

MSequence *BIF_Flatten_Header(MSequence *seq, MExpr header context_param)
{
    MExprList *stack = MExprList_Create();
    MBool found = BIF_Flatten_Header(stack, seq, header);
    if (found)
    {
        MSequence *r = MSequence_FromExprList(stack);
        MExprList_Release(stack);
        return r;
    }
    else
    {
        MExprList_Release(stack);
        return NULL;
    }
}

def_bif(First)
{
    verify_arity_with_r(First, 1);
    if ((MSequence_EleAt(seq, 0)->Type == etHeadSeq) 
        && (MSequence_Len(MSequence_EleAt(seq, 0)->HeadSeq.pSequence) >= 1))
    {
        r = MSequence_EleAt(MSequence_EleAt(seq, 0)->HeadSeq.pSequence, 0);
        XRef_IncRef(r);
        return r;
    }

    return MExpr_CreateHeadSeq(sym_First, seq);
}

def_bif(Last)
{
    verify_arity_with_r(Last, 1);
    if ((MSequence_EleAt(seq, 0)->Type == etHeadSeq) 
        && (MSequence_Len(MSequence_EleAt(seq, 0)->HeadSeq.pSequence) >= 1))
    {
        r = MSequence_EleAt(MSequence_EleAt(seq, 0)->HeadSeq.pSequence, 
                            MSequence_Len(MSequence_EleAt(seq, 0)->HeadSeq.pSequence) - 1);
        XRef_IncRef(r);
        return r;
    }

    return MExpr_CreateHeadSeq(sym_First, seq);
}

def_bif(Rest)
{
    verify_arity_with_r(Rest, 1);
    r = MSequence_EleAt(seq, 0);
    if (r->Type == etHeadSeq)
    {
        if (MSequence_Len(r->HeadSeq.pSequence) >= 1)
            return MExpr_CreateHeadSeqX(r->HeadSeq.Head,
                                    MSequence_Copy(r->HeadSeq.pSequence,
                                                   1, MSequence_Len(r->HeadSeq.pSequence) - 1));
        else
            return MExpr_CreateHeadSeqX(r->HeadSeq.Head,
                                    MSequence_Create(0));
    }

    return MExpr_CreateHeadSeq(sym_Rest, seq);
}

def_bif(Most)
{
    verify_arity_with_r(Most, 1);
    r = MSequence_EleAt(seq, 0);
    if (r->Type == etHeadSeq)
    {
        if (MSequence_Len(r->HeadSeq.pSequence) >= 1)
            return MExpr_CreateHeadSeqX(r->HeadSeq.Head,
                                        MSequence_Copy(r->HeadSeq.pSequence,
                                                       0, MSequence_Len(r->HeadSeq.pSequence) - 1));
        else
            return MExpr_CreateHeadSeqX(r->HeadSeq.Head,
                                    MSequence_Create(0));
    }

    return MExpr_CreateHeadSeq(sym_First, seq);
}

def_bif(Apply)
{
    if (check_arg_types(".X", seq, sym_Apply, false, _CONTEXT))
        return MExpr_CreateHeadSeq(MSequence_EleAt(seq, 0), MSequence_EleAt(seq, 1)->HeadSeq.pSequence);
    else 
        return MExpr_CreateHeadSeq(sym_Apply, seq);
}

MExpr BIF_GetFile(const MString fn, const MString key context_param)
{
    MTokener *tokener;
    MExpr expr;
    if (key != NULL)
        return MExpr_CreateSymbol(sym_DollarFailed);

    tokener = Token_NewFromFile(fn);
    if (NULL == tokener)
        return MExpr_CreateSymbol(sym_DollarFailed);
    expr = Parse_FromTokener(tokener);
    Token_Delete(tokener);
//    {
//        MSequence *s = MSequence_Create(1);
//        MSequence_SetAtX(s, 0, expr);
//        return MExpr_CreateHeadSeqX(sym_Hold, s);
//    }
    return expr;
}

def_bif(Get)
{
    MString fn = NULL;
    MString key = NULL;

    if ((MSequence_Len(seq) == 1) || (MSequence_Len(seq) == 2))
    {
        MExpr t = MSequence_EleAt(seq, 0);
        if (t->Type == etString)
            fn = t->Str;

        if (MSequence_Len(seq) == 2)
        {
            MExpr t = MSequence_EleAt(seq, 1);
            if (t->Type == etString)
                key = t->Str;
        }
        else;

        if (fn)
            return BIF_GetFile(fn, key, _CONTEXT);
    }
    else;

    return MExpr_CreateHeadSeq(sym_Get, seq);
}

def_bif(Join)
{
    MExpr h;
    MSequence *s;
    int len;
    int i;
    if (MSequence_Len(seq) < 1)
        return MExpr_CreateHeadSeqX(sym_List, MSequence_Create(0));
    h = MSequence_EleAt(seq, 0);
    if (h->Type != etHeadSeq)
        return MExpr_CreateHeadSeq(sym_Join, seq);
    len = MSequence_Len(h->HeadSeq.pSequence);
    h = h->HeadSeq.Head;
    for (i = 1; i < MSequence_Len(seq); i++)
    {
        MExpr t = MSequence_EleAt(seq, i);
        if ((t->Type == etHeadSeq) && IsExprSameQ(h, t->HeadSeq.Head))
            len += MSequence_Len(t->HeadSeq.pSequence);
        else
            return MExpr_CreateHeadSeq(sym_Join, seq);
    }
    s = MSequence_Create(len);
    len = 0;
    for (i = 0; i < MSequence_Len(seq); i++)
    {
        MSequence *pcur = MSequence_EleAt(seq, i)->HeadSeq.pSequence;
        int j;
        memcpy(s->pExpr + len, pcur->pExpr, MSequence_Len(pcur) * sizeof(pcur->pExpr[0]));
        len += MSequence_Len(pcur);
        for (j = 0; j < MSequence_Len(pcur); j++)
            XRef_IncRef(MSequence_EleAt(pcur, j));
    }

    return MExpr_CreateHeadSeqX(h, s);
}

def_bif(Append)
{
    MExpr h;
    MSequence *s;
    verify_arity_with_r(Append, 2);
    h = MSequence_EleAt(seq, 0);
    if (h->Type != etHeadSeq)
        return MExpr_CreateHeadSeq(sym_Append, seq);
    s = MSequence_Create(MSequence_Len(h->HeadSeq.pSequence) + 1);
    MSequence_OverwriteSetFrom(s, h->HeadSeq.pSequence, 0, 0, MSequence_Len(h->HeadSeq.pSequence));
    MSequence_SetAt(s, MSequence_Len(h->HeadSeq.pSequence), MSequence_EleAt(seq, 1));
    return MExpr_CreateHeadSeqX(h->HeadSeq.Head, s);
}

def_bif(Prepend)
{
    MExpr h;
    MSequence *s;
    verify_arity_with_r(Prepend, 2);
    h = MSequence_EleAt(seq, 0);
    if (h->Type != etHeadSeq)
        return MExpr_CreateHeadSeq(sym_Prepend, seq);
    s = MSequence_Create(MSequence_Len(h->HeadSeq.pSequence) + 1);
    MSequence_OverwriteSetFrom(s, h->HeadSeq.pSequence, 1, 0, MSequence_Len(h->HeadSeq.pSequence));
    MSequence_SetAt(s, 0, MSequence_EleAt(seq, 1));
    return MExpr_CreateHeadSeqX(h->HeadSeq.Head, s);
}

def_bif(ToCharacterCode)
{
    MString source = NULL;
    MString encoding = NULL;
    MExpr r;
    if (MSequence_Len(seq) >= 1)
    {
        if (is_str(MSequence_EleAt(seq, 0)))
            source = MSequence_EleAt(seq, 0)->Str;
        if (MSequence_Len(seq) == 2)
        {
            if (is_str(MSequence_EleAt(seq, 1)))
                encoding = MSequence_EleAt(seq, 1)->Str;
            else
                source = NULL;
        }
    }
    else;

    if (!source)
        return MExpr_CreateHeadSeq(sym_ToCharacterCode, seq);

    if (NULL == encoding)
    {
        MSequence *s = MSequence_Create(MString_Len(source));
        const MChar *pc = MString_GetData(source);
        int i;
        for (i = 0; i < MString_Len(source); i++)
            MSequence_SetAtX(s, i, MExpr_CreateMachineInt(*pc++));
        return MExpr_CreateHeadSeqX(sym_List, s);
    }
    else
    {
        int cap = MString_Len(source) * 4;
        MByte *b = (MByte *)YM_malloc(cap);
        int len = MStringToEncoding(source, b, cap, encoding);
        ASSERT(len <= cap);
        
        if (len > 0)
        {
            MSequence *s = MSequence_Create(len);
            MByte *pb = b;
            int i;
            for (i = 0; i < len; i++)
                MSequence_SetAtX(s, i, MExpr_CreateMachineInt(*pb++));
            r = MExpr_CreateHeadSeqX(sym_List, s);
        }
        else
            r = MExpr_CreateHeadSeqX(sym_List, MSequence_Create(0)); 
        YM_free(b);
        return r;
    }
}

static MString ToCharacterCode(MSequence *code, const MString encoding)
{
    MString r;
    if (NULL == encoding)
    {
        r = MString_NewWithCapacity(MSequence_Len(code));
        MChar *pc = (MChar *)MString_GetData(r);
        int i;
        MString_SetLen(r, MSequence_Len(code));
        for (i = 0; i < MSequence_Len(code); i++)
        {
            MExpr t = MSequence_EleAt(code, i);
            *pc++ = t->Type == etNum ? Num_ToInt(t->Num) : 0;
        }
        return r;
    }
    else
    {
        int cap = MSequence_Len(code) * 2;
        MByte *b = (MByte *)YM_malloc(cap);
        int len = 0;
        int i;
        
        for (i = 0; i < MSequence_Len(code); i++)
        {
            MExpr t = MSequence_EleAt(code, i);
            MInt code = (t->Type == etNum ? Num_ToInt(t->Num) : 0) & 0xFFFF;
            if (code > 0xFF)
            {
                *(MChar *)(b + len) = (MChar)(code);
                len += sizeof(MChar);
            }
            else
            {
                *(b + len) = (MByte)(code);
                len++;
            }
        }

        r = EncodingToMString(b, len, encoding);
        YM_free(b);
        
    }
    return r;
}

def_bif(FromCharacterCode)
{
    MSequence *source = NULL;
    MString encoding = NULL;
    if (MSequence_Len(seq) >= 1)
    {
        if (MSequence_EleAt(seq, 0)->Type == etHeadSeq)
            source = MSequence_EleAt(seq, 0)->HeadSeq.pSequence;
        if (MSequence_Len(seq) == 2)
        {
            if (is_str(MSequence_EleAt(seq, 1)))
                encoding = MSequence_EleAt(seq, 1)->Str;
            else
                source = NULL;
        }
    }
    else;

    if ((!source) || (MSequence_Len(source) < 1))
        return MExpr_CreateHeadSeq(sym_FromCharacterCode, seq);

    if (etHeadSeq == MSequence_EleAt(source, 0)->Type)
    {
        int i;
        MSequence *ps = MSequence_Create(MSequence_Len(source));
        for (i = 0; i < MSequence_Len(source); i++)
        {
            MExpr tt = MSequence_EleAt(source, i);
            if (etHeadSeq == tt->Type)
                MSequence_SetAtX(ps, i, MExpr_CreateStringX(ToCharacterCode(tt->HeadSeq.pSequence, encoding)));
            else
                MSequence_SetAtX(ps, i, MExpr_CreateSymbol(sym_DollarFailed));
        }
        
        return MExpr_CreateHeadSeqX(MSequence_EleAt(source, 0)->HeadSeq.Head, ps);
    }
    else
        return MExpr_CreateStringX(ToCharacterCode(source, encoding));
}

def_bif(Characters)
{
    MString s;
    MSequence *ps;
    int i;

    if (!check_arg_types("S", seq, sym_Characters, true, _CONTEXT))
        return MExpr_CreateHeadSeq(sym_Characters, seq);
    s = MSequence_EleAt(seq, 0)->Str;
    ps = MSequence_Create(MString_Len(s));
    for (i = 1; i <= MString_Len(s); i++)
        MSequence_SetAtX(ps, i - 1, MExpr_CreateStringX(MString_Copy(s, i, 1)));
    return MExpr_CreateHeadSeqX(sym_List, ps);
}

// TODO:
#define report_attri_modify_err(sym)

static MExpr modify_attributes0(MSequence *seq, MSymbol sym, bool (*f) (MSymbol, MExpr), _MCONTEXT_)
{
    int i;
    MExpr sets;

    if (!check_arg_types("M*", seq, sym, true, _CONTEXT))
        return MExpr_CreateHeadSeq(sym, seq);

    sets = MExpr_CreateHeadSeqX(sym_List,
            MSequence_SetAtX(MSequence_Create(1), 0, MExpr_CreateSymbol(sym_Protected)));

    for (i = 0; i < MSequence_Len(seq); i++)
    {
        if (!f(MSequence_EleAt(seq, i)->Symbol, sets))
        {
            report_attri_modify_err(MSequence_EleAt(seq, i)->Symbol);
        }
    }

    MExpr_Release(sets);

    return MExpr_CreateHeadSeq(sym_List, seq);
}

def_bif(Unprotect)
{
    return modify_attributes0(seq, sym_Unprotect, MSymbol_ClearAttributes, _CONTEXT);
}

def_bif(Protect)
{
    return modify_attributes0(seq, sym_Protect, MSymbol_AddAttributes, _CONTEXT);
}

def_bif(Attributes)
{
    if (!check_arg_types("M", seq, sym_Attributes, true, _CONTEXT))
        return MExpr_CreateHeadSeq(sym_Attributes, seq);
    else
        return MSymbol_Attributes(MSequence_EleAt(seq, 0)->Symbol);
}

static MExpr modify_attributes(MSequence *seq, MSymbol sym, bool (*f) (MSymbol, MExpr), _MCONTEXT_)
{
    MExpr r = verify_arity_fun(seq, sym_SetAttributes, 2, _CONTEXT);
    if (r)
        return r;
    else
    {
        MExpr set = MSequence_EleAt(seq, 1);
        r = MSequence_EleAt(seq, 0);
        XRef_IncRef(r);

        if (set->Type == etSymbol)
        {
            set = MExpr_CreateHeadSeqX(sym_List,
                    MSequence_SetAt(MSequence_Create(1),
                                    0,
                                    set));
        }
        else
            XRef_IncRef(set);

        if (r->Type == etSymbol)
        {
            if (!f(r->Symbol, set))
            {
                report_attri_modify_err(r->Symbol);
            }
        }
        else if (r->Type == etHeadSeq)
        {
            int i;
            for (i = 0; i < MSequence_Len(r->HeadSeq.pSequence); i++)
            {
                MExpr t = MSequence_EleAt(r->HeadSeq.pSequence, i);
                if (t->Type == etSymbol)
                    if (!f(t->Symbol, set))
                    {
                        report_attri_modify_err(t->Symbol);
                    }
            }
        }

        MExpr_Release(set);

        return r;
    }
}

def_bif(SetAttributes)
{
    return modify_attributes(seq, sym_SetAttributes, MSymbol_AddAttributes, _CONTEXT);
}

def_bif(ClearAttributes)
{
    return modify_attributes(seq, sym_ClearAttributes, MSymbol_ClearAttributes, _CONTEXT);
}

def_bif(MatchQ)
{
    MExpr expr;
    MExpr form;
    if (!check_arg_types("..", seq, sym_MatchQ, true, _CONTEXT))
        return MExpr_CreateHeadSeq(sym_MatchQ, seq);
    expr = MSequence_EleAt(seq, 0);
    form = MSequence_EleAt(seq, 1);
    return Match_MatchFirst(expr, form, _Context);
}

def_bif(Context)
{
    if (!check_arg_types("M", seq, sym_Context, true, _CONTEXT))
        return MExpr_CreateHeadSeq(sym_Context, seq);
    return MExpr_CreateString(MSymbol_Context(MSequence_EleAt(seq, 0)->Symbol));
}

def_bif(Car)
{
    MExpr r;
    if (!check_arg_types("O", seq, sym_Car, true, _CONTEXT))
        return MExpr_CreateHeadSeq(sym_Car, seq);
    r = MSequence_EleAt(seq, 0)->Cons.Car;
    XRef_IncRef(r);
    return r;
}

def_bif(Cdr)
{
    MExpr r;
    if (!check_arg_types("O", seq, sym_Cdr, true, _CONTEXT))
        return MExpr_CreateHeadSeq(sym_Cdr, seq);
    r = MSequence_EleAt(seq, 0)->Cons.Cdr;
    XRef_IncRef(r);
    return r;
}

def_bif(Reverse)
{
    MExpr r = NULL;
    if (check_arg_types("X", seq, sym_Reverse, false, _CONTEXT))
    {
        MSequence *ss = MSequence_EleAt(seq, 0)->HeadSeq.pSequence;
        const int L = MSequence_Len(ss);
        MSequence *ns = MSequence_Create(L);
        int i;
        for (i = 0; i < L; i++)
        {
            MSequence_SetAt(ns, i, MSequence_EleAt(ss, L - 1 - i));
        }
        r = MExpr_CreateHeadSeqX(MSequence_EleAt(seq, 0)->HeadSeq.Head, ns);
    }
    else if (check_arg_types("XN", seq, sym_Reverse, false, _CONTEXT))
    {
    }
    else if (check_arg_types("X{N*}", seq, sym_Reverse, true, _CONTEXT))
    {
    }


    return r ? r : MExpr_CreateHeadSeq(sym_Reverse, seq);
}

def_bif(Flatten)
{
    MExpr r;
    bool bRev = false;
    MSequence *ns = seq;
    const int L = MSequence_Len(seq);
    if (L < 1)
        return MExpr_CreateHeadSeqX(sym_List, MSequence_Create(0));
    r = MSequence_EleAt(seq, L - 1);
    if (is_sym(r, sym_Reverse))
    {
        bRev = true;
        ns = MSequence_Copy(seq, 0, L - 1);
    }
    else;

    if (check_arg_types("O*", ns, sym_Flatten, false, _CONTEXT))
    {
        int i;
        MExprList *lst = MExprList_Create();
        for (i = 0; i < MSequence_Len(ns); i++)
        {
            MExpr t = MSequence_EleAt(ns, i);
            while (t->Type == etCons)
            {
                MExprList_PushOrphan(lst, t->Cons.Car);
                t = t->Cons.Cdr;
            }
            if (!is_sym(t, sym_Null)) MExprList_PushOrphan(lst, t);
        }

        r = MExpr_CreateHeadSeqFromExprList(sym_List, lst, bRev);
        MExprList_Clear(lst); 
        MExprList_Release(lst);
    }
    else
        r = MExpr_CreateHeadSeq(sym_Flatten, ns);

    if (ns != seq)
        MSequence_Release(ns);

    return r;
}

def_bif(Cons)
{
    MExpr last;
    int i = MSequence_Len(seq);

    if (i < 1)
        return MExpr_CreateHeadSeq(sym_Cons, seq);
    else if (i == 1)
        return MExpr_CreateCons(MSequence_EleAt(seq, 0),
                                MExpr_CreateSymbol(sym_Null));
    else;

    last = MExpr_CreateCons(MSequence_EleAt(seq, i - 2),
                            MSequence_EleAt(seq, i - 1));
    i -= 3;
    while (i >= 0)
    {
        last = MExpr_CreateConsNX(MSequence_EleAt(seq, i), last);
        i--;
    } 
    return last;
}

def_bif(ConsQ)
{
    MExpr r = verify_arity(seq, sym_ConsQ, 1);
    if (r)
        return r;
    r = MSequence_EleAt(seq, 0);
    return MExpr_CreateSymbol(r->Type == etCons ? sym_True : sym_False);
}

def_bif(ReplaceCar)
{
    MExpr r;
    if (!check_arg_types("O.", seq, sym_ReplaceCar, true, _CONTEXT))
        return MExpr_CreateHeadSeq(sym_ReplaceCar, seq);
    r = MExpr_ReplaceCar(MSequence_EleAt(seq, 0), MSequence_EleAt(seq, 1));
    XRef_IncRef(r);
    return r;
}

def_bif(ReplaceCdr)
{
    MExpr r;
    if (!check_arg_types("O.", seq, sym_ReplaceCdr, true, _CONTEXT))
        return MExpr_CreateHeadSeq(sym_ReplaceCdr, seq);
    r = MExpr_ReplaceCdr(MSequence_EleAt(seq, 0), MSequence_EleAt(seq, 1));
    XRef_IncRef(r);
    return r;
}

def_bif(InputString)
{
    MString prompt = NULL;
    MString init = NULL;
    MString r;
    if ((MSequence_Len(seq) >= 1) 
            && (MSequence_EleAt(seq, 0)->Type == etString))
    {
        prompt = MSequence_EleAt(seq, 0)->Str;
        XRef_IncRef(prompt);
    }
    else
        prompt = MString_NewC("");
    if ((MSequence_Len(seq) >= 2) 
            && (MSequence_EleAt(seq, 1)->Type == etString))
    {
        init = MSequence_EleAt(seq, 1)->Str;
        XRef_IncRef(init);
    }
    else
        init = MString_NewC("");
    r = K_Input(prompt, init);
    MString_Release(prompt);
    MString_Release(init);
    return MExpr_CreateStringX(r);
}   

def_bif(Stack)
{
    extern MExpr Eval_GetStack(_MCONTEXT_);
    MExpr r = verify_arity(seq, sym_Stack, 0);
    if (r)
        return r;
    return Eval_GetStack(_Context);
}

static MExprIterNextType hash_on_expr(MExpr s, MExpr, MExprIterType it, MHash *r, MExprIter)
{
    const static MHash TypeHash[32] = 
    {
        0xddc04e14, 0xddc0b1eb, 0xdd3f4eeb, 0xdd3fb114, 0xd2cf411b,
        0xd2cfbee4, 0xd23041e4, 0xd230be1b, 0xeef37d27, 0xeef382d8,
        0xee0c7dd8, 0xee0c8227, 0xe1fc7228, 0xe1fc8dd7, 0xe10372d7,
        0xe1038d28, 0x88951b41, 0x8895e4be, 0x886a1bbe, 0x886ae441,
        0x879a144e, 0x879aebb1, 0x876514b1, 0x8765eb4e, 0xbba62872,
        0xbba6d78d, 0xbb59288d, 0xbb59d772, 0xb4a9277d, 0xb4a9d882,
        0xb4562782, 0xb456d87d
    };
    MExprIterNextType rr = entContinue;
    MHash t = 0;
    MHash h = *r;
    unsigned long g;

    switch (it)
    {
    case eitEnd:
    case eitInnerDone:
        return entContinue;
    case eitNormal:
    default:
        switch (s->Type)
        {
        case etNum:          t = Num_Hash(s->Num); break;
        case etString:       t = ElfHash(MString_GetData(s->Str), MString_Len(s->Str), 0); break;
        case etComplex:      t = (Num_Hash(s->Complex.Imag) ^ TypeHash[31]) ^ Num_Hash(s->Complex.Real); break;
        case etSymbol:       t =  ElfHash(MString_GetData(MSymbol_Context(s->Symbol)), 
                                          MString_Len(MSymbol_Context(s->Symbol)), 
                                          ElfHash(MString_GetData(s->Symbol->Name), 
                                                  MString_Len(s->Symbol->Name), 0)); 
                             break;
        case etCons:
        case etHeadSeq:
                             rr = entGoDeeper;
        }
    }

    // rotate h by 4bits
    g = h & 0xF0000000L;
    h = (h << 4) ^ (g >> 28);
    *r = h ^ t ^ TypeHash[s->Type];
    return rr;
}

MHash BIF_Hash0(MExpr s)
{
    MHash r = 0;
    MByte iter_buf[EXPR_ITER_SIZE];
    MExprIter iter = MExprIter_CreateStatic(iter_buf, EXPR_ITER_SIZE, s, (f_OnExprTranverse)hash_on_expr, &r);
    MExprIter_Go(iter);
    MExprIter_Release(iter);

    return r ? r : 0xb456d87d;
}

def_bif(Hash)
{
    if (!check_arg_types(".", seq, sym_Hash, false, _CONTEXT))
    {
        if (!check_arg_types(".S", seq, sym_Hash, true, _CONTEXT))
            return MExpr_CreateHeadSeq(sym_Hash, seq);
        else
        {
            return MExpr_CreateMachineInt(0);
        }
    }
    else 
    {
        return MExpr_CreateMachineUInt(MExpr_Hash(MSequence_EleAt(seq, 0)));
    }
}


