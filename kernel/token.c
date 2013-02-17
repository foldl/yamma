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
#include "eval.h"
#include "token.h"
#include "mm.h"
#include "encoding.h"
#include "hash.h"
#include "sys_char.h"
#include "sys_sym.h"

struct MOpSymMap
{
    char Op[50];
    MSymbol *Sym;
    MInt Precedence;
    MInt OperandL;
    MBool rGrouping;
};

#define OP_COMMENT_OPEN  sym_Skip
#define OP_COMMENT_CLOSE sym_Number
#define OP_LEFT_PAREN    sym_Real
#define OP_RIGHT_PAREN   sym_Complex
#define OP_LEFT_BRACE    sym_List
#define OP_RIGHT_BRACE   sym_Close
#define OP_NEXT_LINE     sym_Next

// special
// #:  Precedence 3
// %:  Precedence 4 
// ':  Derivative
static MOpSymMap OpSymMap[] = 
{
    // Precedence 0
    // numbers, symbol, string
    {"(*", &OP_COMMENT_OPEN, 0},
    {"*)", &OP_COMMENT_CLOSE, 0},
    {"(", &OP_LEFT_PAREN, 0},
    {")", &OP_RIGHT_PAREN, 0},
    {"{", &OP_LEFT_BRACE, 0},
    {"}", &OP_RIGHT_BRACE, 0},
    {"\\", &OP_NEXT_LINE, 0},

    // Precedence 1
    // matrix, piecewise

    // Precedence 2
    {"::", &sym_MessageName, 2},        // xx::xx

    // #:  Precedence 3

    // %:  Precedence 4

    // Precedence 5
    {"_", &sym_Blank, 5, 0, true},               // _H _
    {"_.", &sym_Optional, 5, 0, true},               // _H _
    {"__", &sym_BlankSequence, 5, 0, true},      // __
    {"___", &sym_BlankNullSequence, 5, 0, true}, // ___

    // Precedence 6
    {"<<", &sym_Get, 6, 0},             // << x
    
    // Precedence 7
    // Overscript, Overscript

    // Precedence 8
    // Subscript

    // Precedence 9
    // (interpreted version of boxes)

    // Precedence 10
    {"?", &sym_PatternTest, 10},        // expr ? test

    // Precedence 11
    {"[", &sym_Sequence, 11}, 
    {"]", &sym_Sequence, 11},
    {"[[", &sym_Part, 11},
    {"]]", &sym_Part, 11},

    // Precedence 12
    // (boxes constructed from expr)
    {"++", &sym_Increment, 12, 1},      // xx++
    {"--", &sym_Decrement, 12, 1},      // xx--

    // Precedence 13
    {"++", &sym_PreIncrement, 13, 0},   // ++xx
    {"--", &sym_PreDecrement, 13, 0},   // --xx

    // Precedence 14
    {"@", &sym_Apply, 14, 1, true},
    {"\\[InvisibleApplication]", &sym_Apply, 14},
    {"~", &sym_Apply, 14},

    // Precedence 15
    {"/@", &sym_Map, 15, 1, true},
    {"//@", &sym_MapAll, 15, 1, true},
    {"@@", &sym_Apply, 15, 1, true},
    {"@@@", &sym_Apply, 15, 1, true},

    // Precedence 16
    {"!", &sym_Factorial, 16, 1},
    {"!!", &sym_Factorial2, 16},

    // Precedence 17
    // Conjugate

    // Precedence 18
    {"'", &sym_D, 18},

    // Precedence 19
    {"<>", &sym_StringJoin, 19},

    // Precedence 20
    {"^", &sym_Power, 20, 1, true},

    // Precedence 21
    // vertical arrow and vector operators

    // Precedence 22
    // Sqrt

    // Precedence 23
    // Integrate, 
    
    // Precedence 24
    // D

    // Precedence 25
    {"\\[Square]", &sym_Square, 25, 0, true},
    {"\\[SmallCircle]", &sym_SmallCircle, 25},

    // Precedence 26
    {"\\[CircleDot]", &sym_CircleDot, 26}, 

    // Precedence 27
    {"**", &sym_NonCommutativeMultiply, 27},
    {"\\[NonCommutativeMultiply]", &sym_NonCommutativeMultiply, 27},

    // Precedence 28
    {"\\[Cross]", &sym_Cross, 28},

    // Precedence 29
    {".", &sym_Dot, 29},
    
    // Precedence 30
//    {"-", &sym_Plus, 30, 0},
//    {"+", &sym_None, 30, 0},
//    {"\\[PlusMinus]", &sym_PlusMinus, 30, 0},
//    {"\\[MinusPlus]", &sym_MinusPlus, 30, 0},
    
    // Precedence 31
    {"/", &sym_Divide, 31}, 
    {"\\[Divide]", &sym_Divide, 31},
    {"*", &sym_Times, 31}, 
    {"\\[Times]", &sym_Times, 31},

    // Precedence 32
    {"\\[Backslash]", &sym_Backslash, 32}, 

    // Precedence 33
    {"\\[Diamond]", &sym_Diamond, 33}, 

    // Precedence 34
    {"\\[Wedge]", &sym_Wedge, 34}, 

    // Precedence 35
    {"\\[Vee]", &sym_Vee, 35}, 

    // Precedence 36
    {"\\[CircleTimes]", &sym_CircleTimes, 36}, 

    // Precedence 37
    {"\\[CenterDot]", &sym_CenterDot, 37}, 
    
    // Precedence 38
//    {"*", &sym_Times, 38}, 
//    {"\\[Times]", &sym_Times, 38},

    // Precedence 39
    {"\\[Star]", &sym_Star, 39}, 
    
    // Precedence 40
    // Product
    
    // Precedence 41
    {"\\[VerticalTilde]", &sym_VerticalTilde, 41},

    // Precedence 42
    {"\\[Coproduct]", &sym_Coproduct, 42},
    
    // Precedence 43
    {"\\[Cap]", &sym_Cap, 43},
    {"\\[Cup]", &sym_Cup, 43},
    
    // Precedence 44
    {"\\[CirclePlus]", &sym_CirclePlus, 44},
    {"\\[CircleMinus]", &sym_CircleMinus, 44},

    // Precedence 45
    // Sum

    // Precedence 46
    {"+", &sym_Plus, 46, 1},
    {"-", &sym_Subtract, 46, 1},
    {"\\[PlusMinus]", &sym_PlusMinus, 46, 1},
    {"\\[MinusPlus]", &sym_MinusPlus, 46, 1},

    // Precedence 47
    {"\\[Intersection]", &sym_Intersection, 47},
    
    // Precedence 48
    {"\\[Union]", &sym_Union, 48},

    // Precedence 49
    {";;", &sym_Span, 49},

    // Precedence 50
    {"==", &sym_Equal, 50}, 
    {"\\[Equal]", &sym_Equal, 50},
    {"!=", &sym_Unequal, 50}, 
    {"\\[NotEqual]", &sym_Unequal, 50},
    {"<", &sym_Less, 50},
    {"<=", &sym_LessEqual, 50}, 
    {"\\[LessEqual]", &sym_LessEqual, 50},
    {">", &sym_Greater, 50},
    {">=", &sym_GreaterEqual, 50}, 
    {"\\[GreaterEqual]", &sym_GreaterEqual, 50},
    {"\\[VerticalBar]", &sym_VerticalBar, 50},
    {"\\[NotVerticalBar]", &sym_NotVerticalBar, 50},
    {"\\[DoubleVerticalBar]", &sym_DoubleVerticalBar, 50},
    {"\\[NotDoubleVerticalBar]", &sym_NotDoubleVerticalBar, 50},

    // Precedence 51
    {"===", &sym_SameQ, 51},
    {"=!=", &sym_UnsameQ, 51},

    // Precedence 52
    {"\\[Element]", &sym_Element, 52},
    {"\\[NotElement]", &sym_NotElement, 52},
    {"\\[Subset]", &sym_Subset, 52},
    {"\\[Superset]", &sym_Superset, 52},

    // Precedence 53
//    {"\\[ForAll]", &sym_ForAll, 53},
//    {"\\[Exists]", &sym_Exists, 53},
//    {"\\[NotExists]", &sym_NotExists, 53},
    
    // Precedence 54
    {"!", &sym_Not, 54},
    {"\\[Not]", &sym_Not, 54},
    
    // Precedence 55
    {"&&", &sym_And, 55}, 
    {"\\[And]", &sym_And, 55},
    {"\\[Nand]", &sym_Nand, 55},

    // Precedence 56
    {"\\[Xor]", &sym_Xor, 56},

    // Precedence 57
    {"||", &sym_Or, 57},
    {"\\[Or]", &sym_Or, 57},
    {"\\[Nor]", &sym_Nor, 57},

    // Precedence 58
    {"\\[Implies]", &sym_Implies, 58},
    {"\\[RoundImplies]", &sym_Implies, 58},

    // Precedence 59
    {"\\[RightTee]", &sym_RightTee, 59, 1, true},
    {"\\[DoubleRightTee]", &sym_DoubleRightTee, 59, 1, true},
    {"\\[LeftTee]", &sym_LeftTee, 59},
    {"\\[DoubleLeftTee]", &sym_DoubleLeftTee, 59},

    // Precedence 60
    {"\\[SuchThat]", &sym_SuchThat, 60},

    // Precedence 61
    {"..", &sym_Repeated, 61},
    {"...", &sym_RepeatedNull, 61},

    // Precedence 62
    {"|", &sym_Alternatives, 62},

    // Precedence 63
    {":", &sym_Pattern, 63},

    // Precedence 64
    {"~~", &sym_StringExpression, 64},

    // Precedence 65
    {"/;", &sym_Condition, 65},

    // Precedence 66
    {"->", &sym_Rule, 66, 1, true}, 
    {"\\[Rule]", &sym_Rule, 66, 1, true},
    {":->", &sym_RuleDelayed, 66, 1, true}, 
    {"\\[RuleDelayed]", &sym_RuleDelayed, 66, 1, true},

    // Precedence 67
    {"/.", &sym_ReplaceAll, 67},
    {"//.", &sym_ReplaceRepeated, 67},

    // Precedence 68
    {"-=", &sym_SubtractFrom, 68, 1, true},
    {"+=", &sym_AddTo, 68, 1, true},
    {"*=", &sym_TimesBy, 68, 1, true},
    {"/=", &sym_DivideBy, 68, 1, true},

    // Precedence 69
    {"&", &sym_Function, 69},

    // Precedence 70
    {"\\[Colon]", &sym_Colon, 70},

    // Precedence 71
    {"//", &sym_Apply, 71},

    // Precedence 72
    {"\\[VerticalSeparator]", &sym_VerticalSeparator, 72},

    // Precedence 73
    {"\\[Therefore]", &sym_Therefore, 73, 1, true},
    {"\\[Because]", &sym_Because, 73, 1, true},

    // Precedence 74
    {"=", &sym_Set, 74, 1, true},
    {":=", &sym_SetDelayed, 74, 1, true},
    {"^=", &sym_UpSet, 74, 1, true},
    {"^:=", &sym_UpSetDelayed, 74, 1, true},
    {"/:", &sym_TagSet, 74}, // {"\\[]", &sym_TagSetDelayed, 74},
    {"=.", &sym_Unset, 74}, // {"\\[]", &sym_TagUnset, 74},

    // Precedence 75
    {">>", &sym_Put, 75},
    {">>>", &sym_PutAppend, 75},

    // Precedence 76
    {";", &sym_CompoundExpression, 76}
    
    // Precedence 77
    // FormBox
};


