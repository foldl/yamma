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

#include "parser.h"
#include "sym.h"
#include "sys_sym.h"
#include "encoding.h"
#include "sys_msg.h"
#include "bif.h"

MInt ParseMachineInt(const MString s)
{
    MInt i = 0;
    MInt r = 0;
    MBool bNeg = false;
    if (MString_Len(s) == 0)
        return r;

    if (MString_GetAt(s, 1) == '-')
    {
        bNeg = true;
        i = 2;
    }

    for (; i <= MString_Len(s); i++)
    {
        r *= 10;
        r += Char2Int(MString_GetAt(s, i));
    }

    return bNeg ? -r : r;
}

static MString EscapeString(MString s)
{
    const int l = MString_Len(s);
    int wr = 0;
    int i = 0;
    MChar *p = (MChar *)MString_GetData(s);
    while (i < l)
    {
        if ((p[i] == '\\') && (i < MString_Len(s) - 1))
        {
            i++;
            switch (p[i])
            {
            case 'n': p[wr++] = '\n'; break;
            case 't': p[wr++] = '\t'; break;
            case '\\': p[wr++] = '\\'; break;
            case '"': p[wr++] = '"'; break;
            default:
                p[wr++] = '\\'; p[wr++] = p[i];
            }            
        }
        else
            p[wr++] = p[i];
        i++;
    }

    s->Len = wr;

    return s;
}

MExpr Parse1(MTokener *tokener);

static inline void Parser_Report(MTokener *tokener)
{
    MSequence *para = MSequence_Create(4);
    Token_FillMessagePara(tokener, para, 0, 4);
    BIF_Message(sym_Syntax, msg_Generalsntx, para, NULL);
    MSequence_Release(para);
}

static MExpr MExprList_PopOrphanNull0(MTokener *tokener, MExprList *Stack, const MInt OpPrecedence)
{
    MExpr r = NULL;
    ASSERT(MExprList_Len(Stack) <= 2);
    if ((MExprList_Len(Stack) == 2) && (OpPrecedence >= PRECE_TIMES))
    {
        MExpr t1 = MExprList_PopOrphan(Stack);                              
        MExpr t2 = MExprList_PopOrphan(Stack);                              
                                                
        if ((t1->Type == etString) && (t2->Type == etString))               
        {
            MExprList_PushOrphan(Stack, MExpr_CreateStringX(MString_Join(t2->Str, t1->Str)));   
            MExpr_Release(t1);
            MExpr_Release(t2);
        }
        else
        {
            MSequence *ps = MSequence_Create(2);                                           
            MSequence_SetAtX(ps, 1, t1);                                        
            MSequence_SetAtX(ps, 0, t2);            
            MExprList_PushOrphan(Stack, MExpr_CreateHeadSeqX(sym_Times, ps));
        }
    };

    r = MExprList_PopOrphan(Stack);
    if (NULL == r)
        Parser_Report(tokener);
    return r ? r : MExpr_CreateSymbol(sym_Null);
}

#define MExprList_PopOrphanNull(Stack, OpPrecedence) MExprList_PopOrphanNull0(tokener, Stack, OpPrecedence)

static MExpr Parse_FromTokener(MTokener *tokener, const MInt Precedence, const bool bRequired = true);

#define PREP()                                                              \
do {                                                                        \
    if (MExprList_Len(Stack) == 2)                                          \
    {                                                                       \
        MExpr t1 = MExprList_PopOrphan(Stack);                              \
        MExpr t2 = MExprList_PopOrphan(Stack);                              \
        ps = MSequence_Create(2);                                           \
        MSequence_SetAtX(ps, 1, t1);                                        \
        MSequence_SetAtX(ps, 0, t2);                                        \
        if ((t1->Type == etString) && (t2->Type == etString))               \
        {   /* this is "evaluated" in parser */                             \
            MString s = MString_Join(t1->Str, t2->Str);                     \
            MSequence_Release(ps);                                          \
            MExprList_PushOrphan(Stack, MExpr_CreateStringX(s));            \
        }                                                                   \
        else if ((t2->Type == etSymbol) && (t1->Type == etHeadSeq) &&       \
                (is_header(t1, sym_Blank) || is_header(t1, sym_BlankSequence)\
                 || is_header(t1, sym_BlankNullSequence) || is_header(t1, sym_Optional) \
                 || is_header(t1, sym_PatternTest)))\
        {                                                                   \
            MExprList_PushOrphan(Stack, MExpr_CreateHeadSeqX(sym_Pattern, ps));\
        }                                                                   \
        else                                                                \
            MExprList_PushOrphan(Stack, MExpr_CreateHeadSeqX(sym_Times, ps));\
    }                                                                       \
    else;                                                                   \
} while (false)

#define PUSH()                                                              \
do {                                                                        \
    PREP();                                                                 \
    {                                                                       \
        ASSERT(MExprList_Len(Stack) < 2);                                   \
        MExprList_PushOrphan(Stack, r);                                     \
    }                                                                       \
} while (false)

//    MExprList_PushOrphan(Stack, r);

static MSequence *CreateIntSeq(MInt *p, const MInt len)
{
    MSequence *r = MSequence_Create(len);
    MInt i;
    for (i = 0; i < len; i++)
        MSequence_SetAtX(r, i, MExpr_CreateMachineInt(*p++));
    return r;
}

