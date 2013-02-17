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

#include "mm.h"
#include "bif_internal.h"

MSpanIter *CreateMachineSpanIter(MInt start, MInt end, MInt step)
{
    MSpanIter *r = (MSpanIter *)YM_malloc(sizeof(MSpanIter));
    r->istart = start;
    r->iend = end;
    r->istep = step;

    return SpanIterReset(r);
}

MSpanIter *CreateSpanIter(MNum start, MNum end, MNum step)
{
    MSpanIter *r = (MSpanIter *)YM_malloc(sizeof(MSpanIter));
    r->start = Num_Create(start); 
    r->end = Num_Create(end);
    r->step = step ? Num_Create(step) : Num_Create(1);
    return SpanIterReset(r);
}

void       SpanIterDelete(MSpanIter *r)
{
    if (r->start)
    {
        Num_Release(r->start);
        Num_Release(r->end);
        Num_Release(r->step);
        if (r->cur) Num_Release(r->cur);
    }
    else;
    YM_free(r);
}


MSpanIter *SpanIterReset(MSpanIter *r)
{
    r->state = isReset;
    if (r->start)
    {
        if (r->cur) Num_Release(r->cur);
        r->cur = NULL;
        if (Num_IsNeg(r->step))
        {
            if (Num_LT(r->start, r->end))
                r->state = isEnd;
        }
        else if (Num_IsZero(r->step))
            r->state = isEnd;
        else
        {
            if (Num_GT(r->start, r->end))
                r->state = isEnd;
        }

        if (r->state == isEnd)
        {
            if (!Num_IsZero(r->step))
            {
                Num_Release(r->step);
                r->step = Num_Create(0);
            }
        }
    }
    else
    {
        if (r->istep < 0)
        {
            if (r->istart < r->iend) r->state = isEnd;
        }
        else if (r->istep == 0)
            r->state = isEnd;
        else
        {
            if (r->istart > r->iend) r->state = isEnd;
        }

        if (r->state != isEnd)
            r->len = (r->iend - r->istart) / r->istep + 1;
    }
    
    return r;
}

bool       SpanIterMachineNext(MSpanIter *iter, MInt &v)
{
    ASSERT(iter->start == NULL);
    if (iter->state != isEnd)
    {
        if (iter->state != isReset)
        {
            iter->icur += iter->istep;
            if (iter->icur > iter->iend)
            {
               iter->state = isEnd;
               return false;
            }
        }
        else
        {
            iter->icur = iter->istart;
            iter->state = isOngoing;
        }
        v = iter->icur;
        return true;
    }
    else
        return false;
}

MNum       SpanIterNext(MSpanIter *iter)
{
    ASSERT(iter->start != NULL);
    if (iter->state != isEnd)
    {
        if (iter->state != isReset)
        {
            Num_AddBy(iter->cur, iter->step);
            if (Num_GT(iter->cur, iter->end))
            {
               iter->state = isEnd;
               return NULL;
            }
        }
        else
        {
            iter->cur = Num_Create(iter->start);
            iter->state = isOngoing;
        }
        return Num_Create(iter->cur);
    }
    else
        return NULL;
}

MDword SpanIterLen(MSpanIter *iter)
{
    if (iter->len == 0)
    {
        if (iter->start != NULL)
        {
            if (!Num_IsZero(iter->step))
            {
                MNum off = Num_Subtract(iter->end, iter->start);
                MNum q = Num_IntDiv(off, iter->step);
                iter->len = Num_IsMachineInt(q) ? Num_ToInt(q) + 1 : 0;
                Num_Release(off);
                Num_Release(q);
            }
        }
        else;
    }
    
    return iter->len;
}

