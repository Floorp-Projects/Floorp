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
// ===========================================================================
//	CCompleteApp.cp 			©1994-1998 Metrowerks Inc. All rights reserved.
// ===========================================================================
//	This file contains the starter code for a complete PowerPlant project that
//  includes precompiled headers and both debug and release targets.

#include "macshell.h"

#include <LGrowZone.h>
#include <LWindow.h>
#include <PP_Messages.h>
#include <PP_Resources.h>
#include <PPobClasses.h>
#include <UDrawingState.h>
#include <UMemoryMgr.h>
#include <URegistrar.h>
#include "macstdlibextras.h"

#include "prthread.h"
#include "prinit.h"
#include "prlog.h"

#include "serv.h"

// put declarations for resource ids (ResIDTs) here

const PP_PowerPlant::ResIDT	wind_SampleWindow = 128;	// EXAMPLE, create a new window

//
// NSPR repeater
//

class CNSPRRepeater : public LPeriodical
{
public:
	CNSPRRepeater();
	virtual void SpendTime(const EventRecord& inEvent);
};

CNSPRRepeater::CNSPRRepeater(void)
	: LPeriodical()
{
}

CNSPRRepeater *gNSPRRepeater;

PRBool gShouldQuit = PR_FALSE;

void
CNSPRRepeater::SpendTime(const EventRecord& inEvent)
{
	/*  
		Can't just exit on the Mac, because otherwise the NSPR threads
		will keep spinning forever. We have to send the app a
		Quit command so that all the threads will shut down in an orderly
		way. 
		
		On top of that, NSPR threads can't send a Quit Apple Event, because
		that causes the threads to close improperly. So, we keep a flag and allow
		threads to raise it. We check for that flag here.
	*/

	if (gShouldQuit)
	{
		if (gNSPRRepeater)
			gNSPRRepeater->StopIdling();
		// PR_Interrupt all threads (other than the primordial one)
		SSM_KillAllThreads();
		gTheApp->SendAEQuit(); // will cause this repeater to stop repeating
	}
	else
		PR_Sleep(PR_INTERVAL_NO_WAIT); // give time to NSPR threads
}

// ===========================================================================
//		¥ Main Program
// ===========================================================================

PRIntn MacPSMMain(PRIntn argc, char **argv);

int main()
{
	char *fakeArgv[] = { NULL, NULL };
#ifndef NDEBUG									
	SetDebugThrow_(PP_PowerPlant::debugAction_Alert);	// Set Debugging options
	SetDebugSignal_(PP_PowerPlant::debugAction_Alert);
#endif

	PP_PowerPlant::InitializeHeap(3);					// Initialize Memory Manager
														// Parameter is number of Master Pointer
														// blocks to allocate

#ifdef DEBUG
	InitializeSIOUX(false);	// prevent us from getting double menus, etc.
#endif
	PP_PowerPlant::UQDGlobals::InitializeToolbox(&qd);	// Initialize standard Toolbox managers
	
	new PP_PowerPlant::LGrowZone(20000);				// Install a GrowZone function to catch
														// low memory situations.
	PR_Initialize(MacPSMMain, 0, fakeArgv, 0);
	return 0;
}

CBasicApp *gTheApp;

PRIntn MacPSMMain(PRIntn argc, char **argv)
{
	gNSPRRepeater = new CNSPRRepeater;
	PR_ASSERT(gNSPRRepeater != NULL);
	gNSPRRepeater->StartIdling();
	
	CBasicApp	theApp;									// create instance of your application
	gTheApp = &theApp;

	theApp.Run();
	return 0;
}

// ---------------------------------------------------------------------------
//		¥ CBasicApp
// ---------------------------------------------------------------------------
//	Constructor

CBasicApp::CBasicApp()
{
#ifndef NDEBUG
	PP_PowerPlant::RegisterAllPPClasses();		// Register functions to create core
#else											// PowerPlant classes								
	RegisterClass_(PP_PowerPlant::LWindow);
	RegisterClass_(PP_PowerPlant::LCaption);
#endif	
}


// ---------------------------------------------------------------------------
//		¥ ~CBasicApp
// ---------------------------------------------------------------------------
//	Destructor

CBasicApp::~CBasicApp()
{
}

// ---------------------------------------------------------------------------
//		¥ StartUp
// ---------------------------------------------------------------------------
//	This method lets you do something when the application starts up
//	without a document. For example, you could issue your own new command.

extern void RunMacPSM(void *);

void
CBasicApp::StartUp()
{
	// ObeyCommand(PP_PowerPlant::cmd_New, nil);			// EXAMPLE, create a new window
	
	/*
		The Unix/Win32 main function (which we call RunMacPSM) blocks on 
		PR_Accept(), listening for control connections. Since we cannot 
		block from the main thread, we have to spin a separate NSPR thread 
		for RunMacPSM.
	*/
    SSM_CreateAndRegisterThread(PR_USER_THREAD, 
								RunMacPSM, 
								NULL, 
								PR_PRIORITY_NORMAL,
								PR_LOCAL_THREAD,
								PR_UNJOINABLE_THREAD, 0);
}

// ---------------------------------------------------------------------------
//		¥ ObeyCommand
// ---------------------------------------------------------------------------
//	This method lets the application respond to commands like Menu commands

Boolean
CBasicApp::ObeyCommand(
	PP_PowerPlant::CommandT	inCommand,
	void					*ioParam)
{
	Boolean		cmdHandled = true;

	switch (inCommand) {
	
		// Handle command messages (defined in PP_Messages.h).
		case PP_PowerPlant::cmd_New:
#if 0
			PP_PowerPlant::LWindow	*theWindow = 
									PP_PowerPlant::LWindow::CreateWindow(wind_SampleWindow, this);
			ThrowIfNil_(theWindow);

			// LWindow is not initially visible in PPob resource
			theWindow->Show();
			break;
#endif

		case PP_PowerPlant::cmd_Quit:
			if (!gShouldQuit) // do this only if repeater hasn't done it for us
			{
				if (gNSPRRepeater)
					gNSPRRepeater->StopIdling();
				// PR_Interrupt all threads (other than the primordial one)
				SSM_KillAllThreads();
			}
			// fall through to default Quit behavior

		// Any that you don't handle, such as cmd_About and cmd_Quit,
		// will be passed up to LApplication
		default:
			cmdHandled = PP_PowerPlant::LApplication::ObeyCommand(inCommand, ioParam);
			break;
	}
	
	return cmdHandled;
}

// ---------------------------------------------------------------------------
//		¥ FindCommandStatus
// ---------------------------------------------------------------------------
//	This method enables menu items.

void
CBasicApp::FindCommandStatus(
	PP_PowerPlant::CommandT	inCommand,
	Boolean					&outEnabled,
	Boolean					&outUsesMark,
	PP_PowerPlant::Char16	&outMark,
	Str255					outName)
{

	switch (inCommand) {
	
		// Return menu item status according to command messages.
		case PP_PowerPlant::cmd_New:
		case PP_PowerPlant::cmd_Quit:
			outEnabled = true;
			break;

		// Any that you don't handle, such as cmd_About and cmd_Quit,
		// will be passed up to LApplication
		default:
			PP_PowerPlant::LApplication::FindCommandStatus(inCommand, outEnabled,
												outUsesMark, outMark, outName);
			break;
	}
}
