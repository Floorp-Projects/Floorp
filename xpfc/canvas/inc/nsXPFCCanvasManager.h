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

#ifndef nsXPFCCanvasManager_h___
#define nsXPFCCanvasManager_h___

#include "nscore.h"
#include "plstr.h"
#include "prtypes.h"
#include "prmon.h"
#include "plstr.h"
#include "nsCom.h"
#include "nsIArray.h"
#include "nsIIterator.h"
#include "nsIXPFCCanvasManager.h"
#include "nsIView.h"
#include "nsIViewObserver.h"
#include "nsIWebViewerContainer.h"
#include "nsIViewManager.h"

class nsXPFCCanvasManager : public nsIXPFCCanvasManager,
                            public nsIViewObserver
{
public:
  nsXPFCCanvasManager();

  NS_DECL_ISUPPORTS

  NS_IMETHOD Init() ;

  NS_IMETHOD_(nsIXPFCCanvas *) CanvasFromView(nsIView * aView);

  NS_IMETHOD_(nsIXPFCCanvas *) CanvasFromWidget(nsIWidget * aWidget);

  NS_IMETHOD GetRootCanvas(nsIXPFCCanvas ** aCanvas);

  NS_IMETHOD SetRootCanvas(nsIXPFCCanvas * aCanvas);

  NS_IMETHOD RegisterView(nsIXPFCCanvas * aCanvas, 
                          nsIView * aView);

  NS_IMETHOD RegisterWidget(nsIXPFCCanvas * aCanvas, 
                            nsIWidget * aWidget);

  NS_IMETHOD Unregister(nsIXPFCCanvas * aCanvas);

  NS_IMETHOD_(nsIXPFCCanvas *) GetFocusedCanvas();
  NS_IMETHOD SetFocusedCanvas(nsIXPFCCanvas * aCanvas);

  NS_IMETHOD_(nsIXPFCCanvas *) GetPressedCanvas();
  NS_IMETHOD SetPressedCanvas(nsIXPFCCanvas * aCanvas);

  NS_IMETHOD_(nsIXPFCCanvas *) GetMouseOverCanvas();
  NS_IMETHOD SetMouseOverCanvas(nsIXPFCCanvas * aCanvas);

  // nsIViewObserver Interfaces
  NS_IMETHOD Paint(nsIView *            aView,
                   nsIRenderingContext& aRenderingContext,
                   const nsRect&        aDirtyRect);
  NS_IMETHOD HandleEvent(nsIView *       aView,
                         nsGUIEvent*     aEvent,
                         nsEventStatus&  aEventStatus);
  NS_IMETHOD Scrolled(nsIView * aView);
  NS_IMETHOD ResizeReflow(nsIView * aView, nscoord aWidth, nscoord aHeight);


  NS_IMETHOD_(nsIWebViewerContainer *) GetWebViewerContainer() ;
  NS_IMETHOD SetWebViewerContainer(nsIWebViewerContainer * aWebViewerContainer) ;

  NS_IMETHOD_(nsIViewManager *) GetViewManager() ;
  NS_IMETHOD SetViewManager(nsIViewManager * aViewManager) ;

protected:
  ~nsXPFCCanvasManager();

public:
  PRMonitor * monitor;

public:
  nsIArray * mViewList ;
  nsIArray * mWidgetList ;

private:
  nsIXPFCCanvas * mRootCanvas;
  nsIXPFCCanvas * mFocusedCanvas;
  nsIXPFCCanvas * mMouseOverCanvas ;
  nsIXPFCCanvas * mPressedCanvas ;
  nsIWebViewerContainer * mWebViewerContainer;
  nsIViewManager * mViewManager;
};

#endif /* nsXPFCCanvasManager_h___ */
