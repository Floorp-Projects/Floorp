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
 *   Chak Nanga <chak@netscape.com>
 */

#ifndef nsViewSourceChannel_h___
#define nsViewSourceChannel_h___

#include "nsString.h"
#include "nsCOMPtr.h"
#include "nsXPIDLString.h"
#include "nsIViewSourceChannel.h"
#include "nsIURI.h"
#include "nsIStreamListener.h"
#include "nsViewSourceHandler.h"
#include "nsNetCID.h"
#include "nsIHttpChannel.h"
#include "nsICachingChannel.h"

class nsViewSourceChannel : public nsIViewSourceChannel,
                            public nsIStreamListener,
                            public nsIHttpChannel,
                            public nsICachingChannel {

public:
    NS_DECL_ISUPPORTS
    NS_DECL_NSIREQUEST
    NS_DECL_NSICHANNEL
    NS_DECL_NSIVIEWSOURCECHANNEL
    NS_DECL_NSISTREAMLISTENER
    NS_DECL_NSIREQUESTOBSERVER
    NS_FORWARD_SAFE_NSIHTTPCHANNEL(mHttpChannel)
    NS_FORWARD_SAFE_NSICACHINGCHANNEL(mCachingChannel)

    // nsViewSourceChannel methods:
    nsViewSourceChannel();
    virtual ~nsViewSourceChannel();

    // Define a Create method to be used with a factory:
    static NS_METHOD 
        Create(nsISupports* aOuter, const nsIID& aIID, void* *aResult);
    
    nsresult Init(nsIURI* uri);

protected:
    nsCOMPtr<nsIChannel>        mChannel;
    nsCOMPtr<nsIHttpChannel>    mHttpChannel;
    nsCOMPtr<nsICachingChannel> mCachingChannel;
    nsCOMPtr<nsIStreamListener> mListener;
    nsCString                   mContentType;
};

#endif /* nsViewSourceChannel_h___ */
