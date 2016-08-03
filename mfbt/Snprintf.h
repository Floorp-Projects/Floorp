/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* Polyfills snprintf() on platforms that don't provide it, and provides
 * related utilities. */

#ifndef mozilla_Snprintf_h_
#define mozilla_Snprintf_h_

#include <stdio.h>
#include <stdarg.h>

// In addition, in C++ code, on all platforms, provide an snprintf_literal()
// function which uses template argument deduction to deduce the size of the
// buffer, avoiding the need for the user to pass it in explicitly.
#ifdef __cplusplus
template <size_t N>
#if defined(__GNUC__)
  __attribute__((format(printf, 2, 3)))
#endif
int snprintf_literal(char (&buffer)[N], const char* format, ...)
{
  va_list args;
  va_start(args, format);
  int result = vsnprintf(buffer, N, format, args);
  va_end(args);
  buffer[N - 1] = '\0';
  return result;
}
#endif

#endif  /* mozilla_Snprintf_h_ */
