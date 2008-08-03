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

/*
** File:            switch.c
** Description:     trying to time context switches
*/

#include "prinit.h"
#include "prcvar.h"
#include "prmem.h"
#include "prinrval.h"
#include "prlock.h"
#include "prlog.h"
#include "prthread.h"
#include "prprf.h"

#include "plerror.h"
#include "plgetopt.h"

#if defined(XP_MAC)
#include "pprio.h"
#define printf PR_LogPrint
#else
#include "private/pprio.h"
#endif

#include <stdlib.h>

#define INNER_LOOPS 100
#define DEFAULT_LOOPS 100
#define DEFAULT_THREADS 10

static PRFileDesc *debug_out = NULL;
static PRBool debug_mode = PR_FALSE, verbosity = PR_FALSE, failed = PR_FALSE;

typedef struct Shared
{
    PRLock *ml;
    PRCondVar *cv;
    PRBool twiddle;
    PRThread *thread;
    struct Shared *next;
} Shared;

static void Help(void)
{
    debug_out = PR_STDOUT;

    PR_fprintf(
		debug_out, "Usage: >./switch [-c n] [-t n] [-d] [-v] [-G] [-C n]\n");
    PR_fprintf(
		debug_out, "-c n\tloops at thread level (default: %d)\n", DEFAULT_LOOPS);
    PR_fprintf(
		debug_out, "-t n\tnumber of threads (default: %d)\n", DEFAULT_THREADS);
    PR_fprintf(debug_out, "-d\tturn on debugging output (default: FALSE)\n");
    PR_fprintf(debug_out, "-v\tturn on verbose output (default: FALSE)\n");
    PR_fprintf(debug_out, "-G\tglobal threads only (default: FALSE)\n");
    PR_fprintf(debug_out, "-C n\tconcurrency setting (default: 1)\n");
}  /* Help */

static void PR_CALLBACK Notified(void *arg)
{
    Shared *shared = (Shared*)arg;
    PRStatus status = PR_SUCCESS;
    while (PR_SUCCESS == status)
    {
        PR_Lock(shared->ml);
        while (shared->twiddle && (PR_SUCCESS == status))
            status = PR_WaitCondVar(shared->cv, PR_INTERVAL_NO_TIMEOUT);
		if (verbosity) PR_fprintf(debug_out, "+");
        shared->twiddle = PR_TRUE;
        shared->next->twiddle = PR_FALSE;
        PR_NotifyCondVar(shared->next->cv);
        PR_Unlock(shared->ml);
    }
}  /* Notified */