struct MOpHashItem
{
    MString Op;
    MSymbol Symbol[OP_MAGIC_MAX];
    MInt Precedence[OP_MAGIC_MAX];
    MInt OperandL[OP_MAGIC_MAX];
    MInt Magic;
    MBool rGrouping;
};

static MHashTable *OpHashTable = NULL;
static MHashTable *OpSequenceHashTable = NULL;

MTokener *Token_NewFromMemory(const char *str, MString encoding)
{
    MString s = EncodingToMString((MByte *)str, strlen(str), encoding);
    MTokener *r = Token_NewFromMemory(s);
    MString_Release(s);
    return r;
}

MTokener *Token_NewFromMemory(MString str)
{
    MTokener *r = (MTokener *)YM_malloc(sizeof(MTokener));
    r->CurLine = MString_NewS(str);
    r->Source = tsMemory;
    r->PosX = 1;
    r->PosY = 1;
    return r;
}

MTokener *Token_NewFromFile(const char *fn, MString encoding)
{
    MTokener *r = (MTokener *)YM_malloc(sizeof(MTokener));
    if (MIO_OpenFile(&r->file, fn, encoding) == 0)
    {
        YM_free(r);
        return NULL;
    }
    r->Source = tsFile;
    r->PosX = 1;
    r->PosY = 1;
    r->FileName = MString_NewC(fn);
    r->CurLine = MIO_ReadLine(&r->file);
    return r;
}

