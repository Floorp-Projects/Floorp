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

#include "CheckboxLDEF.h"
#include <ToolUtils.h>
#include <Lists.h>
#include <LowMem.h>
#include <TextUtils.h>

pascal void 
main(SInt16 message,Boolean selected,Rect *cellRect,Cell theCell,
	 SInt16 dataOffset,SInt16 dataLen,ListHandle theList)  
{
	switch(message)
	{
		case lDrawMsg:
			Draw(selected,cellRect,theCell,dataLen,theList);
			break;
			
		case lHiliteMsg:
			Highlight(cellRect, selected);
			break;
	}
}

void  
Draw(Boolean selected,Rect *cellRect,Cell theCell,SInt16 dataLen,
     ListHandle theList)
{
	Ptr			componentName;
	Rect		nameRect;
	GrafPtr		oldPort;
	
	GetPort(&oldPort);
	
	componentName = NewPtrClear(255);
	LGetCell(componentName, &dataLen, theCell, theList);
	
	if (dataLen > 0 && componentName)
	{
		Highlight(cellRect, selected);
		SetRect(&nameRect, cellRect->left+20, cellRect->top,
							cellRect->right, cellRect->bottom);
		TETextBox((char*)componentName, dataLen, &nameRect, teFlushDefault);
	}
	
	if (componentName)
		DisposePtr(componentName);
		
	SetPort(oldPort);
}

void
Highlight(Rect *cellRect, Boolean selected)
{
	if (selected)
		DrawCheckedCheckbox(cellRect);
	else
		DrawEmptyCheckbox(cellRect);
}

Rect
DrawEmptyCheckbox(Rect *cellRect)
{
	Rect checkbox;
	
	SetRect(&checkbox, cellRect->left+4, cellRect->top+2, 
						cellRect->left+16, cellRect->top+14);
	EraseRect(&checkbox);
	FrameRect(&checkbox);
	
	return checkbox;
}

void
DrawCheckedCheckbox(Rect *cellRect)
{
	Rect 		checkbox = DrawEmptyCheckbox(cellRect);
	
	/* now fill in check mark */
	
	MoveTo(checkbox.left+1, checkbox.top+1);
	LineTo(checkbox.right-2, checkbox.bottom-2);
	MoveTo(checkbox.right-2, checkbox.top+1);
	LineTo(checkbox.left+1, checkbox.bottom-2); 

}
