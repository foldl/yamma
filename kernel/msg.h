#ifndef msg_h
#define msg_h

#include "mtype.h"
#include "expr.h"


void MMsg_Abort(void);

//#ifdef _DEBUG
//void MMsg_Trace(const char *format, ...);
//#else
//#define MMsg_Trace
//#endif

void Msg_Emit(const char *format, ...);
void Msg_EmitLn(const char *format, ...);
void Msg_Emit(const MString s);
void Msg_EmitLn(const MString s);

#endif
