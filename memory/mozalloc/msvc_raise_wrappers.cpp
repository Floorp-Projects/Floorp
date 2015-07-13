/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: sw=4 ts=4 et :
 */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include <stdio.h>

#include "mozalloc_abort.h"

#define MOZALLOC_DONT_WRAP_RAISE_FUNCTIONS
#include "mozilla/throw_msvc.h"

__declspec(noreturn) static void abort_from_exception(const char* const which,
                                                      const char* const what);
static void
abort_from_exception(const char* const which,  const char* const what)
{
    fprintf(stderr, "fatal: STL threw %s: ", which);
    mozalloc_abort(what);
}

namespace std {

// NB: user code is not supposed to touch the std:: namespace.  We're
// doing this after careful review because we want to define our own
// exception throwing semantics.  Don't try this at home!

void
moz_Xinvalid_argument(const char* what)
{
    abort_from_exception("invalid_argument", what);
}

void
moz_Xlength_error(const char* what)
{
    abort_from_exception("length_error", what);
}

void
moz_Xout_of_range(const char* what)
{
    abort_from_exception("out_of_range", what);
}

void
moz_Xoverflow_error(const char* what)
{
    abort_from_exception("overflow_error", what);
}

void
moz_Xruntime_error(const char* what)
{
    abort_from_exception("runtime_error", what);
}

} // namespace std
