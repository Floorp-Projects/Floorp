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

#ifndef nsTabWidget_h__
#define nsTabWidget_h__

#include "nsWidget.h"
#include "nsITabWidget.h"
#include <qtabbar.h>

//=============================================================================
//
// nsQTabBar class
//
//=============================================================================
class nsQTabBar : public QTabBar, public nsQBaseWidget
{
    Q_OBJECT
public:
    nsQTabBar(nsWidget * widget,
              QWidget * parent = 0, 
              const char * name = 0);
    ~nsQTabBar();
};


/**
 * Native QT tab control wrapper
 */

class nsTabWidget : public nsWidget,
                    public nsITabWidget
{

public:
    nsTabWidget();
    virtual ~nsTabWidget();

    NS_DECL_ISUPPORTS

    // nsITabWidget part 
    NS_IMETHOD SetTabs(PRUint32 aNumberOfTabs, const nsString aTabLabels[]);
    NS_IMETHOD GetSelectedTab(PRUint32& aTabNumber);
  
    // nsIWidget overrides
    virtual PRBool OnMove(PRInt32 aX, PRInt32 aY) { return PR_FALSE; }
    virtual PRBool OnPaint(nsPaintEvent & aEvent) { return PR_FALSE; }
    virtual PRBool OnResize(nsRect &aRect) { return PR_FALSE; }

    NS_IMETHOD     GetBounds(nsRect &aRect);

protected:
    NS_IMETHOD CreateNative(QWidget * parentWidget);
};

#endif // nsTabWidget_h__