MTokener *Token_NewFromFile(const MString fn, MString encoding)
{
    MTokener *r = (MTokener *)YM_malloc(sizeof(MTokener));
    if (MIO_OpenFile(&r->file, fn, encoding) == 0)
    {
        YM_free(r);
        return NULL;
    }
    r->Source = tsFile;
    r->PosX = 1;
    r->PosY = 1;
    r->FileName = MString_NewS(fn);
    r->CurLine = MIO_ReadLine(&r->file);
    return r;
}

void Token_Delete(MTokener *tokener)
{
    if (tokener->Token)
        MString_Release(tokener->Token);
    if (tokener->CurLine)
        MString_Release(tokener->CurLine);

    if (tsFile == tokener->Source)
    {
        MIO_CloseFile(&tokener->file);
    }

    YM_free(tokener);
}

static int Toke_NextMemory1(MTokener *tokener, const MInt OperandL, const bool bSkipBlank)
{
    MChar c;

    // parse multi-line string
    if (tokener->OpSym == OP_NEXT_LINE)
    {
        tokener->OpSym = NULL;
        if (tokener->TokenType == ttString)
        {
            ASSERT(tokener->PosX > 0);
            tokener->PosX--;
            goto find_string;
        }
        else;
    }
    else;
    
    if (tokener->Token)
        MString_Release(tokener->Token);
    tokener->Token = NULL;
    tokener->TokenType = ttUnkown;
    tokener->OpSym = NULL;

    // skip blank
    if (bSkipBlank)
        while ((tokener->PosX <= MString_Len(tokener->CurLine)) && (IsBlankChar(MString_GetAt(tokener->CurLine, tokener->PosX))))
            tokener->PosX++;

    if (tokener->PosX > MString_Len(tokener->CurLine))
        return 0;

    c = MString_GetAt(tokener->CurLine, tokener->PosX);

    if (IsBlankChar(c))
        return 0;
    
    if (IsLetterChar(c))
    {
        int pos = tokener->PosX + 1;
        const MChar *p = MString_GetData(tokener->CurLine) + tokener->PosX;
        tokener->TokenType = ttSymbol;
        while (pos <= MString_Len(tokener->CurLine))
        {
            MChar c = *p++; pos++;
            if (!IsLetterChar(c) && !IsNumChar(c))
            {
                pos--;
                break;
            }
        }

        tokener->Token = MString_Copy(tokener->CurLine, tokener->PosX, pos - tokener->PosX);
        tokener->PosX = pos;
    }
    else if (c == Char_RawDoubleQuote)
    {
find_string:        
        int pos = tokener->PosX + 1;
        bool bOK = false;
        const MChar *p = MString_GetData(tokener->CurLine) + tokener->PosX;
        tokener->TokenType = ttString;
        while (pos <= MString_Len(tokener->CurLine))
        {
            MChar c = *p++; pos++;
            if (c == Char_RawDoubleQuote)
            {
                MString tmp =  MString_Copy(tokener->CurLine, tokener->PosX + 1, pos - tokener->PosX - 2);
                if (tokener->Token)
                {
                    MString_Cat(tokener->Token, tmp);
                    MString_Release(tmp);
                }
                else
                    tokener->Token = tmp;
                bOK = true;
                break;
            }
            else if (c == Char_RawBackslash)
            {
                if (pos <= MString_Len(tokener->CurLine))
                {
                    c = *p++; pos++;
                }
                else
                {
                    MString tmp =  MString_Copy(tokener->CurLine, tokener->PosX + 1, pos - tokener->PosX - 2);
                    if (tokener->Token)
                    {
                        MString_Cat(tokener->Token, tmp);
                        MString_Release(tmp);
                    }
                    else
                        tokener->Token = tmp;

                    bOK = true;
                    tokener->OpSym = OP_NEXT_LINE;
                    break;
                }
            }
            else;
        }

        tokener->PosX = pos;
        if (!bOK)
        {
            if (tokener->Token)
            {
                MString_Release(tokener->Token);
                tokener->Token = NULL;
            }
            return -1;
        }
        else;
    }
    else if (c == Char_RawComma)
    {
        tokener->Token = MString_Copy(tokener->CurLine, tokener->PosX++, 1);
        tokener->TokenType = ttComma;
    }
    else if (IsNumChar(c))
    {
        int pos = tokener->PosX + 1;
        const MChar *p = MString_GetData(tokener->CurLine) + tokener->PosX;
        tokener->TokenType = ttNumber;
        while (pos <= MString_Len(tokener->CurLine))
        {
            MChar c = *p++; pos++;

            if (!IsNumChar(c) && (Char2Int(c) == 0))
            {
                if ((c == Char_RawDot))
                    continue;
                
                if (c == Char_RawWedge)
                {
                    if ((pos <= MString_Len(tokener->CurLine)) 
                        && (MString_GetAt(tokener->CurLine, pos) == c))
                    {
                        p++; pos++;
                        continue;
                    }
                    else;
                }
                else if (c == Char_RawStar)
                {
                    if ((pos <= MString_Len(tokener->CurLine)) 
                        && (MString_GetAt(tokener->CurLine, pos) == Char_RawWedge))
                    {
                        p++; pos++;

                        // here, a "-" might appear
                        if (pos <= MString_Len(tokener->CurLine))
                        {
                            if (*p == Char_RawDash)
                            {
                                p++; pos++;
                            }
                            else;
                        }
                        else;

                        continue;
                    }
                    else;
                }
                else if (c == Char_RawBackquote)
                {
                    p++; pos++;
                    continue;
                }
                
                pos--;
                break;
            }
        }

        tokener->Token = MString_Copy(tokener->CurLine, tokener->PosX, pos - tokener->PosX);
        tokener->PosX = pos;
    }
    else if (IsOpChar(c))
    {
        tokener->TokenType = ttOp;
        MOpHashItem Item;
        MOpHashItem *pOp = NULL;
        MInt len;

        if (c == '#')
        {
            tokener->OpSym = sym_Slot;
            tokener->OpPrecedence = 3;

            tokener->PosX++;
            if ((tokener->PosX <= MString_Len(tokener->CurLine)) 
                && (MString_GetAt(tokener->CurLine, tokener->PosX) == '#'))
            {
                tokener->PosX++;
                tokener->OpSym = sym_SlotSequence;
            }

            len = tokener->PosX;
            while (len <= MString_Len(tokener->CurLine))
            {
                if (IsNumChar(MString_GetAt(tokener->CurLine, len)))
                    len++;
                else
                    break;
            }

            if (len - tokener->PosX > 0)
            {
                tokener->Token = MString_Copy(tokener->CurLine, tokener->PosX, len - tokener->PosX);
                tokener->PosX = len;
            }
            else
                tokener->Token = MString_NewC("");
        }
        else if (c == '%')
        {
            tokener->OpSym = sym_Out;
            tokener->OpPrecedence = 4;

            tokener->PosX++;
            len = tokener->PosX;
            if (tokener->PosX <= MString_Len(tokener->CurLine))
            {
                if (MString_GetAt(tokener->CurLine, tokener->PosX) == '%')
                {
                    while (len <= MString_Len(tokener->CurLine))
                    {
                        if (MString_GetAt(tokener->CurLine, len) == '%')
                            len++;
                        else
                            break;
                    }
                }
                else if (IsNumChar(MString_GetAt(tokener->CurLine, tokener->PosX)))
                {
                    while (len <= MString_Len(tokener->CurLine))
                    {
                        if (IsNumChar(MString_GetAt(tokener->CurLine, len)))
                            len++;
                        else
                            break;
                    }
                }
            }

            if (len - tokener->PosX > 0)
            {
                tokener->Token = MString_Copy(tokener->CurLine, tokener->PosX, len - tokener->PosX);
                tokener->PosX = len;
            }
            else
                tokener->Token = MString_NewC("");
        }
        else if (c == '\'')
        {
            tokener->OpSym = sym_Derivative;
            tokener->OpPrecedence = 24;

            len = tokener->PosX;
            while (len <= MString_Len(tokener->CurLine))
            {
                if (MString_GetAt(tokener->CurLine, len) == '\'')
                    len++;
                else
                    break;
            }

            tokener->Token = MString_Copy(tokener->CurLine, tokener->PosX, len - tokener->PosX);
            tokener->PosX = len;
        }
        else;

        if (tokener->Token)
            return 0;

        len = 1;
        while (len <= MString_Len(tokener->CurLine) - tokener->PosX + 1)
        {
            MString sub = MString_Copy(tokener->CurLine, tokener->PosX, len);
            MString p = (MString)MHashTable_Find(OpSequenceHashTable, sub);
            MString_Release(sub);
            if (p != NULL)
                len++;
            else
                break;
        }

        len--;
        {
            Item.Op = MString_Copy(tokener->CurLine, tokener->PosX, len);
            pOp = (MOpHashItem *)MHashTable_Find(OpHashTable, &Item);
            MString_Release(Item.Op);
        }
        
        if (pOp && pOp->Symbol[0])
        {
            if (pOp->Symbol[0] == OP_LEFT_PAREN)
            {
                tokener->Token = MString_Copy(tokener->CurLine, tokener->PosX++, 1);
                tokener->TokenType = ttLeftParenthesis;
            }
            else if (pOp->Symbol[0] == OP_RIGHT_PAREN)
            {
                tokener->Token = MString_Copy(tokener->CurLine, tokener->PosX++, 1);
                tokener->TokenType = ttRightParenthesis;
            }
            else if (pOp->Symbol[0] == OP_LEFT_BRACE)
            {
                tokener->Token = MString_Copy(tokener->CurLine, tokener->PosX++, 1);
                tokener->TokenType = ttLeftBrace;
            }
            else if (pOp->Symbol[0] == OP_RIGHT_BRACE)
            {
                tokener->Token = MString_Copy(tokener->CurLine, tokener->PosX++, 1);
                tokener->TokenType = ttRightBrace;
            }
            else;

            if (tokener->Token)
                return 0;

            tokener->OpSym = NULL;

            // choose a symbol for magic operators
            if (pOp->Magic == 0)
            {
                tokener->OpSym = pOp->Symbol[0];
                tokener->OpPrecedence = pOp->Precedence[0];
            }
            else
            {
                int k;
                for (k = 0; k <= pOp->Magic; k++)
                {
                    if (pOp->OperandL[k] <= OperandL)
                    {
                        tokener->OpSym = pOp->Symbol[k];
                        tokener->OpPrecedence = pOp->Precedence[k];
                        break;
                    }
                }
            }

            //
            if (tokener->OpSym)
            {
                tokener->Token = MString_Copy(tokener->CurLine, tokener->PosX, len);
                tokener->PosX += len;            
                tokener->OpMagic = pOp->Magic;
                tokener->lGrouping = !pOp->rGrouping;
            }
            
            
        }
    }
    else 
        return -1;

    return 0;
}

