/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.0 (the "NPL"); you may not use this file except in
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

/* Mac specific viewer driver - a sample application for NGLayout */

// PowerPlant
#include <LWindow.h>
#include <PP_Messages.h>
#include <PP_Resources.h>
#include <PPobClasses.h>
#include <UDrawingState.h>
#include <UMemoryMgr.h>
#include <URegistrar.h>
#include <LApplication.h>

// Netscape
#include "prlog.h"
#include "xp_trace.h"
#include "nsViewer.h"

const ResIDT	window_Sample		= 1;	// EXAMPLE

class nsMacViewer : public nsViewer {
    // From nsViewer
  public:
    virtual void AddMenu(nsIWidget* aMainWindow);
};

void nsMacViewer::AddMenu(nsIWidget* aMainWindow)
{
	PR_ASSERT(FALSE);
}

#pragma mark class nsMacApplication 

// 
// Prototype simple NGLayout application
// 
class	nsMacApplication : public LApplication {

	nsMacViewer * mViewer;

public:
									 nsMacApplication();
		virtual				~nsMacApplication();
			
		virtual Boolean		ObeyCommand(CommandT inCommand, void* ioParam);	
	
		virtual void		FindCommandStatus(CommandT inCommand,
									Boolean &outEnabled, Boolean &outUsesMark,
									Char16 &outMark, Str255 outName);
protected:

		virtual void		StartUp();		// overriding startup functions

};

nsMacApplication::nsMacApplication()
{ 
	// Register functions to create core PowerPlant classes
	RegisterAllPPClasses();
	mViewer = new nsMacViewer();
	SetViewer(mViewer);
}

nsMacApplication::~nsMacApplication()
{
		delete mViewer;
}

void nsMacApplication::StartUp()
{
	nsIWidget *mainWindow = nsnull;
 // nsDocLoader* dl = aViewer->SetupViewer(&mainWindow, 0, 0);

	ObeyCommand(cmd_New, nil);		// EXAMPLE, create a new window
}

Boolean  nsMacApplication::ObeyCommand( CommandT	inCommand, void *ioParam)
{
	Boolean		cmdHandled = true;

	switch (inCommand) {
	 			
		case cmd_New:
										// EXAMPLE, create a new window

			LWindow		*theWindow;
			theWindow = LWindow::CreateWindow(window_Sample, this);	
			theWindow->Show();
			break;



		default:
			cmdHandled = LApplication::ObeyCommand(inCommand, ioParam);
			break;
	}
	
	return cmdHandled;
}

// ---------------------------------------------------------------------------
//		¥ FindCommandStatus
// ---------------------------------------------------------------------------
//	This function enables menu commands.
//

void
nsMacApplication::FindCommandStatus(
	CommandT	inCommand,
	Boolean		&outEnabled,
	Boolean		&outUsesMark,
	Char16		&outMark,
	Str255		outName)
{

	switch (inCommand) {

		case cmd_New:					// EXAMPLE
			outEnabled = true;			// enable the New command
			break;

		default:
			LApplication::FindCommandStatus(inCommand, outEnabled,
												outUsesMark, outMark, outName);
			break;
	}
}

void main( void )
{
	SetDebugThrow_(debugAction_Nothing);
	SetDebugSignal_(debugAction_Nothing);
	
#ifdef DEBUG
	SetDebugSignal_(debugAction_LowLevelDebugger);	// debugAction_SourceDebugger, but SysBreak broken in MetroNub 1.3.2
	XP_TraceInit();
#endif
	
	// Initialize standard Toolbox managers
	UQDGlobals::InitializeToolbox( &qd );

	nsMacApplication * app = new nsMacApplication;
	app->Run();
	delete app;
}