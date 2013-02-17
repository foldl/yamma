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

#include "matcher.h"
#include "bif.h"
#include "sys_sym.h"
#include "hash.h"
#include "sym.h"
#include "eval.h"
#include "bif_internal.h"

struct MMatchContext;
typedef int (* fOnElementMatched)(MExpr Expr, MMatchContext *Context);

typedef int (* fOnMatched)(MExpr Expr, void *param);

#define MATCH_OPT_GREEDY    0x00000001
#define MATCH_OPT_VERBATIM  0x00000002
#define MATCH_OPT_EXCEPT    0x00000008

static MExpr Eval_SubEvaluate(MExpr e, _MCONTEXT_)
{
    return NULL;
}

struct MMatchContext
{
    MHashTable *FinalRes;
    MHashTable *OptPatternRes;
    MExprList *target;
    MExprList *pattern;
    MExprList *results;
    MExprList *objHolder;   // all of the above containers does not hold objects' ref, 
                            // this one holds the newly created objs.
                            // So, this field is not actually an element of "context"
    MExprList *evalHolder;  // all of the Complex[Index, expr] objects in evalution are stored in here.
                            // So, this field is not actually an element of "context", either.
    _MCONTEXT_;

    MExpr emptySeq;

    int StackBottom;

    // Option handling
    MDword Option;          // current Option
    MStack *OptionStack;
    
    fOnMatched OnDone;
    void *param;
    
    fOnElementMatched OnBlankSeqMatched;
    void *BlankSeqMatchedParam;
};

#define MATCH_OK             0
#define MATCH_NO_MATCH      -1

#define MATCH_DONE_BREAK       -2
#define MATCH_DONE_CONTINUE    -3

#define is_greedy(C) ((C)->Option & MATCH_OPT_GREEDY)
#define is_verbatim(C) ((C)->Option & MATCH_OPT_VERBATIM)

#define mtrace  printf

void Match_PrintExpr(MExpr expr)
{
    extern MExpr BIF_Print(MSequence *seq, _MCONTEXT_);
    MSequence *seq = MSequence_Create(1);
    MSequence_SetAt(seq, 0, expr);
    MExpr_Release(BIF_Print(seq, NULL));
    MSequence_Release(seq);
}

static bool IsHead(MExpr expr, MExpr header)
{
    MExpr h = BIF_GetHead(expr);
    bool r = IsExprSameQ(h, header);
    MExpr_Release(h);   // BIF_GetHead creates a *new* obj
    return r;
}

// // HashTable of Rule/RuleDelayed expr
// //  Rule/RuleDelayed[key, value]
// 
// static MInt HashCmp_RuleExpr(const MExpr p1, const MExpr p2)
// {
//     return IsExprSameQ(get_seq_i(p1, 0), get_seq_i(p2, 0)) ? 1 : 0;
// }
// 
// static MHashTable *Match_CreateKVTable(const MInt Size)
// {
//     return MHashTable_Create(Size, (f_Hash1)(MExpr_Hash), NULL, (f_HashCompare)(HashCmp_RuleExpr));
// }
//

// HashTable of Rule/RuleDelayed expr
// [kay, value]: value is freed automaticaly
static MInt HashCmp_Expr(const MExpr p1, const MExpr p2)
{
    return IsExprSameQ(p1, p2) ? 1 : 0;
}

static MHashTable *Match_CreateKVTable(const MInt Size)
{
    return MHashTable_Create(Size, (f_Hash1)(MExpr_Hash), NULL, (f_HashCompare)(HashCmp_Expr));
}

static void Match_DestroyKVTable(MHashTable *Table)
{
    MKVTable_Destroy(Table, NULL);
}

/************************************************************************
Basic Pattern Objects: (+: done)
Blank
    +Blank[]
    +Blank[h]

BlankSequence
    +BlankSequence[]
    +BlankSequence[h]

BlankNullSequence
    +BlankNullSequence[]
    +BlankNullSequence[h]
------------------------------------------------------------------------------------------

Composite Patterns:
Alternatives
    +Alternatives[p1, p2, ...]
Repeated
    -Repeated[p]
    -Repeated[p, max]
    -Repeated[p, {min, max}]
    -Repeated[p, {n}]
RepeatedNull
    RepeatedNull[p]
Pattern
    +Pattern[name, p]
Except
    Except[c]
    Except[c, p]: match p, but not c
Longest, Shortest
    Longest[p]
    Longest[p, priority]
OptionsPattern: Matches options for following OptionValue
    OptionsPattern[]
    OptionsPattern[f]
    OptionsPattern[{a -> b, ...}]
        f[x_, OptionsPattern[]] := {x, OptionValue[a]}
        f[7, a -> uuu] --> {7, uuu}
        f[7] --> {7, get Options[f]}
        g[x_, OptionsPattern[{a -> foo}] := {x, OptionValue[a]}
        f[7] --> {7, foo}
PatternSequence
    PatternSequence[]
    PatternSequence[p1, p2, ...]
Verbatim
    +Verbatim[expr]: represents expr in pattern matching, requiring that expr be matched exactly as it appears, 
                    with no substitutions for blanks or other transformations. 
HoldPattern
    HoldPattern[expr]: is equivalent to expr for pattern matching, but maintains expr in an unevaluated form. 
------------------------------------------------------------------------------------------

Restrictions on Patterns:
Condition
    Condition[pat, test]
PatternTest
    PatternTest[pat, fun]
------------------------------------------------------------------------------------------

Pattern Defaults:
Optional
    Optional[p]    : if p is missing (cannot be matched), get v from Default
    Optional[p, v] : if p is missing (cannot be matched), use v as the match of p
Default
    Default[f]      : gives the default value for arguments of the function f obtained with a _. pattern object. 
    Default[f, i]   : gives the default value to use when _. appears as the i-th argument of f.
    Default[f, i, n]: gives the default value for the i-th argument out of a total of n arguments.
------------------------------------------------------------------------------------------

Related attributes:
Orderless, Flat, OneIdentity
*************************************************************************/

