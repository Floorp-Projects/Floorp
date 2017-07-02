// Copyright (c) 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PDFIUM_THIRD_PARTY_BASE_LOGGING_H_
#define PDFIUM_THIRD_PARTY_BASE_LOGGING_H_

#include <assert.h>
#include <stdlib.h>

#ifndef _WIN32
#define NULL_DEREF_IF_POSSIBLE \
  *(reinterpret_cast<volatile char*>(NULL) + 42) = 0x42;
#else
#define NULL_DEREF_IF_POSSIBLE
#endif

#define CHECK(condition)   \
  if (!(condition)) {      \
    abort();               \
    NULL_DEREF_IF_POSSIBLE \
  }

#define NOTREACHED() assert(false)

#endif  // PDFIUM_THIRD_PARTY_BASE_LOGGING_H_
