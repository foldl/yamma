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

#include "sym.h"
#include "hash.h"
#include "mm.h"
#include "bif.h"
#include "encoding.h"

//static MSymbol _Context = NULL;
//static MSymbol _ContextPath = NULL;

#define CHAR_CONTEXT_DELI   '`'

#define _Context        sym_DollarContext 
#define _ContextPath    sym_DollarContextPath

static MHashTable *ContextTable = NULL;
static MHeap       Heap_MSym = NULL;

struct MContext
{
    MString Name;
    MHashTable *SymbolTable;
};

static MContext * _ContextSystem = NULL;
static MContext * _ContextGlobal = NULL;
static MXRefObjType Type_MSymbol = NULL;

#include "sys_sym_dec.inc"
#include "sys_msg_dec.inc"

#define create_sys(name, att)       \
        _MSymbol_Create(_ContextSystem, MString_NewC(name), att)

#define create_sys2(name, att, f) \
        _MSymbol_Create(_ContextSystem, MString_NewC(name), att, f)

MSymbol _MSymbol_Create(MContext *context, MString name, MAttribute attr, f_InternalApply f = NULL);

static MHash HashSymbol(const MSymbol s)
{
    return ElfHash(MString_GetData(s->Name), MString_Len(s->Name));
}

static MInt CmpHashSymbol(const MSymbol s1, const MSymbol s2)
{
    return MString_Compare(s1->Name, s2->Name);
}

MContext *MSymbol_RegisterContext(MString name)
{
    MContext *c = (MContext *)MHeap_malloc(Heap_MSym, sizeof(MContext));
    c->Name = MString_NewS(name);
    MContext * r = (MContext *)MHashTable_FindOrInsert(ContextTable, c);
    if (r != c)
    {
        MString_Release(c->Name);
        MHeap_free(Heap_MSym, c);
    }
    else
        c->SymbolTable = MHashTable_Create(100000, (f_Hash1)HashSymbol, NULL, (f_HashCompare)CmpHashSymbol);
    return r;
}

MContext *MSymbol_RegisterContext(const char *name)
{
    return MSymbol_RegisterContext(MString_NewC(name));
}

#define MSymbol_GetIn(name)                             \
    c = MSymbol_RegisterContext(name);                  \
    p = (MSymbol)MHashTable_Find(c->SymbolTable, &t)

MSymbol MSymbol_GetOrNewSymbol(MString name, const int pos)
{
    MTagSymbol t;
    MSymbol p = NULL;
    MContext *c = NULL;
    MString context;

    t.Name = MString_Delete(name, pos);

    if (MString_GetAt(name, 1) == CHAR_CONTEXT_DELI)
    {
        MString t = MString_Copy(name, 2, pos - 1);
        context = MString_Join(_Context->Immediate->Str, t);
        MString_Release(t);
    }
    else
        context = MString_Copy(name, 1, pos);

    MSymbol_GetIn(context);
    if (!p)
        p = _MSymbol_Create(c, MString_NewS(t.Name), 0);

    MString_Release(t.Name);
    MString_Release(context);
    
    return p;
}

MSymbol MSymbol_GetOrNewSymbol(MString name)
{
    MTagSymbol t;
    MSymbol p = NULL;
    MContext *c = NULL;
    MContext *cur;
    int i;

    t.Name = name;

    if (MString_Len(name) < 1)
        return NULL;

    for (i = MString_Len(name); i >= 1; i--)
    {
        if (MString_GetAt(name, i) == CHAR_CONTEXT_DELI)
            return MSymbol_GetOrNewSymbol(name, i);
    }

    MSymbol_GetIn(_Context->Immediate->Str);
    if (p)
        return p;
    cur = c;

    if ((_ContextPath->Immediate) && (_ContextPath->Immediate->Type == etHeadSeq)
        && (_ContextPath->Immediate->HeadSeq.Head->Symbol == sym_List))
    {
        MSequence *ps = _ContextPath->Immediate->HeadSeq.pSequence;
        int n;
        for (n = 0; n < ps->Len; n++)
        {
            MSymbol_GetIn(ps->pExpr[n]->Str);
            if (p)
                return p;
        }
    }
    
    return _MSymbol_Create(cur, MString_NewS(name), 0);
}

void _MContext_Push(MString context)
{
}

void _MContext_Pop(void)
{
}

