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

#ifndef nsStreamLoader_h__
#define nsStreamLoader_h__

#include "nsIStreamLoader.h"
#include "nsIStreamListener.h"
#include "nsCOMPtr.h"
#include "nsString.h"

class nsStreamLoader : public nsIStreamLoader,
                       public nsIStreamListener
{
public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSISTREAMLOADER
  NS_DECL_NSISTREAMOBSERVER
  NS_DECL_NSISTREAMLISTENER

  nsStreamLoader() { NS_INIT_REFCNT();} ;
  virtual ~nsStreamLoader() {};

  static NS_METHOD
  Create(nsISupports *aOuter, REFNSIID aIID, void **aResult);

protected:
  nsCOMPtr<nsIStreamLoaderObserver> mObserver;
  nsCOMPtr<nsISupports>             mContext;  // the observer's context
  nsCString                         mData;
  nsCOMPtr<nsIRequest>              mRequest;
///  nsCOMPtr<nsILoadGroup> mLoadGroup;
};

#endif // nsStreamLoader_h__
