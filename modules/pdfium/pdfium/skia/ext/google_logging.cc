// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file provides integration with Google-style "base/logging.h" assertions
// for Skia SkASSERT. If you don't want this, you can link with another file
// that provides integration with the logging of your choice.

#include <stdarg.h>
#include <stdio.h>

#include "third_party/skia/include/core/SkTypes.h"

void SkDebugf_FileLine(const char* file,
                       int line,
                       bool fatal,
                       const char* format,
                       ...) {
  va_list ap;
  va_start(ap, format);

  fprintf(stderr, "%s:%d ", file, line);
  vfprintf(stderr, format, ap);
  va_end(ap);
}
