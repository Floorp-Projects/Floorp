/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set sw=2 ts=8 et tw=80 : */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "WebSocketLog.h"
#include "WebSocketChannelParent.h"
#include "nsIAuthPromptProvider.h"
#include "mozilla/ipc/InputStreamUtils.h"
#include "mozilla/ipc/URIUtils.h"
#include "mozilla/ipc/BackgroundUtils.h"
#include "SerializedLoadContext.h"
#include "mozilla/net/NeckoCommon.h"
#include "mozilla/net/WebSocketChannel.h"
#include "nsComponentManagerUtils.h"
#include "IPCTransportProvider.h"
#include "mozilla/net/ChannelEventQueue.h"

using namespace mozilla::ipc;

namespace mozilla {
namespace net {

NS_IMPL_ISUPPORTS(WebSocketChannelParent, nsIWebSocketListener,
                  nsIInterfaceRequestor)

WebSocketChannelParent::WebSocketChannelParent(
    nsIAuthPromptProvider* aAuthProvider, nsILoadContext* aLoadContext,
    PBOverrideStatus aOverrideStatus, uint32_t aSerial)
    : mAuthProvider(aAuthProvider),
      mLoadContext(aLoadContext),
      mSerial(aSerial) {
  // Websocket channels can't have a private browsing override
  MOZ_ASSERT_IF(!aLoadContext, aOverrideStatus == kPBOverride_Unset);
}
//-----------------------------------------------------------------------------
// WebSocketChannelParent::PWebSocketChannelParent
//-----------------------------------------------------------------------------

mozilla::ipc::IPCResult WebSocketChannelParent::RecvDeleteSelf() {
  LOG(("WebSocketChannelParent::RecvDeleteSelf() %p\n", this));
  mChannel = nullptr;
  mAuthProvider = nullptr;
  IProtocol* mgr = Manager();
  if (CanRecv() && !Send__delete__(this)) {
    return IPC_FAIL_NO_REASON(mgr);
  }
  return IPC_OK();
}

mozilla::ipc::IPCResult WebSocketChannelParent::RecvAsyncOpen(
    nsIURI* aURI, const nsCString& aOrigin,
    const OriginAttributes& aOriginAttributes, const uint64_t& aInnerWindowID,
    const nsCString& aProtocol, const bool& aSecure,
    const uint32_t& aPingInterval, const bool& aClientSetPingInterval,
    const uint32_t& aPingTimeout, const bool& aClientSetPingTimeout,
    const Maybe<LoadInfoArgs>& aLoadInfoArgs,
    const Maybe<PTransportProviderParent*>& aTransportProvider,
    const nsCString& aNegotiatedExtensions) {
  LOG(("WebSocketChannelParent::RecvAsyncOpen() %p\n", this));

  nsresult rv;
  nsCOMPtr<nsILoadInfo> loadInfo;
  nsCOMPtr<nsIURI> uri;

  rv = LoadInfoArgsToLoadInfo(aLoadInfoArgs, getter_AddRefs(loadInfo));
  if (NS_FAILED(rv)) {
    goto fail;
  }

  if (aSecure) {
    mChannel =
        do_CreateInstance("@mozilla.org/network/protocol;1?name=wss", &rv);
  } else {
    mChannel =
        do_CreateInstance("@mozilla.org/network/protocol;1?name=ws", &rv);
  }
  if (NS_FAILED(rv)) goto fail;

  rv = mChannel->SetSerial(mSerial);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    goto fail;
  }

  rv = mChannel->SetLoadInfo(loadInfo);
  if (NS_FAILED(rv)) {
    goto fail;
  }

  rv = mChannel->SetNotificationCallbacks(this);
  if (NS_FAILED(rv)) goto fail;

  rv = mChannel->SetProtocol(aProtocol);
  if (NS_FAILED(rv)) goto fail;

  if (aTransportProvider.isSome()) {
    RefPtr<TransportProviderParent> provider =
        static_cast<TransportProviderParent*>(aTransportProvider.value());
    rv = mChannel->SetServerParameters(provider, aNegotiatedExtensions);
    if (NS_FAILED(rv)) {
      goto fail;
    }
  } else {
    uri = aURI;
    if (!uri) {
      rv = NS_ERROR_FAILURE;
      goto fail;
    }
  }

