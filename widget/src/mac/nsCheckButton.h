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

#include "nsWindow.h"
#include "nsICheckButton.h"


/**
 * Native Macintosh button wrapper
 */

class nsCheckButton : public nsWindow
{

public:
  nsCheckButton(nsISupports *aOuter);
  virtual ~nsCheckButton();

  NS_IMETHOD QueryObject(const nsIID& aIID, void** aInstancePtr);

  void Create(nsIWidget *aParent,
              const nsRect &aRect,
              EVENT_CALLBACK aHandleEventFunction,
              nsIDeviceContext *aContext = nsnull,
              nsIAppShell *aAppShell = nsnull,
              nsIToolkit *aToolkit = nsnull,
              nsWidgetInitData *aInitData = nsnull);
  void Create(nsNativeWidget aParent,
              const nsRect &aRect,
              EVENT_CALLBACK aHandleEventFunction,
              nsIDeviceContext *aContext = nsnull,
              nsIAppShell *aAppShell = nsnull,
              nsIToolkit *aToolkit = nsnull,
              nsWidgetInitData *aInitData = nsnull);


    // nsIRadioButton part
  virtual void   SetLabel(const nsString& aText);
  virtual void   GetLabel(nsString& aBuffer);
  virtual PRBool OnPaint(nsPaintEvent & aEvent);
  virtual PRBool OnResize(nsSizeEvent &aEvent);
  virtual PRBool DispatchMouseEvent(nsMouseEvent &aEvent);

  virtual void            SetState(PRBool aState);
  virtual PRBool          GetState();
  
  // Mac specific methods
  void LocalToWindowCoordinate(nsPoint& aPoint);
  void LocalToWindowCoordinate(nsRect& aRect);	
  
  // Overriden from nsWindow
  virtual PRBool PtInWindow(PRInt32 aX,PRInt32 aY);
  

private:

	void StringToStr255(const nsString& aText, Str255& aStr255);
	void DrawWidget(PRBool	aMouseInside);

  // this should not be public
  static PRInt32 GetOuterOffset() {
    return offsetof(nsCheckButton,mAggWidget);
  }


  // Aggregator class and instance variable used to aggregate in the
  // nsIRadioButton interface to nsCheckButton w/o using multiple
  // inheritance.
  class AggCheckButton : public nsICheckButton {
  public:
    AggCheckButton();
    virtual ~AggCheckButton();

    AGGREGATE_METHOD_DEF

    // nsICheckButton
    virtual void   SetLabel(const nsString &aText);
    virtual void   GetLabel(nsString &aBuffer);
    virtual void   SetState(PRBool aState);
    virtual PRBool GetState();

  };

  AggCheckButton mAggWidget;
  friend class AggCheckButton;
  
  nsString			mLabel;
  PRBool				mMouseDownInButton;
  PRBool				mWidgetArmed;
  PRBool				mButtonSet;


};

#endif // nsCheckButton_h__