#define IsSpecialFlattenOp(sym)                 \
          ((sym == sym_CompoundExpression)      \
        || (sym == sym_Alternatives)            \
        || (sym == sym_SameQ)                   \
        || (sym == sym_UnsameQ)                 \
        || (sym == sym_Equal)                   \
        || (sym == sym_Unequal)                 \
          )

bool Parse1(MExprList *Stack, MTokener *tokener, const MInt Precedence, const bool bAcceptComma = false)
{
    MExpr r = NULL;
    MExprList *elst = NULL;
    MSymbol op;
    MSequence *ps;
    MInt OpPrecedence;

    if (tokener->Token == NULL)
        return false;

    if ((MExprList_Len(Stack) > 0) && (tokener->TokenType != ttOp))
    {
        if ((tokener->TokenType != ttString) && (PRECE_TIMES >= Precedence))
            return false;
    }
    else;

    switch (tokener->TokenType)
    {
    case ttSymbol:
        op = MSymbol_GetOrNewSymbol(tokener->Token);
        r = MExpr_CreateSymbol(op);
        PUSH();
        Token_Next(tokener, MExprList_Len(Stack));
//        if (op == sym_I)
//        {
//            r = MExpr_CreateComplexUnit();
//            PUSH();
//            Token_Next(tokener, MExprList_Len(Stack));
//            return true;
//        }

/*
        Token_NextCont(tokener, MExprList_Len(Stack) + 1);
        if ((tokener->TokenType == ttOp)
            && (   (tokener->OpSym == sym_Blank)
                || (tokener->OpSym == sym_BlankSequence)
                || (tokener->OpSym == sym_BlankNullSequence)
                || (tokener->OpSym == sym_Default)))
        {
            if (tokener->OpSym != sym_Default)
            {
                ps = MSequence_Create(2);
                MSequence_SetAtX(ps, 0, MExpr_CreateSymbol(op));
                MSequence_SetAtX(ps, 1, Parse_FromTokener(tokener, tokener->OpPrecedence));
                r = MExpr_CreateHeadSeqX(sym_Pattern, ps);
                PUSH();
            }
            else
            {
                ps = MSequence_Create(0);
                r = MExpr_CreateHeadSeq(sym_Blank, ps);
                MSequence_Release(ps);
                
                ps = MSequence_Create(2);
                MSequence_SetAtX(ps, 0, MExpr_CreateSymbol(op));
                MSequence_SetAtX(ps, 1, r);
                r = MExpr_CreateHeadSeq(sym_Pattern, ps);
                MSequence_Release(ps);

                ps = MSequence_Create(1);
                MSequence_SetAtX(ps, 0, r);
                r = MExpr_CreateHeadSeq(sym_Optional, ps);
                MSequence_Release(ps);

                PUSH();
                Token_Next(tokener, MExprList_Len(Stack));
            }
        }
        else
        {
            Token_ForgiveMe(tokener);
            r = MExpr_CreateSymbol(op);
            PUSH();
            Token_Next(tokener, MExprList_Len(Stack));
        }
*/
        return true;
    case ttNumber:
        r = MExpr_CreateNumX(Num_Create(tokener->Token));
        PUSH();
        Token_Next(tokener, MExprList_Len(Stack));
        return true;
    case ttString:
        r = MExpr_CreateString(EscapeString(MString_Unique(tokener->Token)));
        PUSH();
        Token_Next(tokener, MExprList_Len(Stack));
        return true;
    case ttOp:
        if (tokener->OpPrecedence > Precedence)
            return false;
        if ((tokener->OpPrecedence == Precedence) && tokener->lGrouping)
            return false;

        if ((tokener->OpSym == sym_Get) 
            || (tokener->OpSym == sym_PreIncrement)
            || (tokener->OpSym == sym_PreDecrement)
            || (tokener->OpSym == sym_Not)
            || (tokener->OpSym == sym_Square)
            || (tokener->OpSym == sym_Put)
            || (tokener->OpSym == sym_PutAppend))
        {
            op = tokener->OpSym;
            OpPrecedence = tokener->OpPrecedence;
            ps = MSequence_Create(1);
            Token_Next(tokener, MExprList_Len(Stack));
            MSequence_SetAtX(ps, 0, Parse_FromTokener(tokener, OpPrecedence));
            MExprList_PushOrphan(Stack, MExpr_CreateHeadSeq(op, ps));
            MSequence_Release(ps);
        }
        else if ((tokener->OpSym == sym_Blank) 
            || (tokener->OpSym == sym_BlankSequence)
            || (tokener->OpSym == sym_BlankNullSequence))
        {
            op = tokener->OpSym;
            Token_NextCont(tokener, MExprList_Len(Stack));
            if (tokener->TokenType == ttSymbol)
            {
                ps = MSequence_Create(1);
                MSequence_SetAtX(ps, 0, MExpr_CreateSymbol(MSymbol_GetOrNewSymbol(tokener->Token)));
                MExprList_PushOrphan(Stack, MExpr_CreateHeadSeq(op, ps));
                MSequence_Release(ps);
            }
            else
            {
                Token_ForgiveMe(tokener);
                ps = MSequence_Create(0);
                MExprList_PushOrphan(Stack, MExpr_CreateHeadSeq(op, ps));
                MSequence_Release(ps);
            }
            Token_Next(tokener, MExprList_Len(Stack));
        }
        else if (tokener->OpSym == sym_Optional)
        {
            ps = MSequence_Create(0);
            r = MExpr_CreateHeadSeq(sym_Blank, ps);
            MSequence_Release(ps);

            ps = MSequence_Create(1);
            MSequence_SetAtX(ps, 0, r);
            r = MExpr_CreateHeadSeq(sym_Optional, ps);
            MSequence_Release(ps);

            MExprList_PushOrphan(Stack, r);
            Token_Next(tokener, MExprList_Len(Stack));
        }
        else if ((tokener->OpSym == sym_Increment)
            || (tokener->OpSym == sym_Decrement)
            || (tokener->OpSym == sym_Factorial)
            || (tokener->OpSym == sym_Factorial2)
            || (tokener->OpSym == sym_Unset))
        {
            ps = MSequence_Create(1);
            MSequence_SetAtX(ps, 0, MExprList_PopOrphanNull(Stack, tokener->OpPrecedence));
            r = MExpr_CreateHeadSeqX(tokener->OpSym, ps);
            PUSH();
            Token_Next(tokener, MExprList_Len(Stack));
        }
//        else if ((tokener->OpSym == sym_None))
//        {
//            Token_Next(tokener, MExprList_Len(Stack));
//            MExprList_PushOrphan(Stack, Parse_FromTokener(tokener, tokener->OpPrecedence));
//        }
        else if (tokener->OpSym == sym_Subtract)
        {
            if (MExprList_Len(Stack) == 0)
            {
                Token_Next(tokener, MExprList_Len(Stack));
                MExpr t = Parse_FromTokener(tokener, PRECE_MONO_PLUS_SUB);
                if (t->Type == etNum)
                {
                    Num_NegBy(t->Num);
                    MExprList_PushOrphan(Stack, t);
                }
                else
                {
                    // this is: -a -> Times[-1, a]
                    ps = MSequence_Create(2);
                    MSequence_SetAtX(ps, 0, MExpr_CreateMachineInt(-1));
                    MSequence_SetAtX(ps, 1, t);
                    MExprList_PushOrphan(Stack, MExpr_CreateHeadSeq(sym_Times, ps));
                    MSequence_Release(ps);
                }                
            }
            else
            {
                // this is: a - b -> Plus[a, Times[-1, b]]
                OpPrecedence = tokener->OpPrecedence;
                Token_Next(tokener, MExprList_Len(Stack));
                r = Parse_FromTokener(tokener, OpPrecedence);

                if (r->Type == etNum)
                {
                    Num_NegBy(r->Num);
                }
                else
                {
                    ps = MSequence_Create(2);
                    MSequence_SetAtX(ps, 0, MExpr_CreateMachineInt(-1));
                    MSequence_SetAtX(ps, 1, r);
                    r = MExpr_CreateHeadSeqX(sym_Times, ps);
                }

                ps = MSequence_Create(2);
                MSequence_SetAtX(ps, 0, MExprList_PopOrphanNull(Stack, OpPrecedence));
                MSequence_SetAtX(ps, 1, r);
                MExprList_PushOrphan(Stack, MExpr_CreateHeadSeqX(sym_Plus, ps));
            }
        }
        else if (tokener->OpSym == sym_Divide)
        {
            if (MExprList_Len(Stack) == 0)
            {
                Token_Next(tokener, MExprList_Len(Stack));
                return false;
            }
            else
            {
                // this is: a / b -> Times[a, Power[b, -1]]
                OpPrecedence = tokener->OpPrecedence;

                ps = MSequence_Create(2);
                Token_Next(tokener, MExprList_Len(Stack));
                MSequence_SetAtX(ps, 0, Parse_FromTokener(tokener, OpPrecedence));
                MSequence_SetAtX(ps, 1, MExpr_CreateMachineInt(-1));
                r = MExpr_CreateHeadSeq(sym_Power, ps);
                MSequence_Release(ps);

                ps = MSequence_Create(2);
                MSequence_SetAtX(ps, 0, MExprList_PopOrphanNull(Stack, OpPrecedence));
                MSequence_SetAtX(ps, 1, r);
                MExprList_PushOrphan(Stack, MExpr_CreateHeadSeq(sym_Times, ps));
                MSequence_Release(ps);
            }
        }
        else if ((tokener->OpSym == sym_Plus) && (MExprList_Len(Stack) == 0))
        {
            // nop
            Token_Next(tokener, MExprList_Len(Stack));
            MExprList_PushOrphan(Stack, Parse_FromTokener(tokener, PRECE_MONO_PLUS_SUB));
        }
        else if (((tokener->OpSym == sym_PlusMinus)
            || (tokener->OpSym == sym_MinusPlus)) && MExprList_Len(Stack) == 0)
        {
            op = tokener->OpSym;
            ps = MSequence_Create(1);
            Token_Next(tokener, 0);
            MSequence_SetAtX(ps, 0, Parse_FromTokener(tokener, PRECE_MONO_PLUS_SUB));
            MExprList_PushOrphan(Stack, MExpr_CreateHeadSeq(op, ps));
            MSequence_Release(ps);
        }
        else if (tokener->OpSym == sym_Out)
        {
            MInt pos;
            if (MString_Len(tokener->Token) < 1)
                pos = -1;
            else
            {
                if (IsNumChar(MString_GetAt(tokener->Token, 1)))
                    pos = ParseMachineInt(tokener->Token);
                else
                    pos = - (MString_Len(tokener->Token) + 1);
            }
            
            ps = CreateIntSeq(&pos, 1);
            MExprList_PushOrphan(Stack, MExpr_CreateHeadSeq(sym_Out, ps));
            MSequence_Release(ps);
            Token_Next(tokener, 0);
        }
        else if (tokener->OpSym == sym_Derivative)
        {
            MInt pos = MString_Len(tokener->Token);
            
            ps = CreateIntSeq(&pos, 1);
            r = MExpr_CreateHeadSeq(sym_Derivative, ps);
            MSequence_Release(ps);
            
            ps = MSequence_Create(1);
            MSequence_SetAtX(ps, 0, MExprList_PopOrphanNull(Stack, tokener->OpPrecedence));
            MExprList_PushOrphan(Stack, MExpr_CreateHeadSeq(r, ps));
            MSequence_Release(r);
            MSequence_Release(ps);

            Token_Next(tokener, 0);
        }
        else if ((tokener->OpSym == sym_Slot) || (tokener->OpSym == sym_SlotSequence))
        {
            MInt pos = 1;
            if (MString_Len(tokener->Token) > 0)
                pos = ParseMachineInt(tokener->Token);

            ps = CreateIntSeq(&pos, 1);
            r = MExpr_CreateHeadSeqX(tokener->OpSym, ps);
            PUSH();
            Token_Next(tokener, 0);
        }
        else if (tokener->OpSym == sym_Function)
        {
            ps = MSequence_Create(1);
            MSequence_SetAtX(ps, 0, MExprList_PopOrphanNull(Stack, tokener->OpPrecedence));
            MExprList_PushOrphan(Stack, MExpr_CreateHeadSeqX(sym_Function, ps));
            Token_Next(tokener, 0);
        }
        else if (tokener->OpSym == sym_TagSet)
        {
            if ((MExprList_Len(Stack) < 1) || (MExprList_Top(Stack)->Type != etSymbol))
                return false;
            
            Token_Next(tokener, 0);
            OpPrecedence = tokener->OpPrecedence;
            r = Parse_FromTokener(tokener, OpPrecedence);
            if ((r->Type != etHeadSeq) || (r->HeadSeq.Head->Type != etSymbol))
                return false;

            ps = MSequence_Create(MSequence_Len(r->HeadSeq.pSequence) + 1);
            MSequence_SetAtX(ps, 0, MExprList_PopOrphan(Stack));
            MSequence_OverwriteSetFrom(ps, r->HeadSeq.pSequence, 1, 0, MSequence_Len(r->HeadSeq.pSequence));

            if (r->HeadSeq.Head->Symbol == sym_Set)
                MExprList_PushOrphan(Stack, MExpr_CreateHeadSeqX(sym_TagSet, ps));
            else if (r->HeadSeq.Head->Symbol == sym_SetDelayed)
                MExprList_PushOrphan(Stack, MExpr_CreateHeadSeqX(sym_TagSetDelayed, ps));
            else if (r->HeadSeq.Head->Symbol == sym_Unset)
                MExprList_PushOrphan(Stack, MExpr_CreateHeadSeqX(sym_TagUnset, ps));
            else
                MSequence_Release(ps);
            MExpr_Release(r);
        }
        else if (tokener->OpSym == sym_Pattern)
        {
            if (MExprList_Len(Stack) < 1)
                return false;
            if (MExprList_Top(Stack)->Type == etSymbol)
            {
                OpPrecedence = tokener->OpPrecedence;
                ps = MSequence_Create(2);
                Token_Next(tokener, MExprList_Len(Stack));
                MSequence_SetAtX(ps, 1, Parse_FromTokener(tokener, OpPrecedence));
                MSequence_SetAtX(ps, 0, MExprList_PopOrphanNull(Stack, OpPrecedence));
                MExprList_PushOrphan(Stack, MExpr_CreateHeadSeq(sym_Pattern, ps));
                MSequence_Release(ps);

            }
            else if ((MExprList_Top(Stack)->Type == etHeadSeq) 
                && (MExprList_Top(Stack)->HeadSeq.Head->Type == etSymbol)
                && (MExprList_Top(Stack)->HeadSeq.Head->Symbol == sym_Pattern))
            {
                OpPrecedence = tokener->OpPrecedence;
                ps = MSequence_Create(2);
                Token_Next(tokener, MExprList_Len(Stack));
                MSequence_SetAtX(ps, 1, Parse_FromTokener(tokener, OpPrecedence));
                MSequence_SetAtX(ps, 0, MExprList_PopOrphanNull(Stack, OpPrecedence));
                MExprList_PushOrphan(Stack, MExpr_CreateHeadSeq(sym_Optional, ps));
                MSequence_Release(ps);
            }
            else
                return false;
        }
        else if (tokener->OpSym == sym_Apply)
        {
            switch (MString_Len(tokener->Token))
            {
            case 1:
                
                OpPrecedence = tokener->OpPrecedence;

                if (MString_GetAt(tokener->Token, 1) == '~')
                {
                    // ~ f ~
                    Token_Next(tokener, 0);
                    if (tokener->TokenType == ttSymbol)
                    {
                        MSymbol fun = MSymbol_GetOrNewSymbol(tokener->Token);
                        Token_Next(tokener, 0);
                        if ((tokener->TokenType == ttOp) && (MString_Len(tokener->Token) == 1)
                            && (MString_GetAt(tokener->Token, 1) == '~'))
                        {
                            Token_Next(tokener, 0);
                            
                            ps = MSequence_Create(2);
                            MSequence_SetAtX(ps, 0, MExprList_PopOrphanNull(Stack, OpPrecedence));
                            MSequence_SetAtX(ps, 1, Parse_FromTokener(tokener, OpPrecedence));
                            MExprList_PushOrphan(Stack, MExpr_CreateHeadSeqX(fun, ps));
                        }
                    }
                }
                else
                {
                    // @
                    Token_Next(tokener, 0);
                    ps = MSequence_Create(1);
                    MSequence_SetAtX(ps, 0, Parse_FromTokener(tokener, OpPrecedence));
                    MExprList_PushOrphan(Stack, MExpr_CreateHeadXSeqX(MExprList_PopOrphanNull(Stack, OpPrecedence), ps));
                }
                
                break;
            case 2:
                if (MString_GetAt(tokener->Token, 1) == '/')
                {
                    // //
                    OpPrecedence = tokener->OpPrecedence;
                    Token_Next(tokener, 0);
                    ps = MSequence_Create(1);
                    MSequence_SetAtX(ps, 0, MExprList_PopOrphanNull(Stack, OpPrecedence));
                    MExprList_PushOrphan(Stack, MExpr_CreateHeadXSeqX(Parse_FromTokener(tokener, OpPrecedence), ps));
                }
                else 
                {
                    // @@
                    OpPrecedence = tokener->OpPrecedence;
                    Token_Next(tokener, 0);
                    ps = MSequence_Create(2);
                    MSequence_SetAtX(ps, 0, MExprList_PopOrphanNull(Stack, OpPrecedence));
                    MSequence_SetAtX(ps, 1, Parse_FromTokener(tokener, OpPrecedence));
                    MExprList_PushOrphan(Stack, MExpr_CreateHeadSeqX(sym_Apply, ps));
                }
                break;
            default:
                // @@@
                OpPrecedence = tokener->OpPrecedence;
                Token_Next(tokener, 0);
                ps = MSequence_Create(3);
                MSequence_SetAtX(ps, 0, MExprList_PopOrphanNull(Stack, OpPrecedence));
                MSequence_SetAtX(ps, 1, Parse_FromTokener(tokener, OpPrecedence));
                {
                    MSequence *l = MSequence_Create(1);
                    MSequence_SetAtX(l, 0, MExpr_CreateMachineInt(1));
                    MSequence_SetAtX(ps, 2, MExpr_CreateHeadSeq(sym_List, l)); // {1}
                    MSequence_Release(l);
                }
                
                MExprList_PushOrphan(Stack, MExpr_CreateHeadSeqX(sym_Apply, ps));
            }
        }
        else if (tokener->OpSym == sym_Span)
        {
            // stop it
            if (tokener->OpPrecedence == Precedence)
                return false;

            MExpr from = (MExprList_Len(Stack) == 0) ? MExpr_CreateMachineInt(1) : MExprList_PopOrphan(Stack);
            MExpr to;
            MExpr step = NULL;

            OpPrecedence = tokener->OpPrecedence;
            Token_Next(tokener, 0);
            to = Parse_FromTokener(tokener, OpPrecedence, false);
            if ((tokener->TokenType == ttOp) && (tokener->OpSym == sym_Span))
            {
                step = to;
                Token_Next(tokener, 0);
                to = Parse_FromTokener(tokener, OpPrecedence);
            }
            else if ((to->Type == etSymbol) && (to->Symbol == sym_Null))
                to = MExpr_CreateSymbol(sym_All);

            if (step)
            {
                ps = MSequence_Create(3);
                MSequence_SetAtX(ps, 0, from);
                MSequence_SetAtX(ps, 1, step);
                MSequence_SetAtX(ps, 2, to);
                MExprList_PushOrphan(Stack, MExpr_CreateHeadSeq(sym_Span, ps));
            }
            else
            {
                ps = MSequence_Create(2);
                MSequence_SetAtX(ps, 0, from);
                MSequence_SetAtX(ps, 1, to);
                MExprList_PushOrphan(Stack, MExpr_CreateHeadSeq(sym_Span, ps));
            }
            MSequence_Release(ps);
        }
        else if (MString_GetAt(tokener->Token, 1) == '~')
        {
            MSymbol s = sym_DollarFailed;
            OpPrecedence = tokener->OpPrecedence;
            Token_Next(tokener, 0);
            if (tokener->TokenType == ttSymbol)
            {
                s = MSymbol_GetOrNewSymbol(tokener->Token);
                Token_Next(tokener, 0);
                if ((tokener->TokenType == ttOp) && (MString_GetAt(tokener->Token, 1) == '~'))
                {
                    Token_Next(tokener, 0);
                    ps = MSequence_Create(2);
                    MSequence_SetAtX(ps, 0, MExprList_PopOrphanNull(Stack, OpPrecedence));
                    MSequence_SetAtX(ps, 1, Parse_FromTokener(tokener, OpPrecedence));
                    MExprList_PushOrphan(Stack, MExpr_CreateHeadSeq(s, ps));
                    MSequence_Release(ps);
                }
                else
                {
                    Token_Next(tokener, 0);
                    return false;
                }
            }
            else
            {
                Token_Next(tokener, 0);
                return false;
            }
        }
        else if (tokener->OpSym == sym_Sequence)
        {
            MString t;

            if (MString_GetAt(tokener->Token, 1) == ']')
            {
                return false;
            }

            t = MString_NewS(tokener->Token);    
            OpPrecedence = tokener->OpPrecedence;
            elst = MExprList_Create();
            Token_Next(tokener, 0);
            while (tokener->Token != NULL)
            {
                Token_Fixup(tokener, TOKEN_SEQ_OPEN);
                if ((tokener->TokenType == ttOp) && (tokener->OpSym == sym_Sequence))
                {
                    ps = MSequence_FromExprList(elst);

                    if (MExprList_Len(Stack) > 0)
                    {
                        MExpr t = MExprList_PopOrphanNull(Stack, OpPrecedence);
                        MExprList_PushOrphan(Stack, MExpr_CreateHeadSeqX(t, ps));
                        MExpr_Release(t);
                    }
                    else
                        MExprList_PushOrphan(Stack, MExpr_CreateHeadSeqX(sym_DollarFailed, ps));

                    Token_Next(tokener, MExprList_Len(Stack));
                    break;
                }
                else if (tokener->TokenType == ttComma)
                    Token_Next(tokener, 0);
                else
                    MExprList_PushOrphan(elst, Parse_FromTokener(tokener, MAX_PRECEDENCE));
            }

            MExprList_Release(elst);
        }
        else if (tokener->OpSym == sym_Part)
        {
            OpPrecedence = tokener->OpPrecedence;

            if (MString_GetAt(tokener->Token, 1) == ']')
            {
                return false;
            }
            
            Token_Next(tokener, 0);
            elst = MExprList_Create();
            while (tokener->Token != NULL)
            {
                if ((tokener->TokenType == ttOp) && (tokener->OpSym == sym_Part))
                {
                    ps = MSequence_Create(MExprList_Len(elst) + 1);
                    MSequence_SetFrom(ps, elst, 1, MExprList_Len(elst));

                    if (MExprList_Len(Stack) > 0)
                    {
                        MSequence_SetAtX(ps, 0, MExprList_PopOrphanNull(Stack, OpPrecedence));

                        MExprList_PushOrphan(Stack, MExpr_CreateHeadSeq(sym_Part, ps));
                    }
                    else;

                    MSequence_Release(ps);

                    Token_Next(tokener, MExprList_Len(Stack));
                    break;
                }
                else if (tokener->TokenType == ttComma)
                    Token_Next(tokener, 0);
                else
                    MExprList_PushOrphan(elst, Parse_FromTokener(tokener, MAX_PRECEDENCE));
            }

            MExprList_Release(elst);
        }
        else if (IsSpecialFlattenOp(tokener->OpSym))
        {
            MSymbol curOp = tokener->OpSym; XRef_IncRef(curOp);
            bool bRequired = curOp != sym_CompoundExpression;
            OpPrecedence = tokener->OpPrecedence;

            elst = MExprList_Create();
            MExprList_PushOrphan(elst, MExprList_PopOrphanNull(Stack, OpPrecedence));

            do
            {
                Token_Next(tokener, MExprList_Len(Stack));
                MExprList_PushOrphan(elst, Parse_FromTokener(tokener, OpPrecedence, bRequired));
            }
            while ((tokener->Token != NULL) && (tokener->TokenType == ttOp)
                   && (tokener->OpSym == curOp));

            MExprList_PushOrphan(Stack, MExpr_CreateHeadSeqX(curOp, MSequence_FromExprList(elst)));
            MSymbol_Release(curOp);
            MExprList_Release(elst);
        }
        else if (IsInequalityOp(tokener->OpSym))
        {
            OpPrecedence = tokener->OpPrecedence;

            elst = MExprList_Create();
            MExprList_PushOrphan(elst, MExprList_PopOrphanNull(Stack, OpPrecedence));

            do
            {
                MExprList_PushOrphan(elst, MExpr_CreateSymbol(tokener->OpSym));
                Token_Next(tokener, MExprList_Len(elst));
                MExprList_PushOrphan(elst, Parse_FromTokener(tokener, OpPrecedence));
            }
            while ((tokener->Token != NULL) && (tokener->TokenType == ttOp)
                   && IsInequalityOp(tokener->OpSym));

            ps = MSequence_FromExprList(elst);
            if (MSequence_Len(ps) > 3)
                MExprList_PushOrphan(Stack, MExpr_CreateHeadSeqX(sym_Inequality, ps));
            else
            {
                MSequence *p = MSequence_Create(2);
                MSequence_SetAt(p, 0, MSequence_EleAt(ps, 0));
                MSequence_SetAt(p, 1, MSequence_EleAt(ps, 2));
                MExprList_PushOrphan(Stack, MExpr_CreateHeadSeqX(MSequence_EleAt(ps, 1), p));
                MSequence_Release(ps);
            }

            MExprList_Release(elst);
            
//            MExpr t = MExprList_PopOrphanNull(Stack);
//            OpPrecedence = tokener->OpPrecedence;
//            int k;
//            if ((t->Type == etHeadSeq) 
//                && (t->HeadSeq.Head->Type == etSymbol)
//                && (t->HeadSeq.Head->Symbol == sym_Inequality))
//            {
//                ps = MSequence_Create(MSequence_Len(t->HeadSeq.pSequence) + 2);
//                for (k = 0; k < MSequence_Len(t->HeadSeq.pSequence); k++)
//                    MSequence_SetAt(ps, k, MSequence_GetAt(t->HeadSeq.pSequence, k));
//                MExpr_Release(t);
//            }
//            else
//            {
//                k = 1;
//                ps = MSequence_Create(3);
//                MSequence_SetAtX(ps, 0, t);
//            }
//            
//            MSequence_SetAtX(ps, k + 0, MExpr_CreateSymbol(tokener->OpSym));
//
//            Token_Next(tokener, MExprList_Len(Stack));
//            MSequence_SetAtX(ps, k + 1, Parse_FromTokener(tokener, OpPrecedence));
//
//            MExprList_PushOrphan(Stack, MExpr_CreateHeadSeq(sym_Inequality, ps));
//            MSequence_Release(ps);
        }
        else if ((tokener->OpSym == sym_Repeated) || (tokener->OpSym == sym_RepeatedNull))
        {
            ps = MSequence_Create(1);
            MSequence_SetAtX(ps, 0, MExprList_PopOrphanNull(Stack, tokener->OpPrecedence));
            MExprList_PushOrphan(Stack, MExpr_CreateHeadSeqX(tokener->OpSym, ps));
            Token_Next(tokener, MExprList_Len(Stack));
        }
        else
        {
            op = tokener->OpSym;
            OpPrecedence = tokener->OpPrecedence;
            ps = MSequence_Create(2);
            Token_Next(tokener, MExprList_Len(Stack));
            MSequence_SetAtX(ps, 1, Parse_FromTokener(tokener, OpPrecedence));
            MSequence_SetAtX(ps, 0, MExprList_PopOrphanNull(Stack, OpPrecedence));
            MExprList_PushOrphan(Stack, MExpr_CreateHeadSeq(op, ps));
            MSequence_Release(ps);
        }
        
        return true;
    case ttComma:
        return false;
    case ttLeftParenthesis:
        Token_Next(tokener, 0);
        r = Parse_FromTokener(tokener, MAX_PRECEDENCE);
        if (tokener->TokenType == ttRightParenthesis)
        {
            PUSH();
            Token_Next(tokener, MExprList_Len(Stack));
            return true;
        }
        else
        {
            MExpr_Release(r);
            return false;
        }        
    case ttRightParenthesis:
        return false;
    case ttLeftBrace:
        Token_Next(tokener, 0);
        elst = MExprList_Create();
        while (tokener->Token != NULL)
        {
            if (tokener->TokenType == ttRightBrace)
            {
                ps = MSequence_FromExprList(elst);
                r = MExpr_CreateHeadSeq(sym_List, ps);
                MSequence_Release(ps);

                PUSH();
                Token_Next(tokener, MExprList_Len(Stack));

                break;
            }
            else if (tokener->TokenType == ttComma)
                Token_Next(tokener, 0);
            else
                MExprList_PushOrphan(elst, Parse_FromTokener(tokener, MAX_PRECEDENCE)); 
        }

        MExprList_Release(elst);
        return true;
    case ttRightBrace:
        return false;
    default:
        return false;
    }
}

