
#ifndef bif_h
#define bif_h

#include "expr.h"

#define context_param   , _MCONTEXT_
#define def_bif(name) \
MExpr BIF_##name(MSequence *seq context_param)

MExpr BIF_CallFun(MExpr expr context_param);
bool  IsExprSameQ(MExpr expr1, MExpr expr2);
bool  IsSeqSameQ(MSequence *s1, MSequence *s2, const int from, const int to);
bool  BIF_SameQ(MExpr expr1, MExpr expr2 context_param);

void  BIF_Message(MSymbol sym, MExpr expr, MSequence *seq context_param);
MExpr BIF_GetHead(MExpr para);
MExpr BIF_GetFile(const MString fn, const MString key context_param);

MSequence *BIF_Flatten_Header(MSequence *seq, MExpr header context_param);
bool IsExprNegInfinity(const MExpr t);
MInt ToIntWithClipInfinity(MExpr expr);

def_bif(InputString);

// a minimal lisp: car, cdr, cons, atom and null.
def_bif(Car);
def_bif(Cdr);
def_bif(Cons);
def_bif(ConsQ); // !atom

// pattern related
def_bif(MatchQ);

def_bif(Plus);
def_bif(Times);
def_bif(Power);
def_bif(Complex);
def_bif(Rational);

// bool
def_bif(Or);
def_bif(Nor);
def_bif(And);
def_bif(Nand);
def_bif(Not);
def_bif(Or);
def_bif(Xor);
def_bif(Xnor);

def_bif(ToExpression);
def_bif(ToString);
def_bif(StringForm);
def_bif(ToCharacterCode);
def_bif(FromCharacterCode);
def_bif(Characters);

def_bif(Attributes);
def_bif(SetAttributes);
def_bif(ClearAttributes);
def_bif(Protect);
def_bif(Unprotect);

def_bif(Message);
def_bif(Print);

def_bif(CompoundExpression);

def_bif(Inequality);
def_bif(SameQ);
def_bif(UnsameQ);
def_bif(Equal);
def_bif(Unequal);
def_bif(Less);
def_bif(LessEqual);
def_bif(Greater);
def_bif(GreaterEqual);
def_bif(Order);

def_bif(Set);
def_bif(SetDelayed);
def_bif(Unset);

def_bif(Context);
def_bif(Head);
def_bif(First);
def_bif(Last);
def_bif(Rest);
def_bif(Most);
def_bif(Join);
def_bif(Append);
def_bif(Prepend);

def_bif(Length);
def_bif(AtomQ);
def_bif(OddQ);
def_bif(EvenQ);

def_bif(If);
def_bif(Apply);
def_bif(Which);

def_bif(Evaluate);
def_bif(Get);

def_bif(Factorial);
def_bif(Factorial2);
def_bif(Quotient);
def_bif(Mod);
def_bif(Ceiling);
def_bif(Floor);
def_bif(Round);

def_bif(FoldList);
def_bif(Fold);

def_bif(StringJoin);
def_bif(Map);
def_bif(Range);
def_bif(Nest);
def_bif(NestList);

// helpers
bool check_arg_types(const char *types, MSequence *seq, const MSymbol sym, const MBool bReport context_param);
MExpr verify_arity_fun(MSequence *seq, MSymbol s, const MInt arity, _MCONTEXT_);
#define verify_arity(seq, s, arity) verify_arity_fun(seq, s, arity, _Context)
#define verify_arity_with_r(s, arity) \
    MExpr r = verify_arity(seq, sym_##s, arity);\
    if (r) return r;

#define is_header_expr(expr, header) (((expr)->Type == etHeadSeq) && ((expr)->HeadSeq.Head->Type == etSymbol) && ((expr)->HeadSeq.Head->Symbol == (header)))
#define is_throw_expr(expr) is_header_expr((expr), sym_Throw)

#endif
