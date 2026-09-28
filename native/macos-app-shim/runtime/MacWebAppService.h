/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_widget_MacWebAppService_h
#define mozilla_widget_MacWebAppService_h

#include <mach/mach.h>
#include <map>

#include "mozilla/UniquePtr.h"
#include "mozilla/layers/NativeLayerCA.h"
#include "nsIMacWebAppService.h"
#include "nsIObserver.h"
#include "nsString.h"

#define NS_MACWEBAPPSERVICE_CONTRACTID "@floorp.org/mac-web-app-service;1"

namespace mozilla::widget {

class MacWebAppService final : public nsIMacWebAppService, public nsIObserver {
 public:
  NS_DECL_THREADSAFE_ISUPPORTS
  NS_DECL_NSIMACWEBAPPSERVICE
  NS_DECL_NSIOBSERVER

  static already_AddRefed<MacWebAppService> GetSingleton();
  static nsCString TakePendingAppId();

  // Success means queued, not acknowledged by the Shim.
  bool SendControl(const nsACString& aAppId, uint32_t aType,
                   const nsACString& aPayload,
                   mach_port_t aSurface = MACH_PORT_NULL);
  // A destroyed widget no longer receives events. Keep its producer leases
  // until the authenticated peer confirms close or actually exits.
  void RetireSurfaces(const nsACString& aAppId, uint32_t aWindowId,
      std::map<uint32_t, RefPtr<layers::NativeLayerSurface>>&& aSurfaces);

 private:
  MacWebAppService();
  ~MacWebAppService();

  struct Impl;
  UniquePtr<Impl> mImpl;
};

}  // namespace mozilla::widget

#endif
