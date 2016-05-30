/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_net_WebSocketFrame_h
#define mozilla_net_WebSocketFrame_h

#include "nsAutoPtr.h"
#include "nsIWebSocketEventService.h"
#include "nsString.h"

namespace IPC {
class Message;
}

namespace mozilla {
namespace net {

class WebSocketFrameData final
{
public:
  WebSocketFrameData();

  explicit WebSocketFrameData(const WebSocketFrameData& aData);

  WebSocketFrameData(DOMHighResTimeStamp aTimeStamp, bool aFinBit,
                     bool aRsvBit1, bool aRsvBit2, bool aRsvBit3,
                     uint8_t aOpCode, bool aMaskBit, uint32_t aMask,
                     const nsCString& aPayload);

  ~WebSocketFrameData();

  // For IPC serialization
  void WriteIPCParams(IPC::Message* aMessage) const;
  bool ReadIPCParams(const IPC::Message* aMessage, PickleIterator* aIter);

  DOMHighResTimeStamp mTimeStamp;

  bool mFinBit : 1;
  bool mRsvBit1 : 1;
  bool mRsvBit2 : 1;
  bool mRsvBit3 : 1;
  bool mMaskBit : 1;
  uint8_t mOpCode;

  uint32_t mMask;

  nsCString mPayload;
};

class WebSocketFrame final : public nsIWebSocketFrame
{
public:
  NS_DECL_THREADSAFE_ISUPPORTS
  NS_DECL_NSIWEBSOCKETFRAME

  explicit WebSocketFrame(const WebSocketFrameData& aData);

  WebSocketFrame(bool aFinBit, bool aRsvBit1, bool aRsvBit2, bool aRsvBit3,
                 uint8_t aOpCode, bool aMaskBit, uint32_t aMask,
                 const nsCString& aPayload);

  const WebSocketFrameData& Data() const
  {
    return mData;
  }

private:
  ~WebSocketFrame();

  WebSocketFrameData mData;
};

} // net namespace
} // mozilla namespace

#endif // mozilla_net_WebSocketFrame_h
