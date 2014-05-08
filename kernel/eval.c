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
#include <stdlib.h>

#include "eval.h"
#include "sym.h"
#include "sys_sym.h"
#include "bif.h"
#include "msg.h"
#include "encoding.h"
#include "bif_internal.h"

typedef unsigned int uint;

// bif call with conti
#define EVAL_BIF_CC_MAX  9

#define EVAL_BIF_CC_EVAL 1
#define EVAL_BIF_CC_BIF  2

void BIF_Print0(MExpr expr, _MCONTEXT_);

typedef struct MEContext
{
    // core
    MStack *stack;
    MExprList *throws;
    MExpr result;
    MStack *topStack;    // lists of Top

    // for cc
    MExpr ccexpr;           // this is only a temp var for passing info
    MDword cctag;
    MStack *tagStack;

    // options
    bool bTrace;

    // stat.
    uint reduceNo;

    // limits
    uint MAX_RECURSION;
    uint MAX_ITERATION;
    uint MAX_REDUCE;
} *PEContext;

int Eval_Init()
{
    MString t = MString_NewC("KernelMain.m");
    MExpr expr = BIF_GetFile(t, NULL, NULL);
    MExpr r = Eval_Evaluate(expr);
    MString_Release(t);
    int rr = r->Type == etNum ? Num_ToInt(r->Num) : -1;
    MString_Release(expr);
    MString_Release(r);
    return rr;
}

static MSequence * Eval_FlattenSequence2(MSequence *Seq)
{
    int i;
    int len = MSequence_Len(Seq);
    int pos;
    MExpr *p = Seq->pExpr;
    bool bFound = false;
    MSequence *n = NULL;
    for (i = 0; i < MSequence_Len(Seq); i++, p++)
    {
        if (((*p)->Type == etHeadSeq) && ((*p)->HeadSeq.Head->Type == etSymbol)
                    && ((*p)->HeadSeq.Head->Symbol == sym_Sequence))
        {
            bFound = true;
            len += MSequence_Len((*p)->HeadSeq.pSequence) - 1;
        }
    }

    if (!bFound)
        return Seq;
    
    n = MSequence_Create(len);
    p = Seq->pExpr;
    pos = 0;
    for (i = 0; i < MSequence_Len(Seq); i++, p++)
    {
        if (((*p)->Type == etHeadSeq) && ((*p)->HeadSeq.Head->Type == etSymbol)
                    && ((*p)->HeadSeq.Head->Symbol == sym_Sequence))
        {
            int j;
            MExpr *p2 = (*p)->HeadSeq.pSequence->pExpr;
            for (j = 0; j < MSequence_Len((*p)->HeadSeq.pSequence); j++, pos++, p2++)
                MSequence_SetAt(n, pos, *p2);
        }
        else
        {
            MSequence_SetAt(n, pos, *p);
            pos++;
        }
    }

    return n;
}

static MSequence * Eval_FlattenSequence(MSequence *Seq)
{
    MSequence *r;
    r = Eval_FlattenSequence2(Seq);
    if (r == Seq)
    {
        XRef_IncRef(Seq);
        return Seq;
    }

    while (true) 
    {
        Seq = r;
        r = Eval_FlattenSequence2(Seq);
        if (r != Seq)
            MSequence_Release(Seq);
        else
            break;
    }

    return r;
}

static int expr_cmp(const MExpr *p1, const MExpr *p2)
{
    const MExpr expr1 = *(const MExpr *)p1;
    const MExpr expr2 = *(const MExpr *)p2;
    int r = expr1->Type - expr2->Type;
    
    if (r == 0)
    {
        switch (expr1->Type)
        {
        case etNum:
            return Num_Cmp(expr1->Num, expr2->Num);
        case etComplex:
            r = Num_Cmp(expr1->Complex.Real, expr2->Complex.Real);
            if (r == 0)
                r = Num_Cmp(expr1->Complex.Imag, expr2->Complex.Imag);
            break;
        case etString:
            return MString_Compare(expr1->Str, expr2->Str);
        case etCons:         
            r = expr_cmp(&expr1->Cons.Car, &expr2->Cons.Car);
            r = r == 0 ? expr_cmp(&expr1->Cons.Cdr, &expr2->Cons.Cdr) : r; 
            break;
        case etSymbol:
            return MString_Compare(expr1->Symbol->Name, expr2->Symbol->Name);
        case etHeadSeq:
            r = expr_cmp(&expr1->HeadSeq.Head, &expr2->HeadSeq.Head);
            if (r == 0)
            {
                int i;
                r = MSequence_Len(expr1->HeadSeq.pSequence) - MSequence_Len(expr2->HeadSeq.pSequence);
                if (0 == r)
                    for (i = 0; i < MSequence_Len(expr1->HeadSeq.pSequence); i++)
                    {
                        r = expr_cmp(expr1->HeadSeq.pSequence->pExpr + i, 
                                     expr2->HeadSeq.pSequence->pExpr + i);
                        if (r != 0)
                            break;
                    }
            }
            break;
        default:
            ASSERT("no default" == NULL);
        }
    }

    return r;
}

