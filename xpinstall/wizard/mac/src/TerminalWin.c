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

static Boolean bDownloadedRedirect = false;

void 
ShowTerminalWin(void)
{
	Str255		next, back;
	Handle		rectH;
	Rect		viewRect;
	short		reserr;
#if MOZILLA == 0
	MenuHandle 			popupMenu;
	PopupPrivateData ** pvtDataHdl;
	unsigned char *		currMenuItem;
	short 				i;
#endif /* MOZILLA == 0 */
	GrafPtr		oldPort;
	GetPort(&oldPort);

	if (gWPtr != NULL)
	{
		SetPort(gWPtr);
	
		gCurrWin = kTerminalID; 
		/* gControls->tw = (TermWin*) NewPtrClear(sizeof(TermWin)); */
	
		GetIndString(next, rStringList, sInstallBtn);
		GetIndString(back, rStringList, sBackBtn);
	
		// malloc and get control
		rectH = Get1Resource('RECT', rStartMsgBox);
		reserr = ResError();
		if (reserr == noErr && rectH != NULL)
			viewRect = (Rect) **((Rect **)rectH);
		else
		{
			ErrorHandler();
			return;
		}
		
		gControls->tw->startMsgBox = viewRect;
	
		gControls->tw->startMsg = TENew(&viewRect, &viewRect);
	    if (gControls->tw->startMsg == NULL)
	    {
	    	ErrorHandler();
	    	return;
	    }
	    
#if MOZILLA == 0	
		if (gControls->cfg->numSites > 0)
		{
			gControls->tw->siteSelector = NULL;
			gControls->tw->siteSelector = GetNewControl( rSiteSelector, gWPtr );
			if (!gControls->tw->siteSelector)
			{
				ErrorHandler();
				return;
			}
			
			// populate popup button menus
			HLock((Handle)gControls->tw->siteSelector);
			pvtDataHdl = (PopupPrivateData **) (*(gControls->tw->siteSelector))->contrlData;
			HLock((Handle)pvtDataHdl);
			popupMenu = (MenuHandle) (**pvtDataHdl).mHandle;
			for (i=0; i<gControls->cfg->numSites; i++)
			{
				HLock(gControls->cfg->site[i].desc);
				currMenuItem = CToPascal(*gControls->cfg->site[i].desc);		
				HUnlock(gControls->cfg->site[i].desc);
				InsertMenuItem( popupMenu, currMenuItem, i );
			}
			HUnlock((Handle)pvtDataHdl);
			HUnlock((Handle)gControls->tw->siteSelector);
			SetControlMaximum(gControls->tw->siteSelector, gControls->cfg->numSites);
			SetControlValue(gControls->tw->siteSelector, gControls->opt->siteChoice);
			ShowControl(gControls->tw->siteSelector);
		}
#endif /* MOZILLA == 0 */
		
		// populate control
		HLock(gControls->cfg->startMsg);
		TESetText(*gControls->cfg->startMsg, strlen(*gControls->cfg->startMsg), 
					gControls->tw->startMsg);
		HUnlock(gControls->cfg->startMsg);
	
		// show controls
		TEUpdate(&viewRect, gControls->tw->startMsg);
		ShowNavButtons( back, next );
	}
	
	SetPort(oldPort);
}

void
UpdateTerminalWin(void)
{
	GrafPtr	oldPort;
	Rect	instMsgRect;
	GetPort(&oldPort);
	SetPort(gWPtr);
	
	TEUpdate(&gControls->tw->startMsgBox, gControls->tw->startMsg);
	if (gControls->tw->allProgressMsg)
	{
		HLock((Handle)gControls->tw->allProgressMsg);
		SetRect(&instMsgRect, (*gControls->tw->allProgressMsg)->viewRect.left,
				(*gControls->tw->allProgressMsg)->viewRect.top,
				(*gControls->tw->allProgressMsg)->viewRect.right,
				(*gControls->tw->allProgressMsg)->viewRect.bottom );
		HUnlock((Handle)gControls->tw->allProgressMsg);
		
		TEUpdate(&instMsgRect, gControls->tw->allProgressMsg);
	}
	
	SetPort(oldPort);
}

void 
InTerminalContent(EventRecord* evt, WindowPtr wCurrPtr)
{	
	Point 			localPt;
	Rect			r;
	ControlPartCode	part;
#if MOZILLA == 0
	ControlHandle	currCntl;
#endif /* MOZILLA == 0 */
	ThreadID 		tid;
	GrafPtr			oldPort;
	GetPort(&oldPort);
	
	SetPort(wCurrPtr);
	localPt = evt->where;
	GlobalToLocal( &localPt);
	
#if MOZILLA == 0			
	HLock((Handle)gControls->tw->siteSelector);
	r = (**(gControls->tw->siteSelector)).contrlRect;
	HUnlock((Handle)gControls->tw->siteSelector);
	if (PtInRect(localPt, &r))
	{
		part = FindControl(localPt, gWPtr, &currCntl);
		part = TrackControl(currCntl, localPt, (ControlActionUPP) -1);
		gControls->opt->siteChoice = GetControlValue(currCntl);
		return;
	}		
#endif /* MOZILLA == 0 */
					
	HLock((Handle)gControls->backB);
	SetRect(&r, (**(gControls->backB)).contrlRect.left,
				(**(gControls->backB)).contrlRect.top,
				(**(gControls->backB)).contrlRect.right,
				(**(gControls->backB)).contrlRect.bottom);
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
			
	HLock((Handle)gControls->nextB);		
	SetRect(&r, (**(gControls->nextB)).contrlRect.left,
				(**(gControls->nextB)).contrlRect.top,
				(**(gControls->nextB)).contrlRect.right,
				(**(gControls->nextB)).contrlRect.bottom);	
	HUnlock((Handle)gControls->nextB);
	if (PtInRect( localPt, &r))
	{
		part = TrackControl(gControls->nextB, evt->where, NULL);
		if (part)
		{
		    DisableNavButtons();
		    ClearSiteSelector();
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
					0, kCreateIfNeeded, (void**)nil, tid);
				//  ^---- 0 means gimme the default stack size from Thread Manager
	if (err == noErr)
		YieldToThread(*tid); /* force ctx switch */
	else
	{
		return false;
	}
	
	return true;
}

void
ClearSiteSelector(void)
{	
#if MOZILLA == 0
	if (gControls->tw->siteSelector)
		HideControl(gControls->tw->siteSelector);
#endif /* MOZILLA == 0*/
			
	// erase the rect contents to get rid of the message
	EraseRect(&gControls->tw->startMsgBox);
}

void
EnableTerminalWin(void)
{
	EnableNavButtons();
	
#if MOZILLA == 0
	if (gControls->tw->siteSelector)
		HiliteControl(gControls->tw->siteSelector, kEnableControl);
#endif /* MOZILLA == 0 */
}

void
DisableTerminalWin(void)
{
	DisableNavButtons();

#if MOZILLA == 0
	if (gControls->tw->siteSelector)
		HiliteControl(gControls->tw->siteSelector, kDisableControl);
#endif /* MOZILLA == 0 */
}
