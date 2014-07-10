/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
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
#  define NS_LINEBREAK           "\015\012"
#  define NS_LINEBREAK_LEN       2
#else
#  ifdef XP_UNIX
#    define NS_LINEBREAK         "\012"
#    define NS_LINEBREAK_LEN     1
#  endif /* XP_UNIX */
#endif /* XP_WIN */

extern const char16_t kIsoLatin1ToUCS2[256];

// This macro can be used in a class declaration for classes that want
// to ensure that their instance memory is zeroed.
#define NS_DECL_AND_IMPL_ZEROING_OPERATOR_NEW   \
  void* operator new(size_t sz) CPP_THROW_NEW { \
    void* rv = ::operator new(sz);              \
    if (rv) {                                   \
      memset(rv, 0, sz);                        \
    }                                           \
    return rv;                                  \
  }                                             \
  void operator delete(void* ptr) {             \
    ::operator delete(ptr);                     \
  }

// This macro works with the next macro to declare a non-inlined
// version of the above.
#define NS_DECL_ZEROING_OPERATOR_NEW           \
  void* operator new(size_t sz) CPP_THROW_NEW; \
  void operator delete(void* ptr);

#define NS_IMPL_ZEROING_OPERATOR_NEW(_class)            \
  void* _class::operator new(size_t sz) CPP_THROW_NEW { \
    void* rv = ::operator new(sz);                      \
    if (rv) {                                           \
      memset(rv, 0, sz);                                \
    }                                                   \
    return rv;                                          \
  }                                                     \
  void _class::operator delete(void* ptr) {             \
    ::operator delete(ptr);                             \
  }

// Freeing helper
#define CRTFREEIF(x) if (x) { nsCRT::free(x); x = 0; }

/// This is a wrapper class around all the C runtime functions.

class nsCRT
{
public:
  enum
  {
    LF = '\n'   /* Line Feed */,
    VTAB = '\v' /* Vertical Tab */,
    CR = '\r'   /* Carriage Return */
  };

  /// String comparison.
  static int32_t strcmp(const char* aStr1, const char* aStr2)
  {
    return int32_t(PL_strcmp(aStr1, aStr2));
  }

  static int32_t strncmp(const char* aStr1, const char* aStr2,
                         uint32_t aMaxLen)
  {
    return int32_t(PL_strncmp(aStr1, aStr2, aMaxLen));
  }

  /// Case-insensitive string comparison.
  static int32_t strcasecmp(const char* aStr1, const char* aStr2)
  {
    return int32_t(PL_strcasecmp(aStr1, aStr2));
  }

  /// Case-insensitive string comparison with length
  static int32_t strncasecmp(const char* aStr1, const char* aStr2,
                             uint32_t aMaxLen)
  {
    int32_t result = int32_t(PL_strncasecmp(aStr1, aStr2, aMaxLen));
    //Egads. PL_strncasecmp is returning *very* negative numbers.
    //Some folks expect -1,0,1, so let's temper its enthusiasm.
    if (result < 0) {
      result = -1;
    }
    return result;
  }

  static int32_t strncmp(const char* aStr1, const char* aStr2, int32_t aMaxLen)
  {
    // inline the first test (assumes strings are not null):
    int32_t diff =
      ((const unsigned char*)aStr1)[0] - ((const unsigned char*)aStr2)[0];
    if (diff != 0) {
      return diff;
    }
    return int32_t(PL_strncmp(aStr1, aStr2, unsigned(aMaxLen)));
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
  /// Like strcmp except for ucs2 strings
  static int32_t strncmp(const char16_t* aStr1, const char16_t* aStr2,
                         uint32_t aMaxLen);

  // The GNU libc has memmem, which is strstr except for binary data
  // This is our own implementation that uses memmem on platforms
  // where it's available.
  static const char* memmem(const char* aHaystack, uint32_t aHaystackLen,
                            const char* aNeedle, uint32_t aNeedleLen);

  // String to longlong
  static int64_t atoll(const char* aStr);

  static char ToUpper(char aChar) { return NS_ToUpper(aChar); }
  static char ToLower(char aChar) { return NS_ToLower(aChar); }

  static bool IsUpper(char aChar) { return NS_IsUpper(aChar); }
  static bool IsLower(char aChar) { return NS_IsLower(aChar); }

  static bool IsAscii(char16_t aChar) { return NS_IsAscii(aChar); }
  static bool IsAscii(const char16_t* aString) { return NS_IsAscii(aString); }
  static bool IsAsciiAlpha(char16_t aChar) { return NS_IsAsciiAlpha(aChar); }
  static bool IsAsciiDigit(char16_t aChar) { return NS_IsAsciiDigit(aChar); }
  static bool IsAsciiSpace(char16_t aChar) { return NS_IsAsciiWhitespace(aChar); }
  static bool IsAscii(const char* aString) { return NS_IsAscii(aString); }
  static bool IsAscii(const char* aString, uint32_t aLength)
  {
    return NS_IsAscii(aString, aLength);
  }
};


inline bool
NS_IS_SPACE(char16_t aChar)
{
  return ((int(aChar) & 0x7f) == int(aChar)) && isspace(int(aChar));
}

#define NS_IS_CNTRL(i)   ((((unsigned int) (i)) > 0x7f) ? (int) 0 : iscntrl(i))
#define NS_IS_DIGIT(i)   ((((unsigned int) (i)) > 0x7f) ? (int) 0 : isdigit(i))
#if defined(XP_WIN)
#define NS_IS_ALPHA(VAL) (isascii((int)(VAL)) && isalpha((int)(VAL)))
#else
#define NS_IS_ALPHA(VAL) ((((unsigned int) (VAL)) > 0x7f) ? (int) 0 : isalpha((int)(VAL)))
#endif


#endif /* nsCRT_h___ */
