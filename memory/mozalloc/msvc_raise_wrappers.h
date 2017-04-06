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
#  error "Unable to wrap _X[exception](); CRT versions already declared"
#endif
#ifdef _FUNCTIONAL_
#  error "Unable to wrap _Xbad_function_call(); CRT version already declared"
#endif

#include "mozilla/mozalloc_abort.h"

// xutility will declare the following functions in the std namespace.
// We #define them to be named differently so we can ensure the exception
// throwing semantics of these functions work exactly the way we want, by
// defining our own versions in msvc_raise_wrappers.cpp.
#  define _Xinvalid_argument  moz_Xinvalid_argument
#  define _Xlength_error      moz_Xlength_error
#  define _Xout_of_range      moz_Xout_of_range
#  define _Xoverflow_error    moz_Xoverflow_error
#  define _Xruntime_error     moz_Xruntime_error
// used by <functional>
#  define _Xbad_function_call moz_Xbad_function_call

#  include <xstddef>
#  include <xutility>

#  undef _RAISE
#  define _RAISE(x) mozalloc_abort((x).what())

#endif  // ifndef mozilla_msvc_raise_wrappers_h
