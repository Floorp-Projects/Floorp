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
** File:        arena.c
** Description: Testing arenas
**
*/

#include <string.h>
#include <time.h>
#include <stdlib.h>
#include "nspr.h"
#include "plarena.h"
#include "plgetopt.h"

PRLogModuleInfo *tLM;
PRIntn  threadCount = 0;
PRMonitor   *tMon;
PRBool failed_already = PR_FALSE;

/* Arguments from the command line with default values */
PRIntn debug_mode = 0;
PRIntn  poolMin = 4096;
PRIntn  poolMax = (100 * 4096);
PRIntn  arenaMin = 40;
PRIntn  arenaMax = (100 * 40);
PRIntn  stressIterations = 15;
PRIntn  maxAlloc = (1024 * 1024);
PRIntn  stressThreads = 4;

void DumpAll( void )
{
	return;
}

/*
** Test Arena allocation.
*/
static void ArenaAllocate( void )
{
    PLArenaPool ap;
    void    *ptr;
	PRInt32	i;

    PL_InitArenaPool( &ap, "AllocArena", 2048, sizeof(double));
    PR_LOG( tLM, PR_LOG_DEBUG, ("AA, InitPool -- Pool: %p. first: %p, current: %p, size: %d", 
        &ap, ap.first, ap.current, ap.arenasize  ));

	for( i = 0; i < 150; i++ )
	{
		PL_ARENA_ALLOCATE( ptr, &ap, 512 );
        PR_LOG( tLM, PR_LOG_DEBUG,("AA, after alloc -- Pool: %p. first: %p, current: %p, size: %d", 
               &ap, ap.first, ap.current, ap.arenasize  ));
		PR_LOG( tLM, PR_LOG_DEBUG,(
		    "AA -- Pool: %p. alloc: %p ", &ap, ptr ));
	}

    PL_FreeArenaPool( &ap );

	for( i = 0; i < 221; i++ )
	{
		PL_ARENA_ALLOCATE( ptr, &ap, 512 );
        PR_LOG( tLM, PR_LOG_DEBUG,("AA, after alloc -- Pool: %p. first: %p, current: %p, size: %d", 
               &ap, ap.first, ap.current, ap.arenasize  ));
		PR_LOG( tLM, PR_LOG_DEBUG,(
		    "AA -- Pool: %p. alloc: %p ", &ap, ptr ));
	}

    PL_FreeArenaPool( &ap );
    
    return;
} /* end ArenaGrow() */
/*
** Test Arena grow.
*/
static void ArenaGrow( void )
{
    PLArenaPool ap;
    void    *ptr;
	PRInt32	i;

    PL_InitArenaPool( &ap, "TheArena", 4096, sizeof(double));
    PL_ARENA_ALLOCATE( ptr, &ap, 512 );

	PR_LOG( tLM, PR_LOG_DEBUG, ("Before growth -- Pool: %p. alloc: %p ", &ap, ptr ));

	for( i = 0; i < 10; i++ )
	{
		PL_ARENA_GROW( ptr, &ap, 512, 7000 );
		PR_LOG( tLM, PR_LOG_DEBUG, ("After growth -- Pool: %p. alloc: %p ", &ap, ptr ));
	}


    return;
} /* end ArenaGrow() */


