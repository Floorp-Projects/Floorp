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
				React2InContent(evt, wCurrPtr);
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
#ifdef DEBUG
	if ( (keyPressed == 'z') || (keyPressed == 'Z'))
		gDone = true;	// backdoor exit
#endif
	if (keyPressed == '\r')                   //dougt: what about tab, esc, arrows, doublebyte?
	{		
		switch(gCurrWin)
		{
			case kLicenseID:
				KillControls(gWPtr);
				ShowWelcomeWin();
				return;
			case kWelcomeID:				
				KillControls(gWPtr);
				ShowSetupTypeWin();
				return;
			case kSetupTypeID:
				KillControls(gWPtr);
				
				/* treat last setup type selection as custom */
				if (gControls->opt->instChoice == gControls->cfg->numSetupTypes)
					ShowComponentsWin();
				else
					ShowTerminalWin();
				return;				
			case kComponentsID:
				KillControls(gWPtr);
				ShowTerminalWin();
				return;
			case kTerminalID:
				SpawnSDThread(Install, &tid);
				return;
			default:
				break; // never reached
		}
	}
}

void HandleUpdateEvt(EventRecord* evt)
{
	Rect		bounds;
	
	GrafPtr	oldPort;
	GetPort(&oldPort);
	
	SetPort( gWPtr );

	if (gWPtr != NULL)
	{
		BeginUpdate( gWPtr );
		DrawControls( gWPtr );
		ShowLogo();
	
		switch(gCurrWin)
		{
			case kLicenseID:
			case kWelcomeID:
				ShowTxt();
				break;
			case kSetupTypeID:
				ShowSetupDescTxt();
				break;
			case kComponentsID:
				UpdateCompWin();
				break;
			case kTerminalID:
				UpdateTerminalWin();
				break;
			default:
				break;
		}
		
		if (gControls->nextB)
		{
			HLock( (Handle) gControls->nextB);
	
			bounds = (*(gControls->nextB))->contrlRect;
			PenMode(patCopy);
			ForeColor(blackColor);
			InsetRect( &bounds, -4, -4 );
			FrameGreyButton( &bounds );
	
			HUnlock( (Handle)gControls->nextB );	
		}
	
		EndUpdate( gWPtr );
	}
	
	SetPort(oldPort);
}

void HandleActivateEvt(EventRecord* evt)
{
	// TO DO
}

void HandleOSEvt(EventRecord* evt)
{
	switch ( (evt->message >> 24) & 0x000000FF)
	{
		case suspendResumeMessage:
			if ((evt->message & resumeFlag) == 1)
			{
				switch(gCurrWin)
				{
					case kLicenseID:
						EnableLicenseWin();
						break;
					case kWelcomeID:
						EnableWelcomeWin();
						break;
					case kSetupTypeID:
						EnableSetupTypeWin();
						break;
					case kComponentsID:
						EnableComponentsWin();
						break;
					case kTerminalID:
						EnableTerminalWin();
						break;
				}
				
				InvalRect(&gWPtr->portRect);
			}
			else
			{
				switch(gCurrWin)
				{
					case kLicenseID:
						DisableLicenseWin();
						break;
					case kWelcomeID:
						DisableWelcomeWin();
						break;
					case kSetupTypeID:
						DisableSetupTypeWin();
						break;
					case kComponentsID:
						DisableComponentsWin();
						break;
					case kTerminalID:
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
		case kLicenseID:
			InLicenseContent(evt, gWPtr);
			break;
			
		case kWelcomeID:
			InWelcomeContent(evt, gWPtr);
			break;
			
		case kSetupTypeID:
			InSetupTypeContent(evt, gWPtr);
			break;
			
		case kComponentsID:
			InComponentsContent(evt, gWPtr);
			break;
		
		case kTerminalID:
			InTerminalContent(evt, gWPtr);
			break;
			
		default:
			gDone = true;
			break;
	}
}
			
			
			