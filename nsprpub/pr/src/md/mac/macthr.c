/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* 
 * The contents of this file are subject to the Mozilla Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/MPL/
 * 
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 * 
 * The Original Code is the Netscape Portable Runtime (NSPR).
 * 
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation.  Portions created by Netscape are 
 * Copyright (C) 1998-2000 Netscape Communications Corporation.  All
 * Rights Reserved.
 * 
 * Contributor(s):
 * 
 * Alternatively, the contents of this file may be used under the
 * terms of the GNU General Public License Version 2 or later (the
 * "GPL"), in which case the provisions of the GPL are applicable 
 * instead of those above.  If you wish to allow use of your 
 * version of this file only under the terms of the GPL and not to
 * allow others to use your version of this file under the MPL,
 * indicate your decision by deleting the provisions above and
 * replace them with the notice and other provisions required by
 * the GPL.  If you do not delete the provisions above, a recipient
 * may use your version of this file under either the MPL or the
 * GPL.
 */

#include "primpl.h"

#include <string.h>

#include <MacTypes.h>
#include <Timer.h>
#include <OSUtils.h>

#include <LowMem.h>
#include <Multiprocessing.h>
#include <Gestalt.h>

#include "mdcriticalregion.h"

TimerUPP	gTimerCallbackUPP	= NULL;
PRThread *	gPrimaryThread		= NULL;

PR_IMPLEMENT(PRThread *) PR_GetPrimaryThread()
{
	return gPrimaryThread;
}

//##############################################################################
//##############################################################################
#pragma mark -
#pragma mark CREATING MACINTOSH THREAD STACKS

#if defined(GC_LEAK_DETECTOR)
extern void* GC_malloc_atomic(PRUint32 size);
#endif

/*
**	Allocate a new memory segment.  We allocate it from our figment heap.  Currently,
**	it is being used for per thread stack space.
**	
**	Return the segment's access rights and size.  vaddr is used on Unix platforms to
**	map an existing address for the segment.
*/
PRStatus _MD_AllocSegment(PRSegment *seg, PRUint32 size, void *vaddr)
{
	PR_ASSERT(seg != 0);
	PR_ASSERT(size != 0);
	PR_ASSERT(vaddr == 0);

	/*	
	** Take the actual memory for the segment out of our Figment heap.
	*/

#if defined(GC_LEAK_DETECTOR)
	seg->vaddr = (char *)GC_malloc_atomic(size);
#else
	seg->vaddr = (char *)malloc(size);
#endif

	if (seg->vaddr == NULL) {

#if DEBUG
		DebugStr("\p_MD_AllocSegment failed.");
#endif

		return PR_FAILURE;
	}

	seg->size = size;	

	return PR_SUCCESS;
}


/*
**	Free previously allocated memory segment.
*/
void _MD_FreeSegment(PRSegment *seg)
{
	PR_ASSERT((seg->flags & _PR_SEG_VM) == 0);

	if (seg->vaddr != NULL)
		free(seg->vaddr);
}


/*
**	The thread's stack has been allocated and its fields are already properly filled
**	in by PR.  Perform any debugging related initialization here.
**
**	Put a recognizable pattern so that we can find it from Macsbug.
**	Put a cookie at the top of the stack so that we can find it from Macsbug.
*/
void _MD_InitStack(PRThreadStack *ts, int redZoneBytes)
	{
#pragma unused (redZoneBytes)
#if DEVELOPER_DEBUG
	//	Put a cookie at the top of the stack so that we can find 
	//	it from Macsbug.
	
	memset(ts->allocBase, 0xDC, ts->stackSize);
	
	((UInt32 *)ts->stackTop)[-1] = 0xBEEFCAFE;
	((UInt32 *)ts->stackTop)[-2] = (UInt32)gPrimaryThread;
	((UInt32 *)ts->stackTop)[-3] = (UInt32)(ts);
	((UInt32 *)ts->stackBottom)[0] = 0xCAFEBEEF;
#else
#pragma unused (ts)
#endif	
	}

extern void _MD_ClearStack(PRThreadStack *ts)
	{
#if DEVELOPER_DEBUG
	//	Clear out our cookies. 
	
	memset(ts->allocBase, 0xEF, ts->allocSize);
	((UInt32 *)ts->stackTop)[-1] = 0;
	((UInt32 *)ts->stackTop)[-2] = 0;
	((UInt32 *)ts->stackTop)[-3] = 0;
	((UInt32 *)ts->stackBottom)[0] = 0;
#else
#pragma unused (ts)
#endif
	}


//##############################################################################
//##############################################################################
#pragma mark -
#pragma mark TIME MANAGER-BASED CLOCK

TMTask		gTimeManagerTaskElem;

