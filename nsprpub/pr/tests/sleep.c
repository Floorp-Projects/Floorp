/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nspr.h"

#if defined(XP_UNIX) || defined(XP_OS2)

#include <stdio.h>

#ifndef XP_OS2
#include <unistd.h>
#endif
#include <sys/time.h>

#if defined(HAVE_SVID_GETTOD)
#define GTOD(_a) gettimeofday(_a)
#else
#define GTOD(_a) gettimeofday((_a), NULL)
#endif

static PRIntn rv = 0;

static void Other(void *unused)
{
    PRIntn didit = 0;
    while (PR_SUCCESS == PR_Sleep(PR_MillisecondsToInterval(250)))
    {
        fprintf(stderr, ".");
        didit += 1;
    }
    if (didit < 5) {
        rv = 1;
    }
}

int main(int argc, char **argv)
{
    PRUint32 elapsed;
    PRThread *thread;
    struct timeval timein, timeout;
    PRInt32 onePercent = 3000000UL / 100UL;

    fprintf (stderr, "First sleep will sleep 3 seconds.\n");
    fprintf (stderr, "   sleep 1 begin\n");
    (void)GTOD(&timein);
    sleep (3);
    (void)GTOD(&timeout);
    fprintf (stderr, "   sleep 1 end\n");
    elapsed = 1000000UL * (timeout.tv_sec - timein.tv_sec);
    elapsed += (timeout.tv_usec - timein.tv_usec);
    fprintf(stderr, "elapsed %u usecs\n", elapsed);
    if (labs(elapsed - 3000000UL) > onePercent) {
        rv = 1;
    }

    PR_Init (PR_USER_THREAD, PR_PRIORITY_NORMAL, 100);
    PR_STDIO_INIT();

    fprintf (stderr, "Second sleep should do the same (does it?).\n");
    fprintf (stderr, "   sleep 2 begin\n");
    (void)GTOD(&timein);
    sleep (3);
    (void)GTOD(&timeout);
    fprintf (stderr, "   sleep 2 end\n");
    elapsed = 1000000UL * (timeout.tv_sec - timein.tv_sec);
    elapsed += (timeout.tv_usec - timein.tv_usec);
    fprintf(stderr, "elapsed %u usecs\n", elapsed);
    if (labs(elapsed - 3000000UL) > onePercent) {
        rv = 1;
    }

    fprintf (stderr, "What happens to other threads?\n");
    fprintf (stderr, "You should see dots every quarter second.\n");
    fprintf (stderr, "If you don't, you're probably running on classic NSPR.\n");
    thread = PR_CreateThread(
                 PR_USER_THREAD, Other, NULL, PR_PRIORITY_NORMAL,
                 PR_LOCAL_THREAD, PR_JOINABLE_THREAD, 0);
    fprintf (stderr, "   sleep 2 begin\n");
    (void)GTOD(&timein);
    sleep (3);
    (void)GTOD(&timeout);
    fprintf (stderr, "   sleep 2 end\n");
    PR_Interrupt(thread);
    PR_JoinThread(thread);
    elapsed = 1000000UL * (timeout.tv_sec - timein.tv_sec);
    elapsed += (timeout.tv_usec - timein.tv_usec);
    fprintf(stderr, "elapsed %u usecs\n", elapsed);
    if (labs(elapsed - 3000000UL) > onePercent) {
        rv = 1;
    }
    fprintf(stderr, "%s\n", (0 == rv) ? "PASSED" : "FAILED");
    return rv;
}

#else /* defined(XP_UNIX) */

PRIntn main()
{
    return 2;
}

#endif /*  defined(XP_UNIX) */

