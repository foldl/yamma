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
#include "bif_internal.h"
#include <math.h>

def_bif(Fold)
{
    struct fold_tag
    {
        int counter;
        MExpr t;
    } *tag = (fold_tag *)Eval_GetCallTag(_Context);

    MSequence *lseq = MSequence_EleAt(seq, 2)->HeadSeq.pSequence;
    MExpr acc;

    if (tag == NULL)
    {
        if (check_arg_types("..X", seq, sym_Fold, true, _CONTEXT))
        {
            acc = MSequence_EleAt(seq, 1);
            XRef_IncRef(acc);
            tag = (fold_tag*)YM_malloc(sizeof(*tag));
            tag->counter = 0;
            tag->t = MExpr_CreateHeadSeqX(MSequence_EleAt(seq, 0), MSequence_Create(2));
        }
        else
            return MExpr_CreateHeadSeq(sym_Fold, seq);;
    }
    else
        acc = Eval_GetCallExpr(_Context);

    while (tag->counter < MSequence_Len(lseq))
    {
        MSequence *ps;
        MExpr r = NULL;
        MExpr t;

        if (!MObj_IsUnique(tag->t) || !MObj_IsUnique(tag->t->HeadSeq.pSequence))
        {
            MExpr_Release(tag->t);
            tag->t = MExpr_CreateHeadSeqX(MSequence_EleAt(seq, 0), MSequence_Create(2));
        }
        else;

        t = tag->t; XRef_IncRef(tag->t);
        ps = tag->t->HeadSeq.pSequence;
        MSequence_SetAtX(ps, 0, acc);
        MSequence_SetAt (ps, 1, MSequence_EleAt(lseq, tag->counter));
        Eval_eval_num_opt(t, r);
        ++tag->counter;
        if (r == NULL)
        {
            return Eval_RetCCEval(t, (MDword)tag, _Context);
        }
        else
        {
            MExpr_Release(t);
            acc = r;
        }
    }

    MExpr_Release(tag->t);
    YM_free(tag);

    return acc;
}

def_bif(FoldList)
{
    // [0]: rexpr
    // [1]: counter
    MSequence *ps = (MSequence *)(Eval_GetCallTag(_Context));
    MExpr rexpr;
    int counter;
    MSequence *rseq;
    MSequence *lseq = MSequence_EleAt(seq, 2)->HeadSeq.pSequence;

    if (NULL == ps)
    {
        if (check_arg_types("..X", seq, sym_Fold, true, _CONTEXT))
        {
            rseq = MSequence_Create(MSequence_Len(lseq) + 1);           
            rexpr = MExpr_CreateHeadSeqX(MSequence_EleAt(seq, 2)->HeadSeq.Head, rseq);
            MSequence_SetAt(rseq, 0, MSequence_EleAt(seq, 1));
            MSequence_OverwriteSetFrom(rseq, lseq, 1, 0, MSequence_Len(lseq));

            ps = MSequence_Create(2);
            MSequence_EleAt(ps, 0) = rexpr;
            counter = 0;
        }
        else
            return MExpr_CreateHeadSeq(sym_FoldList, seq);
    }
    else
    {
        rexpr = MSequence_EleAt(ps, 0);
        counter = (MDword)(MSequence_EleAt(ps, 1)) + 1;
        rseq = rexpr->HeadSeq.pSequence;
        MSequence_SetAtX(rseq, counter, Eval_GetCallExpr(_Context));
    }

    while (counter < MSequence_Len(lseq))
    {
        MExpr t = MExpr_CreateHeadSeqX(MSequence_EleAt(seq, 0), 
                                       MSequence_UniqueCopy(rseq, counter, 2));
        MExpr r = NULL;
        Eval_eval_num_opt(t, r);
        if (r == NULL)
        {
            MSequence_EleAt(ps, 1) = (MExpr)(counter);
            return Eval_RetCCEval(t, (MDword)(ps), _Context);
        }
        MExpr_Release(t);
        MSequence_SetAtX(rseq, ++counter, r);
    }

    ASSERT(counter == MSequence_Len(lseq));

    MSequence_EleAt(ps, 0) = NULL;
    MSequence_EleAt(ps, 1) = NULL;
    MSequence_Release(ps);

    return rexpr;
}

