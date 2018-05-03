/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsContentSecurityManager.h"
#include "nsSecCheckWrapChannel.h"
#include "nsIForcePendingChannel.h"
#include "nsIStreamListener.h"
#include "mozilla/Logging.h"
#include "nsCOMPtr.h"

namespace mozilla {
namespace net {

static LazyLogModule gChannelWrapperLog("ChannelWrapper");
#define CHANNELWRAPPERLOG(args) MOZ_LOG(gChannelWrapperLog, LogLevel::Debug, args)

NS_IMPL_ADDREF(nsSecCheckWrapChannelBase)
NS_IMPL_RELEASE(nsSecCheckWrapChannelBase)

NS_INTERFACE_MAP_BEGIN(nsSecCheckWrapChannelBase)
  NS_INTERFACE_MAP_ENTRY_CONDITIONAL(nsIHttpChannel, mHttpChannel)
  NS_INTERFACE_MAP_ENTRY_CONDITIONAL(nsIHttpChannelInternal, mHttpChannelInternal)
  NS_INTERFACE_MAP_ENTRY_AMBIGUOUS(nsISupports, nsIHttpChannel)
  NS_INTERFACE_MAP_ENTRY(nsIRequest)
  NS_INTERFACE_MAP_ENTRY(nsIChannel)
  NS_INTERFACE_MAP_ENTRY_CONDITIONAL(nsIUploadChannel, mUploadChannel)
  NS_INTERFACE_MAP_ENTRY_CONDITIONAL(nsIUploadChannel2, mUploadChannel2)
  NS_INTERFACE_MAP_ENTRY(nsISecCheckWrapChannel)
NS_INTERFACE_MAP_END

//---------------------------------------------------------
// nsSecCheckWrapChannelBase implementation
//---------------------------------------------------------

nsSecCheckWrapChannelBase::nsSecCheckWrapChannelBase(nsIChannel* aChannel)
 : mChannel(aChannel)
 , mHttpChannel(do_QueryInterface(aChannel))
 , mHttpChannelInternal(do_QueryInterface(aChannel))
 , mRequest(do_QueryInterface(aChannel))
 , mUploadChannel(do_QueryInterface(aChannel))
 , mUploadChannel2(do_QueryInterface(aChannel))
{
  MOZ_ASSERT(mChannel, "can not create a channel wrapper without a channel");
}

//---------------------------------------------------------
// nsISecCheckWrapChannel implementation
//---------------------------------------------------------

NS_IMETHODIMP
nsSecCheckWrapChannelBase::GetInnerChannel(nsIChannel **aInnerChannel)
{
  NS_IF_ADDREF(*aInnerChannel = mChannel);
  return NS_OK;
}

//---------------------------------------------------------
// nsSecCheckWrapChannel implementation
//---------------------------------------------------------

nsSecCheckWrapChannel::nsSecCheckWrapChannel(nsIChannel* aChannel,
                                             nsILoadInfo* aLoadInfo)
 : nsSecCheckWrapChannelBase(aChannel)
 , mLoadInfo(aLoadInfo)
{
  {
    nsCOMPtr<nsIURI> uri;
    mChannel->GetURI(getter_AddRefs(uri));
    CHANNELWRAPPERLOG(("nsSecCheckWrapChannel::nsSecCheckWrapChannel [%p] (%s)",
                       this, uri ? uri->GetSpecOrDefault().get() : ""));
  }
}

// static
already_AddRefed<nsIChannel>
nsSecCheckWrapChannel::MaybeWrap(nsIChannel* aChannel, nsILoadInfo* aLoadInfo)
{
  // Maybe a custom protocol handler actually returns a gecko
  // http/ftpChannel - To check this we will check whether the channel
  // implements a gecko non-scriptable interface e.g. nsIForcePendingChannel.
  nsCOMPtr<nsIForcePendingChannel> isGeckoChannel = do_QueryInterface(aChannel);

  nsCOMPtr<nsIChannel> channel;
  if (isGeckoChannel) {
    // If it is a gecko channel (ftp or http) we do not need to wrap it.
    channel = aChannel;
    channel->SetLoadInfo(aLoadInfo);
  } else {
    channel = new nsSecCheckWrapChannel(aChannel, aLoadInfo);
  }
  return channel.forget();
}

//---------------------------------------------------------
// SecWrapChannelStreamListener helper
//---------------------------------------------------------

class SecWrapChannelStreamListener final : public nsIStreamListener
{
  public:
    SecWrapChannelStreamListener(nsIRequest *aRequest,
                                 nsIStreamListener *aStreamListener)
    : mRequest(aRequest)
    , mListener(aStreamListener) {}

    NS_DECL_ISUPPORTS
    NS_DECL_NSISTREAMLISTENER
    NS_DECL_NSIREQUESTOBSERVER

  private:
    ~SecWrapChannelStreamListener() = default;

    nsCOMPtr<nsIRequest>        mRequest;
    nsCOMPtr<nsIStreamListener> mListener;
};

NS_IMPL_ISUPPORTS(SecWrapChannelStreamListener,
                  nsIStreamListener,
                  nsIRequestObserver)

NS_IMETHODIMP
SecWrapChannelStreamListener::OnStartRequest(nsIRequest *aRequest,
                                             nsISupports *aContext)
{
  return mListener->OnStartRequest(mRequest, aContext);
}

NS_IMETHODIMP
SecWrapChannelStreamListener::OnStopRequest(nsIRequest *aRequest,
                                            nsISupports *aContext,
                                            nsresult aStatus)
{
  return mListener->OnStopRequest(mRequest, aContext, aStatus);
}

NS_IMETHODIMP
SecWrapChannelStreamListener::OnDataAvailable(nsIRequest *aRequest,
                                              nsISupports *aContext,
                                              nsIInputStream *aInStream,
                                              uint64_t aOffset,
                                              uint32_t aCount)
{
  return mListener->OnDataAvailable(mRequest, aContext, aInStream, aOffset, aCount);
}

//---------------------------------------------------------
// nsIChannel implementation
//---------------------------------------------------------

NS_IMETHODIMP
nsSecCheckWrapChannel::GetLoadInfo(nsILoadInfo** aLoadInfo)
{
  CHANNELWRAPPERLOG(("nsSecCheckWrapChannel::GetLoadInfo() [%p]",this));
  NS_IF_ADDREF(*aLoadInfo = mLoadInfo);
  return NS_OK;
}

NS_IMETHODIMP
nsSecCheckWrapChannel::SetLoadInfo(nsILoadInfo* aLoadInfo)
{
  CHANNELWRAPPERLOG(("nsSecCheckWrapChannel::SetLoadInfo() [%p]", this));
  mLoadInfo = aLoadInfo;
  return NS_OK;
}

NS_IMETHODIMP
nsSecCheckWrapChannel::AsyncOpen2(nsIStreamListener *aListener)
{
  nsCOMPtr<nsIStreamListener> secWrapChannelListener =
    new SecWrapChannelStreamListener(this, aListener);
  nsresult rv = nsContentSecurityManager::doContentSecurityCheck(this, secWrapChannelListener);
  NS_ENSURE_SUCCESS(rv, rv);
  return AsyncOpen(secWrapChannelListener, nullptr);
}

NS_IMETHODIMP
nsSecCheckWrapChannel::Open2(nsIInputStream** aStream)
{
  nsCOMPtr<nsIStreamListener> listener;
  nsresult rv = nsContentSecurityManager::doContentSecurityCheck(this, listener);
  NS_ENSURE_SUCCESS(rv, rv);
  return Open(aStream);
}

} // namespace net
} // namespace mozilla
