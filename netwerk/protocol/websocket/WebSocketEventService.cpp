/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "WebSocketEventListenerChild.h"
#include "WebSocketEventService.h"
#include "WebSocketFrame.h"

#include "mozilla/net/NeckoChild.h"
#include "mozilla/StaticPtr.h"
#include "nsISupportsPrimitives.h"
#include "nsIObserverService.h"
#include "nsXULAppAPI.h"
#include "nsSocketTransportService2.h"
#include "nsThreadUtils.h"
#include "mozilla/Services.h"

namespace mozilla {
namespace net {

namespace {

StaticRefPtr<WebSocketEventService> gWebSocketEventService;

bool IsChildProcess() {
  return XRE_GetProcessType() != GeckoProcessType_Default &&
    // Middleman processes are not Necko children.
    !recordreplay::IsMiddleman();
}

}  // anonymous namespace

class WebSocketBaseRunnable : public Runnable {
 public:
  WebSocketBaseRunnable(uint32_t aWebSocketSerialID, uint64_t aInnerWindowID)
      : Runnable("net::WebSocketBaseRunnable"),
        mWebSocketSerialID(aWebSocketSerialID),
        mInnerWindowID(aInnerWindowID) {}

  NS_IMETHOD Run() override {
    MOZ_ASSERT(NS_IsMainThread());

    RefPtr<WebSocketEventService> service =
        WebSocketEventService::GetOrCreate();
    MOZ_ASSERT(service);

    WebSocketEventService::WindowListeners listeners;
    service->GetListeners(mInnerWindowID, listeners);

    for (uint32_t i = 0; i < listeners.Length(); ++i) {
      DoWork(listeners[i]);
    }

    return NS_OK;
  }

 protected:
  ~WebSocketBaseRunnable() = default;

  virtual void DoWork(nsIWebSocketEventListener* aListener) = 0;

  uint32_t mWebSocketSerialID;
  uint64_t mInnerWindowID;
};

class WebSocketFrameRunnable final : public WebSocketBaseRunnable {
 public:
  WebSocketFrameRunnable(uint32_t aWebSocketSerialID, uint64_t aInnerWindowID,
                         already_AddRefed<WebSocketFrame> aFrame,
                         bool aFrameSent)
      : WebSocketBaseRunnable(aWebSocketSerialID, aInnerWindowID),
        mFrame(std::move(aFrame)),
        mFrameSent(aFrameSent) {}

 private:
  virtual void DoWork(nsIWebSocketEventListener* aListener) override {
    DebugOnly<nsresult> rv;
    if (mFrameSent) {
      rv = aListener->FrameSent(mWebSocketSerialID, mFrame);
    } else {
      rv = aListener->FrameReceived(mWebSocketSerialID, mFrame);
    }

    NS_WARNING_ASSERTION(NS_SUCCEEDED(rv), "Frame op failed");
  }

  RefPtr<WebSocketFrame> mFrame;
  bool mFrameSent;
};

class WebSocketCreatedRunnable final : public WebSocketBaseRunnable {
 public:
  WebSocketCreatedRunnable(uint32_t aWebSocketSerialID, uint64_t aInnerWindowID,
                           const nsAString& aURI, const nsACString& aProtocols)
      : WebSocketBaseRunnable(aWebSocketSerialID, aInnerWindowID),
        mURI(aURI),
        mProtocols(aProtocols) {}

 private:
  virtual void DoWork(nsIWebSocketEventListener* aListener) override {
    DebugOnly<nsresult> rv =
        aListener->WebSocketCreated(mWebSocketSerialID, mURI, mProtocols);
    NS_WARNING_ASSERTION(NS_SUCCEEDED(rv), "WebSocketCreated failed");
  }

  const nsString mURI;
  const nsCString mProtocols;
};

class WebSocketOpenedRunnable final : public WebSocketBaseRunnable {
 public:
  WebSocketOpenedRunnable(uint32_t aWebSocketSerialID, uint64_t aInnerWindowID,
                          const nsAString& aEffectiveURI,
                          const nsACString& aProtocols,
                          const nsACString& aExtensions,
                          uint64_t aHttpChannelId)
      : WebSocketBaseRunnable(aWebSocketSerialID, aInnerWindowID),
        mEffectiveURI(aEffectiveURI),
        mProtocols(aProtocols),
        mExtensions(aExtensions),
        mHttpChannelId(aHttpChannelId) {}

