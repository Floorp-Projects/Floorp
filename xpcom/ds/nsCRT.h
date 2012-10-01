/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#ifndef nsCRT_h___
#define nsCRT_h___

#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include "plstr.h"
#include "nscore.h"
#include "nsCppSharedAllocator.h"
#include "nsCRTGlue.h"

#if defined(XP_WIN) || defined(XP_OS2)
#  define NS_LINEBREAK           "\015\012"
#  define NS_LINEBREAK_LEN       2
#else
#  ifdef XP_UNIX
#    define NS_LINEBREAK         "\012"
#    define NS_LINEBREAK_LEN     1
#  endif /* XP_UNIX */
#endif /* XP_WIN || XP_OS2 */

extern const PRUnichar kIsoLatin1ToUCS2[256];

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

class nsCRT {
public:
  enum {
    LF='\n'   /* Line Feed */,
    VTAB='\v' /* Vertical Tab */,
    CR='\r'   /* Carriage Return */
  };

  /// Compare s1 and s2.
  static int32_t strcmp(const char* s1, const char* s2) {
    return int32_t(PL_strcmp(s1, s2));
  }

  static int32_t strncmp(const char* s1, const char* s2,
                         uint32_t aMaxLen) {
    return int32_t(PL_strncmp(s1, s2, aMaxLen));
  }

  /// Case-insensitive string comparison.
  static int32_t strcasecmp(const char* s1, const char* s2) {
    return int32_t(PL_strcasecmp(s1, s2));
  }

  /// Case-insensitive string comparison with length
  static int32_t strncasecmp(const char* s1, const char* s2, uint32_t aMaxLen) {
    int32_t result=int32_t(PL_strncasecmp(s1, s2, aMaxLen));
    //Egads. PL_strncasecmp is returning *very* negative numbers.
    //Some folks expect -1,0,1, so let's temper its enthusiasm.
    if (result<0) 
      result=-1;
    return result;
  }

  static int32_t strncmp(const char* s1, const char* s2, int32_t aMaxLen) {
    // inline the first test (assumes strings are not null):
    int32_t diff = ((const unsigned char*)s1)[0] - ((const unsigned char*)s2)[0];
    if (diff != 0) return diff;
    return int32_t(PL_strncmp(s1,s2,unsigned(aMaxLen)));
  }
  
  static char* strdup(const char* str) {
    return PL_strdup(str);
  }

  static char* strndup(const char* str, uint32_t len) {
    return PL_strndup(str, len);
  }

  static void free(char* str) {
    PL_strfree(str);
  }

  /**

    How to use this fancy (thread-safe) version of strtok: 

    void main(void) {
      printf("%s\n\nTokens:\n", string);
      // Establish string and get the first token:
      char* newStr;
      token = nsCRT::strtok(string, seps, &newStr);   
      while (token != NULL) {
        // While there are tokens in "string"
        printf(" %s\n", token);
        // Get next token:
        token = nsCRT::strtok(newStr, seps, &newStr);
      }
    }
    * WARNING - STRTOK WHACKS str THE FIRST TIME IT IS CALLED *
    * MAKE A COPY OF str IF YOU NEED TO USE IT AFTER strtok() *
  */
  static char* strtok(char* str, const char* delims, char* *newStr); 

  /// Like strcmp except for ucs2 strings
  static int32_t strcmp(const PRUnichar* s1, const PRUnichar* s2);
  /// Like strcmp except for ucs2 strings
  static int32_t strncmp(const PRUnichar* s1, const PRUnichar* s2,
                         uint32_t aMaxLen);

  // The GNU libc has memmem, which is strstr except for binary data
  // This is our own implementation that uses memmem on platforms
  // where it's available.
  static const char* memmem(const char* haystack, uint32_t haystackLen,
                            const char* needle, uint32_t needleLen);

  // You must use nsCRT::free(PRUnichar*) to free memory allocated
  // by nsCRT::strdup(PRUnichar*).
  static PRUnichar* strdup(const PRUnichar* str);

  // You must use nsCRT::free(PRUnichar*) to free memory allocated
  // by strndup(PRUnichar*, uint32_t).
  static PRUnichar* strndup(const PRUnichar* str, uint32_t len);

  static void free(PRUnichar* str) {
  	nsCppSharedAllocator<PRUnichar> shared_allocator;
  	shared_allocator.deallocate(str, 0 /*we never new or kept the size*/);
  }

  // String to longlong
  static int64_t atoll(const char *str);
  
  static char ToUpper(char aChar) { return NS_ToUpper(aChar); }
  static char ToLower(char aChar) { return NS_ToLower(aChar); }
  
  static bool IsUpper(char aChar) { return NS_IsUpper(aChar); }
  static bool IsLower(char aChar) { return NS_IsLower(aChar); }

  static bool IsAscii(PRUnichar aChar) { return NS_IsAscii(aChar); }
  static bool IsAscii(const PRUnichar* aString) { return NS_IsAscii(aString); }
  static bool IsAsciiAlpha(PRUnichar aChar) { return NS_IsAsciiAlpha(aChar); }
  static bool IsAsciiDigit(PRUnichar aChar) { return NS_IsAsciiDigit(aChar); }
  static bool IsAsciiSpace(PRUnichar aChar) { return NS_IsAsciiWhitespace(aChar); }
  static bool IsAscii(const char* aString) { return NS_IsAscii(aString); }
  static bool IsAscii(const char* aString, uint32_t aLength) { return NS_IsAscii(aString, aLength); }
};


inline bool
NS_IS_SPACE(PRUnichar c)
{
  return ((int(c) & 0x7f) == int(c)) && isspace(int(c));
}

#define NS_IS_CNTRL(i)   ((((unsigned int) (i)) > 0x7f) ? (int) 0 : iscntrl(i))
#define NS_IS_DIGIT(i)   ((((unsigned int) (i)) > 0x7f) ? (int) 0 : isdigit(i))
#if defined(XP_WIN) || defined(XP_OS2)
#define NS_IS_ALPHA(VAL) (isascii((int)(VAL)) && isalpha((int)(VAL)))
#else
#define NS_IS_ALPHA(VAL) ((((unsigned int) (VAL)) > 0x7f) ? (int) 0 : isalpha((int)(VAL)))
#endif


#endif /* nsCRT_h___ */
