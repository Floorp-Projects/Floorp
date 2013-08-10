/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsWyciwyg.h"

#include "mozilla/net/WyciwygChannelParent.h"
#include "nsWyciwygChannel.h"
#include "nsNetUtil.h"
#include "nsISupportsPriority.h"
#include "nsCharsetSource.h"
#include "nsISerializable.h"
#include "nsSerializationHelper.h"
#include "mozilla/ipc/URIUtils.h"
#include "mozilla/net/NeckoParent.h"

using namespace mozilla::ipc;

namespace mozilla {
namespace net {

WyciwygChannelParent::WyciwygChannelParent()
 : mIPCClosed(false)
 , mReceivedAppData(false)
{
#if defined(PR_LOGGING)
  if (!gWyciwygLog)
    gWyciwygLog = PR_NewLogModule("nsWyciwygChannel");
#endif
}

WyciwygChannelParent::~WyciwygChannelParent()
{
}

void
WyciwygChannelParent::ActorDestroy(ActorDestroyReason why)
{
  // We may still have refcount>0 if the channel hasn't called OnStopRequest
  // yet, but we must not send any more msgs to child.
  mIPCClosed = true;

  // We need to force the cycle to break here
  mChannel->SetNotificationCallbacks(nullptr);
}

//-----------------------------------------------------------------------------
// WyciwygChannelParent::nsISupports
//-----------------------------------------------------------------------------

NS_IMPL_ISUPPORTS3(WyciwygChannelParent,
                   nsIStreamListener,
                   nsIInterfaceRequestor,
                   nsIRequestObserver)

//-----------------------------------------------------------------------------
// WyciwygChannelParent::PWyciwygChannelParent
//-----------------------------------------------------------------------------

bool
WyciwygChannelParent::RecvInit(const URIParams& aURI)
{
  nsresult rv;

  nsCOMPtr<nsIURI> uri = DeserializeURI(aURI);
  if (!uri)
    return false;

  nsCString uriSpec;
  uri->GetSpec(uriSpec);
  LOG(("WyciwygChannelParent RecvInit [this=%p uri=%s]\n",
       this, uriSpec.get()));

  nsCOMPtr<nsIIOService> ios(do_GetIOService(&rv));
  if (NS_FAILED(rv))
    return SendCancelEarly(rv);

  nsCOMPtr<nsIChannel> chan;
  rv = NS_NewChannel(getter_AddRefs(chan), uri, ios);
  if (NS_FAILED(rv))
    return SendCancelEarly(rv);

  mChannel = do_QueryInterface(chan, &rv);
  if (NS_FAILED(rv))
    return SendCancelEarly(rv);

  return true;
}

bool
WyciwygChannelParent::RecvAppData(const IPC::SerializedLoadContext& loadContext,
                                  PBrowserParent* parent)
{
  LOG(("WyciwygChannelParent RecvAppData [this=%p]\n", this));

  if (!SetupAppData(loadContext, parent))
    return false;

  mChannel->SetNotificationCallbacks(this);
  return true;
}

bool
WyciwygChannelParent::SetupAppData(const IPC::SerializedLoadContext& loadContext,
                                   PBrowserParent* aParent)
{
  if (!mChannel)
    return true;

  const char* error = NeckoParent::CreateChannelLoadContext(aParent,
                                                            Manager()->Manager(),
                                                            loadContext,
                                                            mLoadContext);
  if (error) {
    printf_stderr(nsPrintfCString("WyciwygChannelParent::SetupAppData: FATAL ERROR: %s\n",
                                  error).get());
    return false;
  }

  if (!mLoadContext && loadContext.IsPrivateBitValid()) {
    nsCOMPtr<nsIPrivateBrowsingChannel> pbChannel = do_QueryInterface(mChannel);
    if (pbChannel)
      pbChannel->SetPrivate(loadContext.mUsePrivateBrowsing);
  }

  mReceivedAppData = true;
  return true;
}

bool
WyciwygChannelParent::RecvAsyncOpen(const URIParams& aOriginal,
                                    const uint32_t& aLoadFlags,
                                    const IPC::SerializedLoadContext& loadContext,
                                    PBrowserParent* aParent)
{
  nsCOMPtr<nsIURI> original = DeserializeURI(aOriginal);
  if (!original)
    return false;

  LOG(("WyciwygChannelParent RecvAsyncOpen [this=%p]\n", this));

  if (!mChannel)
    return true;

  nsresult rv;

  rv = mChannel->SetOriginalURI(original);
  if (NS_FAILED(rv))
    return SendCancelEarly(rv);

  rv = mChannel->SetLoadFlags(aLoadFlags);
  if (NS_FAILED(rv))
    return SendCancelEarly(rv);

  if (!mReceivedAppData && !SetupAppData(loadContext, aParent)) {
    return false;
  }

  rv = mChannel->SetNotificationCallbacks(this);
  if (NS_FAILED(rv))
    return SendCancelEarly(rv);

  rv = mChannel->AsyncOpen(this, nullptr);
  if (NS_FAILED(rv))
    return SendCancelEarly(rv);

  return true;
}

bool
WyciwygChannelParent::RecvWriteToCacheEntry(const nsString& data)
{
  if (!mReceivedAppData) {
    printf_stderr("WyciwygChannelParent::RecvWriteToCacheEntry: FATAL ERROR: didn't receive app data\n");
    return false;
  }

  if (mChannel)
    mChannel->WriteToCacheEntry(data);

  return true;
}

bool
WyciwygChannelParent::RecvCloseCacheEntry(const nsresult& reason)
{
  if (mChannel) {
    mChannel->CloseCacheEntry(reason);
  }

  return true;
}

bool
WyciwygChannelParent::RecvSetCharsetAndSource(const int32_t& aCharsetSource,
                                              const nsCString& aCharset)
{
  if (mChannel)
    mChannel->SetCharsetAndSource(aCharsetSource, aCharset);

  return true;
}

bool
WyciwygChannelParent::RecvSetSecurityInfo(const nsCString& aSecurityInfo)
{
  if (mChannel) {
    nsCOMPtr<nsISupports> securityInfo;
    NS_DeserializeObject(aSecurityInfo, getter_AddRefs(securityInfo));
    mChannel->SetSecurityInfo(securityInfo);
  }

  return true;
}

bool
WyciwygChannelParent::RecvCancel(const nsresult& aStatusCode)
{
  if (mChannel)
    mChannel->Cancel(aStatusCode);
  return true;
}

//-----------------------------------------------------------------------------
// WyciwygChannelParent::nsIRequestObserver
//-----------------------------------------------------------------------------

NS_IMETHODIMP
WyciwygChannelParent::OnStartRequest(nsIRequest *aRequest, nsISupports *aContext)
{
  LOG(("WyciwygChannelParent::OnStartRequest [this=%p]\n", this));

  nsresult rv;

  nsCOMPtr<nsIWyciwygChannel> chan = do_QueryInterface(aRequest, &rv);
  NS_ENSURE_SUCCESS(rv, rv);

  nsresult status;
  chan->GetStatus(&status);

  int64_t contentLength = -1;
  chan->GetContentLength(&contentLength);

  int32_t charsetSource = kCharsetUninitialized;
  nsAutoCString charset;
  chan->GetCharsetAndSource(&charsetSource, charset);

  nsCOMPtr<nsISupports> securityInfo;
  chan->GetSecurityInfo(getter_AddRefs(securityInfo));
  nsCString secInfoStr;
  if (securityInfo) {
    nsCOMPtr<nsISerializable> serializable = do_QueryInterface(securityInfo);
    if (serializable)
      NS_SerializeToString(serializable, secInfoStr);
    else {
      NS_ERROR("Can't serialize security info");
      return NS_ERROR_UNEXPECTED;
    }
  }

  if (mIPCClosed ||
      !SendOnStartRequest(status, contentLength, charsetSource, charset, secInfoStr)) {
    return NS_ERROR_UNEXPECTED;
  }

  return NS_OK;
}

NS_IMETHODIMP
WyciwygChannelParent::OnStopRequest(nsIRequest *aRequest,
                                    nsISupports *aContext,
                                    nsresult aStatusCode)
{
  LOG(("WyciwygChannelParent::OnStopRequest: [this=%p status=%ul]\n",
       this, aStatusCode));

  if (mIPCClosed || !SendOnStopRequest(aStatusCode)) {
    return NS_ERROR_UNEXPECTED;
  }

  return NS_OK;
}

//-----------------------------------------------------------------------------
// WyciwygChannelParent::nsIStreamListener
//-----------------------------------------------------------------------------

NS_IMETHODIMP
WyciwygChannelParent::OnDataAvailable(nsIRequest *aRequest,
                                      nsISupports *aContext,
                                      nsIInputStream *aInputStream,
                                      uint64_t aOffset,
                                      uint32_t aCount)
{
  LOG(("WyciwygChannelParent::OnDataAvailable [this=%p]\n", this));

  nsCString data;
  nsresult rv = NS_ReadInputStreamToString(aInputStream, data, aCount);
  if (NS_FAILED(rv))
    return rv;

  if (mIPCClosed || !SendOnDataAvailable(data, aOffset)) {
    return NS_ERROR_UNEXPECTED;
  }

  return NS_OK;
}

//-----------------------------------------------------------------------------
// WyciwygChannelParent::nsIInterfaceRequestor
//-----------------------------------------------------------------------------

NS_IMETHODIMP
WyciwygChannelParent::GetInterface(const nsIID& uuid, void** result)
{
  // Only support nsILoadContext if child channel's callbacks did too
  if (uuid.Equals(NS_GET_IID(nsILoadContext)) && mLoadContext) {
    NS_ADDREF(mLoadContext);
    *result = static_cast<nsILoadContext*>(mLoadContext);
    return NS_OK;
  }

  return QueryInterface(uuid, result);
}


}} // mozilla::net
