/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 sw=2 et: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/net/CookiePermission.h"
#include "mozilla/StaticPtr.h"
#include "mozilla/ClearOnShutdown.h"

/****************************************************************
 ************************ CookiePermission **********************
 ****************************************************************/

namespace mozilla {
namespace net {

namespace {
StaticRefPtr<CookiePermission> gSingleton;
}  // namespace

NS_IMPL_ISUPPORTS(CookiePermission, nsICookiePermission)

// static
already_AddRefed<nsICookiePermission> CookiePermission::GetOrCreate() {
  if (!gSingleton) {
    gSingleton = new CookiePermission();
    ClearOnShutdown(&gSingleton);
  }
  return do_AddRef(gSingleton);
}

bool CookiePermission::Init() { return true; }

}  // namespace net
}  // namespace mozilla
