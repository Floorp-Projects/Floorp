/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
 */

#ifndef nsComboBox_h__
#define nsComboBox_h__

#include "nsMacControl.h"
#include "nsIComboBox.h"


class nsComboBox :	public nsMacControl,
					public nsIListWidget,
					public nsIComboBox
{
private:
	typedef nsMacControl Inherited;

public:
   	nsComboBox();
	virtual ~nsComboBox();

	// nsISupports
	NS_IMETHOD_(nsrefcnt) AddRef();
	NS_IMETHOD_(nsrefcnt) Release();
	NS_IMETHOD QueryInterface(const nsIID& aIID, void** aInstancePtr);

	// nsIWidget
	NS_IMETHODIMP				Create(nsIWidget *aParent,
									const nsRect &aRect,
									EVENT_CALLBACK aHandleEventFunction,
									nsIDeviceContext *aContext = nsnull,
									nsIAppShell *aAppShell = nsnull,
									nsIToolkit *aToolkit = nsnull,
									nsWidgetInitData *aInitData = nsnull);

	// nsIComboBox part
	NS_IMETHOD				AddItemAt(nsString &aItem, PRInt32 aPosition);
    NS_IMETHOD_(PRInt32)	FindItem(nsString &aItem, PRInt32 aStartPos);
    NS_IMETHOD_(PRInt32)	GetItemCount();
    NS_IMETHOD_(PRBool)		RemoveItemAt(PRInt32 aPosition);
	NS_IMETHOD_(PRBool)		GetItemAt(nsString& anItem, PRInt32 aPosition);
	NS_IMETHOD				GetSelectedItem(nsString &aItem);
    NS_IMETHOD_(PRInt32)	GetSelectedIndex();
    NS_IMETHOD				SelectItem(PRInt32 aPosition);
    NS_IMETHOD				Deselect();

	// nsWindow
	virtual PRBool			DispatchMouseEvent(nsMouseEvent &aEvent);

protected:
	PRInt16		mMenuID;
	MenuHandle	mMenuHandle;
};


#endif // nsComboBox_h__