MInt Eval_ExprCmp(const MExpr expr1, const MExpr expr2)
{
    return expr_cmp(&expr1, &expr2);
}

static void OnWatchedObj(void *Obj, void *)
{
    MObj_ClearAttrib(Obj, EXPR_ATT_EXECUTED);
}

#define set_executed(expr)  \
    do {ALWAYS(MWatchDog_Watch(ExecutedList, expr), ==, true); MObj_SetAttrib(expr, EXPR_ATT_EXECUTED); } while(false)

#define try_set_executed(expr)  \
    do {if (MWatchDog_Watch(ExecutedList, expr)) MObj_SetAttrib(expr, EXPR_ATT_EXECUTED); } while(false)

#define is_executed(expr)  \
    MObj_HasAttrib(expr, EXPR_ATT_EXECUTED)

#define push_exec_n(expr, n)    \
do                              \
{                               \
    XRef_IncRef(expr);          \
    MStack_PushOrphan(stack, (n), (expr));   \
} while(false)

#define push_exec_n_orphan(expr, n)    \
do                              \
{                               \
    MStack_PushOrphan(stack, (n), (expr));   \
} while(false)

#define push_exec_header(expr) push_exec_n_orphan(expr, 0)

#define push_inc(expr) \
do { MStack_PushOrphan(stack, -1, expr); XRef_IncRef(expr); } while (false)
#define push_orphan(expr) MStack_PushOrphan(stack, -1, expr)

//|| (MObj_RefCnt(expr) > 0)      
//|| (MObj_RefCnt(expr->HeadSeq.pSequence) > 0)

#define ensure_standalone() \
do                          \
{                           \
    if (!MObj_HasAttrib(expr->HeadSeq.pSequence, OBJ_ATT_STANDALONE)        \
         )       \
    {                                                                       \
        MSequence *newSeq = MSequence_Uniquelize(expr->HeadSeq.pSequence);  \
        MExpr t = expr;                                                     \
        MObj_SetAttrib(newSeq, OBJ_ATT_STANDALONE);                         \
        expr = MExpr_CreateHeadSeqX(expr->HeadSeq.Head, newSeq);            \
        MExpr_Release(t);                                                   \
    }	                                                                    \
} while(0 == 1)


#define handle_symbol(symExpr, r) \
do                                                   \
{                                                    \
    MExpr expr314 = (symExpr);                       \
    if (expr314->Symbol == sym_I)                    \
    {                                                \
        r = expr314->Symbol->Immediate;              \
        XRef_IncRef(r);                              \
    }                                                \
    else                                             \
    {                                                \
        attr = MSymbol_AttributeWord(expr314->Symbol);\
        if (attr & aReadProtected)                   \
        {                                            \
            r = expr314;                             \
            XRef_IncRef(r);                          \
        }                                            \
        else                                         \
        {                                            \
            if (expr314->Symbol->Immediate)          \
            {                                        \
                r = expr314->Symbol->Immediate;      \
                XRef_IncRef(r);                      \
            }                                        \
            else                                     \
            {                                        \
                r = expr314;                         \
                XRef_IncRef(r);                      \
            }                                        \
        }                                            \
    }                                                \
}while (0)

#define need_push_eval(expr) ((((expr)->Type >= etHeadSeq) && !is_executed(expr)) \
        || (((expr)->Type == etSymbol) && ((expr)->Symbol->Immediate != NULL)))

#define is_evaluate_1(expr) (((expr)->Type == etHeadSeq) && (is_header(expr, sym_Evaluate)) \
        && (MSequence_Len((expr)->HeadSeq.pSequence) == 1))

