/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "WebSocketFrame.h"

#include "WebSocketChannel.h"

extern PRThread *gSocketThread;

namespace mozilla {
namespace net {

NS_INTERFACE_MAP_BEGIN(WebSocketFrame)
  NS_INTERFACE_MAP_ENTRY_AMBIGUOUS(nsISupports, nsIWebSocketFrame)
  NS_INTERFACE_MAP_ENTRY(nsIWebSocketFrame)
NS_INTERFACE_MAP_END

NS_IMPL_ADDREF(WebSocketFrame)
NS_IMPL_RELEASE(WebSocketFrame)

WebSocketFrame::WebSocketFrame(bool aFinBit, bool aRsvBit1, bool aRsvBit2,
                               bool aRsvBit3, uint8_t aOpCode, bool aMaskBit,
                               uint32_t aMask, const nsCString& aPayload)
  : mFinBit(aFinBit)
  , mRsvBit1(aRsvBit1)
  , mRsvBit2(aRsvBit2)
  , mRsvBit3(aRsvBit3)
  , mMaskBit(aMaskBit)
  , mOpCode(aOpCode)
  , mMask(aMask)
  , mPayload(aPayload)
{
  MOZ_ASSERT(PR_GetCurrentThread() == gSocketThread);
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

WSF_GETTER(GetFinBit, mFinBit, bool);
WSF_GETTER(GetRsvBit1, mRsvBit1, bool);
WSF_GETTER(GetRsvBit2, mRsvBit2, bool);
WSF_GETTER(GetRsvBit3, mRsvBit3, bool);
WSF_GETTER(GetOpCode, mOpCode, uint16_t);
WSF_GETTER(GetMaskBit, mMaskBit, bool);
WSF_GETTER(GetMask, mMask, uint32_t);

#undef WSF_GETTER

NS_IMETHODIMP
WebSocketFrame::GetPayload(nsACString& aValue)
{
  MOZ_ASSERT(NS_IsMainThread());
  aValue = mPayload;
  return NS_OK;
}

} // net namespace
} // mozilla namespace
