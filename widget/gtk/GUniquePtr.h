/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef GUniquePtr_h_
#define GUniquePtr_h_

// Provides GUniquePtr to g_free a given pointer.

#include <glib.h>
#include "mozilla/UniquePtr.h"

namespace mozilla {

struct GFreeDeleter {
  constexpr GFreeDeleter() = default;
  void operator()(void* aPtr) const { g_free(aPtr); }
};

template <typename T>
using GUniquePtr = UniquePtr<T, GFreeDeleter>;

}  // namespace mozilla

#endif
