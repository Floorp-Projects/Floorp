/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// This is a dummy version of base/debug/crash_logging.cc

#include "base/debug/crash_logging.h"

namespace base {
namespace debug {

CrashKeyString* AllocateCrashKeyString(const char name[],
                                       CrashKeySize value_length) {
  return nullptr;
}

void SetCrashKeyString(CrashKeyString* crash_key, base::StringPiece value) {}

}  // namespace debug
}  // namespace base
