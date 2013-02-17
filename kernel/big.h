
#ifndef _big_h
#define _big_h

#include <stdio.h>
#include "mstr.h"
#include "mtype.h"

#define WORDBITS        16
#define WORDBITSONEHALF (WORDBITS + WORDBITS / 2)
#define WORDMASK        0xFFFF
#define WORDSIZE        (WORDBITS / 8)
#define WORDHALF        (1 << (WORDBITS - 1))

#define IEEE_EXPO_BIAS ((MInt)(1023))
#define IEEE_FACT_BITS       ((MInt)(sizeof(MReal) * 8 - 12))
#define IEEE_FACT_WORD_BITS (((IEEE_FACT_BITS + WORDBITS - 1) / WORDBITS) * WORDBITS)

#define MAX_PRECISION (M_MAX_INT - 1)

// For Integers:
//      wordprecision = -1
//      binexp should be 0
// For Reals:
//      bitpreci: how many effective bits are there in buf.
//      decimal point is before the 1st word of buf, which makes the buf represent an integer:   *    *   *    *   .0
//                                                                                             buf[i]  ...  buf[0]
//      binexp is also used
// In all, 
//      Value = buf * base ^ binexp, buf is an integer 
struct MTagBigNum
{
    MWord *buf;         // sign is stored in bNeg, buf is the abs value, always an integer
    MInt   capacity;
    MInt   size;   

    MInt  binexp;       // * (0x10000)^binexp
    MInt  bitpreci;

    MBool bNeg;
};

typedef MTagBigNum * MBigNum;

MBigNum Big_Create(MString s);
MBigNum Big_Create(MInt v);
MBigNum Big_CreateU(MUInt v);
MBigNum Big_Create(MReal v);
MBigNum Big_Create(MBigNum a);
MBigNum Big_Uniquelize(MBigNum a);
MBigNum Big_UniquelizeIntPart(MBigNum a);
MBigNum Big_UniquelizeFractPart(MBigNum a);
MBigNum Big_SetFrom(MBigNum a, const MBigNum b);
MReal   Big_ToIEEE754(MBigNum a);
MInt    Big_ToInt(MBigNum a);
MString Big_ToString(MBigNum a, const MInt Base = 10);

#define Big_Release XRef_DecRef

MBigNum Big_Add(const MBigNum a, const MBigNum b);
MBigNum Big_AddBy(MBigNum a, const MBigNum b);
MBigNum Big_AddBy(MBigNum a, MSWord b);
MBigNum Big_Subtract(const MBigNum a, const MBigNum b);
MBigNum Big_SubtractBy(MBigNum a, const MBigNum b);
MBigNum Big_Divide(const MBigNum a, const MBigNum b, MInt TargetPrecision = -1);
MBigNum Big_DivideBy(MBigNum a, const MBigNum b, MInt TargetPrecision = -1);
MBigNum Big_IntDivMod(const MBigNum a, const MBigNum b, MBigNum *premainer);
MBigNum Big_IntDiv(const MBigNum a, const MBigNum b);
MBigNum Big_IntMod(const MBigNum a, const MBigNum b);
MBigNum Big_DivMod(const MBigNum a, const MBigNum b, MBigNum *premainer);
MBigNum Big_Div(const MBigNum a, const MBigNum b);
MBigNum Big_Mod(const MBigNum a, const MBigNum b);
MBigNum Big_Multiply(const MBigNum a, const MBigNum b);
MBigNum Big_MultiplyBy(MBigNum a, const MBigNum b);

MBigNum Big_ShiftLeft(const MBigNum a, const MInt off);
MBigNum Big_ShiftLeftBy(MBigNum a, MInt off);

MBigNum Big_Normalize(MBigNum a);

MBool   Big_IsMachineInt(MBigNum a);
#define Big_IsZero(a)               ((a)->size == 0)
#define Big_IsInt(a)                (((a)->bitpreci < 0) && ((a)->binexp == 0))

MBool   Big_LT (const MBigNum a, const MBigNum b);
MBool   Big_LTE(const MBigNum a, const MBigNum b);
MBool   Big_E  (const MBigNum a, const MBigNum b);
MBool   Big_GTE(const MBigNum a, const MBigNum b);
MBool   Big_GT (const MBigNum a, const MBigNum b);
MBool   Big_NE (const MBigNum a, const MBigNum b);
MInt    Big_Cmp(const MBigNum a, const MBigNum b);
MInt    Big_AbsCmp(const MBigNum a, const MBigNum b);
MBool   Big_SameQ(const MBigNum a, const MBigNum b);
MBool   Big_UnsameQ(const MBigNum a, const MBigNum b);
MBool   Big_EvenQ(MBigNum a);

MReal   Big_Accuracy(MBigNum a);
MReal   Big_Precision(MBigNum a);
MInt    Big_WordPrecision(MBigNum a);

MBigNum Big_Power(MBigNum a, MBigNum b);
MBigNum Big_Sqrt(MBigNum a, MInt precision);

MInt Big_SetWorkingPrecission(const MInt NewPreci);
MInt Big_GetWorkingPrecission(void);

void Big_Init();
void Big_DumpHeap(FILE *file);

void Big_Print(MBigNum a);

#endif
