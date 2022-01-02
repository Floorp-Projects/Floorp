/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "gk_rust_utils_ffi_generated.h"
#include "GkRustUtils.h"

using namespace mozilla;

/* static */
bool GkRustUtils::ParseSemVer(const nsACString& aVersion, uint64_t& aOutMajor,
                              uint64_t& aOutMinor, uint64_t& aOutPatch) {
  return GkRustUtils_ParseSemVer(&aVersion, &aOutMajor, &aOutMinor, &aOutPatch);
}
