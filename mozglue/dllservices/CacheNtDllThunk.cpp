/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "CacheNtDllThunk.h"

#include "mozilla/Maybe.h"
#include "mozilla/Span.h"
#include "mozilla/StaticPtr.h"

namespace mozilla {

static StaticAutoPtr<Buffer<IMAGE_THUNK_DATA>> sCachedNtDllThunk;

// This static method initializes sCachedNtDllThunk.  Because it's called in
// XREMain::XRE_main, which happens long before WindowsProcessLauncher's ctor
// accesses sCachedNtDllThunk, there is no race on sCachedNtDllThunk, thus
// no mutex is needed.
static void CacheNtDllThunk() {
  if (sCachedNtDllThunk) {
    return;
  }

  do {
    nt::PEHeaders ourExeImage(::GetModuleHandleW(nullptr));
    if (!ourExeImage) {
      break;
    }

    nt::PEHeaders ntdllImage(::GetModuleHandleW(L"ntdll.dll"));
    if (!ntdllImage) {
      break;
    }

    Maybe<Range<const uint8_t>> ntdllBoundaries = ntdllImage.GetBounds();
    if (!ntdllBoundaries) {
      break;
    }

    Maybe<Span<IMAGE_THUNK_DATA>> maybeNtDllThunks =
        ourExeImage.GetIATThunksForModule("ntdll.dll", ntdllBoundaries.ptr());
    if (maybeNtDllThunks.isNothing()) {
      break;
    }

    sCachedNtDllThunk = new Buffer<IMAGE_THUNK_DATA>(maybeNtDllThunks.value());
    return;
  } while (false);

  // Failed to cache IAT.  Initializing the variable with nullptr.
  sCachedNtDllThunk = new Buffer<IMAGE_THUNK_DATA>();
}

static Buffer<IMAGE_THUNK_DATA>* GetCachedNtDllThunk() {
  return sCachedNtDllThunk;
}

}  // namespace mozilla
