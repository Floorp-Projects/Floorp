/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "ProtocolHandlerInfo.h"
#include "StaticComponents.h"
#include "nsIProtocolHandler.h"

namespace mozilla::net {

uint32_t ProtocolHandlerInfo::StaticProtocolFlags() const {
  uint32_t flags = mInner.match(
      [&](const xpcom::StaticProtocolHandler* handler) {
        return handler->mProtocolFlags;
      },
      [&](const RuntimeProtocolHandler& handler) {
        return handler.mProtocolFlags;
      });
#if !IS_ORIGIN_IS_FULL_SPEC_DEFINED
  MOZ_RELEASE_ASSERT(!(flags & nsIProtocolHandler::ORIGIN_IS_FULL_SPEC),
                     "ORIGIN_IS_FULL_SPEC is unsupported but used");
#endif
  return flags;
}

int32_t ProtocolHandlerInfo::DefaultPort() const {
  return mInner.match(
      [&](const xpcom::StaticProtocolHandler* handler) {
        return handler->mDefaultPort;
      },
      [&](const RuntimeProtocolHandler& handler) {
        return handler.mDefaultPort;
      });
}

nsresult ProtocolHandlerInfo::DynamicProtocolFlags(nsIURI* aURI,
                                                   uint32_t* aFlags) const {
  MOZ_DIAGNOSTIC_ASSERT(NS_IsMainThread());

  // If we're querying dynamic flags, we'll need to fetch the actual xpcom
  // component in order to check them.
  if (HasDynamicFlags()) {
    nsCOMPtr<nsIProtocolHandler> handler = Handler();
    if (nsCOMPtr<nsIProtocolHandlerWithDynamicFlags> dynamic =
            do_QueryInterface(handler)) {
      nsresult rv = dynamic->GetFlagsForURI(aURI, aFlags);
      NS_ENSURE_SUCCESS(rv, rv);
      MOZ_DIAGNOSTIC_ASSERT(
          (StaticProtocolFlags() & ~nsIProtocolHandler::DYNAMIC_URI_FLAGS) ==
              (*aFlags & ~nsIProtocolHandler::DYNAMIC_URI_FLAGS),
          "only DYNAMIC_URI_FLAGS may be changed by a "
          "nsIProtocolHandlerWithDynamicFlags implementation");
      return NS_OK;
    }
  }

  // Otherwise, just check against static flags.
  *aFlags = StaticProtocolFlags();
  return NS_OK;
}

bool ProtocolHandlerInfo::HasDynamicFlags() const {
  return mInner.match(
      [&](const xpcom::StaticProtocolHandler* handler) {
        return handler->mHasDynamicFlags;
      },
      [&](const RuntimeProtocolHandler&) { return false; });
}

already_AddRefed<nsIProtocolHandler> ProtocolHandlerInfo::Handler() const {
  MOZ_ASSERT(NS_IsMainThread());

  nsCOMPtr<nsIProtocolHandler> retval;
  mInner.match(
      [&](const xpcom::StaticProtocolHandler* handler) {
        retval = handler->Module().GetService();
      },
      [&](const RuntimeProtocolHandler& handler) {
        retval = handler.mHandler.get();
      });
  return retval.forget();
}

}  // namespace mozilla::net
