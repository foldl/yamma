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
#include "hash.h"
#include "mm.h"
#include "mstr.h"

#define get_hash1(table, p) ((table)->fHashPreprocess ? (table)->fHash1((table)->fHashPreprocess(p)) : (table)->fHash1(p))
#define get_hash2(table, p, h1) ((table)->fHash2 ? ((table)->fHashPreprocess ? (table)->fHash2((table)->fHashPreprocess(p), h1) : (table)->fHash2(p, h1)) : 0)

#define call_fcmp(table, p1, p2) ((table)->fHashComparePreprocess ? (table)->fCompare((table)->fHashComparePreprocess(p1), (table)->fHashComparePreprocess(p2)) : (table)->fCompare(p1, p2))

unsigned int MurmurHash(const unsigned char * data, int len)
{
	const unsigned int m = 0x7fd652ad;
	const int r = 16;

	unsigned int h = 0xdeadbeef;

	while (len >= 4)
	{
		h += *(unsigned int *)data;
		h *= m;
		h ^= h >> r;

		data += 4; len -= 4;
	}

	switch(len)
	{
	case 3:
		h += data[2] << 16;
	case 2:
		h += data[1] << 8;
	case 1:
		h += data[0];
		h *= m;
		h ^= h >> r;
	};

	h *= m;
	h ^= h >> 10;
	h *= m;
	h ^= h >> 17;

	return h;
}

unsigned int ElfHash(const MChar *p, const int Len, unsigned int h)
{
    int i;
    for (i = 0; i < Len; i++)
    {
        h = (h << 4) + (*p++);
        unsigned long g = h & 0xF0000000L;
        if (g) 
            h ^= g >> 16;
        h &= ~g;
    }

  return h;
}

unsigned int ElfHash2(const MChar *p, const int Len, unsigned int h)
{
    int i;
    p += Len - 1;
    for (i = 0; i < Len; i++)
    {
        h = (h << 4) + (*p--);
        unsigned long g = h & 0xF0000000L;
        if (g) 
            h ^= g >> 24;
        h &= ~g;
    }

  return h;
}

static int TableSizeList[] = 
{55,89,144,233,377,610,987,1597,2584,4181,6765,10946,17711,28657,46368,75025,
 121393,196418,317811,514229,832040,1346269,2178309,3524578,5702887,9227465,
 14930352,24157817,39088169,63245986,102334155
};

#define auto_grow(t)    if (t->EmptyNum < t->Size20) MHashTable_Grow(t)
#define set_limit(t)    t->Size20 = (MInt)(t->Size * 0.2)

static void SuggestTableSize(MHashTable *t, const int size)
{
    int i = 0;
    int j = arr_size(TableSizeList) - 1;
    if (size < TableSizeList[0])
    {
        t->Size = TableSizeList[0];
        t->SizeIndex = 0;
        return;
    }

    if (size >= TableSizeList[arr_size(TableSizeList) - 1])
    {
        t->Size = (MInt) (size * 1.2);
        t->SizeIndex = -1;
        return;
    }

    while (i <= j)
    {
        int z = (i + j) / 2;
        if (i == z)
            break;

        if (TableSizeList[z] > size)
            j = z;
        else if (TableSizeList[z] < size) 
            i = z;
    }

    t->SizeIndex = max(i, j);
    t->Size = TableSizeList[t->SizeIndex];
    set_limit(t);
}

MHashTable *MHashTable_Create(const MInt Size, f_Hash1 fHash1, f_Hash2 fHash2, f_HashCompare fCompare)
{
    MHashTable *r = (MHashTable *)YM_malloc(sizeof(MHashTable));
    r->fHash1 = fHash1;
    r->fHash2 = fHash2;
    r->fCompare = fCompare;
    SuggestTableSize(r, Size);
    r->Slots = (MHashItem *)YM_malloc(r->Size * sizeof(MHashItem));
    r->EmptyNum = r->Size;
    set_limit(r);
    return r;
}

MHashTable *MHashTable_Duplicate(const MHashTable *Table)
{
    MHashTable *r = (MHashTable *)YM_malloc(sizeof(MHashTable));
    MHashItem *p = (MHashItem *)YM_malloc(Table->Size * sizeof(MHashItem));
    memcpy(r, Table, sizeof(*Table));
    memcpy(p, Table->Slots, Table->Size * sizeof(MHashItem));
    r->Slots = p;
    return r;
}

static MHash HashString1(MString s)
{
    return ElfHash(MString_GetData(s), MString_Len(s));
}

MHashTable *MHashTable_CreateString(const MInt Size)
{
    return MHashTable_Create(Size, (f_Hash1)HashString1, NULL, (f_HashCompare)(MString_Compare));
}

