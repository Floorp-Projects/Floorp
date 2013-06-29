/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef MOZ_DECIMAL_UTILS_H
#define MOZ_DECIMAL_UTILS_H

// This file contains extra includes, defines and typedefs to allow compilation
// of Decimal.cpp under the Mozilla source without blink core dependencies. Do
// not include it into any file other than Decimal.cpp.

#include "../double-conversion/double-conversion.h"
#include "mozilla/Util.h"
#include "mozilla/Casting.h"
#include "mozilla/FloatingPoint.h"
#include "mozilla/NullPtr.h"

#include <cmath>
#include <cstring>
#include <iomanip>
#include <limits>
#include <sstream>

#ifndef UINT64_C
// For Android toolchain
#define UINT64_C(c) (c ## ULL)
#endif

#ifdef ASSERT
#undef ASSERT
#endif
#define ASSERT MOZ_ASSERT

#define ASSERT_NOT_REACHED() MOZ_ASSUME_NOT_REACHED()

#define WTF_MAKE_NONCOPYABLE(ClassName) \
  private: \
    ClassName(const ClassName&) MOZ_DELETE; \
    void operator=(const ClassName&) MOZ_DELETE;

#if defined(_MSC_VER) && (_MSC_VER <= 1700)
namespace std {
  inline bool isinf(double num) { return mozilla::IsInfinite(num); }
  inline bool isnan(double num) { return mozilla::IsNaN(num); }
  inline bool isfinite(double num) { return mozilla::IsFinite(num); }
}
#endif

typedef std::string String;

double mozToDouble(const String &aStr, bool *valid) {
  double_conversion::StringToDoubleConverter converter(
    double_conversion::StringToDoubleConverter::NO_FLAGS,
    mozilla::UnspecifiedNaN(), mozilla::UnspecifiedNaN(), nullptr, nullptr);
  const char* str = aStr.c_str();
  int length = mozilla::SafeCast<int>(strlen(str));
  int processed_char_count; // unused - NO_FLAGS requires the whole string to parse
  double result = converter.StringToDouble(str, length, &processed_char_count);
  *valid = mozilla::IsFinite(result);
  return result;
}

String mozToString(double aNum) {
  char buffer[64];
  int buffer_length = mozilla::ArrayLength(buffer);
  const double_conversion::DoubleToStringConverter& converter =
    double_conversion::DoubleToStringConverter::EcmaScriptConverter();
  double_conversion::StringBuilder builder(buffer, buffer_length);
  converter.ToShortest(aNum, &builder);
  return String(builder.Finalize());
}

String mozToString(int64_t aNum) {
  std::ostringstream o;
  o << std::setprecision(std::numeric_limits<int64_t>::digits10) << aNum;
  return o.str();
}

String mozToString(uint64_t aNum) {
  std::ostringstream o;
  o << std::setprecision(std::numeric_limits<uint64_t>::digits10) << aNum;
  return o.str();
}

class StringBuilder
{
public:
  void append(char c) {
    mStr += c;
  }
  void appendLiteral(const char *aStr) {
    mStr += aStr;
  }
  void appendNumber(int aNum) {
    mStr += mozToString(int64_t(aNum));
  }
  void append(const String& aStr) {
    mStr += aStr;
  }
  std::string toString() const {
    return mStr;
  }
private:
  std::string mStr;
};

#endif

