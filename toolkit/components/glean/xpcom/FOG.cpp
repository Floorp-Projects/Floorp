/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsIFOG.h"
#include "mozilla/FOG.h"

#include "mozilla/ClearOnShutdown.h"

namespace mozilla {

static StaticRefPtr<FOG> gFOG;

// static
already_AddRefed<FOG> FOG::GetSingleton() {
  if (gFOG) {
    return do_AddRef(gFOG);
  }

  gFOG = new FOG();
  ClearOnShutdown(&gFOG);
  return do_AddRef(gFOG);
}

extern "C" {
nsresult fog_init();
nsresult fog_set_log_pings(bool aEnableLogPings);
nsresult fog_set_debug_view_tag(const nsACString* aDebugTag);
nsresult fog_submit_ping(const nsACString* aPingName);
}

NS_IMETHODIMP
FOG::InitializeFOG() { return fog_init(); }

NS_IMETHODIMP
FOG::SetLogPings(bool aEnableLogPings) {
  return fog_set_log_pings(aEnableLogPings);
}

NS_IMETHODIMP
FOG::SetTagPings(const nsACString& aDebugTag) {
  return fog_set_debug_view_tag(&aDebugTag);
}

NS_IMETHODIMP
FOG::SendPing(const nsACString& aPingName) {
  return fog_submit_ping(&aPingName);
}

NS_IMPL_ISUPPORTS(FOG, nsIFOG)

}  //  namespace mozilla
