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
