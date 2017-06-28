
#ifndef expr_h
#define expr_h

//#include <string.h>

#include "mtype.h"
#include "mstr.h"
#include "mm.h"
#include "hash.h"
#include "assert.h"
#include "num.h"

#define SEQ_CACHE_SIZE 3

typedef MDword MAttribute;

#define aConstant          (0x00000001 << 0)  // all derivatives of f are zero 
#define aFlat              (0x00000001 << 1)  // f is associative 
#define aHoldAll           (0x00000001 << 2)  // all the arguments of f are not evaluated
#define aHoldAllComplete   (0x00000001 << 3)  // the arguments of f are completely shielded from evaluation 
#define aHoldFirst         (0x00000001 << 4)  // the first argument of f is not evaluated 
#define aHoldRest          (0x00000001 << 5)  // all but the first argument of f are not evaluated 
#define aListable          (0x00000001 << 6)  // f is automatically "threaded" over lists 
#define aLocked            (0x00000001 << 7)  // attributes of f cannot be changed 
#define aNHoldAll          (0x00000001 << 8)  // the arguments of f are not affected by N 
#define aNHoldFirst        (0x00000001 << 9)  // the first argument of f is not affected by N 
#define aNHoldRest         (0x00000001 <<10)  // all but the first argument of f are not affected by N 
#define aNumericFunction   (0x00000001 <<11)  // the value of f is assumed to be a number when its arguments are numbers 
#define aOneIdentity       (0x00000001 <<12)  // f[a] f[f[a]] etc. are equivalent to a in pattern matching 
#define aOrderless         (0x00000001 <<13)  // f is commutative 
#define aProtected         (0x00000001 <<14)  // values of f cannot be changed 
#define aReadProtected     (0x00000001 <<15)  // values of f cannot be read 
#define aSequenceHold      (0x00000001 <<16)  // Sequence objects in the arguments of f are not flattened out 
#define aStub              (0x00000001 <<17)  // Needs is automatically called if the symbol is ever input 
#define aTemporary         (0x00000001 <<18)  // f is a local variable removed when no longer used 

// some expr attribute
#define EXPR_ATT_EXECUTED (1 << (OBJ_ATT_USER_START))

enum MExprType
{
    etNum,
    etComplex,
    etString,
    etSymbol,
    etBinary,
    etCons,
    etFun,
    etHeadSeq,
    etUnknown
};

struct MTagSymbol;
typedef struct MTagSymbol * MSymbol;
struct MTagExpr;
typedef MTagExpr * MExpr;

struct MComplex
{
    MNum Real;
    MNum Imag;
};

struct MSequence;

struct MExprHeadSeq
{
    MExpr   Head;
    MSequence *pSequence;
};

struct MCons
{
    MExpr Car;
    MExpr Cdr;
};

struct MBinary
{
    MInt Size;      // if Size >= 0, pBin should be freed when expr is freed, 
                    //      when Size < 0, pBin points to some point within another bin
    MByte *pBin;
};

struct MTagExpr
{
    MHash Hash;
    enum MExprType Type;
    union
    {
        MNum    Num;
        MString Str;
        MComplex  Complex;
        MSymbol  Symbol;
        MCons    Cons;
        MBinary  Bin;
        MExprHeadSeq HeadSeq;
    };
};

struct MExprListNode
{
    MExprListNode *next;
    MExprListNode *prev;
    MExpr Expr;
};

struct MExprList
{
    MInt Len;
    MExprList     *Mirror;
    MExprListNode  Head;
    MExprListNode *Last;
};

//typedef MExprList MStack;

#define MExprWatch MExprList

typedef void *MEvalContext;

typedef MExpr (* f_InternalApply)(MSequence *param, MEvalContext EvalContext);
//typedef MExpr (* f_InternalApply)(MSequence *param);

// prefixed with "_" to notify that it's *special*
#define _MCONTEXT_ MEvalContext _Context
#define _CONTEXT                _Context

struct MTagSymbol
{
    MPtr Context;
    MString Name;
    
    MAttribute Attrib;

    MExpr SymExpr;      // cache an expression of this symbol

    MExpr Immediate;
    bool  bDelayedImmediate;
    
    // DownValues
    MSequence *PatternDownValues;
    MHashTable *ExactDownValues;

    // UpValues
    MSequence  *PatternUpValues;
    MHashTable *ExactUpValues;

    MExprList *DefaultValues;
    MExprList *Messages;

    f_InternalApply InternalApply;
};

struct MFun
{
    MExpr def;
    MAttribute Attrib;
    MExprList *Pos;

    MByte Buf[OBJ_POOL_SIZE];
    MObj_ObjPool Pool;
};