bool  Match_MatchQ(MExpr expr, MExpr pattern, _MCONTEXT_)
{
    MExpr r;
    bool b;

    if (pattern->Type != etHeadSeq)
        return IsExprSameQ(expr, pattern);
    if (expr->Type != etHeadSeq)
        return false;

    r = Match_MatchFirst(expr, pattern, _Context);
    b = (r != NULL);
    MExpr_Release(r);
    return b;
}

void Match_Execute(MExpr tarExpr, MExpr patExpr,
        fOnMatched OnDone, void *param, _MCONTEXT_);

static int Match_CollectOne(MExpr Expr, MExpr *Result)
{
    *Result = Expr;
    return MATCH_DONE_BREAK;
}

static int Match_CollectAll(MExpr Expr, MExprList *Result)
{
    MExprList_PushOrphan(Result, Expr);
    return MATCH_DONE_CONTINUE;
}

MExpr Match_MatchFirst(MExpr expr, MExpr pattern, _MCONTEXT_)
{
    MExpr r = NULL;
    Match_Execute(expr, pattern, (fOnMatched)Match_CollectOne, &r, _Context);
    if (r) Match_PrintExpr(r);
    return r ? r : MExpr_CreateSymbol(sym_DollarFailed);
}

MExpr Match_MatchAll(MExpr expr, MExpr pattern, _MCONTEXT_)
{
    MExprList *l = MExprList_Create();
    MSequence *s;
    Match_Execute(expr, pattern, (fOnMatched)Match_CollectAll, l, _Context);
    s = MSequence_FromExprList(l);
    MExprList_Release(l);
    return MExpr_CreateHeadSeqX(sym_List, s);
}

#define match_seq(cmpPos_Len, cmpLen_0, allow_null)                                             \
                                                                                                \
    if (StartPos cmpPos_Len MSequence_Len(seq))                                                 \
        return MATCH_NO_MATCH;                                                                  \
                                                                                                \
    if (is_greedy(MatchContext))                                                                \
    {                                                                                           \
        int pos = StartPos;                                                                     \
        if (h)                                                                                  \
        {                                                                                       \
            while (pos < MSequence_Len(seq))                                                    \
            {                                                                                   \
                MExpr t = MSequence_EleAt(seq, pos);                                            \
                if (IsHead(t, h))                                                               \
                    pos++;                                                                      \
                else                                                                            \
                    break;                                                                      \
            }                                                                                   \
                                                                                                \
            pos = pos - StartPos;                                                               \
        }                                                                                       \
        else                                                                                    \
        {                                                                                       \
            pos = MSequence_Len(seq) - StartPos;                                                \
        }                                                                                       \
                                                                                                \
        if (pos cmpLen_0 0)                                                                     \
            return MATCH_NO_MATCH;                                                              \
                                                                                                \
        while (!(pos cmpLen_0 0))                                                               \
        {                                                                                       \
            MSequence *slice = MSequence_UniqueCopy(seq, StartPos, pos);                        \
            MExpr t = MExpr_CreateHeadSeqX(sym_Sequence, slice);                                \
            r = MatchContext->OnBlankSeqMatched(t, MatchContext);                               \
            if (MATCH_DONE_BREAK == r)                                                          \
                break;                                                                          \
            pos--;                                                                              \
        }                                                                                       \
    }                                                                                           \
   else                                                                                         \
    {                                                                                           \
        if (allow_null)                                                                         \
        {                                                                                       \
            MSequence *slice = MSequence_Create(0);                                             \
            MExpr t = MExpr_CreateHeadSeqX(sym_Sequence, slice);                                \
            r = MatchContext->OnBlankSeqMatched(t, MatchContext);                               \
            if (MATCH_DONE_BREAK == r)                                                          \
                return MATCH_OK;                                                                \
        }                                                                                       \
        else;                                                                                   \
        if (h)                                                                                  \
        {                                                                                       \
            int pos = StartPos;                                                                 \
            while (pos < MSequence_Len(seq))                                                    \
            {                                                                                   \
                MExpr t = MSequence_EleAt(seq, pos);                                            \
                if (IsHead(t, h))                                                               \
                {                                                                               \
                    MSequence *slice = MSequence_UniqueCopy(seq, StartPos, (++pos) - StartPos); \
                    MExpr t = MExpr_CreateHeadSeqX(sym_Sequence, slice);                        \
                    r = MatchContext->OnBlankSeqMatched(t, MatchContext);                       \
                    if (MATCH_DONE_BREAK == r)                                                  \
                        break;                                                                  \
                }                                                                               \
                else                                                                            \
                    break;                                                                      \
            }                                                                                   \
        }                                                                                       \
       else                                                                                     \
        {                                                                                       \
            int pos = StartPos;                                                                 \
            while (pos < MSequence_Len(seq))                                                    \
            {                                                                                   \
                MSequence *slice = MSequence_UniqueCopy(seq, StartPos, (++pos) - StartPos);     \
                MExpr t = MExpr_CreateHeadSeqX(sym_Sequence, slice);                            \
                r = MatchContext->OnBlankSeqMatched(t, MatchContext);                           \
                if (MATCH_DONE_BREAK == r)                                                      \
                    break;                                                                      \
                                                                                                \
            }                                                                                   \
        }                                                                                       \
    }                                                                                           \
                                                                                                \
    return r;