static MHash MHashTable_GetFreePos(const MHashItem *Slots, const MDword Size, const MHash hash1, const MHash hash2)
{
    MHash hash = hash1 % Size;
    if ((Slots[hash].Item != NULL) && (hash2 > 0))
        hash = hash2 % Size;
    if (Slots[hash].Item == NULL)
        return hash;
    else
    {   
        // printf("confilict\n");
        MHash newhash = hash;
        while (newhash + 1 < Size)
        {
            newhash++;
            if (NULL == Slots[newhash].Item)
                break;
        }

        if (Slots[newhash].Item != NULL)
        {
            newhash = 0;
            do
            {
                if (NULL == Slots[newhash].Item)
                    break;
                newhash++;
            } while (newhash < hash);
        }

        // this will always be OK now
        return newhash;
    }
}

static MHashTable *MHashTable_Grow(MHashTable *t)
{
    MHashItem* OldSlots = t->Slots;
    MInt OldSize = t->Size;
    int i;

    if (t->SizeIndex >= 0)
    {
        t->SizeIndex++;
        if ((MDword)(t->SizeIndex) < arr_size(TableSizeList))
            t->Size = TableSizeList[t->SizeIndex];
        else
            t->SizeIndex = -1;
    };

    if (t->SizeIndex < 0)
        t->Size = MInt(t->Size * 1.4);

    set_limit(t);
    t->Slots = (MHashItem *)YM_malloc(t->Size * sizeof(MHashItem));
    t->EmptyNum += t->Size - OldSize;
    set_limit(t);

    for (i = 0; i < OldSize; i++)
    {
        if (OldSlots[i].Item)
        {
            MHash newPos = MHashTable_GetFreePos(t->Slots, t->Size, OldSlots[i].Hash1, OldSlots[i].Hash2);
            t->Slots[newPos] = OldSlots[i];
        }
    }
    YM_free(OldSlots);

    return t;
}

MHashTable *MHashTable_Insert(MHashTable *Table, void *p)
{
    auto_grow(Table);
    Table->EmptyNum--;

    MHash hash1 = get_hash1(Table, p);
    MHash hash2 = get_hash2(Table, p, hash1);
    MHash hash = MHashTable_GetFreePos(Table->Slots, Table->Size, hash1, hash2);

    Table->Slots[hash].Hash1 = hash1;
    Table->Slots[hash].Hash2 = hash2;
    Table->Slots[hash].Item = p;

    return Table;
}

static MHashItem *MHashTable_FindItem(MHashTable *Table, void *p, MHash &hash1, MHash &hash2)
{
    MHash hash;
    hash1 = get_hash1(Table, p);
    hash2 = 0;
    hash = hash1 % Table->Size;
    MHashItem *Slots = Table->Slots;
    if (Table->Slots[hash].Item == NULL)
        return &Table->Slots[hash];
    hash2 = get_hash2(Table, p, hash1);
    if (Slots[hash].Hash1 == hash1)
    {
        if ((Slots[hash].Hash2 == hash2) && (call_fcmp(Table, Slots[hash].Item, p) == 0))
            return &Slots[hash];
    }

    if (hash2 > 0)
    {
        hash = hash2 % Table->Size;
        if (Slots[hash].Item == NULL)
            return &Slots[hash];

        if (Table->fCompare(Slots[hash].Item, p) == 0)
            return &Slots[hash];
    }

    {   
        MHash newhash = hash;
        while (newhash + 1 < (MDword)Table->Size)
        {
            newhash++;
            if (NULL == Slots[newhash].Item)
                return &Slots[newhash];
            if (Table->fCompare(Slots[newhash].Item, p) == 0)
                return &Slots[newhash];
        }

        newhash = 0;
        do
        {
            if (NULL == Slots[newhash].Item)
                return &Slots[newhash];
            if (Table->fCompare(Slots[newhash].Item, p) == 0)
                return &Slots[newhash];
            newhash++;
        } while (newhash < hash);
    }

    return NULL;
}

void *MHashTable_FindAndClear(MHashTable *Table, void *p)
{
    MHash hash1, hash2;
    MHashItem *Slot = MHashTable_FindItem(Table, p, hash1, hash2);
    if (Slot)
    {
        void *r = Slot->Item;
        Slot->Item = NULL;
        return r;
    }
    return NULL;
}

void *MHashTable_Find(MHashTable *Table, void *p)
{
    MHash hash1, hash2;
    MHashItem *Slot = MHashTable_FindItem(Table, p, hash1, hash2);
    return Slot ? Slot->Item : NULL;
}

void *MHashTable_InsertOrReplace(MHashTable *Table, void *p)
{
    MHash hash1, hash2;
    void *r;
    MHashItem *Slot;
    auto_grow(Table);
    Slot = MHashTable_FindItem(Table, p, hash1, hash2);
    ASSERT(Slot != NULL);
    r = Slot->Item;
    Slot->Item = p;
    Slot->Hash1 = hash1;
    Slot->Hash2 = hash2;
    if (r == NULL) Table->EmptyNum--;
    return r;
}