static MHash HashContext(const MContext *p)
{
    return ElfHash(MString_GetData(p->Name), MString_Len(p->Name), 0);
}

static MInt CmpContext(const MContext *s1, const MContext *s2)
{
    return MString_Compare(s1->Name, s2->Name);
}

static MSymbol MSymbol_Dispose(MSymbol s)
{
    XRef_DecRef(s->Context);
    XRef_DecRef(s->Name);
    // TODO: dispose other definitions
    return NULL;
}

MSymbol _MSymbol_Create(MContext *context, MString name, MAttribute attr, f_InternalApply f)
{
    MSymbol s = MXRefObj_Create(MTagSymbol, Type_MSymbol);
    s->Context = context;
    s->Name = name;
    s->Attrib = attr;
    s->InternalApply = f;
    MHashTable_Insert(context->SymbolTable, s);

    if (msg_Generalnewsym)
    {
        MSequence *p = MSequence_Create(1);
        MSequence_SetAtX(p, 0, MExpr_CreateSymbol(s));
        BIF_Message(sym_General, msg_Generalnewsym, p, NULL);
        MSequence_Release(p);
    }
    
    return s;
}

void MSymbol_Release(MSymbol s)
{
    if (MObj_RefCnt(s) >= 1)
        XRef_DecRef(s);
    else if (s->Attrib & aTemporary)
        ASSERT("remove of MSymbol not implemented!" == NULL);
}

static char *MSymbol_DumpObj(MSymbol sym, char *s, const MInt maxlen)
{
    MString str = sym->Name;
    s[maxlen - 1] = '\0';
    MStringToASCII(str, (MByte *)s, maxlen - 2);
    return s;
}

static MXRefObjMT MSymbol_MT = {NULL, (f_YM_XRefObjDispose)MSymbol_Dispose, NULL}; //(f_YM_XRefObjDump)MSymbol_DumpObj

void MSymbol_Init()
{
    Heap_MSym = MHeap_NewHeap("MSym", 100000);

    ContextTable = MHashTable_Create(200, (f_Hash1)HashContext, NULL, (f_HashCompare)CmpContext);
    Type_MSymbol = MXRefObj_Register(&MSymbol_MT, sizeof(MTagSymbol), "MSymbol");

    _ContextSystem = MSymbol_RegisterContext("System`");
    _ContextGlobal = MSymbol_RegisterContext("Global`");

    #include "sys_sym_def.inc"
    #include "sys_msg_def.inc"

    // _ContextPath
    {
        MExprList *p = MExprList_Create();
        MExpr e = MExpr_CreateString(_ContextGlobal->Name);
        MExprList_Append(p, e);
        MExpr_Release(e);
        e = MExpr_CreateString(_ContextSystem->Name);
        MExprList_Append(p, e);
        MExpr_Release(e);

        MSequence *m = MSequence_FromExprList(p);
        MExpr r = MExpr_CreateHeadSeq(sym_List, m);
        MSequence_Release(m);
        MSymbol_SetImm(_ContextPath, r);
        MExpr_Release(r);
        MExprList_Release(p);
    }
    
    // _Context
    MExpr e = MExpr_CreateString(_ContextGlobal->Name);
    MSymbol_SetImm(_Context, e);
    MExpr_Release(e);

    sym_I->Immediate = MExpr_CreateComplexUnit();

    sym_DollarIterationLimit->Immediate = MExpr_CreateMachineInt(4096);
    sym_DollarRecursionLimit->Immediate = MExpr_CreateMachineInt(256);
}

MExpr MSymbol_AddMsg(MSymbol s, MString name, MString lang, MString msg)
{
    MSequence *seq = MSequence_Create(2);
    MExpr r = NULL;
    if (MString_Len(lang) > 0)
        MSequence_SetAtX(seq, 0, MExpr_CreateStringX(MString_Join(name, lang)));
    else
        MSequence_SetAtX(seq, 0, MExpr_CreateString(name));
    MSequence_SetAtX(seq, 1, MExpr_CreateString(msg));
    r = MExpr_CreateHeadSeqX(sym_RuleDelayed, seq);
    if (s->Messages == NULL)
        s->Messages = MExprList_Create();
    MExprList_PushOrphan(s->Messages, r);
    return r;
}

