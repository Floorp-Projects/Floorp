/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 4 -*-
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

// data implementation header

#ifndef nsDataChannel_h___
#define nsDataChannel_h___

#include "nsIDataChannel.h"
#include "nsIURI.h"
#include "nsString.h"
#include "nsIEventQueue.h"
#include "nsILoadGroup.h"
#include "nsIStreamListener.h"
#include "nsIInputStream.h"
#include "nsIInterfaceRequestor.h"
#include "nsCOMPtr.h"

class nsDataChannel : public nsIDataChannel {
public:
    NS_DECL_ISUPPORTS
    NS_DECL_NSIREQUEST
    NS_DECL_NSICHANNEL
    NS_DECL_NSIDATACHANNEL

    // nsFTPChannel methods:
    nsDataChannel();
    virtual ~nsDataChannel();

    // Define a Create method to be used with a factory:
    static NS_METHOD
    Create(nsISupports* aOuter, const nsIID& aIID, void* *aResult);
    
    nsresult Init(nsIURI* uri);
    nsresult ParseData();

protected:
    nsCOMPtr<nsIInterfaceRequestor>     mCallbacks;
    nsCOMPtr<nsIURI>                    mOriginalURI;
    nsCOMPtr<nsIURI>                    mUrl;
    nsCOMPtr<nsIInputStream>            mDataStream;
    PRUint32                            mLoadAttributes;
    nsCOMPtr<nsILoadGroup>              mLoadGroup;
    nsCString                           mContentType;
    PRInt32                             mContentLength;
    nsCOMPtr<nsISupports>               mOwner; 
};

#endif /* nsFTPChannel_h___ */
