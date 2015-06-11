/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: sw=4 ts=4 et :
 */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_msvc_raise_wrappers_h
#define mozilla_msvc_raise_wrappers_h

#ifdef _XSTDDEF_
#  error "Unable to wrap _RAISE(); CRT _RAISE() already defined"
#endif
#ifdef _XUTILITY_
#  error "Unabled to wrap _X[exception]"(); CRT versions already declared"
#endif

#include "mozilla/mozalloc_abort.h"

namespace std {

// NB: user code is not supposed to touch the std:: namespace.  We're
// doing this after careful review because we want to define our own
// exception throwing semantics.  Don't try this at home!

MOZALLOC_EXPORT __declspec(noreturn) void moz_Xinvalid_argument(const char*);
MOZALLOC_EXPORT __declspec(noreturn) void moz_Xlength_error(const char*);
MOZALLOC_EXPORT __declspec(noreturn) void moz_Xout_of_range(const char*);
MOZALLOC_EXPORT __declspec(noreturn) void moz_Xoverflow_error(const char*);
MOZALLOC_EXPORT __declspec(noreturn) void moz_Xruntime_error(const char*);

} // namespace std

#ifndef MOZALLOC_DONT_WRAP_RAISE_FUNCTIONS

#  define _Xinvalid_argument  moz_Xinvalid_argument
#  define _Xlength_error      moz_Xlength_error
#  define _Xout_of_range      moz_Xout_of_range
#  define _Xoverflow_error    moz_Xoverflow_error
#  define _Xruntime_error     moz_Xruntime_error

#  include <xstddef>
#  include <xutility>

#  undef _RAISE
#  define _RAISE(x) mozalloc_abort((x).what())

#endif  // ifndef MOZALLOC_DONT_WRAP_RAISE_FUNCTIONS

#endif  // ifndef mozilla_msvc_raise_wrappers_h
