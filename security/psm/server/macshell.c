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
 * The Original Code is the Netscape security libraries.
 * 
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation.  Portions created by Netscape are 
 * Copyright (C) 1994-2000 Netscape Communications Corporation.  All
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

/*
 * See <http://developer.apple.com/technotes/tn/tn1070.html> for information about BOAs
 *
 */
 

#include <AppleEvents.h>
#include <Gestalt.h>
#include <Errors.h>
#include <LowMem.h>

#include "prthread.h"
#include "prinit.h"
#include "prlog.h"

#include "serv.h"
#include "dataconn.h"

#include "macshell.h"
#include "macglue.h"

#include "macstdlibextras.h"

#define kSleepMax	20

static PRBool	 	gQuitFlag;
static long 		gSleepVal;


//----------------------------------------------------------------------------
PRBool GetQuitFlag()
{
	return gQuitFlag;
}

//----------------------------------------------------------------------------
void SetQuitFlag(PRBool quitNow)
{
	gQuitFlag = quitNow;
}


// Apple event handlers to be installed

//----------------------------------------------------------------------------
static pascal OSErr DoAEOpenApplication(AppleEvent * theAppleEvent, AppleEvent * replyAppleEvent, long refCon)
{
    return noErr;
}

//----------------------------------------------------------------------------
static pascal OSErr DoAEOpenDocuments(AppleEvent * theAppleEvent, AppleEvent * replyAppleEvent, long refCon)
{
    return errAEEventNotHandled;
}

//----------------------------------------------------------------------------
static pascal OSErr DoAEPrintDocuments(AppleEvent * theAppleEvent, AppleEvent * replyAppleEvent, long refCon)
{
    return errAEEventNotHandled;
}

//----------------------------------------------------------------------------
static pascal OSErr DoAEQuitApplication(AppleEvent * theAppleEvent, AppleEvent * replyAppleEvent, long refCon)
{
    gQuitFlag = PR_TRUE;
    return noErr;
}

// install Apple event handlers
//----------------------------------------------------------------------------
static void InitAppleEventsStuff(void)
{
    OSErr err;

	err = AEInstallEventHandler(kCoreEventClass, kAEOpenApplication, NewAEEventHandlerProc(DoAEOpenApplication), 0, false);

	if (err == noErr)
		err = AEInstallEventHandler(kCoreEventClass, kAEOpenDocuments, NewAEEventHandlerProc(DoAEOpenDocuments), 0, false);

	if (err == noErr)
		err = AEInstallEventHandler(kCoreEventClass, kAEPrintDocuments, NewAEEventHandlerProc(DoAEPrintDocuments), 0, false);

	if (err == noErr)
		err = AEInstallEventHandler(kCoreEventClass, kAEQuitApplication, NewAEEventHandlerProc(DoAEQuitApplication), 0, false);

#if DEBUG
	if (err != noErr)
		DebugStr("\pInstall event handler failed");
#endif
}

// high-level event dispatching
//----------------------------------------------------------------------------
static void DoHighLevelEvent(EventRecord * theEventRecPtr)
{
    (void) AEProcessAppleEvent(theEventRecPtr);
}

#pragma mark -


//----------------------------------------------------------------------------
// Increase the space allocated for the background only application stack.
 //
 // Warning: SetApplLimit always sets the stack to at least as large as the
 //    default stack for the machine (8K on machines with original QuickDraw,
 //    24K on machines with Color QuickDraw), so the application partition
 //    must be large enough to accommodate an appropriate stack and heap.
 //    Call this only once, at the beginning of the BOA.
 //
 // Another warning:
 //    Don't bother trying to set the stack size to something lower than 24K.
 //    If SetApplLimit is called to do this, it will silently lower ApplLimit
 //    to a 24K stack. (The limit is 8K on machines without Color QuickDraw.
 //    In this sample, we don't allow an increment less than 24K.)

