/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
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

#ifndef nsListBox_h__
#define nsListBox_h__

#include "nsWidget.h"
#include "nsIListBox.h"
#include <qlistbox.h>

//=============================================================================
//
// nsQListBox class
//
//=============================================================================
class nsQListBox : public QListBox, public nsQBaseWidget
{
    Q_OBJECT
public:
    nsQListBox(nsWidget * widget,
               QWidget * parent = 0, 
               const char * name = 0, 
               WFlags f = 0);
    ~nsQListBox();
};

/**
 * Native QT Listbox wrapper
 */

class nsListBox :   public nsWidget,
                    public nsIListWidget,
                    public nsIListBox
{

public:
    nsListBox();
    virtual ~nsListBox();

    NS_DECL_ISUPPORTS_INHERITED

    virtual PRBool OnMove(PRInt32 aX, PRInt32 aY) { return PR_FALSE; }
    virtual PRBool OnPaint(nsPaintEvent & aEvent) { return PR_FALSE; }
    virtual PRBool OnResize(nsRect &aRect) { return PR_FALSE; }


    // nsIListBox
    NS_IMETHOD PreCreateWidget(nsWidgetInitData *aInitData);
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

protected:

    PRBool    mMultiSelect;
};

#endif // nsListBox_h__
