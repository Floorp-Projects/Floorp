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

//#include "nsdefs.h"
#include "nsWindow.h"
#include "nsIButton.h"

/**
 * Native Motif button wrapper
 */

class nsButton :  public nsWindow
{

public:
  nsButton(nsISupports *aOuter);
  virtual ~nsButton();

  NS_IMETHOD QueryInterface(REFNSIID aIID, void** aInstancePtr);

  void Create(nsIWidget *aParent,
              const nsRect &aRect,
              EVENT_CALLBACK aHandleEventFunction,
              nsIDeviceContext *aContext = nsnull,
              nsIToolkit *aToolkit = nsnull,
              nsWidgetInitData *aInitData = nsnull);
  void Create(nsNativeWindow aParent,
              const nsRect &aRect,
              EVENT_CALLBACK aHandleEventFunction,
              nsIDeviceContext *aContext = nsnull,
              nsIToolkit *aToolkit = nsnull,
              nsWidgetInitData *aInitData = nsnull);


    // nsIButton part
  virtual void   SetLabel(const nsString& aText);
  virtual void   GetLabel(nsString& aBuffer);
  virtual PRBool OnPaint(nsPaintEvent & aEvent);
  virtual PRBool OnResize(nsRect &aWindowRect);

private:

  // this should not be public
  static PRInt32 GetOuterOffset() {
    return offsetof(nsButton,mAggWidget);
  }


  // Aggregator class and instance variable used to aggregate in the
  // nsIButton interface to nsButton w/o using multiple
  // inheritance.
  class AggButton : public nsIButton {
  public:
    AggButton();
    virtual ~AggButton();

    AGGRRGATE_METHOD_DEF

    // nsIButton
    virtual void SetLabel(const nsString &aText);
    virtual void GetLabel(nsString &aBuffer);

  };
  AggButton mAggWidget;


};

#endif // nsButton_h__
