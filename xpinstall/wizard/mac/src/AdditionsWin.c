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
 *   Additions Window
 *-----------------------------------------------------------*/

static int rowToComp[kMaxComponents];
static int numRows = 0;			
static Boolean bFirstDraw = true;

void
ShowAdditionsWin(void)
{
	Str255		next, back;
	Str255		compDescTitle;
	StringPtr	selCompMsg;
	Handle		listBoxRect;
	Rect 		dataBounds, listBoxFrame, viewRect;
	short		reserr;
	int			totalRows = 0, i, instChoice;
	Point		cSize;
	Boolean		bCellSelected;
	GrafPtr		oldPort;
	GetPort(&oldPort);
		
	SetPort(gWPtr);
	
	gCurrWin = kAdditionsID; 
	/* gControls->aw = (CompWin *) NewPtrClear(sizeof(CompWin)); */
	
	GetResourcedString(next, rInstList, sNextBtn);
	GetResourcedString(back, rInstList, sBackBtn);

	// get controls
	listBoxRect = Get1Resource('RECT', rCompListBox);
	reserr = ResError(); 
	if (reserr == noErr && listBoxRect != NULL)
	{
		HLock((Handle)listBoxRect);
		SetRect(&gControls->aw->compListBox, 	((Rect*)*listBoxRect)->left,
												((Rect*)*listBoxRect)->top,
												((Rect*)*listBoxRect)->right,
												((Rect*)*listBoxRect)->bottom);
		HUnlock((Handle)listBoxRect);
	}
	else
	{
		ErrorHandler(reserr, nil);
		return;
	}
	gControls->aw->compDescBox = NULL;
	gControls->aw->compDescBox = GetNewControl(rCompDescBox, gWPtr);
	if (!gControls->aw->compDescBox)
	{
		ErrorHandler(eMem, nil);
		return;
	}

	gControls->aw->compListBox.right -= kScrollBarWidth;
	instChoice = gControls->opt->instChoice-1;
	for (i=0; i<kMaxComponents; i++)
	{
		if (totalRows >= gControls->cfg->numComps)
			break;
		if (!gControls->cfg->comp[i].invisible && gControls->cfg->comp[i].additional &&
			(gControls->cfg->st[instChoice].comp[i] == kInSetupType))
			totalRows++;
	}
		
	SetRect(&dataBounds, 0, 0, 1, totalRows);
	SetPt( &cSize, 0, 0);
	gControls->aw->compList = LNew((const Rect*)&gControls->aw->compListBox, (const Rect*)&dataBounds,
									cSize, rCheckboxLDEF, gWPtr, true, false, false, true);
	(*gControls->aw->compList)->selFlags = lExtendDrag + lUseSense + lDoVAutoscroll;
	
	HLock((Handle)gControls->aw->compDescBox);
	SetRect(&viewRect, (*gControls->aw->compDescBox)->contrlRect.left,
					   (*gControls->aw->compDescBox)->contrlRect.top,
					   (*gControls->aw->compDescBox)->contrlRect.right,
					   (*gControls->aw->compDescBox)->contrlRect.bottom);
	HUnlock((Handle)gControls->aw->compDescBox);
	viewRect.top += kInterWidgetPad;
	SetRect(&viewRect, viewRect.left + kTxtRectPad,
						viewRect.top + kTxtRectPad,
						viewRect.right - kTxtRectPad,
						viewRect.bottom - kTxtRectPad);
	TextFont(applFont);
	TextSize(9);
	gControls->aw->compDescTxt = TENew(&viewRect, &viewRect);
	TextFont(systemFont);
	TextSize(12);
	
	// populate controls
	bCellSelected = AddPopulateCompInfo();
	
	// show controls
	GetResourcedString(compDescTitle, rInstList, sCompDescTitle);
	SetControlTitle(gControls->aw->compDescBox, compDescTitle);
	
	MoveTo( gControls->aw->compListBox.left, gControls->aw->compListBox.top - kInterWidgetPad);
	HLock(gControls->cfg->selCompMsg);
	selCompMsg = CToPascal(*gControls->cfg->selAddMsg);
	if (selCompMsg)
		DrawString( selCompMsg );
	HUnlock(gControls->cfg->selCompMsg);
	SetRect(&listBoxFrame, gControls->aw->compListBox.left, 
						 --gControls->aw->compListBox.top,
						   gControls->aw->compListBox.right + kScrollBarWidth,
						 ++gControls->aw->compListBox.bottom);
	FrameRect(&listBoxFrame);
	ShowNavButtons( back, next );
	if (bCellSelected)
		AddSetOptInfo(true);
	else
		DrawDiskSpaceMsgs( gControls->opt->vRefNum );

	// default highlight first row
	AddInitRowHighlight(0);

#if 0	    
	RGBColor backColorOld;
    Rect adjustedRect, *clRect = &gControls->aw->compListBox;
    SetRect(&adjustedRect, clRect->left, clRect->top+1, clRect->right, clRect->bottom-1);
    GetBackColor(&backColorOld);
    BackColor(whiteColor);
    EraseRect(&adjustedRect);
    RGBBackColor(&backColorOld);
#endif
    
	SetPort(oldPort);
}

