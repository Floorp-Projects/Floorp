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

#ifndef nsIStreamManager_h___
#define nsIStreamManager_h___

#include "nscore.h"
#include "nsxpfc.h"
#include "nsISupports.h"
#include "nsIWebViewerContainer.h"
#include "nsIURL.h"
#include "nsIStreamListener.h"
#include "nsIXPFCContentSinkContainer.h"

class nsIXPFCCanvas;

// 5f680140-360f-11d2-9248-00805f8a7ab6
#define NS_ISTREAM_MANAGER_IID      \
 { 0x5f680140, 0x360f, 0x11d2, \
   {0x92, 0x48, 0x00, 0x80, 0x5f, 0x8a, 0x7a, 0xb6} }

// Application Shell Interface
class nsIStreamManager : public nsISupports 
{
public:

  /**
   * Initialize the StreamManager
   * @result The result of the initialization, NS_OK if no errors
   */
  NS_IMETHOD Init() = 0;

#if 0
  NS_IMETHOD LoadURL(nsIWebViewerContainer * aWebViewerContainer,
                     nsIXPFCCanvas * aParentCanvas,
                     const nsString& aURLSpec, 
                     nsIPostData * aPostData,
                     nsIArray * aCalendarVector,
                     nsIID *aDTDIID = nsnull,
                     nsIID *aSinkIID = nsnull) = 0;
#endif

  /** 
   * aContainer is generic variable used to pass information to sink variable.
   * replaces aParentCanvas.
   */
  NS_IMETHOD LoadURL(nsIWebViewerContainer * aWebViewerContainer,
                     nsISupports * aContentSinkContainer,
                     const nsString& aURLSpec, 
                     nsIPostData * aPostData,
                     nsIID *aDTDIID = nsnull,
                     nsIID *aSinkIID = nsnull) = 0;


};

#endif /* nsIStreamManager_h___ */
