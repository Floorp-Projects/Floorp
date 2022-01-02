/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/*
**  prolock.c -- NSPR Ordered Lock
**
**  Implement the API defined in prolock.h
**
*/
#include "prolock.h"
#include "prlog.h"
#include "prerror.h"

PR_IMPLEMENT(PROrderedLock *)
PR_CreateOrderedLock(
    PRInt32 order,
    const char *name
)
{
    PR_NOT_REACHED("Not implemented"); /* Not implemented yet */
    PR_SetError(PR_NOT_IMPLEMENTED_ERROR, 0);
    return NULL;
} /*  end PR_CreateOrderedLock() */


PR_IMPLEMENT(void)
PR_DestroyOrderedLock(
    PROrderedLock *lock
)
{
    PR_NOT_REACHED("Not implemented"); /* Not implemented yet */
    PR_SetError(PR_NOT_IMPLEMENTED_ERROR, 0);
} /*  end PR_DestroyOrderedLock() */


PR_IMPLEMENT(void)
PR_LockOrderedLock(
    PROrderedLock *lock
)
{
    PR_NOT_REACHED("Not implemented"); /* Not implemented yet */
    PR_SetError(PR_NOT_IMPLEMENTED_ERROR, 0);
} /*  end PR_LockOrderedLock() */


PR_IMPLEMENT(PRStatus)
PR_UnlockOrderedLock(
    PROrderedLock *lock
)
{
    PR_NOT_REACHED("Not implemented"); /* Not implemented yet */
    PR_SetError(PR_NOT_IMPLEMENTED_ERROR, 0);
    return PR_FAILURE;
} /*  end PR_UnlockOrderedLock() */
