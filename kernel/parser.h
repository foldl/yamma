
#ifndef parser_h
#define parser_h

#include "expr.h"
#include "token.h"

#define IsInequalityOp(sym)                     \
          ((sym == sym_SameQ)                   \
        || (sym == sym_UnsameQ)                 \
        || (sym == sym_Equal)                   \
        || (sym == sym_Unequal)                 \
        || (sym == sym_Less)                    \
        || (sym == sym_LessEqual)               \
        || (sym == sym_Greater)                 \
        || (sym == sym_GreaterEqual)            \
          )

MExpr Parse_FromTokener(MTokener *tokener);
MExpr Parse_FromTokenerLisp(MTokener *tokener);
MInt  ParseMachineInt(const MString s);

#endif
