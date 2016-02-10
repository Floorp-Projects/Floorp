/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsLoadGroup_h__
#define nsLoadGroup_h__

#include "nsILoadGroup.h"
#include "nsILoadGroupChild.h"
#include "nsPILoadGroupInternal.h"
#include "nsAgg.h"
#include "nsCOMPtr.h"
#include "nsWeakPtr.h"
#include "nsWeakReference.h"
#include "nsISupportsPriority.h"
#include "PLDHashTable.h"
#include "mozilla/TimeStamp.h"

class nsISchedulingContext;
class nsISchedulingContextService;
class nsITimedChannel;

class nsLoadGroup : public nsILoadGroup,
                    public nsILoadGroupChild,
                    public nsISupportsPriority,
                    public nsSupportsWeakReference,
                    public nsPILoadGroupInternal
{
public:
    NS_DECL_AGGREGATED
    
    ////////////////////////////////////////////////////////////////////////////
    // nsIRequest methods:
    NS_DECL_NSIREQUEST

    ////////////////////////////////////////////////////////////////////////////
    // nsILoadGroup methods:
    NS_DECL_NSILOADGROUP
    NS_DECL_NSPILOADGROUPINTERNAL

    ////////////////////////////////////////////////////////////////////////////
    // nsILoadGroupChild methods:
    NS_DECL_NSILOADGROUPCHILD

    ////////////////////////////////////////////////////////////////////////////
    // nsISupportsPriority methods:
    NS_DECL_NSISUPPORTSPRIORITY

    ////////////////////////////////////////////////////////////////////////////
    // nsLoadGroup methods:

    explicit nsLoadGroup(nsISupports* outer);
    virtual ~nsLoadGroup();

    nsresult Init();

protected:
    nsresult MergeLoadFlags(nsIRequest *aRequest, nsLoadFlags& flags);
    nsresult MergeDefaultLoadFlags(nsIRequest *aRequest, nsLoadFlags& flags);

private:
    void TelemetryReport();
    void TelemetryReportChannel(nsITimedChannel *timedChannel,
                                bool defaultRequest);

protected:
    uint32_t                        mForegroundCount;
    uint32_t                        mLoadFlags;
    uint32_t                        mDefaultLoadFlags;

    nsCOMPtr<nsILoadGroup>          mLoadGroup; // load groups can contain load groups
    nsCOMPtr<nsIInterfaceRequestor> mCallbacks;
    nsCOMPtr<nsISchedulingContext>  mSchedulingContext;
    nsCOMPtr<nsISchedulingContextService> mSchedulingContextService;

    nsCOMPtr<nsIRequest>            mDefaultLoadRequest;
    PLDHashTable                    mRequests;

    nsWeakPtr                       mObserver;
    nsWeakPtr                       mParentLoadGroup;
    
    nsresult                        mStatus;
    int32_t                         mPriority;
    bool                            mIsCanceling;

    /* Telemetry */
    mozilla::TimeStamp              mDefaultRequestCreationTime;
    bool                            mDefaultLoadIsTimed;
    uint32_t                        mTimedRequests;
    uint32_t                        mCachedRequests;

    /* For nsPILoadGroupInternal */
    uint32_t                        mTimedNonCachedRequestsUntilOnEndPageLoad;
};

#endif // nsLoadGroup_h__
