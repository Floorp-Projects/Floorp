/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.0 (the "NPL"); you may not use this file except in
 * compliance with the NPL.  You may obtain a copy of the NPL at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the NPL is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the NPL
 * for the specific language governing rights and limitations under the
 * NPL.
 *
 * The Initial Developer of this code under the NPL is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation.  All Rights
 * Reserved.
 */

#include "nsListBox.h"
#include <StringCompare.h>
#include <Resources.h>

NS_IMPL_ADDREF(nsListBox);
NS_IMPL_RELEASE(nsListBox);

typedef struct ldesRsrc
{
	short	version;
	short	rows;
	short	cols;
	short	cellHeight;
	short	cellWidth;
	Boolean	vertScroll;
	Boolean	__pad1;
	Boolean	horizScroll;
	Boolean	__pad2;
	short	ldefID;
	Boolean	hasGrow;
	Boolean	__pad3;
} ldesRsrc;


//-------------------------------------------------------------------------
//
// nsListBox constructor
//
//-------------------------------------------------------------------------
nsListBox::nsListBox() : nsMacControl(), nsIListWidget(), nsIListBox()
{
	NS_INIT_REFCNT();
	strcpy(gInstanceClassName, "nsListBox");
	SetControlType(kControlListBoxAutoSizeProc);

	mListHandle	= nsnull;
	mMultiSelect = PR_FALSE;
}

//-------------------------------------------------------------------------
//
// nsListBox:: destructor
//
//-------------------------------------------------------------------------
nsListBox::~nsListBox()
{
}

//-------------------------------------------------------------------------
//
//
//-------------------------------------------------------------------------
void nsListBox::GetRectForMacControl(nsRect &outRect)
{
	//¥TODO: we can certainly remove this function if we
	// implement GetDesiredSize() in nsComboboxControlFrame
	outRect = mBounds;
	outRect.x = 1;
	outRect.y = 1;
	outRect.width -= 1;
	outRect.height -= 1;
}

//-------------------------------------------------------------------------
//
//
//-------------------------------------------------------------------------
NS_IMETHODIMP nsListBox::Create(nsIWidget *aParent,
                      const nsRect &aRect,
                      EVENT_CALLBACK aHandleEventFunction,
                      nsIDeviceContext *aContext,
                      nsIAppShell *aAppShell,
                      nsIToolkit *aToolkit,
                      nsWidgetInitData *aInitData) 
{
	Handle resH = ::NewHandleClear(sizeof(ldesRsrc));
	if (resH)
	{
		ldesRsrc** ldesH = (ldesRsrc**)resH;
		(*ldesH)->cols = 1;
		(*ldesH)->vertScroll = 1;
		short resID = mValue = 2222;
		::AddResource(resH, 'ldes', resID, "\p");
	}

	nsresult res = Inherited::Create(aParent, aRect, aHandleEventFunction,
						aContext, aAppShell, aToolkit, aInitData);
	if (resH)
	{
		::RemoveResource(resH);
		::DisposeHandle(resH);
	}

  	if (res == NS_OK)
  	{
		Size actualSize;
		::GetControlData(mControl, kControlNoPart, kControlListBoxListHandleTag, sizeof(ListHandle), (Ptr)&mListHandle, &actualSize);
		if (mListHandle)
		{
			SetMultipleSelection(mMultiSelect);
			::SetControlMinimum(mControl, 0);
			::SetControlMaximum(mControl, 0);
			::LSetDrawingMode(mVisible, mListHandle);
		}
		else
  			res = NS_ERROR_FAILURE;
  	}

	return res;
}

//-------------------------------------------------------------------------
//
//
//-------------------------------------------------------------------------
nsresult nsListBox::QueryInterface(const nsIID& aIID, void** aInstancePtr)
{
    if (NULL == aInstancePtr) {
        return NS_ERROR_NULL_POINTER;
    }

	static NS_DEFINE_IID(kInsListBoxIID, NS_ILISTBOX_IID);
	static NS_DEFINE_IID(kInsListWidgetIID, NS_ILISTWIDGET_IID);

	if (aIID.Equals(kInsListBoxIID)) {
		*aInstancePtr = (void*) ((nsIListBox*)this);
		NS_ADDREF_THIS();
		return NS_OK;
	}
	else if (aIID.Equals(kInsListWidgetIID)) {
		*aInstancePtr = (void*) ((nsIListWidget*)this);
		NS_ADDREF_THIS();
		return NS_OK;
	}

	return nsWindow::QueryInterface(aIID,aInstancePtr);
}

//-------------------------------------------------------------------------
//
//
//-------------------------------------------------------------------------
NS_IMETHODIMP nsListBox::Show(PRBool bState)
{
	if (! mListHandle)
		return NS_ERROR_NOT_INITIALIZED;

	if (mVisible == bState)
		return NS_OK;

  Inherited::Show(bState);
	StartDraw();
	::LSetDrawingMode(bState, mListHandle);
	EndDraw();

  return NS_OK;
}

