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
** prtrace.c -- NSPR Trace Instrumentation
**
** Implement the API defined in prtrace.h
**
**
**
*/

#if defined (DEBUG) || defined (FORCE_NSPR_TRACE)
#include <string.h>
#include "prtrace.h"
#include "prclist.h"
#include "prlock.h"
#include "prcvar.h"
#include "prio.h"
#include "prlog.h"
#include "prenv.h"
#include "prmem.h"
#include "prerror.h"


#define DEFAULT_TRACE_BUFSIZE ( 1024 * 1024 )
#define DEFAULT_BUFFER_SEGMENTS    2

/*
** Enumerate states in a RName structure
*/
typedef enum TraceState
{
    Running = 1,
    Suspended = 2
} TraceState;

/*
** Define QName structure
*/
typedef struct QName
{
    PRCList link;
    PRCList rNameList;
    char    name[PRTRACE_NAME_MAX+1];
} QName;

/*
** Define RName structure
*/
typedef struct RName
{
    PRCList link;
    PRLock  *lock;
    QName   *qName;
    TraceState state;
    char    name[PRTRACE_NAME_MAX+1];
    char    desc[PRTRACE_DESC_MAX+1];
} RName;


/*
** The Trace Facility database
**
*/
static PRLogModuleInfo *lm;

static PRLock      *traceLock;      /* Facility Lock */
static PRCList     qNameList;       /* anchor to all QName structures */
static TraceState traceState = Running;

/*
** in-memory trace buffer controls
*/
static  PRTraceEntry    *tBuf;      /* pointer to buffer */
static  PRInt32         bufSize;    /* size of buffer, in bytes, rounded up to sizeof(PRTraceEntry) */
static  volatile PRInt32  next;     /* index to next PRTraceEntry */
static  PRInt32         last;       /* index of highest numbered trace entry */

/*
** Real-time buffer capture controls
*/
static PRInt32 fetchLastSeen = 0;
static PRBool  fetchLostData = PR_FALSE;

/*
** Buffer write-to-file controls
*/
static  PRLock      *logLock;               /* Sync lock */
static  PRCondVar   *logCVar;               /* Sync Condidtion Variable */
/*
** Inter-thread state communication.
** Controling thread writes to logOrder under protection of logCVar
** the logging thread reads logOrder and sets logState on Notify.
** 
** logSegments, logCount, logLostData must be read and written under
** protection of logLock, logCVar.
** 
*/
static  enum LogState
{
    LogNotRunning,  /* Initial state */
    LogReset,       /* Causes logger to re-calc controls */
    LogActive,      /* Logging in progress, set only by log thread */
    LogSuspend,     /* Suspend Logging */ 
    LogResume,      /* Resume Logging => LogActive */ 
    LogStop         /* Stop the log thread */
}   logOrder, logState, localState;         /* controlling state variables */
static  PRInt32     logSegments;            /* Number of buffer segments */
static  PRInt32     logEntries;             /* number of Trace Entries in the buffer */
static  PRInt32     logEntriesPerSegment;   /* number of PRTraceEntries per buffer segment */
static  PRInt32     logSegSize;             /* size of buffer segment */
static  PRInt32     logCount;               /* number of segments pending output */
static  PRInt32     logLostData;            /* number of lost log buffer segments */

/*
** end Trace Database
**
*/

/*
** _PR_InitializeTrace() -- Initialize the trace facility
*/
static void NewTraceBuffer( PRInt32 size )
{
    /*
    ** calculate the size of the buffer
    ** round down so that each segment has the same number of
    ** trace entries
    */
    logSegments = DEFAULT_BUFFER_SEGMENTS;
    logEntries = size / sizeof(PRTraceEntry);
    logEntriesPerSegment = logEntries / logSegments;
    logEntries = logSegments * logEntriesPerSegment;
    bufSize = logEntries * sizeof(PRTraceEntry);
    logSegSize = logEntriesPerSegment * sizeof(PRTraceEntry);
    PR_ASSERT( bufSize != 0);
    PR_LOG( lm, PR_LOG_ERROR,
        ("NewTraceBuffer: logSegments: %ld, logEntries: %ld, logEntriesPerSegment: %ld, logSegSize: %ld",
            logSegments, logEntries, logEntriesPerSegment, logSegSize ));


    tBuf = PR_Malloc( bufSize );
    if ( tBuf == NULL )
    {
        PR_LOG( lm, PR_LOG_ERROR,
            ("PRTrace: Failed to get trace buffer"));
        PR_ASSERT( 0 );
    } 
    else
    {
        PR_LOG( lm, PR_LOG_NOTICE,
            ("PRTrace: Got trace buffer of size: %ld, at %p", bufSize, tBuf));
    }

    next = 0;
    last = logEntries -1;
    logCount = 0;
    logLostData = PR_TRUE; /* not really on first call */
    logOrder = LogReset;

} /* end NewTraceBuffer() */

