/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_fuzzing_NyxWrapper_h
#define mozilla_fuzzing_NyxWrapper_h

#include "mozilla/Types.h"

/*
 * We need this event handler definition both in C and C++ to differentiate
 * the various flavors of controlled aborts (e.g. MOZ_DIAGNOSTIC_ASSERT
 * vs. MOZ_RELEASE_ASSERT). Hence we can't use the higher level C++ API
 * for this and directly redirect to the Nyx event handler in the preloaded
 * library.
 */

#ifdef __cplusplus
extern "C" {
#endif
MOZ_EXPORT __attribute__((weak)) void nyx_handle_event(const char* type,
                                                       const char* file,
                                                       int line,
                                                       const char* reason);

MOZ_EXPORT __attribute__((weak)) void nyx_puts(const char*);
#ifdef __cplusplus
}
#endif

#endif /* mozilla_fuzzing_NyxWrapper_h */
