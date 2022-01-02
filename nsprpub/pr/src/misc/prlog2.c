/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "prbit.h"

/*
** Compute the log of the least power of 2 greater than or equal to n
*/
PR_IMPLEMENT(PRIntn) PR_CeilingLog2(PRUint32 n)
{
    PRIntn log2;
    PR_CEILING_LOG2(log2, n);
    return log2;
}

/*
** Compute the log of the greatest power of 2 less than or equal to n.
** This really just finds the highest set bit in the word.
*/
PR_IMPLEMENT(PRIntn) PR_FloorLog2(PRUint32 n)
{
    PRIntn log2;
    PR_FLOOR_LOG2(log2, n);
    return log2;
}
