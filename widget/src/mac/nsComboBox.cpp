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

#include "nsComboBox.h"
#include "nsMenu.h"
#include <StringCompare.h>
#if TARGET_CARBON
#include <ControlDefinitions.h>
#endif

NS_IMPL_ADDREF(nsComboBox);
NS_IMPL_RELEASE(nsComboBox);

#define DASH					'-'
#define OPTION_DASH		'Ð'


//-------------------------------------------------------------------------
//
// nsComboBox constructor
//
//-------------------------------------------------------------------------
nsComboBox::nsComboBox() : nsMacControl(), nsIListWidget(), nsIComboBox()
{
	NS_INIT_REFCNT();
	strcpy(gInstanceClassName, "nsComboBox");
	SetControlType(kControlPopupButtonProc + kControlPopupFixedWidthVariant);

	mMenuHandle	= nsnull;
	mMenuID		=  -12345;	// so that the control doesn't look for a menu in resource
	mMin		= mMenuID;	// when creating a popup, 'min' must contain the menuID
}

//-------------------------------------------------------------------------
//
// nsComboBox:: destructor
//
//-------------------------------------------------------------------------
nsComboBox::~nsComboBox()
{
	if (mMenuHandle)
	{
//		::DeleteMenu(mMenuID);
//		::DisposeMenu(mMenuHandle);
		mMenuHandle = nsnull;
	}
}