static int Match_BlankSequence(MSequence *seq, const int StartPos, 
        MExpr Pattern, const bool bAllowNull, MMatchContext *MatchContext)
{
    MExpr h = NULL;
    int r = MATCH_NO_MATCH;
    ASSERT(Pattern->Type == etHeadSeq);
    if (MSequence_Len(Pattern->HeadSeq.pSequence) == 1)
        h = MSequence_EleAt(Pattern->HeadSeq.pSequence, 0);

    if (bAllowNull)
    {
        match_seq(>, <, true);
    }
    else
    {
        match_seq(>=, <=, false);
    }
}

static int Match_Repeat(MSequence *seq, const int StartPos, 
    MExpr Pattern, const bool bAllowNull, MMatchContext *MatchContext)
{
    MExpr pat = NULL;
    int iMin = bAllowNull ? 0 : 1;
    int iMax = M_MAX_INT;
    int i = 0;
    MSequence *ps = Pattern->HeadSeq.pSequence;
    _MCONTEXT_ = MatchContext->_Context;
    ASSERT(Pattern->Type == etHeadSeq);
    
    switch (MSequence_Len(ps))
    {
    case 2:
        if (check_arg_types(".T", ps, sym_Repeated, false, _CONTEXT))
        {
            iMax = ToIntWithClipInfinity(MSequence_EleAt(ps, 1));
        }
        else if (check_arg_types(".{T}", ps, sym_Repeated, false, _CONTEXT))
        {
            MSequence *pRange = MSequence_EleAt(ps, 1)->HeadSeq.pSequence;
            iMin = iMax = ToIntWithClipInfinity(MSequence_EleAt(pRange, 0));
        }
        else if (check_arg_types(".{TT}", ps, sym_Repeated, true, _CONTEXT))
        {
            MSequence *pRange = MSequence_EleAt(ps, 1)->HeadSeq.pSequence;
            iMin = ToIntWithClipInfinity(MSequence_EleAt(pRange, 0));
            iMax = ToIntWithClipInfinity(MSequence_EleAt(pRange, 1));
        }
        else
            iMax = -1;

        if (iMax < iMin)
            return MATCH_NO_MATCH;
    case 1:
        pat = MSequence_EleAt(Pattern->HeadSeq.pSequence, 0);
        break;
    default:
        return MATCH_NO_MATCH;
    }

    while (i < iMin)
    {
    }
    
    return MATCH_NO_MATCH;
}

static void Match_CloneContext(MMatchContext *Clone, MMatchContext *Context)
{
    Clone->FinalRes = MHashTable_Duplicate(Context->FinalRes);
    Clone->OptPatternRes = Context->OptPatternRes ? MHashTable_Duplicate(Context->OptPatternRes) : NULL;
    Clone->target  = MExprList_Mirror(Context->target);
    Clone->pattern = MExprList_Mirror(Context->pattern);
    Clone->results = MExprList_Mirror(Context->results);
    Clone->objHolder = Context->objHolder;
    Clone->evalHolder = Context->evalHolder;
    Clone->_Context = Context->_Context;
    Clone->OnDone = Context->OnDone;
    Clone->param = Context->param;
    Clone->OnBlankSeqMatched = Context->OnBlankSeqMatched;
    Clone->Option = Context->Option;
    Clone->OptionStack = MStack_Mirror(Context->OptionStack);
    Clone->StackBottom = Context->StackBottom;
    Clone->emptySeq = Context->emptySeq;
}

static void Match_OnUnmirrorComplex(MExprList *l, MExpr expr)
{
    ASSERT(expr->Type == etComplex);
    expr->Complex.Real = NULL;
    expr->Complex.Imag = NULL; 
    MExpr_Release(expr);
}

static void Match_ReleaseClone(MMatchContext *Clone)
{
    MHashTable_Destroy(Clone->FinalRes, NULL);
    if (Clone->OptPatternRes) MHashTable_Destroy(Clone->OptPatternRes, NULL);
    MExprList_ReleaseMirror(Clone->target, NULL);
    MExprList_ReleaseMirror(Clone->pattern, NULL);
    MExprList_ReleaseMirror(Clone->results, NULL);
    MStack_ReleaseMirror(Clone->OptionStack, NULL);
}

static int Match_Execute0(MMatchContext *MatchContext, const bool bResume);

static void Match_OnKVItem(MExprList *bag, MExpr key, MExpr value)
{
    MSequence *s = MSequence_Create(2);
    MExpr e;
    MSequence_SetAt(s, 0, key);
    MSequence_SetAt(s, 1, value);
    e = MExpr_CreateHeadSeqX(sym_List, s);
    Match_PrintExpr(e);
    MExprList_PushOrphan(bag, e);
}

static int Match_OnEleMatched(MExpr expr, MMatchContext *MatchContext);

void Match_ClearEvalStack(MExprList *l)
{
    while (MExprList_Len(l))
    {
        MExpr t = MExprList_Top(l);
        ASSERT(t->Type == etComplex);
        t->Complex.Real = NULL;
        t->Complex.Imag = NULL;
        MObj_Unlock(t);
        MExprList_Pop(l);
    }
}

void Match_Execute(MExpr tarExpr, MExpr patExpr,
        fOnMatched OnDone, void *param, _MCONTEXT_)