struct MSequence
{
#ifdef use_shadow    
    MSequence *Shadow;
#endif
    MSequence *Holder;
    MInt Len;
    MInt Capacity;

    MExpr *pExpr;
    MExpr  ExprCache[SEQ_CACHE_SIZE];
    
    // packed array
    //MInt  *pPackedInt;
    //MReal *pPackedReal;
};

typedef void (* f_OnUnmirror)(MExprList *l, MExpr expr);

#define MExprList_Append MExprList_Push
MExprList *MExprList_Create(void);
MExprList *MExprList_Mirror(MExprList *Mirror);
MExprList *MExprList_Push(MExprList *l, MExpr expr);
MExprList *MExprList_PushOrphan(MExprList *l, MExpr expr);
MExpr      MExprList_Pop(MExprList *l);
MExpr      MExprList_PopOrphan(MExprList *l);
MExpr      MExprList_Top(MExprList *l);
void       MExprList_Clear(MExprList *l);
void       MExprList_ReleaseMirror(MExprList *l, f_OnUnmirror OnUnmirror);
#define    MExprList_Release(l) \
do {                            \
   ASSERT((l)->Mirror == NULL); \
   XRef_DecRef(l);              \
} while (false)

#define MExprList_Len(l) ((const MInt)(l)->Len)
#ifdef _DEBUG
#define MExprListIter MExprListNode *
#else
typedef void *MExprListIter;
#endif
#define MExprList_GetIter(l)     ((MExprListIter)(l)->Last)
#define MExprListIter_Get(iter)  ((iter) ? ((MExprListNode *)(iter))->Expr : NULL)
#define MExprListIter_SetX(iter, expr)  \
do {                                    \
    MExpr_Release(((MExprListNode *)(iter))->Expr); \
    ((MExprListNode *)(iter))->Expr = (expr);       \
} while (false)

#define MExprListIter_Prev(iter) ((iter) ? (MExprListIter)(((MExprListNode *)(iter))->prev) : NULL)

struct MStackEle
{
    MInt  t;
    MExpr e;
};

struct MStack
{
    int cap;
    int top;
    int mt;
    MStack *m;

    MStackEle *p;
};
typedef void (* f_OnStackUnmirror)(MStack *l, MInt tag, MExpr expr);

MStack *MStack_Create(int Capacity);
MStack *MStack_PushOrphan(MStack *Stack, MInt Tag, MExpr Expr) __attribute__ ((fastcall));
MExpr   MStack_PopOrphan(MStack *Stack, MInt *Tag);
MInt    MStack_PopOrphan2(MStack *Stack);
MStackEle *MStack_Top0(MStack *Stack);
MInt    MStack_TopTag(MStack *Stack);
MExpr   MStack_TopExpr(MStack *Stack);
MExpr   MStack_Top(MStack *Stack, MInt *Tag);
void    MStack_Release(MStack *Stack);
MStack *MStack_Mirror(MStack *Stack);
void    MStack_ReleaseMirror(MStack *Stack, f_OnStackUnmirror OnUnmirror); // OnUnmirror /@ (new items)
#define MStack_Depth(Stack) ((const MInt)(Stack)->top + 1)

MExprWatch *MExprWatch_Create();
MExprWatch *MExprWatch_Watch(MExprWatch *Watch, MExpr expr);
MExpr       MExprWatch_ReleaseWatch(MExprWatch *Watch, bool &bWatched);
#define    MExprWatch_Release(l) XRef_DecRef(l)

MSequence *MSequence_FromExprList(MExprList *l, bool bRev = false);
MSequence *MSequence_Create(const MInt len);
MSequence *MSequence_Uniquelize(MSequence *s);
MSequence *MSequence_SetAt(MSequence *s, const MInt i, MExpr expr);
MSequence *MSequence_SetAtX(MSequence *s, const MInt i, MExpr expr);
MSequence *MSequence_Copy(MSequence *s, const int Start, const int Len);
MSequence *MSequence_UniqueCopy(MSequence *s, const int Start, const int Len);
MSequence *MSequence_SetFrom(MSequence *s, MExprList *l, const int Start, const int Len);
MSequence *MSequence_OverwriteSetFrom(MSequence *destination, const MSequence *source, 
                                      const int DestStart, const int SourceStart, const int Len);
MExpr      MSequence_GetAt(MSequence *s, const MInt i);
#define    MSequence_Len(s) ((s)->Len)
#define    MSequence_Release XRef_DecRef
#define    MSequence_EleAt(s, i) ((s)->pExpr[i])
#define              EleAt(s, i) ((s)->pExpr[i])
MSequence *MSequence_InitStatic(MSequence *s, const MInt len);
void       MSequence_ReleaseStatic(MSequence *s);
void       MSequence_Shorten(MSequence *s, const int new_len);

