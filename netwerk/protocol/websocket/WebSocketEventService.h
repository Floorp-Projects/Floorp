/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_net_WebSocketEventService_h
#define mozilla_net_WebSocketEventService_h

#include "mozilla/AlreadyAddRefed.h"
#include "mozilla/Atomics.h"
#include "nsIWebSocketEventService.h"
#include "nsAutoPtr.h"
#include "nsCOMPtr.h"
#include "nsClassHashtable.h"
#include "nsHashKeys.h"
#include "nsIObserver.h"
#include "nsISupportsImpl.h"
#include "nsTArray.h"

namespace mozilla {
namespace net {

class WebSocketFrame;
class WebSocketEventListenerChild;

class WebSocketEventService final : public nsIWebSocketEventService
                                  , public nsIObserver
{
  friend class WebSocketBaseRunnable;

public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSIOBSERVER
  NS_DECL_NSIWEBSOCKETEVENTSERVICE

  static already_AddRefed<WebSocketEventService> GetOrCreate();

  void WebSocketCreated(uint32_t aWebSocketSerialID,
                        uint64_t aInnerWindowID,
                        const nsAString& aURI,
                        const nsACString& aProtocols,
                        nsIEventTarget* aTarget = nullptr);

  void WebSocketOpened(uint32_t aWebSocketSerialID,
                       uint64_t aInnerWindowID,
                       const nsAString& aEffectiveURI,
                       const nsACString& aProtocols,
                       const nsACString& aExtensions,
                       nsIEventTarget* aTarget = nullptr);

  void WebSocketMessageAvailable(uint32_t aWebSocketSerialID,
                                 uint64_t aInnerWindowID,
                                 const nsACString& aData,
                                 uint16_t aMessageType,
                                 nsIEventTarget* aTarget = nullptr);

  void WebSocketClosed(uint32_t aWebSocketSerialID,
                       uint64_t aInnerWindowID,
                       bool aWasClean,
                       uint16_t aCode,
                       const nsAString& aReason,
                       nsIEventTarget* aTarget = nullptr);

  void FrameReceived(uint32_t aWebSocketSerialID,
                     uint64_t aInnerWindowID,
                     already_AddRefed<WebSocketFrame> aFrame,
                     nsIEventTarget* aTarget = nullptr);

  void  FrameSent(uint32_t aWebSocketSerialID,
                  uint64_t aInnerWindowID,
                  already_AddRefed<WebSocketFrame> aFrame,
                  nsIEventTarget* aTarget = nullptr);

  already_AddRefed<WebSocketFrame>
  CreateFrameIfNeeded(bool aFinBit, bool aRsvBit1, bool aRsvBit2, bool aRsvBit3,
                      uint8_t aOpCode, bool aMaskBit, uint32_t aMask,
                      const nsCString& aPayload);

  already_AddRefed<WebSocketFrame>
  CreateFrameIfNeeded(bool aFinBit, bool aRsvBit1, bool aRsvBit2, bool aRsvBit3,
                      uint8_t aOpCode, bool aMaskBit, uint32_t aMask,
                      uint8_t* aPayload, uint32_t aPayloadLength);

  already_AddRefed<WebSocketFrame>
  CreateFrameIfNeeded(bool aFinBit, bool aRsvBit1, bool aRsvBit2, bool aRsvBit3,
                      uint8_t aOpCode, bool aMaskBit, uint32_t aMask,
                      uint8_t* aPayloadInHdr, uint32_t aPayloadInHdrLength,
                      uint8_t* aPayload, uint32_t aPayloadLength);

private:
  WebSocketEventService();
  ~WebSocketEventService();

  bool HasListeners() const;
  void Shutdown();

  typedef nsTArray<nsCOMPtr<nsIWebSocketEventListener>> WindowListeners;

  struct WindowListener
  {
    WindowListeners mListeners;
    RefPtr<WebSocketEventListenerChild> mActor;
  };

  void GetListeners(uint64_t aInnerWindowID,
                    WindowListeners& aListeners) const;

  void ShutdownActorListener(WindowListener* aListener);

  // Used only on the main-thread.
  nsClassHashtable<nsUint64HashKey, WindowListener> mWindows;

  Atomic<uint64_t> mCountListeners;
};

} // net namespace
} // mozilla namespace

/**
 * Casting WebSocketEventService to nsISupports is ambiguous.
 * This method handles that.
 */
inline nsISupports*
ToSupports(mozilla::net::WebSocketEventService* p)
{
  return NS_ISUPPORTS_CAST(nsIWebSocketEventService*, p);
}

#endif // mozilla_net_WebSocketEventService_h
