/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set sw=2 ts=8 et tw=80 : */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "WebSocketLog.h"
#include "BaseWebSocketChannel.h"
#include "MainThreadUtils.h"
#include "nsILoadGroup.h"
#include "nsIInterfaceRequestor.h"
#include "nsAutoPtr.h"
#include "nsProxyRelease.h"
#include "nsStandardURL.h"

PRLogModuleInfo *webSocketLog = nullptr;

namespace mozilla {
namespace net {

BaseWebSocketChannel::BaseWebSocketChannel()
  : mEncrypted(0)
  , mWasOpened(0)
  , mClientSetPingInterval(0)
  , mClientSetPingTimeout(0)
  , mPingForced(0)
  , mPingInterval(0)
  , mPingResponseTimeout(10000)
{
  if (!webSocketLog)
    webSocketLog = PR_NewLogModule("nsWebSocket");
}

//-----------------------------------------------------------------------------
// BaseWebSocketChannel::nsIWebSocketChannel
//-----------------------------------------------------------------------------

NS_IMETHODIMP
BaseWebSocketChannel::GetOriginalURI(nsIURI **aOriginalURI)
{
  LOG(("BaseWebSocketChannel::GetOriginalURI() %p\n", this));

  if (!mOriginalURI)
    return NS_ERROR_NOT_INITIALIZED;
  NS_ADDREF(*aOriginalURI = mOriginalURI);
  return NS_OK;
}

NS_IMETHODIMP
BaseWebSocketChannel::GetURI(nsIURI **aURI)
{
  LOG(("BaseWebSocketChannel::GetURI() %p\n", this));

  if (!mOriginalURI)
    return NS_ERROR_NOT_INITIALIZED;
  if (mURI)
    NS_ADDREF(*aURI = mURI);
  else
    NS_ADDREF(*aURI = mOriginalURI);
  return NS_OK;
}

NS_IMETHODIMP
BaseWebSocketChannel::
GetNotificationCallbacks(nsIInterfaceRequestor **aNotificationCallbacks)
{
  LOG(("BaseWebSocketChannel::GetNotificationCallbacks() %p\n", this));
  NS_IF_ADDREF(*aNotificationCallbacks = mCallbacks);
  return NS_OK;
}

NS_IMETHODIMP
BaseWebSocketChannel::
SetNotificationCallbacks(nsIInterfaceRequestor *aNotificationCallbacks)
{
  LOG(("BaseWebSocketChannel::SetNotificationCallbacks() %p\n", this));
  mCallbacks = aNotificationCallbacks;
  return NS_OK;
}

NS_IMETHODIMP
BaseWebSocketChannel::GetLoadGroup(nsILoadGroup **aLoadGroup)
{
  LOG(("BaseWebSocketChannel::GetLoadGroup() %p\n", this));
  NS_IF_ADDREF(*aLoadGroup = mLoadGroup);
  return NS_OK;
}

NS_IMETHODIMP
BaseWebSocketChannel::SetLoadGroup(nsILoadGroup *aLoadGroup)
{
  LOG(("BaseWebSocketChannel::SetLoadGroup() %p\n", this));
  mLoadGroup = aLoadGroup;
  return NS_OK;
}

NS_IMETHODIMP
BaseWebSocketChannel::SetLoadInfo(nsILoadInfo* aLoadInfo)
{
  mLoadInfo = aLoadInfo;
  return NS_OK;
}

NS_IMETHODIMP
BaseWebSocketChannel::GetLoadInfo(nsILoadInfo** aLoadInfo)
{
  NS_IF_ADDREF(*aLoadInfo = mLoadInfo);
  return NS_OK;
}

NS_IMETHODIMP
BaseWebSocketChannel::GetExtensions(nsACString &aExtensions)
{
  LOG(("BaseWebSocketChannel::GetExtensions() %p\n", this));
  aExtensions = mNegotiatedExtensions;
  return NS_OK;
}

NS_IMETHODIMP
BaseWebSocketChannel::GetProtocol(nsACString &aProtocol)
{
  LOG(("BaseWebSocketChannel::GetProtocol() %p\n", this));
  aProtocol = mProtocol;
  return NS_OK;
}

NS_IMETHODIMP
BaseWebSocketChannel::SetProtocol(const nsACString &aProtocol)
{
  LOG(("BaseWebSocketChannel::SetProtocol() %p\n", this));
  mProtocol = aProtocol;                        /* the sub protocol */
  return NS_OK;
}

NS_IMETHODIMP
BaseWebSocketChannel::GetPingInterval(uint32_t *aSeconds)
{
  // stored in ms but should only have second resolution
  MOZ_ASSERT(!(mPingInterval % 1000));

  *aSeconds = mPingInterval / 1000;
  return NS_OK;
}

NS_IMETHODIMP
BaseWebSocketChannel::SetPingInterval(uint32_t aSeconds)
{
  if (mWasOpened) {
    return NS_ERROR_IN_PROGRESS;
  }

  mPingInterval = aSeconds * 1000;
  mClientSetPingInterval = 1;

  return NS_OK;
}

NS_IMETHODIMP
BaseWebSocketChannel::GetPingTimeout(uint32_t *aSeconds)
{
  // stored in ms but should only have second resolution
  MOZ_ASSERT(!(mPingResponseTimeout % 1000));

  *aSeconds = mPingResponseTimeout / 1000;
  return NS_OK;
}

NS_IMETHODIMP
BaseWebSocketChannel::SetPingTimeout(uint32_t aSeconds)
{
  if (mWasOpened) {
    return NS_ERROR_IN_PROGRESS;
  }

  mPingResponseTimeout = aSeconds * 1000;
  mClientSetPingTimeout = 1;

  return NS_OK;
}

NS_IMETHODIMP
BaseWebSocketChannel::InitLoadInfo(nsIDOMNode* aLoadingNode,
                                   nsIPrincipal* aLoadingPrincipal,
                                   nsIPrincipal* aTriggeringPrincipal,
                                   uint32_t aSecurityFlags,
                                   uint32_t aContentPolicyType)
{
  nsCOMPtr<nsINode> node = do_QueryInterface(aLoadingNode);
  mLoadInfo = new LoadInfo(aLoadingPrincipal, aTriggeringPrincipal,
                           node, aSecurityFlags, aContentPolicyType);
  return NS_OK;
}

//-----------------------------------------------------------------------------
// BaseWebSocketChannel::nsIProtocolHandler
//-----------------------------------------------------------------------------


NS_IMETHODIMP
BaseWebSocketChannel::GetScheme(nsACString &aScheme)
{
  LOG(("BaseWebSocketChannel::GetScheme() %p\n", this));

  if (mEncrypted)
    aScheme.AssignLiteral("wss");
  else
    aScheme.AssignLiteral("ws");
  return NS_OK;
}

NS_IMETHODIMP
BaseWebSocketChannel::GetDefaultPort(int32_t *aDefaultPort)
{
  LOG(("BaseWebSocketChannel::GetDefaultPort() %p\n", this));

  if (mEncrypted)
    *aDefaultPort = kDefaultWSSPort;
  else
    *aDefaultPort = kDefaultWSPort;
  return NS_OK;
}

NS_IMETHODIMP
BaseWebSocketChannel::GetProtocolFlags(uint32_t *aProtocolFlags)
{
  LOG(("BaseWebSocketChannel::GetProtocolFlags() %p\n", this));

  *aProtocolFlags = URI_NORELATIVE | URI_NON_PERSISTABLE | ALLOWS_PROXY | 
      ALLOWS_PROXY_HTTP | URI_DOES_NOT_RETURN_DATA | URI_DANGEROUS_TO_LOAD;
  return NS_OK;
}

NS_IMETHODIMP
BaseWebSocketChannel::NewURI(const nsACString & aSpec, const char *aOriginCharset,
                             nsIURI *aBaseURI, nsIURI **_retval)
{
  LOG(("BaseWebSocketChannel::NewURI() %p\n", this));

  int32_t port;
  nsresult rv = GetDefaultPort(&port);
  if (NS_FAILED(rv))
    return rv;

  nsRefPtr<nsStandardURL> url = new nsStandardURL();
  rv = url->Init(nsIStandardURL::URLTYPE_AUTHORITY, port, aSpec,
                aOriginCharset, aBaseURI);
  if (NS_FAILED(rv))
    return rv;
  url.forget(_retval);
  return NS_OK;
}

NS_IMETHODIMP
BaseWebSocketChannel::NewChannel2(nsIURI* aURI,
                                  nsILoadInfo* aLoadInfo,
                                  nsIChannel** outChannel)
{
  LOG(("BaseWebSocketChannel::NewChannel2() %p\n", this));
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
BaseWebSocketChannel::NewChannel(nsIURI *aURI, nsIChannel **_retval)
{
  LOG(("BaseWebSocketChannel::NewChannel() %p\n", this));
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
BaseWebSocketChannel::AllowPort(int32_t port, const char *scheme,
                                bool *_retval)
{
  LOG(("BaseWebSocketChannel::AllowPort() %p\n", this));

  // do not override any blacklisted ports
  *_retval = false;
  return NS_OK;
}

//-----------------------------------------------------------------------------
// BaseWebSocketChannel::nsIThreadRetargetableRequest
//-----------------------------------------------------------------------------

NS_IMETHODIMP
BaseWebSocketChannel::RetargetDeliveryTo(nsIEventTarget* aTargetThread)
{
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(aTargetThread);
  MOZ_ASSERT(!mTargetThread, "Delivery target should be set once, before AsyncOpen");
  MOZ_ASSERT(!mWasOpened, "Should not be called after AsyncOpen!");

  mTargetThread = do_QueryInterface(aTargetThread);
  MOZ_ASSERT(mTargetThread);
  return NS_OK;
}

BaseWebSocketChannel::ListenerAndContextContainer::ListenerAndContextContainer(
                                               nsIWebSocketListener* aListener,
                                               nsISupports* aContext)
  : mListener(aListener)
  , mContext(aContext)
{
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(mListener);
}

BaseWebSocketChannel::ListenerAndContextContainer::~ListenerAndContextContainer()
{
  MOZ_ASSERT(mListener);

  nsCOMPtr<nsIThread> mainThread;
  NS_GetMainThread(getter_AddRefs(mainThread));

  NS_ProxyRelease(mainThread, mListener, false);
  NS_ProxyRelease(mainThread, mContext, false);
}

} // namespace net
} // namespace mozilla
