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

#ifndef nsDialog_h__
#define nsDialog_h__

//#include "nsdefs.h"
#include "nsWindow.h"
#include "nsIDialog.h"

/**
 * Native Motif Dialog wrapper
 */

class nsDialog :  public nsWindow
{

public:
  nsDialog(nsISupports *aOuter);
  virtual ~nsDialog();

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


    // nsIDialog part
  virtual void   SetLabel(const nsString& aText);
  virtual void   GetLabel(nsString& aBuffer);
  virtual PRBool OnPaint(nsPaintEvent & aEvent);
  virtual PRBool OnResize(nsSizeEvent &aEvent);

  //virtual void PreCreateWidget(nsWidgetInitData *aInitData);

private:

  // this should not be public
  static PRInt32 GetOuterOffset() {
    return offsetof(nsDialog,mAggWidget);
  }


  // Aggregator class and instance variable used to aggregate in the
  // nsIDialog interface to nsDialog w/o using multiple
  // inheritance.
  class AggDialog : public nsIDialog {
  public:
    AggDialog();
    virtual ~AggDialog();

    AGGREGATE_METHOD_DEF

    // nsIDialog
    virtual void SetLabel(const nsString &aText);
    virtual void GetLabel(nsString &aBuffer);

  };
  AggDialog mAggWidget;
  friend class AggDialog;


};

#endif // nsDialog_h__
