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


/*--------------------------------------------------------------*
 *   Event Handlers
 *--------------------------------------------------------------*/
void HandleNextEvent(EventRecord* nextEvt)
{
	switch(nextEvt->what)
	{
		case mouseDown:
			HandleMouseDown(nextEvt);
			break;
			
		case keyDown:
			HandleKeyDown(nextEvt);
			break;
			
		case updateEvt:
			HandleUpdateEvt(nextEvt);
			break;
			
		case activateEvt:
			HandleActivateEvt(nextEvt);
			break;
			
		case osEvt:
			HandleOSEvt(nextEvt);
			break;
		
		case nullEvent:
			TEIdle(gControls->lw->licTxt);
			break;
	}
}

void HandleMouseDown(EventRecord* evt)
{
	WindowPtr			wCurrPtr;
	SInt16				cntlPartCode;
	SInt32				menuChoice;
	
	cntlPartCode = FindWindow(evt->where, &wCurrPtr);
	
	switch(cntlPartCode)
	{
		case inMenuBar:
			//AdjustMenus();
			menuChoice = MenuSelect(evt->where);
			//HandleMenuChoice(menuChoice);
			break;
		
		case inContent:
			if (wCurrPtr != FrontWindow())
			{
				/* InvalRect(&qd.screenBits.bounds); */
				SelectWindow(wCurrPtr);
			}
			else
				React2InContent(evt, wCurrPtr);  //dougt: what does this do?  
			break;
		
		case inDrag:
			DragWindow(wCurrPtr, evt->where, &qd.screenBits.bounds);
			InvalRect(&qd.screenBits.bounds);
			break;
			
		case inGoAway:
			// TO DO
			break;
	}
}

void HandleKeyDown(EventRecord* evt)
{
	char keyPressed;
	ThreadID		tid;
	
	keyPressed = evt->message & charCodeMask;
	if ( (keyPressed == 'z') || (keyPressed == 'Z'))
		gDone = true;	// backdoor exit      //dougt:  Get rid of this.  (or make it debug only)
	if (keyPressed == '\r')                   //dougt: what about tab, esc, arrows, doublebyte?
	{		
		switch(gCurrWin)
		{
			case LICENSE:
				KillControls(gWPtr);
				ShowWelcomeWin();
				return;
			case WELCOME:				
				KillControls(gWPtr);
				ShowSetupTypeWin();
				return;
			case SETUP_TYPE:
				KillControls(gWPtr);
				
				/* treat last setup type selection as custom */
				if (gControls->opt->instChoice == gControls->cfg->numSetupTypes)
					ShowComponentsWin();
				else
					ShowTerminalWin();
				return;				
			case COMPONENTS:
				KillControls(gWPtr);
				ShowTerminalWin();
				return;
			case TERMINAL:
				SpawnSDThread(Install, &tid);
				return;
			default:
				break; // never reached
		}
	}
}

void HandleUpdateEvt(EventRecord* evt)
{
	WindowPtr	wCurrPtr;
	SInt16		cntlPartCode;
	Rect		bounds;
	
	GrafPtr	oldPort;
	GetPort(&oldPort);
	
	SetPort( gWPtr );
		
	cntlPartCode = FindWindow( evt->where, &wCurrPtr );
	//dougt: check for null
	BeginUpdate( gWPtr );
	DrawControls( gWPtr );
	ShowLogo();
	
	switch(gCurrWin)
	{
		case LICENSE:
		case WELCOME:
			ShowTxt();
			break;
		case SETUP_TYPE:
			ShowSetupDescTxt();
			break;
		case COMPONENTS:
			UpdateCompWin();
			break;
		case TERMINAL:
			UpdateTerminalWin();
			break;
		default:
			break;
	}
		
	if (gControls->nextB)
	{
		HLockHi( (Handle) gControls->nextB);
	
		bounds = (*(gControls->nextB))->contrlRect;
		PenMode(patCopy);
		ForeColor(blackColor);
		InsetRect( &bounds, -4, -4 );
		FrameGreyButton( &bounds );
	
		HUnlock( (Handle)gControls->nextB );	
	}
	
	EndUpdate( gWPtr );
	
	SetPort(oldPort);
}

void HandleActivateEvt(EventRecord* evt)
{
	// TO DO
}

void HandleOSEvt(EventRecord* evt)
{
	switch ( (evt->message >> 24) & 0x000000FF)  //dougt: Okay, what is this?
	{
		case suspendResumeMessage:
			if ((evt->message & resumeFlag) == 1)
			{
				switch(gCurrWin)
				{
					case LICENSE:
						EnableLicenseWin();
						break;
					case WELCOME:
						EnableWelcomeWin();
						break;
					case SETUP_TYPE:
						EnableSetupTypeWin();
						break;
					case COMPONENTS:
						EnableComponentsWin();
						break;
					case TERMINAL:
						EnableTerminalWin();
						break;
				}
				
				InvalRect(&gWPtr->portRect);
			}
			else
			{
				switch(gCurrWin)
				{
					case LICENSE:
						DisableLicenseWin();
						break;
					case WELCOME:
						DisableWelcomeWin();
						break;
					case SETUP_TYPE:
						DisableSetupTypeWin();
						break;
					case COMPONENTS:
						DisableComponentsWin();
						break;
					case TERMINAL:
						DisableTerminalWin();
						break;
				}
				
				InvalRect(&gWPtr->portRect);
			}						
	}
}

void React2InContent(EventRecord* evt, WindowPtr wCurrPtr)
{	
	switch (gCurrWin)
	{
		case LICENSE:
			InLicenseContent(evt, gWPtr);
			break;
			
		case WELCOME:
			InWelcomeContent(evt, gWPtr);
			break;
			
		case SETUP_TYPE:
			InSetupTypeContent(evt, gWPtr);
			break;
			
		case COMPONENTS:
			InComponentsContent(evt, gWPtr);
			break;
		
		case TERMINAL:
			InTerminalContent(evt, gWPtr);
			break;
			
		default:
			gDone = true;  //dougt: are you sure you want to do this?
			break;
	}
}
			
			
			