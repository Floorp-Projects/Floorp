/* -*- Mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is Mozilla Communicator client code, released
 * March 31, 1998.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1999
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Samir Gehani <sgehani@netscape.com>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either of the GNU General Public License Version 2 or later (the "GPL"),
 * or the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#include "MacInstallWizard.h"


/*-----------------------------------------------------------*
 *   License Window
 *-----------------------------------------------------------*/

void
ShowLicenseWin(void)
{
	Str255 		accept;
	Str255 		decline;
	Rect 		sbRect;
	int 		sbWidth;
	
	GrafPtr	oldPort;
	GetPort(&oldPort);
	
	SetPort(gWPtr);

	gCurrWin = kLicenseID; 
	/* gControls->lw = (LicWin *) NewPtrClear(sizeof(LicWin)); */
	
	GetResourcedString(accept, rInstList, sAcceptBtn);
	GetResourcedString(decline, rInstList, sDeclineBtn);
	
	gControls->lw->scrollBar = GetNewControl( rLicScrollBar, gWPtr);
	gControls->lw->licBox = GetNewControl( rLicBox, gWPtr);

	if(gControls->lw->scrollBar && gControls->lw->licBox)
	{
		HLock( (Handle) gControls->lw->scrollBar);
		sbRect = (*(gControls->lw->licBox))->contrlRect;
				
		sbWidth = (*(gControls->lw->scrollBar))->contrlRect.right -
				  (*(gControls->lw->scrollBar))->contrlRect.left;
		
		(*(gControls->lw->scrollBar))->contrlRect.right = sbRect.right + kScrollBarPad;
		(*(gControls->lw->scrollBar))->contrlRect.left = sbRect.right + kScrollBarPad - 
														 sbWidth;
		(*(gControls->lw->scrollBar))->contrlRect.top = sbRect.top - kScrollBarPad;
		(*(gControls->lw->scrollBar))->contrlRect.bottom = sbRect.bottom + kScrollBarPad;
		HUnlock( (Handle) gControls->lw->scrollBar);
	}
	else
	{
		ErrorHandler(eParam, nil);
		return;
	}
	InitLicTxt();

	ShowNavButtons( decline, accept);
	ShowControl( gControls->lw->scrollBar);
	ShowControl( gControls->lw->licBox);
	ShowTxt();
	InitScrollBar( gControls->lw->scrollBar);
	ShowLogo(false);
	
	SetPort(oldPort);
}

void
InitLicTxt(void)
{
	Rect	destRect, viewRect;
	FSSpec	licFile;
	long 	dirID, dataSize;
	short 	vRefNum, dataRef, resRef;
	unsigned char* 	cLicFName;
	Str255			pLicFName;
	OSErr	err;
	Handle 	text, stylHdl;
	
	ERR_CHECK(GetCWD(&dirID, &vRefNum));
	
	/* open and read license file */
	HLock(gControls->cfg->licFileName);
	if(**gControls->cfg->licFileName != nil)
	{
		cLicFName = CToPascal(*gControls->cfg->licFileName);
		
		ERR_CHECK(FSMakeFSSpec(vRefNum, dirID, cLicFName, &licFile));
		if (cLicFName)
			DisposePtr((char*)cLicFName);
	}
	else /* assume default license filename from str rsrc */
	{	
		GetResourcedString(pLicFName, rInstList, sLicenseFName);
		ERR_CHECK(FSMakeFSSpec(vRefNum, dirID, pLicFName, &licFile));
	}
	HUnlock(gControls->cfg->licFileName);
	
	/* read license text */
	ERR_CHECK(FSpOpenDF( &licFile, fsRdPerm, &dataRef));
	ERR_CHECK(GetEOF(dataRef, &dataSize));

	if (dataSize > 0)
	{
		if (!(text = NewHandle(dataSize)))
		{
			ErrorHandler(eMem, nil);
			return;
		}
		ERR_CHECK(FSRead(dataRef, &dataSize, *text));
	}
	else
		text = nil;
	ERR_CHECK(FSClose(dataRef));

	/* get 'styl' if license is multistyled */
	resRef = FSpOpenResFile( &licFile, fsRdPerm);
	ERR_CHECK(ResError());

	UseResFile(resRef);
	stylHdl = RGetResource('styl', 128);
	ERR_CHECK(ResError());
	
	if(stylHdl)
		DetachResource(stylHdl);
	else
		stylHdl = nil;
	CloseResFile(resRef);
	
	/* TE specific init */
	HLock( (Handle) gControls->lw->licBox);
	SetRect(&viewRect, 	(*(gControls->lw->licBox))->contrlRect.left, 
						(*(gControls->lw->licBox))->contrlRect.top, 
						(*(gControls->lw->licBox))->contrlRect.right, 
						(*(gControls->lw->licBox))->contrlRect.bottom);
	HUnlock( (Handle) gControls->lw->licBox);

	destRect.left = viewRect.left;
		viewRect.right = (*(gControls->lw->scrollBar))->contrlRect.left; 
	destRect.right = viewRect.right;
	destRect.top = viewRect.top;
	destRect.bottom = viewRect.bottom * kNumLicScrns;
	
	// gControls->lw->licTxt = (TEHandle) NewPtrClear(sizeof(TEPtr));
	
	TextFont(applFont);
	TextFace(normal);
	TextSize(9);
	
	HLock(text);
	if (stylHdl)
	{
		gControls->lw->licTxt = TEStyleNew( &destRect, &viewRect );
		TEStyleInsert( *text, dataSize, (StScrpRec ** )stylHdl, 
						gControls->lw->licTxt);
	}
	else
	{
		gControls->lw->licTxt = TENew( &destRect, &viewRect);
		TEInsert( *text, dataSize, gControls->lw->licTxt);
	}
	HUnlock(text);
	
	TextFont(systemFont);
	TextSize(12);
	
	TESetAlignment(teFlushDefault, gControls->lw->licTxt);
}

