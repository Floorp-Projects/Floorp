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

NS_IMETHODIMP
AboutGlean::SetLogPings(bool aEnableLogPings) {
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
AboutGlean::SetTagPings(const nsACString& aDebugTag) {
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
AboutGlean::SendPing(const nsACString& aPingName) {
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMPL_ISUPPORTS(AboutGlean, nsIAboutGlean)

}  //  namespace mozilla
