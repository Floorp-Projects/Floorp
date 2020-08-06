/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/ClearOnShutdown.h"
#include "mozilla/StaticPtr.h"
#include "nsCOMPtr.h"
#include "SFVService.h"

// This anonymous namespace prevents outside C++ code from improperly accessing
// these implementation details.
namespace {
extern "C" {
// Implemented in Rust.
void new_sfv_service(nsISFVService** result);
}

static mozilla::StaticRefPtr<nsISFVService> sService;
}  // namespace

namespace mozilla::net {

already_AddRefed<nsISFVService> GetSFVService() {
  nsCOMPtr<nsISFVService> service;

  if (sService) {
    service = sService;
  } else {
    new_sfv_service(getter_AddRefs(service));
    sService = service;
    mozilla::ClearOnShutdown(&sService);
  }

  return service.forget();
}

}  // namespace mozilla::net