extern void _MD_IOInterrupt(void);
_PRInterruptTable _pr_interruptTable[] = {
    { "clock", _PR_MISSED_CLOCK, _PR_ClockInterrupt, },
    { "i/o", _PR_MISSED_IO, _MD_IOInterrupt, },
    { 0 }
};

pascal void TimerCallback(TMTaskPtr tmTaskPtr)
{
    _PRCPU *cpu = _PR_MD_CURRENT_CPU();
    PRIntn is;

    if (_PR_MD_GET_INTSOFF()) {
        cpu->u.missed[cpu->where] |= _PR_MISSED_CLOCK;
        PrimeTime((QElemPtr)tmTaskPtr, kMacTimerInMiliSecs);
        return;
    }

    _PR_INTSOFF(is);

    //	And tell nspr that a clock interrupt occured.
    _PR_ClockInterrupt();
	
    if ((_PR_RUNQREADYMASK(cpu)) >> ((_PR_MD_CURRENT_THREAD()->priority)))
        _PR_SET_RESCHED_FLAG();
	
    _PR_FAST_INTSON(is);

    //	Reset the clock timer so that we fire again.
    PrimeTime((QElemPtr)tmTaskPtr, kMacTimerInMiliSecs);
}


void _MD_StartInterrupts(void)
{
	gPrimaryThread = _PR_MD_CURRENT_THREAD();

	if ( !gTimerCallbackUPP )
		gTimerCallbackUPP = NewTimerProc(TimerCallback);

	//	Fill in the Time Manager queue element
	
	gTimeManagerTaskElem.tmAddr = (TimerUPP)gTimerCallbackUPP;
	gTimeManagerTaskElem.tmCount = 0;
	gTimeManagerTaskElem.tmWakeUp = 0;
	gTimeManagerTaskElem.tmReserved = 0;

	//	Make sure that our time manager task is ready to go.
	InsTime((QElemPtr)&gTimeManagerTaskElem);
	
	PrimeTime((QElemPtr)&gTimeManagerTaskElem, kMacTimerInMiliSecs);
}

void _MD_StopInterrupts(void)
{
	if (gTimeManagerTaskElem.tmAddr != NULL) {
		RmvTime((QElemPtr)&gTimeManagerTaskElem);
		gTimeManagerTaskElem.tmAddr = NULL;
	}
}

void _MD_PauseCPU(PRIntervalTime timeout)
{
    if (timeout != PR_INTERVAL_NO_WAIT)
    {
        EventRecord theEvent;
	
         /*
            ** Calling WaitNextEvent() here is suboptimal. This routine should
            ** pause the process until IO or the timeout occur, yielding time to
            ** other processes on operating systems that require this (Mac OS classic).
            ** WaitNextEvent() may incur too much latency, and has other problems,
            ** such as the potential to drop suspend/resume events, and to handle
            ** AppleEvents at a time at which we're not prepared to handle them.
         */
        (void) WaitNextEvent(nullEvent, &theEvent, 1, NULL);
	   
        (void) _MD_IOInterrupt();
    }
}


//##############################################################################
//##############################################################################
#pragma mark -
#pragma mark THREAD SUPPORT FUNCTIONS

#include <OpenTransport.h> /* for error codes */

PRStatus _MD_InitThread(PRThread *thread)
{
	thread->md.asyncIOLock = PR_NewLock();
	PR_ASSERT(thread->md.asyncIOLock != NULL);
	thread->md.asyncIOCVar = PR_NewCondVar(thread->md.asyncIOLock);
	PR_ASSERT(thread->md.asyncIOCVar != NULL);

	if (thread->md.asyncIOLock == NULL || thread->md.asyncIOCVar == NULL)
		return PR_FAILURE;
	else
		return PR_SUCCESS;
}

PRStatus _MD_wait(PRThread *thread, PRIntervalTime timeout)
{
#pragma unused (timeout)

	_MD_SWITCH_CONTEXT(thread);
	return PR_SUCCESS;
}


void WaitOnThisThread(PRThread *thread, PRIntervalTime timeout)
{
    intn is;
    PRIntervalTime timein = PR_IntervalNow();
	PRStatus status = PR_SUCCESS;

    // Turn interrupts off to avoid a race over lock ownership with the callback
    // (which can fire at any time). Interrupts may stay off until we leave
    // this function, or another NSPR thread turns them back on. They certainly
    // stay off until PR_WaitCondVar() relinquishes the asyncIOLock lock, which
    // is what we care about.
	_PR_INTSOFF(is);
	PR_Lock(thread->md.asyncIOLock);
	if (timeout == PR_INTERVAL_NO_TIMEOUT) {
	    while ((thread->io_pending) && (status == PR_SUCCESS))
	        status = PR_WaitCondVar(thread->md.asyncIOCVar, PR_INTERVAL_NO_TIMEOUT);
	} else {
	    while ((thread->io_pending) && ((PRIntervalTime)(PR_IntervalNow() - timein) < timeout) && (status == PR_SUCCESS))
	        status = PR_WaitCondVar(thread->md.asyncIOCVar, timeout);
	}
	if ((status == PR_FAILURE) && (PR_GetError() == PR_PENDING_INTERRUPT_ERROR)) {
		thread->md.osErrCode = kEINTRErr;
	} else if (thread->io_pending) {
		thread->md.osErrCode = kETIMEDOUTErr;
		PR_SetError(PR_IO_TIMEOUT_ERROR, kETIMEDOUTErr);
	}

	thread->io_pending = PR_FALSE;
	PR_Unlock(thread->md.asyncIOLock);
	_PR_FAST_INTSON(is);
}


