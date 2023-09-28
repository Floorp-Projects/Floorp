/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: sw=2 ts=4 et :
 */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_mozalloc_oom_h
#define mozilla_mozalloc_oom_h

#include "mozalloc.h"

/**
 * Called when memory is critically low.  Returns iff it was able to
 * remedy the critical memory situation; if not, it will abort().
 */
#ifdef __wasm__
__attribute__((import_module("env")))
__attribute__((import_name("mozalloc_handle_oom")))
#  if defined(__clang__) && (__clang_major__ < 11)
__attribute__((weak))
#  endif
#endif
MFBT_API void
mozalloc_handle_oom(size_t requestedSize);

extern MFBT_DATA size_t gOOMAllocationSize;

/* TODO: functions to query system memory usage and register
 * critical-memory handlers. */

#endif /* ifndef mozilla_mozalloc_oom_h */