static Shared home;
PRIntn PR_CALLBACK Switch(PRIntn argc, char **argv)
{
	PLOptStatus os;
    PRStatus status;
    PRBool help = PR_FALSE;
    PRUintn concurrency = 1;
    Shared *shared, *link;
    PRIntervalTime timein, timeout;
    PRThreadScope thread_scope = PR_LOCAL_THREAD;
    PRUintn thread_count, inner_count, loop_count, average;
    PRUintn thread_limit = DEFAULT_THREADS, loop_limit = DEFAULT_LOOPS;
	PLOptState *opt = PL_CreateOptState(argc, argv, "hdvc:t:C:G");
	while (PL_OPT_EOL != (os = PL_GetNextOpt(opt)))
    {
		if (PL_OPT_BAD == os) continue;
        switch (opt->option)
        {
        case 'v':  /* verbose mode */
			verbosity = PR_TRUE;
        case 'd':  /* debug mode */
			debug_mode = PR_TRUE;
            break;
        case 'c':  /* loop counter */
			loop_limit = atoi(opt->value);
            break;
        case 't':  /* thread limit */
			thread_limit = atoi(opt->value);
            break;
        case 'C':  /* Concurrency limit */
			concurrency = atoi(opt->value);
            break;
        case 'G':  /* global threads only */
			thread_scope = PR_GLOBAL_THREAD;
            break;
        case 'h':  /* help message */
			Help();
			help = PR_TRUE;
            break;
         default:
            break;
        }
    }
	PL_DestroyOptState(opt);

    if (help) return -1;

	if (PR_TRUE == debug_mode)
	{
		debug_out = PR_STDOUT;
		PR_fprintf(debug_out, "Test parameters\n");
		PR_fprintf(debug_out, "\tThreads involved: %d\n", thread_limit);
		PR_fprintf(debug_out, "\tIteration limit: %d\n", loop_limit);
		PR_fprintf(debug_out, "\tConcurrency: %d\n", concurrency);
		PR_fprintf(
			debug_out, "\tThread type: %s\n",
			(PR_GLOBAL_THREAD == thread_scope) ? "GLOBAL" : "LOCAL");
	}

    PR_SetConcurrency(concurrency);

    link = &home;
    home.ml = PR_NewLock();
    home.cv = PR_NewCondVar(home.ml);
    home.twiddle = PR_FALSE;
    home.next = NULL;

    timeout = 0;

    for (thread_count = 1; thread_count <= thread_limit; ++thread_count)
    {
        shared = PR_NEWZAP(Shared);

        shared->ml = home.ml;
        shared->cv = PR_NewCondVar(home.ml);
        shared->twiddle = PR_TRUE;
        shared->next = link;
        link = shared;

        shared->thread = PR_CreateThread(
            PR_USER_THREAD, Notified, shared,
            PR_PRIORITY_HIGH, thread_scope,
            PR_JOINABLE_THREAD, 0);
        PR_ASSERT(shared->thread != NULL);
        if (NULL == shared->thread)
            failed = PR_TRUE;
	}

    for (loop_count = 1; loop_count <= loop_limit; ++loop_count)
    {
		timein = PR_IntervalNow();
		for (inner_count = 0; inner_count < INNER_LOOPS; ++inner_count)
		{
			PR_Lock(home.ml);
			home.twiddle = PR_TRUE;
			shared->twiddle = PR_FALSE;
			PR_NotifyCondVar(shared->cv);
			while (home.twiddle)
            {
				status = PR_WaitCondVar(home.cv, PR_INTERVAL_NO_TIMEOUT);
				if (PR_FAILURE == status)
				    failed = PR_TRUE;
            }
			PR_Unlock(home.ml);
		}
		timeout += (PR_IntervalNow() - timein);
	}

	if (debug_mode)
	{
		average = PR_IntervalToMicroseconds(timeout)
			/ (INNER_LOOPS * loop_limit * thread_count);
		PR_fprintf(
			debug_out, "Average switch times %d usecs for %d threads\n",
            average, thread_limit);
	}

    link = shared;
    for (thread_count = 1; thread_count <= thread_limit; ++thread_count)
    {
        if (&home == link) break;
        status = PR_Interrupt(link->thread);
		if (PR_SUCCESS != status)
        {
            failed = PR_TRUE;
            if (debug_mode)
			    PL_FPrintError(debug_out, "Failed to interrupt");
        }
		link = link->next; 
    }

    for (thread_count = 1; thread_count <= thread_limit; ++thread_count)
    {
        link = shared->next;
        status = PR_JoinThread(shared->thread);
		if (PR_SUCCESS != status)
		{
            failed = PR_TRUE;
            if (debug_mode)
			    PL_FPrintError(debug_out, "Failed to join");
        }
        PR_DestroyCondVar(shared->cv);
        PR_DELETE(shared);
        if (&home == link) break;
        shared = link;
    }
    PR_DestroyCondVar(home.cv);
    PR_DestroyLock(home.ml);

    PR_fprintf(PR_STDOUT, ((failed) ? "FAILED\n" : "PASSED\n"));
    return ((failed) ? 1 : 0);
}  /* Switch */

PRIntn main(PRIntn argc, char **argv)
{
    PRIntn result;
    PR_STDIO_INIT();
    result = PR_Initialize(Switch, argc, argv, 0);
    return result;
}  /* main */

/* switch.c */