/*
** _PR_InitializeTrace() -- Initialize the trace facility
*/
static void _PR_InitializeTrace( void )
{
    /* The lock pointer better be null on this call */
    PR_ASSERT( traceLock == NULL );

    traceLock = PR_NewLock();
    PR_ASSERT( traceLock != NULL );

    PR_Lock( traceLock );
    
    PR_INIT_CLIST( &qNameList );

    lm = PR_NewLogModule("trace");

    bufSize = DEFAULT_TRACE_BUFSIZE;
    NewTraceBuffer( bufSize );

    /* Initialize logging controls */
    logLock = PR_NewLock();
    logCVar = PR_NewCondVar( logLock );

    PR_Unlock( traceLock );
    return;    
} /* end _PR_InitializeTrace() */

/*
** Create a Trace Handle
*/
PR_IMPLEMENT(PRTraceHandle)
	PR_CreateTrace( 
    	const char *qName,          /* QName for this trace handle */
	    const char *rName,          /* RName for this trace handle */
	    const char *description     /* description for this trace handle */
)
{
    QName   *qnp;
    RName   *rnp;
    PRBool  matchQname = PR_FALSE;

    /* Self initialize, if necessary */
    if ( traceLock == NULL )
        _PR_InitializeTrace();

    /* Validate input arguments */
    PR_ASSERT( strlen(qName) <= PRTRACE_NAME_MAX );
    PR_ASSERT( strlen(rName) <= PRTRACE_NAME_MAX );
    PR_ASSERT( strlen(description) <= PRTRACE_DESC_MAX );

    PR_LOG( lm, PR_LOG_DEBUG,
            ("PRTRACE: CreateTrace: Qname: %s, RName: %s", qName, rName));

    /* Lock the Facility */
    PR_Lock( traceLock );

    /* Do we already have a matching QName? */
    if (!PR_CLIST_IS_EMPTY( &qNameList ))
    {
        qnp = (QName *) PR_LIST_HEAD( &qNameList );
        do {
            if ( strcmp(qnp->name, qName) == 0)
            {
                matchQname = PR_TRUE;
                break;
            }
            qnp = (QName *)PR_NEXT_LINK( &qnp->link );
        } while( qnp != (QName *)PR_LIST_HEAD( &qNameList ));
    }
    /*
    ** If we did not find a matching QName,
    **    allocate one and initialize it.
    **    link it onto the qNameList.
    **
    */
    if ( matchQname != PR_TRUE )
    {
        qnp = PR_NEWZAP( QName );
        PR_ASSERT( qnp != NULL );
        PR_INIT_CLIST( &qnp->link ); 
        PR_INIT_CLIST( &qnp->rNameList ); 
        strcpy( qnp->name, qName );
        PR_APPEND_LINK( &qnp->link, &qNameList ); 
    }

    /* Do we already have a matching RName? */
    if (!PR_CLIST_IS_EMPTY( &qnp->rNameList ))
    {
        rnp = (RName *) PR_LIST_HEAD( &qnp->rNameList );
        do {
            /*
            ** No duplicate RNames are allowed within a QName
            **
            */
            PR_ASSERT( strcmp(rnp->name, rName));
            rnp = (RName *)PR_NEXT_LINK( &rnp->link );
        } while( rnp != (RName *)PR_LIST_HEAD( &qnp->rNameList ));
    }

    /* Get a new RName structure; initialize its members */
    rnp = PR_NEWZAP( RName );
    PR_ASSERT( rnp != NULL );
    PR_INIT_CLIST( &rnp->link );
    strcpy( rnp->name, rName );
    strcpy( rnp->desc, description );
    rnp->lock = PR_NewLock();
    rnp->state = Running;
    if ( rnp->lock == NULL )
    {
        PR_ASSERT(0);
    }

    PR_APPEND_LINK( &rnp->link, &qnp->rNameList ); /* add RName to QName's rnList */    
    rnp->qName = qnp;                       /* point the RName to the QName */

    /* Unlock the Facility */
    PR_Unlock( traceLock );
    PR_LOG( lm, PR_LOG_DEBUG, ("PRTrace: Create: QName: %s %p, RName: %s %p\n\t",
        qName, qnp, rName, rnp ));

    return((PRTraceHandle)rnp);
} /* end  PR_CreateTrace() */

