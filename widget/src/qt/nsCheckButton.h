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

#ifndef nsCheckButton_h__
#define nsCheckButton_h__

#include "nsWidget.h"
#include "nsICheckButton.h"
#include <qcheckbox.h>

//=============================================================================
//
// nsQCheckBox class
//
//=============================================================================
class nsQCheckBox : public QCheckBox, public nsQBaseWidget
{
    Q_OBJECT
public:
    nsQCheckBox(nsWidget * widget,
                QWidget * parent = 0, 
                const char * name = 0);
    ~nsQCheckBox();
};

/**
 * Native QT Checkbox wrapper
 */

class nsCheckButton : public nsWidget,
                      public nsICheckButton
{

public:
    nsCheckButton();
    virtual ~nsCheckButton();

    NS_DECL_ISUPPORTS

    // nsICheckButton
    NS_IMETHOD SetLabel(const nsString &aText);
    NS_IMETHOD GetLabel(nsString &aBuffer);
    NS_IMETHOD SetState(const PRBool aState);
    NS_IMETHOD GetState(PRBool& aState);

    virtual PRBool OnMove(PRInt32 aX, PRInt32 aY) { return PR_FALSE; }
    virtual PRBool OnPaint(nsPaintEvent & aEvent) { return PR_FALSE; }
    virtual PRBool OnResize(nsRect &aRect) { return PR_FALSE; }

protected:
    NS_IMETHOD CreateNative(QWidget *parentWindow);
    QWidget *mLabel;
};

#endif // nsCheckButton_h__