MExpr MSymbol_AddMsg(MSymbol s, const char *name, const char * msg, const bool bOff)
{
    MSequence *seq = MSequence_Create(2);
    MExpr r = NULL;
    MSequence_SetAtX(seq, 0, MExpr_CreateStringX(MString_NewC(name)));
    if (!bOff)
        MSequence_SetAtX(seq, 1, MExpr_CreateStringX(MString_NewC(msg)));
    else
    {
        MSequence *p = MSequence_Create(1);
        MSequence_SetAtX(p, 0, MExpr_CreateStringX(MString_NewC(msg)));
        MSequence_SetAtX(seq, 1, MExpr_CreateHeadSeqX(sym_DollarOff, p));
    }
    r = MExpr_CreateHeadSeqX(sym_RuleDelayed, seq);
    if (s->Messages == NULL)
        s->Messages = MExprList_Create();
    MExprList_PushOrphan(s->Messages, r);
    return r;
}

MExpr MSymbol_QueryMsg(MSymbol s, MString name, MString lang)
{
    if (s->Messages == NULL)
        return NULL;
    MString namelang = MString_Len(lang) > 0 ? MString_Join(name, lang) : MString_NewS(name);
    MExprListNode *n = s->Messages->Last;
    while ((n != &s->Messages->Head) && !MString_SameQ(n->Expr->HeadSeq.pSequence->pExpr[0]->Str, namelang))
        n = n->prev;
    MString_Release(namelang);
    return n != &s->Messages->Head ? n->Expr->HeadSeq.pSequence->pExpr[1] : NULL;
}

MExpr MSymbol_QueryMsg(MExpr expr)
{
    return expr->Type == etHeadSeq ? expr->HeadSeq.pSequence->pExpr[1] : NULL;
}

MSymbol MSymbol_SetImm(MSymbol s, MExpr expr)
{
    if (s->Immediate)
        XRef_DecRef(s->Immediate);
    s->Immediate = expr;
    XRef_IncRef(expr);
    return s;
}

MString MSymbol_Context(MSymbol s)
{
    return ((MContext *)s->Context)->Name;
}

struct PropMap
{
    MAttribute att;
    MSymbol *sym;
};

static PropMap PropMap[] = 
{
    {aConstant       , &sym_Constant       },
    {aFlat           , &sym_Flat           },
    {aHoldAll        , &sym_HoldAll        },
    {aHoldAllComplete, &sym_HoldAllComplete},
    {aHoldFirst      , &sym_HoldFirst      },
    {aHoldRest       , &sym_HoldRest       },
    {aListable       , &sym_Listable       },
    {aLocked         , &sym_Locked         },
    {aNHoldAll       , &sym_NHoldAll       },
    {aNHoldFirst     , &sym_NHoldFirst     },
    {aNHoldRest      , &sym_NHoldRest      },
    {aNumericFunction, &sym_NumericFunction},
    {aOneIdentity    , &sym_OneIdentity    },
    {aOrderless      , &sym_Orderless      },
    {aProtected      , &sym_Protected      },
    {aReadProtected  , &sym_ReadProtected  },
    {aSequenceHold   , &sym_SequenceHold   },
    {aStub           , &sym_Stub           },
    {aTemporary      , &sym_Temporary      }
};

//MDword MSymbol_AttributeWord(MSymbol s)
//{
//    return s->Attrib;
//}

static MAttribute ExprAttribToMAttrib(MExpr Atts)
{
    MDword i;
    int j;
    MSequence *ps = Atts->HeadSeq.pSequence;
    ASSERT(Atts->Type == etHeadSeq);

    MAttribute r = 0;
    for (j = 0; j < MSequence_Len(ps); j++)
    {
        MSymbol m = MSequence_EleAt(ps, j)->Symbol;
        ASSERT(MSequence_EleAt(ps, j)->Type == etSymbol);

        for (i = 0; i < arr_size(PropMap); i++)
        {
            if (MString_SameQ(m->Name, (*PropMap[i].sym)->Name))
                r |= PropMap[i].att;
        }
    }
    
    return r;
}

static MExpr MAttribToExprAttrib(MAttribute a)
{
    unsigned int i;
    MExprList *p = MExprList_Create();
    for (i = 0; i < arr_size(PropMap); i++)
    {
        if (a & PropMap[i].att)
        {
            MExpr e = MExpr_CreateSymbol(*(PropMap[i].sym));
            MExprList_PushOrphan(p, e);
        }  
    }
    
    MExpr r = MExpr_CreateHeadSeqFromExprList(sym_List, p);
    MExprList_Release(p);
    return r;
}

