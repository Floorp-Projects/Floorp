/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: NPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.1 (the "License"); you may not use this file except in
 * compliance with the License. You may obtain a copy of the License at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is 
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or 
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the NPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the NPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#ifndef nsPopUpMenu_h__
#define nsPopUpMenu_h__

#include "nsIPopUpMenu.h"

#include <qpopupmenu.h>

class nsIWidget;
class nsQEventHandler;

/**
 * Native QT PopUp wrapper
 */

class nsPopUpMenu : public nsIPopUpMenu
{

public:
    nsPopUpMenu();
    virtual ~nsPopUpMenu();

    NS_DECL_ISUPPORTS
  
    NS_IMETHOD Create(nsIWidget * aParent);

    // nsIPopUpMenu Methods
    NS_IMETHOD AddItem(const nsString &aText);
    NS_IMETHOD AddItem(nsIMenuItem * aMenuItem);
    NS_IMETHOD AddMenu(nsIMenu * aMenu);
    NS_IMETHOD AddSeparator();
    NS_IMETHOD GetItemCount(PRUint32 &aCount);
    NS_IMETHOD GetItemAt(const PRUint32 aCount, nsIMenuItem *& aMenuItem);
    NS_IMETHOD InsertItemAt(const PRUint32 aCount, nsIMenuItem *& aMenuItem);
    NS_IMETHOD InsertItemAt(const PRUint32 aCount, 
                            const nsString & aMenuItemName);
    NS_IMETHOD InsertSeparator(const PRUint32 aCount);
    NS_IMETHOD RemoveItem(const PRUint32 aCount);
    NS_IMETHOD RemoveAll();
    NS_IMETHOD ShowMenu(PRInt32 aX, PRInt32 aY);
    NS_IMETHOD GetNativeData(void*& aData);
    NS_IMETHOD GetParent(nsIWidget*& aParent);

protected:

    nsString           mLabel;
    PRUint32           mNumMenuItems;
    nsIWidget        * mParent;
    QPopupMenu       * mMenu;
    PRUint32           mX;
    PRUint32           mY;
    nsQEventHandler  * mEventHandler;
};

#endif // nsPopUpMenu_h__

