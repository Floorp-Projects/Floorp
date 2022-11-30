/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_net_ProtocolHandlerInfo_h
#define mozilla_net_ProtocolHandlerInfo_h

#include "mozilla/Variant.h"
#include "nsProxyRelease.h"
#include "nsIProtocolHandler.h"

namespace mozilla {
namespace xpcom {
struct StaticProtocolHandler;
}

namespace net {

struct RuntimeProtocolHandler {
  nsMainThreadPtrHandle<nsIProtocolHandler> mHandler;
  uint32_t mProtocolFlags;
  int32_t mDefaultPort;
};

// Information about a specific protocol handler.
class ProtocolHandlerInfo {
 public:
  explicit ProtocolHandlerInfo(const xpcom::StaticProtocolHandler& aStatic)
      : mInner(AsVariant(&aStatic)) {}
  explicit ProtocolHandlerInfo(RuntimeProtocolHandler aDynamic)
      : mInner(AsVariant(std::move(aDynamic))) {}

  // Returns the statically known protocol-specific flags.
  // See `nsIProtocolHandler` for valid values.
  uint32_t StaticProtocolFlags() const;

  // The port that this protocol normally uses.
  // If a port does not make sense for the protocol (e.g., "about:") then -1
  // will be returned.
  int32_t DefaultPort() const;

  // If true, `DynamicProtocolFlags()` may return a different value than
  // `StaticProtocolFlags()` for flags in `DYNAMIC_URI_FLAGS`, due to a
  // `nsIProtocolHandlerWithDynamicFlags` implementation.
  bool HasDynamicFlags() const;

  // Like `StaticProtocolFlags()` but also checks
  // `nsIProtocolHandlerWithDynamicFlags` for uri-specific flags.
  //
  // NOTE: Only safe to call from the main thread.
  nsresult DynamicProtocolFlags(nsIURI* aURI, uint32_t* aFlags) const
      MOZ_REQUIRES(sMainThreadCapability);

  // Get the main-thread-only nsIProtocolHandler instance.
  already_AddRefed<nsIProtocolHandler> Handler() const
      MOZ_REQUIRES(sMainThreadCapability);

 private:
  Variant<const xpcom::StaticProtocolHandler*, RuntimeProtocolHandler> mInner;
};

}  // namespace net
}  // namespace mozilla

#endif  // mozilla_net_ProtocolHandlerInfo_h