def_bif(StringJoin)
{
    MString r;
	MSequence *ps = seq;

    if (MSequence_Len(seq) == 0)
        return MExpr_CreateStringX(MString_NewC(""));

	if (check_arg_types("{S*}", seq, sym_StringJoin, false, _CONTEXT))
		ps = MSequence_EleAt(seq, 0)->HeadSeq.pSequence;
    else if (!check_arg_types("S*", seq, sym_StringJoin, true, _CONTEXT))
		return MExpr_CreateHeadSeq(sym_StringJoin, seq);

    {
		int i;
		int cap = 0;
		for (i = 0; i < MSequence_Len(ps); i++)
			cap += MString_Len(MSequence_EleAt(ps, i)->Str);
		r = MString_NewWithCapacity(cap);
		for (i = 0; i < MSequence_Len(ps); i++)
			MString_Cat(r, MSequence_EleAt(ps, i)->Str);
		return MExpr_CreateStringX(r);
	}
}

static int GetExprDepth(const MExpr expr)
{
    const MExpr temp = expr;
    int imax = 0;
    int i;
    MSequence *ps;

    if (temp->Type != etHeadSeq)
        return 0;
    ps = temp->HeadSeq.pSequence;
    if (MSequence_Len(ps) < 1)
        return 1;
    imax = GetExprDepth(MSequence_EleAt(ps, 0));
    for (i = 1; i < MSequence_Len(ps); i++)
        imax = max(imax, GetExprDepth(MSequence_EleAt(ps, 1)));

    return imax + 1;
}