/*
** Test arena Mark and Release.
*/
static void MarkAndRelease( void )
{
    PLArenaPool ap;
    void    *ptr = NULL;
    void    *mark0, *mark1;
    PRIntn  i;

    PL_InitArenaPool( &ap, "TheArena", 4096, sizeof(double));
    mark0 = PL_ARENA_MARK( &ap );
    PR_LOG( tLM, PR_LOG_DEBUG,
        ("mark0. ap: %p, ap.f: %p, ap.c: %p, ap.siz: %d, alloc: %p, m0: %p", 
            &ap, ap.first.next, ap.current, ap.arenasize, ptr, mark0 ));

	for( i = 0; i < 201; i++ )
	{
		PL_ARENA_ALLOCATE( ptr, &ap, 512 );
        PR_LOG( tLM, PR_LOG_DEBUG,
            ("mr. ap: %p, ap.f: %p, ap.c: %p, ap.siz: %d, alloc: %p", 
                &ap, ap.first.next, ap.current, ap.arenasize, ptr ));
	}

    mark1 = PL_ARENA_MARK( &ap );
    PR_LOG( tLM, PR_LOG_DEBUG,
        ("mark1. ap: %p, ap.f: %p, ap.c: %p, ap.siz: %d, alloc: %p, m1: %p", 
            &ap, ap.first.next, ap.current, ap.arenasize, ptr, mark1 ));


	for( i = 0; i < 225; i++ )
	{
		PL_ARENA_ALLOCATE( ptr, &ap, 512 );
        PR_LOG( tLM, PR_LOG_DEBUG,
            ("mr. ap: %p, ap.f: %p, ap.c: %p, ap.siz: %d, alloc: %p", 
                &ap, ap.first.next, ap.current, ap.arenasize, ptr ));
	}

    PL_ARENA_RELEASE( &ap, mark1 );
    PR_LOG( tLM, PR_LOG_DEBUG,
        ("Release-1: %p -- Pool: %p. first: %p, current: %p, size: %d", 
               mark1, &ap, ap.first, ap.current, ap.arenasize  ));

	for( i = 0; i < 20; i++ )
	{
		PL_ARENA_ALLOCATE( ptr, &ap, 512 );
        PR_LOG( tLM, PR_LOG_DEBUG,
            ("mr. ap: %p, ap.f: %p, ap.c: %p, ap.siz: %d, alloc: %p", 
                &ap, ap.first.next, ap.current, ap.arenasize, ptr ));
	}

    PL_ARENA_RELEASE( &ap, mark1 );
    PR_LOG( tLM, PR_LOG_DEBUG,
        ("Release-1. ap: %p, ap.f: %p, ap.c: %p, ap.siz: %d, alloc: %p", 
            &ap, ap.first.next, ap.current, ap.arenasize, ptr ));

    PL_ARENA_RELEASE( &ap, mark0 );
    PR_LOG( tLM, PR_LOG_DEBUG,
        ("Release-0. ap: %p, ap.f: %p, ap.c: %p, ap.siz: %d, alloc: %p", 
            &ap, ap.first.next, ap.current, ap.arenasize, ptr ));

    PL_FreeArenaPool( &ap );
    PR_LOG( tLM, PR_LOG_DEBUG,
        ("Free. ap: %p, ap.f: %p, ap.c: %p, ap.siz: %d, alloc: %p", 
            &ap, ap.first.next, ap.current, ap.arenasize, ptr ));
    
    PL_FinishArenaPool( &ap );
    PR_LOG( tLM, PR_LOG_DEBUG,
        ("Finish. ap: %p, ap.f: %p, ap.c: %p, ap.siz: %d, alloc: %p", 
            &ap, ap.first.next, ap.current, ap.arenasize, ptr ));

    return;
} /* end MarkAndRelease() */

/*
** RandSize() returns a random number in the range 
** min..max, rounded to the next doubleword
**
*/
static PRIntn RandSize( PRIntn min, PRIntn max )
{
    PRIntn  sz = (rand() % (max -min)) + min + sizeof(double);

    sz &= ~sizeof(double)-1;

    return(sz);
}


/*
** StressThread()
** A bunch of these beat on individual arenas
** This tests the free_list protection.
**
*/
static void PR_CALLBACK StressThread( void *arg )
{
    PLArenaPool ap;
    PRIntn i;
    PRIntn sz;
    void *ptr;
    PRThread *tp = PR_GetCurrentThread();

    PR_LOG( tLM, PR_LOG_DEBUG, ("Stress Thread %p started\n", PR_GetCurrentThread()));
    PL_InitArenaPool( &ap, "TheArena", RandSize( poolMin, poolMax), sizeof(double));

    for ( i = 0; i < stressIterations; i++ )
    {
        PRIntn allocated = 0;

        while ( allocated < maxAlloc )
        {
            sz = RandSize( arenaMin, arenaMax );
            PL_ARENA_ALLOCATE( ptr, &ap, sz );
            if ( ptr == NULL )
            {
                PR_LOG( tLM, PR_LOG_ERROR, ("ARENA_ALLOCATE() returned NULL\n\tAllocated: %d\n", allocated));
                break;
            }
            allocated += sz;
        }
        PR_LOG( tLM, PR_LOG_DEBUG, ("Stress thread %p finished one iteration\n", tp));
        PL_FreeArenaPool( &ap );
    }
    PR_LOG( tLM, PR_LOG_DEBUG, ("Stress thread %p finished all iteration\n", tp));
    PL_FinishArenaPool( &ap );
    PR_LOG( tLM, PR_LOG_DEBUG, ("Stress thread %p after FinishArenaPool()\n", tp));

    /* That's all folks! let's quit */
    PR_EnterMonitor(tMon);
    threadCount--;
    PR_Notify(tMon);
    PR_ExitMonitor(tMon);    
    return;
}    

