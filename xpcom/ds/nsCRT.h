/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#ifndef nsCRT_h___
#define nsCRT_h___

#include <stdlib.h>
#include <ctype.h>
#include "plstr.h"
#include "nscore.h"
#include "nsCRTGlue.h"

#if defined(XP_WIN)
#  define NS_LINEBREAK "\015\012"
#  define NS_ULINEBREAK u"\015\012"
#  define NS_LINEBREAK_LEN 2
#else
#  ifdef XP_UNIX
#    define NS_LINEBREAK "\012"
#    define NS_ULINEBREAK u"\012"
#    define NS_LINEBREAK_LEN 1
#  endif /* XP_UNIX */
#endif   /* XP_WIN */

/// This is a wrapper class around all the C runtime functions.

class nsCRT {
 public:
  enum {
    LF = '\n' /* Line Feed */,
    VTAB = '\v' /* Vertical Tab */,
    CR = '\r' /* Carriage Return */
  };

  /// String comparison.
  static int32_t strcmp(const char* aStr1, const char* aStr2) {
    return int32_t(PL_strcmp(aStr1, aStr2));
  }

  /// Case-insensitive string comparison.
  static int32_t strcasecmp(const char* aStr1, const char* aStr2) {
    /* Some functions like `PL_strcasecmp` are reimplementations
     * of the their native POSIX counterparts, which breaks libFuzzer.
     * For this purpose, we use the natives instead when fuzzing.
     */
#if defined(LIBFUZZER) && defined(LINUX)
    return int32_t(::strcasecmp(aStr1, aStr2));
#else
    return int32_t(PL_strcasecmp(aStr1, aStr2));
#endif
  }

  /// Case-insensitive string comparison with length
  static int32_t strncasecmp(const char* aStr1, const char* aStr2,
                             uint32_t aMaxLen) {
#if defined(LIBFUZZER) && defined(LINUX)
    int32_t result = int32_t(::strncasecmp(aStr1, aStr2, aMaxLen));
#else
    int32_t result = int32_t(PL_strncasecmp(aStr1, aStr2, aMaxLen));
#endif
    // Egads. PL_strncasecmp is returning *very* negative numbers.
    // Some folks expect -1,0,1, so let's temper its enthusiasm.
    if (result < 0) {
      result = -1;
    }
    return result;
  }

  /// Case-insensitive substring search.
  static char* strcasestr(const char* aStr1, const char* aStr2) {
#if defined(LIBFUZZER) && defined(LINUX)
    return const_cast<char*>(::strcasestr(aStr1, aStr2));
#else
    return PL_strcasestr(aStr1, aStr2);
#endif
  }

  /**

    How to use this fancy (thread-safe) version of strtok:

    void main(void) {
      printf("%s\n\nTokens:\n", string);
      // Establish string and get the first token:
      char* newStr;
      token = nsCRT::strtok(string, seps, &newStr);
      while (token != nullptr) {
        // While there are tokens in "string"
        printf(" %s\n", token);
        // Get next token:
        token = nsCRT::strtok(newStr, seps, &newStr);
      }
    }
    * WARNING - STRTOK WHACKS str THE FIRST TIME IT IS CALLED *
    * MAKE A COPY OF str IF YOU NEED TO USE IT AFTER strtok() *
  */
  static char* strtok(char* aStr, const char* aDelims, char** aNewStr);

  /// Like strcmp except for ucs2 strings
  static int32_t strcmp(const char16_t* aStr1, const char16_t* aStr2);

  // String to longlong
  static int64_t atoll(const char* aStr);

  static char ToUpper(char aChar) { return NS_ToUpper(aChar); }
  static char ToLower(char aChar) { return NS_ToLower(aChar); }

  static bool IsAsciiSpace(char16_t aChar) {
    return NS_IsAsciiWhitespace(aChar);
  }
};

inline bool NS_IS_SPACE(char16_t aChar) {
  return ((int(aChar) & 0x7f) == int(aChar)) && isspace(int(aChar));
}

#endif /* nsCRT_h___ */