/*
**
*/
PR_IMPLEMENT(void) 
	PR_DestroyTrace( 
		PRTraceHandle handle    /* Handle to be destroyed */
)
{
    RName   *rnp = (RName *)handle;
    QName   *qnp = rnp->qName;

    PR_LOG( lm, PR_LOG_DEBUG, ("PRTrace: Deleting: QName: %s, RName: %s", 
        qnp->name, rnp->name));

    /* Lock the Facility */
    PR_Lock( traceLock );

    /*
    ** Remove RName from the list of RNames in QName
    ** and free RName
    */
    PR_LOG( lm, PR_LOG_DEBUG, ("PRTrace: Deleting RName: %s, %p", 
        rnp->name, rnp));
    PR_REMOVE_LINK( &rnp->link );
    PR_Free( rnp->lock );
    PR_DELETE( rnp );

    /*
    ** If this is the last RName within QName
    **   remove QName from the qNameList and free it
    */
    if ( PR_CLIST_IS_EMPTY( &qnp->rNameList ) )
    {
        PR_LOG( lm, PR_LOG_DEBUG, ("PRTrace: Deleting unused QName: %s, %p", 
            qnp->name, qnp));
        PR_REMOVE_LINK( &qnp->link );
        PR_DELETE( qnp );
    } 

    /* Unlock the Facility */
    PR_Unlock( traceLock );
    return;
} /* end PR_DestroyTrace()  */

/*
** Create a TraceEntry in the trace buffer
*/
PR_IMPLEMENT(void) 
	PR_Trace( 
    	PRTraceHandle handle,       /* use this trace handle */
	    PRUint32    userData0,      /* User supplied data word 0 */
	    PRUint32    userData1,      /* User supplied data word 1 */
	    PRUint32    userData2,      /* User supplied data word 2 */
	    PRUint32    userData3,      /* User supplied data word 3 */
	    PRUint32    userData4,      /* User supplied data word 4 */
	    PRUint32    userData5,      /* User supplied data word 5 */
	    PRUint32    userData6,      /* User supplied data word 6 */
	    PRUint32    userData7       /* User supplied data word 7 */
)
{
    PRTraceEntry   *tep;
    PRInt32         mark;

    if ( (traceState == Suspended ) 
        || ( ((RName *)handle)->state == Suspended )) 
        return;

    /*
    ** Get the next trace entry slot w/ minimum delay
    */
    PR_Lock( traceLock );

    tep = &tBuf[next++]; 
    if ( next > last )
        next = 0;
    if ( fetchLostData == PR_FALSE && next == fetchLastSeen )
        fetchLostData = PR_TRUE;
    
    mark = next;
        
    PR_Unlock( traceLock );

    /*
    ** We have a trace entry. Fill it in.
    */
    tep->thread = PR_GetCurrentThread();
    tep->handle = handle;
    tep->time   = PR_Now();
    tep->userData[0] = userData0;
    tep->userData[1] = userData1;
    tep->userData[2] = userData2;
    tep->userData[3] = userData3;
    tep->userData[4] = userData4;
    tep->userData[5] = userData5;
    tep->userData[6] = userData6;
    tep->userData[7] = userData7;

    /* When buffer segment is full, signal trace log thread to run */
    if (( mark % logEntriesPerSegment) == 0 )
    {
        PR_Lock( logLock );
        logCount++;
        PR_NotifyCondVar( logCVar );
        PR_Unlock( logLock );
        /*
        ** Gh0D! This is awful!
        ** Anyway, to minimize lost trace data segments,
        ** I inserted the PR_Sleep(0) to cause a context switch
        ** so that the log thread could run.
        ** I know, it perturbs the universe and may cause
        ** funny things to happen in the optimized builds.
        ** Take it out, loose data; leave it in risk Heisenberg.
        */
        /* PR_Sleep(0); */
    }

    return;
} /* end PR_Trace() */

