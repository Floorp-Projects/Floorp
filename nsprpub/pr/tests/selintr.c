/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/*
 * The contents of this file are subject to the Netscape Public License
 * Version 1.1 (the "NPL"); you may not use this file except in
 * compliance with the NPL.  You may obtain a copy of the NPL at
 * http://www.mozilla.org/NPL/
 * 
 * Software distributed under the NPL is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the NPL
 * for the specific language governing rights and limitations under the
 * NPL.
 * 
 * The Initial Developer of this code under the NPL is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 2000 Netscape Communications Corporation.  All Rights
 * Reserved.
 */

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

int main()
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
