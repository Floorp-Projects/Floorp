/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/Attributes.h"

#ifndef _MSC_VER  // Not supported by clang-cl yet

extern "C" const char* __ubsan_default_options() {
  return "print_stacktrace=1";
}

extern "C" const char* __ubsan_default_suppressions() { return ""; }

#endif