def_bif(DownValues);
/*
static void show_def(char * cname)
{
    MString name = MString_NewC(cname);
    MSymbol sym = MSymbol_GetOrNewSymbol(name);
    MSequence *s = MSequence_Create(1);
    MSequence_EleAt(s, 0) = MExpr_CreateSymbol(sym);
    MExpr r = BIF_DownValues(s, NULL);
    BIF_Print0(r, NULL);
    MExpr_Release(r);
    MSequence_Release(s);
    MString_Release(name);
}

static void trace_stack(MStack *stack)
{
    char t2[2000 + 1] = {0};

    MInt i = 1;
    MExprListNode *n = stack->Last;

    printf("Stack:\n");

    if (stack->Len > 0)
    {
        MString s2 = MExpr_ToFullForm(n->Expr);
        MStringToASCII(s2, (MByte *)t2, sizeof(t2) - 1);        
        MString_Release(s2);    
        n = n->prev;
        printf("|> ---------> %s\n", t2);
    }
    else;

    for (; i < stack->Len; i++)
    {
        MInt index = (MInt)(n->Expr->Complex.Real);
        MExpr item = (MExpr)(n->Expr->Complex.Imag);
        ASSERT(n->Expr->Type == etComplex);

        MString s2 = MExpr_ToFullForm(item);
        MStringToASCII(s2, (MByte *)t2, sizeof(t2) - 1);        
        MString_Release(s2);    
        printf("|> %2d: %s\n", index, t2);

        n = n->prev;
    }
    printf("============\n");
}

static void trace_stack_2(MStack *stack)
{
    char t2[2000 + 1] = {0};

    MInt i = 0;
    MExprListNode *n = stack->Last;

    printf("Stack:\n");

    for (; i < stack->Len; i++)
    {
        MInt index = (MInt)(n->Expr->Complex.Real);
        MExpr item = (MExpr)(n->Expr->Complex.Imag);
        ASSERT(n->Expr->Type == etComplex);

        MString s2 = MExpr_ToFullForm(item);
        MStringToASCII(s2, (MByte *)t2, sizeof(t2) - 1);        
        MString_Release(s2);    
        printf("|> %2d: %s\n", index, t2);

        n = n->prev;
    }
    printf("============\n");
}
*/
static inline MExpr _Eval_InternalCallWithListable(f_InternalApply InternalApply, MSequence *seq, _MCONTEXT_)
{
    // handle Listable
    if ((MSequence_Len(seq) == 1)
        && (MSequence_EleAt(seq, 0)->Type == etHeadSeq)
        && (is_header(MSequence_EleAt(seq, 0), sym_List)))
    {
        MExpr t = MSequence_EleAt(seq, 0);
        MSequence *ps = MSequence_Create(MSequence_Len(t->HeadSeq.pSequence));
        MSequence tags;
        MSequence *para = MSequence_InitStatic(&tags, 1);
        MInt i;
        for (i = 0; i < MSequence_Len(ps); i++)
        {
            MExpr tt = MSequence_EleAt(t->HeadSeq.pSequence, i);
            MSequence_SetAt(para, 0, tt);
            if ((tt->Type == etHeadSeq)
                && (is_header(tt, sym_List)))
                MSequence_SetAtX(ps, i, _Eval_InternalCallWithListable(InternalApply, para, _Context));
            else
                MSequence_SetAtX(ps, i, InternalApply(para, _Context));
        }
        MSequence_ReleaseStatic(para);
        return MExpr_CreateHeadSeqX(sym_List, ps);
    }
    else 
        return InternalApply(seq, _Context);
}

