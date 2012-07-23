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

namespace mozilla {
namespace net {

WyciwygChannelParent::WyciwygChannelParent()
 : mIPCClosed(false)
 , mHaveLoadContext(false)
 , mIsContent(false)
 , mUsePrivateBrowsing(false)
 , mIsInBrowserElement(false)
 , mAppId(0)
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
}

//-----------------------------------------------------------------------------
// WyciwygChannelParent::nsISupports
//-----------------------------------------------------------------------------

NS_IMPL_ISUPPORTS4(WyciwygChannelParent,
                   nsIStreamListener,
                   nsIInterfaceRequestor,
                   nsILoadContext,
                   nsIRequestObserver);

//-----------------------------------------------------------------------------
// WyciwygChannelParent::PWyciwygChannelParent
//-----------------------------------------------------------------------------

bool
WyciwygChannelParent::RecvInit(const IPC::URI& aURI)
{
  nsresult rv;

  nsCOMPtr<nsIURI> uri(aURI);

  nsCString uriSpec;
  uri->GetSpec(uriSpec);
  LOG(("WyciwygChannelParent RecvInit [this=%x uri=%s]\n",
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
WyciwygChannelParent::RecvAsyncOpen(const IPC::URI& aOriginal,
                                    const PRUint32& aLoadFlags,
                                    const bool& haveLoadContext,
                                    const bool& isContent,
                                    const bool& usePrivateBrowsing,
                                    const bool& isInBrowserElement,
                                    const PRUint32& appId,
                                    const nsCString& extendedOrigin)
{
  nsCOMPtr<nsIURI> original(aOriginal);

  LOG(("WyciwygChannelParent RecvAsyncOpen [this=%x]\n", this));

  if (!mChannel)
    return true;

  nsresult rv;

  rv = mChannel->SetOriginalURI(original);
  if (NS_FAILED(rv))
    return SendCancelEarly(rv);

  rv = mChannel->SetLoadFlags(aLoadFlags);
  if (NS_FAILED(rv))
    return SendCancelEarly(rv);

  // fields needed to impersonate nsILoadContext
  mHaveLoadContext = haveLoadContext;
  mIsContent = isContent;
  mUsePrivateBrowsing = usePrivateBrowsing;
  mIsInBrowserElement = isInBrowserElement;
  mAppId = appId;
  mExtendedOrigin = extendedOrigin;
  mChannel->SetNotificationCallbacks(this);

  rv = mChannel->AsyncOpen(this, nsnull);
  if (NS_FAILED(rv))
    return SendCancelEarly(rv);

  return true;
}

bool
WyciwygChannelParent::RecvWriteToCacheEntry(const nsString& data)
{
  if (mChannel)
    mChannel->WriteToCacheEntry(data);

  return true;
}

bool
WyciwygChannelParent::RecvCloseCacheEntry(const nsresult& reason)
{
  if (mChannel)
    mChannel->CloseCacheEntry(reason);

  return true;
}

bool
WyciwygChannelParent::RecvSetCharsetAndSource(const PRInt32& aCharsetSource,
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
  LOG(("WyciwygChannelParent::OnStartRequest [this=%x]\n", this));

  nsresult rv;

  nsCOMPtr<nsIWyciwygChannel> chan = do_QueryInterface(aRequest, &rv);
  NS_ENSURE_SUCCESS(rv, rv);

  nsresult status;
  chan->GetStatus(&status);

  PRInt32 contentLength = -1;
  chan->GetContentLength(&contentLength);

  PRInt32 charsetSource = kCharsetUninitialized;
  nsCAutoString charset;
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
  LOG(("WyciwygChannelParent::OnStopRequest: [this=%x status=%ul]\n",
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
                                      PRUint32 aOffset,
                                      PRUint32 aCount)
{
  LOG(("WyciwygChannelParent::OnDataAvailable [this=%x]\n", this));

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
  if (uuid.Equals(NS_GET_IID(nsILoadContext)) && !mHaveLoadContext) {
    return NS_NOINTERFACE;
  }

  return QueryInterface(uuid, result);
}

//-----------------------------------------------------------------------------
// WyciwygChannelParent::nsILoadContext
//-----------------------------------------------------------------------------

NS_IMETHODIMP
WyciwygChannelParent::GetAssociatedWindow(nsIDOMWindow**)
{
  // can't support this in the parent process
  return NS_ERROR_UNEXPECTED;
}

NS_IMETHODIMP
WyciwygChannelParent::GetTopWindow(nsIDOMWindow**)
{
  // can't support this in the parent process
  return NS_ERROR_UNEXPECTED;
}

NS_IMETHODIMP
WyciwygChannelParent::IsAppOfType(PRUint32, bool*)
{
  // don't expect we need this in parent (Thunderbird/SeaMonkey specific?)
  return NS_ERROR_UNEXPECTED;
}

NS_IMETHODIMP
WyciwygChannelParent::GetIsContent(bool *aIsContent)
{
  NS_ENSURE_ARG_POINTER(aIsContent);

  *aIsContent = mIsContent;
  return NS_OK;
}

NS_IMETHODIMP
WyciwygChannelParent::GetUsePrivateBrowsing(bool* aUsePrivateBrowsing)
{
  NS_ENSURE_ARG_POINTER(aUsePrivateBrowsing);

  *aUsePrivateBrowsing = mUsePrivateBrowsing;
  return NS_OK;
}

NS_IMETHODIMP
WyciwygChannelParent::SetUsePrivateBrowsing(bool aUsePrivateBrowsing)
{
  // We shouldn't need this on parent...
  return NS_ERROR_UNEXPECTED;
}

NS_IMETHODIMP
WyciwygChannelParent::GetIsInBrowserElement(bool* aIsInBrowserElement)
{
  NS_ENSURE_ARG_POINTER(aIsInBrowserElement);

  *aIsInBrowserElement = mIsInBrowserElement;
  return NS_OK;
}

NS_IMETHODIMP
WyciwygChannelParent::GetAppId(PRUint32* aAppId)
{
  NS_ENSURE_ARG_POINTER(aAppId);

  *aAppId = mAppId;
  return NS_OK;
}

NS_IMETHODIMP
WyciwygChannelParent::GetExtendedOrigin(nsIURI *aUri,
                                        nsACString &aResult)
{
  aResult = mExtendedOrigin;
  return NS_OK;
}


}} // mozilla::net
