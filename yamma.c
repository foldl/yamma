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

#include <stdio.h>
#include <string.h>
#include <math.h>

#include "./kernel/mstr.h"
#include "./kernel/mm.h"
#include "./kernel/encoding.h"
#include "./kernel/eval.h"
#include "./kernel/msg.h"
#include "./kernel/sym.h"
#include "./kernel/moi.h"
#include "./kernel/token.h"
#include "./kernel/parser.h"
#include "./kernel/eval.h"
#include "./kernel/sys_sym.h"
#include "./kernel/big.h"
#include "./kernel/kernel.h"

static MInt BIF_GCD_uint_uint(const MDword v1, const MDword v2)
{
    if (v2 == 0)
        return v1;

    MDword a = v2;
    MDword b = v1 % v2;
    while (b != 0)
    {        
        MDword t = a % b;
        a = b;
        b = t;
    }
    return a;
}

static char s_encoding[] = "CP936";
static int fe_input(int port, 
                          char *p, const int size,
                          const char *prompt, const int prompt_len,
                          const char *init, const int init_len)
{
    printf(prompt);
    return strlen(fgets(p, size, stdin)) - 1;
}

int fe_output(int port, const char *p, const int size)
{
    printf(p);
    return 0;
}

const char *lang_mma = "mma";
const char *lang_lisp = "lisp";

outside this_one = 
{
    KVERSION,
    fe_input,
    fe_output,
    s_encoding,
    lang_mma
};

typedef struct
{
    const char *full;
    const char *abbr;
    const char *help;
    int (* opt_handler)(int argc, char *argv[], int index);
} cl_opt;

static int handle_lang(int argc, char *argv[], int index)
{
    index++;
    if (index >= argc) return index;
    if (strcmp(argv[index], lang_mma) == 0)
        this_one.lang = lang_mma;
    else if (strcmp(argv[index], lang_lisp) == 0)
        this_one.lang = lang_lisp;
    return index;
}

static void show_help();

static int handle_help(int argc, char *argv[], int index)
{
    show_help();
    exit(0);
}

static cl_opt opts[] = 
{
    {
        "-lang",
        "-l",
        "set dialect of REPL, mma or lisp",
        handle_lang
    },
    {
        "-help",
        "-h",
        "show this help information",
        handle_help
    }
};

static void show_help()
{
    unsigned int i;
    printf("options:\n");
    for (i = 0; i < sizeof(opts) / sizeof(opts[0]); i++)
    {
        printf("\t%s(%s)\t%s\n", opts[i].full, opts[i].abbr, opts[i].help);
    }
}

void cl_process(int argc, char *argv[])
{
    int index = 1;
    while (index < argc)
    {
        unsigned int i;
        for (i = 0; i < sizeof(opts) / sizeof(opts[0]); i++)
        {
            if ((strcmp(argv[index], opts[i].full) == 0) || (strcmp(argv[index], opts[i].abbr) == 0))
            {
                index = opts[i].opt_handler(argc, argv, index);
                goto next_arg;
            }
        }
        index++;
next_arg:        
        ;
    }
}

int Yamma_Init(int argc, char* argv[])
{
    cl_process(argc, argv);
    printf("Initializing ...\n");
    return Kernel_Init(&this_one);
}

void Encoding_Test();
void Sym_Test();

void MM_test();

void Big_test();

void MIO_test();
void MString_Test();
void Token_test();

void Eval_test();
void GCD_test();
void repl_test2();
void Big_double_test();

int main(int argc, char* argv[])
{
    int r = Yamma_Init(argc, argv);
    if (0 != r)
    {
        printf("Yamma_Init() failed.\n");
        exit(r);
    }

    repl_test2();
    return 0;
}

int main_test()
{
//    Eval_test();
//    Big_test();

    repl_test2();

//    MM_test();

    return 0;
    
    MIO_test();
//    MXRef_Dump(stdout);
//    MString_DumpHeap(stdout);
    
    MString s = MString_NewC("DoubleStruckOne");
    printf("%d\n", GetUnicodeCharacterCode(s));

//    MXRef_Dump(stdout);
//    MString_DumpHeap(stdout);
    
#define print_size(t)    printf("sizeof(" #t ") = %d\n", sizeof(t))

    Sym_Test();
    
    
	return 0;
}

void GCD_test()
{
    printf("GCD[100, 10] = %d\n", BIF_GCD_uint_uint(200000, 300000));
    return;
}