int Eval_Evaluate(PEContext pContext)
{
#define ret(v) do {ASSERT(v); pContext->result = v; goto return_v;} while (false)

    MStack *stack = pContext->stack;
    char Buf[WATCHDOG_SIZE];
    MObj_WatchDog ExecutedList = MWatchDog_InitWatchDog(Buf, WATCHDOG_SIZE);
    MDword reduceNo = 0;
    const int Top = (int)DWStack_Top(pContext->topStack);

    ASSERT(pContext->result == NULL);

    // 1. evaluate the *top* expression, resulting in r
    // 2. replace stack top: old one is freed, and replaced by r
    while (MStack_Depth(stack) > Top)
    {
main_loop:        
        MExpr h = NULL;
        MExpr r = NULL;
        MDword attr;
        MSequence *seq;
        MInt Cursor; 
        MExpr expr = NULL;

        reduceNo++;
        if (reduceNo > pContext->MAX_REDUCE) goto yield;

        // check Aborted
        if (sym_DollarAborted->Immediate)
            ret(MExpr_CreateSymbol(sym_DollarAborted));

        // check exception

        // check time constraint

        // trace_stack(stack);

        expr = MStack_PopOrphan(stack, &Cursor);
        
        switch (expr->Type) 
        {
        case etSymbol:
            handle_symbol(expr, r);

            if ((r != expr) && need_push_eval(r))
            {
                MExpr_Release(expr);
                push_orphan(r);
                continue;
            }      
        	break;
        case etHeadSeq:
            if (need_push_eval(expr->HeadSeq.Head))
            {
                push_exec_header(expr);
                push_inc(expr->HeadSeq.Head);
                continue;
            }
            else
            {
                r = expr->HeadSeq.Head;
                Cursor = 0;
                XRef_IncRef(r);
                goto start_evaluate_param;
            }
            
            break;
        case etNum:
        case etString:
        case etComplex:
        case etCons:
            r = expr;
            XRef_IncRef(r);
            break;
        default:
            ASSERT("no default" == NULL);
        }

        // processing of stack top is done, release it
        MExpr_Release(expr);

basic_op_done:

        // r = Evaluate[top]
        if (MStack_Depth(stack) == Top)
            ret(r);

//        trace_stack_2(stack);

        expr = MStack_PopOrphan(stack, &Cursor);

start_evaluate_param:
        ASSERT(expr->Type == etHeadSeq);

        if (Cursor == 0)
        {
            h = r;

            // we have got a new head?
            if (h != expr->HeadSeq.Head)
            {
                MExpr t = expr;
                expr = MExpr_CreateHeadSeq(h, expr->HeadSeq.pSequence);
                MExpr_Release(h);
                MExpr_Release(t);
            }
            else
            {
                MExpr_Release(h);
            }

            attr = (h->Type == etSymbol) ? MSymbol_AttributeWord(h->Symbol) : 0;

            if (MSequence_Len(expr->HeadSeq.pSequence) == 0)
                goto sort_para;

            if ((attr & aHoldAllComplete) != 0)
                goto sort_para;

            ASSERT(!MObj_HasAttrib(expr->HeadSeq.pSequence, OBJ_ATT_STANDALONE));

            if ((attr & aSequenceHold) == 0)
            {
                MSequence *newSeq = Eval_FlattenSequence(expr->HeadSeq.pSequence);
                if (XRef_CNT(newSeq) == 0)
                {
                    MObj_SetAttrib(newSeq, OBJ_ATT_STANDALONE);
                    MSequence_Release(expr->HeadSeq.pSequence);
                    expr->HeadSeq.pSequence = newSeq;
                }
                else
                    MSequence_Release(newSeq);
            }
            
            // flatten
            if (attr & aFlat)
            {
                MSequence *newSeq = BIF_Flatten_Header(expr->HeadSeq.pSequence, expr->HeadSeq.Head, pContext);
                if (newSeq)
                {
                    MExpr t = expr;
                    MObj_SetAttrib(newSeq, OBJ_ATT_STANDALONE);
                    expr = MExpr_CreateHeadSeqX(expr->HeadSeq.Head, newSeq);
                    MExpr_Release(t);
                }
            }

            seq = expr->HeadSeq.pSequence;

            if ((attr & aHoldAll) == 0)
            {
                MExpr t = MSequence_EleAt(seq, 0);
                if ((attr & aHoldFirst) == 0)
                {
                    if (need_push_eval(t))
                    {
                        ensure_standalone();
                        push_exec_n_orphan(expr, 1);
                        push_inc(t);
                        continue;
                    }
                    else
                        Cursor++;
                }
                else
                {
                    if (is_evaluate_1(t))
                    {                            
                        ensure_standalone();
                        push_exec_n_orphan(expr, 1);
                        push_inc(get_seq_i(t, 0));
                        continue;
                    }
                    else
                        Cursor++;
                }

                // fast loop through arg
                //  do not goto main loop to make things faster
                while (Cursor < MSequence_Len(seq)) 
                {
                    t = MSequence_EleAt(seq, Cursor);
                    if (need_push_eval(t))
                    {
                        ensure_standalone();
                        push_exec_n_orphan(expr, Cursor + 1);
                        push_inc(t);
                        goto main_loop;
                    }
                    else
                        Cursor++;
                }
            }
            else
            {
                while (Cursor < MSequence_Len(seq)) 
                {
                    MExpr t = MSequence_EleAt(seq, Cursor);
                    if (is_evaluate_1(t))
                    {                            
                        MExpr tt = get_seq_i(t, 0); 
                        if (need_push_eval(tt))
                        {
                            ensure_standalone();
                            push_exec_n_orphan(expr, Cursor + 1);
                            push_inc(tt);
                            break;
                        }
                        else
                        {
                            ensure_standalone();
                            MSequence_SetAt(seq, Cursor, tt);
                        }
                    }
                    else;
                    Cursor++;
                }

                if (Cursor < MSequence_Len(seq))
                    continue;
            }
        }
        else if (Cursor <= MSequence_Len(expr->HeadSeq.pSequence))
        {
            ASSERT_REF_CNT(expr->HeadSeq.pSequence, 0);
            ASSERT(MObj_HasAttrib(expr->HeadSeq.pSequence, OBJ_ATT_STANDALONE));
            ASSERT(MObj_IsUnique(expr));
            MSequence_SetAtX(expr->HeadSeq.pSequence, Cursor - 1, r);
        }
        else
        {
            int t;
            attr = expr->HeadSeq.Head->Type == etSymbol ? MSymbol_AttributeWord(expr->HeadSeq.Head->Symbol) : 0;
            h = expr->HeadSeq.Head;
            seq = expr->HeadSeq.pSequence;
            t = Cursor - MSequence_Len(seq);
            MObj_ClearAttrib(seq, OBJ_ATT_STANDALONE);  // clear OBJ_ATT_STANDALONE
            
            switch (t)
            {
            case EVAL_BIF_CC_EVAL:
                pContext->cctag = DWStack_Pop(pContext->tagStack);
                pContext->ccexpr = r;
                break;
            default:
                ASSERT("unkown CC ret type" == NULL);                
            }
            goto call_internal;
        }

        {
            attr = expr->HeadSeq.Head->Type == etSymbol ? MSymbol_AttributeWord(expr->HeadSeq.Head->Symbol) : 0;
            seq = expr->HeadSeq.pSequence; 
            if (Cursor == MSequence_Len(seq))
                goto sort_para;

            if (((attr & aHoldAll) == 0) && ((attr & aHoldRest) == 0))
            {
                do 
                {
                    MExpr t = MSequence_EleAt(seq, Cursor);
                    if (need_push_eval(t))
                    {
                        ensure_standalone();
                        push_exec_n_orphan(expr, Cursor + 1);
                        push_inc(t);
                        break;
                    }
                    Cursor++;
                } 
                while (Cursor < MSequence_Len(seq));
                
                if (Cursor < MSequence_Len(seq))
                    continue;
            }
            else
            {
                do 
                {
                    MExpr t = MSequence_EleAt(seq, Cursor);
                    if (is_evaluate_1(t))
                    {               
                        MExpr tt = get_seq_i(t, 0); 
                        if (need_push_eval(tt))
                        {
                            ensure_standalone();
                            push_exec_n_orphan(expr, Cursor + 1);
                            push_inc(tt);
                            break;
                        }
                        else
                        {
                            ensure_standalone();
                            MSequence_SetAt(seq, Cursor, tt);
                        }
                    }
                    else;
                    Cursor++;
                } 
                while (Cursor < MSequence_Len(seq));
                
                if (Cursor < MSequence_Len(seq))
                    continue;
            }
        }
        
sort_para:        

        h = expr->HeadSeq.Head;
        seq = expr->HeadSeq.pSequence;
        MObj_ClearAttrib(seq, OBJ_ATT_STANDALONE);  // clear OBJ_ATT_STANDALONE
        pContext->cctag = 0;

        // special cases for Function, D, etc.
        if (h->Type == etHeadSeq)
        {
            r = BIF_CallFun(expr, pContext);
            if (r == NULL)
            {
                r = expr;
                set_executed(r);
                goto basic_op_done;
            }
            else
                goto no_exec;
        }
        else if (h->Type != etSymbol)
        {
            r = expr;
            if (!MObj_HasAttrib(r, EXPR_ATT_EXECUTED))
                set_executed(r);
            goto basic_op_done;
        }
        else;

        attr = MSymbol_AttributeWord(h->Symbol);

        // handle Sequence again
        if ((attr & aSequenceHold) == 0)
        {
            MSequence *newSeq = Eval_FlattenSequence(expr->HeadSeq.pSequence);
            if (XRef_CNT(newSeq) == 0)
            {
                // replace seq with newSeq
                MSequence_Release(expr->HeadSeq.pSequence);
                expr->HeadSeq.pSequence = newSeq;
                seq = newSeq;
            }
            else
                MSequence_Release(newSeq);
        }

        // sort Seq
        //printf("sorting...");
        if ((h->Symbol->InternalApply == NULL) && (attr & aOrderless) && (MSequence_Len(seq) > 1))
            qsort(seq->pExpr, MSequence_Len(seq), sizeof(MExpr), 
                    (int (*)(const void *,const void *))expr_cmp);
        //printf("ok\n");

        if ((attr & aOneIdentity) && (MSequence_Len(seq) == 1))
        {
            r = MSequence_EleAt(seq, 0);
            XRef_IncRef(r);
        }
        else
        {
            // upvalues

            // downvalues
call_internal:
            if (h->Symbol->InternalApply)
            {
                MBool bEvaluted;
                r = attr & aListable ?
                    _Eval_InternalCallWithListable(h->Symbol->InternalApply, seq, pContext)
                    : h->Symbol->InternalApply(seq, pContext);
                ASSERT(r);

                if (r <= (MExpr)(EVAL_BIF_CC_MAX))
                {
                    push_exec_n_orphan(expr, MSequence_Len(seq) + ((int)(r)));
                    push_orphan(pContext->ccexpr);
                    // r = pContext->ccexpr;
                    // XRef_IncRef(expr); // for the release at "no_exec"
                    continue;
                }
                else if (r->Type == etSymbol)
                {
                    MExpr t = r;
                    handle_symbol(t, r);
                    MExpr_Release(t);
                    bEvaluted = true;
                }
                else
                {
                        bEvaluted = (r->Type != etHeadSeq) 
                        || (is_header(r, h->Symbol) 
                            && (MSequence_Len(seq) == MSequence_Len(r->HeadSeq.pSequence))
                            && IsSeqSameQ(r->HeadSeq.pSequence, seq, 0, MSequence_Len(seq)));                
                }

                if (bEvaluted)
                {
                    if (!MObj_HasAttrib(r, EXPR_ATT_EXECUTED))
                        set_executed(r);
                    MExpr_Release(expr);
                    goto basic_op_done;
                }
            }
            else
            {
                // TODO: handle Listable

                r = BIF_CallFun(expr, pContext);
                
                if (r == NULL)
                {
                    r = expr;
                    if (!MObj_HasAttrib(r, EXPR_ATT_EXECUTED))
                        set_executed(r);
                    goto basic_op_done;
                }
                else;
            }
        }    

no_exec:
        MExpr_Release(expr);

        if ((MStack_Depth(stack) > Top) || need_push_eval(r))
            push_orphan(r);
        else
            goto basic_op_done;
    }

    ASSERT("Should not get out of while loop" == NULL);
    return -1;

return_v:

    // unset evaluted bit
    MWatchDog_TraverseAndRelease(ExecutedList, OnWatchedObj, NULL);

    ASSERT(pContext->result);
    DWStack_Pop(pContext->topStack);
    pContext->reduceNo += reduceNo;

    return 0;

yield:
    // unset evaluted bit
    MWatchDog_TraverseAndRelease(ExecutedList, OnWatchedObj, NULL);
    ASSERT(pContext->result == NULL);
    pContext->reduceNo += reduceNo;
    return 1;
}