/*
** Stress()
** Flog the hell out of arenas multi-threaded.
** Do NOT pass an individual arena to another thread.
** 
*/
static void Stress( void )
{
    PRThread    *tt;
    PRIntn      i;

    tMon = PR_NewMonitor();

    for ( i = 0 ; i < stressThreads ; i++ )
    {
        PR_EnterMonitor(tMon);
        tt = PR_CreateThread(PR_USER_THREAD,
               StressThread,
               NULL,
               PR_PRIORITY_NORMAL,
               PR_GLOBAL_THREAD,
               PR_UNJOINABLE_THREAD,
               0);
        threadCount++;
        PR_ExitMonitor(tMon);
    }

    /* Wait for all threads to exit */
    PR_EnterMonitor(tMon);
    while ( threadCount != 0 ) 
    {
        PR_Wait(tMon, PR_INTERVAL_NO_TIMEOUT);
    }
    PR_ExitMonitor(tMon);
	PR_DestroyMonitor(tMon);

    return;
} /* end Stress() */

/*
** EvaluateResults()
** uses failed_already to display results and set program
** exit code.
*/
static PRIntn  EvaluateResults(void)
{
    PRIntn rc = 0;

    if ( failed_already == PR_TRUE )
    {
        PR_LOG( tLM, PR_LOG_DEBUG, ("FAIL\n"));
        rc =1;
    } 
    else
    {
        PR_LOG( tLM, PR_LOG_DEBUG, ("PASS\n"));
    }
    return(rc);
} /* EvaluateResults() */

void Help( void )
{
    printf("arena [options]\n");
    printf("where options are:\n");
    printf("-p <n>   minimum size of an arena pool. Default(%d)\n", poolMin);
    printf("-P <n>   maximum size of an arena pool. Default(%d)\n", poolMax);
    printf("-a <n>   minimum size of an arena allocation. Default(%d)\n", arenaMin);
    printf("-A <n>   maximum size of an arena allocation. Default(%d)\n", arenaMax);
    printf("-i <n>   number of iterations in a stress thread. Default(%d)\n", stressIterations);
    printf("-s <n>   maximum allocation for a single stress thread. Default(%d)\n", maxAlloc);
    printf("-t <n>   number of stress threads. Default(%d)\n", stressThreads );
    printf("-d       enable debug mode\n");
    printf("\n");
    exit(1);
}    

PRIntn main(PRIntn argc, char *argv[])
{
    PLOptStatus os;
	PLOptState *opt = PL_CreateOptState(argc, argv, "dhp:P:a:A:i:s:t:");
	while (PL_OPT_EOL != (os = PL_GetNextOpt(opt)))
    {
		if (PL_OPT_BAD == os) continue;
        switch (opt->option)
        {
        case 'a':  /* arena Min size */
            arenaMin = atol( opt->value );
            break;
        case 'A':  /* arena Max size  */
            arenaMax = atol( opt->value );
            break;
        case 'p':  /* pool Min size */
            poolMin = atol( opt->value );
            break;
        case 'P':  /* pool Max size */
            poolMax = atol( opt->value );
            break;
        case 'i':  /* Iterations in stress tests */
            stressIterations = atol( opt->value );
            break;
        case 's':  /* storage to get per iteration */
            maxAlloc = atol( opt->value );
            break;
        case 't':  /* Number of stress threads to create */
            stressThreads = atol( opt->value );
            break;
        case 'd':  /* debug mode */
			debug_mode = 1;
            break;
        case 'h':  /* help */
        default:
            Help();
        } /* end switch() */
    } /* end while() */
	PL_DestroyOptState(opt);

    srand( (unsigned)time( NULL ) ); /* seed random number generator */
    tLM = PR_NewLogModule("testcase");


#if 0
	ArenaAllocate();
	ArenaGrow();
#endif

    MarkAndRelease();

    Stress();

    return(EvaluateResults());
} /* end main() */

/* arena.c */
