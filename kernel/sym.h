
#ifndef sym_h
#define sym_h

#include "mtype.h"
#include "mstr.h"
#include "expr.h"

#define MSymbol_HasAttr(s, a) (((s->Attrib) & (a)) != 0)

void MSymbol_Init(void);

MSymbol MSymbol_GetOrNewSymbol(MString name);
void MSymbol_Release(MSymbol s);

//MSymbol _MSymbol_Create(MHashTable *SymbolTable, MString name, MAttribute attr);
MExpr MSymbol_Attributes(MSymbol s);
#define MSymbol_AttributeWord(s) ((s)->Attrib)
bool MSymbol_AddAttributes(MSymbol s, MExpr Atts);
bool MSymbol_ClearAttributes(MSymbol s, MExpr Atts);

void _MContext_Push(MString context);
void _MContext_Pop(void);

MSymbol MSymbol_SetImm(MSymbol s, MExpr expr);
MString MSymbol_Context(MSymbol s);

MExpr MSymbol_AddMsg  (MSymbol s, MString name, MString lang, MString msg);
MExpr MSymbol_AddMsg(MSymbol s, const char *name, const char * msg, const bool bOff = false);
MExpr MSymbol_QueryMsg(MSymbol s, MString name, MString lang);
MExpr MSymbol_QueryMsg(MExpr expr);

MExpr MSymbol_GetDefaultValue(MSymbol s, MInt Index, MInt N);
void  MSymbol_SetDefaultValue(MSymbol s, MExpr DefaultExpr, MExpr V);

#endif
