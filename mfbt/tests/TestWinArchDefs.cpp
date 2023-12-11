/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// This code tests the consistency of architecture-specific predefined macros
// inherited from MSVC, before and after windows.h inclusion. See
// https://learn.microsoft.com/en-us/cpp/preprocessor/predefined-macros for a
// list of such macros.

// If this test compiles, it is successful. See bug 1866562 for an example
// where mingwclang builds were failing to compile this code.

#if defined(_M_IX86)
constexpr auto kIX86 = _M_IX86;
#endif

#if defined(_M_X64)
constexpr auto kX64 = _M_X64;
#endif

#if defined(_M_AMD64)
constexpr auto kAMD64 = _M_AMD64;
#endif

#if defined(_M_ARM)
constexpr auto kARM = _M_ARM;
#endif

#if defined(_M_ARM64)
constexpr auto kARM64 = _M_ARM64;
#endif

#include <windows.h>

#if defined(_M_IX86)
static_assert(kIX86 == _M_IX86);
#endif

#if defined(_M_X64)
static_assert(kX64 == _M_X64);
#endif

#if defined(_M_AMD64)
static_assert(kAMD64 == _M_AMD64);
#endif

#if defined(_M_ARM)
static_assert(kARM == _M_ARM);
#endif

#if defined(_M_ARM64)
static_assert(kARM64 == _M_ARM64);
#endif

// If this test compiles, it is successful.
int main() { return 0; }
