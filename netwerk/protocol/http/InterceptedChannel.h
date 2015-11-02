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

// An object representing a channel that has been intercepted. This avoids complicating
// the actual channel implementation with the details of synthesizing responses.
class InterceptedChannelBase : public nsIInterceptedChannel {
protected:
  // The interception controller to notify about the successful channel interception
  nsCOMPtr<nsINetworkInterceptController> mController;

  // The stream to write the body of the synthesized response
  nsCOMPtr<nsIOutputStream> mResponseBody;

  // Response head for use when synthesizing
  Maybe<nsAutoPtr<nsHttpResponseHead>> mSynthesizedResponseHead;

  nsCOMPtr<nsIConsoleReportCollector> mReportCollector;
  nsCOMPtr<nsISupports> mReleaseHandle;

  void EnsureSynthesizedResponse();
  void DoNotifyController();
  nsresult DoSynthesizeStatus(uint16_t aStatus, const nsACString& aReason);
  nsresult DoSynthesizeHeader(const nsACString& aName, const nsACString& aValue);

  virtual ~InterceptedChannelBase();
public:
  explicit InterceptedChannelBase(nsINetworkInterceptController* aController);

  // Notify the interception controller that the channel has been intercepted
  // and prepare the response body output stream.
  virtual void NotifyController() = 0;

  NS_DECL_ISUPPORTS

  NS_IMETHOD GetResponseBody(nsIOutputStream** aOutput) override;
  NS_IMETHOD GetConsoleReportCollector(nsIConsoleReportCollector** aCollectorOut) override;
  NS_IMETHOD SetReleaseHandle(nsISupports* aHandle) override;

  static already_AddRefed<nsIURI>
  SecureUpgradeChannelURI(nsIChannel* aChannel);
};

class InterceptedChannelChrome : public InterceptedChannelBase
{
  // The actual channel being intercepted.
  RefPtr<nsHttpChannel> mChannel;

  // Writeable cache entry for use when synthesizing a response in a parent process
  nsCOMPtr<nsICacheEntry> mSynthesizedCacheEntry;

  // When a channel is intercepted, content decoding is disabled since the
  // ServiceWorker will have already extracted the decoded data. For parent
  // process channels we need to preserve the earlier value in case
  // ResetInterception is called.
  bool mOldApplyConversion;
public:
  InterceptedChannelChrome(nsHttpChannel* aChannel,
                           nsINetworkInterceptController* aController,
                           nsICacheEntry* aEntry);

  NS_IMETHOD ResetInterception() override;
  NS_IMETHOD FinishSynthesizedResponse(const nsACString& aFinalURLSpec) override;
  NS_IMETHOD GetChannel(nsIChannel** aChannel) override;
  NS_IMETHOD GetSecureUpgradedChannelURI(nsIURI** aURI) override;
  NS_IMETHOD SynthesizeStatus(uint16_t aStatus, const nsACString& aReason) override;
  NS_IMETHOD SynthesizeHeader(const nsACString& aName, const nsACString& aValue) override;
  NS_IMETHOD Cancel(nsresult aStatus) override;
  NS_IMETHOD SetChannelInfo(mozilla::dom::ChannelInfo* aChannelInfo) override;
  NS_IMETHOD GetInternalContentPolicyType(nsContentPolicyType *aInternalContentPolicyType) override;

  virtual void NotifyController() override;
};

class InterceptedChannelContent : public InterceptedChannelBase
{
  // The actual channel being intercepted.
  RefPtr<HttpChannelChild> mChannel;

  // Reader-side of the response body when synthesizing in a child proces
  nsCOMPtr<nsIInputStream> mSynthesizedInput;

  // Listener for the synthesized response to fix up the notifications before they reach
  // the actual channel.
  RefPtr<InterceptStreamListener> mStreamListener;
public:
  InterceptedChannelContent(HttpChannelChild* aChannel,
                            nsINetworkInterceptController* aController,
                            InterceptStreamListener* aListener);

  NS_IMETHOD ResetInterception() override;
  NS_IMETHOD FinishSynthesizedResponse(const nsACString& aFinalURLSpec) override;
  NS_IMETHOD GetChannel(nsIChannel** aChannel) override;
  NS_IMETHOD GetSecureUpgradedChannelURI(nsIURI** aURI) override;
  NS_IMETHOD SynthesizeStatus(uint16_t aStatus, const nsACString& aReason) override;
  NS_IMETHOD SynthesizeHeader(const nsACString& aName, const nsACString& aValue) override;
  NS_IMETHOD Cancel(nsresult aStatus) override;
  NS_IMETHOD SetChannelInfo(mozilla::dom::ChannelInfo* aChannelInfo) override;
  NS_IMETHOD GetInternalContentPolicyType(nsContentPolicyType *aInternalContentPolicyType) override;

  virtual void NotifyController() override;
};

} // namespace net
} // namespace mozilla

#endif // InterceptedChannel_h
