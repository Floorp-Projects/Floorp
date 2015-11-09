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
#include "nsXULAppAPI.h"
#include "nsSocketTransportService2.h"

namespace mozilla {
namespace net {

namespace {

StaticRefPtr<WebSocketEventService> gWebSocketEventService;

bool
IsChildProcess()
{
  return XRE_GetProcessType() != GeckoProcessType_Default;
}

} // anonymous namespace

class WebSocketBaseRunnable : public nsRunnable
{
public:
  WebSocketBaseRunnable(uint32_t aWebSocketSerialID,
                        uint64_t aInnerWindowID)
    : mWebSocketSerialID(aWebSocketSerialID)
    , mInnerWindowID(aInnerWindowID)
  {}

  NS_IMETHOD Run() override
  {
    MOZ_ASSERT(NS_IsMainThread());

    RefPtr<WebSocketEventService> service = WebSocketEventService::GetOrCreate();
    MOZ_ASSERT(service);

    WebSocketEventService::WindowListeners listeners;
    service->GetListeners(mInnerWindowID, listeners);

    for (uint32_t i = 0; i < listeners.Length(); ++i) {
      DoWork(listeners[i]);
    }

    return NS_OK;
  }

protected:
  ~WebSocketBaseRunnable()
  {}

  virtual void DoWork(nsIWebSocketEventListener* aListener) = 0;

  uint32_t mWebSocketSerialID;
  uint64_t mInnerWindowID;
};

class WebSocketFrameRunnable final : public WebSocketBaseRunnable
{
public:
  WebSocketFrameRunnable(uint32_t aWebSocketSerialID,
                         uint64_t aInnerWindowID,
                         WebSocketFrame* aFrame,
                         bool aFrameSent)
    : WebSocketBaseRunnable(aWebSocketSerialID, aInnerWindowID)
    , mFrame(aFrame)
    , mFrameSent(aFrameSent)
  {}

private:
  virtual void DoWork(nsIWebSocketEventListener* aListener) override
  {
    nsresult rv;

    if (mFrameSent) {
      rv = aListener->FrameSent(mWebSocketSerialID, mFrame);
    } else {
      rv = aListener->FrameReceived(mWebSocketSerialID, mFrame);
    }

    NS_WARN_IF(NS_FAILED(rv));
  }

  RefPtr<WebSocketFrame> mFrame;
  bool mFrameSent;
};

class WebSocketCreatedRunnable final : public WebSocketBaseRunnable
{
public:
  WebSocketCreatedRunnable(uint32_t aWebSocketSerialID,
                           uint64_t aInnerWindowID,
                           const nsAString& aURI,
                           const nsACString& aProtocols)
    : WebSocketBaseRunnable(aWebSocketSerialID, aInnerWindowID)
    , mURI(aURI)
    , mProtocols(aProtocols)
  {}

private:
  virtual void DoWork(nsIWebSocketEventListener* aListener) override
  {
    nsresult rv = aListener->WebSocketCreated(mWebSocketSerialID,
                                             mURI, mProtocols);
    NS_WARN_IF(NS_FAILED(rv));
  }

  const nsString mURI;
  const nsCString mProtocols;
};

class WebSocketOpenedRunnable final : public WebSocketBaseRunnable
{
public:
  WebSocketOpenedRunnable(uint32_t aWebSocketSerialID,
                           uint64_t aInnerWindowID,
                           const nsAString& aEffectiveURI,
                           const nsACString& aProtocols,
                           const nsACString& aExtensions)
    : WebSocketBaseRunnable(aWebSocketSerialID, aInnerWindowID)
    , mEffectiveURI(aEffectiveURI)
    , mProtocols(aProtocols)
    , mExtensions(aExtensions)
  {}

private:
  virtual void DoWork(nsIWebSocketEventListener* aListener) override
  {
    nsresult rv = aListener->WebSocketOpened(mWebSocketSerialID,
                                             mEffectiveURI,
                                             mProtocols,
                                             mExtensions);
    NS_WARN_IF(NS_FAILED(rv));
  }

  const nsString mEffectiveURI;
  const nsCString mProtocols;
  const nsCString mExtensions;
};

class WebSocketMessageAvailableRunnable final : public WebSocketBaseRunnable
{
public:
  WebSocketMessageAvailableRunnable(uint32_t aWebSocketSerialID,
                          uint64_t aInnerWindowID,
                          const nsACString& aData,
                          uint16_t aMessageType)
    : WebSocketBaseRunnable(aWebSocketSerialID, aInnerWindowID)
    , mData(aData)
    , mMessageType(aMessageType)
  {}

private:
  virtual void DoWork(nsIWebSocketEventListener* aListener) override
  {
    nsresult rv = aListener->WebSocketMessageAvailable(mWebSocketSerialID,
                                                       mData, mMessageType);
    NS_WARN_IF(NS_FAILED(rv));
  }

