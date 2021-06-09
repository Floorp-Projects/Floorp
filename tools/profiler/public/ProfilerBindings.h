/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef ProfilerBindings_h
#define ProfilerBindings_h

/* FFI functions for Profiler Rust API to call into profiler */

// Everything in here is safe to include unconditionally, implementations must
// take !MOZ_GECKO_PROFILER into account.

extern "C" {

void gecko_profiler_register_thread(const char* aName);
void gecko_profiler_unregister_thread();

}  // extern "C"

#endif  // ProfilerBindings_h
