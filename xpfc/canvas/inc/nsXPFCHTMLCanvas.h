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

#ifndef nsXPFCHTMLCanvas_h___
#define nsXPFCHTMLCanvas_h___

#include "nsXPFCCanvas.h"
#include "nsIWebShell.h"

class nsXPFCHTMLCanvas : public nsXPFCCanvas
{

public:

  nsXPFCHTMLCanvas(nsISupports* outer);

  NS_DECL_ISUPPORTS

  NS_IMETHOD Init();
  NS_IMETHOD SetBounds(const nsRect& aBounds);

  // nsIXMLParserObject methods
  NS_IMETHOD SetParameter(nsString& aKey, nsString& aValue) ;

  NS_IMETHOD_(nsEventStatus) HandleEvent(nsGUIEvent *aEvent);
  NS_IMETHOD_(nsEventStatus) OnPaint(nsIRenderingContext& aRenderingContext,
                                     const nsRect& aDirtyRect);

  NS_IMETHOD_(nsEventStatus) OnResize(nscoord aX, nscoord aY, nscoord aWidth, nscoord aHeight);
  NS_IMETHOD GetClassPreferredSize(nsSize& aSize);

protected:
  ~nsXPFCHTMLCanvas();

private:
    nsIWebShell * mWebShell;


};

#endif /* nsXPFCHTMLCanvas_h___ */
