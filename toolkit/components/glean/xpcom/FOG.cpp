/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsIFOG.h"
#include "mozilla/FOG.h"
#include "mozilla/glean/fog_ffi_generated.h"

#include "mozilla/ClearOnShutdown.h"

namespace mozilla {

static StaticRefPtr<FOG> gFOG;

// static
already_AddRefed<FOG> FOG::GetSingleton() {
  if (gFOG) {
    return do_AddRef(gFOG);
  }

  gFOG = new FOG();
  RunOnShutdown([&] {
    gFOG->Shutdown();
    gFOG = nullptr;
  });
  return do_AddRef(gFOG);
}

void FOG::Shutdown() { glean::impl::fog_shutdown(); }

NS_IMETHODIMP
FOG::InitializeFOG() { return glean::impl::fog_init(); }

NS_IMETHODIMP
FOG::SetLogPings(bool aEnableLogPings) {
  return glean::impl::fog_set_log_pings(aEnableLogPings);
}

NS_IMETHODIMP
FOG::SetTagPings(const nsACString& aDebugTag) {
  return glean::impl::fog_set_debug_view_tag(&aDebugTag);
}

NS_IMETHODIMP
FOG::SendPing(const nsACString& aPingName) {
  return glean::impl::fog_submit_ping(&aPingName);
}

NS_IMPL_ISUPPORTS(FOG, nsIFOG)

}  //  namespace mozilla
