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
** File:        event.c
** Description: Test functions in plevent.c
**
** This test creates a number of threads. Each thread sends
** an event to the next higher numbered (in creation order)
** thread. On receipt of that event, that thread in turn
** sends an event to the next thread until the event goes
** around the circle of threads. The test self terminates
** after a predetermined number of events have circled the
** circle of threads.
**
**
** Internal Design
**
** TState:
** TState is the Event Handler State Strucuture. It
** contains the state for an EventThread. There is one TState
** structure per thread running in the EventThread()
** function.
**
** TestEvents():
** The TestEvent() function is the "real main()" function of
** this test. The function creates 'numThreads' instances of
** a thread running the EventThread() function. He then waits
** for these threads to complete.
**
** EventThread():
** The EventThread() function runs in the context of a thread
** instance created by TestEvents(). The function processes
** events from threads running the same function. An event is
** created and sent around the ring 
** 
** EventHandler(): 
** The EventHandler() function processes events received by
** EventThread(). His job is to create and send a new event
** to the next EventThread() thread in the ring of threads.
**
** EventDestructor():
** The EventDestructor() function disposes of the event.
** Required by the protocol for PLEvent.
**
**
*/          

#include "nspr.h"
#include "plgetopt.h"
#include "plevent.h"

/*
** TState -- Event Handler State Structure
**
*/
typedef struct TState
{
    PRIntn  threadIndex;        /* thread number. [0..n] */
    PRIntn  quitSwitch;         /* when 0, running; when 1, order to quit */
    PLEventQueue *eventQueue;   /* event queue for this thread */
} TState;

/*
** Thread Event structure
*/
typedef struct TEvent
{
    PLEvent plEvent;
    PRIntn  tIndex;         /* Thread index of thread receiving event */
    PRIntn  quitSwitch;     /* when 0, keep going; when 1 set TState[me].quitSwitch = 1 */
} TEvent;

static void PR_CALLBACK EventHandler( TEvent *ep );
static void PR_CALLBACK EventDestructor( TEvent *ep );
PR_EXTERN(PRIntn) TestEvents( void );

/* --- Global data ---------------------------------------*/

static PRIntn  failed = 0;          /* when 1, indicates test failed */
static PRLogModuleInfo    *lm;      /* LogModule "test" */

static PRIntn   numThreads = 4;     /* Number of threads in the ring */
static PRIntn   iterations = 7;     /* Number of iterations per thread */

static PRIntn   iterationCount = 0; /* Counting times around the ring */

static PRIntn   activeThreads = 0;  /* Number of active threads */
static TState   *tsa = NULL;         /* array of all TStates */
static PRMonitor *mon;
static PRBool   useMonitoredQueue = PR_FALSE;

/*
** SendEvent() -- Send an event to the next thread
**
*/
static void SendEvent( 
    PRIntn threadIndex, 
    PRIntn quitSwitch 
)
{
    PLEventQueue    *eq = (tsa+threadIndex)->eventQueue;
    TEvent  *event = PR_NEWZAP( TEvent );
    
    PR_ASSERT( event != NULL );

    PR_LOG( lm, PR_LOG_NOTICE, ("SendEvent: event: %p, threadIndex: %d quitSwitch: %d", event, threadIndex, quitSwitch ));
    PL_ENTER_EVENT_QUEUE_MONITOR( eq );
    
    PL_InitEvent( &event->plEvent, 0,
            (PLHandleEventProc)EventHandler, 
            (PLDestroyEventProc)EventDestructor  );
    event->quitSwitch = quitSwitch;
    event->tIndex = threadIndex;

    PL_PostEvent( eq, &event->plEvent );
    PL_EXIT_EVENT_QUEUE_MONITOR( eq );
    return;        
} /* end SendEvent() */
 

/*
** EventHandler() -- Handle events posted from EventThread()
**
** if message says quit
**   Tell my thread to quit.
** create an event, if I quit, pass it on.
** PostEvent( TEvent.threadIndex+1)
**
*/
static void PR_CALLBACK EventHandler( TEvent *ep )
{
    TState  *ts = tsa+(ep->tIndex);
    PRIntn  ndx;

    PR_LOG( lm, PR_LOG_NOTICE, ("EventHandler: %p, ti: %d", ep, ep->tIndex ));
    if ( ep->quitSwitch == PR_TRUE )
        ts->quitSwitch = PR_TRUE;

    if ( ts->threadIndex == 0 )
    {
        if ( iterationCount++ >= iterations )
            ep->quitSwitch = PR_TRUE;
    }
    /*
    ** Calculate thread index of the next thread to get an event,
    ** then send him the event.
    */
    ndx = ep->tIndex +1;
    if ( ndx >= numThreads )
        ndx = 0;

    SendEvent( ndx, ep->quitSwitch );

    return;    
} /* end EventHandler() */

/*
** EventDestructor() -- Free  the event handled in EventHandler()
**
** free(the event)
**
*/
static void PR_CALLBACK EventDestructor( TEvent *ep )
{
    PR_LOG( lm, PR_LOG_NOTICE, ("EventDestructor: event: %p", ep ));
    PR_Free( ep );
    return;
} /* end EventDestructor() */