 private:
  virtual void DoWork(nsIWebSocketEventListener* aListener) override {
    DebugOnly<nsresult> rv =
        aListener->WebSocketOpened(mWebSocketSerialID, mEffectiveURI,
                                   mProtocols, mExtensions, mHttpChannelId);
    NS_WARNING_ASSERTION(NS_SUCCEEDED(rv), "WebSocketOpened failed");
  }

  const nsString mEffectiveURI;
  const nsCString mProtocols;
  const nsCString mExtensions;
  uint64_t mHttpChannelId;
};

class WebSocketMessageAvailableRunnable final : public WebSocketBaseRunnable {
 public:
  WebSocketMessageAvailableRunnable(uint32_t aWebSocketSerialID,
                                    uint64_t aInnerWindowID,
                                    const nsACString& aData,
                                    uint16_t aMessageType)
      : WebSocketBaseRunnable(aWebSocketSerialID, aInnerWindowID),
        mData(aData),
        mMessageType(aMessageType) {}

 private:
  virtual void DoWork(nsIWebSocketEventListener* aListener) override {
    DebugOnly<nsresult> rv = aListener->WebSocketMessageAvailable(
        mWebSocketSerialID, mData, mMessageType);
    NS_WARNING_ASSERTION(NS_SUCCEEDED(rv), "WebSocketMessageAvailable failed");
  }

  const nsCString mData;
  uint16_t mMessageType;
};

class WebSocketClosedRunnable final : public WebSocketBaseRunnable {
 public:
  WebSocketClosedRunnable(uint32_t aWebSocketSerialID, uint64_t aInnerWindowID,
                          bool aWasClean, uint16_t aCode,
                          const nsAString& aReason)
      : WebSocketBaseRunnable(aWebSocketSerialID, aInnerWindowID),
        mWasClean(aWasClean),
        mCode(aCode),
        mReason(aReason) {}

 private:
  virtual void DoWork(nsIWebSocketEventListener* aListener) override {
    DebugOnly<nsresult> rv = aListener->WebSocketClosed(
        mWebSocketSerialID, mWasClean, mCode, mReason);
    NS_WARNING_ASSERTION(NS_SUCCEEDED(rv), "WebSocketClosed failed");
  }