static MExpr BIF_Map0(const MExpr f, const MExpr lst,
                     const int FromLevel, const int EndLevel, 
                     const int NegFromLevel, const int NegEndLevel, 
                     const bool bHeads context_param)
{
    MExpr rDepthFirst = NULL;
    bool bActive = false;       // has this *list* been "mapped"?
    bool bTranversed = true;

    // check this level: positive level
    if (FromLevel == 0)
    {
        MSequence *t = MSequence_Create(1);
        MExpr tr = BIF_Map0(f, lst, 
                            FromLevel - 1, EndLevel - 1,
                            NegFromLevel + 1, NegEndLevel + 1, 
                            bHeads, _CONTEXT);
        MSequence_SetAtX(t, 0, tr);
        rDepthFirst = MExpr_CreateHeadSeqX(f, t);
    }
    else;

    if (lst->Type != etHeadSeq)
    {
        XRef_IncRef(lst);
        if (rDepthFirst) MExpr_Release(rDepthFirst);
        return lst;
    }

    if (FromLevel == 1)
    {
        int i;
        MExpr h = lst->HeadSeq.Head;
        MSequence *ps = lst->HeadSeq.pSequence;
        MSequence *t = MSequence_Create(MSequence_Len(ps));

        for (i = 0; i < MSequence_Len(ps); i++)
        {
            MExpr tt = MSequence_EleAt(ps, i);
            MSequence *ele = MSequence_Create(1);
            MExpr tr = BIF_Map0(f, tt, 
                                FromLevel - 1, EndLevel - 1,
                                NegFromLevel + 1, NegEndLevel + 1, 
                                bHeads, _CONTEXT);
            
            MSequence_SetAtX(ele, 0, tr);
            MSequence_SetAtX(t, i, MExpr_CreateHeadSeqX(f, ele));
        }

        if (bHeads)
        {
            MSequence *ele = MSequence_Create(1);
            MSequence_SetAt(ele, 0, h);
            rDepthFirst = MExpr_CreateHeadXSeqX(MExpr_CreateHeadSeqX(f, ele), t);
        }
        else
            rDepthFirst = MExpr_CreateHeadSeqX(h, t);

        bActive = true;
    }
    else
    {
        bTranversed = false;
    }

    // now check neg level
    // TODO: optimize
    if (!bActive)
    {
        int NegLevel = -GetExprDepth(rDepthFirst);
        if ((NegLevel >= NegFromLevel) && (NegLevel <= NegEndLevel))
        {
            MSequence *t = MSequence_Create(1);
            MExpr tr = BIF_Map0(f, rDepthFirst, 
                                FromLevel - 1, EndLevel - 1,
                                NegFromLevel + 1, NegEndLevel + 1, 
                                bHeads, _CONTEXT);
            MSequence_SetAtX(t, 0, tr);
            rDepthFirst = MExpr_CreateHeadSeqX(f, t);
        }
        else
        {
            NegLevel--;
            if ((NegLevel >= NegFromLevel) && (NegLevel <= NegEndLevel))
            {
                int i;
                MExpr h = lst->HeadSeq.Head;
                MSequence *ps = lst->HeadSeq.pSequence;
                MSequence *t = MSequence_Create(MSequence_Len(ps));

                for (i = 0; i < MSequence_Len(ps); i++)
                {
                    MExpr tt = MSequence_EleAt(ps, i);
                    MSequence *ele = MSequence_Create(1);
                    MExpr tr = BIF_Map0(f, tt, 
                                        FromLevel - 1, EndLevel - 1,
                                        NegFromLevel + 1, NegEndLevel + 1, 
                                        bHeads, _CONTEXT);
                    
                    MSequence_SetAtX(ele, 0, tr);
                    MSequence_SetAtX(t, i, MExpr_CreateHeadSeqX(f, ele));
                }

                if (bHeads)
                {
                    MSequence *ele = MSequence_Create(1);
                    MSequence_SetAt(ele, 0, h);
                    rDepthFirst = MExpr_CreateHeadXSeqX(MExpr_CreateHeadSeqX(f, ele), t);
                }
                else
                    rDepthFirst = MExpr_CreateHeadSeqX(h, t);
            }
        }
    }
    else;

    // go somewhere deeper
    if (!bTranversed)
    {
        int i;
        MExpr h = lst->HeadSeq.Head;
        MSequence *ps = lst->HeadSeq.pSequence;
        MSequence *t = MSequence_Create(MSequence_Len(ps));

        for (i = 0; i < MSequence_Len(ps); i++)
        {
            MSequence_SetAtX(t, i, 
                    BIF_Map0(f, MSequence_EleAt(ps, i), 
                             FromLevel - 1, EndLevel - 1, 
                             NegFromLevel + 1, NegEndLevel + 1, 
                             bHeads, _CONTEXT));
        }

        rDepthFirst = MExpr_CreateHeadSeqX(h, t);
    }

    return rDepthFirst;
}

static MExpr BIF_Map1(const MExpr f, const MExpr lst,
                     const int FromLevel, const int EndLevel, const bool bHeads context_param)
{
    //MExpr h = lst->HeadSeq.Head;
    //MSequence *ps = lst->HeadSeq.pSequence;
    return NULL;
}

static MExpr BIF_Map(const MExpr f, const MExpr lst, 
                    const int FromLevel, const int EndLevel, const bool bHeads context_param)
{
    return EndLevel >= 0 ? BIF_Map0(f, lst, FromLevel, EndLevel, 0, 0, bHeads, _CONTEXT) : 
            BIF_Map1(f, lst, FromLevel, EndLevel, bHeads, _CONTEXT);
}

