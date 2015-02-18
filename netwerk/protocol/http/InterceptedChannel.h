/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set expandtab ts=2 sw=2 sts=2 cin: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef InterceptedChannel_h
#define InterceptedChannel_h

#include "nsINetworkInterceptController.h"
#include "nsRefPtr.h"
#include "mozilla/Maybe.h"

class nsICacheEntry;
class nsInputStreamPump;
class nsIStorageStream;
class nsIStreamListener;

namespace mozilla {
namespace net {

class nsHttpChannel;
class HttpChannelChild;
class nsHttpResponseHead;

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

  // Whether this intercepted channel was performing a navigation.
  bool mIsNavigation;

  void EnsureSynthesizedResponse();
  void DoNotifyController();
  nsresult DoSynthesizeHeader(const nsACString& aName, const nsACString& aValue);

  virtual ~InterceptedChannelBase();
public:
  InterceptedChannelBase(nsINetworkInterceptController* aController,
                         bool aIsNavigation);

  // Notify the interception controller that the channel has been intercepted
  // and prepare the response body output stream.
  virtual void NotifyController() = 0;

  NS_DECL_ISUPPORTS

  NS_IMETHOD GetResponseBody(nsIOutputStream** aOutput) MOZ_OVERRIDE;
  NS_IMETHOD GetIsNavigation(bool* aIsNavigation) MOZ_OVERRIDE;
};

class InterceptedChannelChrome : public InterceptedChannelBase
{
  // The actual channel being intercepted.
  nsRefPtr<nsHttpChannel> mChannel;

  // Writeable cache entry for use when synthesizing a response in a parent process
  nsCOMPtr<nsICacheEntry> mSynthesizedCacheEntry;
public:
  InterceptedChannelChrome(nsHttpChannel* aChannel,
                           nsINetworkInterceptController* aController,
                           nsICacheEntry* aEntry);

  NS_IMETHOD ResetInterception() MOZ_OVERRIDE;
  NS_IMETHOD FinishSynthesizedResponse() MOZ_OVERRIDE;
  NS_IMETHOD GetChannel(nsIChannel** aChannel) MOZ_OVERRIDE;
  NS_IMETHOD SynthesizeHeader(const nsACString& aName, const nsACString& aValue) MOZ_OVERRIDE;
  NS_IMETHOD Cancel() MOZ_OVERRIDE;

  virtual void NotifyController() MOZ_OVERRIDE;
};

class InterceptedChannelContent : public InterceptedChannelBase
{
  // The actual channel being intercepted.
  nsRefPtr<HttpChannelChild> mChannel;

  // Reader-side of the response body when synthesizing in a child proces
  nsCOMPtr<nsIInputStream> mSynthesizedInput;

  // Pump to read the synthesized body in child processes
  nsRefPtr<nsInputStreamPump> mStoragePump;

  // Listener for the synthesized response to fix up the notifications before they reach
  // the actual channel.
  nsCOMPtr<nsIStreamListener> mStreamListener;
public:
  InterceptedChannelContent(HttpChannelChild* aChannel,
                            nsINetworkInterceptController* aController,
                            nsIStreamListener* aListener);

  NS_IMETHOD ResetInterception() MOZ_OVERRIDE;
  NS_IMETHOD FinishSynthesizedResponse() MOZ_OVERRIDE;
  NS_IMETHOD GetChannel(nsIChannel** aChannel) MOZ_OVERRIDE;
  NS_IMETHOD SynthesizeHeader(const nsACString& aName, const nsACString& aValue) MOZ_OVERRIDE;
  NS_IMETHOD Cancel() MOZ_OVERRIDE;

  virtual void NotifyController() MOZ_OVERRIDE;
};

} // namespace net
} // namespace mozilla

#endif // InterceptedChannel_h
