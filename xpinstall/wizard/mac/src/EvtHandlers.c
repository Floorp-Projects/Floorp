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
			
		case mouseUp:
			if (gCurrWin == kComponentsID)
				InComponentsContent(nextEvt, gWPtr);
			else if (gCurrWin == kAdditionsID)
				InAdditionsContent(nextEvt, gWPtr);
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
			menuChoice = MenuSelect(evt->where);
			HandleMenuChoice(menuChoice);
			InvalMenuBar();
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
	char 			keyPressed;
	unsigned long 	finalTicks;
		
	keyPressed = evt->message & charCodeMask;
#ifdef MIW_DEBUG
	if ( (keyPressed == 'z') || (keyPressed == 'Z'))
		gDone = true;	// backdoor exit
#endif
	switch(keyPressed)                   //dougt: what about tab, esc, arrows, doublebyte?
	{		
		case '\r':
		case '\3':
			if (gControls->nextB && !gInstallStarted)
			{
				HiliteControl(gControls->nextB, 1);
				Delay(8, &finalTicks);
				HiliteControl(gControls->nextB, 0);
			}

			switch(gCurrWin)
			{
                case kWelcomeID:				
					KillControls(gWPtr);
					ShowLicenseWin();
					return;
				case kLicenseID:
					KillControls(gWPtr);
					ShowSetupTypeWin();
					return;
				case kSetupTypeID:
					ClearDiskSpaceMsgs();
					KillControls(gWPtr);	
					/* treat last setup type selection as custom */
					if (gControls->opt->instChoice == gControls->cfg->numSetupTypes)
						ShowComponentsWin();
					else
						ShowTerminalWin();
					return;				
				case kComponentsID:		
                    gControls->cw->compListBox.top = 0;
                    EraseRect(&gControls->cw->compListBox);			
					ClearDiskSpaceMsgs();
					KillControls(gWPtr);
					if (gControls->cfg->bAdditionsExist)
						ShowAdditionsWin();
					else
						ShowTerminalWin();
					return;
				case kAdditionsID:
					KillControls(gWPtr);			    		
                    gControls->aw->compListBox.top = 0;
                    EraseRect(&gControls->aw->compListBox);					
					ClearDiskSpaceMsgs();
					ShowTerminalWin();
					return;
				case kTerminalID:
					if (!gInstallStarted)
					{	
                        BeginInstall();
					}
					return;
				default:
					break; // never reached
			}
		break;
		
		default:
			break; 
	}
	
	if ( (evt->modifiers & cmdKey) != 0 )
	{
		switch(keyPressed)
		{
			case 'Q':
			case 'q':
			case '.':
				gDone = true;
				break;
				
			default:
				break;
		}
	}
}

void HandleMenuChoice(SInt32 aChoice)
{
	long menuID = HiWord(aChoice);
	long menuItem = LoWord(aChoice);
	
	switch(menuID)
	{
		case mApple:
			if (menuItem == iAbout)
			{
				Alert(rAboutBox, nil);
			}
			break;
		
		case mFile:
			if (menuItem == iQuit)
				gDone = true;		
			break;
				
		default:
			break;
	}
}

void HandleUpdateEvt(EventRecord* evt)
{
    Boolean bDefault = true;
    	
	GrafPtr	oldPort;
	GetPort(&oldPort);
	
	SetPort( gWPtr );

	if (gWPtr != NULL)
	{
		BeginUpdate( gWPtr );
		DrawControls( gWPtr );
		ShowLogo(false);
	
		switch(gCurrWin)
		{
			case kWelcomeID:
			    break;
            case kLicenseID:
				ShowTxt();
				break;
			case kSetupTypeID:
				ShowSetupDescTxt();
				break;
			case kComponentsID:
				UpdateCompWin();
				break;
			case kAdditionsID:
				UpdateAdditionsWin();
				break;
			case kTerminalID:
				UpdateTerminalWin();
				break;
			default:
				break;
		}
		
		if (gControls->nextB)
		{
            SetControlData(gControls->nextB, kControlNoPart, 
                kControlPushButtonDefaultTag, sizeof(bDefault),(Ptr) &bDefault);
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
	HiliteMenu(0);
	
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
					case kAdditionsID:
						EnableAdditionsWin();
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
					case kAdditionsID:
						DisableAdditionsWin();
						break;
					case kTerminalID:
						DisableTerminalWin();
						break;
				}
				
				InvalRect(&gWPtr->portRect);
			}	
    	
		default:
			break;					
	}
}

void React2InContent(EventRecord* evt, WindowPtr wCurrPtr)
{	
    if (DidUserCancel(evt))
        return;
        
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
		
		case kAdditionsID:
			InAdditionsContent(evt, gWPtr);
			break;
			
		case kTerminalID:
			InTerminalContent(evt, gWPtr);
			break;
			
		default:
			gDone = true;
			break;
	}
}

Boolean
DidUserCancel(EventRecord *evt)
{
    GrafPtr oldPort;
    Boolean bUserCancelled = false;
    Point localPt;
    Rect r;
    ControlPartCode	part;

    GetPort(&oldPort);
	SetPort(gWPtr);
	localPt = evt->where;
	GlobalToLocal(&localPt);
	
    HLock((Handle)gControls->cancelB);
	r = (**(gControls->cancelB)).contrlRect;
	HUnlock((Handle)gControls->cancelB);
	if (PtInRect(localPt, &r))
	{
	    if (gControls->state == eInstallNotStarted)
	    {
    		part = TrackControl(gControls->cancelB, evt->where, NULL);
    		if (part)
    		{
    		    gDone = true;  
                bUserCancelled = true;
    	    }
    	}
	}
	
    SetPort(oldPort);
    return bUserCancelled;
}
