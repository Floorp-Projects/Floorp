/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/Assertions.h"
#include "mozilla/fuzzing/Nyx.h"

namespace mozilla {
namespace fuzzing {

Nyx::Nyx() {}

// static
Nyx& Nyx::instance() {
  static Nyx nyx;
  return nyx;
}

extern "C" {
MOZ_EXPORT __attribute__((weak)) void nyx_start(void);
MOZ_EXPORT __attribute__((weak)) uint32_t nyx_get_next_fuzz_data(void*,
                                                                 uint32_t);
MOZ_EXPORT __attribute__((weak)) void nyx_release(void);
MOZ_EXPORT __attribute__((weak)) void nyx_handle_event(const char*, const char*,
                                                       int, const char*);
}

/*
 * In this macro, we must avoid calling MOZ_CRASH and friends, as these
 * calls will redirect to the Nyx event handler routines. If the library
 * is not properly preloaded, we will crash in the process. Instead, emit
 * a descriptive error and then force a crash that won't be redirected.
 */
#define NYX_CHECK_API(func)                                                    \
  if (!func) {                                                                 \
    fprintf(                                                                   \
        stderr,                                                                \
        "Error: Nyx library must be in LD_PRELOAD. Missing function \"%s\"\n", \
        #func);                                                                \
    MOZ_REALLY_CRASH(__LINE__);                                                \
  }

void Nyx::start(void) {
  MOZ_RELEASE_ASSERT(!mInited);
  mInited = true;

  NYX_CHECK_API(nyx_start);
  NYX_CHECK_API(nyx_get_next_fuzz_data);
  NYX_CHECK_API(nyx_release);
  NYX_CHECK_API(nyx_handle_event);

  nyx_start();
}

bool Nyx::is_enabled(const char* identifier) {
  static char* fuzzer = getenv("NYX_FUZZER");
  if (!fuzzer || strcmp(fuzzer, identifier)) {
    return false;
  }
  return true;
}

uint32_t Nyx::get_data(uint8_t* data, uint32_t size) {
  MOZ_RELEASE_ASSERT(mInited);
  return nyx_get_next_fuzz_data(data, size);
}

void Nyx::release(void) {
  MOZ_RELEASE_ASSERT(mInited);
  nyx_release();
}

void Nyx::handle_event(const char* type, const char* file, int line,
                       const char* reason) {
  MOZ_RELEASE_ASSERT(mInited);
  nyx_handle_event(type, file, line, reason);
}

}  // namespace fuzzing
}  // namespace mozilla