static void Parse_FlattenTop(MExprList *Stack)
{
    MExpr expr = MExprList_Top(Stack);
    if (NULL == expr)
        return;
    if ((expr->Type == etHeadSeq) && (expr->HeadSeq.Head->Type == etSymbol) &&
        (MSymbol_HasAttr(expr->HeadSeq.Head->Symbol, aFlat)))
    {
        MSymbol s = expr->HeadSeq.Head->Symbol;
        int len = MSequence_Len(expr->HeadSeq.pSequence);
        MExpr *p = expr->HeadSeq.pSequence->pExpr;
        int i;
        bool bFound = false;
        for (i = 0; i < MSequence_Len(expr->HeadSeq.pSequence); i++)
        {
            if (((*p)->Type == etHeadSeq) && ((*p)->HeadSeq.Head->Type == etSymbol)
                && ((*p)->HeadSeq.Head->Symbol == s))
            {
                len += MSequence_Len((*p)->HeadSeq.pSequence) - 1;
                bFound = true;
            }
            p++;
        }

        if (bFound)
        {
            MSequence *ps = MSequence_Create(len);
            int index = 0;
            p = expr->HeadSeq.pSequence->pExpr;
            for (i = 0; i < MSequence_Len(expr->HeadSeq.pSequence); i++)
            {
                if (((*p)->Type == etHeadSeq) && ((*p)->HeadSeq.Head->Type == etSymbol)
                    && ((*p)->HeadSeq.Head->Symbol == s))
                {
                    int j;
                    for (j = 0; j < MSequence_Len((*p)->HeadSeq.pSequence); j++)
                        MSequence_SetAt(ps, index++, (*p)->HeadSeq.pSequence->pExpr[j]);
                }
                else
                    MSequence_SetAt(ps, index++, *p);
                p++;
            }

            // replace
            expr = MExpr_CreateHeadSeqX(expr->HeadSeq.Head->Symbol, ps);
            MExprList_Pop(Stack);
            MExprList_PushOrphan(Stack, expr);
        }   
    }
}

