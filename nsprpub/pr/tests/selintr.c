/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/*
 * Test whether classic NSPR's select() wrapper properly blocks
 * the periodic SIGALRM clocks.  On some platforms (such as
 * HP-UX and SINIX) an interrupted select() system call is
 * restarted with the originally specified timeout, ignoring
 * the time that has elapsed.  If a select() call is interrupted
 * repeatedly, it will never time out.  (See Bugzilla bug #39674.)
 */

#if !defined(XP_UNIX)

/*
 * This test is applicable to Unix only.
 */

int main()
{
    return 0;
}

#else /* XP_UNIX */

#include "nspr.h"

#include <sys/time.h>
#include <stdio.h>
#ifdef SYMBIAN
#include <sys/select.h>
#endif

int main(int argc, char **argv)
{
    struct timeval timeout;
    int rv;

    PR_SetError(0, 0);  /* force NSPR to initialize */
    PR_EnableClockInterrupts();

    /* 2 seconds timeout */
    timeout.tv_sec = 2;
    timeout.tv_usec = 0;
    rv = select(1, NULL, NULL, NULL, &timeout);
    printf("select returned %d\n", rv);
    return 0;
}

#endif /* XP_UNIX */