static int GetIntDef(MSymbol s, int def)
{
    if (s->Immediate)
    {
        if (s->Immediate->Type == etNum)
            return Num_ToInt(s->Immediate->Num);
        else
            return def;
    }
    else
        return def;
}

static PEContext init_context(MExpr expr)
{
    PEContext Context = (PEContext)YM_malloc(sizeof(MEContext));
    Context->stack = MStack_Create(128);
    MStack_PushOrphan(Context->stack, -1, expr);
    XRef_IncRef(expr);
    Context->topStack = DWStack_CreateDwordStack();
    Context->MAX_RECURSION = GetIntDef(sym_DollarRecursionLimit, 256);
    Context->MAX_ITERATION = GetIntDef(sym_DollarIterationLimit, 256);
    Context->MAX_REDUCE = 1000;
    return Context;
}

MExpr Eval_Evaluate(MExpr expr)
{
    MExpr r;
    PEContext Context = init_context(expr);
    Context->MAX_REDUCE = (uint)(-1);
    DWStack_Push(Context->topStack, 0);

    ALWAYS(Eval_Evaluate(Context), ==, 0);
    
    r = Context->result;
    Context->result = NULL;
    Eval_Cleanup(Context);

    return r;
}

MExpr Eval_SubEvaluate(MExpr expr context_param)
{
    PEContext Context = (PEContext)(_Context);
    MExpr r;
    MStack *stack = Context->stack;

    DWStack_Push(Context->topStack, MStack_Depth(Context->stack));
    push_inc(expr);

    ALWAYS(Eval_Evaluate(Context), ==, 0);
    
    r = Context->result;
    Context->result = NULL;

    return r;
}