void DoneWaitingOnThisThread(PRThread *thread)
{
    intn is;

    PR_ASSERT(thread->md.asyncIOLock->owner == NULL);

	// DoneWaitingOnThisThread() is called from OT notifiers and async file I/O
	// callbacks that can run at "interrupt" time (Classic Mac OS) or on pthreads
	// that may run concurrently with the main threads (Mac OS X). They can thus
	// be called when any NSPR thread is running, or even while NSPR is in a
	// thread context switch. It is therefore vital that we can guarantee to
	// be able to get the asyncIOLock without blocking (thus avoiding code
	// that makes assumptions about the current NSPR thread etc). To achieve
	// this, we use NSPR interrrupts as a semaphore on the lock; all code 
	// that grabs the lock also disables interrupts for the time the lock
	// is held. Callers of DoneWaitingOnThisThread() thus have to check whether
	// interrupts are already off, and, if so, simply set the missed_IO flag on
	// the CPU rather than calling this function.
	
	_PR_INTSOFF(is);
	PR_Lock(thread->md.asyncIOLock);
	thread->io_pending = PR_FALSE;
	/* let the waiting thread know that async IO completed */
	PR_NotifyCondVar(thread->md.asyncIOCVar);
	PR_Unlock(thread->md.asyncIOLock);
	_PR_FAST_INTSON(is);
}


PR_IMPLEMENT(void) PR_Mac_WaitForAsyncNotify(PRIntervalTime timeout)
{
    intn is;
    PRIntervalTime timein = PR_IntervalNow();
	PRStatus status = PR_SUCCESS;
    PRThread *thread = _PR_MD_CURRENT_THREAD();

    // See commments in WaitOnThisThread()
	_PR_INTSOFF(is);
	PR_Lock(thread->md.asyncIOLock);
	if (timeout == PR_INTERVAL_NO_TIMEOUT) {
	    while ((!thread->md.asyncNotifyPending) && (status == PR_SUCCESS))
	        status = PR_WaitCondVar(thread->md.asyncIOCVar, PR_INTERVAL_NO_TIMEOUT);
	} else {
	    while ((!thread->md.asyncNotifyPending) && ((PRIntervalTime)(PR_IntervalNow() - timein) < timeout) && (status == PR_SUCCESS))
	        status = PR_WaitCondVar(thread->md.asyncIOCVar, timeout);
	}
	if ((status == PR_FAILURE) && (PR_GetError() == PR_PENDING_INTERRUPT_ERROR)) {
		thread->md.osErrCode = kEINTRErr;
	} else if (!thread->md.asyncNotifyPending) {
		thread->md.osErrCode = kETIMEDOUTErr;
		PR_SetError(PR_IO_TIMEOUT_ERROR, kETIMEDOUTErr);
	}
	thread->md.asyncNotifyPending = PR_FALSE;
	PR_Unlock(thread->md.asyncIOLock);
	_PR_FAST_INTSON(is);
}


void AsyncNotify(PRThread *thread)
{
    intn is;
	
    PR_ASSERT(thread->md.asyncIOLock->owner == NULL);

    // See commments in DoneWaitingOnThisThread()
	_PR_INTSOFF(is);
	PR_Lock(thread->md.asyncIOLock);
	thread->md.asyncNotifyPending = PR_TRUE;
	/* let the waiting thread know that async IO completed */
	PR_NotifyCondVar(thread->md.asyncIOCVar);
	PR_Unlock(thread->md.asyncIOLock);
	_PR_FAST_INTSON(is);
}


PR_IMPLEMENT(void) PR_Mac_PostAsyncNotify(PRThread *thread)
{
	_PRCPU *  cpu = _PR_MD_CURRENT_CPU();
	
	if (_PR_MD_GET_INTSOFF()) {
		thread->md.missedAsyncNotify = PR_TRUE;
		cpu->u.missed[cpu->where] |= _PR_MISSED_IO;
	} else {
		AsyncNotify(thread);
	}
}


//##############################################################################
//##############################################################################
#pragma mark -
#pragma mark PROCESS SUPPORT FUNCTIONS

