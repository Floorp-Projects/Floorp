/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
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

#include "nsdefs.h"
#include "nsWindow.h"
#include "nsSwitchToUIThread.h"

#include "nsICheckButton.h"

#include <CheckBox.h>

/**
 * Native Win32 Checkbox wrapper
 */

class nsCheckButton : public nsWindow,
                      public nsICheckButton
{

public:
                            nsCheckButton();
    virtual                 ~nsCheckButton();

    // nsISupports
    NS_IMETHOD QueryInterface(REFNSIID aIID, void** aInstancePtr);                           
    NS_IMETHOD_(nsrefcnt) AddRef(void);                                       
    NS_IMETHOD_(nsrefcnt) Release(void);                                      
  
    // nsICheckButton part
    NS_IMETHOD SetLabel(const nsString &aText);
    NS_IMETHOD GetLabel(nsString &aBuffer);
    NS_IMETHOD SetState(const PRBool aState);
    NS_IMETHOD GetState(PRBool& aState);

    NS_IMETHOD Paint(nsIRenderingContext& aRenderingContext,
                     const nsRect& aDirtyRect);

    virtual PRBool          OnMove(PRInt32 aX, PRInt32 aY);
    virtual PRBool          OnPaint(nsRect &r);
    virtual PRBool          OnResize(nsRect &aWindowRect);


protected:
  PRBool mState; 
  virtual BView *CreateBeOSView();
  BCheckBox	*mCheckBox;
};

//
// A BCheckBox subclass
//
class nsCheckBoxBeOS : public BCheckBox, public nsIWidgetStore {
  public:
    nsCheckBoxBeOS( nsIWidget *aWidgetWindow, BRect aFrame, const char *aName,
        const char *aLabel, BMessage *aMessage, uint32 aResizingMode = B_FOLLOW_LEFT | B_FOLLOW_TOP, 
        uint32 aFlags = B_WILL_DRAW | B_NAVIGABLE );
};

#endif // nsCheckButton_h__
