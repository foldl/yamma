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

#include "encoding.h"
#include "moi.h"
#include "mm.h"


#define BUFF_SIZE (10 * 1024) //   

MInt MIO_OpenFile(MFile *f, const MString fn, MString encoding)
{
    int size = (MString_Len(fn) + 2) * sizeof(MChar);
    char *ch = (char *)YM_malloc(MString_Len(fn) * sizeof(MChar));
    int r = 0;
    if (MStringToASCII(fn, (MByte *)ch, size) < size)
        r = MIO_OpenFile(f, ch, encoding);
    return r;
}

MInt MIO_OpenFile(MFile *f, const char *fn, MString encoding)
{
    f->fp = fopen(fn, "r");
    if (f->fp)
    {
        f->encoding = encoding ? MString_NewS(encoding) : NULL;
        f->Buffer = (MByte *)YM_malloc(BUFF_SIZE);
        f->BufferSize = BUFF_SIZE;
        f->Len = 0;
        f->Cursor = 0;

        return 1;
    }
    else
        return 0;
}

void MIO_CloseFile(MFile *f)
{
    if (f->fp)
    {
        fclose(f->fp);
        f->fp = NULL;
        if (f->encoding)
            MString_Release(f->encoding);
        f->encoding = NULL;
        YM_free(f->Buffer);
    }
}

bool MIO_IsEOF(MFile *f)
{
    if (f->Len > f->Cursor)
        return false;
    else
        return feof(f->fp) ? true : false;
}

MString MIO_ReadLine(MFile *f)
{
    int Cursor = f->Cursor;
    int Start = Cursor;
    MString r;
    if (f->Len <= 0)
        f->Len = fread(f->Buffer, 1, f->BufferSize, f->fp);
    if (f->Len <= 0)
        return MString_NewC("");

//    while ((start < f->Len) && ((f->Buffer[start] == '\n') || (f->Buffer[start] == '\r')))
//        start++;
//    if (start >= f->Len)
//    {
//        f->Len = 0;
//        goto read_block;
//    }
//    
//    f->Len -= start;
//    memcpy(f->Buffer, f->Buffer + start, f->Len);
//    start = 0;

continue_search:
    while ((Cursor < f->Len) && (f->Buffer[Cursor] != '\n') && (f->Buffer[Cursor] != '\r'))
        Cursor++;

    if ((Cursor == f->Len) && (!feof(f->fp)))
    {
        if (f->BufferSize - f->Len < 10)
        {
            f->BufferSize += BUFF_SIZE;
            f->Buffer = (MByte *)YM_realloc(f->Buffer, f->BufferSize);
        }
        
        f->Len += fread(f->Buffer + f->Len, 1, f->BufferSize - f->Len, f->fp);
        goto continue_search;
    }

    r = f->encoding ? EncodingToMString(f->Buffer + Start, Cursor - Start, f->encoding) :
                      LocaleToMString(f->Buffer + Start, Cursor - Start);

    f->Len -= Cursor;
    memcpy(f->Buffer, f->Buffer + Cursor, f->Len);
    Cursor = 0;

    // skip line feed
    if (f->Len < 2)
        f->Len += fread(f->Buffer + f->Len, 1, f->BufferSize - f->Len, f->fp);
    
    if (f->Len > 0)
    {
        if (f->Buffer[Cursor] == '\r')
        {
            Cursor++;
            if ((f->Len > 1) && (f->Buffer[Cursor] == '\n'))
                Cursor++;
        }
        else
        if (f->Buffer[Cursor] == '\n')
                Cursor++;
    }

    f->Cursor = Cursor;
    return r;
}
