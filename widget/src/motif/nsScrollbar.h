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

#ifndef nsScrollbar_h__
#define nsScrollbar_h__

#include "nsWindow.h"

#include "nsIScrollbar.h"

/**
 * Native Motif scrollbar wrapper. 
 */

class nsScrollbar : public nsWindow
{

public:
  nsScrollbar(nsISupports *aOuter, PRBool aIsVertical);
  virtual ~nsScrollbar();

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


  // nsIScrollbar part
  virtual void      SetMaxRange(PRUint32 aEndRange);
  virtual PRUint32  GetMaxRange();
  virtual void      SetPosition(PRUint32 aPos);
  virtual PRUint32  GetPosition();
  virtual void      SetThumbSize(PRUint32 aSize);
  virtual PRUint32  GetThumbSize();
  virtual void      SetLineIncrement(PRUint32 aSize);
  virtual PRUint32  GetLineIncrement();
  virtual void      SetParameters(PRUint32 aMaxRange, PRUint32 aThumbSize,
                                  PRUint32 aPosition, PRUint32 aLineIncrement);

    virtual PRBool    OnPaint(nsPaintEvent & aEvent);
    virtual PRBool    OnScroll(PRUint32 scrollCode, PRUint32 cPos);
    virtual PRBool    OnResize(nsRect &aWindowRect);

private:
  // this should not be public
  static PRInt32 GetOuterOffset() {
    return offsetof(nsScrollbar,mAggWidget);
  }

  // Aggregator class and instance variable used to aggregate in the
  // nsIButton interface to nsButton w/o using multiple
  // inheritance.
  class AggScrollbar : public nsIScrollbar {
  public:
    AggScrollbar();
    virtual ~AggScrollbar();

    AGGRRGATE_METHOD_DEF

    // nsIScrollbar part
    virtual void      SetMaxRange(PRUint32 aEndRange);
    virtual PRUint32  GetMaxRange();
    virtual void      SetPosition(PRUint32 aPos);
    virtual PRUint32  GetPosition();
    virtual void      SetThumbSize(PRUint32 aSize);
    virtual PRUint32  GetThumbSize();
    virtual void      SetLineIncrement(PRUint32 aSize);
    virtual PRUint32  GetLineIncrement();
    virtual void      SetParameters(PRUint32 aMaxRange, PRUint32 aThumbSize,
                                  PRUint32 aPosition, PRUint32 aLineIncrement);
  };
  AggScrollbar mAggWidget;



};

#endif // nsScrollbar_
