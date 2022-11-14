/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/. */

#include "mozilla/Viaduct.h"

#include "mozilla/ViaductRequest.h"

namespace mozilla {

namespace {

extern "C" {
uint8_t viaduct_initialize(
    ViaductByteBuffer (*fetch_callback)(ViaductByteBuffer));
}

static ViaductByteBuffer ViaductCallback(ViaductByteBuffer buffer) {
  MOZ_ASSERT(!NS_IsMainThread(), "Background thread only!");
  RefPtr<ViaductRequest> request = new ViaductRequest();
  return request->MakeRequest(buffer);
}

}  // namespace

void InitializeViaduct() { viaduct_initialize(ViaductCallback); }

}  // namespace mozilla