// Args:
// Map[f, lst]    ==> Map[f, lst, {1}]
// Map[f, lst, n] ==> Map[f, lst, {1, n}] (if n >= 1) or Map[f, lst, {n, -1}] (if n <= -1)
// Map[f, lst, {n}]
// Map[f, lst, {m, n}]:
// Map[f, lst, {n, Infinity}]
// Map[f, lst, {-Infinity, n}]
// Options: Heads -> True
def_bif(Map)
{
    MExpr r;
    bool bHeads = false;
    MSequence *newseq;

    // Hardcoded: check Heads -> ... option
    if (MSequence_Len(seq))
    {
        MExpr opt = MSequence_EleAt(seq, MSequence_Len(seq) - 1);
        if ((opt->Type == etHeadSeq) && is_header(opt, sym_Rule) 
                && (2 == MSequence_Len(opt->HeadSeq.pSequence)))
        {
            bHeads = is_true(MSequence_EleAt(opt->HeadSeq.pSequence, 1));
            newseq = MSequence_Copy(seq, 0, MSequence_Len(seq) - 1);
        }
        else
        {
            XRef_IncRef(seq);
            newseq = seq;
        }
    }
    else
        return MExpr_CreateHeadSeq(sym_Map, seq);

    if (check_arg_types(".X", newseq, sym_Map, false, _CONTEXT))
        r = BIF_Map(MSequence_EleAt(newseq, 0), MSequence_EleAt(newseq, 1), 1, 1, bHeads, _CONTEXT);
    else if (check_arg_types(".XT", newseq, sym_Map, false, _CONTEXT))
    {
        int n = ToIntWithClipInfinity(MSequence_EleAt(newseq, 2));
        if (n == 0)
            r = BIF_Map(MSequence_EleAt(newseq, 0), MSequence_EleAt(newseq, 1), 0, 0, bHeads, _CONTEXT);
        else if (n > 1)
            r = BIF_Map(MSequence_EleAt(newseq, 0), MSequence_EleAt(newseq, 1), 1, n, bHeads, _CONTEXT);
        else
            r = BIF_Map(MSequence_EleAt(newseq, 0), MSequence_EleAt(newseq, 1), n, -1, bHeads, _CONTEXT);
    }
    else if (check_arg_types(".X{T}", newseq, sym_Map, false, _CONTEXT))
    {
        MSequence *pRange = MSequence_EleAt(newseq, 2)->HeadSeq.pSequence;
        int n = ToIntWithClipInfinity(MSequence_EleAt(pRange, 0));
        r = BIF_Map(MSequence_EleAt(newseq, 0), MSequence_EleAt(newseq, 1), n, n, bHeads, _CONTEXT);
    }
    else if (check_arg_types(".X{TT}", newseq, sym_Map, true, _CONTEXT))
    {
        MSequence *pRange = MSequence_EleAt(newseq, 2)->HeadSeq.pSequence;
        int m = ToIntWithClipInfinity(MSequence_EleAt(pRange, 0));
        int n = ToIntWithClipInfinity(MSequence_EleAt(pRange, 1));
        r = BIF_Map(MSequence_EleAt(newseq, 0), MSequence_EleAt(newseq, 1), m, n, bHeads, _CONTEXT);
    }
    else
        r = MExpr_CreateHeadSeq(sym_Map, seq);

    MSequence_Release(newseq);

    return r;
}

