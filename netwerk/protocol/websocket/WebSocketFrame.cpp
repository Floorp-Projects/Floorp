/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "WebSocketFrame.h"

#include "WebSocketChannel.h"
#include "nsSocketTransportService2.h"
#include "nsThreadUtils.h" // for NS_IsMainThread
#include "ipc/IPCMessageUtils.h"

namespace mozilla {
namespace net {

NS_INTERFACE_MAP_BEGIN(WebSocketFrame)
  NS_INTERFACE_MAP_ENTRY_AMBIGUOUS(nsISupports, nsIWebSocketFrame)
  NS_INTERFACE_MAP_ENTRY(nsIWebSocketFrame)
NS_INTERFACE_MAP_END

NS_IMPL_ADDREF(WebSocketFrame)
NS_IMPL_RELEASE(WebSocketFrame)

WebSocketFrame::WebSocketFrame(const WebSocketFrameData& aData)
  : mData(aData)
{}

WebSocketFrame::WebSocketFrame(bool aFinBit, bool aRsvBit1, bool aRsvBit2,
                               bool aRsvBit3, uint8_t aOpCode, bool aMaskBit,
                               uint32_t aMask, const nsCString& aPayload)
  : mData(PR_Now(), aFinBit, aRsvBit1, aRsvBit2, aRsvBit3, aOpCode, aMaskBit,
          aMask, aPayload)
{
  MOZ_ASSERT(PR_GetCurrentThread() == gSocketThread);
  mData.mTimeStamp = PR_Now();
}

WebSocketFrame::~WebSocketFrame()
{}

#define WSF_GETTER( method, value , type )     \
NS_IMETHODIMP                                  \
WebSocketFrame::method(type* aValue)           \
{                                              \
  MOZ_ASSERT(NS_IsMainThread());               \
  if (!aValue) {                               \
    return NS_ERROR_FAILURE;                   \
  }                                            \
  *aValue = value;                             \
  return NS_OK;                                \
}

WSF_GETTER(GetTimeStamp, mData.mTimeStamp, DOMHighResTimeStamp);
WSF_GETTER(GetFinBit, mData.mFinBit, bool);
WSF_GETTER(GetRsvBit1, mData.mRsvBit1, bool);
WSF_GETTER(GetRsvBit2, mData.mRsvBit2, bool);
WSF_GETTER(GetRsvBit3, mData.mRsvBit3, bool);
WSF_GETTER(GetOpCode, mData.mOpCode, uint16_t);
WSF_GETTER(GetMaskBit, mData.mMaskBit, bool);
WSF_GETTER(GetMask, mData.mMask, uint32_t);

#undef WSF_GETTER

NS_IMETHODIMP
WebSocketFrame::GetPayload(nsACString& aValue)
{
  MOZ_ASSERT(NS_IsMainThread());
  aValue = mData.mPayload;
  return NS_OK;
}

WebSocketFrameData::WebSocketFrameData()
  : mTimeStamp(0)
  , mFinBit(false)
  , mRsvBit1(false)
  , mRsvBit2(false)
  , mRsvBit3(false)
  , mMaskBit(false)
  , mOpCode(0)
  , mMask(0)
{
  MOZ_COUNT_CTOR(WebSocketFrameData);
}

WebSocketFrameData::WebSocketFrameData(DOMHighResTimeStamp aTimeStamp,
                                       bool aFinBit, bool aRsvBit1,
                                       bool aRsvBit2, bool aRsvBit3,
                                       uint8_t aOpCode, bool aMaskBit,
                                       uint32_t aMask,
                                       const nsCString& aPayload)
  : mTimeStamp(aTimeStamp)
  , mFinBit(aFinBit)
  , mRsvBit1(aRsvBit1)
  , mRsvBit2(aRsvBit2)
  , mRsvBit3(aRsvBit3)
  , mMaskBit(aMaskBit)
  , mOpCode(aOpCode)
  , mMask(aMask)
  , mPayload(aPayload)
{
  MOZ_COUNT_CTOR(WebSocketFrameData);
}

WebSocketFrameData::WebSocketFrameData(const WebSocketFrameData& aData)
  : mTimeStamp(aData.mTimeStamp)
  , mFinBit(aData.mFinBit)
  , mRsvBit1(aData.mRsvBit1)
  , mRsvBit2(aData.mRsvBit2)
  , mRsvBit3(aData.mRsvBit3)
  , mMaskBit(aData.mMaskBit)
  , mOpCode(aData.mOpCode)
  , mMask(aData.mMask)
  , mPayload(aData.mPayload)
{
  MOZ_COUNT_CTOR(WebSocketFrameData);
}

WebSocketFrameData::~WebSocketFrameData()
{
  MOZ_COUNT_DTOR(WebSocketFrameData);
}

void
WebSocketFrameData::WriteIPCParams(IPC::Message* aMessage) const
{
  WriteParam(aMessage, mTimeStamp);
  WriteParam(aMessage, mFinBit);
  WriteParam(aMessage, mRsvBit1);
  WriteParam(aMessage, mRsvBit2);
  WriteParam(aMessage, mRsvBit3);
  WriteParam(aMessage, mOpCode);
  WriteParam(aMessage, mMaskBit);
  WriteParam(aMessage, mMask);
  WriteParam(aMessage, mPayload);
}

bool
WebSocketFrameData::ReadIPCParams(const IPC::Message* aMessage,
                                  void** aIter)
{
  if (!ReadParam(aMessage, aIter, &mTimeStamp)) {
    return false;
  }

#define ReadParamHelper(x)                     \
  {                                            \
    bool bit;                                  \
    if (!ReadParam(aMessage, aIter, &bit)) {   \
      return false;                            \
    }                                          \
    x = bit;                                   \
  }

  ReadParamHelper(mFinBit);
  ReadParamHelper(mRsvBit1);
  ReadParamHelper(mRsvBit2);
  ReadParamHelper(mRsvBit3);
  ReadParamHelper(mMaskBit);

#undef ReadParamHelper

  return ReadParam(aMessage, aIter, &mOpCode) &&
         ReadParam(aMessage, aIter, &mMask) &&
         ReadParam(aMessage, aIter, &mPayload);
}

} // net namespace
} // mozilla namespace
