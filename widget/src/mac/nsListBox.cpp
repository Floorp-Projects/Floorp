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
#include "nsMacResources.h"
#include <StringCompare.h>
#include <Resources.h>
#include <Folders.h>
#include <Appearance.h>
#if TARGET_CARBON
#include <ControlDefinitions.h>
#endif

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
	SetControlType(kControlListBoxProc);

	mListHandle	= nsnull;
	mMultiSelect = PR_FALSE;

  AcceptFocusOnClick(PR_TRUE);
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
	outRect = mBounds;
	outRect.x = outRect.y = 0;
	// inset to make space for border
	outRect.Deflate(2, 2);
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
	#define kLDESRsrcID 128 // The Appearance Manager needs a 'ldes' resource to create list controls

	nsMacResources::OpenLocalResourceFile();

		mValue = kLDESRsrcID;
		nsresult res = Inherited::Create(aParent, aRect, aHandleEventFunction, aContext, aAppShell, aToolkit, aInitData);
		mValue = 0;

	nsMacResources::CloseLocalResourceFile();

	if (res == NS_OK)
	{
		Size actualSize;
		::GetControlData(mControl, kControlNoPart, kControlListBoxListHandleTag, sizeof(ListHandle), (Ptr)&mListHandle, &actualSize);
		if (mListHandle)
		{
			SetMultipleSelection(mMultiSelect);
			StartDraw();
				::LSetDrawingMode(mVisible, mListHandle);
				::SetControlMinimum(mControl, 0);
				::SetControlMaximum(mControl, 0);
				::SetControlValue(mControl, 0);
  			EndDraw();
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
	PRBool eventHandled = PR_FALSE;
	switch (aEvent.message)
	{
		case NS_MOUSE_LEFT_DOUBLECLICK:
		case NS_MOUSE_LEFT_BUTTON_DOWN:
			eventHandled = Inherited::DispatchMouseEvent(aEvent);
			if (!eventHandled && mListHandle != NULL) {
				EventRecord* macEvent = (EventRecord*)aEvent.nativeMsg;
	
				StartDraw();
					Point where = macEvent->where;
					::GlobalToLocal(&where);
					::HandleControlClick(mControl, where, macEvent->modifiers, NULL);
					// ::LClick(where, macEvent->modifiers, mListHandle);
				EndDraw();

				ControlChanged(GetSelectedIndex());
				
				// since the mouseUp event will be consumed by TrackControl,
				// simulate the mouse up event immediately.
				nsMouseEvent mouseUpEvent = aEvent;
				mouseUpEvent.message = NS_MOUSE_LEFT_BUTTON_UP;
				Inherited::DispatchMouseEvent(mouseUpEvent);
			}
			break;
	}
	return (Inherited::DispatchMouseEvent(aEvent));
}

PRBool nsListBox::DispatchWindowEvent(nsGUIEvent& aEvent)
{
	PRBool eventHandled = Inherited::DispatchWindowEvent(aEvent);
	if (!eventHandled && mListHandle != NULL) {
		if (aEvent.message == NS_KEY_DOWN) {
	  		EventRecord* macEvent = (EventRecord*)aEvent.nativeMsg;
			SInt16 keyCode = (macEvent->message & keyCodeMask) >> 8;
			SInt16 charCode = (macEvent->message & charCodeMask);
			StartDraw();
			::HandleControlKey(mControl, keyCode, charCode, macEvent->modifiers);
			EndDraw();
			eventHandled = PR_TRUE;
		} else {
			switch (aEvent.message) {
			case NS_GOTFOCUS:
			case NS_LOSTFOCUS:
				StartDraw();
				::SetKeyboardFocus(mWindowPtr, mControl, (aEvent.message == NS_GOTFOCUS ? kControlFocusNextPart : kControlFocusNoPart));
				EndDraw();
				eventHandled = PR_TRUE;
				break;
			}
		}
	}
	return eventHandled;
}