void Eval_test()
{
//    MXRef_Dump(stdout);

    MString encoding = MString_NewC("CP936");
    MTokener *tokener = Token_NewFromMemory("MatchQ[{1}, {a_?(# === 1&)}]", encoding); // "MatchQ[{1,2,3,4}, {a___, b_, c_, d_Integer, e_}]", encoding); // fac = If[# < 2, 1, # #0[# - 1]]&; f[x]=9*^6; f[g]=a; z[x]=y; z[y] = abc; z[e]=d;
    MExpr expr = Parse_FromTokener(tokener);
    Token_Delete(tokener);
    char t2[2000 + 1] = {0};
    MString s2 = MExpr_ToFullForm(expr);
    MStringToASCII(s2, (MByte *)t2, sizeof(t2) - 1);
    printf("In : %s\n", t2);
    
    MExpr out = Eval_Evaluate(expr);
    if ((out->Type != etSymbol) || (out->Symbol != sym_Null))
    {
        MString s2 = MExpr_ToFullForm(out);
        MStringToASCII(s2, (MByte *)t2, sizeof(t2) - 1);
        printf("Out: %s\n", t2);
        
    }
    else;
    MExpr_Release(out);
    MExpr_Release(expr);
}

#define BIG_BUF 10000
void repl_test2()
{
    while (true)
    {
        char input[BIG_BUF + 1] = {0};
        printf(">");
        if (gets(input) == NULL)
            break;

        if (strlen(input) < 1)
            break;

        Kernel_Eval(input, strlen(input));
    }
 
    if (1)
    {
        FILE *f = fopen("dump.dat", "w");
        MXRef_Dump(f);
        MString_DumpHeap(f);
        MExpr_DumpHeap(f);
        Big_DumpHeap(f);
        fclose(f);
    }
}

void repl_test()
{
    MString encoding = MString_NewC("CP936");
    while (true)
    {
        char input[BIG_BUF + 1] = {0};
        printf(">");
        if (gets(input) == NULL)
            break;

        if (strlen(input) < 1)
            break;

        MTokener *tokener = Token_NewFromMemory(input, encoding); // f[x]=9*^6; f[g]=a; z[x]=y; z[y] = abc; z[e]=d;
        MExpr expr = Parse_FromTokenerLisp(tokener);
        char t2[BIG_BUF + 1] = {0};

        Token_Delete(tokener);
        
        // Print
        if (0)
        {
            MString s2 = MExpr_ToFullForm(expr);
            MStringToASCII(s2, (MByte *)t2, sizeof(t2) - 1);
            printf("In : %s\n", t2);
            MString_Release(s2);
        }
        
        
        // Evaluate
        {
            MExpr out = Eval_Evaluate(expr);    
        
            if ((out->Type != etSymbol) || (out->Symbol != sym_Null))
            {
                MString s2 = MExpr_ToFullForm(out);
                int l1 = MStringToASCII(s2, (MByte *)t2, sizeof(t2) - 1);
                printf("Out: %s", t2);
                if (l1 < (int)sizeof(t2) - 1)
                    printf("\n");
                else
                    printf("...\n");
                MString_Release(s2);
            }    
            MExpr_Release(out);
        }
        
        MExpr_Release(expr);
    }
 
    {
        FILE *f = fopen("dump.dat", "w");
        MXRef_Dump(f);
        MString_DumpHeap(f);
        MExpr_DumpHeap(f);
        Big_DumpHeap(f);
        fclose(f);
    }
}

void Parser_test()
{
    // a b c [x, y z]
    // a + a b c
    // a b c + a
    // (a b c) [x y z]
    // (a(b c))[(x y)(x)]
}

void Token_test()
{
//    MXRef_Dump(stdout);

    MString encoding = MString_NewC("CP936");
    MTokener *tokener = Token_NewFromMemory("xxxxxxx", encoding); // f[x]=9*^6; f[g]=a; z[x]=y; z[y] = abc; z[e]=d;
    MExpr expr = Parse_FromTokener(tokener);
    Token_Delete(tokener);
    char t2[2000 + 1] = {0};
    MString s2 = MExpr_ToFullForm(expr);
    MStringToASCII(s2, (MByte *)t2, sizeof(t2) - 1);
    printf("expr: %s\n", t2);
    
//    MXRef_Dump(stdout);
//    MExpr_Release(expr);// MExpr_Release(expr); 
//    MXRef_Dump(stdout);

    {
        MString s1 = MString_NewC("f");
        MSymbol s = MSymbol_GetOrNewSymbol(s1);
        MExpr e = MExpr_CreateSymbol(s);
        MString s2 = MExpr_ToFullForm(e);
        char t2[100];
        MStringToASCII(s2, (MByte *)t2, sizeof(t2));
        printf("%s\n", t2);
        MStringToASCII(MSymbol_Context(s), (MByte *)t2, sizeof(t2));
        printf("%s\n", t2);

//        MXRef_Dump(stdout);
        e = MSymbol_Attributes(s);
        s2 = MExpr_ToFullForm(e);
        MStringToASCII(s2, (MByte *)t2, sizeof(t2));
        printf("Attribute: %s\n", t2);
    }

//    {
//        FILE *f = fopen("dump.dat", "w");
//        MXRef_Dump(f);
//        MString_DumpHeap(f);
//        fclose(f);
//    }
    

//    do {
//    	Token_Next(tokener);
//        if (tokener->Token == NULL)
//            break;
//        
//        char t2[100 + 1] = {0};
//        int l1 = MStringToASCII(tokener->Token, (MByte *)t2, sizeof(t2) - 1);
//        printf("Token: %s\n", t2);
//
//    } while(true);
//    Token_Delete(tokener);

}

