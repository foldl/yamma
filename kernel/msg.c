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

#include <wchar.h>
#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include "msg.h"


void MMsg_Trace(const char *format, ...)
{
    va_list marker;
    va_start(marker, format);
    vfprintf(stdout, format, marker);
    va_end(marker);
}

void MMsg_TraceLn(const char *format, ...)
{
    va_list marker;
    va_start(marker, format);
    vfprintf(stdout, format, marker);
    printf("\n");
    va_end(marker);
}

void Msg_Emit(const char *format, ...)
{
    va_list marker;
    va_start(marker, format);
    fprintf(stdout, format, marker);
    va_end(marker);
}

void Msg_EmitLn(const char *format, ...)
{
    va_list marker;
    va_start(marker, format);
    fprintf(stdout, format, marker);
    fprintf(stdout, "\n");
    va_end(marker);
}

void Msg_EmitLn(const MString s)
{
    wchar_t *str = (wchar_t *)YM_malloc((MString_Len(s) << 1) + 2);
    memcpy(str, MString_GetData(s), MString_Len(s) << 1); 
    fwprintf(stdout, str);
    YM_free(str);
    printf("\n");
}

void Msg_Emit(const MString s)
{
    wchar_t *str = (wchar_t *)YM_malloc((MString_Len(s) << 1) + 2);
    memcpy(str, MString_GetData(s), MString_Len(s) << 1); 
    fwprintf(stdout, str);
    YM_free(str);
}
