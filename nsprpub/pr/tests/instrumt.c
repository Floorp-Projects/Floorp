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

/*
** File:    instrumt.c
** Description: This test is for the NSPR debug aids defined in
** prcountr.h, prtrace.h, prolock.h
**
** The test case tests the three debug aids in NSPR:
**
** Diagnostic messages can be enabled using "instrumt -v 6"
** This sets the msgLevel to something that PR_LOG() likes.
** Also define in the environment "NSPR_LOG_MODULES=Test:6"
**
** CounterTest() tests the counter facility. This test
** creates 4 threads. Each thread either increments, decrements,
** adds to or subtracts from a counter, depending on an argument
** passed to the thread at thread-create time. Each of these threads
** does COUNT_LIMIT iterations doing its thing. When all 4 threads
** are done, the result of the counter is evaluated. If all was atomic,
** the the value of the counter should be zero.
**
** TraceTest():
** This test mingles with the counter test. Counters trace.
** A thread to extract trace entries on the fly is started.
** A thread to dump trace entries to a file is started.
**
** OrderedLockTest():
**
**
**
**
**
*/

#include <stdio.h>
#include <plstr.h>
#include <prclist.h>
#include <prmem.h>
#include <plgetopt.h> 
#include <prlog.h> 
#include <prmon.h> 
#include <pratom.h> 
#include <prtrace.h> 
#include <prcountr.h> 
#include <prolock.h> 

#define COUNT_LIMIT  (10 * ( 1024))

#define SMALL_TRACE_BUFSIZE  ( 60 * 1024 )

typedef enum 
{
    CountLoop = 1,
    TraceLoop = 2,
    TraceFlow = 3
} TraceTypes;


PRLogModuleLevel msgLevel = PR_LOG_ALWAYS;

PRBool  help = PR_FALSE;
PRBool  failed = PR_FALSE;


PRLogModuleInfo *lm;
PRMonitor   *mon;
PRInt32     activeThreads = 0;
PR_DEFINE_COUNTER( hCounter );
PR_DEFINE_TRACE( hTrace );

static void Help(void)
{
    printf("Help? ... Ha!\n");
}    

static void ListCounters(void)
{
    PR_DEFINE_COUNTER( qh );
    PR_DEFINE_COUNTER( rh );
    char *qn, *rn, *dn;
    char **qname = &qn, **rname = &rn, **desc = &dn;
    PRUint32    tCtr;

    PR_INIT_COUNTER_HANDLE( qh, NULL );
    PR_FIND_NEXT_COUNTER_QNAME(qh, qh );
    while ( qh != NULL )
    {
        PR_INIT_COUNTER_HANDLE( rh, NULL );
        PR_FIND_NEXT_COUNTER_RNAME(rh, rh, qh );
        while ( rh != NULL )
        {
            PR_GET_COUNTER_NAME_FROM_HANDLE( rh, qname, rname, desc );
            tCtr = PR_GET_COUNTER(tCtr, rh);
            PR_LOG( lm, msgLevel,
                ( "QName: %s  RName: %s  Desc: %s  Value: %ld\n", 
                qn, rn, dn, tCtr ));
            PR_FIND_NEXT_COUNTER_RNAME(rh, rh, qh );
        } 
        PR_FIND_NEXT_COUNTER_QNAME(qh, qh);
    }
    return;    
} /* end ListCounters() */

static void ListTraces(void)
{
    PR_DEFINE_TRACE( qh );
    PR_DEFINE_TRACE( rh );
    char *qn, *rn, *dn;
    char **qname = &qn, **rname = &rn, **desc = &dn;

    PR_INIT_TRACE_HANDLE( qh, NULL );
    PR_FIND_NEXT_TRACE_QNAME(qh, qh );
    while ( qh != NULL )
    {
        PR_INIT_TRACE_HANDLE( rh, NULL );
        PR_FIND_NEXT_TRACE_RNAME(rh, rh, qh );
        while ( rh != NULL )
        {
            PR_GET_TRACE_NAME_FROM_HANDLE( rh, qname, rname, desc );
            PR_LOG( lm, msgLevel,
                ( "QName: %s  RName: %s  Desc: %s", 
                qn, rn, dn ));
            PR_FIND_NEXT_TRACE_RNAME(rh, rh, qh );
        } 
        PR_FIND_NEXT_TRACE_QNAME(qh, qh);
    }
    return;    
} /* end ListCounters() */


