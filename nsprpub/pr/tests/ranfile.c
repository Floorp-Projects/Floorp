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

/***********************************************************************
**
** Contact:     AOF<freier@netscape.com>
**
** Name: ranfile.c
**
** Description: Test to hammer on various components of NSPR
** Modification History:
** 20-May-97 AGarcia- Converted the test to accomodate the debug_mode flag.
**	         The debug mode will print all of the printfs associated with this test.
**			 The regress mode will be the default mode. Since the regress tool limits
**           the output to a one line status:PASS or FAIL,all of the printf statements
**			 have been handled with an if (debug_mode) statement.
** 04-June-97 AGarcia removed the Test_Result function. Regress tool has been updated to
**			recognize the return code from tha main program.
***********************************************************************/


/***********************************************************************
** Includes
***********************************************************************/
/* Used to get the command line option */
#include "plgetopt.h"

#include "prinit.h"
#include "prthread.h"
#include "prlock.h"
#include "prcvar.h"
#include "prmem.h"
#include "prinrval.h"
#include "prio.h"

#include <string.h>
#include <stdio.h>

static PRIntn debug_mode = 0;
static PRIntn failed_already=0;
static PRThreadScope thread_scope = PR_LOCAL_THREAD;

typedef enum {sg_go, sg_stop, sg_done} Action;
typedef enum {sg_okay, sg_open, sg_close, sg_delete, sg_write, sg_seek} Problem;

typedef struct Hammer_s {
    PRLock *ml;
    PRCondVar *cv;
    PRUint32 id;
    PRUint32 limit;
    PRUint32 writes;
    PRThread *thread;
    PRIntervalTime timein;
    Action action;
    Problem problem;
} Hammer_t;

#define DEFAULT_LIMIT		10
#define DEFAULT_THREADS		2
#define DEFAULT_LOOPS		1

static PRInt32 pageSize = 1024;
static const char* baseName = "./";
static const char *programName = "Random File";

#ifdef XP_MAC
#include "prlog.h"
#define printf PR_LogPrint
extern void SetupMacPrintfLog(char *logFile);
#endif

/***********************************************************************
** PRIVATE FUNCTION:    Random
** DESCRIPTION:
**   Generate a pseudo-random number
** INPUTS:      None
** OUTPUTS:     None
** RETURN:      A pseudo-random unsigned number, 32-bits wide
** SIDE EFFECTS:
**      Updates random seed (a static)
** RESTRICTIONS:
**      None
** MEMORY:      NA
** ALGORITHM:
**      Uses the current interval timer value, promoted to a 64 bit
**      float as a multiplier for a static residue (which begins
**      as an uninitialized variable). The result is bits [16..48)
**      of the product. Seed is then updated with the return value
**      promoted to a float-64.
***********************************************************************/
static PRUint32 Random(void)
{
    PRUint32 rv;
    PRUint64 shift;
    static PRFloat64 seed = 0x58a9382;  /* Just make sure it isn't 0! */
    PRFloat64 random = seed * (PRFloat64)PR_IntervalNow();
    LL_USHR(shift, *((PRUint64*)&random), 16);
    LL_L2UI(rv, shift);
    seed = (PRFloat64)rv;
    return rv;
}  /* Random */

