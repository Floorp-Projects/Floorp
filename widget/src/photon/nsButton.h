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

#ifndef nsButton_h__
#define nsButton_h__

#include "nsWidget.h"
#include "nsIButton.h"
#include <Pt.h>

/**
 * Native Photon button wrapper
 */

class nsButton : public nsWidget, public nsIButton
{

public:
  nsButton();
  virtual ~nsButton();

  //nsISupports
  NS_DECL_ISUPPORTS_INHERITED
  
  // nsIButton part
  NS_IMETHOD SetLabel(const nsString& aText);
  NS_IMETHOD GetLabel(nsString& aBuffer);
  
  // nsBaseWidget
  NS_IMETHOD Paint(nsIRenderingContext& aRenderingContext,
                 const nsRect& aDirtyRect);

  virtual PRBool OnMove(PRInt32 aX, PRInt32 aY);
  virtual PRBool OnPaint();
  virtual PRBool OnResize(nsRect &aWindowRect);

protected:
  NS_IMETHOD CreateNative( PtWidget_t* aParent );
  
  nsString    mLabel;
};

#endif // nsButton_h__
