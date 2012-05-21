/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsLoadGroup_h__
#define nsLoadGroup_h__

#include "nsILoadGroup.h"
#include "nsIChannel.h"
#include "nsIStreamListener.h"
#include "nsAgg.h"
#include "nsCOMPtr.h"
#include "nsWeakPtr.h"
#include "nsWeakReference.h"
#include "nsIInterfaceRequestor.h"
#include "nsIInterfaceRequestorUtils.h"
#include "nsISupportsPriority.h"
#include "nsITimedChannel.h"
#include "pldhash.h"
#include "mozilla/TimeStamp.h"

class  nsISupportsArray;

class nsLoadGroup : public nsILoadGroup,
                    public nsISupportsPriority,
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
    // nsISupportsPriority methods:
    NS_DECL_NSISUPPORTSPRIORITY

    ////////////////////////////////////////////////////////////////////////////
    // nsLoadGroup methods:

    nsLoadGroup(nsISupports* outer);

    nsresult Init();

protected:
    virtual ~nsLoadGroup();

    nsresult MergeLoadFlags(nsIRequest *aRequest, nsLoadFlags& flags);

private:
    void TelemetryReport();
    void TelemetryReportChannel(nsITimedChannel *timedChannel,
                                bool defaultRequest);

protected:
    PRUint32                        mForegroundCount;
    PRUint32                        mLoadFlags;

    nsCOMPtr<nsILoadGroup>          mLoadGroup; // load groups can contain load groups
    nsCOMPtr<nsIInterfaceRequestor> mCallbacks;

    nsCOMPtr<nsIRequest>            mDefaultLoadRequest;
    PLDHashTable                    mRequests;

    nsWeakPtr                       mObserver;
    
    nsresult                        mStatus;
    PRInt32                         mPriority;
    bool                            mIsCanceling;

    /* Telemetry */
    mozilla::TimeStamp              mDefaultRequestCreationTime;
    bool                            mDefaultLoadIsTimed;
    PRUint32                        mTimedRequests;
    PRUint32                        mCachedRequests;
};

#endif // nsLoadGroup_h__