{
    MMatchContext MatchContext;
    MHashTable *res = Match_CreateKVTable(128);
    MExprList *target  = MExprList_Create();
    MExprList *pattern = MExprList_Create();
    MExprList *results = MExprList_Create();

    MExprList_PushOrphan(target, tarExpr);
    MExprList_PushOrphan(pattern, patExpr);

    // so much stupid fields need to be initialized
    MatchContext.FinalRes = res;
    MatchContext.OptPatternRes = NULL;
    MatchContext.target = target;
    MatchContext.pattern = pattern;
    MatchContext.results = results;
    MatchContext.objHolder = MExprList_Create();
    MatchContext.evalHolder = MExprList_Create();
    MatchContext._Context = _Context;
    MatchContext.OnDone = OnDone;
    MatchContext.param = param;
    MatchContext.OnBlankSeqMatched = Match_OnEleMatched;
    MatchContext.Option = MATCH_OPT_GREEDY;
    MatchContext.OptionStack = DWStack_CreateDwordStack();
    MatchContext.StackBottom = 0;
    MatchContext.emptySeq = MExpr_CreateEmptySeq();

    Match_Execute0(&MatchContext, false);

#define clear_and_rel(l) MExprList_Clear(l); MExprList_Release(l)
 
    clear_and_rel(target);
    clear_and_rel(pattern);
    clear_and_rel(results);

    MExprList_Release(MatchContext.objHolder);

    if (*(MExpr *)(MatchContext.param))
        Match_PrintExpr(*(MExpr *)(MatchContext.param));

    Match_ClearEvalStack(MatchContext.evalHolder);
    MExprList_Release(MatchContext.evalHolder);

    DWStack_Release(MatchContext.OptionStack);

    Match_DestroyKVTable(res);
    MExpr_Release(MatchContext.emptySeq);
}

#define store_eval_obj(n, expr)                 \
    MExpr_CreateComplexX((MNum)(n),             \
                         (MNum)(expr));         \
    MObj_Lock(entry314);                        \
    MExprList_PushOrphan(evalHolder, entry314);

#define push_match_n(stack, expr, n)                                                  \
do                                                                                    \
{                                                                                     \
    MExpr entry314 = store_eval_obj(n, expr);                                         \
    MExprList_PushOrphan((stack), entry314);                                          \
    MExprList_PushOrphan((stack), MSequence_EleAt((expr)->HeadSeq.pSequence, n - 1)); \
} while(false)

#define push_match_head(stack, expr)                     \
do                                                       \
{                                                        \
    MExpr entry314 = store_eval_obj(0, expr);            \
    MExprList_PushOrphan((stack), entry314);             \
    MExprList_PushOrphan((stack), (expr)->HeadSeq.Head); \
} while(false)

#define push_match_self(stack, expr)           \
do                                             \
{                                              \
    MExpr entry314 = store_eval_obj(-1, expr); \
    MExprList_PushOrphan((stack), entry314);   \
    MExprList_PushOrphan((stack), (expr));     \
} while(false)

#define split_exp_index(comp, expr, index) \
do {                                        \
    index = (MDword)(comp->Complex.Real);   \
    expr = (MExpr)(comp->Complex.Imag);     \
} while (false)

#define inc_index_by(comp, n)   \
    comp->Complex.Real = (MNum)(void *)((MDword)(comp->Complex.Real) + n)

#define update_index_by(comp, n) \
do {                                        \
    comp = MExpr_CreateComplexX((MNum)(void *)((MDword)(comp->Complex.Real) + (n)), \
                                (MNum)(comp->Complex.Imag));        \
    MExprList_PushOrphan(evalHolder, comp);                         \
} while (false)    

#define update_index(comp) update_index_by(comp, 1)

#define store_result(expr)          MExprList_PushOrphan(results, (expr))
#define reg_newexpr(expr)           MExprList_PushOrphan(MatchContext->objHolder, (expr))

#define push_set_property(opt)  \
do                              \
{                               \
    DWStack_Push(MatchContext->OptionStack, MatchContext->Option);    \
    MatchContext->Option |= (opt);         \
} while (false)

#define push_clear_property(opt)                                   \
do                                                                 \
{                                                                  \
    DWStack_Push(MatchContext->OptionStack, MatchContext->Option); \
    MatchContext->Option &= ~(opt);                                \
} while (false)

#define pop_property()          \
do                              \
{                               \
    MatchContext->Option = DWStack_Pop(MatchContext->OptionStack);    \
} while (false)

static int Match_OnEleMatched(MExpr expr, MMatchContext *MatchContext)
{
    int r;
    MMatchContext clone;
    MExprListIter StartIter = (MExprListIter)(MatchContext->BlankSeqMatchedParam);
    MExpr StartEntry = MExprListIter_Get(StartIter);
    MExpr StartExpr;
    MExprListIter tarIter = StartIter;
    int StartIndex;
    int dirtyNo = 1;
    const int matchLen = MSequence_Len(expr->HeadSeq.pSequence) - 1;

    split_exp_index(StartEntry, StartExpr, StartIndex);
    reg_newexpr(expr);
    Match_CloneContext(&clone, MatchContext);
    MExprList_PushOrphan(clone.results, expr);

    // fix up target stack
    if (matchLen)
    {
        while (MExprListIter_Get(tarIter))
        {
            MExpr tarExpr;
            int tarIndex;
            split_exp_index(MExprListIter_Get(tarIter), tarExpr, tarIndex);
            if (tarExpr == StartExpr)
            {
                inc_index_by(MExprListIter_Get(tarIter), matchLen);
                tarIter = MExprListIter_Prev(tarIter);
                dirtyNo++;
            }
            else
                break;
        }
    }
    else;

    r = Match_Execute0(&clone, true);
    Match_ReleaseClone(&clone);

    // restore target stack
    {
        tarIter = StartIter;
        while (--dirtyNo)
        {
            inc_index_by(MExprListIter_Get(tarIter), -matchLen);
            tarIter = MExprListIter_Prev(tarIter);
        }
    }

    return r;
}

