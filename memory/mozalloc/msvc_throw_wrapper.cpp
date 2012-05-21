/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: sw=4 ts=4 et :
 */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include <exception>

#if defined(XP_WIN) || defined(XP_OS2)
#  define MOZALLOC_EXPORT __declspec(dllexport)
#endif

#include "mozilla/mozalloc_abort.h"

namespace std {

// NB: user code is not supposed to touch the std:: namespace.  We're
// doing this after careful review because we want to define our own
// exception throwing semantics.  Don't try this at home!

__declspec(dllexport) void mozilla_Throw(const exception& e);

void
mozilla_Throw(const exception& e)
{
    mozalloc_abort(e.what());
}

}  // namespace std
