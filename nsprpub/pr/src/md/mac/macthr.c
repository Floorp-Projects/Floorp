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

#include "primpl.h"

#include <Types.h>
#include <Timer.h>
#include <OSUtils.h>

#include <LowMem.h>

// This should be in LowMem.h
#ifndef LMSetStackLowPoint
#define LMSetStackLowPoint(value)		\
	*((UInt32 *)(0x110)) = (value)
#endif

PRThread *gPrimaryThread = NULL;

PR_IMPLEMENT(PRThread *) PR_GetPrimaryThread()
{
	return gPrimaryThread;
}

//##############################################################################
//##############################################################################
#pragma mark -
#pragma mark CREATING MACINTOSH THREAD STACKS


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

	seg->vaddr = (char *)malloc(size);

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
	
	/*
	**	Turn off the snack stiffer.  The NSPR stacks are allocated in the 
	**	application's heap; this throws the stack sniffer for a tizzy.
	**	Note that the sniffer does not run on machines running the thread manager.
	**	Yes, we will blast the low-mem every time a new stack is created.  We can afford
	**	a couple extra cycles.
	*/
	LMSetStackLowPoint(0);
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

    if (_PR_MD_GET_INTSOFF()) {
        cpu->u.missed[cpu->where] |= _PR_MISSED_CLOCK;
		PrimeTime((QElemPtr)tmTaskPtr, kMacTimerInMiliSecs);
		return;
    }
    _PR_MD_SET_INTSOFF(1);

	//	And tell nspr that a clock interrupt occured.
	_PR_ClockInterrupt();
	
	if ((_PR_RUNQREADYMASK(cpu)) >> ((_PR_MD_CURRENT_THREAD()->priority)))
		_PR_SET_RESCHED_FLAG();
	
    _PR_MD_SET_INTSOFF(0);

	//	Reset the clock timer so that we fire again.
	PrimeTime((QElemPtr)tmTaskPtr, kMacTimerInMiliSecs);
}

#if GENERATINGCFM

RoutineDescriptor 	gTimerCallbackRD = BUILD_ROUTINE_DESCRIPTOR(uppTimerProcInfo, &TimerCallback);

#else

asm void gTimerCallbackRD(void)
{
	//	Check out LocalA5.  If it is zero, then
	//	it is our first time through, and we should
	//	store away our A5.  If not, then we are
	//	a real time manager callback, so we should
	//	store away A5, set up our local a5, jsr
	//	to our callback, and then restore the
	//	previous A5.
	
	lea			LocalA5, a0
	move.l		(a0), d0
	cmpi.l		#0, d0
	bne			TimerProc
	
	move.l		a5, (a0)
	rts

TimerProc:
	
	//	Save A5, restore our local A5
	
	move.l		a5, -(sp)
	move.l		d0, a5 
	
	//	Jump to our C routine
	
	move.l 		a1, -(sp)
	jsr 		TimerCallback
	
	//	Restore the previous A5
	
	move.l		(sp)+, a5

	rts

LocalA5:

	dc.l		0
	
}

#endif

void _MD_StartInterrupts(void)
{
	gPrimaryThread = _PR_MD_CURRENT_THREAD();

	//	If we are not generating CFM-happy code, then make sure that 
	//	we call our callback wrapper once so that we can
	//	save away our A5.

#if !GENERATINGCFM
	gTimerCallbackRD();
#endif

	//	Fill in the Time Manager queue element
	
	gTimeManagerTaskElem.tmAddr = (TimerUPP)&gTimerCallbackRD;
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
#pragma unused (timeout)

	unsigned long finalTicks;
	
	if (timeout != PR_INTERVAL_NO_WAIT) {
	    Delay(1,&finalTicks);
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

	_PR_INTSOFF(is);
	PR_Lock(thread->md.asyncIOLock);
	if (timeout == PR_INTERVAL_NO_TIMEOUT) {
	    while ((thread->io_pending) && (status == PR_SUCCESS))
	        status = PR_WaitCondVar(thread->md.asyncIOCVar, PR_INTERVAL_NO_TIMEOUT);
	} else {
	    while ((thread->io_pending) && ((PRIntervalTime)(PR_IntervalNow() - timein) < timeout))
	        status = PR_WaitCondVar(thread->md.asyncIOCVar, timeout);
	}
	if ((status == PR_FAILURE) && (PR_GetError() == PR_PENDING_INTERRUPT_ERROR)) {
		thread->md.osErrCode = kEINTRErr;
	} else if (thread->io_pending) {
		thread->md.osErrCode = kETIMEDOUTErr;
		PR_SetError(PR_IO_TIMEOUT_ERROR, kETIMEDOUTErr);
	}
	PR_Unlock(thread->md.asyncIOLock);
	_PR_FAST_INTSON(is);
}

void DoneWaitingOnThisThread(PRThread *thread)
{
    intn is;

	_PR_INTSOFF(is);
	PR_Lock(thread->md.asyncIOLock);
    thread->io_pending = PR_FALSE;
	/* let the waiting thread know that async IO completed */
	PR_NotifyCondVar(thread->md.asyncIOCVar);
	PR_Unlock(thread->md.asyncIOLock);
	_PR_FAST_INTSON(is);
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
