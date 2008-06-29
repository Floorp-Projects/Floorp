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

#include "prio.h"
#include "prprf.h"
#include "prinit.h"
#include "prthread.h"
#include "prproces.h"
#include "prinrval.h"

#include "plgetopt.h"

#include <stdlib.h>

static PRInt32 dally = 0;
static PRFileDesc *err = NULL;
static PRBool verbose = PR_FALSE, force = PR_FALSE;

static void Help(void)
{
    PR_fprintf(err, "Usage: [-t s] [-h]\n");
    PR_fprintf(err, "\t-d   Verbose output              (default: FALSE)\n");
    PR_fprintf(err, "\t-x   Forced termination          (default: FALSE)\n");
    PR_fprintf(err, "\t-t   Time for thread to block    (default: 10 seconds)\n");
    PR_fprintf(err, "\t-h   This message and nothing else\n");
}  /* Help */

static void Dull(void *arg)
{
    PR_Sleep(PR_SecondsToInterval(dally));
    if (verbose && force)
        PR_fprintf(err, "If you see this, the test failed\n");
}  /* Dull */

static PRIntn PR_CALLBACK RealMain(PRIntn argc, char **argv)
{
    PLOptStatus os;
    PLOptState *opt = PL_CreateOptState(argc, argv, "ht:dx");

    err = PR_GetSpecialFD(PR_StandardError);

    while (PL_OPT_EOL != (os = PL_GetNextOpt(opt)))
    {
        if (PL_OPT_BAD == os) continue;
        switch (opt->option)
        {
        case 'd':  /* verbosity */
            verbose = PR_TRUE;
            break;
        case 'x':  /* force exit */
            force = PR_TRUE;
            break;
        case 't':  /* seconds to dally in child */
            dally = atoi(opt->value);
            break;
        case 'h':  /* user wants some guidance */
        default:
            Help();  /* so give him an earful */
            return 2;  /* but not a lot else */
        }
    }
    PL_DestroyOptState(opt);

    if (0 == dally) dally = 10;

	/*
	 * Create LOCAL and GLOBAL threads
	 */
    (void)PR_CreateThread(
        PR_USER_THREAD, Dull, NULL, PR_PRIORITY_NORMAL,
			  PR_LOCAL_THREAD, PR_UNJOINABLE_THREAD, 0);

    (void)PR_CreateThread(
        PR_USER_THREAD, Dull, NULL, PR_PRIORITY_NORMAL,
			  PR_GLOBAL_THREAD, PR_UNJOINABLE_THREAD, 0);

    if (verbose)
        PR_fprintf(
            err, "Main is exiting now. Program should exit %s.\n",
            (force) ? "immediately" : "after child dally time");

    if (force)
    {
        PR_ProcessExit(0);
        if (verbose)
        {
            PR_fprintf(err, "You should not have gotten here.\n");
            return 1;
        }
    }
    return 0;
        
}


PRIntn main(PRIntn argc, char *argv[])
{
    PRIntn rv;
    
    PR_STDIO_INIT();
    rv = PR_Initialize(RealMain, argc, argv, 0);
    return rv;
}  /* main */
