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


#ifndef _MIW_H_
	#include "MacInstallWizard.h"
#endif


/*-----------------------------------------------------------*
 *   Welcome Window
 *-----------------------------------------------------------*/

void 
ShowWelcomeWin(void)
{
	Str255 next;
	Str255 back;
	Rect sbRect;
	int sbWidth;
	
	GrafPtr	oldPort;
	GetPort(&oldPort);
	
	SetPort(gWPtr);
	
	gCurrWin = WELCOME; 
	/* gControls->ww = (WelcWin *) NewPtrClear(sizeof(WelcWin)); */
	
	GetIndString(next, rStringList, sNextBtn);
	GetIndString(back, rStringList, sBackBtn);
	
	gControls->ww->scrollBar = GetNewControl( rLicScrollBar, gWPtr);
	gControls->ww->welcBox = GetNewControl( rLicBox, gWPtr);

	if(gControls->ww->scrollBar && gControls->ww->welcBox)
	{
		HLockHi( (Handle) gControls->ww->scrollBar);
		sbRect = (*(gControls->ww->welcBox))->contrlRect;
				
		sbWidth = (*(gControls->ww->scrollBar))->contrlRect.right -
				  (*(gControls->ww->scrollBar))->contrlRect.left;
		
		(*(gControls->ww->scrollBar))->contrlRect.right = sbRect.right + kScrollBarPad;
		(*(gControls->ww->scrollBar))->contrlRect.left = sbRect.right + kScrollBarPad - 
														 sbWidth;
		(*(gControls->ww->scrollBar))->contrlRect.top = sbRect.top - kScrollBarPad;
		(*(gControls->ww->scrollBar))->contrlRect.bottom = sbRect.bottom + kScrollBarPad;
		HUnlock( (Handle) gControls->ww->scrollBar);
	}
	InitWelcTxt();

	ShowNavButtons( back, next);
	ShowControl( gControls->ww->scrollBar);
	ShowControl( gControls->ww->welcBox);
	ShowTxt();
	InitScrollBar( gControls->ww->scrollBar);
	
}

void
InitWelcTxt(void)
{
	Rect 		viewRect, destRect;
	long		welcStrLen;
	int 		i;
	char *		newPara;
	
	/* TE specific init */
	HLockHi( (Handle) gControls->ww->welcBox);
	viewRect = (*(gControls->ww->welcBox))->contrlRect;	
	HUnlock( (Handle) gControls->ww->welcBox);

	destRect.left = viewRect.left;
		viewRect.right = (*(gControls->ww->scrollBar))->contrlRect.left; 
	destRect.right = viewRect.right;
	destRect.top = viewRect.top;
	destRect.bottom = viewRect.bottom * kNumWelcScrns; /* XXX: hack */
	
	gControls->ww->welcTxt = (TEHandle) NewPtrClear(sizeof(TEPtr));
	
	TextFont(applFont);
	TextFace(normal);
	TextSize(12);

	newPara = "\r\r";
	
	gControls->ww->welcTxt = TENew( &destRect, &viewRect);
	for (i=0; i<kNumWelcMsgs; i++)
	{
		HLockHi(gControls->cfg->welcMsg[i]);
		welcStrLen = strlen( *gControls->cfg->welcMsg[i]);
		TEInsert( *gControls->cfg->welcMsg[i], welcStrLen, gControls->ww->welcTxt);
		HUnlock(gControls->cfg->welcMsg[i]);
		TEInsert( newPara, strlen(newPara), gControls->ww->welcTxt);
	}
	
	TextFont(systemFont);
	TextSize(12);
	
	TESetAlignment(teFlushDefault, gControls->ww->welcTxt);
}

void 
InWelcomeContent(EventRecord* evt, WindowPtr wCurrPtr)
{	
	Point 			localPt;
	Rect			r;
	short 			code, value;
	ControlPartCode	part;
	ControlHandle	scrollBar;
	ControlActionUPP	scrollActionFunctionUPP;
	GrafPtr			oldPort;
	
	GetPort(&oldPort);
	SetPort(wCurrPtr);
	localPt = evt->where;
	GlobalToLocal( &localPt);
	
	code = FindControl(localPt, wCurrPtr, &scrollBar);
	switch (code)
	{
		case kControlUpButtonPart:
		case kControlDownButtonPart:
		case kControlPageUpPart:
		case kControlPageDownPart:
			scrollActionFunctionUPP = NewControlActionProc((ProcPtr) DoScrollProc);
			value = TrackControl(scrollBar, localPt, scrollActionFunctionUPP);
			return;
			break; 
			
		case kControlIndicatorPart:
			value = GetControlValue(scrollBar);
			code = TrackControl(scrollBar, localPt, nil);
			if (code) 
			{
				value -= GetControlValue(scrollBar);
				if (value) 
				{
					TEScroll(0, value * kScrollAmount, gControls->ww->welcTxt);
				}
			}
			return;
			break;
	}	
				
	HLockHi((Handle)gControls->backB);
	r = (**(gControls->backB)).contrlRect;
	HUnlock((Handle)gControls->backB);
	if (PtInRect( localPt, &r))
	{
		part = TrackControl(gControls->backB, evt->where, NULL);
		if (part)
		{
			KillControls(gWPtr);
			ShowLicenseWin();
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
			KillControls(gWPtr);
			ShowSetupTypeWin();
			return;
		}
	}
	SetPort(oldPort);
}

void
EnableWelcomeWin(void)
{
	EnableNavButtons();

	if(gControls->ww->scrollBar)
		HiliteControl(gControls->ww->scrollBar, kEnableControl);
}

void
DisableWelcomeWin(void)
{
	DisableNavButtons();
	
	if(gControls->ww->scrollBar)
		HiliteControl(gControls->ww->scrollBar, kDisableControl);
}