
#ifndef _num_h
#define _num_h

#include "hash.h"
#include "mstr.h"

typedef MUnknown MNum;

enum MNumErrorCode
{
    ecOK,
    ecDivBy0,
    ecOverflow,
    ecUnderflow
};

// Log[10, 256.] = 2.4082399653118495617099
#define MACHINE_ACCURACY (2.4082399653118495617099 * ((sizeof(MReal) - 2) + 0.25))

MNum Num_Create(MString s);
MNum Num_Create(MInt v);
MNum Num_CreateU(MUInt v);
MNum Num_CreateNoCache(MInt v);
MNum Num_Create(MReal v);
MNum Num_Create(MNum a);
MNum Num_CreateRational(const MNum Numerator, const MNum Denominator);
MNum Num_CreateRationalX(const MNum Numerator, const MNum Denominator);
MNum Num_Uniquelize(MNum a);
MNum Num_UniquelizeIntPart(MNum a);
MNum Num_UniquelizeFractPart(MNum a);
MNum Num_SetFrom(MNum a, const MNum b);
MReal   Num_ToIEEE754(MNum a);
MInt    Num_ToInt(MNum a);
MString Num_ToString(MNum a, const MInt Base = 10);

#define Num_Release XRef_DecRef

MNum Num_Add(const MNum a, const MNum b);
MNum Num_AddBy(MNum a, const MNum b);
MNum Num_AddBy(MNum a, const MSWord b);
MNum Num_Subtract(const MNum a, const MNum b);
MNum Num_SubtractBy(MNum a, const MNum b);
MNum Num_Divide(const MNum a, const MNum b, const MInt TargetPrecision = -1);
MNum Num_DivideBy(MNum a, const MNum b, const MInt TargetPrecision = -1);
MNum Num_IntDivMod(const MNum a, const MNum b, MNum *premainer);
MNum Num_IntDiv(const MNum a, const MNum b);
MNum Num_IntMod(const MNum a, const MNum b);
MNum Num_DivMod(const MNum a, const MNum b, MNum *premainer);
MNum Num_Div(const MNum a, const MNum b);
MNum Num_Mod(const MNum a, const MNum b);
MNum Num_Multiply(const MNum a, const MNum b);
MNum Num_MultiplyBy(MNum a, const MNum b);
MNum Num_ShiftLeft(const MNum a, const MInt off);
MNum Num_ShiftLeftBy(MNum a, MInt off);
MNum Num_Round(MNum a);

MNum Num_Abs(MNum a);
MNum Num_AbsBy(MNum a);
MNum Num_Neg(MNum a);
MNum Num_NegBy(MNum a);

MBool Num_IsZero(MNum a);
MBool Num_IsInt(MNum a);
MBool Num_IsReal(MNum a);
MBool Num_IsMachineReal(MNum a);
MBool Num_IsMachineInt(MNum a);
MBool Num_IsRational(MNum a);
MBool Num_IsNeg(MNum a);
MBool Num_EvenQ(MNum a);

MHash Num_Hash(MNum a);

MBool   Num_SameQSmall(const MNum a, const short v);
MBool   Num_LT (const MNum a, const MNum b);
MBool   Num_LTE(const MNum a, const MNum b);
MBool   Num_E  (const MNum a, const MNum b);
MBool   Num_GTE(const MNum a, const MNum b);
MBool   Num_GT (const MNum a, const MNum b);
MBool   Num_NE (const MNum a, const MNum b);
MInt    Num_Cmp(const MNum a, const MNum b);
MInt    Num_AbsCmp(const MNum a, const MNum b);
MBool   Num_SameQ(const MNum a, const MNum b);
MBool   Num_UnsameQ(const MNum a, const MNum b);

MReal   Num_Accuracy(MNum a);
MReal   Num_Precision(MNum a);

MNum Num_Power(MNum a, MNum b);
MNum Num_Sqrt(MNum a, MInt precision);

MNum Num_GCD(const MNum v1, const MNum v2);
MNum Num_LCM(const MNum v1, const MNum v2);

void Num_Init();
//void Num_DumpHeap(FILE *file);

#endif
