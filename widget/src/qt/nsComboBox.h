/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
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

#include <qcombobox.h>

//=============================================================================
//
// nsQComboBox class
//
//=============================================================================

class nsQComboBox : public QComboBox, public nsQBaseWidget
{
    Q_OBJECT
public:
	nsQComboBox(nsWidget * widget,
                QWidget * parent = 0, 
                const char * name = 0);
	~nsQComboBox();
};

/**
 * Native QT ComboBox wrapper
 */

class nsComboBox : public nsWidget,
                   public nsIListWidget,
                   public nsIComboBox
{

public:
    nsComboBox();
    ~nsComboBox();

    NS_DECL_ISUPPORTS

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

    virtual PRBool OnMove(PRInt32 aX, PRInt32 aY) { return PR_FALSE; }
    virtual PRBool OnPaint(nsPaintEvent & aEvent) { return PR_FALSE; }
    virtual PRBool OnResize(nsRect &aRect) { return PR_FALSE; }

    // nsIComboBox interface
    NS_IMETHOD      SetMultipleSelection(PRBool aMultipleSelections);
    PRInt32         GetSelectedCount();
    NS_IMETHOD      GetSelectedIndices(PRInt32 aIndices[], PRInt32 aSize);

protected:
    NS_IMETHOD  CreateNative(QWidget *parentWindow);

    /*
    QWidget * mCombo;
    QWidget * mPullDownMenu;
    QWidget * mOptionMenu;
    GList   * mItems;
    */
    PRBool    mMultiSelect;
    int       mNumItems;
};

#endif // nsComboBox_h__
