/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "WebSocketFrame.h"
#include "WebSocketFrameListenerChild.h"
#include "WebSocketFrameService.h"

#include "mozilla/net/NeckoChild.h"
#include "mozilla/StaticPtr.h"
#include "nsISupportsPrimitives.h"
#include "nsXULAppAPI.h"

extern PRThread *gSocketThread;

namespace mozilla {
namespace net {

namespace {

StaticRefPtr<WebSocketFrameService> gWebSocketFrameService;

bool
IsChildProcess()
{
  return XRE_GetProcessType() != GeckoProcessType_Default;
}

} // anonymous namespace

class WebSocketFrameRunnable final : public nsRunnable
{
public:
  WebSocketFrameRunnable(uint32_t aWebSocketSerialID,
                         uint64_t aInnerWindowID,
                         WebSocketFrame* aFrame,
                         bool aFrameSent)
    : mWebSocketSerialID(aWebSocketSerialID)
    , mInnerWindowID(aInnerWindowID)
    , mFrame(aFrame)
    , mFrameSent(aFrameSent)
  {}

  NS_IMETHOD Run() override
  {
    MOZ_ASSERT(NS_IsMainThread());

    RefPtr<WebSocketFrameService> service =
      WebSocketFrameService::GetOrCreate();
    MOZ_ASSERT(service);

    WebSocketFrameService::WindowListeners* listeners =
      service->GetListeners(mInnerWindowID);
    if (!listeners) {
      return NS_OK;
    }

    nsresult rv;
    WebSocketFrameService::WindowListeners::ForwardIterator iter(*listeners);
    while (iter.HasMore()) {
      nsCOMPtr<nsIWebSocketFrameListener> listener = iter.GetNext();

      if (mFrameSent) {
        rv = listener->FrameSent(mWebSocketSerialID, mFrame);
      } else {
        rv = listener->FrameReceived(mWebSocketSerialID, mFrame);
      }

      NS_WARN_IF(NS_FAILED(rv));
    }

    return NS_OK;
  }

protected:
  ~WebSocketFrameRunnable()
  {}

  uint32_t mWebSocketSerialID;
  uint64_t mInnerWindowID;

  RefPtr<WebSocketFrame> mFrame;

  bool mFrameSent;
};

/* static */ already_AddRefed<WebSocketFrameService>
WebSocketFrameService::GetOrCreate()
{
  MOZ_ASSERT(NS_IsMainThread());

  if (!gWebSocketFrameService) {
    gWebSocketFrameService = new WebSocketFrameService();
  }

  RefPtr<WebSocketFrameService> service = gWebSocketFrameService.get();
  return service.forget();
}

NS_INTERFACE_MAP_BEGIN(WebSocketFrameService)
  NS_INTERFACE_MAP_ENTRY_AMBIGUOUS(nsISupports, nsIWebSocketFrameService)
  NS_INTERFACE_MAP_ENTRY(nsIObserver)
  NS_INTERFACE_MAP_ENTRY(nsIWebSocketFrameService)
NS_INTERFACE_MAP_END

NS_IMPL_ADDREF(WebSocketFrameService)
NS_IMPL_RELEASE(WebSocketFrameService)

WebSocketFrameService::WebSocketFrameService()
  : mCountListeners(0)
{
  MOZ_ASSERT(NS_IsMainThread());

  nsCOMPtr<nsIObserverService> obs = mozilla::services::GetObserverService();
  if (obs) {
    obs->AddObserver(this, "xpcom-shutdown", false);
    obs->AddObserver(this, "inner-window-destroyed", false);
  }
}

WebSocketFrameService::~WebSocketFrameService()
{
  MOZ_ASSERT(NS_IsMainThread());
}

void
WebSocketFrameService::FrameReceived(uint32_t aWebSocketSerialID,
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
WebSocketFrameService::FrameSent(uint32_t aWebSocketSerialID,
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
WebSocketFrameService::AddListener(uint64_t aInnerWindowID,
                                   nsIWebSocketFrameListener* aListener)
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
      PWebSocketFrameListenerChild* actor =
        gNeckoChild->SendPWebSocketFrameListenerConstructor(aInnerWindowID);

      listener->mActor = static_cast<WebSocketFrameListenerChild*>(actor);
      MOZ_ASSERT(listener->mActor);
    }

    mWindows.Put(aInnerWindowID, listener);
  }

  listener->mListeners.AppendElement(aListener);

  return NS_OK;
}

NS_IMETHODIMP
WebSocketFrameService::RemoveListener(uint64_t aInnerWindowID,
                                      nsIWebSocketFrameListener* aListener)
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
WebSocketFrameService::Observe(nsISupports* aSubject, const char* aTopic,
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
WebSocketFrameService::Shutdown()
{
  MOZ_ASSERT(NS_IsMainThread());

  if (gWebSocketFrameService) {
    nsCOMPtr<nsIObserverService> obs = mozilla::services::GetObserverService();
    if (obs) {
      obs->RemoveObserver(gWebSocketFrameService, "xpcom-shutdown");
      obs->RemoveObserver(gWebSocketFrameService, "inner-window-destroyed");
    }

    mWindows.Clear();
    gWebSocketFrameService = nullptr;
  }
}

bool
WebSocketFrameService::HasListeners() const
{
  return !!mCountListeners;
}

WebSocketFrameService::WindowListeners*
WebSocketFrameService::GetListeners(uint64_t aInnerWindowID) const
{
  WindowListener* listener = mWindows.Get(aInnerWindowID);
  return listener ? &listener->mListeners : nullptr;
}

void
WebSocketFrameService::ShutdownActorListener(WindowListener* aListener)
{
  MOZ_ASSERT(aListener);
  MOZ_ASSERT(aListener->mActor);
  aListener->mActor->Close();
  aListener->mActor = nullptr;
}

WebSocketFrame*
WebSocketFrameService::CreateFrameIfNeeded(bool aFinBit, bool aRsvBit1,
                                           bool aRsvBit2, bool aRsvBit3,
                                           uint8_t aOpCode, bool aMaskBit,
                                           uint32_t aMask,
                                           const nsCString& aPayload)
{
  if (!HasListeners()) {
    return nullptr;
  }

  return new WebSocketFrame(aFinBit, aRsvBit1, aRsvBit2, aRsvBit3, aOpCode,
                            aMaskBit, aMask, aPayload);
}

WebSocketFrame*
WebSocketFrameService::CreateFrameIfNeeded(bool aFinBit, bool aRsvBit1,
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

  return new WebSocketFrame(aFinBit, aRsvBit1, aRsvBit2, aRsvBit3, aOpCode,
                            aMaskBit, aMask, payloadStr);
}

WebSocketFrame*
WebSocketFrameService::CreateFrameIfNeeded(bool aFinBit, bool aRsvBit1,
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

  return new WebSocketFrame(aFinBit, aRsvBit1, aRsvBit2, aRsvBit3, aOpCode,
                            aMaskBit, aMask, payloadStr);
}

} // net namespace
} // mozilla namespace
