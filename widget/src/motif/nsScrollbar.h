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

#ifndef nsScrollbar_h__
#define nsScrollbar_h__

#include "nsWindow.h"

#include "nsIScrollbar.h"

/**
 * Native Motif scrollbar wrapper. 
 */

class nsScrollbar : public nsWindow,
                    public nsIScrollbar
{

public:
                            nsScrollbar(PRBool aIsVertical);
    virtual                 ~nsScrollbar();

    // nsISupports
    NS_IMETHOD_(nsrefcnt) AddRef();
    NS_IMETHOD_(nsrefcnt) Release();
    NS_IMETHOD QueryInterface(const nsIID& aIID, void** aInstancePtr);

    // nsIScrollBar implementation
    NS_IMETHOD SetMaxRange(PRUint32 aEndRange);
    NS_IMETHOD GetMaxRange(PRUint32& aMaxRange);
    NS_IMETHOD SetPosition(PRUint32 aPos);
    NS_IMETHOD GetPosition(PRUint32& aPos);
    NS_IMETHOD SetThumbSize(PRUint32 aSize);
    NS_IMETHOD GetThumbSize(PRUint32& aSize);
    NS_IMETHOD SetLineIncrement(PRUint32 aSize);
    NS_IMETHOD GetLineIncrement(PRUint32& aSize);
    NS_IMETHOD SetParameters(PRUint32 aMaxRange, PRUint32 aThumbSize,
                               PRUint32 aPosition, PRUint32 aLineIncrement);


  NS_IMETHOD Create(nsIWidget *aParent,
              const nsRect &aRect,
              EVENT_CALLBACK aHandleEventFunction,
              nsIDeviceContext *aContext = nsnull,
              nsIAppShell *aAppShell = nsnull,
              nsIToolkit *aToolkit = nsnull,
              nsWidgetInitData *aInitData = nsnull);

  NS_IMETHOD Create(nsNativeWidget aParent,
              const nsRect &aRect,
              EVENT_CALLBACK aHandleEventFunction,
              nsIDeviceContext *aContext = nsnull,
              nsIAppShell *aAppShell = nsnull,
              nsIToolkit *aToolkit = nsnull,
              nsWidgetInitData *aInitData = nsnull);


    virtual PRBool    OnPaint(nsPaintEvent & aEvent);
    virtual PRBool    OnScroll(nsScrollbarEvent & aEvent, PRUint32 cPos);
    virtual PRBool    OnResize(nsSizeEvent &aEvent);

private:

  PRUint32 mLineIncrement;
  int      mOrientation;

  int AdjustScrollBarPosition(int aPosition);

};

#endif // nsScrollbar_h__
