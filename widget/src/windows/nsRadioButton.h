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

#ifndef nsRadioButton_h__
#define nsRadioButton_h__

#include "nsdefs.h"
#include "nsWindow.h"
#include "nsSwitchToUIThread.h"
#include "nsString.h"

#include "nsIRadioButton.h"
#include "nsIRadioGroup.h"


/**
 * Native Win32 Radio button wrapper
 */

class nsRadioButton : public nsWindow,
                      public nsIRadioButton
{

public:
                            nsRadioButton(nsISupports *aOuter);
    virtual                 ~nsRadioButton();

    // nsISupports. Forward to the nsObject base class
    BASE_SUPPORT

    virtual nsresult        QueryObject(const nsIID& aIID, void** aInstancePtr);

    // nsIWidget interface
    BASE_IWIDGET_IMPL

    // nsIRadioButton part
    virtual void            SetLabel(const nsString& aText);
    virtual void            GetLabel(nsString& aBuffer);

    virtual PRBool          OnPaint();
    virtual PRBool          OnResize(nsRect &aWindowRect);

    virtual void            SetState(PRBool aState);
    virtual void            SetStateNoNotify(PRBool aState);
    virtual PRBool          GetState();

    virtual nsIRadioGroup*  GetRadioGroup();
    virtual void            SetRadioGroup(nsIRadioGroup* aGroup);

protected:
    PRBool         fState;
    nsIRadioGroup* fRadioGroup;

    virtual LPCTSTR         WindowClass();
    virtual DWORD           WindowStyle();
    virtual DWORD           WindowExStyle();

    virtual PRBool          ProcessMessage(UINT msg, WPARAM wParam, LPARAM lParam, LRESULT *aRetValue);

};

#endif // nsRadioButton_h__