MExpr MSymbol_Attributes(MSymbol s)
{
    return MAttribToExprAttrib(s->Attrib);
}

bool MSymbol_SetAttributes(MSymbol s, MExpr Atts)
{
    if (s->Attrib & aLocked)
        return false;
    s->Attrib = ExprAttribToMAttrib(Atts);
    return true;
}

bool MSymbol_AddAttributes(MSymbol s, MExpr Atts)
{
    if (s->Attrib & aLocked)
        return false;
    s->Attrib |= ExprAttribToMAttrib(Atts);
    return true;
}

bool MSymbol_ClearAttributes(MSymbol s, MExpr Atts)
{
    if (s->Attrib & aLocked)
        return false;
    s->Attrib &= ~ExprAttribToMAttrib(Atts);
    return true;
}

MExprListIter MSymbol_FindDef(MExprList *Defaults, MExpr DefaultExpr)
{
    MExprListIter Iter = MExprList_GetIter(Defaults);
    MExprListIter r[2] = {NULL};
    MExpr expr;
    int i;

    while ((expr = MExprListIter_Get(Iter)) != NULL)
    {
        MExpr pat  = MSequence_EleAt(expr->HeadSeq.pSequence, 0);
        if (IsExprSameQ(pat, DefaultExpr))
            return Iter;

        if ((NULL == r) 
                && (MSequence_Len(pat->HeadSeq.pSequence) <= MSequence_Len(DefaultExpr->HeadSeq.pSequence)))
        {
            int i;
            for (i = 1; i < MSequence_Len(pat->HeadSeq.pSequence); i++)
            {
                if (!IsExprSameQ(get_seq_i(pat, i), get_seq_i(DefaultExpr, i)))
                    break;
            }

            if (i >= MSequence_Len(pat->HeadSeq.pSequence))
                r[MSequence_Len(pat->HeadSeq.pSequence) - 1] = Iter;
        }

        Iter = MExprListIter_Prev(Iter);
    }
    
    for (i = sizeof(r) / sizeof(r[0]) - 1; i >= 0; i++)
        if (r[i]) return r[i];
        
    return NULL;
}

MExpr MSymbol_GetDefaultValue(MSymbol s, MInt Index, MInt N)
{
    MSequence *seq = MSequence_Create(3);
    MExprListIter Iter;
    MExpr DefExpr;
    MSequence_SetAtX(seq, 0, MExpr_CreateSymbol(s));
    MSequence_SetAtX(seq, 1, MExpr_CreateMachineInt(Index));
    MSequence_SetAtX(seq, 2, MExpr_CreateMachineInt(N));
    DefExpr = MExpr_CreateHeadSeqX(sym_Default, seq);
    Iter = s->DefaultValues ? MSymbol_FindDef(s->DefaultValues, DefExpr) : NULL;
    MExpr_Release(DefExpr);
    return Iter ? get_seq_i(MExprListIter_Get(Iter), 1) : NULL;
}

void  MSymbol_SetDefaultValue(MSymbol s, MExpr DefaultExpr, MExpr V)
{
    MExprListIter Iter;
    MSequence *seq = MSequence_Create(2);
    MExpr defRule;
    MSequence_SetAt(seq, 0, DefaultExpr);
    MSequence_SetAt(seq, 1, V);
    defRule = MExpr_CreateHeadSeqX(sym_Rule, seq);

    if (NULL == s->DefaultValues)
        s->DefaultValues = MExprList_Create();
    Iter = MSymbol_FindDef(s->DefaultValues, DefaultExpr);
    if (Iter)
    {
        if (IsExprSameQ(get_seq_i(MExprListIter_Get(Iter), 0), DefaultExpr))
        {
            if (!IsExprSameQ(get_seq_i(MExprListIter_Get(Iter), 1), V))
                MExprListIter_SetX(Iter, defRule); 
            else
                MExpr_Release(defRule);
        }
        else
            MExprList_PushOrphan(s->DefaultValues, defRule);
    }
    else
        MExprList_PushOrphan(s->DefaultValues, defRule);
}