static int Toke_NextMemory0(MTokener *tokener, const MInt OperandL, const bool bSkipBlank)
{    
    int pos = tokener->PosX;
    int r = Toke_NextMemory1(tokener, OperandL, bSkipBlank);

    // force move to next character if something wrong happened.
    if (r != 0)
    {
        if (pos == tokener->PosX)
            tokener->PosX++;
    }
    else if (OP_NEXT_LINE == tokener->OpSym)
    {
        tokener->PosX++;
        if (tokener->PosX <= MString_Len(tokener->CurLine))
            r = Toke_NextMemory1(tokener, OperandL, bSkipBlank);
        else;
    }

    return r;
}

#define report_with_comment(l, t, r)    \
    if (l == 0) \
    {   \
        return r; \
    } \
    else    \
    {   \
        if (t->Token) MString_Release(t->Token); \
        t->Token = NULL;\
        t->TokenType = ttUnkown; \
        t->OpSym = NULL;\
        return -1;\
    }

static int Token_NextFile(MTokener *tokener, const MInt OperandL, const bool bSkipBlank)
{
    int CommentLevel = 0;
    int r;
    do 
    {
again:
        r = Toke_NextMemory0(tokener, OperandL, bSkipBlank);
        if (r != 0) break;

        if ((OP_NEXT_LINE == tokener->OpSym) || 
            ((tokener->Token == NULL) && (tokener->PosX > MString_Len(tokener->CurLine))))
        {
            if (!MIO_IsEOF(&tokener->file))
            {
                MString_Release(tokener->CurLine);
                tokener->CurLine = MIO_ReadLine(&tokener->file);
                tokener->PosY++;
                tokener->PosX = 1;
                goto again;
            }
            else
                break;
        }
        else if (OP_COMMENT_OPEN == tokener->OpSym)
        {
            CommentLevel++;
        }
        else if (OP_COMMENT_CLOSE == tokener->OpSym)
        {
            CommentLevel--;
            goto again;
        }
        else;
        
    } while (CommentLevel > 0);
    report_with_comment(CommentLevel, tokener, r);
}

