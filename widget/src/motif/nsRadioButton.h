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

#include "nsWindow.h"
#include "nsIRadioButton.h"

/**
 * Native Motif Radiobutton wrapper
 */

class nsRadioButton : public nsWindow
                      
{

public:

  nsRadioButton(nsISupports *aOuter);
  virtual                 ~nsRadioButton();

  NS_IMETHOD QueryObject(REFNSIID aIID, void** aInstancePtr);

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

  // nsIRadioButton part
  virtual void            SetLabel(const nsString& aText);
  virtual void            GetLabel(nsString& aBuffer);

  virtual PRBool          OnMove(PRInt32 aX, PRInt32 aY);
  virtual PRBool          OnPaint(nsPaintEvent &aEvent);
  virtual PRBool          OnResize(nsSizeEvent &aEvent);

  virtual void            SetState(PRBool aState);
  virtual PRBool          GetState();

  // These are needed to Override the auto check behavior
  void Armed();
  void DisArmed();

private:

  // this should not be public
  static PRInt32 GetOuterOffset() {
    return offsetof(nsRadioButton,mAggWidget);
  }

  Widget  mRadioBtn;
  Boolean mInitialState;
  Boolean mNewValue;
  Boolean mValueWasSet;
  Boolean mIsArmed;

  // Aggregator class and instance variable used to aggregate in the
  // nsIButton interface to nsRadioButton w/o using multiple
  // inheritance.
  class AggRadioButton : public nsIRadioButton {
  public:
    AggRadioButton();
    virtual ~AggRadioButton();

    AGGREGATE_METHOD_DEF

    // nsIRadioButton
    virtual void   SetLabel(const nsString &aText);
    virtual void   GetLabel(nsString &aBuffer);
    virtual void   SetState(PRBool aState);
    virtual PRBool GetState();

  };
  AggRadioButton mAggWidget;
  friend class AggRadioButton;

};

#endif // nsRadioButton_h__
