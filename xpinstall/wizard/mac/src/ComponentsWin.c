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
 *   Components Window
 *-----------------------------------------------------------*/

void 
ShowComponentsWin(void)
{
	Str255		next, back;
	Str255		compDescTitle;
	StringPtr	selCompMsg;
	Handle		listBoxRect;
	Rect 		dataBounds, listBoxFrame;
	short		reserr;
	Point		cSize;
	Boolean		bCellSelected;
	GrafPtr		oldPort;
	GetPort(&oldPort);
		
	SetPort(gWPtr);
	
	gCurrWin = kComponentsID; 
	/* gControls->cw = (CompWin *) NewPtrClear(sizeof(CompWin)); */
	
	GetIndString(next, rStringList, sNextBtn);
	GetIndString(back, rStringList, sBackBtn);

	// get controls
	listBoxRect = Get1Resource('RECT', rCompListBox);
	reserr = ResError(); 
	if (reserr == noErr && listBoxRect != NULL)
	{
		HLock((Handle)listBoxRect);
		SetRect(&gControls->cw->compListBox, 	((Rect*)*listBoxRect)->left,
												((Rect*)*listBoxRect)->top,
												((Rect*)*listBoxRect)->right,
												((Rect*)*listBoxRect)->bottom);
		HUnlock((Handle)listBoxRect);
	}
	else
	{
		ErrorHandler();
		return;
	}
	gControls->cw->compDescBox = NULL;
	gControls->cw->compDescBox = GetNewControl(rCompDescBox, gWPtr);
	if (!gControls->cw->compDescBox)
	{
		ErrorHandler();
		return;
	}

	gControls->cw->compListBox.right -= kScrollBarWidth;
	SetRect(&dataBounds, 0, 0, 1, gControls->cfg->numComps);
	SetPt( &cSize, 0, 0);
	gControls->cw->compList = LNew((const Rect*)&gControls->cw->compListBox, (const Rect*)&dataBounds,
									cSize, 0, gWPtr, true, false, false, true);
	
	// populate controls
	bCellSelected = PopulateCompInfo();
	
	// show controls
	GetIndString(compDescTitle, rStringList, sCompDescTitle);
	SetControlTitle(gControls->cw->compDescBox, compDescTitle);
	
	MoveTo( gControls->cw->compListBox.left, gControls->cw->compListBox.top - kInterWidgetPad);
	HLock(gControls->cfg->selCompMsg);
	selCompMsg = CToPascal(*gControls->cfg->selCompMsg);
	if (selCompMsg)
		DrawString( selCompMsg );
	HUnlock(gControls->cfg->selCompMsg);
	SetRect(&listBoxFrame, gControls->cw->compListBox.left, 
						 --gControls->cw->compListBox.top,
						   gControls->cw->compListBox.right += kScrollBarWidth,
						 ++gControls->cw->compListBox.bottom);
	FrameRect(&listBoxFrame);
	ShowNavButtons( back, next);
	if (bCellSelected)
		SetOptInfo();
	else
		DrawDiskSpaceMsgs( gControls->opt->vRefNum );

	//if (selCompMsg)
	//	DisposePtr((Ptr) selCompMsg);
	
	SetPort(oldPort);
}

Boolean
PopulateCompInfo(void)
{
	int 	i;
	char	*currDesc;
	Point	currCell;
	Boolean bCellSelected = false;
	
	for (i=0; i<gControls->cfg->numComps; i++)
	{
		HLock(gControls->cfg->comp[i].shortDesc);
		currDesc = *gControls->cfg->comp[i].shortDesc;
		SetPt(&currCell, 0, i);	
		LSetCell( currDesc, strlen(currDesc), currCell, gControls->cw->compList);
		HUnlock(gControls->cfg->comp[i].shortDesc);
		if (gControls->opt->compSelected[i] == kSelected)
		{
			LSetSelect(true, currCell, gControls->cw->compList);
			bCellSelected = true;
		}
	}
	
	return bCellSelected;
}

void		
UpdateCompWin(void)
{	
	Rect		r;
	Cell		c;
	GrafPtr		oldPort;
	GetPort(&oldPort);
	
	SetPort(gWPtr);
	
	MoveTo( gControls->cw->compListBox.left, gControls->cw->compListBox.top - kInterWidgetPad + 1);
	HLock(gControls->cfg->selCompMsg);
	DrawString( CToPascal(*gControls->cfg->selCompMsg));
	HUnlock(gControls->cfg->selCompMsg);
	LUpdate( (*gControls->cw->compList)->port->visRgn, 
				gControls->cw->compList);
	FrameRect(&gControls->cw->compListBox);	
	
	SetPt(&c, 0, 0);
	if (LGetSelect(true, &c, gControls->cw->compList))
	{
		HLock((Handle)gControls->cw->compDescTxt);
		SetRect(&r, (*gControls->cw->compDescTxt)->viewRect.left,
					(*gControls->cw->compDescTxt)->viewRect.top,
					(*gControls->cw->compDescTxt)->viewRect.right,
					(*gControls->cw->compDescTxt)->viewRect.bottom);
		HUnlock((Handle)gControls->cw->compDescTxt);
		TEUpdate(&r, gControls->cw->compDescTxt);	
	}
	
	DrawDiskSpaceMsgs( gControls->opt->vRefNum );
	
	SetPort(oldPort);
}