void
ShowTxt(void)
{	    
	switch (gCurrWin)
	{
		case kLicenseID:
			if(gControls->lw->licTxt)
			{
			    RGBColor backColorOld;
			    Rect     textRect;
			    
			    // get back color
			    GetBackColor(&backColorOld);
			    
			    // set to white
			    BackColor(whiteColor);
			    
			    // erase rect and update
			    textRect = (**(gControls->lw->licTxt)).viewRect;
			    EraseRect(&textRect);
				TEUpdate(&textRect, gControls->lw->licTxt);
				
				// restore back color
				RGBBackColor(&backColorOld);
			}
			break;
		default:
			break;
	}		
}

void
ShowLogo(Boolean bEraseRect)
{
	short 		reserr;
	Rect 		derefd, logoRect;
	PicHandle 	logoPicH;
	Handle		logoRectH; 

	/* draw the image well */
    ControlHandle imgWellH = GetNewControl(rLogoImgWell, gWPtr);
    if (!imgWellH)
    {
        ErrorHandler(eMem, nil);
        return;
	}

	/* initialize Netscape logo */
	logoPicH = GetPicture(rNSLogo);  
	reserr = ResError();
	
	if (reserr == noErr)
	{
		/* draw Netscape logo */
		if (logoPicH != nil)
		{		
			logoRectH = GetResource('RECT', rNSLogoBox);
			reserr = ResError();
			if (reserr == noErr && logoRectH)
			{
				HLock(logoRectH);
				derefd = (Rect) **((Rect**)logoRectH);
				SetRect(&logoRect, derefd.left, derefd.top, derefd.right, derefd.bottom);
				HUnlock(logoRectH);
				reserr = ResError();
				if (reserr == noErr)
				{
					if (bEraseRect)
					{
						EraseRect(&logoRect);
						InvalRect(&logoRect);
					}
					DrawPicture(logoPicH, &logoRect);
					ReleaseResource((Handle)logoPicH);
				}
				
				ReleaseResource((Handle)logoRectH);
			}
		}
	}
	
	if (reserr != noErr)
		ErrorHandler(reserr, nil);
}

void
InLicenseContent(EventRecord* evt, WindowPtr wCurrPtr)
{
	Point 			localPt;
	Rect			r;
	ControlPartCode	part;
	short 			code, value;
	ControlHandle	scrollBar;
	ControlActionUPP	scrollActionFunctionUPP;
	GrafPtr			oldPort;
	
	GetPort(&oldPort);
	SetPort(gWPtr);
	localPt = evt->where;
	GlobalToLocal( &localPt);
	
	code = FindControl(localPt, wCurrPtr, &scrollBar);
	switch (code)
	{
		case kControlUpButtonPart:
		case kControlDownButtonPart:
		case kControlPageUpPart:
		case kControlPageDownPart:
			scrollActionFunctionUPP = NewControlActionUPP(DoScrollProc);
			value = TrackControl(scrollBar, localPt, scrollActionFunctionUPP);
 			return;
			
		case kControlIndicatorPart:
			value = GetControlValue(scrollBar);
			code = TrackControl(scrollBar, localPt, nil);
			if (code) 
			{
				value -= GetControlValue(scrollBar);
				if (value) 
				{
					TEScroll(0, value * kScrollAmount, gControls->lw->licTxt);
                    ShowTxt();
				}
			}
			return;
	}
	
	HLock((Handle)gControls->backB);
	r = (**(gControls->backB)).contrlRect;
	HUnlock((Handle)gControls->backB);
	if (PtInRect( localPt, &r))
	{
		part = TrackControl(gControls->backB, evt->where, NULL);
		if (part)
			gDone = true;  /* Decline pressed */
	}
	
	HLock((Handle)gControls->nextB);			
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
	
	ShowTxt();
	// TO DO
	SetPort(oldPort);
}