  // only use ping values from child if they were overridden by client code.
  if (aClientSetPingInterval) {
    // IDL allows setting in seconds, so must be multiple of 1000 ms
    MOZ_ASSERT(aPingInterval >= 1000 && !(aPingInterval % 1000));
    DebugOnly<nsresult> rv = mChannel->SetPingInterval(aPingInterval / 1000);
    MOZ_ASSERT(NS_SUCCEEDED(rv));
  }
  if (aClientSetPingTimeout) {
    MOZ_ASSERT(aPingTimeout >= 1000 && !(aPingTimeout % 1000));
    DebugOnly<nsresult> rv = mChannel->SetPingTimeout(aPingTimeout / 1000);
    MOZ_ASSERT(NS_SUCCEEDED(rv));
  }

  rv = mChannel->AsyncOpenNative(uri, aOrigin, aOriginAttributes,
                                 aInnerWindowID, this, nullptr);
  if (NS_FAILED(rv)) goto fail;

  return IPC_OK();

fail:
  mChannel = nullptr;
  if (!SendOnStop(rv)) {
    return IPC_FAIL_NO_REASON(this);
  }
  return IPC_OK();
}

mozilla::ipc::IPCResult WebSocketChannelParent::RecvClose(
    const uint16_t& code, const nsCString& reason) {
  LOG(("WebSocketChannelParent::RecvClose() %p\n", this));
  if (mChannel) {
    nsresult rv = mChannel->Close(code, reason);
    NS_ENSURE_SUCCESS(rv, IPC_OK());
  }

  return IPC_OK();
}

mozilla::ipc::IPCResult WebSocketChannelParent::RecvSendMsg(
    const nsCString& aMsg) {
  LOG(("WebSocketChannelParent::RecvSendMsg() %p\n", this));
  if (mChannel) {
    nsresult rv = mChannel->SendMsg(aMsg);
    NS_ENSURE_SUCCESS(rv, IPC_OK());
  }
  return IPC_OK();
}

mozilla::ipc::IPCResult WebSocketChannelParent::RecvSendBinaryMsg(
    const nsCString& aMsg) {
  LOG(("WebSocketChannelParent::RecvSendBinaryMsg() %p\n", this));
  if (mChannel) {
    nsresult rv = mChannel->SendBinaryMsg(aMsg);
    NS_ENSURE_SUCCESS(rv, IPC_OK());
  }
  return IPC_OK();
}

mozilla::ipc::IPCResult WebSocketChannelParent::RecvSendBinaryStream(
    const IPCStream& aStream, const uint32_t& aLength) {
  LOG(("WebSocketChannelParent::RecvSendBinaryStream() %p\n", this));
  if (mChannel) {
    nsCOMPtr<nsIInputStream> stream = DeserializeIPCStream(aStream);
    if (!stream) {
      return IPC_FAIL_NO_REASON(this);
    }
    nsresult rv = mChannel->SendBinaryStream(stream, aLength);
    NS_ENSURE_SUCCESS(rv, IPC_OK());
  }
  return IPC_OK();
}

//-----------------------------------------------------------------------------
// WebSocketChannelParent::nsIRequestObserver
//-----------------------------------------------------------------------------

NS_IMETHODIMP
WebSocketChannelParent::OnStart(nsISupports* aContext) {
  LOG(("WebSocketChannelParent::OnStart() %p\n", this));
  nsAutoCString protocol, extensions;
  nsString effectiveURL;
  bool encrypted = false;
  uint64_t httpChannelId = 0;
  if (mChannel) {
    DebugOnly<nsresult> rv = mChannel->GetProtocol(protocol);
    MOZ_ASSERT(NS_SUCCEEDED(rv));
    rv = mChannel->GetExtensions(extensions);
    MOZ_ASSERT(NS_SUCCEEDED(rv));

    RefPtr<WebSocketChannel> channel;
    channel = static_cast<WebSocketChannel*>(mChannel.get());
    MOZ_ASSERT(channel);

    channel->GetEffectiveURL(effectiveURL);
    encrypted = channel->IsEncrypted();
    httpChannelId = channel->HttpChannelId();
  }
  if (!CanRecv() || !SendOnStart(protocol, extensions, effectiveURL, encrypted,
                                 httpChannelId)) {
    return NS_ERROR_FAILURE;
  }
  return NS_OK;
}

NS_IMETHODIMP
WebSocketChannelParent::OnStop(nsISupports* aContext, nsresult aStatusCode) {
  LOG(("WebSocketChannelParent::OnStop() %p\n", this));
  if (!CanRecv() || !SendOnStop(aStatusCode)) {
    return NS_ERROR_FAILURE;
  }
  return NS_OK;
}

static bool SendOnMessageAvailableHelper(
    const nsACString& aMsg,
    const std::function<bool(const nsDependentCSubstring&, bool)>& aSendFunc) {
  // To avoid the crash caused by too large IPC message, we have to split the
  // data in small chunks and send them to child process. Note that the chunk
  // size used here is the same as what we used for PHttpChannel.
  static uint32_t const kCopyChunkSize = 128 * 1024;
  uint32_t count = aMsg.Length();
  if (count <= kCopyChunkSize) {
    return aSendFunc(nsDependentCSubstring(aMsg), false);
  }

  uint32_t start = 0;
  uint32_t toRead = std::min<uint32_t>(count, kCopyChunkSize);
  while (count) {
    nsDependentCSubstring data(Substring(aMsg, start, toRead));

    if (!aSendFunc(data, count > kCopyChunkSize)) {
      return false;
    }

    start += toRead;
    count -= toRead;
    toRead = std::min<uint32_t>(count, kCopyChunkSize);
  }

  return true;
}

NS_IMETHODIMP
WebSocketChannelParent::OnMessageAvailable(nsISupports* aContext,
                                           const nsACString& aMsg) {
  LOG(("WebSocketChannelParent::OnMessageAvailable() %p\n", this));

  if (!CanRecv()) {
    return NS_ERROR_FAILURE;
  }

  auto sendFunc = [self = UnsafePtr<WebSocketChannelParent>(this)](
                      const nsDependentCSubstring& aMsg, bool aMoreData) {
    return self->SendOnMessageAvailable(aMsg, aMoreData);
  };

  if (!SendOnMessageAvailableHelper(aMsg, sendFunc)) {
    return NS_ERROR_FAILURE;
  }

  return NS_OK;
}

NS_IMETHODIMP
WebSocketChannelParent::OnBinaryMessageAvailable(nsISupports* aContext,
                                                 const nsACString& aMsg) {
  LOG(("WebSocketChannelParent::OnBinaryMessageAvailable() %p\n", this));

  if (!CanRecv()) {
    return NS_ERROR_FAILURE;
  }

  auto sendFunc = [self = UnsafePtr<WebSocketChannelParent>(this)](
                      const nsDependentCSubstring& aMsg, bool aMoreData) {
    return self->SendOnBinaryMessageAvailable(aMsg, aMoreData);
  };

  if (!SendOnMessageAvailableHelper(aMsg, sendFunc)) {
    return NS_ERROR_FAILURE;
  }

  return NS_OK;
}

NS_IMETHODIMP
WebSocketChannelParent::OnAcknowledge(nsISupports* aContext, uint32_t aSize) {
  LOG(("WebSocketChannelParent::OnAcknowledge() %p\n", this));
  if (!CanRecv() || !SendOnAcknowledge(aSize)) {
    return NS_ERROR_FAILURE;
  }
  return NS_OK;
}

NS_IMETHODIMP
WebSocketChannelParent::OnServerClose(nsISupports* aContext, uint16_t code,
                                      const nsACString& reason) {
  LOG(("WebSocketChannelParent::OnServerClose() %p\n", this));
  if (!CanRecv() || !SendOnServerClose(code, nsCString(reason))) {
    return NS_ERROR_FAILURE;
  }
  return NS_OK;
}

NS_IMETHODIMP
WebSocketChannelParent::OnError() { return NS_OK; }

void WebSocketChannelParent::ActorDestroy(ActorDestroyReason why) {
  LOG(("WebSocketChannelParent::ActorDestroy() %p\n", this));

  // Make sure we close the channel if the content process dies without going
  // through a clean shutdown.
  if (mChannel) {
    Unused << mChannel->Close(nsIWebSocketChannel::CLOSE_GOING_AWAY,
                              "Child was killed"_ns);
  }
}

//-----------------------------------------------------------------------------
// WebSocketChannelParent::nsIInterfaceRequestor
//-----------------------------------------------------------------------------

NS_IMETHODIMP
WebSocketChannelParent::GetInterface(const nsIID& iid, void** result) {
  LOG(("WebSocketChannelParent::GetInterface() %p\n", this));
  if (mAuthProvider && iid.Equals(NS_GET_IID(nsIAuthPromptProvider))) {
    nsresult rv = mAuthProvider->GetAuthPrompt(
        nsIAuthPromptProvider::PROMPT_NORMAL, iid, result);
    if (NS_FAILED(rv)) {
      return NS_ERROR_NO_INTERFACE;
    }
    return NS_OK;
  }

  // Only support nsILoadContext if child channel's callbacks did too
  if (iid.Equals(NS_GET_IID(nsILoadContext)) && mLoadContext) {
    nsCOMPtr<nsILoadContext> copy = mLoadContext;
    copy.forget(result);
    return NS_OK;
  }

  return QueryInterface(iid, result);
}

}  // namespace net
}  // namespace mozilla
