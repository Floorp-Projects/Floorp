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
 *   Darin Fisher <darin@netscape.com> (original author)
 */

#ifndef nsStreamProviderProxy_h__
#define nsStreamProviderProxy_h__

#include "nsRequestObserverProxy.h"
#include "nsIStreamProvider.h"
#include "nsIInputStream.h"
#include "nsIOutputStream.h"

class nsStreamProviderProxy : public nsIStreamProviderProxy
{
public:
    NS_DECL_ISUPPORTS
    NS_DECL_NSIREQUESTOBSERVER
    NS_DECL_NSISTREAMPROVIDER
    NS_DECL_NSISTREAMPROVIDERPROXY

    nsStreamProviderProxy();
    virtual ~nsStreamProviderProxy();

    nsresult GetProvider(nsIStreamProvider **);

    void SetProviderStatus(nsresult status) { mProviderStatus = status; }

protected:
    nsRequestObserverProxy     *mObserverProxy;
    nsCOMPtr<nsIInputStream>    mPipeIn;
    nsCOMPtr<nsIOutputStream>   mPipeOut;
    nsresult                    mProviderStatus;
};

#endif // nsStreamProviderProxy_h__
