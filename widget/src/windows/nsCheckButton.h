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

#ifndef nsCheckButton_h__
#define nsCheckButton_h__

#include "nsdefs.h"
#include "nsWindow.h"
#include "nsSwitchToUIThread.h"

#include "nsICheckButton.h"

/**
 * Native Win32 Checkbox wrapper
 */

class nsCheckButton : public nsWindow,
                      public nsICheckButton
{

public:
                            nsCheckButton(nsISupports *aOuter);
    virtual                 ~nsCheckButton();

    // nsISupports. Forward to the nsObject base class
    BASE_SUPPORT

    virtual nsresult        QueryObject(const nsIID& aIID, void** aInstancePtr);

    // nsIWidget interface
    BASE_IWIDGET_IMPL

    // nsICheckButton part
    virtual void            SetLabel(const nsString& aText);
    virtual void            GetLabel(nsString& aBuffer);

    virtual PRBool          OnMove(PRInt32 aX, PRInt32 aY);
    virtual PRBool          OnPaint();
    virtual PRBool          OnResize(nsRect &aWindowRect);

    virtual void            SetState(PRBool aState);
    virtual PRBool          GetState();


protected:

  virtual LPCTSTR         WindowClass();
  virtual DWORD           WindowStyle();
  virtual DWORD           WindowExStyle();

};

#endif // nsCheckButton_h__