Boolean
AddPopulateCompInfo()
{
	int 	i;
	char	*currDesc;
	Point	currCell;
	Boolean bCellSelected = false;
	int		nextRow = 0;
	
	for (i=0; i<gControls->cfg->numComps; i++)
	{
		if (!gControls->cfg->comp[i].invisible && gControls->cfg->comp[i].additional)
		{
			HLock(gControls->cfg->comp[i].shortDesc);
			currDesc = *gControls->cfg->comp[i].shortDesc;
			SetPt(&currCell, 0, nextRow);
			rowToComp[nextRow++] = i;
			LSetCell( currDesc, strlen(currDesc), currCell, gControls->aw->compList);
			HUnlock(gControls->cfg->comp[i].shortDesc);
			if (gControls->cfg->comp[i].selected == true)
			{
				LSetSelect(true, currCell, gControls->aw->compList);
				bCellSelected = true;
			}
		}
	}

	numRows = nextRow;	
	return bCellSelected;
}

void 
InAdditionsContent(EventRecord* evt, WindowPtr wCurrPtr)
{
	Point 			localPt;
	Rect			r, currCellRect, checkbox;
	ControlPartCode	part;
	int				i;
	Cell			currCell;
	UInt8			hiliteVal;
	PixPatHandle	ppH;
	GrafPtr			oldPort;
	GetPort(&oldPort);
	
	SetPort(wCurrPtr);
	localPt = evt->where;
	GlobalToLocal( &localPt);
	
	/* Mouse Up */
	
	/* scroll */
	SetRect(&r, gControls->aw->compListBox.right, gControls->aw->compListBox.top, 
	            gControls->aw->compListBox.right + kScrollBarWidth, gControls->aw->compListBox.bottom);
	if ((evt->what == mouseUp) && (PtInRect( localPt, &r)))
	{    	    
	    LClick(localPt, evt->modifiers, gControls->aw->compList);
	    
	    SetRect(&r, gControls->aw->compListBox.left, gControls->aw->compListBox.top,
	                gControls->aw->compListBox.right + 1, gControls->aw->compListBox.bottom);
	    FrameRect(&r);
	}
	
	/* or un/check components */
	if ((evt->what == mouseUp) && (PtInRect( localPt, &gControls->aw->compListBox)))
	{
		LClick(localPt, evt->modifiers, gControls->aw->compList);
		AddUpdateRowHighlight(localPt);
		
		/* invert the checkbox rect */
		for (i=0; i<numRows; i++)
		{
			SetPt(&currCell, 0, i);
			LRect(&currCellRect, currCell, gControls->aw->compList);
			if (PtInRect(localPt, &currCellRect))
			{
				SetRect(&checkbox, currCellRect.left+4, currCellRect.top+2, 
							currCellRect.left+16, currCellRect.top+14);		
				INVERT_HIGHLIGHT(&checkbox);
				break;
			}
		}
		
		AddSetOptInfo(false);
	}
	
	/* Mouse Down */
	if ((evt->what == mouseDown) && (PtInRect( localPt, &gControls->aw->compListBox)))
	{
		/* show depressed button state */
	    for (i=0; i<numRows; i++)
		{
			SetPt(&currCell, 0, i);
			LRect(&currCellRect, currCell, gControls->aw->compList);
			if (PtInRect(localPt, &currCellRect))
			{
				SetRect(&checkbox, currCellRect.left+4, currCellRect.top+2, 
							currCellRect.left+16, currCellRect.top+14);	
				ppH = GetPixPat(rGrayPixPattern);
				FillCRect(&checkbox, ppH);	
				FrameRect(&checkbox);	
				if (gControls->cfg->comp[rowToComp[i]].selected)
				{
					/* draw check mark */
					MoveTo(checkbox.left+1, checkbox.top+1);
					LineTo(checkbox.right-2, checkbox.bottom-2);
					MoveTo(checkbox.right-2, checkbox.top+1);
					LineTo(checkbox.left+1, checkbox.bottom-2); 
				}
				/* create 3D depression */
				
				MoveTo(checkbox.left+1, checkbox.top+1);
				LineTo(checkbox.left+1, checkbox.bottom-1);
				MoveTo(checkbox.left+1, checkbox.top+1);
				LineTo(checkbox.right-1, checkbox.top+1);
				
				ForeColor(whiteColor);
				
				MoveTo(checkbox.right-1, checkbox.top+1);
				LineTo(checkbox.right-1, checkbox.bottom-1);
				MoveTo(checkbox.left+1, checkbox.bottom-1);
				LineTo(checkbox.right-1, checkbox.bottom-1);
				
				ForeColor(blackColor);
			
				if (ppH)
					DisposePixPat(ppH);
				break;
			}
		}
	}
			
	HLock((Handle)gControls->backB);
	r = (**(gControls->backB)).contrlRect;
	HUnlock((Handle)gControls->backB);
	if (PtInRect( localPt, &r))
	{
		/* reset all rows to be not highlighted */
		for (i=1; i<numRows; i++)
			gControls->cfg->comp[rowToComp[i]].highlighted = false;
		
		part = TrackControl(gControls->backB, evt->where, NULL);
		if (part)
		{ 
			/* extra handling since we used DrawString for static text msg 
			 * and framed our own listbox etc. 
			 */
			gControls->aw->compListBox.top = 0;
			EraseRect(&gControls->aw->compListBox);
			ClearDiskSpaceMsgs();
			
			KillControls(gWPtr);
			ShowComponentsWin();
			return;
		}
	}
			
	HLock((Handle)gControls->nextB);			
	r = (**(gControls->nextB)).contrlRect;
	HUnlock((Handle)gControls->nextB);
	if (PtInRect( localPt, &r))
	{
		/* reset all rows to be not highlighted */
		for (i=0; i<numRows; i++)
			gControls->cfg->comp[rowToComp[i]].highlighted = false;
			
		part = TrackControl(gControls->nextB, evt->where, NULL);
		if (part)
		{	
			if (!VerifyDiskSpace())
			    return;
			    		
			gControls->aw->compListBox.top = 0;
			EraseRect(&gControls->aw->compListBox);
			ClearDiskSpaceMsgs();
						
			KillControls(gWPtr);
			ShowTerminalWin();
			return;
		}
	}
	SetPort(oldPort);
}