/*
**
*/
PR_IMPLEMENT(void) 
	PR_SetTraceOption( 
	    PRTraceOption command,  /* One of the enumerated values */
	    void *value             /* command value or NULL */
)
{
    RName * rnp;

    switch ( command )
    {
        case PRTraceBufSize :
            PR_Lock( traceLock );
            PR_Free( tBuf );
            bufSize = *(PRInt32 *)value;
            NewTraceBuffer( bufSize );
            PR_Unlock( traceLock );
            PR_LOG( lm, PR_LOG_DEBUG,
                ("PRSetTraceOption: PRTraceBufSize: %ld", bufSize));
            break;
        
        case PRTraceEnable :
            rnp = *(RName **)value;
            rnp->state = Running;
            PR_LOG( lm, PR_LOG_DEBUG,
                ("PRSetTraceOption: PRTraceEnable: %p", rnp));
            break;
        
        case PRTraceDisable :
            rnp = *(RName **)value;
            rnp->state = Suspended;
            PR_LOG( lm, PR_LOG_DEBUG,
                ("PRSetTraceOption: PRTraceDisable: %p", rnp));
            break;
        
        case PRTraceSuspend :
            traceState = Suspended;
            PR_LOG( lm, PR_LOG_DEBUG,
                ("PRSetTraceOption: PRTraceSuspend"));
            break;
        
        case PRTraceResume :
            traceState = Running;
            PR_LOG( lm, PR_LOG_DEBUG,
                ("PRSetTraceOption: PRTraceResume"));
            break;
        
        case PRTraceSuspendRecording :
            PR_Lock( logLock );
            logOrder = LogSuspend;
            PR_NotifyCondVar( logCVar );
            PR_Unlock( logLock );
            PR_LOG( lm, PR_LOG_DEBUG,
                ("PRSetTraceOption: PRTraceSuspendRecording"));
            break;
        
        case PRTraceResumeRecording :
            PR_LOG( lm, PR_LOG_DEBUG,
                ("PRSetTraceOption: PRTraceResumeRecording"));
            if ( logState != LogSuspend )
                break;
            PR_Lock( logLock );
            logOrder = LogResume;
            PR_NotifyCondVar( logCVar );
            PR_Unlock( logLock );
            break;
        
        case PRTraceStopRecording :
            PR_Lock( logLock );
            logOrder = LogStop;
            PR_NotifyCondVar( logCVar );
            PR_Unlock( logLock );
            PR_LOG( lm, PR_LOG_DEBUG,
                ("PRSetTraceOption: PRTraceStopRecording"));
            break;

        case PRTraceLockHandles :
            PR_LOG( lm, PR_LOG_DEBUG,
                ("PRSetTraceOption: PRTraceLockTraceHandles"));
            PR_Lock( traceLock );
            break;
        
        case PRTraceUnLockHandles :
            PR_LOG( lm, PR_LOG_DEBUG,
                ("PRSetTraceOption: PRTraceUnLockHandles"));
            PR_Lock( traceLock );
            break;

        default:
            PR_LOG( lm, PR_LOG_ERROR,
                ("PRSetTraceOption: Invalid command %ld", command ));
            PR_ASSERT( 0 );
            break;
    } /* end switch() */
    return;
} /* end  PR_SetTraceOption() */

