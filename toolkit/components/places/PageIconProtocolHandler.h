/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_places_PageIconProtocolHandler_h
#define mozilla_places_PageIconProtocolHandler_h

#include "mozilla/AlreadyAddRefed.h"
#include "mozilla/ClearOnShutdown.h"
#include "mozilla/StaticPtr.h"
#include "nsIProtocolHandler.h"
#include "nsThreadUtils.h"
#include "nsWeakReference.h"

namespace mozilla {
namespace places {

class PageIconProtocolHandler final : public nsIProtocolHandler,
                                      public nsSupportsWeakReference {
 public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSIPROTOCOLHANDLER

  static already_AddRefed<PageIconProtocolHandler> GetSingleton() {
    MOZ_ASSERT(NS_IsMainThread());
    if (MOZ_UNLIKELY(!sSingleton)) {
      sSingleton = new PageIconProtocolHandler();
      ClearOnShutdown(&sSingleton);
    }
    return do_AddRef(sSingleton);
  }

 private:
  ~PageIconProtocolHandler() = default;

  nsresult NewChannelInternal(nsIURI*, nsILoadInfo*, nsIChannel**);
  static StaticRefPtr<PageIconProtocolHandler> sSingleton;
};

}  // namespace places
}  // namespace mozilla

#endif