static PRInt32 one = 1;
static PRInt32 two = 2;
static PRInt32 three = 3;
static PRInt32 four = 4;

/*
** Thread to iteratively count something.
*/
static void PR_CALLBACK CountSomething( void *arg )
{
    PRInt32 switchVar = *((PRInt32 *)arg);
    PRInt32 i;

    PR_LOG( lm, msgLevel,
        ("CountSomething: begin thread %ld", switchVar ));
    
    for ( i = 0; i < COUNT_LIMIT ; i++)
    {
        switch ( switchVar )
        {
        case 1 :
            PR_INCREMENT_COUNTER( hCounter );
            break;
        case 2 :
            PR_DECREMENT_COUNTER( hCounter );
            break;
        case 3 :
            PR_ADD_TO_COUNTER( hCounter, 1 );
            break;
        case 4 :
            PR_SUBTRACT_FROM_COUNTER( hCounter, 1 );
            break;
        default :
            PR_ASSERT( 0 );
            break;
        }
        PR_TRACE( hTrace, CountLoop, switchVar, i, 0, 0, 0, 0, 0 );
    } /* end for() */

    PR_LOG( lm, msgLevel,
        ("CounterSomething: end thread %ld", switchVar ));
    
    PR_EnterMonitor(mon);
    --activeThreads;
    PR_Notify( mon );
    PR_ExitMonitor(mon);

    return;    
} /* end CountSomething() */

/*
** Create the counter threads.
*/
static void CounterTest( void )
{
    PRThread *t1, *t2, *t3, *t4;
    PRIntn i = 0;
    PR_DEFINE_COUNTER( tc );
    PR_DEFINE_COUNTER( zCounter );

    PR_LOG( lm, msgLevel,
        ("Begin CounterTest"));
    
    /*
    ** Test Get and Set of a counter.
    **
    */
    PR_CREATE_COUNTER( zCounter, "Atomic", "get/set test", "test get and set of counter" );
    PR_SET_COUNTER( zCounter, 9 );
    PR_GET_COUNTER( i, zCounter );
    if ( i != 9 )
    {
        failed = PR_TRUE;
        PR_LOG( lm, msgLevel,
            ("Counter set/get failed"));
    }

    activeThreads += 4;
    PR_CREATE_COUNTER( hCounter, "Atomic", "SMP Tests", "test atomic nature of counter" );

    PR_GET_COUNTER_HANDLE_FROM_NAME( tc, "Atomic", "SMP Tests" );
    PR_ASSERT( tc == hCounter );

	t1 = PR_CreateThread(PR_USER_THREAD,
	        CountSomething, &one, 
			PR_PRIORITY_NORMAL,
			PR_GLOBAL_THREAD,
    		PR_UNJOINABLE_THREAD,
			0);
	PR_ASSERT(t1);

	t2 = PR_CreateThread(PR_USER_THREAD,
			CountSomething, &two, 
			PR_PRIORITY_NORMAL,
			PR_GLOBAL_THREAD,
    		PR_UNJOINABLE_THREAD,
			0);
	PR_ASSERT(t2);
        
	t3 = PR_CreateThread(PR_USER_THREAD,
			CountSomething, &three, 
			PR_PRIORITY_NORMAL,
			PR_GLOBAL_THREAD,
    		PR_UNJOINABLE_THREAD,
			0);
	PR_ASSERT(t3);
        
	t4 = PR_CreateThread(PR_USER_THREAD,
			CountSomething, &four, 
			PR_PRIORITY_NORMAL,
			PR_GLOBAL_THREAD,
    		PR_UNJOINABLE_THREAD,
			0);
	PR_ASSERT(t4);

    PR_LOG( lm, msgLevel,
        ("Counter Threads started"));

    ListCounters();
    return;
} /* end CounterTest() */

