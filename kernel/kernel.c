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

#include "kernel.h"
#include "mstr.h"
#include "mm.h"
#include "token.h"
#include "encoding.h"
#include "big.h"
#include "parser.h"
#include "sys_sym.h"

outside *the_outside = NULL;

#define convert_s(str, size, len, buf)   \
    int size = MString_Len(str) * sizeof(MChar) + 100;  \
    char *buf = (char *)YM_malloc(size); \
    int len = MStringToLocale(str, (MByte *)buf, size - 1);

MString K_Input(MString prompt, MString init)
{
    if (the_outside)
    {
        char in[1024 + 1] = {'\0'}; 
        convert_s(prompt, s1, len1, buf1);
        convert_s(init,   s2, len2, buf2);

        int in_len = the_outside->io_input(ioSTD, in, sizeof(in) - 1, buf1, len1, buf2, len2);
        YM_free(buf1);
        YM_free(buf2);

        if (in_len > 0)
        {
            return LocaleToMString((MByte *)in, in_len);
        }
        else
            return MString_Copy(init, 1, MString_Len(init));
    }
    else
        return MString_Copy(init, 1, MString_Len(init));
}

void K_Output(int port, MString msg)
{
#define BSIZE (8 * 1024)
    if (the_outside)
    {
        char buf[BSIZE + 1024 + 1] = {0};
        int i = 1;
        int len;
        while (true)
        {
            MString s = MString_Copy(msg, i, BSIZE);
            if (MString_Len(s) < 1)
            {
                MString_Release(s);
                break;
            }
            len = MStringToLocale(s, (MByte *)buf, sizeof(buf) - 1);
            MString_Release(s);
            the_outside->io_output(port, buf, len);
            i += BSIZE;
        }
        
    }
}

extern int SetLocaleEncoding(const MString name);

typedef MExpr (*f_parse) (MTokener *tokener);

static f_parse K_Parse = Parse_FromTokener;

int Kernel_Init(outside *p)
{
    if (p->version != KVERSION)
        return MERROR;

    the_outside = p;

    MString_Init();
    Big_Init();
    Num_Init();
    MExpr_Init();
    MSymbol_Init();   

    EncodingInit();
    LoadUnicodeCharacters("UnicodeCharacters.tr");
    LoadEncoding("cp936.m");

    SetLocaleEncoding(p->encoding);
    if (stricmp(p->lang, "lisp") == 0)
        K_Parse = Parse_FromTokenerLisp;

    Token_Init();
    return Eval_Init();
}

int Kernel_Eval(const char *p, const int size, const bool bPrint)
{
    MString input = LocaleToMString((MByte *)p, size);
    MTokener *tokener = Token_NewFromMemory(input);
    MExpr expr = K_Parse(tokener);

    Token_Delete(tokener);
    MString_Release(input);

#if (0)    
    {
        MString s2 = MExpr_ToFullForm(expr);
        K_Output(ioSTD, s2); 
        the_outside->io_output(ioSTD, "\n", 1);
        MString_Release(s2);
    }
#endif

    // Evaluate
    {
        MExpr out = Eval_Evaluate(expr);    
        MExpr_Release(expr);

        if ((out->Type != etSymbol) || (out->Symbol != sym_Null))
        {
            if (bPrint)
            {
                MString s2 = MExpr_ToFullForm(out);
                K_Output(ioSTD, s2); 
                the_outside->io_output(ioSTD, "\n", 1);
                MString_Release(s2);
            }
        }    
        MExpr_Release(out);
    }

    return 0;
}