void *MHashTable_FindOrInsert(MHashTable *Table, void *p)
{
    MHash hash1, hash2;
    auto_grow(Table);
    MHashItem *Slot = MHashTable_FindItem(Table, p, hash1, hash2);
    ASSERT(Slot != NULL);
    if (NULL == Slot->Item)
    {
        Slot->Item = p;
        Slot->Hash1 = hash1;
        Slot->Hash2 = hash2;
        Table->EmptyNum--;
    }
    else;
    return Slot->Item;
}

void MHashTable_Destroy(MHashTable *Table, f_HashBeforeDestroy f)
{
    int i;
    if (f)
        for (i = 0; i < Table->Size; i++)
        {
            if (Table->Slots[i].Item)
                f(Table->Slots[i].Item);
        }
    else;

    YM_free(Table->Slots);
    YM_free(Table);
}

void  MHashTable_Traverse(MHashTable *Table, f_HashTraverse f, void *para)
{
    int i;
    if (f)
    {
        for (i = 0; i < Table->Size; i++)
        {
            if (Table->Slots[i].Item)
                f(para, Table->Slots[i].Item);
        }
    }
}

struct MKVTableItem
{
    void *key;
    void *value;
};

static void * KVHashPP(MKVTableItem *p)
{
    return p->key;
}

MHashTable *MKVTable_Create(const MInt Size, f_Hash1 fHash1, f_Hash2 fHash2, f_HashCompare fCompare)
{
    MHashTable *r = MHashTable_Create(Size, fHash1, fHash2, fCompare);
    r->fHashPreprocess = (f_HashPreprocess)(KVHashPP);
    r->fHashComparePreprocess = (f_HashComparePreprocess)(KVHashPP);
    return r;
}

MHashTable *MKVTable_CreateString(const MInt Size)
{
    return MKVTable_Create(Size, (f_Hash1)HashString1, NULL, (f_HashCompare)(MString_Compare));
}

MHashTable *MKVTable_Insert(MHashTable *Table, void *key, void *value)
{
    MKVTableItem *item = (MKVTableItem *)YM_malloc(sizeof(MKVTableItem));
    item->key = key;
    item->value = value;
    return MHashTable_Insert(Table, item);
}

//MHashTable *MKVTable_InsertIfNo(MHashTable *Table, void *key, void *value)
//{
//    MKVTableItem *item = (MKVTableItem *)YM_malloc(sizeof(MKVTableItem));
//    item->key;
//    item->value;
//    return MHashTable_InsertIfNo(Table, item);
//}

void *MKVTable_Find(MHashTable *Table, void *key)
{
    MKVTableItem item;
    MKVTableItem *r;
    item.key = key;
    item.value = NULL;
    r = (MKVTableItem*)(MHashTable_Find(Table, &item));
    return r ? r->value : NULL;
}

void *MKVTable_FindAndClear(MHashTable *Table, void *key)
{
    MKVTableItem item;
    MKVTableItem *r;
    item.key = key;
    item.value = NULL;
    r = (MKVTableItem*)(MHashTable_FindAndClear(Table, &item));
    if (r)
    {
        void *rr = r->value;
        YM_free(rr);
        return rr;
    }
    else
        return NULL;
}

void *MKVTable_FindOrInsert(MHashTable *Table, void *key, void *value)
{
    MKVTableItem *item = (MKVTableItem *)YM_malloc(sizeof(MKVTableItem));
    MKVTableItem *r;
    item->key = key;
    item->value = value;
    r = (MKVTableItem *)MHashTable_FindOrInsert(Table, item);
    if (r != item)
    {
        YM_free(item);
        return r->value;
    }
    else
        return NULL;
}

void  MKVTable_Destroy(MHashTable *Table, f_KVHashBeforeDestroy f)
{
    int i;
    
    if (f)
        for (i = 0; i < Table->Size; i++)
        {
            if (Table->Slots[i].Item)
            {
                MKVTableItem *r = (MKVTableItem *)Table->Slots[i].Item;
                f(r->key, r->value);
                YM_free(r);
            }
        }
    else
        for (i = 0; i < Table->Size; i++)
        {
            if (Table->Slots[i].Item)
                YM_free(Table->Slots[i].Item);
        }
    
    YM_free(Table->Slots);
    YM_free(Table);
}

struct KVTraverseItem
{
    f_KVHashTraverse f;
    void *para;
};

static void KVTable_Traverse(KVTraverseItem *para, MKVTableItem *item)
{
    para->f(para->para, item->key, item->value);
}

void  MKVTable_Traverse(MHashTable *Table, f_KVHashTraverse f, void *para)
{
    KVTraverseItem item;
    item.f = f;
    item.para = para;
    MHashTable_Traverse(Table, (f_HashTraverse)(KVTable_Traverse), &item);
}