static bool IsRuleExpr(MExpr expr, MSymbol sym)
{
    return (expr->Type == etHeadSeq)
        && (is_header(expr, sym))
        && (MSequence_Len(expr->HeadSeq.pSequence) == 2);
}

static bool IsRuleExpr(MExpr expr)
{
    return (expr->Type == etHeadSeq)
        && (is_header(expr, sym_Rule) || is_header(expr, sym_RuleDelayed))
        && (MSequence_Len(expr->HeadSeq.pSequence) == 2);
}

static MExpr Match_ReplaceExec(MHashTable *Table, MExpr Expr, _MCONTEXT_);

#define try_store_kv(t, k, v, OnNoMatch)                      \
do {                                                          \
    MExpr v314 = (v);                                         \
    MExpr oldV314 = (MExpr)MKVTable_FindOrInsert(t, k, v314); \
    if (oldV314)                                              \
    {                                                         \
        if (!IsExprSameQ(oldV314, v314))                      \
        {                                                     \
            bMatched = false;                                 \
            OnNoMatch;                                        \
        }                                                     \
    }                                                         \
} while (false)

static int Match_Execute0(MMatchContext *MatchContext, const bool bResume)
{
    MExprList *target   = MatchContext->target;     // no ref inc
    MExprList *pattern  = MatchContext->pattern;    // no ref inc
    MExprList *results  = MatchContext->results;    
    MExprList *evalHolder = MatchContext->evalHolder;
    MHashTable *res     = MatchContext->FinalRes;
    const int StackBottom = MatchContext->StackBottom;
    bool bPatternOK = true;
    bool bMatched = true;
    MExpr tarExpr;
    MExpr patExpr;
    MSymbol SpecialSym;

    if (bResume)
        goto stack_top_done;

#define is_sym_equal(s) (sym == sym_##s)

    while (bPatternOK)
    {
        MSymbol sym;
        int tarIndex;
        int patIndex;
        MExpr tarEntry;
        MExpr patEntry;

        tarExpr = MExprList_PopOrphan(target);
        patExpr = MExprList_PopOrphan(pattern);
        SpecialSym = NULL;

        switch (patExpr->Type)
        {
        case etNum:
        case etComplex:
        case etString:
        case etSymbol:
        case etCons:
            if (IsExprSameQ(tarExpr, patExpr))
                store_result(tarExpr);
            else
                bMatched = false;
            goto stack_top_done;               
        default:
            // verbatim?
            if (is_verbatim(MatchContext))
            {
                if (IsExprSameQ(tarExpr, patExpr))
                    store_result(tarExpr);
                else
                    bMatched = false;
                goto stack_top_done;   
            }
            else;
        };

        ASSERT(patExpr->Type == etHeadSeq);

        if (patExpr->HeadSeq.Head->Type != etSymbol)
        {
            push_match_head(target, tarExpr);
            push_match_head(pattern, patExpr);
            continue;
        }
        else;

        // TODO: handle Attributes: Orderless, Flat, OneIdentity
        //

        sym = patExpr->HeadSeq.Head->Symbol;
        if (sym == sym_Blank)
        {
            switch (MSequence_Len(patExpr->HeadSeq.pSequence))
            {
            case 1:
                if (IsHead(tarExpr, get_seq_i(patExpr, 0)))
                    store_result(tarExpr);
                else
                    bMatched = false;
                break;
            case 0:
                store_result(tarExpr);
                break;
            default:
                bPatternOK = false;
            }
        }
        else if (sym == sym_Alternatives)
        {
            if (MSequence_Len(patExpr->HeadSeq.pSequence) > 0)
            {
                push_match_n(pattern, patExpr, 1);
                push_match_self(target, tarExpr);  
                continue;      
            }
            else
                bMatched = false;
        }
        else if (sym == sym_Pattern)
        {
            if (MSequence_Len(patExpr->HeadSeq.pSequence) == 2)
            {
                push_match_n(pattern, patExpr, 2);
                push_match_self(target, tarExpr);
                continue;
            }
            else
                bPatternOK = false;
        }
        else if (sym == sym_Except)
        {
            if (MSequence_Len(patExpr->HeadSeq.pSequence) == 1)
            {
                push_match_n(pattern, patExpr, 1);
                push_match_self(target, tarExpr);
                continue;
            }
            else
                bPatternOK = false;
        }
        else if (sym == sym_Longest)
        {
            if (MSequence_Len(patExpr->HeadSeq.pSequence) == 1)
            {
                push_set_property(MATCH_OPT_GREEDY);
                push_match_n(pattern, patExpr, 1);
                push_match_self(target, tarExpr);
                continue;
            }
            else
                bPatternOK = false;
        }
        else if (sym == sym_Shortest)
        {
            if (MSequence_Len(patExpr->HeadSeq.pSequence) == 1)
            {
                push_clear_property(MATCH_OPT_GREEDY);
                push_match_n(pattern, patExpr, 1);
                push_match_self(target, tarExpr);
                continue;
            }
            else
                bPatternOK = false;
        }
        else if (sym == sym_Verbatim)
        {
            if (MSequence_Len(patExpr->HeadSeq.pSequence) == 1)
            {
                push_set_property(MATCH_OPT_VERBATIM);
                push_match_n(pattern, patExpr, 1);
                push_match_self(target, tarExpr);
                continue;
            }
            else
                bPatternOK = false;
        }
        else if (sym == sym_HoldPattern)
        {
            if (MSequence_Len(patExpr->HeadSeq.pSequence) == 1)
            {
                push_match_n(pattern, patExpr, 1);
                push_match_self(target, tarExpr);
                continue;
            }
            else
                bPatternOK = false;
        }
        else if (sym == sym_OptionsPattern)
        {
            int Len = MExprList_Len(target);
            MExpr tarEntry = MExprList_Top(target);
            MExpr   tarExpr = NULL;
            int tarIndex;
            int i;

            if (Len <= StackBottom)
                goto stack_top_done;
            
            split_exp_index(tarEntry, tarExpr, tarIndex);

            if (tarIndex < 1)
            {
                bPatternOK = false;
                goto stack_top_done;
            }
            else;

            for (i = tarIndex - 1; i < MSequence_Len(tarExpr->HeadSeq.pSequence); i++)
            {
                if (IsRuleExpr(get_seq_i(tarExpr, i)))
                {
                    MExpr tt = get_seq_i(tarExpr, i);
                    MExpr symExpr = get_seq_i(tt, 0);
                    MExpr tar = get_seq_i(tt, 1);

                    if (MatchContext->OptPatternRes == NULL)
                        MatchContext->OptPatternRes = Match_CreateKVTable(128);

                    // store in KV table
                    try_store_kv(MatchContext->OptPatternRes, symExpr, tar, break);
                }
                else
                    break;
            }
        }
        else if (sym == sym_PatternSequence)
        {
            if (MSequence_Len(patExpr->HeadSeq.pSequence) > 0)
            {
                push_match_n(pattern, patExpr, 1);
                push_match_self(target, tarExpr);
                continue;
            }
            else
            {
                MExpr t = MExpr_CreateHeadSeqX(sym_Sequence, MSequence_Create(0));
                reg_newexpr(t);
                store_result(t);
                goto stack_top_done;
            }
        }
        else if ((sym == sym_Condition)
               || (sym == sym_PatternTest))
        {
            if (MSequence_Len(patExpr->HeadSeq.pSequence) == 2)
            {
                push_match_n(pattern, patExpr, 1);
                push_match_self(target, tarExpr);
                continue;
            }
            else
                bPatternOK = false;   
        }
        else if (sym == sym_Optional)
        {
            int Len = MExprList_Len(target);
            MExpr tarForOpt = NULL;
            MSymbol theSym = NULL;
            MExpr   symExpr = NULL;
            MExpr   optHead = NULL;
            MExpr temp;
            bool bOptPat = false;

            if (!bMatched || (Len <= StackBottom))
                goto stack_top_done;

            if ((MSequence_Len(patExpr->HeadSeq.pSequence) < 1)
               || (MSequence_Len(patExpr->HeadSeq.pSequence) > 2))
            {
                bPatternOK = false;
                goto stack_top_done;
            }

            temp = get_seq_i(patExpr, 0);
            switch (temp->Type)
            {
            case etSymbol:
                // s
                theSym = temp->Symbol;
                symExpr = temp;
                break;
            case etHeadSeq:
                // Pattern[s, Blank[]] or Pattern[s, Blank[h]]
                if (is_header(temp, sym_Pattern) 
                        && (MSequence_Len(temp->HeadSeq.pSequence) == 2)
                        && (get_seq_i(temp, 0)->Type == etSymbol)
                        && (get_seq_i(temp, 1)->Type == etHeadSeq)
                        && is_header(get_seq_i(temp, 1), sym_Blank))
                {
                    bOptPat = true;
                    if (MSequence_Len(get_seq_i(temp, 1)->HeadSeq.pSequence) == 0)
                    {
                        symExpr = get_seq_i(temp, 0);
                        theSym = get_seq_i(temp, 0)->Symbol;
                    }
                    else if (MSequence_Len(get_seq_i(temp, 1)->HeadSeq.pSequence) == 1)
                    {
                        symExpr = get_seq_i(temp, 0);
                        theSym = get_seq_i(temp, 0)->Symbol;
                        optHead = get_seq_i(get_seq_i(temp, 1), 0);
                    }
                    else;
                }
                break;
            default:
                break;
            }
            
            if (theSym == NULL)
            {
                bPatternOK = false;
                goto stack_top_done;
            }

            if (Len > StackBottom)
            {
                tarEntry = MExprList_Top(target);
                split_exp_index(tarEntry, tarExpr, tarIndex);
                ASSERT((tarExpr->Type == etHeadSeq) || (tarIndex == -1)); 

                if (tarIndex <= MSequence_Len(tarExpr->HeadSeq.pSequence))
                    tarForOpt = get_seq_i(tarExpr, tarIndex - 1);
            }
            else
                tarForOpt = tarExpr;

            if (tarForOpt)
            {
                // match against Pattern
                if (optHead)
                    bMatched = IsHead(tarForOpt, optHead) ? bPatternOK : false;
                else;
            }
            else
            {
                if (MSequence_Len(patExpr->HeadSeq.pSequence) == 2)
                {
                    tarForOpt = MSequence_EleAt(patExpr->HeadSeq.pSequence, 1);
                }
                else
                {
                    // query from MSymbol
                    int index = -1;
                    if (MExprList_Len(pattern) > StackBottom)
                    {
                        MExpr patEntry = MExprList_Top(pattern);
                        MExpr patExpr;
                        split_exp_index(patEntry, patExpr, index);
                    }
                    else;
                    tarForOpt = MSymbol_GetDefaultValue(theSym, index, -1);
                    if (NULL == tarForOpt)
                    {
                        bMatched = false;
                        bPatternOK = false;
                    }
                }

                if (bPatternOK && bMatched)
                {
                    // store in KV table
                    try_store_kv(res, symExpr, tarForOpt, ;);
                }
            }
        }
        else if (is_sym_equal(BlankSequence)
                || is_sym_equal(BlankNullSequence)
                || is_sym_equal(Repeated)
                || is_sym_equal(RepeatedNull))
        {
            MExprListIter patIter = MExprList_GetIter(pattern);
            MExprListIter tarIter = MExprList_GetIter(target);
            int Len = MExprList_Len(target);
            MExpr blankExpr = patExpr;
            bool bRepeat = is_sym_equal(Repeated) || is_sym_equal(RepeatedNull);
            
            if (!bMatched)
                goto stack_top_done;

            ASSERT(MExprList_Len(pattern) == MExprList_Len(target));

            // let's backtrack for the index in a sequence for pattern matching
            while (Len > StackBottom)
            {
                tarEntry = MExprListIter_Get(tarIter);
                patEntry = MExprListIter_Get(patIter);

                split_exp_index(tarEntry, tarExpr, tarIndex);
                split_exp_index(patEntry, patExpr, patIndex);

                ASSERT((tarExpr->Type == etHeadSeq) || (tarIndex == -1)); 
                ASSERT(patExpr->Type == etHeadSeq);

                if (tarIndex == -1)
                {
                    tarIter = MExprListIter_Prev(tarIter);
                    patIter = MExprListIter_Prev(patIter);
                }
                else if (tarIndex >= 1)
                {
                    MatchContext->BlankSeqMatchedParam = tarIter;
                    return bRepeat ?
                            Match_Repeat(tarExpr->HeadSeq.pSequence,
                                         tarIndex - 1,
                                         blankExpr,
                                         is_sym_equal(RepeatedNull),
                                         MatchContext)
                            :
                            Match_BlankSequence(tarExpr->HeadSeq.pSequence,
                                               tarIndex - 1,
                                               blankExpr,
                                               is_sym_equal(BlankNullSequence),
                                               MatchContext);
                }
                else
                    break;
            }

            bMatched = false;
        }
        else
        {
            if (tarExpr->Type == etHeadSeq)
            {
                push_match_head(target, tarExpr);
                push_match_head(pattern, patExpr);
                continue;
            }
            else
                bMatched = false;
        }

stack_top_done:

        ASSERT(MExprList_Len(pattern) == MExprList_Len(target));

        // the big loop ends here
        if (MExprList_Len(pattern) <= StackBottom)
            break;

        tarEntry = MExprList_PopOrphan(target);
        patEntry = MExprList_PopOrphan(pattern);

        split_exp_index(tarEntry, tarExpr, tarIndex);
        split_exp_index(patEntry, patExpr, patIndex);

        ASSERT((tarExpr->Type == etHeadSeq) || (tarIndex == -1)); 
        ASSERT(patExpr->Type == etHeadSeq);

        if (patExpr->HeadSeq.Head->Type != etSymbol)
            goto basic_case;

        sym = patExpr->HeadSeq.Head->Symbol;

        if (is_sym_equal(BlankSequence)
            || is_sym_equal(BlankNullSequence))
        {
            ASSERT(!bMatched);
            bMatched = false;
            goto stack_top_done;
        }
        else if (sym == sym_Alternatives)
        {
            if (bMatched)
            {
                store_result(tarExpr);
                goto stack_top_done;
            }
            else if (patIndex < MSequence_Len(patExpr->HeadSeq.pSequence))
            {
                update_index(patEntry);
                MExprList_PushOrphan(pattern, patEntry);
                MExprList_PushOrphan(pattern, MSequence_EleAt(patExpr->HeadSeq.pSequence, patIndex));

                push_match_self(target, tarExpr);

                bMatched = true;

                continue;
            }
            else
            {
                bMatched = false;
                goto stack_top_done;
            }
        }
        else if (sym == sym_Pattern)
        {
            ASSERT(patIndex == 2);
            if (bMatched)
            {
                MExpr name = get_seq_i(patExpr, 0);
                if ((name->Type == etSymbol) && (MExprList_Len(results) > 0))
                {
                    // store in KV table
                    try_store_kv(res, name, MExprList_Top(results), ;);
                }
                else
                    bPatternOK = false;
            }
            else;
            goto stack_top_done;
        }
        else if (sym == sym_Except)
        {
            bMatched = !bMatched;
            if (bMatched) store_result(tarExpr);
            goto stack_top_done;
        }
        else if (sym == sym_Longest)
        {
            pop_property();
            goto stack_top_done;
        }
        else if (sym == sym_Shortest)
        {
            pop_property();    
            goto stack_top_done;
        }
        else if (sym == sym_Verbatim)
        {
            pop_property();    
            goto stack_top_done;
        }
        else if (sym == sym_HoldPattern)
        {
            if (bMatched)
            {
                store_result(tarExpr);
            }
            else;
            goto stack_top_done;
        }
        else if (sym == sym_PatternSequence)
        {
            if (bMatched && (patIndex < MSequence_Len(patExpr->HeadSeq.pSequence)))
            {
                update_index(patEntry);
                MExprList_PushOrphan(pattern, patEntry);
                MExprList_PushOrphan(pattern, MSequence_EleAt(patExpr->HeadSeq.pSequence, patIndex));
                push_match_self(target, tarExpr);
                continue;
            }
            else
            {
                MSequence *seq;
                MExpr t;
                if (bMatched)
                {
                    seq = MSequence_Create(patIndex);
                    int i = patIndex;
                    ASSERT(patIndex >= MExprList_Len(results));
                    while (--i >= 0) MSequence_SetAt(seq, i, MExprList_PopOrphan(results));
                }
                else
                    seq = MSequence_Create(0);
                t = MExpr_CreateHeadSeqX(sym_Sequence, seq);
                reg_newexpr(t);
                store_result(t);
                goto stack_top_done;
            }
        }
        else if (sym == sym_Condition)
        {
            if (bMatched)
            {
                MExpr r = Match_ReplaceExec(res, get_seq_i(patExpr, 1), MatchContext->_Context);
                bMatched = is_true(r);
                MExpr_Release(r);
            }
            else;     
            goto stack_top_done;
        }
        else if (sym == sym_PatternTest)
        {
            if (bMatched && (MExprList_Len(results) > 0))
            {
                MExpr r;
                MExpr newExpr;
                MSequence *seq = MSequence_Create(1);
                MSequence_SetAt(seq, 0, MExprList_Top(results));
                r = MExpr_CreateHeadSeqX(get_seq_i(patExpr, 1), seq);
                newExpr = Eval_SubEvaluate(r, MatchContext->_Context);
                bMatched = is_true(newExpr);
                MExpr_Release(newExpr);
                MExpr_Release(r);
            }
            else;     
            goto stack_top_done;     
        }
        else;

basic_case:

        if (patIndex < MSequence_Len(patExpr->HeadSeq.pSequence))
        {    
            if (tarIndex < MSequence_Len(tarExpr->HeadSeq.pSequence))
            {
                update_index(tarEntry);
                update_index(patEntry);
                MExprList_PushOrphan(target, tarEntry);
                MExprList_PushOrphan(pattern, patEntry);

                MExprList_PushOrphan(target, MSequence_EleAt(tarExpr->HeadSeq.pSequence, tarIndex));
                MExprList_PushOrphan(pattern, MSequence_EleAt(patExpr->HeadSeq.pSequence, patIndex));
            }
            else
            {
                update_index(tarEntry);
                update_index(patEntry);
                MExprList_PushOrphan(target, tarEntry);
                MExprList_PushOrphan(pattern, patEntry);

                MExprList_PushOrphan(target, MatchContext->emptySeq);
                MExprList_PushOrphan(pattern, MSequence_EleAt(patExpr->HeadSeq.pSequence, patIndex));
            }

            continue;
        }
        else if ((tarIndex == MSequence_Len(tarExpr->HeadSeq.pSequence)) &&
            (patIndex == MSequence_Len(patExpr->HeadSeq.pSequence)))
        {
            store_result(tarExpr);
            goto stack_top_done;
        }
        else
        {
            // match failed
            bMatched = false;
            goto stack_top_done;
        }
    }

    ASSERT(MExprList_Len(pattern) == MExprList_Len(target));

    // call OnDone
    if (bMatched && bPatternOK)
    {
        int r;
        MExprList *bag = MExprList_Create();
        MSequence *s;
        MKVTable_Traverse(res, (f_KVHashTraverse)Match_OnKVItem, bag);
        s = MSequence_FromExprList(bag);
        r = MatchContext->OnDone(MExpr_CreateHeadSeqX(sym_List, s), MatchContext->param);
        MExprList_Release(bag);

        return r;
    }
    else
        return MATCH_NO_MATCH;
}