MEvalContext Eval_Spawn(MExpr expr)
{
    return NULL;
}

void Eval_Run(_MCONTEXT_)
{
}

int Eval_Cleanup(_MCONTEXT_)
{
    PEContext Context = (PEContext)(_Context);
    
    ASSERT(MStack_Depth(Context->stack) == 0);
    MStack_Release(Context->stack);
    MStack_Release(Context->topStack);
    if (Context->result) MExpr_Release(Context->result);
    if (Context->throws) MExprList_Release(Context->throws);
    if (Context->tagStack) DWStack_Release(Context->tagStack);

    return 0;
}

MExpr Eval_GetStack(_MCONTEXT_)
{
    MExprList *rr = MExprList_Create();
    PEContext Context = (PEContext)(_Context);
    MStack *sm = MStack_Mirror(Context->stack);
    MStack *tm = MStack_Mirror(Context->topStack);
    MSequence *ps = NULL;

    while (MStack_Depth(tm) >= 1)
    {
        int t = (int)(MStack_PopOrphan2(tm)) + 1;
        while (MStack_Depth(sm) >= t)
        {
            int tag;
            MExpr c = MStack_PopOrphan(sm, &tag);
#ifndef _DEBUG
            MSequence *ps = MSequence_Create(2);
            MSequence *ps2 = MSequence_Create(2);

            MSequence_SetAt (ps, 0, c->HeadSeq.Head);
            MSequence_SetAtX(ps, 1, MExpr_CreateMachineInt(MSequence_Len(c->HeadSeq.pSequence)));
            MSequence_SetAtX(ps2, 1, MExpr_CreateMachineInt(tag));
            MSequence_SetAtX(ps2, 0, MExpr_CreateHeadSeqX(sym_List, ps));
            MExprList_PushOrphan(rr, MExpr_CreateHeadSeqX(sym_List, ps2));
#else
            MExprList_Push(rr, c->HeadSeq.Head);
#endif
            
        }
    }

    ps = MSequence_FromExprList(rr);
    MExprList_Release(rr);
    MStack_ReleaseMirror(tm, NULL);
    MStack_ReleaseMirror(sm, NULL);
    return MExpr_CreateHeadSeqX(sym_List, ps);
}