static OSErr IncreaseBOAStack(Size additionalStackSize)
{
	OSErr err = noErr;
#if !TARGET_CARBON
	// Check that we aren't running with a corrupt heap. If we are,
	// fix the problem.  This was a bug with FBA's before System 7.5.5.
	// With System Software later than 7.5.5, this "fix" is harmless.
	THz	myZone = GetZone();
	if (myZone->bkLim != LMGetHeapEnd())
		LMSetHeapEnd(myZone->bkLim);

	// Increase the stack size by lowering the heap limit.
	SetApplLimit((Ptr) ((unsigned long) GetApplLimit() - additionalStackSize));
	err = MemError();
	if (err == noErr) MaxApplZone();
#endif
	return err;
}
   
//----------------------------------------------------------------------------
static void MemoryInit(long numMasterBlocks)
{
	long i;
	
	IncreaseBOAStack(1024 * 64);
	
	for (i = 0; i < numMasterBlocks; i ++)
		MoreMasters();
}

#pragma mark -


//----------------------------------------------------------------------------
static void DoIdleProcessing()
{
	PR_Sleep(10);
}
extern short SIOUXHandleOneEvent(EventRecord *userevent);
//----------------------------------------------------------------------------
static OSErr HandleOneEvent()
{
	EventRecord	theEvent;
	Boolean		gotEvent;
	OSErr		err = noErr;

	// tweak the sleep time depending on whether cartman is doing anything or not	
	gSleepVal = AreConnectionsActive() ? 0 : kSleepMax;

	gotEvent = WaitNextEvent(everyEvent, &theEvent, gSleepVal, nil);

	if (gotEvent)
	{
		switch (theEvent.what)
		{
			case kHighLevelEvent:
				DoHighLevelEvent(&theEvent);
				break;
			default:
				SIOUXHandleOneEvent(&theEvent);
		}
	}
	else
	{
		DoIdleProcessing();
	}

	return noErr;
}


//----------------------------------------------------------------------------
static void DoApplicationCleanup()
{
		SSM_KillAllThreads();
}

void RunMacPSM(void *);
//----------------------------------------------------------------------------
static PRIntn DoMainEventLoop(PRIntn argc, char **argv)
{
	
  SSM_CreateAndRegisterThread(PR_USER_THREAD, 
							  RunMacPSM, 
							  NULL, 
							  PR_PRIORITY_NORMAL,
							  PR_LOCAL_THREAD,
							  PR_UNJOINABLE_THREAD, 0);

	// let's let NSPR threads start off
	PR_Sleep(PR_INTERVAL_NO_WAIT); // give time to NSPR threads

	// main event loop
	while (!gQuitFlag)
	{
		OSErr	err = HandleOneEvent();
	}

	return 0;
}


#pragma mark -

extern void InitializeMacToolbox();

//----------------------------------------------------------------------------
void main(void)
{
	long gestResponse;
	Boolean	haveAppleEvents;
	OSErr err;
	char *argv[] = { NULL, NULL };
	
    InitializeMacToolbox();

	// initialize QuickDraw globals. This is the only Toolbox init that we
	// should do for an FBA
	//InitGraf(&qd.thePort);
	//InitializeSIOUX(true);
	MemoryInit(10);
	
	// initialize application globals

	gQuitFlag = PR_FALSE;
	gSleepVal = kSleepMax;

	// is the Apple Event Manager available?
	err = Gestalt(gestaltAppleEventsAttr, &gestResponse);
	if (err == noErr && (gestResponse & (1 << gestaltAppleEventsPresent)) != 0)
		haveAppleEvents = true;
		
	if (!haveAppleEvents) return;

	// install Apple event handlers
	InitAppleEventsStuff();

	// setup the app. This calls into NSPR, which calls our DoMainEventLoop() function.
	
	PR_Initialize(DoMainEventLoop, 0, argv, 0);
	
	DoApplicationCleanup();
}
