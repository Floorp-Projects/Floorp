/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* Provides a safer sprintf for printing to fixed-size character arrays. */

#ifndef mozilla_Sprintf_h_
#define mozilla_Sprintf_h_

#include <stdio.h>
#include <stdarg.h>
#include <algorithm>

#include "mozilla/Assertions.h"
#include "mozilla/Attributes.h"
#include "mozilla/Printf.h"

#ifdef __cplusplus

#  ifndef SPRINTF_H_USES_VSNPRINTF
namespace mozilla {
namespace detail {

struct MOZ_STACK_CLASS SprintfAppend final : public mozilla::PrintfTarget {
  template <size_t N>
  explicit SprintfAppend(char (&aBuf)[N]) : mBuf(aBuf), mBufLen(N) {}

  bool append(const char* aStr, size_t aLen) override {
    if (aLen == 0) {
      return true;
    }
    // Don't copy more than what's left to use.
    size_t copy = std::min(mBufLen, aLen);
    if (copy > 0) {
      memcpy(mBuf, aStr, copy);
      mBuf += copy;
      mBufLen -= copy;
    }
    return true;
  }

 private:
  char* mBuf;
  size_t mBufLen;
};

}  // namespace detail
}  // namespace mozilla
#  endif  // SPRINTF_H_USES_VSNPRINTF

template <size_t N>
MOZ_FORMAT_PRINTF(2, 0)
int VsprintfLiteral(char (&buffer)[N], const char* format, va_list args) {
  MOZ_ASSERT(format != buffer);
#  ifdef SPRINTF_H_USES_VSNPRINTF
  int result = vsnprintf(buffer, N, format, args);
  buffer[N - 1] = '\0';
  return result;
#  else
  mozilla::detail::SprintfAppend ss(buffer);
  ss.vprint(format, args);
  size_t len = ss.emitted();
  buffer[std::min(len, N - 1)] = '\0';
  return len;
#  endif
}

template <size_t N>
MOZ_FORMAT_PRINTF(2, 3)
int SprintfLiteral(char (&buffer)[N], const char* format, ...) {
  va_list args;
  va_start(args, format);
  int result = VsprintfLiteral(buffer, format, args);
  va_end(args);
  return result;
}

#endif
#endif /* mozilla_Sprintf_h_ */
