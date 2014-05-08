#ifndef mstr_h
#define mstr_h

#include <stdio.h>

#include "mtype.h"

#define SMALL_STR_MAX_LEN   (125)

struct MTagString;

typedef MTagString * MString;

struct MTagString
{
    MInt Capacity;
    MString Holder;
    MInt Len;
    MChar *pData;
};

#ifndef stricmp
#define stricmp strcasecmp
#endif

typedef MTagString * MString;

MString MString_NewC(const char *s);
MString MString_NewCN(const char *s, const int Len);
MString MString_NewM(const MChar *s);
MString MString_NewMN(const MChar *s, const int Len);
MString MString_NewS(const MString s);
MString MString_NewWithCapacity(const int Capacity);
MString MString_Unique(const MString s);
#define MString_Release XRef_DecRef

MString MString_CopyCN(MString s, const char *cs, const int len);

MInt    MString_Pos(const MString s, const MString sub, const int Start = 1);
MString MString_Delete(const MString s, const int Len);
MString MString_Copy(const MString s, const int Start, const int Len);
MString MString_Join(const MString s, const MString cat);
MInt    MString_Compare(const MString s1, const MString s2);
MBool   MString_SameQ(const MString s1, const MString s2);
MString MString_SetLen(MString s1, const int Len);
MString MString_Reverse(MString s);

// s will be modified, so RefCount should be 0, or else, something strange will happen
// MString is always not changable, except for this function
MString MString_Cat(MString s, const MString cat);
MString MString_CatChar(MString s, const MChar cat);
MString MString_CatCN(MString s, const char *cz, const int n);

#define MString_Len(s)  ((s)->Len)
#define MString_Cap(s)  ((s)->Capacity)
#define MString_GetAt(s, i)  ((s)->pData[i - 1])

int MString_Dump(MString s, MChar *t, const int size);
const MChar * MString_GetData(const MString s);

void MString_Init(void);
void MString_DumpHeap(FILE *file);

#endif
