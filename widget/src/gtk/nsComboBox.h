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

#include "nsWidget.h"
#include "nsIComboBox.h"

/**
 * Native GTK+ Listbox wrapper
 */

#define nsComboBoxSuper nsWidget
class nsComboBox : public nsComboBoxSuper,
                   public nsIListWidget,
                   public nsIComboBox
{

public:
    nsComboBox();
    ~nsComboBox();

    NS_IMETHOD_(nsrefcnt) AddRef();
    NS_IMETHOD_(nsrefcnt) Release();
    NS_IMETHOD QueryInterface(const nsIID& aIID, void** aInstancePtr);

    // nsIComboBox interface
    NS_IMETHOD      AddItemAt(nsString &aItem, PRInt32 aPosition);
    virtual PRInt32 FindItem(nsString &aItem, PRInt32 aStartPos);
    virtual PRInt32 GetItemCount();
    virtual PRBool  RemoveItemAt(PRInt32 aPosition);
    virtual PRBool  GetItemAt(nsString& anItem, PRInt32 aPosition);
    NS_IMETHOD      GetSelectedItem(nsString& aItem);
    virtual PRInt32 GetSelectedIndex();
    NS_IMETHOD      SelectItem(PRInt32 aPosition);
    NS_IMETHOD      Deselect() ;

    virtual PRBool  OnMove(PRInt32 aX, PRInt32 aY);
    virtual PRBool  OnPaint(nsPaintEvent & aEvent);
    virtual PRBool  OnResize(nsSizeEvent &aEvent);

    // nsIComboBox interface
    NS_IMETHOD      SetMultipleSelection(PRBool aMultipleSelections);
    PRInt32         GetSelectedCount();
    NS_IMETHOD      GetSelectedIndices(PRInt32 aIndices[], PRInt32 aSize);

protected:
    NS_IMETHOD  CreateNative(GtkWidget *parentWindow);

    GtkWidget  *mCombo;  /* workaround for gtkcombo bug */
    GtkWidget  *mPullDownMenu;
    GtkWidget  *mOptionMenu;
    GList *mItems;
    PRBool  mMultiSelect;
    int     mNumItems;
};

#endif // nsComboBox_h__
