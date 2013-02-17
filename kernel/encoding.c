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

#include "encoding.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "hash.h"
#include "mm.h"

struct TUnicodeCharInfo
{
    MString name;
};

static TUnicodeCharInfo UnicodeCharTab[0xFFFF] = {{NULL}};
MDword CharPropties[0x10000] = {0};

static MHashTable *NamedCharToCode;
static MHashTable *CharEncodingTable;

struct MStrCodeMap
{
    MString name;
    MChar   Code;
};

struct MCharEncodingMap
{
    MString name;
    MChar UnicodeToLocale[0xFFFF + 1];
    MChar LocaleToUnicode[0xFFFF + 1];
};

#include "sys_char_dec.inc"

// say hello to the external world
static MChar _UnicodeToLocale[0xFFFF + 1] = {0};
static MChar _LocaleToUnicode[0xFFFF + 1] = {0};
static MString LocaleEncodingName = NULL;
static MString LocaleEncodingASCII;
static MString LocaleEncodingUnicode; // U16
static MString LocaleEncodingUTF8;

int HexCharToInt[128] = {0};
MChar IntToHexChar[128] = {'0'};

static MHash HashCodeMap(const void *p)
{
    MStrCodeMap *map = (MStrCodeMap *)p;
    return ElfHash(MString_GetData(map->name), MString_Len(map->name));
}

static MHash HashCodeMap2(const void *p, const MHash Hash1)
{
    MStrCodeMap *map = (MStrCodeMap *)p;
    return ElfHash2(MString_GetData(map->name), MString_Len(map->name));
}

static MInt CmpCodeMap(const MStrCodeMap *m1, const MStrCodeMap *m2)
{
    return MString_Compare(m1->name, m2->name);
}

static MHash HashEncodingMap(const void *p)
{
    MCharEncodingMap *map = (MCharEncodingMap *)p;
    return ElfHash(MString_GetData(map->name), MString_Len(map->name));
}

static MHash HashEncodingMap2(const void *p, const MHash Hash1)
{
    MCharEncodingMap *map = (MCharEncodingMap *)p;
    return ElfHash2(MString_GetData(map->name), MString_Len(map->name));
}

static MInt CmpHashEncodingMap(const MCharEncodingMap *m1, const MCharEncodingMap *m2)
{
    return MString_Compare(m1->name, m2->name);
}

void EncodingInit(void)
{
    MCharEncodingMap *map;
    int i;
    for (i = '0'; i <= '9'; i++)
    {
        CharPropties[i] = NUM_CHAR;
        HexCharToInt[i] = i - '0';    
    }
    
    for (i = 'a'; i <= 'z'; i++)
        HexCharToInt[i] = i - 'a' + 0xA;    
    for (i = 'A'; i <= 'Z'; i++)
        HexCharToInt[i] = i - 'A' + 0xA;    

    for (i = 0; i <= 9; i++)
        IntToHexChar[i] = i + '0';
    for (i = 10; i <= 35; i++)
        IntToHexChar[i] = i - 10 + 'A';

    LocaleEncodingASCII   = MString_NewC("7Bit");
    LocaleEncodingUnicode = MString_NewC("16Bit"); 
    LocaleEncodingUTF8    = MString_NewC("UTF-8");
    LocaleEncodingName    = LocaleEncodingASCII;

    NamedCharToCode = MHashTable_Create(1500, HashCodeMap, HashCodeMap2, (f_HashCompare)CmpCodeMap);
    CharEncodingTable = MHashTable_Create(10, HashEncodingMap, HashEncodingMap2, (f_HashCompare)CmpHashEncodingMap);

#define insert_encoding(en)   \
    map = (MCharEncodingMap *)YM_malloc(sizeof(MCharEncodingMap));  \
    map->name = en;                                                 \
    MHashTable_Insert(CharEncodingTable, map);

    insert_encoding(LocaleEncodingASCII);
    insert_encoding(LocaleEncodingUnicode);
    insert_encoding(LocaleEncodingUTF8);
}