/*
**
*/
PR_IMPLEMENT(void) 
	PR_GetTraceOption( 
    	PRTraceOption command,  /* One of the enumerated values */
	    void *value             /* command value or NULL */
)
{
    switch ( command )
    {
        case PRTraceBufSize :
            *((PRInt32 *)value) = bufSize;
            PR_LOG( lm, PR_LOG_DEBUG,
                ("PRGetTraceOption: PRTraceBufSize: %ld", bufSize ));
            break;
        
        default:
            PR_LOG( lm, PR_LOG_ERROR,
                ("PRGetTraceOption: Invalid command %ld", command ));
            PR_ASSERT( 0 );
            break;
    } /* end switch() */
    return;
} /* end PR_GetTraceOption() */

/*
**
*/
PR_IMPLEMENT(PRTraceHandle) 
	PR_GetTraceHandleFromName( 
    	const char *qName,      /* QName search argument */
        const char *rName       /* RName search argument */
)
{
    const char    *qn, *rn, *desc;
    PRTraceHandle     qh, rh;
    RName   *rnp = NULL;

    PR_LOG( lm, PR_LOG_DEBUG, ("PRTrace: GetTraceHandleFromName:\n\t"
        "QName: %s, RName: %s", qName, rName ));

    qh = PR_FindNextTraceQname( NULL );
    while (qh != NULL)
    {
        rh = PR_FindNextTraceRname( NULL, qh );
        while ( rh != NULL )
        {
            PR_GetTraceNameFromHandle( rh, &qn, &rn, &desc );
            if ( (strcmp( qName, qn ) == 0)
                && (strcmp( rName, rn ) == 0 ))
            {
                rnp = (RName *)rh;
                goto foundIt;
            }
            rh = PR_FindNextTraceRname( rh, qh );
        }
        qh = PR_FindNextTraceQname( NULL );
    }

foundIt:
    PR_LOG( lm, PR_LOG_DEBUG, ("PR_Counter: GetConterHandleFromName: %p", rnp ));
    return(rh);
} /* end PR_GetTraceHandleFromName() */

/*
**
*/
PR_IMPLEMENT(void) 
	PR_GetTraceNameFromHandle( 
    	PRTraceHandle handle,       /* handle as search argument */
	    const char **qName,         /* pointer to associated QName */
	    const char **rName,         /* pointer to associated RName */
    	const char **description    /* pointer to associated description */
)
{
    RName   *rnp = (RName *)handle;
    QName   *qnp = rnp->qName;

    *qName = qnp->name;
    *rName = rnp->name;
    *description = rnp->desc;

    PR_LOG( lm, PR_LOG_DEBUG, ("PRTrace: GetConterNameFromHandle: "
        "QNp: %p, RNp: %p,\n\tQName: %s, RName: %s, Desc: %s", 
        qnp, rnp, qnp->name, rnp->name, rnp->desc ));

    return;
} /* end PR_GetTraceNameFromHandle() */

/*
**
*/
PR_IMPLEMENT(PRTraceHandle) 
	PR_FindNextTraceQname( 
        PRTraceHandle handle
)
{
    QName *qnp = (QName *)handle;

    if ( PR_CLIST_IS_EMPTY( &qNameList ))
            qnp = NULL;
    else if ( qnp == NULL )
        qnp = (QName *)PR_LIST_HEAD( &qNameList );
    else if ( PR_NEXT_LINK( &qnp->link ) ==  &qNameList )
        qnp = NULL;
    else  
        qnp = (QName *)PR_NEXT_LINK( &qnp->link );

    PR_LOG( lm, PR_LOG_DEBUG, ("PRTrace: FindNextQname: Handle: %p, Returns: %p", 
        handle, qnp ));

    return((PRTraceHandle)qnp);
} /* end PR_FindNextTraceQname() */

/*
**
*/
PR_IMPLEMENT(PRTraceHandle) 
	PR_FindNextTraceRname( 
        PRTraceHandle rhandle,
        PRTraceHandle qhandle
)
{
    RName *rnp = (RName *)rhandle;
    QName *qnp = (QName *)qhandle;


    if ( PR_CLIST_IS_EMPTY( &qnp->rNameList ))
        rnp = NULL;
    else if ( rnp == NULL )
        rnp = (RName *)PR_LIST_HEAD( &qnp->rNameList );
    else if ( PR_NEXT_LINK( &rnp->link ) ==  &qnp->rNameList )
        rnp = NULL;
    else
        rnp = (RName *)PR_NEXT_LINK( &rnp->link );

    PR_LOG( lm, PR_LOG_DEBUG, ("PRTrace: FindNextRname: Rhandle: %p, QHandle: %p, Returns: %p", 
        rhandle, qhandle, rnp ));

    return((PRTraceHandle)rnp);
} /* end PR_FindNextTraceRname() */
    