/***********************************************************************
** PRIVATE FUNCTION:    Thread
** DESCRIPTION:
**   Hammer on the file I/O system
** INPUTS:      A pointer to the thread's private data
** OUTPUTS:     None
** RETURN:      None
** SIDE EFFECTS:
**      Creates, accesses and deletes a file
** RESTRICTIONS:
**      (Currently) must have file create permission in "/usr/tmp".
** MEMORY:      NA
** ALGORITHM:
**      This function is a root of a thread
**      1) Creates a (hopefully) unique file in /usr/tmp/
**      2) Writes a zero to a random number of sequential pages
**      3) Closes the file
**      4) Reopens the file
**      5) Seeks to a random page within the file
**      6) Writes a one byte on that page
**      7) Repeat steps [5..6] for each page in the file
**      8) Close and delete the file
**      9) Repeat steps [1..8] until told to stop
**     10) Notify complete and return
***********************************************************************/
static void PR_CALLBACK Thread(void *arg)
{
    PRUint32 index;
    char filename[30];
    const char zero = 0;
    PRFileDesc *file = NULL;
    PRStatus rv = PR_SUCCESS;
    Hammer_t *cd = (Hammer_t*)arg;

    (void)sprintf(filename, "%ssg%04ld.dat", baseName, cd->id);

    if (debug_mode) printf("Starting work on %s\n", filename);

    while (PR_TRUE)
    {
        PRUint32 bytes;
        PRUint32 minor = (Random() % cd->limit) + 1;
        PRUint32 random = (Random() % cd->limit) + 1;
        PRUint32 pages = (Random() % cd->limit) + 10;
        while (minor-- > 0)
        {
            cd->problem = sg_okay;
            if (cd->action != sg_go) goto finished;
            cd->problem = sg_open;
            file = PR_Open(filename, PR_RDWR|PR_CREATE_FILE, 0666);
            if (file == NULL) goto finished;
            for (index = 0; index < pages; index++)
            {
                cd->problem = sg_okay;
                if (cd->action != sg_go) goto close;
                cd->problem = sg_seek;
                bytes = PR_Seek(file, pageSize * index, PR_SEEK_SET);
                if (bytes != pageSize * index) goto close;
                cd->problem = sg_write;
                bytes = PR_Write(file, &zero, sizeof(zero));
                if (bytes <= 0) goto close;
                cd->writes += 1;
            }
            cd->problem = sg_close;
            rv = PR_Close(file);
            if (rv != PR_SUCCESS) goto purge;

            cd->problem = sg_okay;
            if (cd->action != sg_go) goto purge;

            cd->problem = sg_open;
            file = PR_Open(filename, PR_RDWR, 0666);
            for (index = 0; index < pages; index++)
            {
                cd->problem = sg_okay;
                if (cd->action != sg_go) goto close;
                cd->problem = sg_seek;
                bytes = PR_Seek(file, pageSize * index, PR_SEEK_SET);
                if (bytes != pageSize * index) goto close;
                cd->problem = sg_write;
                bytes = PR_Write(file, &zero, sizeof(zero));
                if (bytes <= 0) goto close;
                cd->writes += 1;
                random = (random + 511) % pages;
            }
            cd->problem = sg_close;
            rv = PR_Close(file);
            if (rv != PR_SUCCESS) goto purge;
            cd->problem = sg_delete;
            rv = PR_Delete(filename);
            if (rv != PR_SUCCESS) goto finished;
       }
    }

close:
    (void)PR_Close(file);
purge:
    (void)PR_Delete(filename);
finished:
    PR_Lock(cd->ml);
    cd->action = sg_done;
    PR_NotifyCondVar(cd->cv);
    PR_Unlock(cd->ml);

    if (debug_mode) printf("Ending work on %s\n", filename);

    return;
}  /* Thread */

