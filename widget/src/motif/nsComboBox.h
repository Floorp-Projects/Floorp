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

#ifndef nsComboBox_h__
#define nsComboBox_h__

#include "nsWindow.h"
#include "nsIComboBox.h"

/**
 * Native Motif Listbox wrapper
 */

class nsComboBox : public nsWindow,
                   public nsIListWidget,
                   public nsIComboBox
{

public:
    nsComboBox();
    ~nsComboBox();

    NS_IMETHOD_(nsrefcnt) AddRef();
    NS_IMETHOD_(nsrefcnt) Release();
    NS_IMETHOD QueryInterface(const nsIID& aIID, void** aInstancePtr);
   
    // nsIWidget overrides
    virtual void   GetBounds(nsRect &aRect);

    // nsIComboBox interface
    virtual void    AddItemAt(nsString &aItem, PRInt32 aPosition);
    virtual PRInt32 FindItem(nsString &aItem, PRInt32 aStartPos);
    virtual PRInt32 GetItemCount();
    virtual PRBool  RemoveItemAt(PRInt32 aPosition);
    virtual PRBool  GetItemAt(nsString& anItem, PRInt32 aPosition);
    virtual void    GetSelectedItem(nsString& aItem);
    virtual PRInt32 GetSelectedIndex();
    virtual void    SelectItem(PRInt32 aPosition);
    virtual void    Deselect() ;

    virtual void    PreCreateWidget(nsWidgetInitData *aInitData);

    void Create(nsIWidget *aParent,
                const nsRect &aRect,
                EVENT_CALLBACK aHandleEventFunction,
                nsIDeviceContext *aContext,
                nsIAppShell *aAppShell = nsnull,
                nsIToolkit *aToolkit = nsnull,
                nsWidgetInitData *aInitData = nsnull);

    void Create(nsNativeWidget aParent,
                const nsRect &aRect, 
                EVENT_CALLBACK aHandleEventFunction,
                nsIDeviceContext *aContext,
                nsIAppShell *aAppShell = nsnull,
                nsIToolkit *aToolkit = nsnull,
                nsWidgetInitData *aInitData = nsnull);

    virtual void SetForegroundColor(const nscolor &aColor);
    virtual void SetBackgroundColor(const nscolor &aColor);

    virtual PRBool OnMove(PRInt32 aX, PRInt32 aY);
    virtual PRBool OnPaint(nsPaintEvent & aEvent);
    virtual PRBool OnResize(nsSizeEvent &aEvent);

    // nsIComboBox interface
    void      SetMultipleSelection(PRBool aMultipleSelections);
    PRInt32   GetSelectedCount();
    void      GetSelectedIndices(PRInt32 aIndices[], PRInt32 aSize);

protected:
    Widget  mPullDownMenu;
    Widget  mOptionMenu;
    PRBool  mMultiSelect;

    Widget  * mItems; // an array of Widgets
    int       mMaxNumItems;
    int       mNumItems;

};

#endif // nsComboBox_h__
