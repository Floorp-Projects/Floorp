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

    NS_DECL_ISUPPORTS_INHERITED

    // nsICheckButton
    NS_IMETHOD SetLabel(const nsString &aText);
    NS_IMETHOD GetLabel(nsString &aBuffer);
    NS_IMETHOD SetState(const PRBool aState);
    NS_IMETHOD GetState(PRBool& aState);

    virtual PRBool OnMove(PRInt32 aX, PRInt32 aY) { return PR_FALSE; }
    virtual PRBool OnPaint(nsPaintEvent & aEvent) { return PR_FALSE; }
    virtual PRBool OnResize(nsRect &aRect) { return PR_FALSE; }

protected:
    QWidget *mLabel;
};

#endif // nsCheckButton_h__
