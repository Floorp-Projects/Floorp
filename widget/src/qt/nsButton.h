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

#ifndef nsButton_h__
#define nsButton_h__

#include "nsWidget.h"
#include "nsIButton.h"
#include <qpushbutton.h>

//=============================================================================
//
// nsQPushButton class
//
//=============================================================================
class nsQPushButton : public QPushButton, public nsQBaseWidget
{
    Q_OBJECT
public:
    nsQPushButton(nsWidget * widget,
                  QWidget * parent = 0, 
                  const char * name = 0);
    ~nsQPushButton();

protected:
#if 0
    bool eventFilter(QObject * object, QEvent * event);
    virtual void mousePressedEvent(QMouseEvent * event);
    virtual void mouseReleasedEvent(QMouseEvent * event);
#endif
};

/**
 * Native QT button wrapper
 */
class nsButton : public nsWidget,
                 public nsIButton
{

public:
    nsButton();
    virtual ~nsButton();

    NS_DECL_ISUPPORTS_INHERITED

    // nsIButton
    NS_IMETHOD     SetLabel(const nsString& aText);
    NS_IMETHOD     GetLabel(nsString& aBuffer);

    virtual PRBool OnMove(PRInt32 aX, PRInt32 aY) { return PR_FALSE; }
    virtual PRBool OnPaint(nsPaintEvent &aEvent) { return PR_FALSE; }
    virtual PRBool OnResize(nsRect &aRect) { return PR_FALSE; }

protected:
    virtual void InitCallbacks(char * aName = nsnull);
};

#endif // nsButton_h__
