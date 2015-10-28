/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_net_WebSocketFrameService_h
#define mozilla_net_WebSocketFrameService_h

#include "mozilla/Atomics.h"
#include "nsIWebSocketFrameService.h"
#include "nsAutoPtr.h"
#include "nsCOMPtr.h"
#include "nsClassHashtable.h"
#include "nsHashKeys.h"
#include "nsIObserver.h"
#include "nsISupportsImpl.h"
#include "nsTObserverArray.h"

namespace mozilla {
namespace net {

class WebSocketFrame;
class WebSocketFrameListenerChild;

class WebSocketFrameService final : public nsIWebSocketFrameService
                                  , public nsIObserver
{
  friend class WebSocketFrameRunnable;

public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSIOBSERVER
  NS_DECL_NSIWEBSOCKETFRAMESERVICE

  static already_AddRefed<WebSocketFrameService> GetOrCreate();

  void FrameReceived(uint32_t aWebSocketSerialID,
                     uint64_t aInnerWindowID,
                     WebSocketFrame* aFrame);

  void  FrameSent(uint32_t aWebSocketSerialID,
                  uint64_t aInnerWindowID,
                  WebSocketFrame* aFrame);

  WebSocketFrame*
  CreateFrameIfNeeded(bool aFinBit, bool aRsvBit1, bool aRsvBit2, bool aRsvBit3,
                      uint8_t aOpCode, bool aMaskBit, uint32_t aMask,
                      const nsCString& aPayload);

  WebSocketFrame*
  CreateFrameIfNeeded(bool aFinBit, bool aRsvBit1, bool aRsvBit2, bool aRsvBit3,
                      uint8_t aOpCode, bool aMaskBit, uint32_t aMask,
                      uint8_t* aPayload, uint32_t aPayloadLength);

  WebSocketFrame*
  CreateFrameIfNeeded(bool aFinBit, bool aRsvBit1, bool aRsvBit2, bool aRsvBit3,
                      uint8_t aOpCode, bool aMaskBit, uint32_t aMask,
                      uint8_t* aPayloadInHdr, uint32_t aPayloadInHdrLength,
                      uint8_t* aPayload, uint32_t aPayloadLength);

private:
  WebSocketFrameService();
  ~WebSocketFrameService();

  bool HasListeners() const;
  void Shutdown();

  typedef nsTObserverArray<nsCOMPtr<nsIWebSocketFrameListener>> WindowListeners;

  struct WindowListener
  {
    WindowListeners mListeners;
    RefPtr<WebSocketFrameListenerChild> mActor;
  };

  WindowListeners* GetListeners(uint64_t aInnerWindowID) const;
  void ShutdownActorListener(WindowListener* aListener);

  // Used only on the main-thread.
  nsClassHashtable<nsUint64HashKey, WindowListener> mWindows;

  Atomic<uint64_t> mCountListeners;
};

} // net namespace
} // mozilla namespace

#endif // mozilla_net_WebSocketFrameService_h