void Sym_Test()
{
    {
        MString s1 = MString_NewC("`hehe`Pi");
        MSymbol s = MSymbol_GetOrNewSymbol(s1);
        MExpr e = MExpr_CreateSymbol(s);
        MString s2 = MExpr_ToFullForm(e);
        char t2[100];
        MStringToASCII(s2, (MByte *)t2, sizeof(t2));
        printf("%s\n", t2);
        MStringToASCII(MSymbol_Context(s), (MByte *)t2, sizeof(t2));
        printf("%s\n", t2);

        e = MSymbol_Attributes(s);
        s2 = MExpr_ToFullForm(e);
        MStringToASCII(s2, (MByte *)t2, sizeof(t2));
        printf("Attribute: %s\n", t2);
//        MXRef_Dump(stdout);

//        MString_Release(s2);
//        MExpr_Release(e);
//        MSymbol_Release(s);
//        MString_Release(s1);
//
//        MXRef_Dump(stdout);
    }

    {
        MString s1 = MString_NewC("Pi");
        MSymbol s = MSymbol_GetOrNewSymbol(s1);
        MExpr e = MExpr_CreateSymbol(s);
        MString s2 = MExpr_ToFullForm(e);
        char t2[100];
        MStringToASCII(s2, (MByte *)t2, sizeof(t2));
        printf("%s\n", t2);
        MStringToASCII(MSymbol_Context(s), (MByte *)t2, sizeof(t2));
        printf("%s\n", t2);

        e = MSymbol_Attributes(s);
        s2 = MExpr_ToFullForm(e);
        MStringToASCII(s2, (MByte *)t2, sizeof(t2));
        printf("Attribute: %s\n", t2);
//        MXRef_Dump(stdout);

//        MString_Release(s2);
//        MExpr_Release(e);
//        MSymbol_Release(s);
//        MString_Release(s1);
//
//        MXRef_Dump(stdout);
    }
    
}
void Encoding_Test()
{
    {
        char t1[] = "你我他";
        char t2[100];
        MString s2 = LocaleToMString((MByte *)t1, sizeof(t1));
        int l1 = MStringToASCII(s2, (MByte *)t2, sizeof(t2));
        printf("%s\n", t2);
        l1 = MStringToLocale(s2, (MByte *)t2, sizeof(t2));
        printf("%s\n", t2);
    }

    {
        char t1[] = "~This is a \\\\[RawTab]: '\\[RawTab]', you cannot see ityou cannot see ityou cannot see ityou cannot see ityou cannot see it";
        char t2[100 + 1] = {0};
        MString s2 = ASCIIToMString((MByte *)t1, sizeof(t1));
        MStringToASCII(s2, (MByte *)t2, sizeof(t2) - 1);
        printf("%s\n", t2);
    }

    {
        char t1[] = "你我他";
        char t2[100 + 1] = {0};
        int l1 = MStringToASCII(GetLocaleEncoding(), (MByte *)t2, sizeof(t2) - 1);
        printf("%s -- ", t2);
        MString s2 = LocaleToMString((MByte *)t1, sizeof(t1));
        l1 = MStringToLocale(s2, (MByte *)t2, sizeof(t2));
        printf("%s\n", t2);
        SetLocaleEncoding("7Bit");
        l1 = MStringToASCII(GetLocaleEncoding(), (MByte *)t2, sizeof(t2) - 1);
        printf("%s -- ", t2);
        l1 = MStringToLocale(s2, (MByte *)t2, sizeof(t2));
        printf("%s\n", t2);

        MStringToEncoding(s2, (MByte *)t2, sizeof(t2), MString_NewC("CP936"));
        printf("CP936 -- %s\n", t2);
        MStringToEncoding(s2, (MByte *)t2, sizeof(t2), MString_NewC("7Bit"));
        printf("7Bit  -- %s\n", t2);
        MStringToEncoding(s2, (MByte *)t2, sizeof(t2), MString_NewC("16Bit"));
        printf("16Bit -- %s\n", t2);
    }
}

