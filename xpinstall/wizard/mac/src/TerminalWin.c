/* -*- Mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* The contents of this file are subject to the Netscape Public License
 * Version 1.1 (the "License"); you may not use this file except in
 * compliance with the License. You may obtain a copy of the License at
 * http://www.mozilla.org/NPL/ 
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License 
 * for the specific language governing rights and limitations under the 
 * License. 
 * 
 * The Original Code is Mozilla Communicator client code, released March
 * 31, 1998. 
 *
 * The Initial Developer of the Original Code is Netscape Communications
 * Corporation. Portions created by Netscape are Copyright (C) 1999
 * Netscape Communications Corporation. All Rights Reserved.  
 * 
 * Contributors:
 *     Samir Gehani <sgehani@netscape.com>
 */


#include "MacInstallWizard.h"

/*-----------------------------------------------------------*
 *   Terminal Window
 *-----------------------------------------------------------*/

void 
ShowTerminalWin(void)
{
	Str255		next, back;
	Handle		rectH;
	Rect		viewRect;
	short		reserr;
	GrafPtr		oldPort;
	GetPort(&oldPort);
	//dougt: check for gWPtr being null
	SetPort(gWPtr);
	
    //dougt: think about changing the constant to something more readable.
	gCurrWin = TERMINAL; 
	/* gControls->tw = (TermWin*) NewPtrClear(sizeof(TermWin)); */
	
	GetIndString(next, rStringList, sInstallBtn);
	GetIndString(back, rStringList, sBackBtn);
	
	// malloc and get control
	rectH = Get1Resource('RECT', rStartMsgBox);
	reserr = ResError();  //dougt: this does not do what you thing.  It does not always return the last error.
	if (reserr == noErr)
		viewRect = (Rect) **((Rect **)rectH);
	else
	{
		ErrorHandler();
		return;
	}
	gControls->tw->startMsgBox = viewRect;
	
	gControls->tw->startMsg = TENew(&viewRect, &viewRect);
    //dougt: check for null
	
	// populate control
    //dougt: remove hi.
	HLockHi(gControls->cfg->startMsg);
	TESetText(*gControls->cfg->startMsg, strlen(*gControls->cfg->startMsg), 
				gControls->tw->startMsg);
	HUnlock(gControls->cfg->startMsg);
	
	// show controls
	TEUpdate(&viewRect, gControls->tw->startMsg);
	ShowNavButtons( back, next);
	
	SetPort(oldPort);
}

void
UpdateTerminalWin(void)
{
	GrafPtr	oldPort;
	GetPort(&oldPort);
	SetPort(gWPtr);
	
	TEUpdate(&gControls->tw->startMsgBox, gControls->tw->startMsg);
	
	SetPort(oldPort);
}

void 
InTerminalContent(EventRecord* evt, WindowPtr wCurrPtr)
{	
	Point 	localPt;
	Rect	r;
	ControlPartCode	part;
	ThreadID		tid;
	
	GrafPtr	oldPort;
	GetPort(&oldPort);
	
	SetPort(wCurrPtr);
	localPt = evt->where;
	GlobalToLocal( &localPt);
					
	HLockHi((Handle)gControls->backB);
	r = (**(gControls->backB)).contrlRect;
	HUnlock((Handle)gControls->backB);
	if (PtInRect( localPt, &r))
	{
		part = TrackControl(gControls->backB, evt->where, NULL);
		if (part)
		{
			KillControls(gWPtr);
			if (&gControls->tw->startMsgBox)
			{
				EraseRect(&gControls->tw->startMsgBox);
				InvalRect(&gControls->tw->startMsgBox);
			}
			else
			{
				ErrorHandler();	
				return;
			}
			
			/* treat last setup type  selection as custom */
			if (gControls->opt->instChoice == gControls->cfg->numSetupTypes)
				ShowComponentsWin();
			else
				ShowSetupTypeWin();
			return;
		}
	}
			
	HLockHi((Handle)gControls->nextB);			
	r = (**(gControls->nextB)).contrlRect;
	HUnlock((Handle)gControls->nextB);
	if (PtInRect( localPt, &r))
	{
		part = TrackControl(gControls->nextB, evt->where, NULL);
		if (part)
		{
			SpawnSDThread(Install, &tid);
			return;
		}
	}
	SetPort(oldPort);
}

Boolean
SpawnSDThread(ThreadEntryProcPtr threadProc, ThreadID *tid)
{
	OSErr			err;
	
	err = NewThread(kCooperativeThread, (ThreadEntryProcPtr)threadProc, (void*) nil, 
					20000, kCreateIfNeeded, (void**)nil, tid);
	if (err == noErr)
		YieldToThread(*tid); /* force ctx switch */
	else
	{
		return false;
	}
	
	return true;
}

void
EnableTerminalWin(void)
{
	EnableNavButtons();

	// TO DO
}

void
DisableTerminalWin(void)
{
	DisableNavButtons();
	
	// TO DO
}