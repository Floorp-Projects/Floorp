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

#include "nsdefs.h"
#include "nsWindow.h"
#include "nsSwitchToUIThread.h"
#include "nsIComboBox.h"

class nsComboBox : public nsWindow,
                   public nsIComboBox
{

public:
    nsComboBox(nsISupports *aOuter);
    ~nsComboBox();

    // nsISupports. Forward to the nsObject base class
    BASE_SUPPORT

    nsresult  QueryObject(const nsIID& aIID, void** aInstancePtr);
    PRBool nsComboBox::OnPaint();
    virtual PRBool OnResize(nsRect &aWindowRect);

    // nsIWidget interface
    BASE_IWIDGET_IMPL

    // nsIComboBox part
  
    void      AddItemAt(nsString &aItem, PRInt32 aPosition);
    PRInt32   FindItem(nsString &aItem, PRInt32 aStartPos);
    PRInt32   GetItemCount();
    PRBool    RemoveItemAt(PRInt32 aPosition);
    PRBool    GetItemAt(nsString& anItem, PRInt32 aPosition);
    void      GetSelectedItem(nsString& aItem);
    PRInt32   GetSelectedIndex();
    void      SelectItem(PRInt32 aPosition);
    void      Deselect() ;

protected:

    LPCTSTR WindowClass();
    DWORD   WindowStyle();
    DWORD   WindowExStyle();

};

#endif // nsComboBox_h__
