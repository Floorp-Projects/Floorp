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

#ifndef nsLoadGroup_h__
#define nsLoadGroup_h__

#include "nsILoadGroup.h"
#include "nsIChannel.h"
#include "nsIStreamListener.h"
#include "nsAgg.h"
#include "nsCOMPtr.h"
#include "nsWeakPtr.h"
#include "nsWeakReference.h"

class  nsISupportsArray;

class nsLoadGroup : public nsILoadGroup,
                    public nsSupportsWeakReference
{
public:
    NS_DECL_AGGREGATED
    
    ////////////////////////////////////////////////////////////////////////////
    // nsIRequest methods:
    NS_DECL_NSIREQUEST

    ////////////////////////////////////////////////////////////////////////////
    // nsILoadGroup methods:
    NS_DECL_NSILOADGROUP

    ////////////////////////////////////////////////////////////////////////////
    // nsLoadGroup methods:

    nsLoadGroup(nsISupports* outer);
    
    static NS_METHOD
    Create(nsISupports *aOuter, REFNSIID aIID, void **aResult);

protected:
    virtual ~nsLoadGroup();
    nsresult Init();

    nsresult MergeLoadAttributes(nsIRequest *aRequest, nsLoadFlags& flags);

protected:
    PRUint32                    mDefaultLoadAttributes;
    PRUint32                    mForegroundCount;

    nsCOMPtr<nsIRequest>        mDefaultLoadRequest;
    nsISupportsArray*           mRequests;

    nsWeakPtr                   mObserver;
    // nsCOMPtr<nsIStreamObserver> mObserver;
    
    nsWeakPtr                   mGroupListenerFactory;
    nsresult                    mStatus;
};

#endif // nsLoadGroup_h__