  bool mWasClean;
  uint16_t mCode;
  const nsString mReason;
};

/* static */
already_AddRefed<WebSocketEventService> WebSocketEventService::Get() {
  MOZ_ASSERT(NS_IsMainThread());
  RefPtr<WebSocketEventService> service = gWebSocketEventService.get();
  return service.forget();
}

/* static */
already_AddRefed<WebSocketEventService> WebSocketEventService::GetOrCreate() {
  MOZ_ASSERT(NS_IsMainThread());

  if (!gWebSocketEventService) {
    gWebSocketEventService = new WebSocketEventService();
  }

  RefPtr<WebSocketEventService> service = gWebSocketEventService.get();
  return service.forget();
}

NS_INTERFACE_MAP_BEGIN(WebSocketEventService)
  NS_INTERFACE_MAP_ENTRY_AMBIGUOUS(nsISupports, nsIWebSocketEventService)
  NS_INTERFACE_MAP_ENTRY(nsIObserver)
  NS_INTERFACE_MAP_ENTRY(nsIWebSocketEventService)
NS_INTERFACE_MAP_END

NS_IMPL_ADDREF(WebSocketEventService)
NS_IMPL_RELEASE(WebSocketEventService)

WebSocketEventService::WebSocketEventService() : mCountListeners(0) {
  MOZ_ASSERT(NS_IsMainThread());

  nsCOMPtr<nsIObserverService> obs = mozilla::services::GetObserverService();
  if (obs) {
    obs->AddObserver(this, "xpcom-shutdown", false);
    obs->AddObserver(this, "inner-window-destroyed", false);
  }
}

WebSocketEventService::~WebSocketEventService() {
  MOZ_ASSERT(NS_IsMainThread());
}

void WebSocketEventService::WebSocketCreated(uint32_t aWebSocketSerialID,
                                             uint64_t aInnerWindowID,
                                             const nsAString& aURI,
                                             const nsACString& aProtocols,
                                             nsIEventTarget* aTarget) {
  // Let's continue only if we have some listeners.
  if (!HasListeners()) {
    return;
  }

  RefPtr<WebSocketCreatedRunnable> runnable = new WebSocketCreatedRunnable(
      aWebSocketSerialID, aInnerWindowID, aURI, aProtocols);
  DebugOnly<nsresult> rv = aTarget
                               ? aTarget->Dispatch(runnable, NS_DISPATCH_NORMAL)
                               : NS_DispatchToMainThread(runnable);
  NS_WARNING_ASSERTION(NS_SUCCEEDED(rv), "NS_DispatchToMainThread failed");
}

void WebSocketEventService::WebSocketOpened(uint32_t aWebSocketSerialID,
                                            uint64_t aInnerWindowID,
                                            const nsAString& aEffectiveURI,
                                            const nsACString& aProtocols,
                                            const nsACString& aExtensions,
                                            uint64_t aHttpChannelId,
                                            nsIEventTarget* aTarget) {
  // Let's continue only if we have some listeners.
  if (!HasListeners()) {
    return;
  }

  RefPtr<WebSocketOpenedRunnable> runnable = new WebSocketOpenedRunnable(
      aWebSocketSerialID, aInnerWindowID, aEffectiveURI, aProtocols,
      aExtensions, aHttpChannelId);
  DebugOnly<nsresult> rv = aTarget
                               ? aTarget->Dispatch(runnable, NS_DISPATCH_NORMAL)
                               : NS_DispatchToMainThread(runnable);
  NS_WARNING_ASSERTION(NS_SUCCEEDED(rv), "NS_DispatchToMainThread failed");
}

void WebSocketEventService::WebSocketMessageAvailable(
    uint32_t aWebSocketSerialID, uint64_t aInnerWindowID,
    const nsACString& aData, uint16_t aMessageType, nsIEventTarget* aTarget) {
  // Let's continue only if we have some listeners.
  if (!HasListeners()) {
    return;
  }

  RefPtr<WebSocketMessageAvailableRunnable> runnable =
      new WebSocketMessageAvailableRunnable(aWebSocketSerialID, aInnerWindowID,
                                            aData, aMessageType);
  DebugOnly<nsresult> rv = aTarget
                               ? aTarget->Dispatch(runnable, NS_DISPATCH_NORMAL)
                               : NS_DispatchToMainThread(runnable);
  NS_WARNING_ASSERTION(NS_SUCCEEDED(rv), "NS_DispatchToMainThread failed");
}

void WebSocketEventService::WebSocketClosed(uint32_t aWebSocketSerialID,
                                            uint64_t aInnerWindowID,
                                            bool aWasClean, uint16_t aCode,
                                            const nsAString& aReason,
                                            nsIEventTarget* aTarget) {
  // Let's continue only if we have some listeners.
  if (!HasListeners()) {
    return;
  }

  RefPtr<WebSocketClosedRunnable> runnable = new WebSocketClosedRunnable(
      aWebSocketSerialID, aInnerWindowID, aWasClean, aCode, aReason);
  DebugOnly<nsresult> rv = aTarget
                               ? aTarget->Dispatch(runnable, NS_DISPATCH_NORMAL)
                               : NS_DispatchToMainThread(runnable);
  NS_WARNING_ASSERTION(NS_SUCCEEDED(rv), "NS_DispatchToMainThread failed");
}

void WebSocketEventService::FrameReceived(
    uint32_t aWebSocketSerialID, uint64_t aInnerWindowID,
    already_AddRefed<WebSocketFrame> aFrame, nsIEventTarget* aTarget) {
  RefPtr<WebSocketFrame> frame(std::move(aFrame));
  MOZ_ASSERT(frame);

  // Let's continue only if we have some listeners.
  if (!HasListeners()) {
    return;
  }

  RefPtr<WebSocketFrameRunnable> runnable =
      new WebSocketFrameRunnable(aWebSocketSerialID, aInnerWindowID,
                                 frame.forget(), false /* frameSent */);
  DebugOnly<nsresult> rv = aTarget
                               ? aTarget->Dispatch(runnable, NS_DISPATCH_NORMAL)
                               : NS_DispatchToMainThread(runnable);
  NS_WARNING_ASSERTION(NS_SUCCEEDED(rv), "NS_DispatchToMainThread failed");
}

void WebSocketEventService::FrameSent(uint32_t aWebSocketSerialID,
                                      uint64_t aInnerWindowID,
                                      already_AddRefed<WebSocketFrame> aFrame,
                                      nsIEventTarget* aTarget) {
  RefPtr<WebSocketFrame> frame(std::move(aFrame));
  MOZ_ASSERT(frame);

  // Let's continue only if we have some listeners.
  if (!HasListeners()) {
    return;
  }

  RefPtr<WebSocketFrameRunnable> runnable = new WebSocketFrameRunnable(
      aWebSocketSerialID, aInnerWindowID, frame.forget(), true /* frameSent */);

  DebugOnly<nsresult> rv = aTarget
                               ? aTarget->Dispatch(runnable, NS_DISPATCH_NORMAL)
                               : NS_DispatchToMainThread(runnable);
  NS_WARNING_ASSERTION(NS_SUCCEEDED(rv), "NS_DispatchToMainThread failed");
}

NS_IMETHODIMP
WebSocketEventService::AddListener(uint64_t aInnerWindowID,
                                   nsIWebSocketEventListener* aListener) {
  MOZ_ASSERT(NS_IsMainThread());

  if (!aListener) {
    return NS_ERROR_FAILURE;
  }

  ++mCountListeners;

  WindowListener* listener = mWindows.Get(aInnerWindowID);
  if (!listener) {
    listener = new WindowListener();

    if (IsChildProcess()) {
      PWebSocketEventListenerChild* actor =
          gNeckoChild->SendPWebSocketEventListenerConstructor(aInnerWindowID);

      listener->mActor = static_cast<WebSocketEventListenerChild*>(actor);
      MOZ_ASSERT(listener->mActor);
    }

    mWindows.Put(aInnerWindowID, listener);
  }

  listener->mListeners.AppendElement(aListener);

  return NS_OK;
}

NS_IMETHODIMP
WebSocketEventService::RemoveListener(uint64_t aInnerWindowID,
                                      nsIWebSocketEventListener* aListener) {
  MOZ_ASSERT(NS_IsMainThread());

  if (!aListener) {
    return NS_ERROR_FAILURE;
  }

  WindowListener* listener = mWindows.Get(aInnerWindowID);
  if (!listener) {
    return NS_ERROR_FAILURE;
  }

  if (!listener->mListeners.RemoveElement(aListener)) {
    return NS_ERROR_FAILURE;
  }

  // The last listener for this window.
  if (listener->mListeners.IsEmpty()) {
    if (IsChildProcess()) {
      ShutdownActorListener(listener);
    }

    mWindows.Remove(aInnerWindowID);
  }

  MOZ_ASSERT(mCountListeners);
  --mCountListeners;

  return NS_OK;
}

NS_IMETHODIMP
WebSocketEventService::HasListenerFor(uint64_t aInnerWindowID, bool* aResult) {
  MOZ_ASSERT(NS_IsMainThread());

  *aResult = mWindows.Get(aInnerWindowID);

  return NS_OK;
}

NS_IMETHODIMP
WebSocketEventService::Observe(nsISupports* aSubject, const char* aTopic,
                               const char16_t* aData) {
  MOZ_ASSERT(NS_IsMainThread());

  if (!strcmp(aTopic, "xpcom-shutdown")) {
    Shutdown();
    return NS_OK;
  }

  if (!strcmp(aTopic, "inner-window-destroyed") && HasListeners()) {
    nsCOMPtr<nsISupportsPRUint64> wrapper = do_QueryInterface(aSubject);
    NS_ENSURE_TRUE(wrapper, NS_ERROR_FAILURE);

    uint64_t innerID;
    nsresult rv = wrapper->GetData(&innerID);
    NS_ENSURE_SUCCESS(rv, rv);

    WindowListener* listener = mWindows.Get(innerID);
    if (!listener) {
      return NS_OK;
    }

    MOZ_ASSERT(mCountListeners >= listener->mListeners.Length());
    mCountListeners -= listener->mListeners.Length();

    if (IsChildProcess()) {
      ShutdownActorListener(listener);
    }

    mWindows.Remove(innerID);
  }

  // This should not happen.
  return NS_ERROR_FAILURE;
}

void WebSocketEventService::Shutdown() {
  MOZ_ASSERT(NS_IsMainThread());

  if (gWebSocketEventService) {
    nsCOMPtr<nsIObserverService> obs = mozilla::services::GetObserverService();
    if (obs) {
      obs->RemoveObserver(gWebSocketEventService, "xpcom-shutdown");
      obs->RemoveObserver(gWebSocketEventService, "inner-window-destroyed");
    }

    mWindows.Clear();
    gWebSocketEventService = nullptr;
  }
}

bool WebSocketEventService::HasListeners() const { return !!mCountListeners; }

void WebSocketEventService::GetListeners(
    uint64_t aInnerWindowID,
    WebSocketEventService::WindowListeners& aListeners) const {
  aListeners.Clear();

  WindowListener* listener = mWindows.Get(aInnerWindowID);
  if (!listener) {
    return;
  }

  aListeners.AppendElements(listener->mListeners);
}

void WebSocketEventService::ShutdownActorListener(WindowListener* aListener) {
  MOZ_ASSERT(aListener);
  MOZ_ASSERT(aListener->mActor);
  aListener->mActor->Close();
  aListener->mActor = nullptr;
}

already_AddRefed<WebSocketFrame> WebSocketEventService::CreateFrameIfNeeded(
    bool aFinBit, bool aRsvBit1, bool aRsvBit2, bool aRsvBit3, uint8_t aOpCode,
    bool aMaskBit, uint32_t aMask, const nsCString& aPayload) {
  if (!HasListeners()) {
    return nullptr;
  }

  return MakeAndAddRef<WebSocketFrame>(aFinBit, aRsvBit1, aRsvBit2, aRsvBit3,
                                       aOpCode, aMaskBit, aMask, aPayload);
}

already_AddRefed<WebSocketFrame> WebSocketEventService::CreateFrameIfNeeded(
    bool aFinBit, bool aRsvBit1, bool aRsvBit2, bool aRsvBit3, uint8_t aOpCode,
    bool aMaskBit, uint32_t aMask, uint8_t* aPayload, uint32_t aPayloadLength) {
  if (!HasListeners()) {
    return nullptr;
  }

  nsAutoCString payloadStr;
  if (NS_WARN_IF(!(payloadStr.Assign((const char*)aPayload, aPayloadLength,
                                     mozilla::fallible)))) {
    return nullptr;
  }

  return MakeAndAddRef<WebSocketFrame>(aFinBit, aRsvBit1, aRsvBit2, aRsvBit3,
                                       aOpCode, aMaskBit, aMask, payloadStr);
}

already_AddRefed<WebSocketFrame> WebSocketEventService::CreateFrameIfNeeded(
    bool aFinBit, bool aRsvBit1, bool aRsvBit2, bool aRsvBit3, uint8_t aOpCode,
    bool aMaskBit, uint32_t aMask, uint8_t* aPayloadInHdr,
    uint32_t aPayloadInHdrLength, uint8_t* aPayload, uint32_t aPayloadLength) {
  if (!HasListeners()) {
    return nullptr;
  }

  uint32_t payloadLength = aPayloadLength + aPayloadInHdrLength;

  nsAutoCString payload;
  if (NS_WARN_IF(!payload.SetLength(payloadLength, fallible))) {
    return nullptr;
  }

  char* payloadPtr = payload.BeginWriting();
  if (aPayloadInHdrLength) {
    memcpy(payloadPtr, aPayloadInHdr, aPayloadInHdrLength);
  }

  memcpy(payloadPtr + aPayloadInHdrLength, aPayload, aPayloadLength);

  return MakeAndAddRef<WebSocketFrame>(aFinBit, aRsvBit1, aRsvBit2, aRsvBit3,
                                       aOpCode, aMaskBit, aMask, payload);
}

}  // namespace net
}  // namespace mozilla
