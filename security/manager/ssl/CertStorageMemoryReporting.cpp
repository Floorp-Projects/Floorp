/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsIMemoryReporter.h"

// Rust doesn't support weak-linking, so MFBT_API functions like
// moz_malloc_size_of need a C++ wrapper that uses the regular ABI
//
// We're not using MOZ_DEFINE_MALLOC_SIZE_OF here because that makes the
// function `static`, which would make it not visible outside this file
extern "C" size_t cert_storage_malloc_size_of(void* aPtr) {
  MOZ_REPORT(aPtr);
  return moz_malloc_size_of(aPtr);
}
