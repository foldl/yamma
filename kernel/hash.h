#ifndef hash_h
#define hash_h

#include "mtype.h"

typedef unsigned int MHash;

struct MHashItem
{
    MHash Hash1;
    MHash Hash2;
    void *Item;    
};

typedef MHash  (*f_Hash1)(const void *p);
typedef MHash  (*f_Hash2)(const void *p, const MHash Hash1);
typedef void * (*f_HashPreprocess)(const void *p);
typedef MInt   (*f_HashCompare)(const void *p1, const void *p2);
typedef void * (*f_HashComparePreprocess)(const void *p);
typedef void   (*f_HashBeforeDestroy)(void *p);
typedef void   (*f_HashTraverse)(void *para, void *item);
typedef void   (*f_KVHashBeforeDestroy)(void *key, void *value);
typedef void   (*f_KVHashTraverse)(void *para, void *key, void *value);

struct MHashTable
{
    MInt Size;
    MInt SizeIndex;
    MInt EmptyNum;
    MInt Size20;
    MHashItem *Slots;
    f_Hash1 fHash1;
    f_Hash2 fHash2;
    f_HashCompare fCompare;
    f_HashPreprocess fHashPreprocess;
    f_HashComparePreprocess fHashComparePreprocess;
};

MHashTable *MHashTable_Create(const MInt Size, f_Hash1 fHash1, f_Hash2 fHash2, f_HashCompare fCompare);
MHashTable *MHashTable_Duplicate(const MHashTable *Table);
MHashTable *MHashTable_CreateString(const MInt Size);
MHashTable *MHashTable_Insert(MHashTable *Table, void *p);
void *MHashTable_Find(MHashTable *Table, void *p);
void *MHashTable_FindAndClear(MHashTable *Table, void *p);
void *MHashTable_FindOrInsert(MHashTable *Table, void *p);
void *MHashTable_InsertOrReplace(MHashTable *Table, void *p);
void  MHashTable_Destroy(MHashTable *Table, f_HashBeforeDestroy f);
void  MHashTable_Traverse(MHashTable *Table, f_HashTraverse f, void *para);
//MHashTable *MHashTable_InsertIfNo(MHashTable *Table, void *p);

unsigned int ElfHash(const MChar *p, const int Len, unsigned int h = 0);
unsigned int ElfHash2(const MChar *p, const int Len, unsigned int h = 0);

MHashTable *MKVTable_Create(const MInt Size, f_Hash1 fHash1, f_Hash2 fHash2, f_HashCompare fCompare);
MHashTable *MKVTable_CreateString(const MInt Size);
MHashTable *MKVTable_Insert(MHashTable *Table, void *key, void *value);
void *MKVTable_Find(MHashTable *Table, void *key);
void *MKVTable_FindAndClear(MHashTable *Table, void *key);
void *MKVTable_FindOrInsert(MHashTable *Table, void *key, void *value);
void  MKVTable_Destroy(MHashTable *Table, f_KVHashBeforeDestroy f);
void  MKVTable_Traverse(MHashTable *Table, f_KVHashTraverse f, void *para);
//MHashTable *MKVTable_InsertIfNo(MHashTable *Table, void *key, void *value);

#endif