MExpr Eval_RetCCEval(MExpr expr, MDword tag, _MCONTEXT_)
{
    PEContext Context = (PEContext)(_Context);
    if (NULL == Context->tagStack)
        Context->tagStack = DWStack_CreateDwordStack();
    DWStack_Push(Context->tagStack, tag);
    Context->ccexpr = expr;
    ASSERT(tag != 0);
    return (MExpr)(EVAL_BIF_CC_EVAL);
}

MDword Eval_GetCallTag(_MCONTEXT_)
{
    PEContext Context = (PEContext)(_Context);
    return Context->cctag;
}

MExpr Eval_GetCallExpr(_MCONTEXT_)
{
    PEContext Context = (PEContext)(_Context);
    return Context->ccexpr;
}

#define POS_CACHE_SIZE  (100)

struct MExprIter_impl
{
    bool bStatic;
    f_OnExprTranverse f;
    void *para;
    
    MStack *stack;
    
    // position handling, dirty, and light-weight
    int *pos;
    int pos_cap;
    int pos_len;
    int pos_buf[POS_CACHE_SIZE];

    int pos_len_max;    // max pos_len on a getting-deepper path. 
                        //      if we have got to the deepest, (pos_len - pos_len_max - 1) is the negtive level index
};

#define push_pos(n)                                                                                       \
    do {                                                                                                  \
        if (impl->pos_len >= impl->pos_cap)                                                               \
        {                                                                                                 \
            if (impl->pos_cap > POS_CACHE_SIZE)                                                           \
                impl->pos = (int *)YM_realloc(impl->pos, (impl->pos_cap + POS_CACHE_SIZE) * sizeof(*impl->pos)); \
            else                                                                                          \
            {                                                                                             \
                impl->pos = (int *)YM_malloc(POS_CACHE_SIZE * 2 * sizeof(*impl->pos));                           \
                memcpy(impl->pos, impl->pos_buf, sizeof(impl->pos_buf));                                  \
            }                                                                                             \
            impl->pos_cap += POS_CACHE_SIZE;                                                              \
        }                                                                                                 \
        impl->pos[impl->pos_len] = n;                                                                     \
        impl->pos_len++;                                                                                  \
    } while (false)

#define push_ele(expr, n)    \
do                              \
{                               \
    push_pos(n);                \
    MStack_PushOrphan(impl->stack, (n),  (expr)); \
} while(false)

#define push_ele_head(expr) push_ele(expr, 0)

#define push_ele_cdr(expr) push_ele(expr, 2)
#define push_ele_car(expr) push_ele(expr, 1)

#define push_expr(expr) push_ele(expr, -1)

#define iter_pop_level()    \
    do {    \
        MStack_PopOrphan2(impl->stack);  \
        impl->pos_len--;    \
    } while (false)

int MExprIter_ReverseLv(MExprIter iter)
{
    MExprIter_impl *impl = (MExprIter_impl *)(iter);
    return impl->pos_len - impl->pos_len_max - 1;
}

static void MExprIter_init(MExprIter_impl *impl, MExpr expr, f_OnExprTranverse f, void *para)
{
    impl->f = f;
    impl->para = para;

    impl->pos = impl->pos_buf;
    impl->pos_cap = POS_CACHE_SIZE;

    impl->stack = MStack_Create(POS_CACHE_SIZE);
    push_expr(expr);
}

MExprIter MExprIter_CreateStatic(MByte *buf, const int Size, MExpr expr, f_OnExprTranverse f, void *para)
{
    MExprIter_impl *impl = (MExprIter_impl *)(buf);
    ASSERT(Size >= (int)sizeof(MExprIter_impl));
    Zero(buf, Size);
    impl->bStatic = true;
    MExprIter_init(impl, expr, f, para);

    return (MExprIter)(impl); 
}