MExpr MExpr_CreateMachineInt(const MInt v);
MExpr MExpr_CreateMachineUInt(const MUInt v);
MExpr MExpr_CreateMachineReal(const MReal v);
MExpr MExpr_CreateNum(const MNum n);
MExpr MExpr_CreateNumX(const MNum n);
MExpr MExpr_CreateString(const MString v);
MExpr MExpr_CreateStringX(const MString v);
MExpr MExpr_CreateBool(const MBool v);
MExpr MExpr_CreateRational(const MNum Numerator, const MNum Denominator);
MExpr MExpr_CreateRationalX(const MNum Numerator, const MNum Denominator);
MExpr MExpr_CreateComplex(const MNum real, const MNum image);
MExpr MExpr_CreateComplexX(const MNum real, const MNum image);
MExpr MExpr_CreateComplexUnit();
MExpr MExpr_CreateSymbol(const MSymbol s);
MExpr MExpr_CreateCons(const MExpr x, const MExpr y);
MExpr MExpr_CreateConsNX(const MExpr x, const MExpr y);
MExpr MExpr_CreateConsXX(const MExpr x, const MExpr y);
MExpr MExpr_CreateHeadSeq(const MExpr h, MSequence *s);
MExpr MExpr_CreateHeadSeq(const MSymbol h, MSequence *s);
MExpr MExpr_CreateHeadSeqX(const MExpr h, MSequence *s);
MExpr MExpr_CreateHeadSeqX(const MSymbol h, MSequence *s);
MExpr MExpr_CreateHeadXSeqX(const MExpr h, MSequence *s);
MExpr MExpr_CreateHeadSeqFromExprList(const MExpr h, MExprList *s, bool bRev = false);
MExpr MExpr_CreateHeadSeqFromExprList(const MSymbol h, MExprList *s, bool bRev = false);
MExpr MExpr_CreateFun(const MSequence funexpr);
MExpr MExpr_CreateEmptySeq();

#define MExpr_Release XRef_DecRef
MHash MExpr_Hash(MExpr s);

#define MExpr_InvalidateHash(expr) ((expr)->Hash = 0)
MExpr MExpr_SetTo(MExpr dest, MExpr s);

MExpr MExpr_ReplaceCar(const MExpr r, const MExpr car);
MExpr MExpr_ReplaceCdr(const MExpr r, const MExpr cdr);

MString MExpr_ToFullForm(MExpr expr);

void MExpr_Init(void);
void MExpr_DumpHeap(FILE *file);
void MExprList_DumpType(FILE *f);

#define is_str(p)  ( (p)->Type == etString)
#define is_int(p)  (((p)->Type == etNum)  && Num_IsInt((p)->Num))
#define is_real(p) (((p)->Type == etNum)  && Num_IsReal((p)->Num))
#define is_int_real(expr) ((expr)->Type == etNum)

#define is_num_int(p)  (Num_IsInt(p))

#define is_sym_header_sym(expr, header) (((expr)->HeadSeq.Head->Symbol == header))
#define is_header(expr, header) (((expr)->HeadSeq.Head->Type == etSymbol) && ((expr)->HeadSeq.Head->Symbol == header))
#define is_inZ(expr) ((expr)->Type <= etComplex)

#define is_num_exact_n(num, n) Num_SameQSmall((num), (n))
#define is_num_exact_zero(num) Num_IsZero(num)

#define is_exact_n(expr, n) (((expr)->Type == etNum) && Num_SameQSmall((expr)->Num, n))
#define is_exact_zero(expr) is_exact_n(expr, 0)

#define is_seq_all2(seq, f) (f((seq)->pExpr[0]) && f((seq)->pExpr[1]))
#define is_seq_all3(seq, f) (f((seq)->pExpr[0]) && f((seq)->pExpr[1]) && f((seq)->pExpr[2]))

#define is_sym(expr, sym) (((expr)->Type == etSymbol) && ((expr)->Symbol == (sym)))
#define is_false(expr) (((expr)->Type == etSymbol) && ((expr)->Symbol == sym_False))
#define is_true(expr)  (((expr)->Type == etSymbol) && ((expr)->Symbol == sym_True))

#define get_seq(expr) ((expr)->HeadSeq.pSequence)
#define get_seq_i(expr, i) MSequence_EleAt((expr)->HeadSeq.pSequence, i)
#define headseq_len(expr) MSequence_Len((expr)->HeadSeq.pSequence)

#define to_int(expr) Num_ToInt((expr)->Num)

#endif