void
UpdateAdditionsWin(void)
{
	Rect		r;
	Cell		c;
	int			i;
	GrafPtr		oldPort;
	GetPort(&oldPort);
	
	SetPort(gWPtr);
	
	MoveTo( gControls->aw->compListBox.left, gControls->aw->compListBox.top - kInterWidgetPad + 1);
	HLock(gControls->cfg->selAddMsg);
	DrawString( CToPascal(*gControls->cfg->selAddMsg));
	HUnlock(gControls->cfg->selAddMsg);
	
#if 0
	RGBColor backColorOld;
    Rect adjustedRect, *clRect = &gControls->aw->compListBox;
    SetRect(&adjustedRect, clRect->left, clRect->top+1, clRect->right, clRect->bottom-1);
    GetBackColor(&backColorOld);
    BackColor(whiteColor);
    EraseRect(&adjustedRect);
    RGBBackColor(&backColorOld);
#endif
   
	LUpdate( (*gControls->aw->compList)->port->visRgn, gControls->aw->compList);
	SetRect(&r, gControls->aw->compListBox.left, gControls->aw->compListBox.top,
	            gControls->aw->compListBox.right + 1, gControls->aw->compListBox.bottom);
	FrameRect(&r);	
	
	SetPt(&c, 0, 0);
	if (LGetSelect(true, &c, gControls->aw->compList))
	{
		HLock((Handle)gControls->aw->compDescTxt);
		SetRect(&r, (*gControls->aw->compDescTxt)->viewRect.left,
					(*gControls->aw->compDescTxt)->viewRect.top,
					(*gControls->aw->compDescTxt)->viewRect.right,
					(*gControls->aw->compDescTxt)->viewRect.bottom);
		HUnlock((Handle)gControls->aw->compDescTxt);
		TEUpdate(&r, gControls->aw->compDescTxt);	
	}
	
	DrawDiskSpaceMsgs( gControls->opt->vRefNum );
	
	for (i = 0; i < numRows; i++)
	{
		if (gControls->cfg->comp[rowToComp[i]].highlighted)
		{
			AddInitRowHighlight(i);
			break;
		}
	}
	
	SetPort(oldPort);
}

short		
AddGetCompRow(int compIdx)
{
	short i = kInvalidCompIdx;
	
	for (i=0; i<numRows; i++)
	{
		if (rowToComp[i] == compIdx)
			break;
	}
	
	return i;
}