static int Token_NextMemory(MTokener *tokener, const MInt OperandL, const bool bSkipBlank)
{
    int CommentLevel = 0;
    int r;
    do 
    {
        r = Toke_NextMemory0(tokener, OperandL, bSkipBlank);
        if (OP_COMMENT_OPEN == tokener->OpSym)
        {
            CommentLevel++;
        }
        else if (OP_COMMENT_CLOSE == tokener->OpSym)
        {
            CommentLevel--;
        }
    }
    while ((r == 0) && (tokener->PosX > MString_Len(tokener->CurLine)) && (CommentLevel > 0));
    report_with_comment(CommentLevel, tokener, r);
}

// fix "]]" to "]" if (Prop & TOKEN_SEQ_OPEN)
void Token_Fixup(MTokener *tokener, const MDword Prop)
{
    if ((Prop & TOKEN_SEQ_OPEN) && (tokener->OpSym == sym_Part) 
        && (MString_GetAt(tokener->Token, 1) == ']'))
    {
        MString_Release(tokener->Token);
        tokener->PosX--;
        tokener->Token = MString_Copy(tokener->CurLine, tokener->PosX, 1);
        tokener->OpMagic = 0;
        tokener->OpSym = sym_Sequence;
        tokener->lGrouping = true;
    }
}