static MExpr Parse_FromTokener(MTokener *tokener, const MInt Precedence, const bool bRequired)
{
    MExprList *Stack = MExprList_Create();
    MExpr expr;
    MInt len;

    while (Parse1(Stack, tokener, Precedence))
        Parse_FlattenTop(Stack);

    while (MExprList_Len(Stack) >= 2) 
    {
        MSequence *ps;
        PREP();
    }

    len = MExprList_Len(Stack);
    ASSERT(len <= 1);

    if (len == 1)
    {
        Parse_FlattenTop(Stack);
        expr = MExprList_PopOrphan(Stack); 
        MExprList_Release(Stack); 
        return expr;
    }
    else
    {
        if (bRequired) 
        {
            Parser_Report(tokener);
            
            // nothing found?
            // maybe, there is a bad token, so move forward
            if (tokener->Token) Token_Next(tokener, 0); 
        }

        MExprList_Release(Stack);
        return MExpr_CreateSymbol(sym_Null);
    }
}

MExpr Parse_FromTokener(MTokener *tokener)
{
    Token_Next(tokener, 0);
    return Parse_FromTokener(tokener, MAX_PRECEDENCE);
}

static MExpr Parse_Lisp1(MTokener *tokener);

static MExpr Parse_Lisp2(MTokener *tokener, bool bRequired = false)
{
#define push return
    switch (tokener->TokenType)
    {
    case ttSymbol:
        push(MExpr_CreateSymbol(MSymbol_GetOrNewSymbol(tokener->Token)));
        break;
    case ttNumber:
        push(MExpr_CreateNumX(Num_Create(tokener->Token)));
        break;
    case ttString:
        push(MExpr_CreateString(EscapeString(MString_Unique(tokener->Token))));
        break;
    case ttOp:        
        push(MExpr_CreateSymbol(tokener->OpSym));
        break;
    case ttLeftParenthesis:
        Token_Next(tokener, 0);
        push(Parse_Lisp1(tokener));
        break;
    case ttComma:
    case ttLeftBrace:
    case ttRightBrace:
    default:
        if (bRequired) Parser_Report(tokener);
        return NULL;
    }
}

