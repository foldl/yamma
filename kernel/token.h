#ifndef token_h
#define token_h

#include <stdio.h>

#include "mtype.h"
#include "eval.h"
#include "moi.h"
#include "sym.h"

#define MAX_TOKEN_LEN   (1024)

#define OP_MAGIC_MAX (4)

#define MAX_PRECEDENCE (1000)
#define PRECE_MONO_PLUS_SUB 30
#define PRECE_TIMES         31

enum MTokenType
{
    ttSymbol,
    ttNumber,
    ttString,
    ttOp,
    ttComma,
    ttLeftParenthesis,
    ttRightParenthesis,
    ttLeftBrace,
    ttRightBrace,

    ttUnkown
};

enum MTokenSource
{
    tsMemory,
    tsFile
};

struct MTokener
{
    MString Token;
    MTokenType  TokenType;
    int PosX;
    int PosY;

    MInt    OpMagic;
    MSymbol OpSym;
    MInt    OpPrecedence;
    MBool   lGrouping;
    
    // private fields
    MTokenSource Source;
    MFile file;
    MString CurLine;
    MString FileName;
    int ForgiveMe;
};

#define TOKEN_PART_OPEN 0x00000001
#define TOKEN_SEQ_OPEN  0x00000002

MTokener *Token_NewFromMemory(const char *str, MString encoding = NULL);
MTokener *Token_NewFromMemory(MString str);
MTokener *Token_NewFromFile(const char *fn, MString encoding = NULL);
MTokener *Token_NewFromFile(const MString fn, MString encoding = NULL);
void      Token_Delete(MTokener *tokener);

int  Token_Next(MTokener *tokener, const MInt OperandL);
int  Token_NextCont(MTokener *tokener, const MInt OperandL);
void Token_Fixup(MTokener *tokener, const MDword Prop);
int  Token_ForgiveMe(MTokener *tokener);

void Token_Init(void);

int Token_FillMessagePara(MTokener *tokener, MSequence *ps, const MInt Start, const MInt Len);

#endif
