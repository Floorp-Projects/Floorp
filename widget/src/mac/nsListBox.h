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

#ifndef nsListBox_h__
#define nsListBox_h__

#include "nsMacControl.h"
#include "nsIListBox.h"
#include <Lists.h>


class nsListBox :	public nsMacControl,
					public nsIListWidget,
					public nsIListBox
{
private:
	typedef nsMacControl Inherited;

public:
   	nsListBox();
	virtual ~nsListBox();

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

	NS_IMETHOD    	Show(PRBool aState);

	// nsIListBox part
	NS_IMETHOD SetMultipleSelection(PRBool aMultipleSelections);
	NS_IMETHOD AddItemAt(nsString &aItem, PRInt32 aPosition);
	PRInt32    FindItem(nsString &aItem, PRInt32 aStartPos);
	PRInt32    GetItemCount();
	PRBool     RemoveItemAt(PRInt32 aPosition);
	PRBool     GetItemAt(nsString& anItem, PRInt32 aPosition);
	NS_IMETHOD GetSelectedItem(nsString& aItem);
	PRInt32    GetSelectedIndex();
	PRInt32    GetSelectedCount();
	NS_IMETHOD GetSelectedIndices(PRInt32 aIndices[], PRInt32 aSize);
	NS_IMETHOD SetSelectedIndices(PRInt32 aIndices[], PRInt32 aSize);
	NS_IMETHOD SelectItem(PRInt32 aPosition);
	NS_IMETHOD Deselect() ;
	NS_IMETHOD PreCreateWidget(nsWidgetInitData *aInitData);

	// nsWindow
	virtual PRBool			DispatchMouseEvent(nsMouseEvent &aEvent);

protected:
	// nsMacControl
	virtual void			GetRectForMacControl(nsRect &outRect);

protected:
	ListHandle	mListHandle;
	PRBool		mMultiSelect;
};


#endif // nsListBox_h__