int LoadUnicodeCharacters(const char *fn)
{
    FILE *f = fopen(fn, "r");

    if (NULL == f)
        return -1;
    
    char name[1000] = {0};
    char line[1000] = {0};

    while (!feof(f))
    {
        int code = 0;
        name[0] = 0;

        fgets(line, 1000, f);
        if (2 == sscanf(line, "0x%x		\\[%s]", &code, name))
        {
            code = code & 0xFFFF;
            int l = strlen(name);
            if (l > 1)
                name[l - 1] = '\0';
            else
                continue;
            UnicodeCharTab[code].name = MString_NewC(name);
            MStrCodeMap *m = (MStrCodeMap *)YM_malloc(sizeof(MStrCodeMap));
            m->Code = code;
            m->name = MString_NewS(UnicodeCharTab[code].name);
            MHashTable_Insert(NamedCharToCode, m);

            char *sub = strstr(line, ")");
            if (sub)
            {
                if (1 == sscanf(sub + 1, "%s", name))
                {
                    if (name == strstr(name, "WhiteSp"))
                        CharPropties[code] |= BLANK_CHAR;
                    else if (name != strstr(name, "Letter"))
                        CharPropties[code] |= OP_CHAR;
                }
            }

        }
    }
    fclose(f);

#include "sys_char_init.inc"

    // patches
    CharPropties[Char_RawDollar] = 0;
    CharPropties[Char_RawBackquote] = 0;

    return 0;
}

MString GetUnicodeCharacterName(const MChar c)
{
    return UnicodeCharTab[c].name;
}

MChar GetUnicodeCharacterCode(const MString name)
{
    MStrCodeMap s;
    s.Code = -1;
    s.name = name;
    MStrCodeMap *m = (MStrCodeMap *)MHashTable_Find(NamedCharToCode, &s);
    return m ? m->Code : Char_Placeholder;
}

enum MState
{
    msReadName,
    msReadFirst,
    msReadSnd
};

int SetLocaleEncoding(const MString name)
{
    int r = 0;
    MCharEncodingMap s;
    s.name = name;
    MCharEncodingMap *m = (MCharEncodingMap *)MHashTable_Find(CharEncodingTable, &s);
    if (m)
    {
        LocaleEncodingName = m->name;
        memcpy(_LocaleToUnicode, m->LocaleToUnicode, sizeof(_LocaleToUnicode));
        memcpy(_UnicodeToLocale, m->UnicodeToLocale, sizeof(_UnicodeToLocale));
    }
    else
        r = -1;

    return r;
}

int SetLocaleEncoding(const char *name)
{
    MString s = MString_NewC(name);
    int r = SetLocaleEncoding(s);
    MString_Release(s);
    return r;
}

MString GetLocaleEncoding(void)
{
    return LocaleEncodingName;
}

int LoadEncoding(const char *fn)
{
    FILE *f = fopen(fn, "rb");
    MState s = msReadName;
    int i;
    int r = -1;
    if (NULL == f)
        return -1;

    Zero(_UnicodeToLocale, sizeof(_UnicodeToLocale));
    Zero(_LocaleToUnicode, sizeof(_LocaleToUnicode));
    for (i = 0; i <= 127; i++)
        _UnicodeToLocale[i] = _LocaleToUnicode[i] = i;

    char line[1001] = {0};
    MChar First = 0;
    MChar Last = 0;
    while (!feof(f))
    {
        char *p = line;
        fgets(line, sizeof(line) - 1, f);

        while (*p)
        {
            // skip comment
            if (strstr(p, "(*") == p)
            {
                p = strstr(p, "*)");
                if (p == NULL)
                    break;
            }

            if (msReadName != s)
            {
                char *p1 = NULL;
                while (*p && ((*p < '0') || (*p > '9')))
                    p++;

                if (*p)
                {
                    p1 = p;
                    p++;
                    while (*p && (*p >= '0') && (*p <= '9'))
                        p++;
                    *p++ = 0;
                    
                    if (msReadFirst == s)
                    {
                        First = atoi(p1);
                        s = msReadSnd;
                    }
                    else
                    {
                        Last = atoi(p1);
                        _UnicodeToLocale[Last] = First;
                        _LocaleToUnicode[First] = Last;
                        s = msReadFirst;
                    }
                }
            }
            else
            {
                char *sub1 = strstr(line, "\"");
                char *sub2 = strstr(sub1 + 1, "\"");
                if (sub2 != NULL)
                {
                    *sub2 = 0;
                    LocaleEncodingName = MString_NewC(sub1 + 1);
                    s = msReadFirst;
                    p = sub2 + 1;
                    r = 0;
                }
                else
                    break;
            }
        }
    }

    fclose(f);

    // insert to hash table
    if (0 == r)
    {
        MCharEncodingMap *map = (MCharEncodingMap *)YM_malloc(sizeof(MCharEncodingMap));
        map->name = LocaleEncodingName;
        memcpy(map->LocaleToUnicode, _LocaleToUnicode, sizeof(_LocaleToUnicode));
        memcpy(map->UnicodeToLocale, _UnicodeToLocale, sizeof(_UnicodeToLocale));
        MHashTable_Insert(CharEncodingTable, map);
    }
    
    
    return r;
}

