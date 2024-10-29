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

  /**
   * Flags inherited from the default request in the load group onto other loads
   * added to the load group.
   *
   * NOTE(emilio): If modifying these, be aware that we allow these flags to be
   * effectively set from the content process on a document navigation, and
   * thus nothing security-critical should be allowed here.
   */
  static constexpr nsLoadFlags kInheritedLoadFlags =
      LOAD_BACKGROUND | LOAD_BYPASS_CACHE | LOAD_FROM_CACHE | VALIDATE_ALWAYS |
      VALIDATE_ONCE_PER_SESSION | VALIDATE_NEVER;

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
  uint32_t mForegroundCount{0};
  uint32_t mLoadFlags{LOAD_NORMAL};
  uint32_t mDefaultLoadFlags{0};
  int32_t mPriority{PRIORITY_NORMAL};

  nsCOMPtr<nsILoadGroup> mLoadGroup;  // load groups can contain load groups
  nsCOMPtr<nsIInterfaceRequestor> mCallbacks;
  nsCOMPtr<nsIRequestContext> mRequestContext;
  nsCOMPtr<nsIRequestContextService> mRequestContextService;

  nsCOMPtr<nsIRequest> mDefaultLoadRequest;
  PLDHashTable mRequests;

  nsWeakPtr mObserver;
  nsWeakPtr mParentLoadGroup;

  nsresult mStatus{NS_OK};
  bool mIsCanceling{false};
  bool mDefaultLoadIsTimed{false};
  bool mBrowsingContextDiscarded{false};
  bool mExternalRequestContext{false};
  bool mNotifyObserverAboutBackgroundRequests{false};

  /* Telemetry */
  mozilla::TimeStamp mDefaultRequestCreationTime;
  uint32_t mTimedRequests{0};
  uint32_t mCachedRequests{0};
};

}  // namespace net
}  // namespace mozilla

#endif  // nsLoadGroup_h__
