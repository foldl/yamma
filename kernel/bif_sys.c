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

#include <time.h>
#include <math.h>
#include <string.h>
#include "bif.h"
#include "eval.h"
#include "expr.h"
#include "sys_sym.h"
#include "bif_internal.h"

def_bif(Timing)
{
    if (Eval_GetCallTag(_Context) > 0)
    {
        MSequence *ps = MSequence_Create(2);
        clock_t now = clock();
        clock_t old = Eval_GetCallTag(_Context);
        double diff = 0.0;
        if (now >= old)
            diff = (double)(now - old) / CLOCKS_PER_SEC;
        else
        {
            old -= now;
            diff = pow(2.0, sizeof(now) * 8) - old;
        }
        MSequence_SetAtX(ps, 0, MExpr_CreateMachineReal(diff));
        MSequence_SetAtX(ps, 1, Eval_GetCallExpr(_Context));
        return MExpr_CreateHeadSeqX(sym_List, ps);
    }
    else
    {
        MExpr r = verify_arity(seq, sym_Timing, 1);
        clock_t now = clock();
        CASSERT(sizeof(MDword) >= sizeof(clock_t));
        if (r) return r;
        RET_Eval_RetCCEvalX(MSequence_EleAt(seq, 0), (MDword)(now));
    }
}