#pragma mark -
//-------------------------------------------------------------------------
//
//  SetMultipleSelection
//
//-------------------------------------------------------------------------
NS_METHOD nsListBox::SetMultipleSelection(PRBool aMultipleSelections) 
{
	mMultiSelect = aMultipleSelections;

	if (mListHandle)
		(*mListHandle)->selFlags = (mMultiSelect ? (lUseSense | lNoRect | lNoExtend) : lOnlyOne);

	 return NS_OK;
}


//-------------------------------------------------------------------------
//
//  PreCreateWidget
//
//-------------------------------------------------------------------------
NS_METHOD nsListBox::PreCreateWidget(nsWidgetInitData *aInitData)
{
  if (nsnull != aInitData) {
    nsListBoxInitData* data = (nsListBoxInitData *) aInitData;
    mMultiSelect = data->mMultiSelect;
  }
  return NS_OK;
}


//-------------------------------------------------------------------------
//
//  AddItemAt
//
//-------------------------------------------------------------------------
nsresult nsListBox::AddItemAt(nsString &aItem, PRInt32 aPosition)
{
	if (! mListHandle)
		return NS_ERROR_NOT_INITIALIZED;

	Str255 pString;
	StringToStr255(aItem, pString);

	StartDraw();
	if (aPosition == -1)
		aPosition = GetItemCount() + 1;
	short rowNum = ::LAddRow(1, aPosition, mListHandle);

	Cell newCell;
	::SetPt(&newCell, 0, rowNum);
	::LSetCell(pString+1, *pString, newCell, mListHandle);

	::SetControlMaximum(mControl, GetItemCount());
	EndDraw();

	return NS_OK;
}

//-------------------------------------------------------------------------
//
//  Finds an item at a postion
//
//-------------------------------------------------------------------------
PRInt32  nsListBox::FindItem(nsString &aItem, PRInt32 aStartPos)
{
	if (! mListHandle)
		return -1;

	PRInt32 index = -1;
	short itemCount = GetItemCount();

	if (aStartPos < itemCount)
	{
		Str255 searchStr;
		StringToStr255(aItem, searchStr);

		for (short i = aStartPos; i < itemCount; i ++)
		{
			Str255	itemStr;
			short strSize = sizeof(itemStr) - 1;

			Cell theCell;
			::SetPt(&theCell, 0, i);

			::LGetCell(itemStr+1, &strSize, theCell, mListHandle);
			if (strSize > sizeof(itemStr) - 1) 
				strSize = sizeof(itemStr) - 1;
			itemStr[0] = strSize;

			if (::EqualString(itemStr, searchStr, FALSE, FALSE))
			{
				index = i;
				break;
			}
		}
	}
	return index;
}

//-------------------------------------------------------------------------
//
//  CountItems - Get Item Count
//
//-------------------------------------------------------------------------
PRInt32  nsListBox::GetItemCount()
{
	if (! mListHandle)
		return 0;

	return (*mListHandle)->dataBounds.bottom;
}

//-------------------------------------------------------------------------
//
//  Removes an Item at a specified location
//
//-------------------------------------------------------------------------
PRBool  nsListBox::RemoveItemAt(PRInt32 aPosition)
{
	if (! mListHandle)
		return PR_FALSE;

	if (GetItemCount() < aPosition)
		return PR_FALSE;

	StartDraw();
	::LDelRow(1, aPosition, mListHandle);
	::SetControlMaximum(mControl, GetItemCount());
	EndDraw();

	return PR_TRUE;
}

//-------------------------------------------------------------------------
//
//  Removes an Item at a specified location
//
//-------------------------------------------------------------------------
PRBool nsListBox::GetItemAt(nsString& anItem, PRInt32 aPosition)
{
	anItem = "";

	if (! mListHandle)
		return PR_FALSE;

	if (GetItemCount() < aPosition)
		return PR_FALSE;

	Str255	itemStr;
	short strSize = sizeof(itemStr) - 1;

	Cell theCell;
	::SetPt(&theCell, 0, aPosition);

	::LGetCell(itemStr+1, &strSize, theCell, mListHandle);
	if (strSize > sizeof(itemStr) - 1) 
		strSize = sizeof(itemStr) - 1;
	itemStr[0] = strSize;
	Str255ToString(itemStr, anItem);

	return PR_TRUE;
}

//-------------------------------------------------------------------------
//
//  Gets the content of selected item
//
//-------------------------------------------------------------------------
nsresult nsListBox::GetSelectedItem(nsString& aItem)
{
	aItem = "";

	if (! mListHandle)
		return PR_FALSE;

	PRInt32 selectedIndex = GetSelectedIndex();
	if (selectedIndex < 0)
		return PR_FALSE;

	GetItemAt(aItem, selectedIndex);
	return PR_TRUE;
}

