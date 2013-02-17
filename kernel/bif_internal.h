
#ifndef _bif_internal_h
#define _bif_internal_h

#include "mtype.h"
#include "num.h"
#include "expr.h"

enum MSpanIterState
{
    isReset,
    isEnd,
    isOngoing
};

struct MSpanIter
{
    MNum start;
    MNum end;
    MNum step;
    MNum cur;

    MInt istart;
    MInt iend;
    MInt istep;
    MInt icur;

    MDword len;
    MSpanIterState state;
};

MSpanIter *CreateMachineSpanIter(MInt start, MInt end, MInt step = 1);
MSpanIter *CreateSpanIter(MNum start, MNum end, MNum step = NULL);
void       SpanIterDelete(MSpanIter *iter);

MSpanIter *SpanIterReset(MSpanIter *iter);
bool       SpanIterMachineNext(MSpanIter *iter, MInt &v);
MNum       SpanIterNext(MSpanIter *iter);

MDword SpanIterLen(MSpanIter *iter);

#define DWStack_CreateDwordStack() MStack_Create(10)
#define DWStack_Push(s, V)         MStack_PushOrphan(s, (MInt)(V), NULL)
#define DWStack_Pop(s)             MStack_PopOrphan2(s)
#define DWStack_Top(s)             MStack_TopTag(s)
#define DWStack_Release(s)         MStack_Release(s)

#define RET_Eval_RetCCEvalX(expr, tag)                          \
    do { MExpr expr314 = (expr); MDword tag314 = (MDword)(tag); \
        XRef_IncRef(expr314);                                   \
        return Eval_RetCCEval(expr314, tag314, _Context);       \
    } while (false)

#endif
