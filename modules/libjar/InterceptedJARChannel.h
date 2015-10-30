/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set expandtab ts=2 sw=2 sts=2 cin: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef InterceptedJARChannel_h
#define InterceptedJARChannel_h

#include "nsJAR.h"
#include "nsJARChannel.h"
#include "nsIInputStream.h"
#include "nsIInputStreamPump.h"
#include "nsINetworkInterceptController.h"
#include "nsIOutputStream.h"
#include "mozilla/RefPtr.h"

#include "mozilla/Maybe.h"

class nsIStreamListener;
class nsJARChannel;

namespace mozilla {
namespace net {

// An object representing a channel that has been intercepted. This avoids
// complicating the actual channel implementation with the details of
// synthesizing responses.
class InterceptedJARChannel : public nsIInterceptedChannel
{
  // The interception controller to notify about the successful channel
  // interception.
  nsCOMPtr<nsINetworkInterceptController> mController;

  // The actual channel being intercepted.
  RefPtr<nsJARChannel> mChannel;

  // Reader-side of the synthesized response body.
  nsCOMPtr<nsIInputStream> mSynthesizedInput;

  // The stream to write the body of the synthesized response.
  nsCOMPtr<nsIOutputStream> mResponseBody;

  // The content type of the synthesized response.
  nsCString mContentType;

  virtual ~InterceptedJARChannel() {};
public:
  InterceptedJARChannel(nsJARChannel* aChannel,
                        nsINetworkInterceptController* aController);

  NS_DECL_ISUPPORTS
  NS_DECL_NSIINTERCEPTEDCHANNEL

  void NotifyController();

  virtual nsIConsoleReportCollector*
  GetConsoleReportCollector() const override;
};

} // namespace net
} // namespace mozilla

#endif // InterceptedJARChannel_h