def_bif(Range)
{
    MSequence *ps;
    MExpr rr = NULL;

    if (check_arg_types("N", seq, sym_Map, false, _CONTEXT))
    {
        int r = Num_ToInt(MSequence_EleAt(seq, 0)->Num);
        if (r >= 1)
        {
            int i;
            ps = MSequence_Create(r);
            for (i = 1; i <= r; i++)
                MSequence_SetAtX(ps, i - 1, MExpr_CreateMachineInt(i));
            return MExpr_CreateHeadSeqX(sym_List, ps);
        }
        else if (r <= -1)
        {
            int i;
            ps = MSequence_Create(-r);
            for (i = 1; i <= -r; i++)
                MSequence_SetAtX(ps, i - 1, MExpr_CreateMachineInt(-i));
            return MExpr_CreateHeadSeqX(sym_List, ps);
        }
        else;
    }
    else if (check_arg_types("NN", seq, sym_Map, false, _CONTEXT))
    {
        MNum start = MSequence_EleAt(seq, 0)->Num;
        MNum end = MSequence_EleAt(seq, 1)->Num;
        
        MNum off = Num_Subtract(end, start);
        MNum one = Num_Create(1);
        MNum len = Num_Div(off, one);
        MNum acc = Num_Uniquelize(start);
        int r = Num_ToInt(len) + 1;

        Num_Release(off);
        Num_Release(len);

        if (r >= 1)
        {
            int i;
            if ((ps = MSequence_Create(r)))
            {
                for (i = 1; i <= r; i++)
                {
                    MSequence_SetAtX(ps, i - 1, MExpr_CreateNumX(Num_Uniquelize(acc)));
                    Num_AddBy(acc, one);
                }
                rr = MExpr_CreateHeadSeqX(sym_List, ps);
            }
            else
            {
                // TODO: throw "out of memory"
                ;
            }
        }
        else;

        Num_Release(acc);
        Num_Release(one);
    }
    else if (check_arg_types("NNN", seq, sym_Map, true, _CONTEXT))
    {
        MNum start = MSequence_EleAt(seq, 0)->Num;
        MNum end = MSequence_EleAt(seq, 1)->Num;
        MNum step = MSequence_EleAt(seq, 2)->Num;

        MNum off = Num_Subtract(end, start);
        MNum len = Num_Div(off, step);
        MNum acc = Num_Uniquelize(start);
        int r = Num_ToInt(len) + 1;

        Num_Release(off);
        Num_Release(len);

        if (r >= 1)
        {
            int i;
            ps = MSequence_Create(r);
            for (i = 1; i <= r; i++)
            {
                MSequence_SetAtX(ps, i - 1, MExpr_CreateNumX(Num_Uniquelize(acc)));
                Num_AddBy(acc, step);
            }
            rr = MExpr_CreateHeadSeqX(sym_List, ps);
        }
        else;

        Num_Release(acc);
    }
    else;

    if (NULL == rr)
    {
        ps = MSequence_Create(0);
        rr = MExpr_CreateHeadSeqX(sym_List, ps);
    }

    return rr;
}

def_bif(Nest)
{
    // [0]: result MSequence
    // [1]: counter
    MSequence *ps = (MSequence *)Eval_GetCallTag(_Context);
    int counter;

    if (NULL == ps)
    {
        if (check_arg_types("..N", seq, sym_NestList, true, _CONTEXT))
        {
            counter = Num_ToInt(MSequence_EleAt(seq, 2)->Num);
            if (counter > 0)
            {
                MExpr t = MExpr_CreateHeadSeqX(MSequence_EleAt(seq, 0), 
                                               MSequence_SetAt(MSequence_Create(1),
                                                               0,
                                                               MSequence_EleAt(seq, 1)));                

                ps = MSequence_Create(3);
                MSequence_EleAt(ps, 1) =  (MExpr)(void*)(--counter);

                return Eval_RetCCEval(t, (MDword)(ps), _Context); 
            }
            else
            {
                MExpr r = MSequence_EleAt(seq, 1);
                XRef_IncRef(r);
                return r;
            }
        }
        else
            return MExpr_CreateHeadSeq(sym_NestList, seq);
    }
    else
    {
        MExpr r = Eval_GetCallExpr(_Context);
        int counter = (int)(MSequence_EleAt(ps, 1));

        if (counter > 0)
        {
            MExpr t = MExpr_CreateHeadSeqX(MSequence_EleAt(seq, 0), 
                                           MSequence_SetAtX(MSequence_Create(1),
                                                           0,
                                                           r));           
            MSequence_EleAt(ps, 1) =  (MExpr)(void*)(--counter);
            return Eval_RetCCEval(t, (MDword)(ps), _Context);
        }
        else
        {
            MSequence_EleAt(ps, 1) = NULL;
            MSequence_Release(ps);
            return r;
        }
    }
}