void MString_Test()
{
	MString_Init();
//    MXRef_Dump(stdout);
    MString s1 = MString_NewC("hello world.");
    MString s2 = MString_NewC("wo");
//    MXRef_Dump(stdout);
    ASSERT(MString_Pos(s1, s2) == 7);
    return;
    MString_DumpHeap(stdout);
    MString_Release(s1);
    MString_Release(s2);
    MString_DumpHeap(stdout);
    MXRef_Dump(stdout);
//    s2 = MString_NewC("world.");
    MString_NewC(""); MString_NewC("");
    MString_NewC("a"); MString_NewC("a");
    MString_DumpHeap(stdout);
    MXRef_Dump(stdout);
    printf("------------\n");
    s1 = MString_NewC("hello world!hello world!hello world!hello world!hello world!hello world!");
    MXRef_Dump(stdout);
    MString_DumpHeap(stdout);
    s2 = MString_Copy(s1, 10, 10);
    MXRef_Dump(stdout);
    MString_DumpHeap(stdout);
    MString_Release(s1); MString_NewC("a");
    MXRef_Dump(stdout);
    MString_DumpHeap(stdout);
}

static MXRefObjType Type_MT = NULL;

struct MMT
{
    MMT *ref;
    char name[10];
};

static MMT *MMT_Init(MMT *p)
{
    printf("Init: 0x%p: %s\n", p, p->name);
    return NULL;
}

static char *MMT_Dump(MMT *p, char *s, int max_len)
{
    strcpy(s, p->name);
    return s;
}

static MMT *MMT_Dispose(MMT *p)
{
    printf("Free: 0x%p: %s\n", p, p->name);
    if (p->ref)
        XRef_DecRef(p->ref);
    return NULL;
}

static MXRefObjMT RefObj_MT = {(f_YM_XRefObjInit)MMT_Init, (f_YM_XRefObjDispose)MMT_Dispose, (f_YM_XRefObjDump)MMT_Dump};

void MM_test()
{
    Type_MT = MXRefObj_Register(&RefObj_MT, sizeof(MMT), "MT");
    MMT* s1, * s2, * s3, * s4;
    s1 = MXRefObj_Create(MMT, Type_MT); strcpy(s1->name, "s1");
    s2 = MXRefObj_Create(MMT, Type_MT); strcpy(s2->name, "s2");
    s3 = MXRefObj_Create(MMT, Type_MT); strcpy(s3->name, "s3");
    s4 = MXRefObj_Create(MMT, Type_MT); strcpy(s4->name, "s4");
    MXRef_Dump(stdout);

    XRef_DecRef(s2);    // s2 freed
    MXRef_Dump(stdout);
fprintf(stderr, "s2\n");
    s2 = MXRefObj_Create(MMT, Type_MT); // s2 re-alloc
    fprintf(stderr, "s2\n");
    //MXRefObj_Create(MMT, Type_MT);
    MXRef_Dump(stdout);
    
//    XRef_DecRef(s2);    // s2 freed
//    MXRef_Dump(stdout);
//
//    XRef_DecRef(s1);    // s1 freed
//    MXRef_Dump(stdout);

//    XRef_DecRef(s3);    // s3 freed
//    MXRef_Dump(stdout);
    
    s1->ref = s3; XRef_IncRef(s3);
    XRef_DecRef(s3);
    XRef_DecRef(s2);    // s2 freed
    XRef_DecRef(s1);    // s1 freed
    MXRef_Dump(stdout);

    s1 = MXRefObj_Create(MMT, Type_MT); // s1 re-alloc, s3 freed
    MXRef_Dump(stdout);
    s2 = MXRefObj_Create(MMT, Type_MT); // s3 re-all
    MXRef_Dump(stdout);
    s3 = MXRefObj_Create(MMT, Type_MT); // s2 re-all
    MXRef_Dump(stdout);
//    MXRefObj_Create(MMT, Type_MT);

    MXRef_Dump(stdout);
}

