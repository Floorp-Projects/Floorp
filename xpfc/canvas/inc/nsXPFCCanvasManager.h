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
#include "nsIVector.h"
#include "nsIIterator.h"

#include "nsIXPFCCanvasManager.h"

class nsXPFCCanvasManager : public nsIXPFCCanvasManager
{
public:
  nsXPFCCanvasManager();

  NS_DECL_ISUPPORTS

  NS_IMETHOD Init() ;
  NS_IMETHOD_(nsIXPFCCanvas *) CanvasFromWidget(nsIWidget * aWidget);
  NS_IMETHOD GetRootCanvas(nsIXPFCCanvas ** aCanvas);
  NS_IMETHOD SetRootCanvas(nsIXPFCCanvas * aCanvas);
  NS_IMETHOD Register(nsIXPFCCanvas * aCanvas, nsIWidget * aWidget);
  NS_IMETHOD Unregister(nsIXPFCCanvas * aCanvas);
  NS_IMETHOD_(nsIXPFCCanvas *) GetFocusedCanvas();
  NS_IMETHOD SetFocusedCanvas(nsIXPFCCanvas * aCanvas);

protected:
  ~nsXPFCCanvasManager();

public:
  PRMonitor * monitor;

public:
  nsIVector * mList ;

private:
  nsIXPFCCanvas * mRootCanvas;
  nsIXPFCCanvas * mFocusedCanvas;

};

#endif /* nsXPFCCanvasManager_h___ */
