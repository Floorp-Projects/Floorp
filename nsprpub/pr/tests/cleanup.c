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
 * Copyright (C) 1998 Netscape Communications Corporation.  All Rights
 * Reserved.
 */

#include "prprf.h"
#include "prio.h"
#include "prinit.h"
#include "prthread.h"
#include "prinrval.h"

#include "plgetopt.h"

#include <stdlib.h>

static void PR_CALLBACK Thread(void *sleep)
{
    PR_Sleep(PR_SecondsToInterval((PRUint32)sleep));
    printf("Thread exiting\n");
}

static void Help(void)
{
    PRFileDesc *err = PR_GetSpecialFD(PR_StandardError);
    PR_fprintf(err, "Cleanup usage: [-g] [-s n] [-t n] [-c n] [-h]\n");
    PR_fprintf(err, "\t-c   Call cleanup before exiting     (default: false)\n");
    PR_fprintf(err, "\t-G   Use global threads only         (default: local)\n");
    PR_fprintf(err, "\t-t n Number of threads involved      (default: 1)\n");
    PR_fprintf(err, "\t-s n Seconds thread(s) should dally  (defaut: 10)\n");
    PR_fprintf(err, "\t-S n Seconds main() should dally     (defaut: 5)\n");
    PR_fprintf(err, "\t-C n Value to set concurrency        (default 1)\n");
    PR_fprintf(err, "\t-h   This message and nothing else\n");
}  /* Help */

PRIntn main(PRIntn argc, char **argv)
{
    PLOptStatus os;
    PRBool cleanup = PR_FALSE;
	PRThreadScope type = PR_LOCAL_THREAD;
    PRFileDesc *err = PR_GetSpecialFD(PR_StandardError);
    PLOptState *opt = PL_CreateOptState(argc, argv, "Ghs:S:t:cC:");
    PRIntn concurrency = 1, child_sleep = 10, main_sleep = 5, threads = 1;

    PR_STDIO_INIT();
    while (PL_OPT_EOL != (os = PL_GetNextOpt(opt)))
    {
        if (PL_OPT_BAD == os) continue;
        switch (opt->option)
        {
        case 'c':  /* call PR_Cleanup() before exiting */
            cleanup = PR_TRUE;
            break;
        case 'G':  /* local vs global threads */
            type = PR_GLOBAL_THREAD;
            break;
        case 's':  /* time to sleep */
            child_sleep = atoi(opt->value);
            break;
        case 'S':  /* time to sleep */
            main_sleep = atoi(opt->value);
            break;
        case 'C':  /* number of cpus to create */
            concurrency = atoi(opt->value);
            break;
        case 't':  /* number of threads to create */
            threads = atoi(opt->value);
            break;
        case 'h':  /* user wants some guidance */
            Help();  /* so give him an earful */
            return 2;  /* but not a lot else */
            break;
         default:
            break;
        }
    }
    PL_DestroyOptState(opt);

    PR_fprintf(err, "Cleanup settings\n");
    PR_fprintf(err, "\tThread type: %s\n",
        (PR_LOCAL_THREAD == type) ? "LOCAL" : "GLOBAL");
    PR_fprintf(err, "\tConcurrency: %d\n", concurrency);
    PR_fprintf(err, "\tNumber of threads: %d\n", threads);
    PR_fprintf(err, "\tThread sleep: %d\n", child_sleep);
    PR_fprintf(err, "\tMain sleep: %d\n", main_sleep); 
    PR_fprintf(err, "\tCleanup will %sbe called\n\n", (cleanup) ? "" : "NOT "); 

    PR_SetConcurrency(concurrency);

	while (threads-- > 0)
		(void)PR_CreateThread(
        	PR_USER_THREAD, Thread, (void*)child_sleep, PR_PRIORITY_NORMAL,
       		type, PR_UNJOINABLE_THREAD, 0);
    PR_Sleep(PR_SecondsToInterval(main_sleep));

    if (cleanup) PR_Cleanup();

    PR_fprintf(err, "main() exiting\n");
    return 0;
}  /* main */
