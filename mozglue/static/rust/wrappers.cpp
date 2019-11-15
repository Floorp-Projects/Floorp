/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

// Because this file is compiled from rust, it hits code in
// config/makefiles/rust.mk that adds -DMOZILLA_CONFIG_H=1 to CFLAGS,
// which inhibits the -include mozilla-config.h. But we do want
// mozilla-config.h to be included _and_ to actually do something,
// so we undefined MOZILLA_CONFIG_H and reinclude mozilla-config.h here.
// This ensures the right configuration for e.g. MOZ_GLUE_IN_PROGRAM,
// used in the MFBT headers included further below.
#undef MOZILLA_CONFIG_H
#include "mozilla-config.h"
#include "mozilla/Assertions.h"
#include "mozilla/Types.h"

// MOZ_Crash wrapper for use by rust, since MOZ_Crash is an inline function.
extern "C" void RustMozCrash(const char* aFilename, int aLine,
                             const char* aReason) {
  MOZ_Crash(aFilename, aLine, aReason);
}