void		
AddSetOptInfo(Boolean bDrawingWindow)
{
	Cell		currCell;
	int			row, beingSelected;
	Boolean 	setSelected;
	
	// so we must determine the row clicked and resolve its dependees
	// bumping up their ref counts if this row is selected
	// and down if this row is unselected
	if (!bDrawingWindow)
	{
		currCell = LLastClick(gControls->aw->compList);
		row = currCell.v;
		
		// toggle from on to off or vice versa
		if (gControls->cfg->comp[rowToComp[row]].selected)
			beingSelected = kNotSelected;
		else
			beingSelected = kSelected;
		ResolveDependees(rowToComp[row], beingSelected); 
	}
	
	// then update the UI
	for (row = 0; row < numRows; row++)
	{
		SetPt(&currCell, 0, row);
		if (gControls->cfg->comp[rowToComp[row]].selected == kSelected)
			setSelected = true;
		else
			setSelected = false;
		LSetSelect(setSelected, currCell, gControls->aw->compList);
	}
	
	ClearDiskSpaceMsgs();
	DrawDiskSpaceMsgs( gControls->opt->vRefNum );
}

void		
AddInitRowHighlight(int row)
{
	Cell hlCell;
	Rect hlCellRect;
	UInt8 hiliteVal;
	int i = 0;
	
	/* reset all highlighted markers */
	for (i=0; i<numRows; i++)
	{
		gControls->cfg->comp[rowToComp[i]].highlighted = false;
	}
	
	/* highlight and set marker for row to init */
	SetPt(&hlCell, 0, row);
	LRect(&hlCellRect, hlCell, gControls->aw->compList);
	INVERT_HIGHLIGHT(&hlCellRect);	
	AddUpdateLongDesc(row);
	gControls->cfg->comp[rowToComp[row]].highlighted = true; 
}

void		
AddUpdateRowHighlight(Point localPt)
{
	int i, j;
	Rect currCellRect, oldCellRect;
	Cell currCell, oldCell;
	UInt8			hiliteVal;
	
	for (i=0; i<numRows; i++) 
	{
		/* note: numComps above includes invisible components */
		SetPt(&currCell, 0, i);
		LRect(&currCellRect, currCell, gControls->aw->compList);
			
		/* mouse move landed over this cell */
		if (PtInRect( localPt, &currCellRect ))
		{
			if (!gControls->cfg->comp[rowToComp[i]].highlighted)
			{	
				/* highlight this cell */
				INVERT_HIGHLIGHT(&currCellRect);	
				AddUpdateLongDesc(i);
					
				/* unhighlight old one */
				for (j=0; j<numRows; j++)
				{
					if (gControls->cfg->comp[rowToComp[j]].highlighted)
					{
						SetPt(&oldCell, 0, j);
						LRect(&oldCellRect, oldCell, gControls->aw->compList);
							
						INVERT_HIGHLIGHT(&oldCellRect);
						gControls->cfg->comp[rowToComp[j]].highlighted = false;
					}
				}
					
				/* mark this row highlighted to prevent incorrect inversion */
				gControls->cfg->comp[rowToComp[i]].highlighted = true; 
			}
		}
	}
}

void		
AddUpdateLongDesc(int row)
{
	Rect		viewRect;
	
	HLock((Handle)gControls->aw->compDescBox);
	SetRect(&viewRect, (*gControls->aw->compDescBox)->contrlRect.left,
					   (*gControls->aw->compDescBox)->contrlRect.top,
					   (*gControls->aw->compDescBox)->contrlRect.right,
					   (*gControls->aw->compDescBox)->contrlRect.bottom);
	HUnlock((Handle)gControls->aw->compDescBox);
	viewRect.top += kInterWidgetPad;
	SetRect(&viewRect, viewRect.left + kTxtRectPad,
						viewRect.top + kTxtRectPad,
						viewRect.right - kTxtRectPad,
						viewRect.bottom - kTxtRectPad);
	EraseRect(&viewRect);
	
	HLock(gControls->cfg->comp[rowToComp[row]].longDesc);
	TESetText( *gControls->cfg->comp[rowToComp[row]].longDesc, 
				strlen(*gControls->cfg->comp[rowToComp[row]].longDesc), gControls->aw->compDescTxt );
	TEUpdate( &viewRect, gControls->aw->compDescTxt );
	HUnlock(gControls->cfg->comp[rowToComp[row]].longDesc);
}

void
EnableAdditionsWin(void)
{	
	EnableNavButtons();
	
	// TO DO
}

void
DisableAdditionsWin(void)
{
	DisableNavButtons();
	
	// TO DO
}