static MExpr Parse_Lisp1(MTokener *tokener)
{
    MExprList *all = MExprList_Create();
    MExpr r;
    MExpr head = NULL;

    while (tokener->TokenType != ttRightParenthesis)
    {
        MExpr t = Parse_Lisp2(tokener);
        if (t)
        {
            if (NULL == head) 
                head = t;
            else
                MExprList_PushOrphan(all, t);
        }
        else
            break;
        Token_Next(tokener, 0);
    }

    r = head ? MExpr_CreateHeadXSeqX(head, MSequence_FromExprList(all)) : MExpr_CreateSymbol(sym_Null); 
    MExprList_Release(all);
    return r;
}

static MExpr Parse_Lisp(MTokener *tokener)
{
    MExpr expr = NULL;
    if (tokener->TokenType == ttLeftParenthesis)
    {
        Token_Next(tokener, 0);
        expr = Parse_Lisp1(tokener);
        Token_Next(tokener, 0);
        if (tokener->Token && (tokener->TokenType != ttRightParenthesis)) 
            Parser_Report(tokener);
    }
    else
        expr = Parse_Lisp2(tokener); 

    return expr ? expr : MExpr_CreateSymbol(sym_Null);

/*
    MExprList *all = MExprList_Create();
    MExpr expr;
    MInt len;
    if (tokener->TokenType == ttLeftParenthesis)
    {
        while (tokener->TokenType == ttLeftParenthesis)
        {
            Token_Next(tokener, 0);
            MExprList_PushOrphan(all, Parse_Lisp1(tokener));
            Token_Next(tokener, 0);
        }
        if (tokener->Token && (tokener->TokenType != ttRightParenthesis)) 
            Parser_Report(tokener);
    }
    else
        MExprList_PushOrphan(all, Parse_Lisp2(tokener)); 

    if (MExprList_Len(all) > 1)
        expr = MExpr_CreateHeadSeqX(sym_CompoundExpression, MSequence_FromExprList(all));
    else if (MExprList_Len(all) == 1)
        expr = MExprList_PopOrphan(all);
    else
        expr = MExpr_CreateSymbol(sym_Null);
    MExprList_Release(all);
    return expr;
    */
}

MExpr Parse_FromTokenerLisp(MTokener *tokener)
{
    Token_Next(tokener, 0);
    return Parse_Lisp(tokener);
}