// we are using recurrsion now, which might be not suitable for LARGE expr
static MExpr Match_Replace(MHashTable *Table, MExpr Expr)
{
    MExpr r;
    int i;
    MSequence *seq;
    MSequence *newSeq;
    MExpr header;
    bool bNew = false;
    switch (Expr->Type)
    {
    case etSymbol:
        r = (MExpr)MKVTable_Find(Table, Expr);
        r = r ? r : Expr;
        XRef_IncRef(r);
        return r;
    case etHeadSeq:
        header = Expr->HeadSeq.Head;
        switch (header->Type)
        {
        case etSymbol:
            header = (MExpr)MKVTable_Find(Table, header);
            if (header)
                bNew = true;
            else
            {
                header = Expr->HeadSeq.Head;
                XRef_IncRef(header);
            }
            break;
        case etHeadSeq:
            bNew = true;
            header = Match_Replace(Table, header);
            break;
        default:
            XRef_IncRef(header);
        }

        seq = Expr->HeadSeq.pSequence;
        newSeq = MSequence_Create(MSequence_Len(seq));
        for (i = 0; i < MSequence_Len(seq); i++)
        {
            switch (MSequence_EleAt(seq, i)->Type)
            {
            case etSymbol:
                r = (MExpr)MKVTable_Find(Table, MSequence_EleAt(seq, i));
                if (r)
                {
                    bNew = true;
                    MSequence_SetAtX(newSeq, i, r);
                }
                else
                    MSequence_SetAt (newSeq, i, MSequence_EleAt(seq, i));
                break;
            case etHeadSeq:
                bNew = true;
                MSequence_SetAtX(newSeq, i, Match_Replace(Table, MSequence_EleAt(seq, i)));
                break;
            default:
                MSequence_SetAt (newSeq, i, MSequence_EleAt(seq, i));
            }
        }
        if (bNew)
            r = MExpr_CreateHeadXSeqX(header, newSeq);
        else
        {
            r = Expr;
            XRef_IncRef(r);
        }
        return r;
    default:
        XRef_IncRef(Expr);
        return Expr;
    }
}

static MExpr Match_ReplaceExec(MHashTable *Table, MExpr Expr, _MCONTEXT_)
{
    MExpr newExpr = Match_Replace(Table, Expr);
    MExpr r = Eval_SubEvaluate(newExpr, _Context);
    MExpr_Release(newExpr);
    return r;
}
