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
