/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set sw=2 ts=8 et tw=80 : */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "WebSocketLog.h"
#include "BaseWebSocketChannel.h"
#include "MainThreadUtils.h"
#include "nsILoadGroup.h"
#include "nsINode.h"
#include "nsIInterfaceRequestor.h"
#include "nsProxyRelease.h"
#include "nsStandardURL.h"
#include "LoadInfo.h"
#include "mozilla/dom/ContentChild.h"
#include "nsITransportProvider.h"

using mozilla::dom::ContentChild;

namespace mozilla {
namespace net {

LazyLogModule webSocketLog("nsWebSocket");
static uint64_t gNextWebSocketID = 0;

// We use only 53 bits for the WebSocket serial ID so that it can be converted
// to and from a JS value without loss of precision. The upper bits of the
// WebSocket serial ID hold the process ID. The lower bits identify the
// WebSocket.
static const uint64_t kWebSocketIDTotalBits = 53;
static const uint64_t kWebSocketIDProcessBits = 22;
static const uint64_t kWebSocketIDWebSocketBits =
    kWebSocketIDTotalBits - kWebSocketIDProcessBits;

BaseWebSocketChannel::BaseWebSocketChannel()
    : mWasOpened(0),
      mClientSetPingInterval(0),
      mClientSetPingTimeout(0),
      mEncrypted(false),
      mPingForced(false),
      mIsServerSide(false),
      mPingInterval(0),
      mPingResponseTimeout(10000),
      mHttpChannelId(0) {
  // Generation of a unique serial ID.
  uint64_t processID = 0;
  if (XRE_IsContentProcess()) {
    ContentChild* cc = ContentChild::GetSingleton();
    processID = cc->GetID();
  }

  uint64_t processBits =
      processID & ((uint64_t(1) << kWebSocketIDProcessBits) - 1);

  // Make sure no actual webSocket ends up with mWebSocketID == 0 but less then
  // what the kWebSocketIDProcessBits allows.
  if (++gNextWebSocketID >= (uint64_t(1) << kWebSocketIDWebSocketBits)) {
    gNextWebSocketID = 1;
  }

  uint64_t webSocketBits =
      gNextWebSocketID & ((uint64_t(1) << kWebSocketIDWebSocketBits) - 1);
  mSerial = (processBits << kWebSocketIDWebSocketBits) | webSocketBits;
}

//-----------------------------------------------------------------------------
// BaseWebSocketChannel::nsIWebSocketChannel
//-----------------------------------------------------------------------------

NS_IMETHODIMP
BaseWebSocketChannel::GetOriginalURI(nsIURI** aOriginalURI) {
  LOG(("BaseWebSocketChannel::GetOriginalURI() %p\n", this));

  if (!mOriginalURI) return NS_ERROR_NOT_INITIALIZED;
  *aOriginalURI = do_AddRef(mOriginalURI).take();
  return NS_OK;
}

NS_IMETHODIMP
BaseWebSocketChannel::GetURI(nsIURI** aURI) {
  LOG(("BaseWebSocketChannel::GetURI() %p\n", this));

  if (!mOriginalURI) return NS_ERROR_NOT_INITIALIZED;
  if (mURI) {
    *aURI = do_AddRef(mURI).take();
  } else {
    *aURI = do_AddRef(mOriginalURI).take();
  }
  return NS_OK;
}

NS_IMETHODIMP
BaseWebSocketChannel::GetNotificationCallbacks(
    nsIInterfaceRequestor** aNotificationCallbacks) {
  LOG(("BaseWebSocketChannel::GetNotificationCallbacks() %p\n", this));
  *aNotificationCallbacks = do_AddRef(mCallbacks).take();
  return NS_OK;
}

NS_IMETHODIMP
BaseWebSocketChannel::SetNotificationCallbacks(
    nsIInterfaceRequestor* aNotificationCallbacks) {
  LOG(("BaseWebSocketChannel::SetNotificationCallbacks() %p\n", this));
  mCallbacks = aNotificationCallbacks;
  return NS_OK;
}

NS_IMETHODIMP
BaseWebSocketChannel::GetLoadGroup(nsILoadGroup** aLoadGroup) {
  LOG(("BaseWebSocketChannel::GetLoadGroup() %p\n", this));
  *aLoadGroup = do_AddRef(mLoadGroup).take();
  return NS_OK;
}

NS_IMETHODIMP
BaseWebSocketChannel::SetLoadGroup(nsILoadGroup* aLoadGroup) {
  LOG(("BaseWebSocketChannel::SetLoadGroup() %p\n", this));
  mLoadGroup = aLoadGroup;
  return NS_OK;
}

NS_IMETHODIMP
BaseWebSocketChannel::SetLoadInfo(nsILoadInfo* aLoadInfo) {
  MOZ_RELEASE_ASSERT(aLoadInfo, "loadinfo can't be null");
  mLoadInfo = aLoadInfo;
  return NS_OK;
}

NS_IMETHODIMP
BaseWebSocketChannel::GetLoadInfo(nsILoadInfo** aLoadInfo) {
  *aLoadInfo = do_AddRef(mLoadInfo).take();
  return NS_OK;
}

NS_IMETHODIMP
BaseWebSocketChannel::GetExtensions(nsACString& aExtensions) {
  LOG(("BaseWebSocketChannel::GetExtensions() %p\n", this));
  aExtensions = mNegotiatedExtensions;
  return NS_OK;
}

NS_IMETHODIMP
BaseWebSocketChannel::GetProtocol(nsACString& aProtocol) {
  LOG(("BaseWebSocketChannel::GetProtocol() %p\n", this));
  aProtocol = mProtocol;
  return NS_OK;
}

NS_IMETHODIMP
BaseWebSocketChannel::SetProtocol(const nsACString& aProtocol) {
  LOG(("BaseWebSocketChannel::SetProtocol() %p\n", this));
  mProtocol = aProtocol; /* the sub protocol */
  return NS_OK;
}

NS_IMETHODIMP
BaseWebSocketChannel::GetPingInterval(uint32_t* aSeconds) {
  // stored in ms but should only have second resolution
  MOZ_ASSERT(!(mPingInterval % 1000));

  *aSeconds = mPingInterval / 1000;
  return NS_OK;
}

NS_IMETHODIMP
BaseWebSocketChannel::SetPingInterval(uint32_t aSeconds) {
  MOZ_ASSERT(NS_IsMainThread());

  if (mWasOpened) {
    return NS_ERROR_IN_PROGRESS;
  }

  mPingInterval = aSeconds * 1000;
  mClientSetPingInterval = 1;

  return NS_OK;
}

NS_IMETHODIMP
BaseWebSocketChannel::GetPingTimeout(uint32_t* aSeconds) {
  // stored in ms but should only have second resolution
  MOZ_ASSERT(!(mPingResponseTimeout % 1000));

  *aSeconds = mPingResponseTimeout / 1000;
  return NS_OK;
}

NS_IMETHODIMP
BaseWebSocketChannel::SetPingTimeout(uint32_t aSeconds) {
  MOZ_ASSERT(NS_IsMainThread());

  if (mWasOpened) {
    return NS_ERROR_IN_PROGRESS;
  }

  mPingResponseTimeout = aSeconds * 1000;
  mClientSetPingTimeout = 1;

  return NS_OK;
}

NS_IMETHODIMP
BaseWebSocketChannel::InitLoadInfoNative(
    nsINode* aLoadingNode, nsIPrincipal* aLoadingPrincipal,
    nsIPrincipal* aTriggeringPrincipal,
    nsICookieJarSettings* aCookieJarSettings, uint32_t aSecurityFlags,
    nsContentPolicyType aContentPolicyType, uint32_t aSandboxFlags) {
  mLoadInfo = new LoadInfo(
      aLoadingPrincipal, aTriggeringPrincipal, aLoadingNode, aSecurityFlags,
      aContentPolicyType, Maybe<mozilla::dom::ClientInfo>(),
      Maybe<mozilla::dom::ServiceWorkerDescriptor>(), aSandboxFlags);
  if (aCookieJarSettings) {
    mLoadInfo->SetCookieJarSettings(aCookieJarSettings);
  }
  return NS_OK;
}

NS_IMETHODIMP
BaseWebSocketChannel::InitLoadInfo(nsINode* aLoadingNode,
                                   nsIPrincipal* aLoadingPrincipal,
                                   nsIPrincipal* aTriggeringPrincipal,
                                   uint32_t aSecurityFlags,
                                   nsContentPolicyType aContentPolicyType) {
  return InitLoadInfoNative(aLoadingNode, aLoadingPrincipal,
                            aTriggeringPrincipal, nullptr, aSecurityFlags,
                            aContentPolicyType, 0);
}

NS_IMETHODIMP
BaseWebSocketChannel::GetSerial(uint32_t* aSerial) {
  if (!aSerial) {
    return NS_ERROR_FAILURE;
  }

  *aSerial = mSerial;
  return NS_OK;
}

NS_IMETHODIMP
BaseWebSocketChannel::SetSerial(uint32_t aSerial) {
  mSerial = aSerial;
  return NS_OK;
}

NS_IMETHODIMP
BaseWebSocketChannel::SetServerParameters(
    nsITransportProvider* aProvider, const nsACString& aNegotiatedExtensions) {
  MOZ_ASSERT(aProvider);
  mServerTransportProvider = aProvider;
  mNegotiatedExtensions = aNegotiatedExtensions;
  mIsServerSide = true;
  return NS_OK;
}

NS_IMETHODIMP
BaseWebSocketChannel::GetHttpChannelId(uint64_t* aHttpChannelId) {
  *aHttpChannelId = mHttpChannelId;
  return NS_OK;
}

//-----------------------------------------------------------------------------
// BaseWebSocketChannel::nsIProtocolHandler
//-----------------------------------------------------------------------------

NS_IMETHODIMP
BaseWebSocketChannel::GetScheme(nsACString& aScheme) {
  LOG(("BaseWebSocketChannel::GetScheme() %p\n", this));

  if (mEncrypted) {
    aScheme.AssignLiteral("wss");
  } else {
    aScheme.AssignLiteral("ws");
  }
  return NS_OK;
}

NS_IMETHODIMP
BaseWebSocketChannel::GetDefaultPort(int32_t* aDefaultPort) {
  LOG(("BaseWebSocketChannel::GetDefaultPort() %p\n", this));

  if (mEncrypted) {
    *aDefaultPort = kDefaultWSSPort;
  } else {
    *aDefaultPort = kDefaultWSPort;
  }
  return NS_OK;
}

NS_IMETHODIMP
BaseWebSocketChannel::GetProtocolFlags(uint32_t* aProtocolFlags) {
  LOG(("BaseWebSocketChannel::GetProtocolFlags() %p\n", this));

  *aProtocolFlags = URI_NORELATIVE | URI_NON_PERSISTABLE | ALLOWS_PROXY |
                    ALLOWS_PROXY_HTTP | URI_DOES_NOT_RETURN_DATA |
                    URI_DANGEROUS_TO_LOAD;
  if (mEncrypted) {
    *aProtocolFlags |= URI_IS_POTENTIALLY_TRUSTWORTHY;
  }
  return NS_OK;
}

NS_IMETHODIMP
BaseWebSocketChannel::NewChannel(nsIURI* aURI, nsILoadInfo* aLoadInfo,
                                 nsIChannel** outChannel) {
  LOG(("BaseWebSocketChannel::NewChannel() %p\n", this));
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
BaseWebSocketChannel::AllowPort(int32_t port, const char* scheme,
                                bool* _retval) {
  LOG(("BaseWebSocketChannel::AllowPort() %p\n", this));

  // do not override any blacklisted ports
  *_retval = false;
  return NS_OK;
}

//-----------------------------------------------------------------------------
// BaseWebSocketChannel::nsIThreadRetargetableRequest
//-----------------------------------------------------------------------------

NS_IMETHODIMP
BaseWebSocketChannel::RetargetDeliveryTo(nsIEventTarget* aTargetThread) {
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(aTargetThread);
  MOZ_ASSERT(!mWasOpened, "Should not be called after AsyncOpen!");
  MOZ_ASSERT(aTargetThread);

  auto lock = mTargetThread.Lock();
  MOZ_ASSERT(!lock.ref(),
             "Delivery target should be set once, before AsyncOpen");
  lock.ref() = aTargetThread;
  return NS_OK;
}

NS_IMETHODIMP
BaseWebSocketChannel::GetDeliveryTarget(nsIEventTarget** aTargetThread) {
  MOZ_ASSERT(NS_IsMainThread());

  nsCOMPtr<nsIEventTarget> target = GetTargetThread();
  if (!target) {
    target = GetCurrentEventTarget();
  }
  target.forget(aTargetThread);
  return NS_OK;
}

already_AddRefed<nsIEventTarget> BaseWebSocketChannel::GetTargetThread() {
  nsCOMPtr<nsIEventTarget> target;
  auto lock = mTargetThread.Lock();
  target = *lock;
  return target.forget();
}

bool BaseWebSocketChannel::IsOnTargetThread() {
  nsCOMPtr<nsIEventTarget> target = GetTargetThread();
  if (!target) {
    MOZ_ASSERT(false);
    return false;
  }

  bool isOnTargetThread = false;
  nsresult rv = target->IsOnCurrentThread(&isOnTargetThread);
  MOZ_ASSERT(NS_SUCCEEDED(rv));
  return NS_FAILED(rv) ? false : isOnTargetThread;
}

BaseWebSocketChannel::ListenerAndContextContainer::ListenerAndContextContainer(
    nsIWebSocketListener* aListener, nsISupports* aContext)
    : mListener(aListener), mContext(aContext) {
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(mListener);
}

BaseWebSocketChannel::ListenerAndContextContainer::
    ~ListenerAndContextContainer() {
  MOZ_ASSERT(mListener);

  NS_ReleaseOnMainThread(
      "BaseWebSocketChannel::ListenerAndContextContainer::mListener",
      mListener.forget());
  NS_ReleaseOnMainThread(
      "BaseWebSocketChannel::ListenerAndContextContainer::mContext",
      mContext.forget());
}

}  // namespace net
}  // namespace mozilla