void MIO_test()
{
    MFile f;
    char r[1001] = {0};
    MString en = MString_NewC("CP936");
    MIO_OpenFile(&f, "e:\\a.txt", en);
    while (!MIO_IsEOF(&f))
    {
        MString s = MIO_ReadLine(&f);
        MStringToASCII(s, (MByte *)r, 1000);
        printf("%s\n", r);
        MString_Release(s);
    }
}

//14320625,14067234
// 32940000 (1 + 0.0387 / 12) - 33046231 // Round
// 14320625 (1 + 0.0387 / 12) - 299576 // Round
// 14320625 (1 + 0.0387 / 12) - 299576 - 14067233 // Round
// fNestList[Round[# (1 + 0.0387/12)] - 299576&, 100 500000, 240]
// fNest[# (1 + 387 / 120000) - 299576 &, 100 500000, 240]
// fNest[# (1 + 387 / 120000) - xx &, 100 500000, 1]
void print_double(double &f)
{
#define IEEE_WORD_LEN 4
    /************************************************************************
    Double precision binary floating-point format
    Sign bit: 1 bit
    Exponent width: 11 bits
    Significant precision: 52 bits (53 implicit)
    **/
    union
    {
        MWord mimic[IEEE_WORD_LEN];
        MReal r;
    };

    r = f;

    if (mimic[IEEE_WORD_LEN - 1] & 0x8000)
        printf("Neg\n");
    else
        printf("Pos\n");

    printf("Exp = %d\n",     ((mimic[IEEE_WORD_LEN - 1] & 0x7FF0) >> 4) - 1023);
    printf("Man = 1.%1X ",  mimic[IEEE_WORD_LEN - 1] & 0xF);
    printf("%04X ",  mimic[IEEE_WORD_LEN - 2]);
    printf("%04X ",  mimic[IEEE_WORD_LEN - 3]);
    printf("%04X\n",  mimic[IEEE_WORD_LEN - 4]);
}

void Big_double_test()
{
    /************************************************************************
    Double precision binary floating-point format
    Sign bit: 1 bit
    Exponent width: 11 bits
    Significant precision: 52 bits (53 implicit)
    **/

    double f = 0.0;
    char r[1001] = {0};
    sscanf("0.387", "%lf", &f);
    f /= 12;
    print_double(f);
    printf("%f\n", f);

    MString v0 = MString_NewC("1"); 
    MBigNum b0 = Big_Create(v0);
    MString v1 = MString_NewC("0.0387"); 
    MBigNum b1 = Big_Create(v1);
    MString v2 = MString_NewC("12");
    MBigNum b2 = Big_Create(v2);
    {
        
        MBigNum y = Big_Divide(b0, b2);
        MBigNum x = Big_Multiply(y, b1);
        Big_Print(x);

        MString s = Big_ToString(x, 10);
        MStringToASCII(s, (MByte *)r, 1000);
        printf("%s, %f\n", r, Big_ToIEEE754(x));

        Big_Release(x);
        MString_Release(s);
    }
}

void Big_test()
{
    char r[1001] = {0};
    if (0)
    {
        MString s = MString_NewC("3.141592653589793238462643383279502884197169399375105820974944592307816406286208998628034825342117067982148086");
        MBigNum b = Big_Create(s);

        Big_Print(b);
        MString ss = Big_ToString(b, 10);
        MStringToASCII(ss, (MByte *)r, 1000);
        printf("%s, %f\n", r, Big_ToIEEE754(b));
        return;
    }
    MString v1 = MString_NewC("0.0387"); 
    MBigNum b1 = Big_Create(v1);
    MString v2 = MString_NewC("12");
    MBigNum b2 = Big_Create(v2);

    MWord tt[20];
    memcpy(tt, b1->buf, sizeof(tt));

    Big_Print(b1);
    Big_Print(b2);
    {
        
        MBigNum x = Big_Divide(b1, b2);
        Big_Print(x);

        x = Big_Add(x, Big_Create(MString_NewC("1")));
        Big_Print(x);

        x = Big_MultiplyBy(x, Big_Create(MString_NewC("32940000")));
        Big_Print(x);

        MString s = Big_ToString(x, 10);
        MStringToASCII(s, (MByte *)r, 1000);
        printf("%s, %f\n", r, Big_ToIEEE754(x));

        Big_Release(x);
        MString_Release(s);
    }

    MString_Release(v1);
    MString_Release(v2);
    
    Big_Release(b1);
    Big_Release(b2);

    {
        FILE *f = fopen("dump.dat", "w");
        MXRef_Dump(f);
        MString_DumpHeap(f);
        MExpr_DumpHeap(f);
        Big_DumpHeap(f);
        fclose(f);
    }
}