def_bif(NestList)
{
    // [0]: result MSequence
    // [1]: counter
    MSequence *ps = (MSequence *)Eval_GetCallTag(_Context);
    int counter;

    if (NULL == ps)
    {
        if (check_arg_types("..N", seq, sym_NestList, true, _CONTEXT))
        {
            counter = Num_ToInt(MSequence_EleAt(seq, 2)->Num);
            if (counter > 0)
            {
                MSequence *psr = MSequence_Create(counter + 1);
                MExpr t = MExpr_CreateHeadSeqX(MSequence_EleAt(seq, 0), 
                                               MSequence_SetAt(MSequence_Create(1),
                                                               0,
                                                               MSequence_EleAt(seq, 1)));                

                ps = MSequence_Create(3);
                MSequence_EleAt(ps, 0) =  (MExpr)(psr);
                MSequence_EleAt(ps, 1) =  (MExpr)(void*)(--counter);

                MSequence_SetAt(psr, 0, MSequence_EleAt(seq, 1));

                return Eval_RetCCEval(t, (MDword)(ps), _Context); 
            }
            else
            {
                MExpr r = MExpr_CreateHeadSeqX(sym_List,
                        MSequence_SetAt(MSequence_Create(1),
                                        0,
                                        MSequence_EleAt(seq, 1)));
                return r;
            }
        }
        else
            return MExpr_CreateHeadSeq(sym_NestList, seq);
    }
    else
    {
        MExpr r = Eval_GetCallExpr(_Context);
        MSequence *psr = (MSequence *)(MSequence_EleAt(ps, 0));
        int counter = (int)(MSequence_EleAt(ps, 1));

        MSequence_SetAtX(psr, MSequence_Len(psr) - 1 - counter, r);
        if (counter > 0)
        {
            MExpr t = MExpr_CreateHeadSeqX(MSequence_EleAt(seq, 0), 
                                           MSequence_SetAt(MSequence_Create(1),
                                                           0,
                                                           r));           
            MSequence_EleAt(ps, 1) =  (MExpr)(void*)(--counter);
            return Eval_RetCCEval(t, (MDword)(ps), _Context);
        }
        else
        {
            MSequence_EleAt(ps, 0) = NULL;
            MSequence_EleAt(ps, 1) = NULL;
            MSequence_Release(ps);
            return MExpr_CreateHeadSeqX(sym_List, psr);
        }
    }
}

#define get_item_n_to_t(r0, n, t)                                                                            \
    do {                                                                                                     \
        MExpr r314 = (r0);                                                                                   \
        int v314 = (n);                                                                                      \
        if ((v314 >= 1) && (v314 <= MSequence_Len(r314->HeadSeq.pSequence)))                                 \
            {                                                                                                \
                t = MSequence_EleAt(r314->HeadSeq.pSequence, v314 - 1);                                      \
            }                                                                                                \
            else if ((v314 <= -1) && (v314 >= -MSequence_Len(r314->HeadSeq.pSequence)))                      \
            {                                                                                                \
                t = MSequence_EleAt(r314->HeadSeq.pSequence, MSequence_Len(r314->HeadSeq.pSequence) + v314); \
            }                                                                                                \
            else if (v314 == 0)                                                                              \
            {                                                                                                \
                t = r314->HeadSeq.Head;                                                                      \
            }                                                                                                \
            else;                                                                                            \
    } while (false)

