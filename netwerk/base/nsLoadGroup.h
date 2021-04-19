/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsLoadGroup_h__
#define nsLoadGroup_h__

#include "nsILoadGroup.h"
#include "nsILoadGroupChild.h"
#include "nsIObserver.h"
#include "nsCOMPtr.h"
#include "nsWeakReference.h"
#include "nsISupportsPriority.h"
#include "PLDHashTable.h"
#include "mozilla/TimeStamp.h"

class nsIRequestContext;
class nsIRequestContextService;
class nsITimedChannel;

namespace mozilla {
namespace net {

class nsLoadGroup : public nsILoadGroup,
                    public nsILoadGroupChild,
                    public nsIObserver,
                    public nsISupportsPriority,
                    public nsSupportsWeakReference {
 public:
  NS_DECL_ISUPPORTS

  ////////////////////////////////////////////////////////////////////////////
  // nsIRequest methods:
  NS_DECL_NSIREQUEST

  ////////////////////////////////////////////////////////////////////////////
  // nsILoadGroup methods:
  NS_DECL_NSILOADGROUP

  ////////////////////////////////////////////////////////////////////////////
  // nsILoadGroupChild methods:
  NS_DECL_NSILOADGROUPCHILD

  ////////////////////////////////////////////////////////////////////////////
  // nsISupportsPriority methods:
  NS_DECL_NSISUPPORTSPRIORITY

  ////////////////////////////////////////////////////////////////////////////
  // nsIObserver methods:
  NS_DECL_NSIOBSERVER

  ////////////////////////////////////////////////////////////////////////////
  // nsLoadGroup methods:

  nsLoadGroup();

  nsresult Init();
  nsresult InitWithRequestContextId(const uint64_t& aRequestContextId);

  void SetGroupObserver(nsIRequestObserver* aObserver,
                        bool aIncludeBackgroundRequests);

 protected:
  virtual ~nsLoadGroup();

  nsresult MergeLoadFlags(nsIRequest* aRequest, nsLoadFlags& flags);
  nsresult MergeDefaultLoadFlags(nsIRequest* aRequest, nsLoadFlags& flags);

 private:
  void TelemetryReport();
  void TelemetryReportChannel(nsITimedChannel* timedChannel,
                              bool defaultRequest);

  nsresult RemoveRequestFromHashtable(nsIRequest* aRequest, nsresult aStatus);
  nsresult NotifyRemovalObservers(nsIRequest* aRequest, nsresult aStatus);

 protected:
  uint32_t mForegroundCount;
  uint32_t mLoadFlags;
  uint32_t mDefaultLoadFlags;
  int32_t mPriority;

  nsCOMPtr<nsILoadGroup> mLoadGroup;  // load groups can contain load groups
  nsCOMPtr<nsIInterfaceRequestor> mCallbacks;
  nsCOMPtr<nsIRequestContext> mRequestContext;
  nsCOMPtr<nsIRequestContextService> mRequestContextService;

  nsCOMPtr<nsIRequest> mDefaultLoadRequest;
  PLDHashTable mRequests;

  nsWeakPtr mObserver;
  nsWeakPtr mParentLoadGroup;

  nsresult mStatus;
  bool mIsCanceling;
  bool mDefaultLoadIsTimed;
  bool mBrowsingContextDiscarded;
  bool mExternalRequestContext;
  bool mNotifyObserverAboutBackgroundRequests;

  /* Telemetry */
  mozilla::TimeStamp mDefaultRequestCreationTime;
  uint32_t mTimedRequests;
  uint32_t mCachedRequests;
};

}  // namespace net
}  // namespace mozilla

#endif  // nsLoadGroup_h__