//-------------------------------------------------------------------------
//
//
//-------------------------------------------------------------------------
NS_IMETHODIMP nsComboBox::Create(nsIWidget *aParent,
                      const nsRect &aRect,
                      EVENT_CALLBACK aHandleEventFunction,
                      nsIDeviceContext *aContext,
                      nsIAppShell *aAppShell,
                      nsIToolkit *aToolkit,
                      nsWidgetInitData *aInitData) 
{
	nsresult res = Inherited::Create(aParent, aRect, aHandleEventFunction,
						aContext, aAppShell, aToolkit, aInitData);

  	if (res == NS_OK)
  	{
  		mMenuID = nsMenu::GetUniqueMenuID();
  		mMenuHandle = ::NewMenu(mMenuID, "\p");
  		if (mMenuHandle)
  		{
  			StartDraw();
  			::InsertMenu(mMenuHandle, hierMenu);
//			::SetControlData(mControl, kControlNoPart, kControlPopupButtonMenuHandleTag, sizeof(mMenuHandle), (Ptr)&mMenuHandle);
//			::SetControlData(mControl, kControlNoPart, kControlPopupButtonMenuIDTag, sizeof(mMenuID), (Ptr)&mMenuID);
#if TARGET_CARBON
			::SetControlPopupMenuHandle ( mControl, mMenuHandle );
			::SetControlPopupMenuID ( mControl, mMenuID );
#else
			PopupPrivateData* popupData = (PopupPrivateData*)*((*mControl)->contrlData);
			if (popupData)
			{
				popupData->mHandle = mMenuHandle;
				popupData->mID = mMenuID;
			}
#endif
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
nsresult nsComboBox::QueryInterface(const nsIID& aIID, void** aInstancePtr)
{
    if (NULL == aInstancePtr) {
        return NS_ERROR_NULL_POINTER;
    }

	static NS_DEFINE_IID(kInsComboBoxIID, NS_ICOMBOBOX_IID);
	static NS_DEFINE_IID(kInsListWidgetIID, NS_ILISTWIDGET_IID);

	if (aIID.Equals(kInsComboBoxIID)) {
		*aInstancePtr = (void*) ((nsIComboBox*)this);
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

#pragma mark -
//-------------------------------------------------------------------------
//
//  AddItemAt
//
//-------------------------------------------------------------------------

nsresult nsComboBox::AddItemAt(nsString &aItem, PRInt32 aPosition)
{
	if (! mMenuHandle)
		return NS_ERROR_NOT_INITIALIZED;

	Str255 pString;
	StringToStr255(aItem, pString);

	// Mac hacks: a menu item should not be an empty string
	// (otherwise a new item is not appended to the menu)
	// and it should not contain a dash '-' as first character
	// (otherwise it is considered as a menu separator even though
	// we are doing a 2-step initialization with MacInsertMenuItem()
	// and SetMenuItemText()).
	if (pString[0] == 0)
	{
		pString[0] = 1;
		pString[1] = ' ';
	}
	else if (pString[1] == DASH) {
		pString[1] = OPTION_DASH;
	}

	StartDraw();

	short menuItems = GetItemCount();
	if (aPosition == -1 || aPosition > menuItems) {
		aPosition = menuItems;
	}
	::MacInsertMenuItem(mMenuHandle, "\p ", aPosition);
	::SetMenuItemText(mMenuHandle, aPosition + 1, pString);

	menuItems = GetItemCount();
	::SetControlMinimum(mControl, (menuItems ? 1 : 0));
	::SetControlMaximum(mControl, (menuItems ? menuItems : 0));
	EndDraw();

	return NS_OK;
}

//-------------------------------------------------------------------------
//
//  Finds an item at a postion
//
//-------------------------------------------------------------------------
PRInt32  nsComboBox::FindItem(nsString &aItem, PRInt32 aStartPos)
{
	if (! mMenuHandle)
		return -1;

	PRInt32 index = -1;
	short itemCount = GetItemCount();
	for (short i = aStartPos; i < itemCount; i ++)
	{
		nsString temp;
		GetItemAt(temp, i);
		if (aItem == temp)
		{
			index = i;
			break;
		}
	}
	return index;
}

//-------------------------------------------------------------------------
//
//  CountItems - Get Item Count
//
//-------------------------------------------------------------------------
PRInt32  nsComboBox::GetItemCount()
{
	if (! mMenuHandle)
		return 0;

	return ::CountMenuItems(mMenuHandle);
}

//-------------------------------------------------------------------------
//
//  Removes an Item at a specified location
//
//-------------------------------------------------------------------------
PRBool  nsComboBox::RemoveItemAt(PRInt32 aPosition)
{
	if (! mMenuHandle)
		return PR_FALSE;

	if (GetItemCount() < aPosition)
		return PR_FALSE;

	StartDraw();
	if (aPosition == -1)
		::DeleteMenuItem(mMenuHandle, GetItemCount());
	else
		::DeleteMenuItem(mMenuHandle, aPosition + 1);

	short menuItems = GetItemCount();
	::SetControlMinimum(mControl, (menuItems ? 1 : 0));
	::SetControlMaximum(mControl, (menuItems ? menuItems : 0));
	EndDraw();

	return PR_TRUE;
}

//-------------------------------------------------------------------------
//
//  Returns an Item at a specified location
//
//-------------------------------------------------------------------------
PRBool nsComboBox::GetItemAt(nsString& anItem, PRInt32 aPosition)
{
	anItem = "";

	if (! mMenuHandle)
		return PR_FALSE;

	if (aPosition >= GetItemCount())
		return PR_FALSE;

	Str255	itemStr;
	::GetMenuItemText(mMenuHandle, aPosition + 1, itemStr);
	if (itemStr[1] == OPTION_DASH)	// see AddItemAt() for more info
		itemStr[1] = DASH;
	Str255ToString(itemStr, anItem);

  return PR_TRUE;
}

//-------------------------------------------------------------------------
//
//  Gets the selected of selected item
//
//-------------------------------------------------------------------------
nsresult nsComboBox::GetSelectedItem(nsString& anItem)
{
	return GetItemAt(anItem, GetSelectedIndex());
}

//-------------------------------------------------------------------------
//
//  Gets the list of selected otems
//
//-------------------------------------------------------------------------
PRInt32 nsComboBox::GetSelectedIndex()
{  
	if (! mMenuHandle)
		return -1;

	return ::GetControlValue(mControl) - 1;
}

//-------------------------------------------------------------------------
//
//  SelectItem
//
//-------------------------------------------------------------------------
nsresult nsComboBox::SelectItem(PRInt32 aPosition)
{
	if (! mMenuHandle)
		return NS_ERROR_NOT_INITIALIZED;

	StartDraw();
	if (aPosition == -1)
		::SetControlValue(mControl, GetItemCount());
	else
		::SetControlValue(mControl, aPosition + 1);
	EndDraw();

	return NS_OK;
}

//-------------------------------------------------------------------------
//
//  Deselect
//
//-------------------------------------------------------------------------
nsresult nsComboBox::Deselect()
{
	return SelectItem(0);
}

//-------------------------------------------------------------------------
//
//  DispatchMouseEvent
//
//-------------------------------------------------------------------------
PRBool nsComboBox::DispatchMouseEvent(nsMouseEvent &aEvent)
{
	PRBool eventHandled = PR_FALSE;
	switch (aEvent.message)
	{
		case NS_MOUSE_LEFT_DOUBLECLICK:
		case NS_MOUSE_LEFT_BUTTON_DOWN:
			eventHandled = Inherited::DispatchMouseEvent(aEvent);
			if (!eventHandled) {
				StartDraw();
				Point thePoint;
				thePoint.h = aEvent.point.x;
				thePoint.v = aEvent.point.y;
				::TrackControl(mControl, thePoint, nil);
				ControlChanged(::GetControlValue(mControl));
				EndDraw();
				
				// since the mouseUp event will be consumed by TrackControl,
				// simulate the mouse up event immediately.
				nsMouseEvent mouseUpEvent = aEvent;
				mouseUpEvent.message = NS_MOUSE_LEFT_BUTTON_UP;
				Inherited::DispatchMouseEvent(mouseUpEvent);
			}
			break;
		default:
			eventHandled = Inherited::DispatchMouseEvent(aEvent);
			break;
	}
	return eventHandled;
}