/*
** EventThread() -- Drive events around the ring of threads
** 
** do
**   WaitforEvent()
**   HandleEvent()
**   PR_LOG( the event)
** until told to quit
** 
*/
static void EventThread( void *arg )
{   
    TEvent *ep;
    TState *ts = (TState *)arg;


   while ( ts->eventQueue == 0 )
        PR_Sleep( PR_MillisecondsToInterval(100));

    do {
        ep = (TEvent *)PL_WaitForEvent( ts->eventQueue );
        PL_HandleEvent( (PLEvent *)ep );
        PR_LOG( lm, PR_LOG_NOTICE,("EventThread() Handled event: %d", ts->threadIndex ));
    } while( ts->quitSwitch == 0 );

    PR_EnterMonitor( mon );
    activeThreads--;
    PR_Notify( mon );
    PR_ExitMonitor( mon );

    PR_LOG( lm, PR_LOG_NOTICE,("EventThread() Exit: %d", ts->threadIndex ));

    return;    
} /* end EventThread() */

/*
** TestEvents() -- 
**
** Create array of TEvents
** CreateThreads of EventThreads
** PostEvent() to the first EventThread
** monitor for completion.
** report results.
**
*/
PR_IMPLEMENT( PRIntn ) TestEvents( void )
{
    PRIntn  i;

    lm = PR_NewLogModule( "test" );
    PR_LOG( lm, PR_LOG_NOTICE,("TestEvent(): Starting" ));
    mon = PR_NewMonitor();
    
    /*
    ** Allocate array of all TEvents
    */
    tsa = PR_Calloc( numThreads ,sizeof(TState));

    /*
    ** Initialize this event queue and create its thead
    */
    PR_ASSERT( tsa != NULL );
    activeThreads = numThreads;
    for ( i = 0 ; i < numThreads; i++ )
    {
        PRThread *me;
        PLEventQueue *eq;

        (tsa +i)->threadIndex = i;
        (tsa +i)->quitSwitch = 0;
        me = PR_CreateThread( PR_USER_THREAD,
                EventThread, 
                (void *)(tsa +i),
                PR_PRIORITY_NORMAL,
                PR_LOCAL_THREAD,
                PR_UNJOINABLE_THREAD,
                0 );
        if ( me == NULL )
        {
            PR_LOG( lm, PR_LOG_ERROR, ("TestEvents: Can't create thread %d", i ));
            exit(1);
        }

        if ( useMonitoredQueue == PR_TRUE )
            eq = PL_CreateMonitoredEventQueue( "EventThread", me );
        else
            eq = PL_CreateNativeEventQueue( "EventThread", me );
        PR_ASSERT( eq != NULL );

        (tsa+i)->eventQueue = eq;
    }
    

    /*
    ** Post and event to the first thread in the ring
    ** to get things started.
    */
    SendEvent( 0, 0 );
    /* 
    ** Wait for all threads to exit 
    */
    PR_EnterMonitor( mon );
    while ( activeThreads > 0 ) 
    	PR_Wait(mon, PR_INTERVAL_NO_TIMEOUT);
    PR_ExitMonitor( mon );

    /*
    ** Release assets: event queue for each thread, the list of TStates.
    */
    for ( i = 0 ; i < numThreads; i++ )
    {
        PL_DestroyEventQueue( (tsa +i)->eventQueue );
    }
    PR_Free( tsa );
    
    /*
    ** All done! Log completion and return
    */
    PR_LOG( lm, PR_LOG_NOTICE,("TestEvent(): Ending" ));
    return 0;
} /* end TestEvents() */




static void Help( void )
{
    printf( "Event: Help\n"
            "syntax: event [-d][-h]\n"
            "where: -h gives this message\n"
            "       -d sets debug mode (a no-op here)\n"
        );    
} /* end Help() */

PRIntn main(PRIntn argc, char **argv )
{
    PLOptStatus os;
  	PLOptState *opt = PL_CreateOptState(argc, argv, "dhmt:i:");


	while (PL_OPT_EOL != (os = PL_GetNextOpt(opt)))
    {
		if (PL_OPT_BAD == os) continue;
        switch (opt->option)
        {
        case 'm':  /* use monitored event queue */
            useMonitoredQueue = PR_TRUE;
            break;
        case 'd':  /* debug mode (noop) */
            break;
        case 't':  /* needs guidance */
            numThreads = atol(opt->value);
            break;
        case 'i':  /* needs guidance */
            iterations = atol(opt->value);
            break;
        case 'h':  /* needs guidance */
        default:
            Help();
            return 2;
        }
    }
	PL_DestroyOptState(opt);

    TestEvents();
    /*
    ** Evaluate results and exit
    */
    if (failed)
      {
        printf("FAILED\n");
        return(1);
      }
    else
      {
        printf("PASSED\n");
        return(0);
      }
}  /* end main() */

/* end event.c */