int Token_Next(MTokener *tokener, const MInt OperandL)
{
    if (tokener->ForgiveMe > 0)
    {
        tokener->ForgiveMe--;
        return 0;
    }

    tokener->ForgiveMe = 0;
    tokener->TokenType = ttUnkown;
    if (tsFile == tokener->Source)
        return Token_NextFile(tokener, OperandL, true);
    else
        return Token_NextMemory(tokener, OperandL, true);
}

int Token_NextCont(MTokener *tokener, const MInt OperandL)
{
    if (tokener->ForgiveMe > 0)
    {
        tokener->ForgiveMe--;
        return 0;
    }

    tokener->ForgiveMe = 0;
    tokener->TokenType = ttUnkown;
    if (tsFile == tokener->Source)
        return Token_NextFile(tokener, OperandL, false);
    else
        return Token_NextMemory(tokener, OperandL, false);
}

int Token_ForgiveMe(MTokener *tokener)
{
    if (tokener->Token)
        return ++tokener->ForgiveMe;
    else
        return 0;
}

static MHash HashOpItem(const MOpHashItem *s)
{
    MInt i;
    const MChar *pData = MString_GetData(s->Op);
    MHash r = 0;
    for (i = 0; i < MString_Len(s->Op); i++)
    {
        r <<= 8;
        r |= *pData++;
    }
    return r;
}

