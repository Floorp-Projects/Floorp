/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either of the GNU General Public License Version 2 or later (the "GPL"),
 * or the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */
#ifndef nsCRT_h___
#define nsCRT_h___

#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include "plstr.h"
#include "nscore.h"
#include "prtypes.h"
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

  /***
   ***  The following nsCRT::mem* functions are no longer
   ***  supported, please use the corresponding lib C
   ***  functions instead.
   ***
   ***  nsCRT::memcpy()
   ***  nsCRT::memcmp()
   ***  nsCRT::memmove()
   ***  nsCRT::memset()
   ***  nsCRT::zero()
   ***
   ***  Additionally, the following char* string utilities
   ***  are no longer supported, please use the
   ***  corresponding lib C functions instead.
   ***
   ***  nsCRT::strlen()
   ***
   ***/

  /** Compute the string length of s
   @param s the string in question
   @return the length of s
   */
  static PRUint32 strlen(const char* s) {                                       
    return PRUint32(::strlen(s));                                               
  }                                                                             

  /// Compare s1 and s2.
  static PRInt32 strcmp(const char* s1, const char* s2) {
    return PRInt32(PL_strcmp(s1, s2));
  }

  static PRInt32 strncmp(const char* s1, const char* s2,
                         PRUint32 aMaxLen) {
    return PRInt32(PL_strncmp(s1, s2, aMaxLen));
  }

  /// Case-insensitive string comparison.
  static PRInt32 strcasecmp(const char* s1, const char* s2) {
    return PRInt32(PL_strcasecmp(s1, s2));
  }

  /// Case-insensitive string comparison with length
  static PRInt32 strncasecmp(const char* s1, const char* s2, PRUint32 aMaxLen) {
    PRInt32 result=PRInt32(PL_strncasecmp(s1, s2, aMaxLen));
    //Egads. PL_strncasecmp is returning *very* negative numbers.
    //Some folks expect -1,0,1, so let's temper its enthusiasm.
    if (result<0) 
      result=-1;
    return result;
  }

  static PRInt32 strncmp(const char* s1, const char* s2, PRInt32 aMaxLen) {
    // inline the first test (assumes strings are not null):
    PRInt32 diff = ((const unsigned char*)s1)[0] - ((const unsigned char*)s2)[0];
    if (diff != 0) return diff;
    return PRInt32(PL_strncmp(s1,s2,unsigned(aMaxLen)));
  }
  
  static char* strdup(const char* str) {
    return PL_strdup(str);
  }

  static char* strndup(const char* str, PRUint32 len) {
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

  static PRUint32 strlen(const PRUnichar* s) {
    // XXXbsmedberg: remove this null-check at some point
    if (!s) {
      NS_ERROR("Passing null to nsCRT::strlen");
      return 0;
    }
    return NS_strlen(s);
  }

  /// Like strcmp except for ucs2 strings
  static PRInt32 strcmp(const PRUnichar* s1, const PRUnichar* s2);
  /// Like strcmp except for ucs2 strings
  static PRInt32 strncmp(const PRUnichar* s1, const PRUnichar* s2,
                         PRUint32 aMaxLen);

  // The GNU libc has memmem, which is strstr except for binary data
  // This is our own implementation that uses memmem on platforms
  // where it's available.
  static const char* memmem(const char* haystack, PRUint32 haystackLen,
                            const char* needle, PRUint32 needleLen);

  // You must use nsCRT::free(PRUnichar*) to free memory allocated
  // by nsCRT::strdup(PRUnichar*).
  static PRUnichar* strdup(const PRUnichar* str);

  // You must use nsCRT::free(PRUnichar*) to free memory allocated
  // by strndup(PRUnichar*, PRUint32).
  static PRUnichar* strndup(const PRUnichar* str, PRUint32 len);

  static void free(PRUnichar* str) {
  	nsCppSharedAllocator<PRUnichar> shared_allocator;
  	shared_allocator.deallocate(str, 0 /*we never new or kept the size*/);
  }

  // Computes the hashcode for a c-string, returns the string length as
  // an added bonus.
  static PRUint32 HashCode(const char* str,
                           PRUint32* resultingStrLen = nsnull);

  // Computes the hashcode for a length number of bytes of c-string data.
  static PRUint32 HashCode(const char* start, PRUint32 length);

  // Computes the hashcode for a ucs2 string, returns the string length
  // as an added bonus.
  static PRUint32 HashCode(const PRUnichar* str,
                           PRUint32* resultingStrLen = nsnull);

  // Computes the hashcode for a buffer with a specified length.
  static PRUint32 HashCode(const PRUnichar* str, PRUint32 strLen);

  // Computes a hashcode for a length number of UTF8
  // characters. Returns the same hash code as the HashCode method
  // taking a |PRUnichar*| would if the string were converted to UTF16.
  static PRUint32 HashCodeAsUTF16(const char* start, PRUint32 length,
                                  bool* err);

  // String to longlong
  static PRInt64 atoll(const char *str);
  
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
  static bool IsAscii(const char* aString, PRUint32 aLength) { return NS_IsAscii(aString, aLength); }
};


#define NS_IS_SPACE(VAL) \
  (((((intn)(VAL)) & 0x7f) == ((intn)(VAL))) && isspace((intn)(VAL)) )

#define NS_IS_CNTRL(i)   ((((unsigned int) (i)) > 0x7f) ? (int) 0 : iscntrl(i))
#define NS_IS_DIGIT(i)   ((((unsigned int) (i)) > 0x7f) ? (int) 0 : isdigit(i))
#if defined(XP_WIN) || defined(XP_OS2)
#define NS_IS_ALPHA(VAL) (isascii((int)(VAL)) && isalpha((int)(VAL)))
#else
#define NS_IS_ALPHA(VAL) ((((unsigned int) (VAL)) > 0x7f) ? (int) 0 : isalpha((int)(VAL)))
#endif


#endif /* nsCRT_h___ */
