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

#include "nscore.h"
#include "nsIFactory.h"
#include "nsIArray.h"
#include "nsIIterator.h"
#include "nsIStreamManager.h"

class nsStreamManager : public nsIStreamManager 
{
public:

  /**
   * Constructor and Destructor
   */

  nsStreamManager();
  ~nsStreamManager();

  /**
   * ISupports Interface
   */
  NS_DECL_ISUPPORTS

  /**
   * Initialize Method
   * @result The result of the initialization, NS_OK if no errors
   */
  NS_IMETHOD Init();

#if 0
  NS_IMETHOD LoadURL(nsIWebViewerContainer * aWebViewerContainer,
                     nsIXPFCCanvas * aParentCanvas,
                     const nsString& aURLSpec, 
                     nsIPostData * aPostData,
                     nsIArray * aCalendarVector,
                     nsIID *aDTDIID = nsnull,
                     nsIID *aSinkIID = nsnull);
#endif

  NS_IMETHOD LoadURL(nsIWebViewerContainer * aWebViewerContainer,
                     nsISupports * aContentSinkContainer,
                     const nsString& aURLSpec, 
                     nsIPostData * aPostData,
                     nsIID *aDTDIID = nsnull,
                     nsIID *aSinkIID = nsnull);

private:
  nsIArray * mStreamObjects;

};


