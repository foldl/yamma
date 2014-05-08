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
#include "sym.h"
#include "matcher.h"
#include "bif_internal.h"

MExpr Eval_Throw(MExpr e, _MCONTEXT_);

void BIF_Message1(MSymbol sym, MExpr expr, MExpr p1, _MCONTEXT_);

def_bif(CompoundExpression)
{
    if (MSequence_Len(seq) > 0)
    {
        MDword i = Eval_GetCallTag(_Context);
        if (i > 0)
            MExpr_Release(Eval_GetCallExpr(_Context));
        if (i + 1 < MSequence_Len(seq))
        {
            RET_Eval_RetCCEvalX(MSequence_EleAt(seq, i), i + 1);
        }
        else
        {
            MExpr t = MSequence_EleAt(seq, i);
            XRef_IncRef(t);
            return t;
        }
    }
    else
        return MExpr_CreateSymbol(sym_Null);
}

#define run_through_bools(sym0, def, checker)                                                               \
do {                                                                                                        \
    int i = (int)Eval_GetCallTag(_Context);                                                                 \
    if (i > 0)                                                                                              \
    {                                                                                                       \
        MExpr v = Eval_GetCallExpr(_Context);                                                               \
        bool bTrue = false;                                                                                 \
        if (v->Type != etSymbol) { MExpr_Release(v); return MExpr_CreateHeadSeq(sym_##sym0, seq); }         \
        if (v->Symbol == sym_True) bTrue = true;                                                            \
        else if (v->Symbol != sym_False) { MExpr_Release(v); return MExpr_CreateHeadSeq(sym_##sym0, seq); } \
        MExpr_Release(v);                                                                                   \
        checker;                                                                                            \
    } else;                                                                                                 \
    if (i + 1 <= MSequence_Len(seq))                                                                        \
        RET_Eval_RetCCEvalX(MSequence_EleAt(seq, i), i + 1);                                                \
    else                                                                                                    \
        return MExpr_CreateSymbol(sym_##def);                                                               \
} while (false)

def_bif(Or)
{
    run_through_bools(Or, False,
                      if (bTrue) return MExpr_CreateSymbol(sym_True));
}

def_bif(Nor)
{
    run_through_bools(Nor, True,
                      if (bTrue) return MExpr_CreateSymbol(sym_False));
}

def_bif(And)
{
    run_through_bools(And, True,
                      if (!bTrue) return MExpr_CreateSymbol(sym_False));
}

def_bif(Nand)
{
    run_through_bools(Nand, False,
                      if (!bTrue) return MExpr_CreateSymbol(sym_True));
}

#define run_through_bools_acc(sym0, tag)                                                                 \
do {                                                                                                     \
    MDword i = Eval_GetCallTag(_Context);                                                                \
    const int SIZE = MSequence_Len(seq) + 2;                                                             \
    MInt n = i / SIZE;                                                                                   \
    i -= n * SIZE;                                                                                       \
    if (i > 0)                                                                                           \
    {                                                                                                    \
        MExpr v = Eval_GetCallExpr(_Context);                                                            \
        bool bTrue = false;                                                                              \
        if (v->Type != etSymbol) { MExpr_Release(v); return MExpr_CreateHeadSeq(sym_##sym0, seq); }      \
        if (v->Symbol == sym_True) bTrue = true;                                                         \
        else if (v->Symbol != sym_False) { MExpr_Release(v); return MExpr_CreateHeadSeq(sym_##sym0, seq); } \
        MExpr_Release(v);                                                                                \
        if (bTrue) n++;                                                                                  \
    }                                                                                                    \
    else;                                                                                                \
    if (i + 1 <= MSequence_Len(seq))                                                                     \
    {                                                                                                    \
        RET_Eval_RetCCEvalX(MSequence_EleAt(seq, i), n * SIZE + i + 1);                                  \
    }                                                                                                    \
    else return MExpr_CreateSymbol(n % 2 == tag ? sym_False : sym_True);                                 \
} while (false)

def_bif(Xor)
{
    run_through_bools_acc(Xor, 0);
}

def_bif(Xnor)
{
    run_through_bools_acc(Xnor, 1);
}

def_bif(Which)
{
    int i = Eval_GetCallTag(_Context);
    if (i > 0)
    {
        MExpr v = Eval_GetCallExpr(_Context);
        bool bt = is_true(v);
        MExpr_Release(v);
        if (bt)
        {
            MExpr r;
            ASSERT(i <  MSequence_Len(seq));
            r = MSequence_EleAt(seq, i);
            XRef_IncRef(r);
            return r;
        }
        else
            i++;
    }
    else;

    if (i + 1 < MSequence_Len(seq))
    {
        RET_Eval_RetCCEvalX(MSequence_EleAt(seq, i), i + 1);
    }
    else if (i < MSequence_Len(seq))
    {
        MExpr t = MSequence_EleAt(seq, i);
        XRef_IncRef(t);
        return t;
    }
    else
        return MExpr_CreateSymbol(sym_Null);
}

def_bif(While)
{
    MExpr condi;
    MExpr r;
    bool bt;
    bool bf;
    switch (Eval_GetCallTag(_Context))
    {
    case 0:
        r = verify_arity(seq, sym_While, 2);
        if (r)
            return r;
        else;
        RET_Eval_RetCCEvalX(MSequence_EleAt(seq, 0), 1);
        break;
    case 1:
        condi = Eval_GetCallExpr(_Context);
        bt = is_true(condi);
        bf = is_false(condi);
        MExpr_Release(condi);
        if (bt)
            RET_Eval_RetCCEvalX(MSequence_EleAt(seq, 1), 2);
        else if (bf)
        {
            return MExpr_CreateSymbol(sym_Null);
        }
        else
            return MExpr_CreateHeadSeq(sym_While, seq);
        break;
    case 2:
        MExpr_Release(Eval_GetCallExpr(_Context));
        RET_Eval_RetCCEvalX(MSequence_EleAt(seq, 0), 1);
        break;
    default:
        ASSERT("While impossible path" == NULL);
        return NULL;
    }
}

def_bif(For)
{
    MExpr test;
    bool bt;
    bool bf;
    switch (Eval_GetCallTag(_Context))
    {
    case 0:
        if (MSequence_Len(seq) == 3)
            RET_Eval_RetCCEvalX(MSequence_EleAt(seq, 0), 1);
        else if (MSequence_Len(seq) == 4)
            RET_Eval_RetCCEvalX(MSequence_EleAt(seq, 0), 5);
        else
            return verify_arity(seq, sym_If, 4);
        break;
    case 1: // start/incr done, run test
        MExpr_Release(Eval_GetCallExpr(_Context));
        RET_Eval_RetCCEvalX(MSequence_EleAt(seq, 1), 2);
        break;
    case 2: // check test, then do incr
        test = Eval_GetCallExpr(_Context);
        bt = is_true(test);
        bf = is_false(test);
        MExpr_Release(test);
        if (bt)
        {
            RET_Eval_RetCCEvalX(MSequence_EleAt(seq, 2), 1);
        }
        else if (bf)
        {
            return MExpr_CreateSymbol(sym_Null);
        }
        else
            return MExpr_CreateHeadSeq(sym_For, seq);
        break;
    case 5: // start/incr done, run test
        MExpr_Release(Eval_GetCallExpr(_Context));
        RET_Eval_RetCCEvalX(MSequence_EleAt(seq, 1), 6);
    case 6: // check test, then do body
        test = Eval_GetCallExpr(_Context);
        bt = is_true(test);
        bf = is_false(test);
        MExpr_Release(test);
        if (bt)
        {
            RET_Eval_RetCCEvalX(MSequence_EleAt(seq, 3), 7);
        }
        else if (bf)
        {
            return MExpr_CreateSymbol(sym_Null);
        }
        else
            return MExpr_CreateHeadSeq(sym_For, seq);
        break;
    case 7: // body done, run incr
        MExpr_Release(Eval_GetCallExpr(_Context));
        RET_Eval_RetCCEvalX(MSequence_EleAt(seq, 2), 5);
        break;
    default:
        ASSERT("For impossible path" == NULL);
        return NULL;
    }
}

static bool IsPatternExpr(MExpr expr)
{
    MInt i;

    if (expr->Type != etHeadSeq)
        return false;
    if ((expr->HeadSeq.Head->Type == etSymbol) 
        && (   (expr->HeadSeq.Head->Symbol == sym_Blank)
            || (expr->HeadSeq.Head->Symbol == sym_BlankSequence)
            || (expr->HeadSeq.Head->Symbol == sym_BlankNullSequence)))
        return true;
    for (i = 0; i < MSequence_Len(expr->HeadSeq.pSequence); i++)
    {
        if (IsPatternExpr(MSequence_EleAt(expr->HeadSeq.pSequence, i)))
            return true;
    }

    return false;
}

static MHash Hash_RuleExpr(MExpr expr)
{
    return MExpr_Hash(MSequence_EleAt(expr->HeadSeq.pSequence, 0)) ^ 
           MExpr_Hash(expr->HeadSeq.Head);
}

static MInt Hash_RuleExprCompare(MExpr expr1, MExpr expr2)
{
    return IsExprSameQ(MSequence_EleAt(expr1->HeadSeq.pSequence, 0),
                       MSequence_EleAt(expr2->HeadSeq.pSequence, 0)) &&
           IsExprSameQ(expr1->HeadSeq.Head,
                       expr2->HeadSeq.Head) 
                ? 0 : 1;
}

static MInt PatternWeight(MExpr expr, MInt Base, bool inPattern)
{
    if ((expr->Type == etNum) || (expr->Type == etString) || (expr->Type == etSymbol))
        return Base + (MInt)(expr->Type);
    if (expr->Type == etComplex)
        return Base + 2 * (MInt)(expr->Type);
    else
    {
        MInt i = 0;
        MInt r = 0;

        // etHeadSeq
        if ((expr->HeadSeq.Head->Type == etSymbol) && (expr->HeadSeq.Head->Symbol == sym_Pattern))
        {
            inPattern = true;
            Base /= 16;
            Base++;
            i = 1;
            r = -1;
        }
        else

        r += inPattern ? 1 : Base;

        for (; i < MSequence_Len(expr->HeadSeq.pSequence); i++)
            r += PatternWeight(MSequence_EleAt(expr->HeadSeq.pSequence, i), Base, inPattern);
        
        return r;
    }
}

static MInt PatternWeight(MExpr expr)
{
    return PatternWeight(expr, 256, false);
}

static MInt PatternWeightOfRule(MExpr expr)
{
    return PatternWeight(MSequence_EleAt(expr->HeadSeq.pSequence, 0));
}

static bool IsPatternSameQ(MExpr expr1, MExpr expr2)
{
    bool r = false;
    int i = 0;
    if (expr1->Type != expr2->Type)
        return false;
    switch (expr1->Type)
    {
    case etNum:          return Num_SameQ(expr1->Num, expr2->Num); break;
    case etString:       return MString_SameQ(expr1->Str, expr2->Str); break;
    case etComplex:      return Num_SameQ(expr1->Complex.Real, expr2->Complex.Real) &&
                                Num_SameQ(expr1->Complex.Imag, expr2->Complex.Imag); break;
    case etCons:         return IsPatternSameQ(expr1->Cons.Car, expr2->Cons.Car) &&
                                IsPatternSameQ(expr1->Cons.Cdr, expr2->Cons.Cdr); 
                         break;
    case etSymbol:       return (expr1->Symbol->Context == expr2->Symbol->Context) &&
                                MString_SameQ(expr1->Symbol->Name, expr2->Symbol->Name); break;
    case etHeadSeq:     r = (MSequence_Len(expr1->HeadSeq.pSequence) == MSequence_Len(expr2->HeadSeq.pSequence))
                            && IsPatternSameQ(expr1->HeadSeq.Head, expr2->HeadSeq.Head);
                        if (!r)
                            return r;

                        if ((expr1->HeadSeq.Head->Type == etSymbol) && (expr1->HeadSeq.Head->Symbol == sym_Pattern))
                            i = 1;

                        for (; i < MSequence_Len(expr1->HeadSeq.pSequence); i++)
                        {
                            r = IsPatternSameQ(expr1->HeadSeq.pSequence->pExpr[i],
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

MSymbol BIF_Set_getsym(MExpr target);
static MExpr BIF_SetHelper(MExpr target, MExpr r, const bool bDelayed, const MSymbol sym_RuleSym, _MCONTEXT_)
{
    MSymbol sym;

    if ((r->Type == etHeadSeq) && is_header(r, sym_Condition)
        && (MSequence_Len(r->HeadSeq.pSequence) == 2))
    {
        MSequence *s = MSequence_Create(2);
        MSequence_SetAt(s, 0, target);
        MSequence_SetAt(s, 1, MSequence_EleAt(r->HeadSeq.pSequence, 1));
        target = MExpr_CreateHeadSeqX(sym_Condition, s);            
        r = MSequence_EleAt(r->HeadSeq.pSequence, 0);
    }
    else
        XRef_IncRef(target);
    
    XRef_IncRef(r);

    if (target->Type >= etSymbol)
    {
        if ((target->Type == etHeadSeq) && is_header(target, sym_Condition)
            && (MSequence_Len(target->HeadSeq.pSequence) > 0))
            sym = BIF_Set_getsym(MSequence_EleAt(target->HeadSeq.pSequence, 0));
        else
            sym = BIF_Set_getsym(target);

        if (sym)
        {
            MDword att = sym->Attrib;
            if ((att & (aProtected | aLocked)) != 0)
            {
                MExpr t = MExpr_CreateSymbol(sym);
                BIF_Message1(sym_Set, msg_Setwrsym, t, _Context);
                MExpr_Release(t);
                goto quit;
            }
            else;

            if (target->Type == etSymbol)
            {
                if (sym->Immediate)
                    MExpr_Release(sym->Immediate);
                sym->Immediate = r;
                sym->bDelayedImmediate = bDelayed;
                XRef_IncRef(r);
            }
            else
            {
                MSequence *s = MSequence_Create(2);
                MExpr def;
                MSequence_SetAt(s, 0, target);
                MSequence_SetAt(s, 1, r);
                def = MExpr_CreateHeadSeqX(sym_RuleSym, s);

                if (IsPatternExpr(target))
                {
                    if (sym->PatternDownValues)
                    {
                        MSequence *s2 = MSequence_Create(MSequence_Len(sym->PatternDownValues) + 1);
                        MInt w0 = PatternWeight(target);
                        MInt wi;
                        MInt i = 0;
                        MInt j = 0;
                        for (; i < MSequence_Len(sym->PatternDownValues); i++)
                        {
                            wi = PatternWeightOfRule(MSequence_EleAt(sym->PatternDownValues, i));
                            if (wi > w0)
                            {
                                MSequence_SetAt(s2, j, MSequence_EleAt(sym->PatternDownValues, i));
                                j++;
                            }
                            else
                                break;
                        }

                        if (i < MSequence_Len(sym->PatternDownValues))
                        {
                            if (wi == w0)
                            {
                                MExpr t = MSequence_EleAt(sym->PatternDownValues, i);
                                if (IsPatternSameQ(MSequence_EleAt(t->HeadSeq.pSequence, 0), target))
                                {
                                    MSequence_Release(s2);
                                    MSequence_SetAtX(sym->PatternDownValues, i, def);
                                    goto quit;
                                }
                            }
                            else
                            {
                                MSequence_SetAtX(s2, j, def); j++;

                                for (; i < MSequence_Len(sym->PatternDownValues); i++)
                                {
                                    MSequence_SetAt(s2, j, MSequence_EleAt(sym->PatternDownValues, i));
                                    j++;
                                }
                            }
                        }
                        else
                            MSequence_SetAtX(s2, j, def);

                        MSequence_Release(sym->PatternDownValues);
                        sym->PatternDownValues = s2;
                    }
                    else
                    {
                        sym->PatternDownValues = MSequence_Create(1);
                        MSequence_SetAtX(sym->PatternDownValues, 0, def);
                    }
                }
                else
                {
                    MExpr oldDef;
                    if (sym->ExactDownValues == NULL)
                        sym->ExactDownValues = MHashTable_Create(16, (f_Hash1)Hash_RuleExpr, NULL,
                                                                 (f_HashCompare)Hash_RuleExprCompare);
                    oldDef = (MExpr)MHashTable_InsertOrReplace(sym->ExactDownValues, def);                    
                    if (oldDef) MExpr_Release(oldDef);
                }
            }
        }
        else
            BIF_Message1(sym_Set, msg_Setwrsym, target, _Context);
    }
    else
        BIF_Message1(sym_Set, msg_Setsetraw, target, _Context);

quit:
    MExpr_Release(target);
    return r;
}

def_bif(Set)
{
    MExpr r;
    MExpr target;
    if (Eval_GetCallTag(_Context) == 0)
    {
        r = verify_arity(seq, sym_Set, 2);
        if (r)
            return r;

        target = MSequence_EleAt(seq, 0);

        if ((target->Type == etHeadSeq) && (MSequence_Len(target->HeadSeq.pSequence) > 0))
        {
            MExpr t = MExpr_CreateHeadSeq(sym_Sequence, target->HeadSeq.pSequence);
            return Eval_RetCCEval(t, 1, _Context);
        }
        else
            XRef_IncRef(target);
    }
    else
    {
        MExpr t = Eval_GetCallExpr(_Context);
        target = MSequence_EleAt(seq, 0);
        ASSERT(t->Type == etHeadSeq);
        ASSERT(target->Type == etHeadSeq);
        target = MExpr_CreateHeadSeq(target->HeadSeq.Head, t->HeadSeq.pSequence);
        MExpr_Release(t);
    }

    r = BIF_SetHelper(target, MSequence_EleAt(seq, 1), false, sym_Rule, _Context);
    MExpr_Release(target);
    
    return r;
}

def_bif(SetDelayed)
{
    MExpr r;
    MExpr target;
    if (Eval_GetCallTag(_Context) == 0)
    {
        r = verify_arity(seq, sym_SetDelayed, 2);
        if (r)
            return r;

        target = MSequence_EleAt(seq, 0);

        if ((target->Type == etHeadSeq) && (MSequence_Len(target->HeadSeq.pSequence) > 0))
        {
            MExpr t = MExpr_CreateHeadSeq(sym_Sequence, target->HeadSeq.pSequence);
            return Eval_RetCCEval(t, 1, _Context);
        }
        else
            XRef_IncRef(target);
    }
    else
    {
        MExpr t = Eval_GetCallExpr(_Context);
        target = MSequence_EleAt(seq, 0);
        ASSERT(t->Type == etHeadSeq);
        ASSERT(target->Type == etHeadSeq);
        target = MExpr_CreateHeadSeq(target->HeadSeq.Head, t->HeadSeq.pSequence);
        MExpr_Release(t);
    }

    r = BIF_SetHelper(target, MSequence_EleAt(seq, 1), true, sym_RuleDelayed, _Context);
    MExpr_Release(target);
    return r;
}

struct iter_state
{
    bool done;
    MExpr f;
    MExpr acc;
    MExpr eval;
    MExprIter iter;
};

static MExprIterNextType iter_on_expr(MExpr s, MExpr, MExprIterType it, iter_state *r, MExprIter)
{
    r->eval = NULL;
    switch (it)
    {
    case eitEnd:
    case eitInnerDone:
        return entContinue;
    case eitNormal:
    default:
       ;//  rr = entGoDeeper;
    }
    return entContinue;
}


// Iterate[e, f, acc]
//    f(ele, parent, MExprIterType, state, iter) -> {MExprIterNextType, newState}
//   return final acc
def_bif(Iterate)
{
    return NULL;
/*    
    iter_state *state = (iter_state *)(Eval_GetCallTag(_Context));
    if (NULL == state)
    {
        verify_arity_with_r(Iterate, 3);
        state = (iter_state *)YM_malloc(sizeof(iter_state));
        state->done = false;
        state->f = MSequence_EleAt(seq, 0);
        state->acc = MSequence_EleAt(seq, 2); XRef_IncRef(state->acc);
        state->iter = MExprIter_Create(iter_buf, EXPR_ITER_SIZE, 
                                        MSequence_EleAt(seq, 1), 
                                        (f_OnExprTranverse)iter_on_expr, 
                                        state);
        MExprIter_Go(state->iter);
    }
    else
    {
        MExpr fr = Eval_GetCallExpr(_Context);
        if ((fr.Type == etHeadSeq) && is_header(fr, sym_List)
                && (MSequence_Len(fr->HeadSeq.pSequence) == 2))
        {
            MExpr nt = MSequence_EleAt(fr->HeadSeq.pSequence, 0);
            MExpr_Release(state->acc);
            state->acc = MSequence_EleAt(fr->HeadSeq.pSequence, 1);
            XRef_IncRef(state->acc);
            MExpr_Release(fr);

            
        }
        else;

throw_r:        
        {
            MExpr_Release(state->acc);
            state->acc = Eval_Throw(fr);
            goto done;
        }
    }

    if (iter->eval)
    {
        return Eval_RetCCEval(iter->eval, 1, _Context);
    }
    else
    {
done:
        MExpr r = state.acc;
        YM_free(state);
        MExprIter_Release(iter);
        return r;
    }
    */
}

