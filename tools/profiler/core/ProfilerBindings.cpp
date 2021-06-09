/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* FFI functions for Profiler Rust API to call into profiler */

#include "ProfilerBindings.h"

#include "GeckoProfiler.h"

void gecko_profiler_register_thread(const char* aName) {
  PROFILER_REGISTER_THREAD(aName);
}

void gecko_profiler_unregister_thread() { PROFILER_UNREGISTER_THREAD(); }
