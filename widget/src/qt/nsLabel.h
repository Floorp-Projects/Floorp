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

#ifndef nsLabel_h__
#define nsLabel_h__

#include "nsWidget.h"
#include "nsIAppShell.h"
#include "nsILabel.h"
#include <qlabel.h>

//=============================================================================
//
// nsQLabel class
//
//=============================================================================
class nsQLabel : public QLabel, public nsQBaseWidget
{
    Q_OBJECT
public:
	nsQLabel(nsWidget * widget,
             QWidget * parent = 0, 
             const char * name = 0, 
             WFlags f = 0);
	~nsQLabel();
};

/**
 * Native QT Label wrapper
 */
class nsLabel :  public nsWidget,
                 public nsILabel
{

public:

    nsLabel();
    virtual ~nsLabel();

    NS_DECL_ISUPPORTS

    // nsILabel part
    NS_IMETHOD SetLabel(const nsString &aText);
    NS_IMETHOD GetLabel(nsString &aBuffer);
    NS_IMETHOD SetAlignment(nsLabelAlignment aAlignment);

    NS_IMETHOD PreCreateWidget(nsWidgetInitData *aInitData);

    virtual PRBool OnMove(PRInt32 aX, PRInt32 aY) { return PR_FALSE; }
    virtual PRBool OnPaint(nsPaintEvent & aEvent) { return PR_FALSE; }
    virtual PRBool OnResize(nsRect &aRect) { return PR_FALSE; }
    
protected:
    NS_METHOD CreateNative(QWidget *parentWindow);
    int GetNativeAlignment();
    nsLabelAlignment mAlignment;
};

#endif // nsLabel_h__
