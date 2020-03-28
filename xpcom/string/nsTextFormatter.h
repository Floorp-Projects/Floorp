/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/*
 * This code was copied from xpcom/ds/nsTextFormatter r1.3
 *           Memory model and Frozen linkage changes only.
 *                           -- Prasad <prasad@medhas.org>
 */

#ifndef nsTextFormatter_h___
#define nsTextFormatter_h___

/*
 ** API for PR printf like routines. Supports the following formats
 **	%d - decimal
 **	%u - unsigned decimal
 **	%x - unsigned hex
 **	%X - unsigned uppercase hex
 **	%o - unsigned octal
 **	%hd, %hu, %hx, %hX, %ho - 16-bit versions of above
 **	%ld, %lu, %lx, %lX, %lo - 32-bit versions of above
 **	%lld, %llu, %llx, %llX, %llo - 64 bit versions of above
 **	%s - utf8 string
 **	%S - char16_t string
 **	%c - character
 **	%p - pointer (deals with machine dependent pointer size)
 **	%f - float
 **	%g - float
 */
#include <stdio.h>
#include <stdarg.h>
#include "nscore.h"
#include "nsString.h"
#include "mozilla/Span.h"

#ifdef XPCOM_GLUE
#  error \
      "nsTextFormatter is not available in the standalone glue due to NSPR dependencies."
#endif

class nsTextFormatter {
 public:
  /*
   * sprintf into a fixed size buffer. Guarantees that the buffer is null
   * terminated. Returns the length of the written output, NOT including the
   * null terminator, or (uint32_t)-1 if an error occurs.
   */
  template <typename... T>
  static uint32_t snprintf(char16_t* aOut, uint32_t aOutLen,
                           const char16_t* aFmt, T... aArgs) {
    BoxedValue values[] = {BoxedValue(aArgs)...};
    return vsnprintf(aOut, aOutLen, aFmt,
                     mozilla::MakeSpan(values, sizeof...(aArgs)));
  }

  /*
   * sprintf into an existing nsAString, overwriting any contents it already
   * has. Infallible.
   */
  template <typename... T>
  static void ssprintf(nsAString& aOut, const char16_t* aFmt, T... aArgs) {
    BoxedValue values[] = {BoxedValue(aArgs)...};
    vssprintf(aOut, aFmt, mozilla::MakeSpan(values, sizeof...(aArgs)));
  }

 private:
  enum ArgumentKind {
    INT,
    UINT,
    POINTER,
    DOUBLE,
    STRING,
    STRING16,
    INTPOINTER,
  };

  union ValueUnion {
    int64_t mInt;
    uint64_t mUInt;
    void const* mPtr;
    double mDouble;
    char const* mString;
    char16_t const* mString16;
    int* mIntPtr;
  };

  struct BoxedValue {
    ArgumentKind mKind;
    ValueUnion mValue;

    explicit BoxedValue(int aArg) : mKind(INT) { mValue.mInt = aArg; }

    explicit BoxedValue(unsigned int aArg) : mKind(UINT) {
      mValue.mUInt = aArg;
    }

    explicit BoxedValue(long aArg) : mKind(INT) { mValue.mInt = aArg; }

    explicit BoxedValue(unsigned long aArg) : mKind(UINT) {
      mValue.mUInt = aArg;
    }

    explicit BoxedValue(long long aArg) : mKind(INT) { mValue.mInt = aArg; }

    explicit BoxedValue(unsigned long long aArg) : mKind(UINT) {
      mValue.mUInt = aArg;
    }

    explicit BoxedValue(const void* aArg) : mKind(POINTER) {
      mValue.mPtr = aArg;
    }

    explicit BoxedValue(double aArg) : mKind(DOUBLE) { mValue.mDouble = aArg; }

    explicit BoxedValue(const char* aArg) : mKind(STRING) {
      mValue.mString = aArg;
    }

    explicit BoxedValue(const char16_t* aArg) : mKind(STRING16) {
      mValue.mString16 = aArg;
    }

#if defined(MOZ_USE_CHAR16_WRAPPER)
    explicit BoxedValue(const char16ptr_t aArg) : mKind(STRING16) {
      mValue.mString16 = aArg;
    }

#endif

    explicit BoxedValue(int* aArg) : mKind(INTPOINTER) {
      mValue.mIntPtr = aArg;
    }

    bool IntCompatible() const { return mKind == INT || mKind == UINT; }

    bool PointerCompatible() const {
      return mKind == POINTER || mKind == STRING || mKind == STRING16 ||
             mKind == INTPOINTER;
    }
  };

  struct SprintfStateStr;

  static int fill2(SprintfStateStr* aState, const char16_t* aSrc, int aSrcLen,
                   int aWidth, int aFlags);
  static int fill_n(SprintfStateStr* aState, const char16_t* aSrc, int aSrcLen,
                    int aWidth, int aPrec, int aFlags);
  static int cvt_ll(SprintfStateStr* aState, uint64_t aNum, int aWidth,
                    int aPrec, int aRadix, int aFlags, const char16_t* aHexStr);
  static int cvt_f(SprintfStateStr* aState, double aDouble, int aWidth,
                   int aPrec, const char16_t aType, int aFlags);
  static int cvt_S(SprintfStateStr* aState, const char16_t* aStr, int aWidth,
                   int aPrec, int aFlags);
  static int cvt_s(SprintfStateStr* aState, const char* aStr, int aWidth,
                   int aPrec, int aFlags);
  static int dosprintf(SprintfStateStr* aState, const char16_t* aFmt,
                       mozilla::Span<BoxedValue> aValues);
  static int StringStuff(SprintfStateStr* aState, const char16_t* aStr,
                         uint32_t aLen);
  static int LimitStuff(SprintfStateStr* aState, const char16_t* aStr,
                        uint32_t aLen);
  static uint32_t vsnprintf(char16_t* aOut, uint32_t aOutLen,
                            const char16_t* aFmt,
                            mozilla::Span<BoxedValue> aValues);
  static void vssprintf(nsAString& aOut, const char16_t* aFmt,
                        mozilla::Span<BoxedValue> aValues);
};

#endif /* nsTextFormatter_h___ */