  const nsCString mData;
  uint16_t mMessageType;
};

class WebSocketClosedRunnable final : public WebSocketBaseRunnable
{
public:
  WebSocketClosedRunnable(uint32_t aWebSocketSerialID,
                          uint64_t aInnerWindowID,
                          bool aWasClean,
                          uint16_t aCode,
                          const nsAString& aReason)
    : WebSocketBaseRunnable(aWebSocketSerialID, aInnerWindowID)
    , mWasClean(aWasClean)
    , mCode(aCode)
    , mReason(aReason)
  {}

private:
  virtual void DoWork(nsIWebSocketEventListener* aListener) override
  {
    nsresult rv = aListener->WebSocketClosed(mWebSocketSerialID,
                                             mWasClean, mCode, mReason);
    NS_WARN_IF(NS_FAILED(rv));
  }

  bool mWasClean;
  uint16_t mCode;
  const nsString mReason;
};

/* static */ already_AddRefed<WebSocketEventService>
WebSocketEventService::GetOrCreate()
{
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

WebSocketEventService::WebSocketEventService()
  : mCountListeners(0)
{
  MOZ_ASSERT(NS_IsMainThread());

  nsCOMPtr<nsIObserverService> obs = mozilla::services::GetObserverService();
  if (obs) {
    obs->AddObserver(this, "xpcom-shutdown", false);
    obs->AddObserver(this, "inner-window-destroyed", false);
  }
}

WebSocketEventService::~WebSocketEventService()
{
  MOZ_ASSERT(NS_IsMainThread());
}

void
WebSocketEventService::WebSocketCreated(uint32_t aWebSocketSerialID,
                                        uint64_t aInnerWindowID,
                                        const nsAString& aURI,
                                        const nsACString& aProtocols)
{
  // Let's continue only if we have some listeners.
  if (!HasListeners()) {
    return;
  }

  RefPtr<WebSocketCreatedRunnable> runnable =
    new WebSocketCreatedRunnable(aWebSocketSerialID, aInnerWindowID,
                                 aURI, aProtocols);
  nsresult rv = NS_DispatchToMainThread(runnable);
  NS_WARN_IF(NS_FAILED(rv));
}

void
WebSocketEventService::WebSocketOpened(uint32_t aWebSocketSerialID,
                                       uint64_t aInnerWindowID,
                                       const nsAString& aEffectiveURI,
                                       const nsACString& aProtocols,
                                       const nsACString& aExtensions)
{
  // Let's continue only if we have some listeners.
  if (!HasListeners()) {
    return;
  }

  RefPtr<WebSocketOpenedRunnable> runnable =
    new WebSocketOpenedRunnable(aWebSocketSerialID, aInnerWindowID,
                                aEffectiveURI, aProtocols, aExtensions);
  nsresult rv = NS_DispatchToMainThread(runnable);
  NS_WARN_IF(NS_FAILED(rv));
}

void
WebSocketEventService::WebSocketMessageAvailable(uint32_t aWebSocketSerialID,
                                                 uint64_t aInnerWindowID,
                                                 const nsACString& aData,
                                                 uint16_t aMessageType)
{
  // Let's continue only if we have some listeners.
  if (!HasListeners()) {
    return;
  }

  RefPtr<WebSocketMessageAvailableRunnable> runnable =
    new WebSocketMessageAvailableRunnable(aWebSocketSerialID, aInnerWindowID,
                                          aData, aMessageType);
  nsresult rv = NS_DispatchToMainThread(runnable);
  NS_WARN_IF(NS_FAILED(rv));
}

void
WebSocketEventService::WebSocketClosed(uint32_t aWebSocketSerialID,
                                       uint64_t aInnerWindowID,
                                       bool aWasClean,
                                       uint16_t aCode,
                                       const nsAString& aReason)
{
  // Let's continue only if we have some listeners.
  if (!HasListeners()) {
    return;
  }

  RefPtr<WebSocketClosedRunnable> runnable =
    new WebSocketClosedRunnable(aWebSocketSerialID, aInnerWindowID,
                                aWasClean, aCode, aReason);
  nsresult rv = NS_DispatchToMainThread(runnable);
  NS_WARN_IF(NS_FAILED(rv));
}

void
WebSocketEventService::FrameReceived(uint32_t aWebSocketSerialID,
                                     uint64_t aInnerWindowID,
                                     WebSocketFrame* aFrame)
{
  MOZ_ASSERT(aFrame);

  // Let's continue only if we have some listeners.
  if (!HasListeners()) {
    return;
  }

  RefPtr<WebSocketFrameRunnable> runnable =
    new WebSocketFrameRunnable(aWebSocketSerialID, aInnerWindowID,
                               aFrame, false /* frameSent */);
  nsresult rv = NS_DispatchToMainThread(runnable);
  NS_WARN_IF(NS_FAILED(rv));
}

void
WebSocketEventService::FrameSent(uint32_t aWebSocketSerialID,
                                 uint64_t aInnerWindowID,
                                 WebSocketFrame* aFrame)
{
  MOZ_ASSERT(aFrame);

  // Let's continue only if we have some listeners.
  if (!HasListeners()) {
    return;
  }

  RefPtr<WebSocketFrameRunnable> runnable =
    new WebSocketFrameRunnable(aWebSocketSerialID, aInnerWindowID,
                               aFrame, true /* frameSent */);

  nsresult rv = NS_DispatchToMainThread(runnable);
  NS_WARN_IF(NS_FAILED(rv));
}

NS_IMETHODIMP
WebSocketEventService::AddListener(uint64_t aInnerWindowID,
                                   nsIWebSocketEventListener* aListener)
{
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
                                      nsIWebSocketEventListener* aListener)
{
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
WebSocketEventService::Observe(nsISupports* aSubject, const char* aTopic,
                               const char16_t* aData)
{
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

void
WebSocketEventService::Shutdown()
{
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

bool
WebSocketEventService::HasListeners() const
{
  return !!mCountListeners;
}

void
WebSocketEventService::GetListeners(uint64_t aInnerWindowID,
                                    WebSocketEventService::WindowListeners& aListeners) const
{
  aListeners.Clear();

  WindowListener* listener = mWindows.Get(aInnerWindowID);
  if (!listener) {
    return;
  }

  aListeners.AppendElements(listener->mListeners);
}

void
WebSocketEventService::ShutdownActorListener(WindowListener* aListener)
{
  MOZ_ASSERT(aListener);
  MOZ_ASSERT(aListener->mActor);
  aListener->mActor->Close();
  aListener->mActor = nullptr;
}

already_AddRefed<WebSocketFrame>
WebSocketEventService::CreateFrameIfNeeded(bool aFinBit, bool aRsvBit1,
                                           bool aRsvBit2, bool aRsvBit3,
                                           uint8_t aOpCode, bool aMaskBit,
                                           uint32_t aMask,
                                           const nsCString& aPayload)
{
  if (!HasListeners()) {
    return nullptr;
  }

  return MakeAndAddRef<WebSocketFrame>(aFinBit, aRsvBit1, aRsvBit2, aRsvBit3,
                                       aOpCode, aMaskBit, aMask, aPayload);
}

already_AddRefed<WebSocketFrame>
WebSocketEventService::CreateFrameIfNeeded(bool aFinBit, bool aRsvBit1,
                                           bool aRsvBit2, bool aRsvBit3,
                                           uint8_t aOpCode, bool aMaskBit,
                                           uint32_t aMask, uint8_t* aPayload,
                                           uint32_t aPayloadLength)
{
  if (!HasListeners()) {
    return nullptr;
  }

  nsAutoCString payloadStr;
  if (NS_WARN_IF(!(payloadStr.Assign((const char*) aPayload, aPayloadLength,
                                     mozilla::fallible)))) {
    return nullptr;
  }

  return MakeAndAddRef<WebSocketFrame>(aFinBit, aRsvBit1, aRsvBit2, aRsvBit3,
                                       aOpCode, aMaskBit, aMask, payloadStr);
}

already_AddRefed<WebSocketFrame>
WebSocketEventService::CreateFrameIfNeeded(bool aFinBit, bool aRsvBit1,
                                           bool aRsvBit2, bool aRsvBit3,
                                           uint8_t aOpCode, bool aMaskBit,
                                           uint32_t aMask,
                                           uint8_t* aPayloadInHdr,
                                           uint32_t aPayloadInHdrLength,
                                           uint8_t* aPayload,
                                           uint32_t aPayloadLength)
{
  if (!HasListeners()) {
    return nullptr;
  }

  uint32_t payloadLength = aPayloadLength + aPayloadInHdrLength;

  nsAutoArrayPtr<uint8_t> payload(new uint8_t[payloadLength]);
  if (NS_WARN_IF(!payload)) {
    return nullptr;
  }

  if (aPayloadInHdrLength) {
    memcpy(payload, aPayloadInHdr, aPayloadInHdrLength);
  }

  memcpy(payload + aPayloadInHdrLength, aPayload, aPayloadLength);

  nsAutoCString payloadStr;
  if (NS_WARN_IF(!(payloadStr.Assign((const char*) payload.get(), payloadLength,
                                     mozilla::fallible)))) {
    return nullptr;
  }

  return MakeAndAddRef<WebSocketFrame>(aFinBit, aRsvBit1, aRsvBit2, aRsvBit3,
                                       aOpCode, aMaskBit, aMask, payloadStr);
}

} // net namespace
} // mozilla namespace