/*
**
*/
static PRFileDesc * InitializeRecording( void )
{
    char    *logFileName;
    PRFileDesc  *logFile;

    /* Self initialize, if necessary */
    if ( traceLock == NULL )
        _PR_InitializeTrace();

    PR_LOG( lm, PR_LOG_DEBUG,
        ("PR_RecordTraceEntries: begins"));

    logLostData = 0; /* reset at entry */
    logState = LogReset;

    /* Get the filename for the logfile from the environment */
    logFileName = PR_GetEnv( "NSPR_TRACE_LOG" );
    if ( logFileName == NULL )
    {
        PR_LOG( lm, PR_LOG_ERROR,
            ("RecordTraceEntries: Environment variable not defined. Exiting"));
        return NULL;
    }
    
    /* Open the logfile */
    logFile = PR_Open( logFileName, PR_WRONLY | PR_CREATE_FILE, 0666 );
    if ( logFile == NULL )
    {
        PR_LOG( lm, PR_LOG_ERROR,
            ("RecordTraceEntries: Cannot open %s as trace log file. OS error: %ld", 
		logFileName, PR_GetOSError()));
        return NULL;
    }
    return logFile;
} /* end InitializeRecording() */

/*
**
*/
static void ProcessOrders( void )
{
    switch ( logOrder )
    {
    case LogReset :
        logOrder = logState = localState;
        PR_LOG( lm, PR_LOG_DEBUG,
            ("RecordTraceEntries: LogReset"));
        break;

    case LogSuspend :
        localState = logOrder = logState = LogSuspend;
        PR_LOG( lm, PR_LOG_DEBUG,
            ("RecordTraceEntries: LogSuspend"));
        break;

    case LogResume :
        localState = logOrder = logState = LogActive;
        PR_LOG( lm, PR_LOG_DEBUG,
            ("RecordTraceEntries: LogResume"));
        break;

    case LogStop :
        logOrder = logState = LogStop;
        PR_LOG( lm, PR_LOG_DEBUG,
            ("RecordTraceEntries: LogStop"));
        break;

    default :
        PR_LOG( lm, PR_LOG_ERROR,
            ("RecordTraceEntries: Invalid logOrder: %ld", logOrder ));
        PR_ASSERT( 0 );
        break;
    } /* end switch() */
    return ;
} /* end ProcessOrders() */

/*
**
*/
static void WriteTraceSegment( PRFileDesc *logFile, void *buf, PRInt32 amount )
{
    PRInt32 rc;


    PR_LOG( lm, PR_LOG_ERROR,
        ("WriteTraceSegment: Buffer: %p, Amount: %ld", buf, amount));
    rc = PR_Write( logFile, buf , amount );
    if ( rc == -1 )
        PR_LOG( lm, PR_LOG_ERROR,
            ("RecordTraceEntries: PR_Write() failed. Error: %ld", PR_GetError() ));
    else if ( rc != amount )
        PR_LOG( lm, PR_LOG_ERROR,
            ("RecordTraceEntries: PR_Write() Tried to write: %ld, Wrote: %ld", amount, rc));
    else 
        PR_LOG( lm, PR_LOG_DEBUG,
            ("RecordTraceEntries: PR_Write(): Buffer: %p, bytes: %ld", buf, amount));

    return;
} /* end WriteTraceSegment() */