/*
** Thread to dump trace buffer to a file.
*/
static void PR_CALLBACK RecordTrace(void *arg )
{
    PR_RECORD_TRACE_ENTRIES();

    PR_EnterMonitor(mon);
    --activeThreads;
    PR_Notify( mon );
    PR_ExitMonitor(mon);

    return;    
} /* end RecordTrace() */



#define NUM_TRACE_RECORDS ( 10000 )
/*
** Thread to extract and print trace entries from the buffer.
*/
static void PR_CALLBACK SampleTrace( void *arg )
{
#if defined(DEBUG) || defined(FORCE_NSPR_TRACE)
    PRInt32 found, rc;
    PRTraceEntry    *foundEntries;
    PRInt32 i;
    
    foundEntries = (PRTraceEntry *)PR_Malloc( NUM_TRACE_RECORDS * sizeof(PRTraceEntry));
    PR_ASSERT(foundEntries != NULL );

    do
    {
        rc = PR_GetTraceEntries( foundEntries, NUM_TRACE_RECORDS, &found);
        PR_LOG( lm, msgLevel,
            ("SampleTrace: Lost Data: %ld found: %ld", rc, found ));

        if ( found != 0)
        {
            for ( i = 0 ; i < found; i++ )
            {
                PR_LOG( lm, msgLevel,
                    ("SampleTrace, detail: Thread: %p, Time: %llX, UD0: %ld, UD1: %ld, UD2: %8.8ld",
                        (foundEntries +i)->thread,
                        (foundEntries +i)->time,
                        (foundEntries +i)->userData[0], 
                        (foundEntries +i)->userData[1], 
                        (foundEntries +i)->userData[2] )); 
            }
        }
        PR_Sleep(PR_MillisecondsToInterval(50));
    }
    while( found != 0 && activeThreads >= 1 );

    PR_Free( foundEntries );

    PR_EnterMonitor(mon);
    --activeThreads;
    PR_Notify( mon );
    PR_ExitMonitor(mon);

    PR_LOG( lm, msgLevel,
        ("SampleTrace(): exiting"));

#endif
    return;    
} /* end RecordTrace() */

