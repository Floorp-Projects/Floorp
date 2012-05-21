/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: sw=4 ts=4 et :
 */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_msvc_throw_wrapper_h
#define mozilla_msvc_throw_wrapper_h

// Define our own _Throw because the Win2k CRT doesn't export it.
#  ifdef _EXCEPTION_
#    error "Unable to wrap _Throw(); CRT _Throw() already declared"
#  endif
#  define _Throw  mozilla_Throw
#  include <exception>

#endif  // ifndef mozilla_msvc_throw_wrapper_h