MExprIter MExprIter_Create(MExpr expr, f_OnExprTranverse f, void *para)
{
    MExprIter_impl *impl = (MExprIter_impl *)YM_malloc(sizeof(MExprIter_impl));
    MExprIter_init(impl, expr, f, para);

    return (MExprIter)(impl); 
}

void      MExprIter_Release(MExprIter iter)
{
    MExprIter_impl *impl = (MExprIter_impl *)(iter);

    impl->f(NULL, NULL, eitEnd, impl->para, iter);
    MStack_Release(impl->stack);
    
    if (impl->pos != impl->pos_buf)
        YM_free(impl->pos);

    if (!impl->bStatic) YM_free(impl);
}

#define get_top_index() ((int)(MExprList_Top(impl)->Complex.Real))

void      MExprIter_Go(MExprIter iter)
{
#define inc_index()     \
        do {            \
            impl->pos_len_max = impl->pos_len;                          \
            MStack_Top0(impl->stack)->t = index + 1;  \
        }   while (false)

    MExprIter_impl *impl = (MExprIter_impl *)(iter);
    MExpr e;
    int index;
    while ((e = MStack_Top(impl->stack, &index)))
    {
        MExpr ele = e;
        MExpr parent = e;

        MExprIterNextType t;
        MExprIterType it = eitNormal;

        if (index == 0)
        {
            ASSERT(e->Type == etHeadSeq);
            ele = e->HeadSeq.Head;
            it = eitGoEleN;
        }
        else if (index > 0)
        {
            if (e->Type == etHeadSeq)
                ele = MSequence_EleAt(e->HeadSeq.pSequence, index - 1);
            else if (e->Type == etCons)
                ele = index == 1 ? e->Cons.Car : e->Cons.Cdr;
            else
                ASSERT("bad type for index > 0");
            it = (MExprIterType)(eitGoEleN + index);
        }
        else
        {
            parent = NULL;
        }

        t = impl->f(ele, parent, it, impl->para, iter);
        switch (t)
        {
        case entBreak:
            if (index >= 0)
                iter_pop_level();
            else
                goto quit;
            break;
        case entGoDeeper:
            switch (ele->Type)
            {
            case etHeadSeq:
                push_ele_head(ele);
                break;
            case etCons:
                push_ele_car(ele);
                break;
            default:
                goto check_t;
            }
            break;
        case entStop:
        case entPause:
            goto quit;
            break;
        case entContinue:
check_t:
            if (index >= 0)
            {
                int max = e->Type == etHeadSeq ? MSequence_Len(e->HeadSeq.pSequence) : 2;
                if (index < max)
                    inc_index();
                else
                {
                    t = impl->f(parent, NULL, eitInnerDone, impl->para, iter);
                    if (t == entContinue)
                    {
                        MStackEle *pt = MStack_Top0(impl->stack);
                        iter_pop_level();
                        if (pt)
                        {
                            index = pt->t;
                            ele = e = pt->e;
                            goto check_t;
                        }
                    }
                    else
                        goto quit;
                }
            }
            else
                iter_pop_level();
            break;
        default:
            ASSERT("invalid return iter type" == NULL);
        }
    }

quit:
    return;
}

struct MExprGener_impl
{
    MExprIter_impl iter;
    f_OnGenerTranverse f;
    void *para;

    MExprList *stack;

    bool bStopped;
};

static MExprIterNextType MExprGener_OnExprIter(MExpr ele, MExpr parent, MExprIterType t, void *para, MExprIter iter)
{
    MExprGener_impl *impl = (MExprGener_impl *)(para);
    MExpr newr = NULL;
    MExprIterNextType nt = entContinue;

    if (impl->bStopped) goto got_it;

    newr = impl->f(ele, parent, t, impl->para, para);
  //  nt = (MExprIterNextType)(newr);
    switch (nt)
    {
    case entStop:
        impl->bStopped = true;
        newr = NULL;
        break;
    case entContinue:
        switch (ele->Type)
        {
        case etHeadSeq:
        case etCons:
            return entGoDeeper;
        default:
            return entContinue;
        }
    default:
        ;
    }

got_it:
  //  newr = 
  return entContinue;
}

MExpr MExprGener_Generate(MExpr expr, f_OnGenerTranverse f, void *para)
{
    return NULL;
}

MExpr Eval_Throw(MExpr e, _MCONTEXT_)
{
    PEContext Context = (PEContext)(_Context);
    MSequence *s = MSequence_Create(1);
    MSequence_EleAt(s, 0) = e;
    MExprList_PushOrphan(Context->throws, MExpr_CreateHeadSeqX(sym_Throw, s));
    return MExpr_CreateSymbol(sym_Null);
}