inline static void MStringDump2ASCII(MString s, MByte *b)
{
    int i = 0;
    const MChar *data = MString_GetData(s);
    for (; i < MString_Len(s); i++)
    {
        MChar c = data[i];
        b[i] = (MByte)c;
    }
}

MInt MStringToASCII_NameFirst(MString s, MByte *b, const MInt size)
{
    static const char Map[] = {
        '0', '1', '2', '3',
        '4', '5', '6', '7',
        '8', '9', 'A', 'B',
        'C', 'D', 'E', 'F'
    };
    int slen = MString_Len(s);
    const MChar *data = MString_GetData(s);
    int wrs = 0;
    int i = 0;
    for (; i < slen; i++)
    {
        MChar c = data[i];

        MString name = GetUnicodeCharacterName(c);
        if (name)
        {
            if (wrs + MString_Len(name) + 3 <= size)
            {
                b[wrs++] = '\\';
                b[wrs++] = '[';
                MStringDump2ASCII(name, b + wrs);
                wrs += MString_Len(name);
                b[wrs++] = ']';
            }
            else
                break;
        }
        else if (c <= 0x7f)
        {
            if (wrs < size)
                b[wrs++] = (MByte)c;
            else
                break;
        }
        else
        {
            if (wrs + 6 <= size)
            {
                b[wrs++] = '\\';
                b[wrs++] = ':';
                b[wrs++] = Map[(c & 0xF000) >> 12];
                b[wrs++] = Map[(c & 0x0F00) >>  8];
                b[wrs++] = Map[(c & 0x00F0) >>  4];
                b[wrs++] = Map[(c & 0x000F)];
            }
            else
                break;
        }
    }

    if (wrs < size)
        b[wrs] = 0;
    return wrs;
}

MInt MStringToASCII_ASCIIFirst(MString s, MByte *b, const MInt size)
{
    static const char Map[] = {
        '0', '1', '2', '3',
        '4', '5', '6', '7',
        '8', '9', 'A', 'B',
        'C', 'D', 'E', 'F'
    };
    int slen = MString_Len(s);
    const MChar *data = MString_GetData(s);
    int wrs = 0;
    int i = 0;
    for (; i < slen; i++)
    {
        MChar c = data[i];

        MString name = GetUnicodeCharacterName(c);
        if (c <= 0x7f)
        {
            if (wrs < size)
                b[wrs++] = (MByte)c;
            else
                break;
        }
        else if (name)
        {
            if (wrs + MString_Len(name) + 3 <= size)
            {
                b[wrs++] = '\\';
                b[wrs++] = '[';
                MStringDump2ASCII(name, b + wrs);
                wrs += MString_Len(name);
                b[wrs++] = ']';
            }
            else
                break;
        }
        else 
        {
            if (wrs + 6 <= size)
            {
                b[wrs++] = '\\';
                b[wrs++] = ':';
                b[wrs++] = Map[(c & 0xF000) >> 12];
                b[wrs++] = Map[(c & 0x0F00) >>  8];
                b[wrs++] = Map[(c & 0x00F0) >>  4];
                b[wrs++] = Map[(c & 0x000F)];
            }
            else
                break;
        }
    }

    if (wrs < size)
        b[wrs] = 0;
    return wrs;
}