void 
InComponentsContent(EventRecord* evt, WindowPtr wCurrPtr)
{	
	Point 			localPt;
	Rect			r;
	ControlPartCode	part;
	GrafPtr			oldPort;
	GetPort(&oldPort);
	
	SetPort(wCurrPtr);
	localPt = evt->where;
	GlobalToLocal( &localPt);
	
	if (PtInRect( localPt, &gControls->cw->compListBox))
	{
		LClick(localPt, evt->modifiers, gControls->cw->compList);
		SetOptInfo();
	}
			
	HLock((Handle)gControls->backB);
	r = (**(gControls->backB)).contrlRect;
	HUnlock((Handle)gControls->backB);
	if (PtInRect( localPt, &r))
	{
		part = TrackControl(gControls->backB, evt->where, NULL);
		if (part)
		{ 
			/* extra handling since we used DrawString for static text msg 
			 * and framed our own listbox etc. 
			 */
			gControls->cw->compListBox.top = 0;
			EraseRect(&gControls->cw->compListBox);
			ClearDiskSpaceMsgs();
			
			KillControls(gWPtr);
			ShowSetupTypeWin();
			return;
		}
	}
			
	HLock((Handle)gControls->nextB);			
	r = (**(gControls->nextB)).contrlRect;
	HUnlock((Handle)gControls->nextB);
	if (PtInRect( localPt, &r))
	{
		part = TrackControl(gControls->nextB, evt->where, NULL);
		if (part)
		{	
			gControls->cw->compListBox.top = 0;
			EraseRect(&gControls->cw->compListBox);
			ClearDiskSpaceMsgs();
						
			KillControls(gWPtr);
			ShowTerminalWin();
			return;
		}
	}
	SetPort(oldPort);
}

void
SetOptInfo(void)
{
	Boolean		isCellSelected;
	Cell		currCell;
	int			i;
	Rect		viewRect;
	
	HLock((Handle)gControls->cw->compDescBox);
	SetRect(&viewRect, (*gControls->cw->compDescBox)->contrlRect.left,
					   (*gControls->cw->compDescBox)->contrlRect.top,
					   (*gControls->cw->compDescBox)->contrlRect.right,
					   (*gControls->cw->compDescBox)->contrlRect.bottom);
	HUnlock((Handle)gControls->cw->compDescBox);
	viewRect.top += kInterWidgetPad;
	SetRect(&viewRect, viewRect.left + kTxtRectPad,
						viewRect.top + kTxtRectPad,
						viewRect.right - kTxtRectPad,
						viewRect.bottom - kTxtRectPad);
	EraseRect(&viewRect);
	
	for(i=0; i<gControls->cfg->numComps; i++)
	{
		SetPt(&currCell, 0, i);
		if ( (isCellSelected = LGetSelect( false, &currCell, gControls->cw->compList)) == true)
		{
			if (gControls->opt->compSelected[i] != kSelected)
			{
				gControls->opt->compSelected[i] = kSelected;
				gControls->opt->numCompSelected++;
			}
			
			if (!gControls->cw->compDescBox)
			{
				ErrorHandler();
				return;
			}
			
			TextFont(applFont);
			TextSize(10);
			gControls->cw->compDescTxt = TENew(&viewRect, &viewRect);
			HLock(gControls->cfg->comp[i].longDesc);
			TESetText( *gControls->cfg->comp[i].longDesc, 
						strlen(*gControls->cfg->comp[i].longDesc), gControls->cw->compDescTxt);
			TEUpdate( &viewRect, gControls->cw->compDescTxt);
			TextFont(systemFont);
			TextSize(12);
			HUnlock(gControls->cfg->comp[i].longDesc);
		}
		else if (gControls->opt->compSelected[i] == kSelected)
		{
			gControls->opt->compSelected[i] = kNotSelected;
			gControls->opt->numCompSelected--;
		}
	}
	
	ClearDiskSpaceMsgs();
	DrawDiskSpaceMsgs( gControls->opt->vRefNum );
}

void
EnableComponentsWin(void)
{
	EnableNavButtons();

	// TO DO
}


void
DisableComponentsWin(void)
{
	DisableNavButtons();
	
	// TO DO
}