PRProcess * _MD_CreateProcess(
    const char *path,
    char *const *argv,
    char *const *envp,
    const PRProcessAttr *attr)
{
#pragma unused (path, argv, envp, attr)

	PR_SetError(PR_NOT_IMPLEMENTED_ERROR, unimpErr);
	return NULL;
}

PRStatus _MD_DetachProcess(PRProcess *process)
{
#pragma unused (process)

	PR_SetError(PR_NOT_IMPLEMENTED_ERROR, unimpErr);
	return PR_FAILURE;
}

PRStatus _MD_WaitProcess(PRProcess *process, PRInt32 *exitCode)
{
#pragma unused (process, exitCode)

	PR_SetError(PR_NOT_IMPLEMENTED_ERROR, unimpErr);
	return PR_FAILURE;
}

PRStatus _MD_KillProcess(PRProcess *process)
{
#pragma unused (process)

	PR_SetError(PR_NOT_IMPLEMENTED_ERROR, unimpErr);
	return PR_FAILURE;
}
//##############################################################################
//##############################################################################
#pragma mark -
#pragma mark INTERRUPT SUPPORT

#if TARGET_CARBON

/*
     This critical region support is required for Mac NSPR to work correctly on dual CPU
     machines on Mac OS X. This note explains why.

     NSPR uses a timer task, and has callbacks for async file I/O and Open Transport
     whose runtime behaviour differs depending on environment. On "Classic" Mac OS
     these run at "interrupt" time (OS-level interrupts, that is, not NSPR interrupts),
     and can thus preempt other code, but they always run to completion.

     On Mac OS X, these are all emulated using MP tasks, which sit atop pthreads. Thus,
     they can be preempted at any time (and not necessarily run to completion), and can
     also run *concurrently* with eachother, and with application code, on multiple
     CPU machines. Note that all NSPR threads are emulated, and all run on the main
     application MP task.

     We thus have to use MP critical sections to protect data that is shared between
     the various callbacks and the main MP thread. It so happens that NSPR has this
     concept of software interrupts, and making interrupt-off times be critical
     sections works.

*/


/*  
    Whether to use critical regions. True if running on Mac OS X and later
*/

PRBool  gUseCriticalRegions;

/*
    Count of the number of times we've entered the critical region.
    We need this because ENTER_CRITICAL_REGION() will *not* block when
    called from different NSPR threads (which all run on one MP thread),
    and we need to ensure that when code turns interrupts back on (by
    settings _pr_intsOff to 0) we exit the critical section enough times
    to leave it.
*/

PRInt32 gCriticalRegionEntryCount;


void _MD_SetIntsOff(PRInt32 ints)
{
    ENTER_CRITICAL_REGION();
    gCriticalRegionEntryCount ++;
    
    _pr_intsOff = ints;
    
    if (!ints)
    {
        PRInt32     i = gCriticalRegionEntryCount;

        gCriticalRegionEntryCount = 0;
        for ( ;i > 0; i --) {
            LEAVE_CRITICAL_REGION();
        }
    }
}


#endif /* TARGET_CARBON */


//##############################################################################
//##############################################################################
#pragma mark -
#pragma mark CRITICAL REGION SUPPORT

#if MAC_CRITICAL_REGIONS

MDCriticalRegionID  gCriticalRegion;

void InitCriticalRegion()
{
    long        systemVersion;
    OSStatus    err;    
    
    // we only need to do critical region stuff on Mac OS X    
    err = Gestalt(gestaltSystemVersion, &systemVersion);
    gUseCriticalRegions = (err == noErr) && (systemVersion >= 0x00001000);
    
    if (!gUseCriticalRegions) return;
    
    err = MD_CriticalRegionCreate(&gCriticalRegion);
    PR_ASSERT(err == noErr);
}

void TermCriticalRegion()
{
    OSStatus    err;    

    if (!gUseCriticalRegions) return;

    err = MD_CriticalRegionDelete(gCriticalRegion);
    PR_ASSERT(err == noErr);
}


void EnterCritialRegion()
{
    OSStatus    err;
    
    if (!gUseCriticalRegions) return;

    PR_ASSERT(gCriticalRegion != kInvalidID);
    
    /* Change to a non-infinite timeout for debugging purposes */
    err = MD_CriticalRegionEnter(gCriticalRegion, kDurationForever /* 10000 * kDurationMillisecond */ );
    PR_ASSERT(err == noErr);
}

void LeaveCritialRegion()
{
    OSStatus    err;    

    if (!gUseCriticalRegions) return;

    PR_ASSERT(gCriticalRegion != kInvalidID);

    err = MD_CriticalRegionExit(gCriticalRegion);
    PR_ASSERT(err == noErr);
}


#endif // MAC_CRITICAL_REGIONS

