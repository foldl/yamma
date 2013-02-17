
#ifndef eval_h
#define eval_h

#include "mtype.h"
#include "mstr.h"
#include "expr.h"

int Eval_Init();
MExpr Eval_Evaluate(MExpr expr);
//MExpr Eval_SubEvaluate(MExpr expr, _MCONTEXT_);
MInt Eval_ExprCmp(const MExpr expr1, const MExpr expr2);

MEvalContext Eval_Spawn(MExpr expr);
void Eval_Run(_MCONTEXT_);
int Eval_Cleanup(_MCONTEXT_);

#define Eval_eval_num_opt(expr, r)                                                             \
do {                                                                                           \
    ASSERT(r == NULL);                                                                         \
    MExpr t_314 = (expr);                                                                      \
    if ((t_314->HeadSeq.Head->Type == etSymbol)                                                \
        && (t_314->HeadSeq.Head->Symbol->InternalApply))                                       \
    {                                                                                          \
        int i;                                                                                 \
        for (i = 0; i < MSequence_Len(t_314->HeadSeq.pSequence); i++)                          \
        {                                                                                      \
            if (MSequence_EleAt(t_314->HeadSeq.pSequence, i)->Type != etNum)                   \
                break;                                                                         \
        }                                                                                      \
                                                                                               \
        if (i == MSequence_Len(t_314->HeadSeq.pSequence))                                      \
            (r) = t_314->HeadSeq.Head->Symbol->InternalApply(t_314->HeadSeq.pSequence, NULL);  \
        else;                                                                                  \
    }                                                                                          \
    else;                                                                                      \
} while (0)

// contracts on RetCC feature
//      1. expr passed to Eval_RetCCEval: a expr passed into Evaluater, and will *be* FREED!
//              i.e. it is treated like an expr returned by BIFs, will be evaluted, and got something new.
//      2. expr got from Eval_GetCallExpr: is a *new* instance from Evaluate(), 
//              which must be released in the calling BIF
MExpr Eval_RetCCEval(MExpr expr, MDword tag, _MCONTEXT_);
MExpr Eval_GetCallExpr(_MCONTEXT_);
MDword Eval_GetCallTag(_MCONTEXT_);

enum MExprIterNextType
{
    entBreak,       // stop on this seq, and move one level up      
    entGoDeeper,
    entStop,        // iter stoped, and iter obj is ready to be released
    entPause,       // paused, and can be resumed later
    entContinue     // continue to iterate
};

enum MExprIterType
{
    // state of iteration
    eitEnd,         // when iter obj is released

    eitNormal,      // 

    eitInnerDone,   // call callback fun with whole HeadSeq/Cons expr when inner level is done
                    //  requested by entGoDeeper, the returned MExprIterNextType is ignored
    eitGoEleN   // head == 0, seq == 1, 2..
                // car == 1, cdr == 2
};
typedef void * MExprIter;
typedef MExprIterNextType (* f_OnExprTranverse)(MExpr ele, MExpr parent, MExprIterType t, void *para, MExprIter iter);
#define EXPR_ITER_SIZE   (500)
MExprIter MExprIter_CreateStatic(MByte *buf, const int Size, MExpr expr, f_OnExprTranverse f, void *para);
MExprIter MExprIter_Create(MExpr expr, f_OnExprTranverse f, void *para);
void      MExprIter_Release(MExprIter iter);
void      MExprIter_Go(MExprIter iter);

typedef void * MExprGener;
typedef MExpr (* f_OnGenerTranverse)(MExpr ele, MExpr parent, MExprIterType t, void *para, MExprGener gener);
MExpr MExprGener_Generate(MExpr expr, f_OnGenerTranverse f, void *para);

#endif
