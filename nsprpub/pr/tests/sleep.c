/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is the Netscape Portable Runtime (NSPR).
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998-2000
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

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

#if defined (XP_OS2_VACPP)
#define INCL_DOSPROCESS
#include <os2.h>
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
    if (didit < 5) rv = 1;
}

PRIntn main ()
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
    if (labs(elapsed - 3000000UL) > onePercent) rv = 1;

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
    if (labs(elapsed - 3000000UL) > onePercent) rv = 1;

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
    if (labs(elapsed - 3000000UL) > onePercent) rv = 1;
    fprintf(stderr, "%s\n", (0 == rv) ? "PASSED" : "FAILED");
    return rv;
}

#else /* defined(XP_UNIX) */

PRIntn main()
{
	return 2;
}

#endif /*  defined(XP_UNIX) */