static MInt CmpHashSymbol(const MOpHashItem *s1, const MOpHashItem *s2)
{
    return MString_Compare(s1->Op, s2->Op);
}

static void MapOnOp(MHashTable *OpSeqTable, MOpHashItem *Item)
{
    int j;
    for (j = 1; j <= MString_Len(Item->Op); j++)
    {
        MString s = MString_Copy(Item->Op, 1, j);
        if (s != MHashTable_FindOrInsert(OpSeqTable, s))
            MString_Release(s);
    }
}

void Token_Init(void)
{
    extern MDword CharPropties[0x10000];
    unsigned int i;
    OpHashTable = MHashTable_Create(arr_size(OpSymMap) << 3, 
                                    (f_Hash1)HashOpItem, NULL, 
                                    (f_HashCompare)CmpHashSymbol);
    
    for (i = 0; i < arr_size(OpSymMap); i++)
    {
        MOpHashItem *Item = (MOpHashItem *)YM_malloc(sizeof(MOpHashItem));
        MOpHashItem *r = NULL;
        Item->Op = ASCIIToMString((MByte *)(OpSymMap[i].Op), strlen(OpSymMap[i].Op));
        r = (MOpHashItem *)MHashTable_FindOrInsert(OpHashTable, Item);
        
        if (Item != r)
        {
            r->Magic++;
            YM_free(Item);
        }
        
        r->rGrouping = OpSymMap[i].rGrouping;
        r->Precedence[r->Magic] = OpSymMap[i].Precedence;
        r->Symbol[r->Magic] = *(OpSymMap[i].Sym);
        r->OperandL[r->Magic] = OpSymMap[i].OperandL;

        CharPropties[MString_GetAt(r->Op, 1)] = OP_CHAR;
    }

    OpSequenceHashTable = MHashTable_CreateString(arr_size(OpSymMap) << 4);
    MHashTable_Traverse(OpHashTable, (f_HashTraverse)(MapOnOp), OpSequenceHashTable);
} 

int Token_FillMessagePara(MTokener *tokener, MSequence *ps, const MInt Start, const MInt Len)
{
    int r = 0;
    ASSERT(Start + Len <= MSequence_Len(ps));
    if (r < Len)
    {
        MSequence_SetAtX(ps, Start + r, MExpr_CreateMachineInt(tokener->PosY));
        r++;
    }
    if (r < Len)
    {
        MSequence_SetAtX(ps, Start + r, MExpr_CreateMachineInt(tokener->PosX));
        r++;
    }
    if (r < Len)
    {
        MSequence_SetAtX(ps, Start + r, MExpr_CreateString(tokener->CurLine));
        r++;
    }
    if (r < Len)
    {
        if (tokener->Source == tsFile)
            MSequence_SetAtX(ps, Start + r, MExpr_CreateString(tokener->FileName));
        else
            MSequence_SetAtX(ps, Start + r, MExpr_CreateStringX(MString_NewC("<<Memory>>")));
        r++;
    }
    return r;
}