MInt MStringToASCII(MString s, MByte *b, const MInt size, const bool bNameFirst)
{
    return bNameFirst ? MStringToASCII_NameFirst(s, b, size) : MStringToASCII_ASCIIFirst(s, b, size);
}

MInt MStringToUnicode(MString s, MByte *b, const MInt size)
{
    int cn = min(MString_Len(s) << 1, size);
    memcpy(b, MString_GetData(s), cn);
    if (cn + 1 < size)
    {
        b[cn] = 0;
        b[cn + 1] = 0;
    }
    return cn;
}

MInt MStringToUTF8(MString s, MByte *b, const MInt size)
{
    return 0;
}

static MInt __MStringToLocale(MString s, MByte *b, const MInt size, const MChar *UnicodeToLocale)
{
    int slen = MString_Len(s);
    const MChar *data = MString_GetData(s);
    int wrs = 0;
    int i;
    for (i = 0; i < slen; i++)
    {
        MChar c = UnicodeToLocale[data[i]];
        if (c <= 0x7f)
        {
            if (wrs < size)
                b[wrs++] = (MByte)c;
            else
                break;
        }
        else
        {
            if (wrs + 2 <= size)
            {
                b[wrs++] = c >> 8;
                b[wrs++] = c & 0xFF;
            }
            else
                break;
        }
    }

    if (wrs < size)
        b[wrs] = 0;
    return wrs;
}

MInt MStringToLocale(MString s, MByte *b, const MInt size)
{
    if (LocaleEncodingName == LocaleEncodingASCII)
        return MStringToASCII_ASCIIFirst(s, b, size);
    else if (LocaleEncodingName == LocaleEncodingUnicode)
        return MStringToUnicode(s, b, size);
    else if (LocaleEncodingName == LocaleEncodingUTF8)
        return MStringToUTF8(s, b, size);
    else
        return __MStringToLocale(s, b, size, _UnicodeToLocale);
}

MInt MStringToEncoding(MString s, MByte *b, const MInt size, const MString encoding)
{
    MCharEncodingMap t;
    t.name = encoding;
    MCharEncodingMap *m = (MCharEncodingMap *)MHashTable_Find(CharEncodingTable, &t);
    if (m)
    {
        if (m->name == LocaleEncodingASCII)
            return MStringToASCII_ASCIIFirst(s, b, size);
        else if (m->name == LocaleEncodingUnicode)
            return MStringToUnicode(s, b, size);
        else if (m->name == LocaleEncodingUTF8)
            return MStringToUTF8(s, b, size);
        else
            return __MStringToLocale(s, b, size, m->UnicodeToLocale);
    }
    else
    {
        if (size > 0)
            b[0] = 0;
        return 0;
    }
}

MString ASCIIToMString(MByte *b, const MInt size)
{
    MString r = MString_NewWithCapacity(size);
    MChar *data = (MChar *)MString_GetData(r);
    int wrs = 0;
    int i = 0;
    int pos;
    while (i < size)
    {
        MChar c = b[i++];
        if (c != '\\')
        {
            data[wrs++] = c;
        }
        else
        {
            if (i >= size)
                break;

            c = b[i++];
            switch (c)
            {
//            case '\\': data[wrs++] = '\\'; break;
//            case 't':  data[wrs++] = '\t'; break;
//            case 'n':  data[wrs++] = '\n'; break;
//            case 'r':  data[wrs++] = '\r'; break;
            case ':': 
                if (i + 4 < size)
                {
                    data[wrs++] =   (HexCharToInt[b[i + 0]] << 12) | 
                                    (HexCharToInt[b[i + 1]] <<  8) | 
                                    (HexCharToInt[b[i + 2]] <<  4) | 
                                    (HexCharToInt[b[i + 3]]);
					i += 4;
                }
                else
                    goto error;
                break;
            case '[':
                pos = i;
                while ((i < size) && (b[i++] != ']'));
                if (b[i - 1] == ']')
                {
                    MString s = MString_NewCN((char *)(b + pos), i - pos - 1);
                    data[wrs++] = GetUnicodeCharacterCode(s);
                    MString_Release(s);
                }
                else
                    goto error;
                break;
            default:
                data[wrs++] = '\\';
                data[wrs++] = c;
            }
        }
    }
error:

    r->Len = wrs;
    return r;
}