static Hammer_t hammer[100];
static PRCondVar *cv;
/***********************************************************************
** PRIVATE FUNCTION:    main
** DESCRIPTION:
**   Hammer on the file I/O system
** INPUTS:      The usual argc and argv
**              argv[0] - program name (not used)
**              argv[1] - the number of times to execute the major loop
**              argv[2] - the number of threads to toss into the batch
**              argv[3] - the clipping number applied to randoms
**              default values: loops = 2, threads = 10, limit = 57
** OUTPUTS:     None
** RETURN:      None
** SIDE EFFECTS:
**      Creates, accesses and deletes lots of files
** RESTRICTIONS:
**      (Currently) must have file create permission in "/usr/tmp".
** MEMORY:      NA
** ALGORITHM:
**      1) Fork a "Thread()"
**      2) Wait for 'interleave' seconds
**      3) For [0..'threads') repeat [1..2]
**      4) Mark all objects to stop
**      5) Collect the threads, accumulating the results
**      6) For [0..'loops') repeat [1..5]
**      7) Print accumulated results and exit
**
**      Characteristic output (from IRIX)
**          Random File: Using loops = 2, threads = 10, limit = 57
**          Random File: [min [avg] max] writes/sec average
***********************************************************************/
int main (int argc,      char   *argv[])
{
    PRLock *ml;
    PRUint32 id = 0;
    int active, poll;
    PRIntervalTime interleave;
    PRIntervalTime duration = 0;
    int limit = 0, loops = 0, threads = 0, times;
    PRUint32 writes, writesMin = 0x7fffffff, writesTot = 0, durationTot = 0, writesMax = 0;

    const char *where[] = {"okay", "open", "close", "delete", "write", "seek"};

	/* The command line argument: -d is used to determine if the test is being run
	in debug mode. The regress tool requires only one line output:PASS or FAIL.
	All of the printfs associated with this test has been handled with a if (debug_mode)
	test.
	Usage: test_name -d
	*/
	PLOptStatus os;
	PLOptState *opt = PL_CreateOptState(argc, argv, "Gdl:t:i:");
	while (PL_OPT_EOL != (os = PL_GetNextOpt(opt)))
    {
		if (PL_OPT_BAD == os) continue;
        switch (opt->option)
        {
        case 'G':  /* global threads */
			thread_scope = PR_GLOBAL_THREAD;
            break;
        case 'd':  /* debug mode */
			debug_mode = 1;
            break;
        case 'l':  /* limiting number */
			limit = atoi(opt->value);
            break;
        case 't':  /* number of threads */
			threads = atoi(opt->value);
            break;
        case 'i':  /* iteration counter */
			loops = atoi(opt->value);
            break;
         default:
            break;
        }
    }
	PL_DestroyOptState(opt);

 /* main test */
	
    PR_Init(PR_USER_THREAD, PR_PRIORITY_NORMAL, 0);
    PR_STDIO_INIT();

    interleave = PR_SecondsToInterval(10);

#ifdef XP_MAC
	SetupMacPrintfLog("ranfile.log");
	debug_mode = 1;
#endif

    ml = PR_NewLock();
    cv = PR_NewCondVar(ml);

    if (loops == 0) loops = DEFAULT_LOOPS;
    if (limit == 0) limit = DEFAULT_LIMIT;
    if (threads == 0) threads = DEFAULT_THREADS;

    if (debug_mode) printf(
        "%s: Using loops = %d, threads = %d, limit = %d and %s threads\n",
        programName, loops, threads, limit,
        (thread_scope == PR_LOCAL_THREAD) ? "LOCAL" : "GLOBAL");

    for (times = 0; times < loops; ++times)
    {
        if (debug_mode) printf("%s: Setting concurrency level to %d\n", programName, times + 1);
        PR_SetConcurrency(times + 1);
        for (active = 0; active < threads; active++)
        {
            hammer[active].ml = ml;
            hammer[active].cv = cv;
            hammer[active].id = id++;
            hammer[active].writes = 0;
            hammer[active].action = sg_go;
            hammer[active].problem = sg_okay;
            hammer[active].limit = (Random() % limit) + 1;
            hammer[active].timein = PR_IntervalNow();
            hammer[active].thread = PR_CreateThread(
                PR_USER_THREAD, Thread, &hammer[active],
                PR_GetThreadPriority(PR_GetCurrentThread()),
                thread_scope, PR_JOINABLE_THREAD, 0);

            PR_Lock(ml);
            PR_WaitCondVar(cv, interleave);  /* start new ones slowly */
            PR_Unlock(ml);
        }

        /*
         * The last thread started has had the opportunity to run for
         * 'interleave' seconds. Now gather them all back in.
         */
        PR_Lock(ml);
        for (poll = 0; poll < threads; poll++)
        {
            if (hammer[poll].action == sg_go)  /* don't overwrite done */
                hammer[poll].action = sg_stop;  /* ask him to stop */
        }
        PR_Unlock(ml);

        while (active > 0)
        {
            for (poll = 0; poll < threads; poll++)
            {
                PR_Lock(ml);
                while (hammer[poll].action < sg_done)
                    PR_WaitCondVar(cv, PR_INTERVAL_NO_TIMEOUT);
                PR_Unlock(ml);

                active -= 1;  /* this is another one down */
                (void)PR_JoinThread(hammer[poll].thread);
                hammer[poll].thread = NULL;
                if (hammer[poll].problem == sg_okay)
                {
                    duration = PR_IntervalToMilliseconds(
                        PR_IntervalNow() - hammer[poll].timein);
                    writes = hammer[poll].writes * 1000 / duration;
                    if (writes < writesMin) 
                        writesMin = writes;
                    if (writes > writesMax) 
                        writesMax = writes;
                    writesTot += hammer[poll].writes;
                    durationTot += duration;
                }
                else
                    if (debug_mode) printf(
                        "%s: test failed %s after %ld seconds\n",
                        programName, where[hammer[poll].problem], duration);
					else failed_already=1;
            }
        }
    }
    if (debug_mode) printf(
        "%s: [%ld [%ld] %ld] writes/sec average\n",
        programName, writesMin, writesTot * 1000 / durationTot, writesMax);

    PR_DestroyCondVar(cv);
    PR_DestroyLock(ml);

	if (failed_already) 
	{
	    printf("FAIL\n");
		return 1;
	}
    else
    {
        printf("PASS\n");
		return 0;
    }
}  /* main */
