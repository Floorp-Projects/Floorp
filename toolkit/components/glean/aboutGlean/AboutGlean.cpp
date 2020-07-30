/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsIAboutGlean.h"
#include "mozilla/AboutGlean.h"

#include "mozilla/ClearOnShutdown.h"

namespace mozilla {

static StaticRefPtr<AboutGlean> gAboutGlean;

// static
already_AddRefed<AboutGlean> AboutGlean::GetSingleton() {
  if (gAboutGlean) {
    return do_AddRef(gAboutGlean);
  }

  gAboutGlean = new AboutGlean();
  ClearOnShutdown(&gAboutGlean);
  return do_AddRef(gAboutGlean);
}

extern "C" {
nsresult fog_set_log_pings(bool aEnableLogPings);
nsresult fog_set_debug_view_tag(const nsACString* aDebugTag);
nsresult fog_submit_ping(const nsACString* aPingName);
}

NS_IMETHODIMP
AboutGlean::SetLogPings(bool aEnableLogPings) {
  return fog_set_log_pings(aEnableLogPings);
}

NS_IMETHODIMP
AboutGlean::SetTagPings(const nsACString& aDebugTag) {
  return fog_set_debug_view_tag(&aDebugTag);
}

NS_IMETHODIMP
AboutGlean::SendPing(const nsACString& aPingName) {
  return fog_submit_ping(&aPingName);
}

NS_IMPL_ISUPPORTS(AboutGlean, nsIAboutGlean)

}  //  namespace mozilla
