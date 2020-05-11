/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/. */

#include "mozilla/Viaduct.h"

#include "mozilla/ClearOnShutdown.h"
#include "mozilla/ScopeExit.h"
#include "mozilla/ViaductRequest.h"

namespace mozilla {

namespace {

extern "C" {
uint8_t viaduct_initialize(
    ViaductByteBuffer (*fetch_callback)(ViaductByteBuffer));
}

}  // namespace

static StaticRefPtr<Viaduct> gViaduct;

// static
already_AddRefed<Viaduct> Viaduct::GetSingleton() {
  if (gViaduct) {
    return do_AddRef(gViaduct);
  }

  gViaduct = new Viaduct();
  ClearOnShutdown(&gViaduct);
  return do_AddRef(gViaduct);
}

NS_IMETHODIMP
Viaduct::EnsureInitialized() {
  if (mInitialized.compareExchange(false, true)) {
    viaduct_initialize(ViaductCallback);
  }
  return NS_OK;
}

// static
ViaductByteBuffer Viaduct::ViaductCallback(ViaductByteBuffer buffer) {
  MOZ_ASSERT(!NS_IsMainThread(), "Background thread only!");
  RefPtr<ViaductRequest> request = new ViaductRequest();
  return request->MakeRequest(buffer);
}

NS_IMPL_ISUPPORTS(Viaduct, mozIViaduct)

};  // namespace mozilla
