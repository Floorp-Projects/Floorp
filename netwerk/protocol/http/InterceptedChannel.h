/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set expandtab ts=2 sw=2 sts=2 cin: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef InterceptedChannel_h
#define InterceptedChannel_h

#include "nsINetworkInterceptController.h"
#include "mozilla/RefPtr.h"
#include "mozilla/Maybe.h"

class nsICacheEntry;
class nsInputStreamPump;
class nsIStreamListener;

namespace mozilla {
namespace net {

class nsHttpChannel;
class HttpChannelChild;
class nsHttpResponseHead;
class InterceptStreamListener;

// An object representing a channel that has been intercepted. This avoids
// complicating the actual channel implementation with the details of
// synthesizing responses.
class InterceptedChannelBase : public nsIInterceptedChannel {
 protected:
  // The interception controller to notify about the successful channel
  // interception
  nsCOMPtr<nsINetworkInterceptController> mController;

  // Response head for use when synthesizing
  Maybe<UniquePtr<nsHttpResponseHead>> mSynthesizedResponseHead;

  nsCOMPtr<nsIConsoleReportCollector> mReportCollector;
  nsCOMPtr<nsISupports> mReleaseHandle;

  bool mClosed;

  void EnsureSynthesizedResponse();
  void DoNotifyController();
  void DoSynthesizeStatus(uint16_t aStatus, const nsACString& aReason);
  [[nodiscard]] nsresult DoSynthesizeHeader(const nsACString& aName,
                                            const nsACString& aValue);

  TimeStamp mLaunchServiceWorkerStart;
  TimeStamp mLaunchServiceWorkerEnd;
  TimeStamp mDispatchFetchEventStart;
  TimeStamp mDispatchFetchEventEnd;
  TimeStamp mHandleFetchEventStart;
  TimeStamp mHandleFetchEventEnd;

  TimeStamp mFinishResponseStart;
  TimeStamp mFinishResponseEnd;
  enum { Invalid = 0, Synthesized, Reset } mSynthesizedOrReset;

  virtual ~InterceptedChannelBase() = default;

 public:
  explicit InterceptedChannelBase(nsINetworkInterceptController* aController);

  // Notify the interception controller that the channel has been intercepted
  // and prepare the response body output stream.
  virtual void NotifyController() = 0;

  NS_DECL_ISUPPORTS

  NS_IMETHOD GetConsoleReportCollector(
      nsIConsoleReportCollector** aCollectorOut) override;
  NS_IMETHOD SetReleaseHandle(nsISupports* aHandle) override;

  NS_IMETHODIMP
  SetLaunchServiceWorkerStart(TimeStamp aTimeStamp) override {
    mLaunchServiceWorkerStart = aTimeStamp;
    return NS_OK;
  }

  NS_IMETHODIMP
  GetLaunchServiceWorkerStart(TimeStamp* aTimeStamp) override {
    MOZ_DIAGNOSTIC_ASSERT(aTimeStamp);
    *aTimeStamp = mLaunchServiceWorkerStart;
    return NS_OK;
  }

  NS_IMETHODIMP
  SetLaunchServiceWorkerEnd(TimeStamp aTimeStamp) override {
    mLaunchServiceWorkerEnd = aTimeStamp;
    return NS_OK;
  }

  NS_IMETHODIMP
  GetLaunchServiceWorkerEnd(TimeStamp* aTimeStamp) override {
    MOZ_DIAGNOSTIC_ASSERT(aTimeStamp);
    *aTimeStamp = mLaunchServiceWorkerEnd;
    return NS_OK;
  }

  NS_IMETHODIMP
  SetDispatchFetchEventStart(TimeStamp aTimeStamp) override {
    mDispatchFetchEventStart = aTimeStamp;
    return NS_OK;
  }

  NS_IMETHODIMP
  SetDispatchFetchEventEnd(TimeStamp aTimeStamp) override {
    mDispatchFetchEventEnd = aTimeStamp;
    return NS_OK;
  }

  NS_IMETHODIMP
  SetHandleFetchEventStart(TimeStamp aTimeStamp) override {
    mHandleFetchEventStart = aTimeStamp;
    return NS_OK;
  }

  NS_IMETHODIMP
  SetHandleFetchEventEnd(TimeStamp aTimeStamp) override {
    mHandleFetchEventEnd = aTimeStamp;
    return NS_OK;
  }

  NS_IMETHODIMP
  SetFinishResponseStart(TimeStamp aTimeStamp) override {
    mFinishResponseStart = aTimeStamp;
    return NS_OK;
  }

  NS_IMETHODIMP
  SetFinishSynthesizedResponseEnd(TimeStamp aTimeStamp) override {
    MOZ_ASSERT(mSynthesizedOrReset == Invalid);
    mSynthesizedOrReset = Synthesized;
    mFinishResponseEnd = aTimeStamp;
    return NS_OK;
  }

  NS_IMETHODIMP
  SetChannelResetEnd(TimeStamp aTimeStamp) override {
    MOZ_ASSERT(mSynthesizedOrReset == Invalid);
    mSynthesizedOrReset = Reset;
    mFinishResponseEnd = aTimeStamp;
    return NS_OK;
  }

  NS_IMETHODIMP SaveTimeStamps() override;

  static already_AddRefed<nsIURI> SecureUpgradeChannelURI(nsIChannel* aChannel);
};

}  // namespace net
}  // namespace mozilla

#endif  // InterceptedChannel_h
