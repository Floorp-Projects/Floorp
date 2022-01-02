/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// This is a cut down version of Chromium source file base/scoped_native_library.h
// The chromium sandbox only requires ScopedNativeLibrary class to automatically
// unload the library, which we can achieve with UniquePtr.

#ifndef BASE_SCOPED_NATIVE_LIBRARY_H_
#define BASE_SCOPED_NATIVE_LIBRARY_H_

#include "mozilla/UniquePtr.h"

namespace base {

struct HModuleFreePolicy
{
  typedef HMODULE pointer;
  void operator()(pointer hModule)
  {
    ::FreeLibrary(hModule);
  }
};

typedef mozilla::UniquePtr<HMODULE, HModuleFreePolicy> ScopedNativeLibrary;

} // namespace base

#endif  // BASE_SCOPED_NATIVE_LIBRARY_H_
