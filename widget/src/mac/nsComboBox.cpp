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

NS_IMPL_ADDREF(nsComboBox);
NS_IMPL_RELEASE(nsComboBox);

//-------------------------------------------------------------------------
//
// nsComboBox constructor
//
//-------------------------------------------------------------------------
nsComboBox::nsComboBox() : nsMacControl(), nsIListWidget(), nsIComboBox()
{
	NS_INIT_REFCNT();
	strcpy(gInstanceClassName, "nsComboBox");
	SetControlType(kControlPopupButtonProc | kControlPopupFixedWidthVariant);

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
		::DisposeMenu(mMenuHandle);
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
			::SetControlData(mControl, kControlNoPart, kControlPopupButtonMenuHandleTag, sizeof(mMenuHandle), (Ptr)mMenuHandle);
			::SetControlData(mControl, kControlNoPart, kControlPopupButtonMenuIDTag, sizeof(mMenuID), (Ptr)mMenuID);

			PopupPrivateData* popupData = (PopupPrivateData*)*((*mControl)->contrlData);
			if (popupData)
			{
				popupData->mHandle = mMenuHandle;
				popupData->mID = mMenuID;
			}
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

	if (aPosition == -1)
		::MacAppendMenu(mMenuHandle, pString);
	else
		::MacInsertMenuItem(mMenuHandle, pString, aPosition);
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
	short itemCount = CountMenuItems(mMenuHandle);

	if (aStartPos < itemCount)
	{
		Str255 searchStr;
		StringToStr255(aItem, searchStr);

		for (short i = aStartPos; i < itemCount; i ++)
		{
			Str255	itemStr;
			::GetMenuItemText(mMenuHandle, i, itemStr);
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
PRInt32  nsComboBox::GetItemCount()
{
	if (! mMenuHandle)
		return 0;

	return CountMenuItems(mMenuHandle);
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

	::DeleteMenuItem(mMenuHandle, aPosition);
	return PR_TRUE;
}

//-------------------------------------------------------------------------
//
//  Removes an Item at a specified location
//
//-------------------------------------------------------------------------
PRBool nsComboBox::GetItemAt(nsString& anItem, PRInt32 aPosition)
{
	anItem = "";

	if (! mMenuHandle)
		return PR_FALSE;

	if (GetItemCount() < aPosition)
		return PR_FALSE;

	Str255	itemStr;
	::GetMenuItemText(mMenuHandle, aPosition, itemStr);
	Str255ToString(itemStr, anItem);

  return PR_TRUE;
}

//-------------------------------------------------------------------------
//
//  Gets the selected of selected item
//
//-------------------------------------------------------------------------
nsresult nsComboBox::GetSelectedItem(nsString& aItem)
{
	if (! mMenuHandle)
	{
		aItem = "";
		return PR_FALSE;
	}

	Str255	itemStr;
	::GetMenuItemText(mMenuHandle, ::GetControlValue(mControl), itemStr);
	Str255ToString(itemStr, aItem);

  return PR_TRUE;
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

	return ::GetControlValue(mControl);
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

	::SetControlValue(mControl, aPosition);
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
