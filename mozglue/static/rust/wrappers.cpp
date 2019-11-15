/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

// This ensures the right configuration for e.g. MOZ_GLUE_IN_PROGRAM,
// used in the MFBT headers included further below. We use js-confdefs.h
// instead of mozilla-config.h because the latter is not present in
// spidermonkey standalone builds while the former is always present.
#include "js-confdefs.h"
#include "mozilla/Assertions.h"
#include "mozilla/Types.h"

// MOZ_Crash wrapper for use by rust, since MOZ_Crash is an inline function.
extern "C" void RustMozCrash(const char* aFilename, int aLine,
                             const char* aReason) {
  MOZ_Crash(aFilename, aLine, aReason);
}
