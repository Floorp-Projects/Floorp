/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#include "mozilla/ClearOnShutdown.h"
#include "mozilla/StaticPtr.h"
#include "nsCOMPtr.h"
#include "Bits.h"

// This anonymous namespace prevents outside C++ code from improperly accessing
// these implementation details.
namespace {
extern "C" {
// Implemented in Rust.
void new_bits_service(nsIBits** result);
}

static mozilla::StaticRefPtr<nsIBits> sBitsService;
}  // namespace

already_AddRefed<nsIBits> GetBitsService() {
  nsCOMPtr<nsIBits> bitsService;

  if (sBitsService) {
    bitsService = sBitsService;
  } else {
    new_bits_service(getter_AddRefs(bitsService));
    sBitsService = bitsService;
    mozilla::ClearOnShutdown(&sBitsService);
  }

  return bitsService.forget();
}
