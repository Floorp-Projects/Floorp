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

  static already_AddRefed<nsIURI> SecureUpgradeChannelURI(nsIChannel* aChannel);
};

}  // namespace net
}  // namespace mozilla

#endif  // InterceptedChannel_h