/*
** Basic trace test.
*/
static void TraceTest( void )
{
    PRInt32 i;
    PRInt32 size;
    PR_DEFINE_TRACE( th );
    PRThread *t1, *t2;
    
    PR_LOG( lm, msgLevel,
        ("Begin TraceTest"));    

    size = SMALL_TRACE_BUFSIZE;
    PR_SET_TRACE_OPTION( PRTraceBufSize, &size );
    PR_GET_TRACE_OPTION( PRTraceBufSize, &i );
    
    PR_CREATE_TRACE( th, "TraceTest", "tt2", "A description for the trace test" );
    PR_CREATE_TRACE( th, "TraceTest", "tt3", "A description for the trace test" );
    PR_CREATE_TRACE( th, "TraceTest", "tt4", "A description for the trace test" );
    PR_CREATE_TRACE( th, "TraceTest", "tt5", "A description for the trace test" );
    PR_CREATE_TRACE( th, "TraceTest", "tt6", "A description for the trace test" );
    PR_CREATE_TRACE( th, "TraceTest", "tt7", "A description for the trace test" );
    PR_CREATE_TRACE( th, "TraceTest", "tt8", "A description for the trace test" );

    PR_CREATE_TRACE( th, "Trace Test", "tt0", "QName is Trace Test, not TraceTest" );
    PR_CREATE_TRACE( th, "Trace Test", "tt1", "QName is Trace Test, not TraceTest" );
    PR_CREATE_TRACE( th, "Trace Test", "tt2", "QName is Trace Test, not TraceTest" );
    PR_CREATE_TRACE( th, "Trace Test", "tt3", "QName is Trace Test, not TraceTest" );
    PR_CREATE_TRACE( th, "Trace Test", "tt4", "QName is Trace Test, not TraceTest" );
    PR_CREATE_TRACE( th, "Trace Test", "tt5", "QName is Trace Test, not TraceTest" );
    PR_CREATE_TRACE( th, "Trace Test", "tt6", "QName is Trace Test, not TraceTest" );
    PR_CREATE_TRACE( th, "Trace Test", "tt7", "QName is Trace Test, not TraceTest" );
    PR_CREATE_TRACE( th, "Trace Test", "tt8", "QName is Trace Test, not TraceTest" );
    PR_CREATE_TRACE( th, "Trace Test", "tt9", "QName is Trace Test, not TraceTest" );
    PR_CREATE_TRACE( th, "Trace Test", "tt10", "QName is Trace Test, not TraceTest" );



    activeThreads += 2;
	t1 = PR_CreateThread(PR_USER_THREAD,
			RecordTrace, NULL, 
			PR_PRIORITY_NORMAL,
			PR_GLOBAL_THREAD,
    		PR_UNJOINABLE_THREAD,
			0);
	PR_ASSERT(t1);

	t2 = PR_CreateThread(PR_USER_THREAD,
			SampleTrace, 0, 
			PR_PRIORITY_NORMAL,
			PR_GLOBAL_THREAD,
    		PR_UNJOINABLE_THREAD,
			0);
	PR_ASSERT(t2);
        
    ListTraces();

    PR_GET_TRACE_HANDLE_FROM_NAME( th, "TraceTest","tt1" );
    PR_ASSERT( th == hTrace );

    PR_LOG( lm, msgLevel,
        ("End TraceTest"));    
    return;
} /* end TraceTest() */


/*
** Ordered lock test.
*/
static void OrderedLockTest( void )
{
    PR_LOG( lm, msgLevel,
        ("Begin OrderedLockTest"));    

    
} /* end OrderedLockTest() */


PRIntn main(PRIntn argc, char *argv[])
{
#if defined(DEBUG) || defined(FORCE_NSPR_TRACE)
    PRUint32    counter;
    PLOptStatus os;
    PLOptState *opt = PL_CreateOptState(argc, argv, "hdv:");
    lm = PR_NewLogModule("Test");

	while (PL_OPT_EOL != (os = PL_GetNextOpt(opt)))
    {
		if (PL_OPT_BAD == os) continue;
        switch (opt->option)
        {
        case 'v':  /* verbose mode */
			msgLevel = (PRLogModuleLevel)atol( opt->value);
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

    PR_CREATE_TRACE( hTrace, "TraceTest", "tt1", "A description for the trace test" );
    mon = PR_NewMonitor();
    PR_EnterMonitor( mon );

    TraceTest();
    CounterTest();
    OrderedLockTest();

    /* Wait for all threads to exit */
    while ( activeThreads > 0 ) {
        if ( activeThreads == 1 )
            PR_SET_TRACE_OPTION( PRTraceStopRecording, NULL );
    	PR_Wait(mon, PR_INTERVAL_NO_TIMEOUT);
        PR_GET_COUNTER( counter, hCounter );
    }
    PR_ExitMonitor( mon );

    /*
    ** Evaluate results
    */
    PR_GET_COUNTER( counter, hCounter );
    if ( counter != 0 )
    {
        failed = PR_TRUE;
        PR_LOG( lm, msgLevel,
            ("Expected counter == 0, found: %ld", counter));
        printf("FAIL\n");
    }
    else
    {
        printf("PASS\n");
    }


    PR_DESTROY_COUNTER( hCounter );

    PR_DestroyMonitor( mon );

    PR_TRACE( hTrace, TraceFlow, 0xfff,0,0,0,0,0,0);
    PR_DESTROY_TRACE( hTrace );
#else
    printf("Test not defined\n");
#endif
    return 0;
}  /* main() */
/* end instrumt.c */