MString UnicodeToMString(MByte *b, const MInt size)
{
    return MString_NewMN((MChar *)b, size >> 1);
}

MString UTF8ToMString(MByte *b, const MInt size)
{
    return NULL;
}

static MString __LocaleToMString(MByte *b, const MInt size, const MChar *LocaleToUnicode)
{
    MString r = MString_NewWithCapacity(size);
    MChar *data = (MChar *)MString_GetData(r);
    int wrs = 0;
    int i = 0;
    while (i < size)
    {
        MChar c = b[i++];
        int pos;
        if (c <= 127)
        {
            if (c != '\\')
            {
                data[wrs++] = c;
            }
            else
            {
                if (i >= size)
                {
                    data[wrs++] = c;        // end of a line
                    break;
                }

                c = b[i++];
                switch (c)
                {
//                case '\\': data[wrs++] = '\\'; break;
//                case 't':  data[wrs++] = '\t'; break;
//                case 'n':  data[wrs++] = '\n'; break;
//                case 'r':  data[wrs++] = '\r'; break;
                case ':': 
                    if (i + 4 < size)
                    {
                        data[wrs++] =   (HexCharToInt[b[i + 0]] << 12) | 
										(HexCharToInt[b[i + 1]] <<  8) | 
										(HexCharToInt[b[i + 2]] <<  4) | 
										(HexCharToInt[b[i + 3]]);
						i += 4;
                    }
                    else
                        goto error;
                    break;
                case '[':
                    pos = i;
                    while ((i < size) && (b[i++] != ']'));
                    if (b[i - 1] == ']')
                    {
                        MString s = MString_NewCN((char *)(b + pos), i - pos - 1);
                        data[wrs++] = GetUnicodeCharacterCode(s);
                        MString_Release(s);
                    }
                    else
                        goto error;
                    break;
                default:
                    data[wrs++] = '\\';
                    data[wrs++] = c;
                }
            }
        }
        else
        {
            if (i < size)
            {
                c <<= 8;
                c |= b[i++];
                data[wrs++] = LocaleToUnicode[c];
            }
            else
                break;
        }
    }
error:
    r->Len = wrs;
    return r;
}

MString LocaleToMString(MByte *b, const MInt size)
{
    if (LocaleEncodingName == LocaleEncodingASCII)
        return ASCIIToMString(b, size);
    else if (LocaleEncodingName == LocaleEncodingUnicode)
        return UnicodeToMString(b, size);
    else if (LocaleEncodingName == LocaleEncodingUTF8)
        return UTF8ToMString(b, size);
    else
        return __LocaleToMString(b, size, _LocaleToUnicode);
}

MString EncodingToMString(MByte *b, const MInt size, const MString encoding)
{
    if (encoding)
    {
        MCharEncodingMap s;
        s.name = encoding;
        MCharEncodingMap *m = (MCharEncodingMap *)MHashTable_Find(CharEncodingTable, &s);
        if (m)
        {
            if (m->name == LocaleEncodingASCII)
                return ASCIIToMString(b, size);
            else if (m->name == LocaleEncodingUnicode)
                return UnicodeToMString(b, size);
            else if (m->name == LocaleEncodingUTF8)
                return UTF8ToMString(b, size);
            else
                return __LocaleToMString(b, size, m->LocaleToUnicode);
        }
        else
            return MString_NewC("");
    }
    else
        return LocaleToMString(b, size);
}
