
#ifndef moi_h
#define moi_h

#include <stdio.h>
#include "mtype.h"
#include "mstr.h"

struct MFile
{
    FILE *fp;
    MString encoding;
    
    int BufferSize;
    int Cursor;
    int Len;
    MByte *Buffer;
};

MInt MIO_OpenFile(MFile *f, const char *fn, MString encoding = NULL);
MInt MIO_OpenFile(MFile *f, const MString fn, MString encoding = NULL);
void MIO_CloseFile(MFile *f);

bool MIO_IsEOF(MFile *f);
MString MIO_ReadLine(MFile *f);

#endif
