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

#ifndef nsToolbarItemHolder_h___
#define nsToolbarItemHolder_h___

#include "nsIToolbarItemHolder.h"
#include "nsIToolbarItem.h"
#include "nsCOMPtr.h"
#include "nsIWidget.h"

struct nsRect;

//------------------------------------------------------------
class nsToolbarItemHolder : public nsIToolbarItemHolder, public nsIToolbarItem

                  
{
public:
    nsToolbarItemHolder();
    virtual ~nsToolbarItemHolder();

    NS_DECL_ISUPPORTS

    // nsIToolbarItemHolder
    NS_IMETHOD SetWidget(nsIWidget * aWidget);
    NS_IMETHOD GetWidget(nsIWidget *&aWidget);

    // nsIToolbarItem
    NS_IMETHOD Repaint(PRBool aIsSynchronous);
    NS_IMETHOD GetBounds(nsRect & aRect);
    NS_IMETHOD SetBounds(PRUint32 aWidth, PRUint32 aHeight, PRBool aRepaint);
    NS_IMETHOD SetBounds(PRUint32 aX, PRUint32 aY, PRUint32 aWidth, PRUint32 aHeight, PRBool aRepaint);
    NS_IMETHOD SetVisible(PRBool aState);
    NS_IMETHOD IsVisible(PRBool & aState);
    NS_IMETHOD SetLocation(PRUint32 aX, PRUint32 aY);
    NS_IMETHOD GetPreferredSize(PRInt32& aWidth, PRInt32& aHeight);
    NS_IMETHOD SetPreferredSize(PRInt32 aWidth, PRInt32 aHeight);

protected:
  
  nsCOMPtr<nsIWidget> mWidget;

};

#endif /* nsToolbarItemHolder_h___ */
