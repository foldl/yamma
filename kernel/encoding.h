
#ifndef encoding_h
#define encoding_h

#include "mstr.h"

#define BLANK_CHAR  (0x00000001 << 1)
#define OP_CHAR     (0x00000001 << 2)
#define NUM_CHAR    (0x00000001 << 3)

extern MDword CharPropties[0x10000];

#define IsBlankChar(c) (CharPropties[(c)] & BLANK_CHAR)
#define IsNumChar(c) (CharPropties[(c)] & NUM_CHAR)
#define IsLetterChar(c) (CharPropties[(c)] == 0)
#define IsOpChar(c) (CharPropties[(c)] & OP_CHAR)

extern int HexCharToInt[128];
#define Char2Int(c) (HexCharToInt[c])
extern MChar IntToHexChar[128];
#define Int2Char(c) (IntToHexChar[c])

void EncodingInit(void);

int LoadUnicodeCharacters(const char *fn);
int LoadEncoding(const char *fn);

int SetLocaleEncoding(const char *name);
MString GetLocaleEncoding(void);

MString GetUnicodeCharacterName(const MChar c);
MChar   GetUnicodeCharacterCode(const MString name);

MInt MStringToASCII(MString s, MByte *b, const MInt size, const bool bNameFirst = false);
MInt MStringToUnicode(MString s, MByte *b, const MInt size);
MInt MStringToUTF8(MString s, MByte *b, const MInt size);
MInt MStringToLocale(MString s, MByte *b, const MInt size);
MInt MStringToEncoding(MString s, MByte *b, const MInt size, const MString encoding);

MString UnicodeToMString(MByte *b, const MInt size);
MString UTF8ToMString(MByte *b, const MInt size);
MString LocaleToMString(MByte *b, const MInt size);
MString ASCIIToMString(MByte *b, const MInt size);
MString EncodingToMString(MByte *b, const MInt size, const MString encoding);

#endif
