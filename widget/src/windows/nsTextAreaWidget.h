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

#ifndef nsTextAreaWidget_h__
#define nsTextAreaWidget_h__

#include "nsdefs.h"
#include "nsWindow.h"
#include "nsSwitchToUIThread.h"
#include "nsTextHelper.h"

#include "nsITextAreaWidget.h"

/**
 * Native WIN32 multi-line edit control wrapper. 
 */

class nsTextAreaWidget :  public nsTextHelper
{

public:
    nsTextAreaWidget();
    virtual                 ~nsTextAreaWidget();

    // nsISupports
    NS_IMETHOD QueryInterface(REFNSIID aIID, void** aInstancePtr);                           
    NS_IMETHOD_(nsrefcnt) AddRef(void);                                       
    NS_IMETHOD_(nsrefcnt) Release(void);          
    
    // nsIWidget Overrides
    virtual PRBool          OnMove(PRInt32 aX, PRInt32 aY);
    virtual PRBool          OnPaint();
    virtual PRBool          OnResize(nsRect &aWindowRect);
    NS_IMETHOD              GetBounds(nsRect &aRect);
    NS_IMETHOD              Paint(nsIRenderingContext& aRenderingContext,
                                  const nsRect& aDirtyRect);


    virtual void            SetUpForPaint(HDC aHDC);

protected:

    virtual LPCTSTR         WindowClass();
    virtual DWORD           WindowStyle();
    virtual DWORD           WindowExStyle();

};

#endif // nsTextAreaWidget_h__
