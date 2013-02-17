
#ifndef _matcher_h
#define _matcher_h

#include "expr.h"

bool  Match_MatchQ(MExpr expr, MExpr pattern, _MCONTEXT_);
MExpr Match_MatchFirst(MExpr expr, MExpr pattern, _MCONTEXT_);
MExpr Match_MatchAll(MExpr expr, MExpr pattern, _MCONTEXT_);

#endif