MExpr part_on_level(MExpr expr, int leveloff, int v)
{
    ASSERT(leveloff >= 1);
    if (expr->Type != etHeadSeq)
        return NULL;
    if (leveloff == 1)
    {
        MSequence *ps = MSequence_Create(MSequence_Len(expr->HeadSeq.pSequence));
        int i;
        for (i = 0; i < MSequence_Len(expr->HeadSeq.pSequence); i++)
        {
            MExpr r = MSequence_EleAt(expr->HeadSeq.pSequence, i); 
            MExpr t = NULL;
            get_item_n_to_t(r, v, t);
            if (t)
                MSequence_SetAt(ps, i, t);
            else
            {
                MSequence_Release(ps);
                return NULL;
            }
        }

        return MExpr_CreateHeadSeqX(expr->HeadSeq.Head, ps);
    }
    else
    {
        MSequence *ps = MSequence_Create(MSequence_Len(expr->HeadSeq.pSequence));
        int i;
        for (i = 0; i < MSequence_Len(expr->HeadSeq.pSequence); i++)
        {
            MExpr r = MSequence_EleAt(expr->HeadSeq.pSequence, i); 
            MExpr t = part_on_level(r, --leveloff, v);
            if (t)
                MSequence_SetAt(ps, i, t);
            else
            {
                MSequence_Release(ps);
                return NULL;
            }
        }

        return MExpr_CreateHeadSeqX(expr->HeadSeq.Head, ps);
    }
}

MExpr part_on_level_iter(MExpr expr, int leveloff, MSpanIter *iter)
{
    MInt index;

    ASSERT(leveloff >= 0);
    if (expr->Type != etHeadSeq)
        return NULL;
    switch (leveloff)
    {
    case 0:
        {
            MSequence *ps = MSequence_Create(SpanIterLen(iter));
            int i = 0;
            SpanIterReset(iter);
            while (SpanIterMachineNext(iter, index))
            {
                MExpr t = NULL;
                get_item_n_to_t(expr, index, t);
                if (t)
                    MSequence_SetAt(ps, i, t);
                else
                {
                    MSequence_Release(ps);
                    return NULL;
                }
                i++;
            }
            return MExpr_CreateHeadSeqX(expr->HeadSeq.Head, ps);
        }
        break;
    default:
        {
            MSequence *ps = MSequence_Create(MSequence_Len(expr->HeadSeq.pSequence));
            int i;
            for (i = 0; i < MSequence_Len(expr->HeadSeq.pSequence); i++)
            {
                MExpr r = MSequence_EleAt(expr->HeadSeq.pSequence, i); 
                MExpr t = part_on_level_iter(r, --leveloff, iter);
                if (t)
                    MSequence_SetAt(ps, i, t);
                else
                {
                    MSequence_Release(ps);
                    return NULL;
                }
            }

            return MExpr_CreateHeadSeqX(expr->HeadSeq.Head, ps);
        }
    }
}

MSpanIter *create_span_iter_for_len(MSequence *seq, MInt len)
{
    int start;
    int end;
    int step = 1;

#define try_get_para_i(i, t)    \
    do {    \
        MNum num;                                                   \
        if (MSequence_EleAt(seq, i)->Type != etNum) return NULL;    \
        num = MSequence_EleAt(seq, i)->Num;                         \
        if (!Num_IsMachineInt(num)) return NULL;                    \
        t = Num_ToInt(num);                                         \
    } while (false)

    switch (MSequence_Len(seq))
    {
    case 3:
        try_get_para_i(2, step);
        if (step == 0) return NULL;
        // go-on to "case 2"        
    case 2:
        try_get_para_i(0, start);
        if (MSequence_EleAt(seq, 1)->Type != etSymbol)
            try_get_para_i(1, end);
        else
        {
            if (MSequence_EleAt(seq, 1)->Symbol == sym_All)
            {
                if (step > 0)
                    end = start > 0 ? len : -1;
                else
                    end = start > 0 ? 1 : -len;
            }
            else
                return NULL;
        }
        if (end < start) step = -1;
        break;
    case 1:
        try_get_para_i(0, start);
        end = start;
        break;
    default:
        return NULL;
    }

    return CreateMachineSpanIter(start, end, step);
}

