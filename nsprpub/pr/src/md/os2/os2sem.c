/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/*
 * OS/2-specific semaphore handling code.
 *
 */

#include "primpl.h"


void
_PR_MD_NEW_SEM(_MDSemaphore *md, PRUintn value)
{
   int rv;

    /* Our Sems don't support a value > 1 */
    PR_ASSERT(value <= 1);

    rv = DosCreateEventSem(NULL, &md->sem, 0, 0);
    PR_ASSERT(rv == NO_ERROR);
}

void
_PR_MD_DESTROY_SEM(_MDSemaphore *md)
{
   int rv;
   rv = DosCloseEventSem(md->sem);
   PR_ASSERT(rv == NO_ERROR);

}

PRStatus
_PR_MD_TIMED_WAIT_SEM(_MDSemaphore *md, PRIntervalTime ticks)
{
    int rv;
    rv = DosWaitEventSem(md->sem, PR_IntervalToMilliseconds(ticks));

    if (rv == NO_ERROR)
        return PR_SUCCESS;
    else
        return PR_FAILURE;
}

PRStatus
_PR_MD_WAIT_SEM(_MDSemaphore *md)
{
    return _PR_MD_TIMED_WAIT_SEM(md, PR_INTERVAL_NO_TIMEOUT);
}

void
_PR_MD_POST_SEM(_MDSemaphore *md)
{
   int rv;
   rv = DosPostEventSem(md->sem);
   PR_ASSERT(rv == NO_ERROR); 
}