/*
**
*/
PR_IMPLEMENT(void)
	PR_RecordTraceEntries(
        void 
)
{
    PRFileDesc  *logFile;
    PRInt32     lostSegments;
    PRInt32     currentSegment = 0;
    void        *buf;
    PRBool      doWrite;

    logFile = InitializeRecording();
    if ( logFile == NULL )
    {
        PR_LOG( lm, PR_LOG_DEBUG,
            ("PR_RecordTraceEntries: Failed to initialize"));
        return;
    }

    /* Do this until told to stop */
    while ( logState != LogStop )
    {

        PR_Lock( logLock );

        while ( (logCount == 0) && ( logOrder == logState ) )
            PR_WaitCondVar( logCVar, PR_INTERVAL_NO_TIMEOUT );

        /* Handle state transitions */
        if ( logOrder != logState )
            ProcessOrders();

        /* recalculate local controls */
        if ( logCount )
        {
            lostSegments = logCount - logSegments;
            if ( lostSegments > 0 )
            {
                logLostData += ( logCount - logSegments );
                logCount = (logCount % logSegments);
                currentSegment = logCount;
                PR_LOG( lm, PR_LOG_DEBUG,
                    ("PR_RecordTraceEntries: LostData segments: %ld", logLostData));
            }
            else
            {
                logCount--;
            }

            buf = tBuf + ( logEntriesPerSegment * currentSegment );
            if (++currentSegment >= logSegments )
                currentSegment = 0;
            doWrite = PR_TRUE;
        }
        else
            doWrite = PR_FALSE;

        PR_Unlock( logLock );

        if ( doWrite == PR_TRUE )
        {
            if ( localState != LogSuspend )
                WriteTraceSegment( logFile, buf, logSegSize );
            else
                PR_LOG( lm, PR_LOG_DEBUG,
                    ("RecordTraceEntries: PR_Write(): is suspended" ));
        }

    } /* end while(logState...) */

    PR_Close( logFile );
    PR_LOG( lm, PR_LOG_DEBUG,
        ("RecordTraceEntries: exiting"));
    return;
} /* end  PR_RecordTraceEntries() */

/*
**
*/
PR_IMPLEMENT(PRIntn)
    PR_GetTraceEntries(
        PRTraceEntry    *buffer,    /* where to write output */
        PRInt32         count,      /* number to get */
        PRInt32         *found      /* number you got */
)
{
    PRInt32 rc; 
    PRInt32 copied = 0;
    
    PR_Lock( traceLock );
    
    /*
    ** Depending on where the LastSeen and Next indices are,
    ** copy the trace buffer in one or two pieces. 
    */
    PR_LOG( lm, PR_LOG_ERROR,
        ("PR_GetTraceEntries: Next: %ld, LastSeen: %ld", next, fetchLastSeen));

    if ( fetchLastSeen <= next )
    {
        while (( count-- > 0 ) && (fetchLastSeen < next ))
        {
            *(buffer + copied++) = *(tBuf + fetchLastSeen++);
        }
        PR_LOG( lm, PR_LOG_ERROR,
            ("PR_GetTraceEntries: Copied: %ld, LastSeen: %ld", copied, fetchLastSeen));
    }
    else /* copy in 2 parts */
    {
        while ( count-- > 0  && fetchLastSeen <= last )
        {
            *(buffer + copied++) = *(tBuf + fetchLastSeen++);
        }
        fetchLastSeen = 0;

        PR_LOG( lm, PR_LOG_ERROR,
            ("PR_GetTraceEntries: Copied: %ld, LastSeen: %ld", copied, fetchLastSeen));

        while ( count-- > 0  && fetchLastSeen < next )
        {
            *(buffer + copied++) = *(tBuf + fetchLastSeen++);
        }
        PR_LOG( lm, PR_LOG_ERROR,
            ("PR_GetTraceEntries: Copied: %ld, LastSeen: %ld", copied, fetchLastSeen));
    }

    *found = copied;
    rc = ( fetchLostData == PR_TRUE )? 1 : 0;
    fetchLostData = PR_FALSE;

    PR_Unlock( traceLock );
    return rc;
} /* end PR_GetTraceEntries() */

#else /* !defined(FORCE_NSPR_TRACE) */
/*
** The trace facility is not defined when !DEBUG and !FORCE_NSPR_TRACE
**
*/

/* Some compilers don't like an empty compilation unit. */
static int dummy = 0;
#endif /* defined(FORCE_NSPR_TRACE) */

/* end prtrace.c */
