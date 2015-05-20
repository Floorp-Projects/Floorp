/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsSecCheckWrapChannel_h__
#define nsSecCheckWrapChannel_h__

#include "nsIHttpChannel.h"
#include "nsIHttpChannelInternal.h"
#include "nsISecCheckWrapChannel.h"
#include "nsIWyciwygChannel.h"
#include "mozilla/LoadInfo.h"

/*
 * The nsSecCheckWrapChannelBase wraps channels that do *not*
 *  * provide a newChannel2() implementation
 *  * provide get/setLoadInfo functions
 *
 * In order to perform security checks for channels
 *   a) before opening the channel, and
 *   b) after redirects
 * we are attaching a loadinfo object to every channel which
 * provides information about the content-type of the channel,
 * who initiated the load, etc.
 *
 * Addon created channels might *not* provide that loadInfo object for
 * some transition time before we mark the NewChannel-API as deprecated.
 * We do not want to break those addons hence we wrap such channels
 * using the provided wrapper in this class.
 *
 * Please note that the wrapper only forwards calls for
 *  * nsIRequest
 *  * nsIChannel
 *  * nsIHttpChannel
 *  * nsIHttpChannelInternal
 *
 * In case any addon needs to query the inner channel this class
 * provides a readonly function to query the wrapped channel.
 *
 */

class nsSecCheckWrapChannelBase : public nsIHttpChannel
                                , public nsIHttpChannelInternal
                                , public nsISecCheckWrapChannel
{
public:
  NS_FORWARD_NSIHTTPCHANNEL(mHttpChannel->)
  NS_FORWARD_NSIHTTPCHANNELINTERNAL(mHttpChannelInternal->)
  NS_FORWARD_NSICHANNEL(mChannel->)
  NS_FORWARD_NSIREQUEST(mRequest->)
  NS_DECL_NSISECCHECKWRAPCHANNEL
  NS_DECL_ISUPPORTS

  explicit nsSecCheckWrapChannelBase(nsIChannel* aChannel);

protected:
  virtual ~nsSecCheckWrapChannelBase();

  nsCOMPtr<nsIChannel>             mChannel;
  // We do a QI in the constructor to set the following pointers.
  nsCOMPtr<nsIHttpChannel>         mHttpChannel;
  nsCOMPtr<nsIHttpChannelInternal> mHttpChannelInternal;
  nsCOMPtr<nsIRequest>             mRequest;
};

/* We define a separate class here to make it clear that we're
 * overriding Get/SetLoadInfo, rather that using the forwarded
 * implementations provided by NS_FORWARD_NSICHANNEL"
 */
class nsSecCheckWrapChannel : public nsSecCheckWrapChannelBase
{
public:
  NS_IMETHOD GetLoadInfo(nsILoadInfo **aLoadInfo);
  NS_IMETHOD SetLoadInfo(nsILoadInfo *aLoadInfo);

  nsSecCheckWrapChannel(nsIChannel* aChannel, nsILoadInfo* aLoadInfo);

protected:
  virtual ~nsSecCheckWrapChannel();

  nsCOMPtr<nsILoadInfo> mLoadInfo;
};

#endif // nsSecCheckWrapChannel_h__
