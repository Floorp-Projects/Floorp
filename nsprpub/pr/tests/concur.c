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
** File:            concur.c 
** Description:     test of adding and removing concurrency options
*/

#include "prcvar.h"
#include "prinit.h"
#include "prinrval.h"
#include "prlock.h"
#include "prprf.h"
#include "prmem.h"
#include "prlog.h"

#include "plgetopt.h"

#if defined(XP_MAC)
#include "pprio.h"
#else
#include "private/pprio.h"
#endif

#include <stdlib.h>

#define DEFAULT_RANGE 10
#define DEFAULT_LOOPS 100

static PRThreadScope thread_scope = PR_LOCAL_THREAD;

typedef struct Context
{
    PRLock *ml;
    PRCondVar *cv;
    PRIntn want, have;
} Context;


/*
** Make the instance of 'context' static (not on the stack)
** for Win16 threads
*/
static Context context = {NULL, NULL, 0, 0};

static void PR_CALLBACK Dull(void *arg)
{
    Context *context = (Context*)arg;
    PR_Lock(context->ml);
    context->have += 1;
    while (context->want >= context->have)
        PR_WaitCondVar(context->cv, PR_INTERVAL_NO_TIMEOUT);
    context->have -= 1;
    PR_Unlock(context->ml);
}  /* Dull */

PRIntn PR_CALLBACK Concur(PRIntn argc, char **argv)
{
    PRUintn cpus;
	PLOptStatus os;
	PRThread **threads;
    PRBool debug = PR_FALSE;
    PRUintn range = DEFAULT_RANGE;
	PRStatus rc;
    PRUintn cnt;
    PRUintn loops = DEFAULT_LOOPS;
	PRIntervalTime hundredMills = PR_MillisecondsToInterval(100);
	PLOptState *opt = PL_CreateOptState(argc, argv, "Gdl:r:");
	while (PL_OPT_EOL != (os = PL_GetNextOpt(opt)))
    {
		if (PL_OPT_BAD == os) continue;
        switch (opt->option)
        {
        case 'G':  /* GLOBAL threads */
			thread_scope = PR_GLOBAL_THREAD;
            break;
        case 'd':  /* debug mode */
			debug = PR_TRUE;
            break;
        case 'r':  /* range limit */
			range = atoi(opt->value);
            break;
        case 'l':  /* loop counter */
			loops = atoi(opt->value);
            break;
         default:
            break;
        }
    }
	PL_DestroyOptState(opt);

    if (0 == range) range = DEFAULT_RANGE;
    if (0 == loops) loops = DEFAULT_LOOPS;

    context.ml = PR_NewLock();
    context.cv = PR_NewCondVar(context.ml);

    if (debug)
        PR_fprintf(
            PR_STDERR, "Testing with %d CPUs and %d interations\n", range, loops);

	threads = (PRThread**) PR_CALLOC(sizeof(PRThread*) * range);
    while (--loops > 0)
    {
        for (cpus = 1; cpus <= range; ++cpus)
        {
            PR_SetConcurrency(cpus);
            context.want = cpus;

            threads[cpus - 1] = PR_CreateThread(
                PR_USER_THREAD, Dull, &context, PR_PRIORITY_NORMAL,
				      thread_scope, PR_JOINABLE_THREAD, 0);
        }

        PR_Sleep(hundredMills);

        for (cpus = range; cpus > 0; cpus--)
        {
            PR_SetConcurrency(cpus);
            context.want = cpus - 1;

            PR_Lock(context.ml);
            PR_NotifyCondVar(context.cv);
            PR_Unlock(context.ml);
        }
		for(cnt = 0; cnt < range; cnt++) {
			rc = PR_JoinThread(threads[cnt]);
			PR_ASSERT(rc == PR_SUCCESS);
		}
    }

    
    if (debug)
        PR_fprintf(
            PR_STDERR, "Waiting for %d thread(s) to exit\n", context.have);

    while (context.have > 0) PR_Sleep(hundredMills);

    if (debug)
        PR_fprintf(
            PR_STDERR, "Finished [want: %d, have: %d]\n",
            context.want, context.have);

    PR_DestroyLock(context.ml);
    PR_DestroyCondVar(context.cv);
    PR_DELETE(threads);

    PR_fprintf(PR_STDERR, "PASSED\n");

    return 0;
} /* Concur */

PRIntn main(PRIntn argc, char **argv)
{
    PR_STDIO_INIT();
    return PR_Initialize(Concur, argc, argv, 0);
}  /* main */

/* concur.c */