def_bif(Part)
{
    MExpr r;
    MSequence *ps;
    int i;
    bool b;
    int level = 1;
    int v;
    MExpr newr;
    if (!check_arg_types("X.*", seq, sym_Part, false, _CONTEXT))
        return MExpr_CreateHeadSeq(sym_Part, seq);

    if (MSequence_EleAt(seq, 1)->Type != etHeadSeq) goto common_case;

    ps = MSequence_UniqueCopy(seq, 1, MSequence_Len(seq) - 1);
    b = check_arg_types("L*", ps, sym_Part, false, _CONTEXT);
    MSequence_Release(ps);
    if (b)
    {
        MSequence *in = MSequence_EleAt(seq, 1)->HeadSeq.pSequence;

        ps = MSequence_Create(MSequence_Len(in));
        for (i = 0; i < MSequence_Len(ps); i++)
        {                
            MSequence tags;
            MSequence *arg = MSequence_InitStatic(&tags, 2);
            MSequence_SetAt(arg, 0, MSequence_EleAt(seq, 0));
            MSequence_SetAt(arg, 1, MSequence_EleAt(in , i));
            MSequence_SetAtX(ps, i, BIF_Part(arg, _Context));     
            MSequence_ReleaseStatic(arg);           
        }
       
        r = MExpr_CreateHeadSeqX(MSequence_EleAt(seq, 0)->HeadSeq.Head, ps);

        if (MSequence_Len(seq) > 2)
        {
            for (i = 0; i < MSequence_Len(ps); i++)
            {                
                MSequence *arg = MSequence_Create(MSequence_Len(seq) - 1);
                MSequence_SetAt(arg, 0, MSequence_EleAt(ps, i));
                MSequence_OverwriteSetFrom(arg, seq, 1, 2, MSequence_Len(seq) - 2);
                MSequence_SetAtX(ps, i, BIF_Part(arg, _Context));     
                MSequence_Release(arg);  
            }
            
        }
        else;

        return r; 
    }
    else;

common_case:
    r = MSequence_EleAt(seq, 0); XRef_IncRef(r);
    for (i = 1; i < MSequence_Len(seq); i++)
    {
        MExpr t = MSequence_EleAt(seq, i);
        if (r->Type != etHeadSeq) goto bif_error_quit;

        switch (t->Type)
        {
        case etNum:
            if (!Num_IsMachineInt(t->Num))
                goto bif_error_quit;
            
            ASSERT(level <= i);
            newr = NULL;
            v = Num_ToInt(t->Num);

            if (level == i)
            {
                get_item_n_to_t(r, v, newr);
                if (newr) XRef_IncRef(newr);
            }
            else
                newr = part_on_level(r, i - level, v);
            if (newr == NULL)
                goto bif_error_quit;
                    
            MExpr_Release(r);
            r = newr;
            level++;
            break;
        case etSymbol:
            if (t->Symbol != sym_All)
                goto bif_error_quit;
            break;
        case etHeadSeq:
            if (!is_header(t, sym_Span))
                goto bif_error_quit;
            {
                MSpanIter *iter = create_span_iter_for_len(t->HeadSeq.pSequence, 
                                                           MSequence_Len(r->HeadSeq.pSequence));
                if ((iter == NULL) || (SpanIterLen(iter) == 0))
                    goto bif_error_quit;

                newr = part_on_level_iter(r, i - level, iter);
                SpanIterDelete(iter);
                if (newr == NULL) goto bif_error_quit;
                MExpr_Release(r);
                r = newr;
            }
            level++;
            break;
        default:
            goto bif_error_quit;
        }
    }

    return r;

bif_error_quit:
    if (r) MExpr_Release(r);    
    return MExpr_CreateHeadSeq(sym_Part, seq);
}
