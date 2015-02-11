/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: sw=4 ts=4 et :
 */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_throw_msvc_h
#define mozilla_throw_msvc_h

#if defined(MOZ_MSVC_STL_WRAP_RAISE)
#  include "msvc_raise_wrappers.h"
#elif defined(MOZ_MSVC_STL_WRAP_Throw)
#  include "msvc_throw_wrapper.h"
#else
#  error "Unknown STL wrapper tactic"
#endif

#endif  // mozilla_throw_msvc_h
