/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 4 -*-
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

#ifndef nsLoadGroup_h__
#define nsLoadGroup_h__

#include "nsILoadGroup.h"
#include "nsIStreamListener.h"
#include "nsAgg.h"
#include "nsCOMPtr.h"

class nsISupportsArray;
class nsLoadGroupEntry;

class nsLoadGroup : public nsILoadGroup
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
    virtual ~nsLoadGroup();
    
    static NS_METHOD
    Create(nsISupports *aOuter, REFNSIID aIID, void **aResult);

    friend class nsLoadGroupEntry;

protected:
    typedef nsresult (*PropagateDownFun)(nsIRequest* request);
    nsresult PropagateDown(PropagateDownFun fun);

    nsresult SubGroupIsEmpty(nsresult aStatus);

protected:
    PRUint32                    mDefaultLoadAttributes;
    nsISupportsArray*           mChannels;
    nsISupportsArray*           mSubGroups;
    nsIStreamObserver*          mObserver;
    nsLoadGroup*                mParent;        // weak ref
    PRUint32                    mForegroundCount;
    PRBool                      mIsActive;

    nsCOMPtr<nsIChannel>        mDefaultLoadChannel;

    nsCOMPtr<nsILoadGroupListenerFactory> mGroupListenerFactory;
};

#endif // nsLoadGroup_h__