void
EnableLicenseWin(void)
{
	GrafPtr	oldPort;
	GetPort(&oldPort);
	
	SetPort(gWPtr);
	
	EnableNavButtons();
	
	if(gControls->lw->licBox)
		HiliteControl(gControls->lw->licBox, kEnableControl);	
	if(gControls->lw->scrollBar)
		HiliteControl(gControls->lw->scrollBar, kEnableControl);			
	
	ShowTxt();
	SetPort(oldPort);
	
	// TO DO
}

void
DisableLicenseWin(void)
{
	DisableNavButtons();
	
	/*
	if(gControls->lw->licBox)		
		HiliteControl(gControls->lw->licBox, kDisableControl);
	*/
	if(gControls->lw->scrollBar)
		HiliteControl(gControls->lw->scrollBar, kDisableControl);
	
}

void 
InitScrollBar(ControlHandle sb)
{
	short		lines;
	short		max;
	short		height;
	TEPtr		te;
	TEHandle	currTE;
	
	switch(gCurrWin)
	{
		case kLicenseID:
			currTE = gControls->lw->licTxt;
			break;
		default:
			ErrorHandler(eUnknownDlgID, nil);
			break;
	}
	
	lines = TEGetHeight((**currTE).nLines,0,currTE) / kScrollAmount;
	te = *currTE;							// point to TERec for convenience

	height = te->viewRect.bottom - te->viewRect.top;
	max = lines - (height / kScrollAmount);
	if (height % kScrollAmount) max++;
	if ( max < 0 ) max = 0;

	SetControlMaximum(sb, max);
}

pascal void 
DoScrollProc(ControlHandle theControl, short part)
{
	short		amount;
	TEPtr		te;
	
	if ( part != 0 ) {
		switch (gCurrWin)
		{
			case kLicenseID:				
				te = *(gControls->lw->licTxt);
				break;
			default:
				ErrorHandler(eUnknownDlgID, nil);
				break;
		}
		
		switch ( part ) {
			case kControlUpButtonPart:
			case kControlDownButtonPart:		// one line
				amount = 1;
				break;
			case kControlPageUpPart:			// one page
			case kControlPageDownPart:
				amount = (te->viewRect.bottom - te->viewRect.top) / kScrollAmount;
				break;
		}
		if ( (part == kControlDownButtonPart) || (part == kControlPageDownPart) )
			amount = -amount;
		CalcChange(theControl, &amount);
		if (amount) {
			TEScroll(0, amount * kScrollAmount, &te);
            ShowTxt();
		}
	}
}

void 
CalcChange(ControlHandle theControl,	short *amount)
{
	short		value, max;
	
	value = GetControlValue(theControl);	// get current value
	max = GetControlMaximum(theControl);		// and maximum value
	*amount = value - *amount;
	if (*amount < 0)
		*amount = 0;
	else if (*amount > max)
		*amount = max;
	SetControlValue(theControl, *amount);
	*amount = value - *amount;			// calculate the change
}

void
ShowNavButtons(unsigned char* backTitle, unsigned char* nextTitle)
{
    Boolean bDefault = true;
    	
	gControls->backB = GetNewControl( rBackBtn, gWPtr);
	gControls->nextB = GetNewControl( rNextBtn, gWPtr);

	if( gControls->backB != NULL)
	{
		SetControlTitle( gControls->backB, backTitle); 
		ShowControl( gControls->backB);

		if (gCurrWin==kWelcomeID || gCurrWin==kSetupTypeID)
			HiliteControl(gControls->backB, kDisableControl);
	}
	
	if ( gControls->nextB != NULL)
	{
		SetControlTitle( gControls->nextB, nextTitle);
		ShowControl( gControls->nextB);

        SetControlData(gControls->nextB, kControlNoPart, 
            kControlPushButtonDefaultTag, sizeof(bDefault),(Ptr) &bDefault);
	}
	
    ShowCancelButton();
}

void
EnableNavButtons(void)
{
	if (gControls->backB && gCurrWin!=kWelcomeID)
		HiliteControl(gControls->backB, kEnableControl);
	if (gControls->nextB)
		HiliteControl(gControls->nextB, kEnableControl);

    if (gControls->cancelB)
		HiliteControl(gControls->cancelB, kEnableControl);

}

void
DisableNavButtons(void)
{	
	if (gControls->backB)
		HiliteControl(gControls->backB, kDisableControl);
	if(gControls->nextB)
		HiliteControl(gControls->nextB, kDisableControl);

    if (gControls->cancelB)
		HiliteControl(gControls->cancelB, kDisableControl);
}