//-------------------------------------------------------------------------
//
//  Gets the list of selected items
//
//-------------------------------------------------------------------------
PRInt32 nsListBox::GetSelectedIndex()
{  
	if (! mListHandle)
		return -1;

	Cell theCell;
	::SetPt(&theCell, 0, 0);

	if (::LGetSelect(true, &theCell, mListHandle))
		return theCell.v;
	else
		return -1;
}

//-------------------------------------------------------------------------
//
//  Gets the count of selected items
//
//-------------------------------------------------------------------------
PRInt32 nsListBox::GetSelectedCount()
{  
	if (! mListHandle)
		return 0;

	PRInt32 itemCount = 0;
	Cell theCell;
	::SetPt(&theCell, 0, 0);
	while (::LGetSelect(true, &theCell, mListHandle))
	{
		itemCount ++;
		::LNextCell(true, true, &theCell, mListHandle);
	}
	return itemCount;
}

//-------------------------------------------------------------------------
//
//  Gets the indices of selected items
//
//-------------------------------------------------------------------------
nsresult nsListBox::GetSelectedIndices(PRInt32 aIndices[], PRInt32 aSize)
{  
	if (! mListHandle)
		return NS_ERROR_NOT_INITIALIZED;

	PRInt32 itemCount = 0;
	Cell theCell;
	::SetPt(&theCell, 0, 0);
	while (::LGetSelect(true, &theCell, mListHandle) && (itemCount < aSize))
	{
		aIndices[itemCount] = theCell.v;
		itemCount ++;
		::LNextCell(true, true, &theCell, mListHandle);
	}
	return NS_OK;
}

//-------------------------------------------------------------------------
//
//  Sets selected items
//
//-------------------------------------------------------------------------
nsresult nsListBox::SetSelectedIndices(PRInt32 aIndices[], PRInt32 aSize)
{  
	if (! mListHandle)
		return NS_ERROR_NOT_INITIALIZED;

	StartDraw();
	::LSetDrawingMode(false, mListHandle);
	for (PRInt32 i = 0; i < aSize; i ++)
	{
		Cell theCell;
		::SetPt(&theCell, 0, 1);
		::LSetSelect(true, theCell, mListHandle);
	}
	::LAutoScroll(mListHandle);
	::LSetDrawingMode(mVisible, mListHandle);
	EndDraw();

	Invalidate(PR_TRUE);

	return NS_OK;
}

//-------------------------------------------------------------------------
//
//  SelectItem
//
//-------------------------------------------------------------------------
nsresult nsListBox::SelectItem(PRInt32 aPosition)
{
	if (! mListHandle)
		return NS_ERROR_NOT_INITIALIZED;

	if (GetItemCount() < aPosition)
		return PR_FALSE;

	if (! mMultiSelect)
		Deselect();

	StartDraw();
	Cell theCell;
	::SetPt(&theCell, 0, aPosition);
	::LSetSelect(true, theCell, mListHandle);
	::LAutoScroll(mListHandle);
	EndDraw();

	return NS_OK;
}

//-------------------------------------------------------------------------
//
//  Deselect
//
//-------------------------------------------------------------------------
nsresult nsListBox::Deselect()
{
	if (! mListHandle)
		return NS_ERROR_NOT_INITIALIZED;

	if (GetItemCount() == 0)
		return PR_FALSE;

	StartDraw();
	::LSetDrawingMode(false, mListHandle);
	Cell theCell;
	::SetPt(&theCell, 0, 0);
	while (::LGetSelect(true, &theCell, mListHandle))
	{
		::LSetSelect(false, theCell, mListHandle);
		::LNextCell(true, true, &theCell, mListHandle);
	}
	::LSetDrawingMode(mVisible, mListHandle);
	EndDraw();

	Invalidate(PR_TRUE);

	return NS_OK;
}

//-------------------------------------------------------------------------
//
//  DispatchMouseEvent
//
//-------------------------------------------------------------------------
PRBool nsListBox::DispatchMouseEvent(nsMouseEvent &aEvent)
{
	PRBool eatEvent = PR_FALSE;
	switch (aEvent.message)
	{
		case NS_MOUSE_LEFT_DOUBLECLICK:
		case NS_MOUSE_LEFT_BUTTON_DOWN:
			StartDraw();
			{
				Point thePoint;
				thePoint.h = aEvent.point.x;
				thePoint.v = aEvent.point.y;
				::TrackControl(mControl, thePoint, nil);
				//¥TODO: the mouseUp event is eaten by TrackControl.
				//¥ We must create it and dispatch it after the mouseDown;
				eatEvent = PR_TRUE;
			}
			EndDraw();
			break;
	}

	if (eatEvent)
		return PR_TRUE;
	return (Inherited::DispatchMouseEvent(aEvent));

}
