/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: NPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.1 (the "License"); you may not use this file except in
 * compliance with the License. You may obtain a copy of the License at
 * http://www.mozilla.org/NPL/
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
 * either the GNU General Public License Version 2 or later (the "GPL"), or 
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the NPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the NPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */
#ifndef nsCRT_h___
#define nsCRT_h___

#include <stdlib.h>
#include <string.h>
#include "plstr.h"
#include "nscore.h"
#include "prtypes.h"
#include "nsCppSharedAllocator.h"

#ifdef XP_MAC
#  define NS_LINEBREAK             "\015"
#  define NS_LINEBREAK_LEN 1
#else
#  ifdef XP_PC
#    define NS_LINEBREAK           "\015\012"
#    define NS_LINEBREAK_LEN       2
#  else
#    if defined(XP_UNIX) || defined(XP_BEOS)
#      define NS_LINEBREAK         "\012"
#      define NS_LINEBREAK_LEN     1
#    endif /* XP_UNIX */
#  endif /* XP_PC */
#endif /* XP_MAC */

extern const PRUnichar kIsoLatin1ToUCS2[256];

// This macro can be used in a class declaration for classes that want
// to ensure that their instance memory is zeroed.
#define NS_DECL_AND_IMPL_ZEROING_OPERATOR_NEW \
  void* operator new(size_t sz) {             \
    void* rv = ::operator new(sz);            \
    if (rv) {                                 \
      nsCRT::zero(rv, sz);                    \
    }                                         \
    return rv;                                \
  }                                           \
  void operator delete(void* ptr) {           \
    ::operator delete(ptr);                   \
  }

// This macro works with the next macro to declare a non-inlined
// version of the above.
#define NS_DECL_ZEROING_OPERATOR_NEW \
  void* operator new(size_t sz);     \
  void operator delete(void* ptr);

#define NS_IMPL_ZEROING_OPERATOR_NEW(_class) \
  void* _class::operator new(size_t sz) {    \
    void* rv = ::operator new(sz);           \
    if (rv) {                                \
      nsCRT::zero(rv, sz);                   \
    }                                        \
    return rv;                               \
  }                                          \
  void _class::operator delete(void* ptr) {  \
    ::operator delete(ptr);                  \
  }

// Freeing helper
#define CRTFREEIF(x) if (x) { nsCRT::free(x); x = 0; }

/// This is a wrapper class around all the C runtime functions. 

class NS_COM nsCRT {
public:
  enum {
    TAB='\t'  /* Horizontal Tab */,
    LF='\n'   /* Line Feed */,
    VTAB='\v' /* Vertical Tab */,
    FF='\f'   /* Form Feed */,
    CR='\r'   /* Carriage Return */
  };

  /** Copy bytes from aSrc to aDest.
    @param aDest the destination address
    @param aSrc the source address
    @param aCount the number of bytes to copy
    */
  static void memcpy(void* aDest, const void* aSrc, PRUint32 aCount) {
    NS_ASSERTION((aDest != NULL && aSrc != NULL) || (aCount == 0), "Invalid NULL argument");
    ::memcpy(aDest, aSrc, (size_t)aCount);
  }

  static PRInt32 memcmp(const void* aDest, const void* aSrc, PRUint32 aCount) {
    NS_ASSERTION((aDest != NULL && aSrc != NULL) || (aCount == 0), "Invalid NULL argument");
    return ::memcmp(aDest, aSrc, (size_t)aCount);
  }

  static void memmove(void* aDest, const void* aSrc, PRUint32 aCount) {
    NS_ASSERTION((aDest != NULL && aSrc != NULL) || (aCount == 0), "Invalid NULL argument");
    ::memmove(aDest, aSrc, (size_t)aCount);
  }

  static void memset(void* aDest, PRUint8 aByte, PRUint32 aCount) {
    NS_ASSERTION((aDest != NULL) || (aCount == 0), "Invalid NULL argument");
    ::memset(aDest, aByte, aCount);
  }

  static void zero(void* aDest, PRUint32 aCount) {
    NS_ASSERTION((aDest != NULL) || (aCount == 0), "Invalid NULL argument");
    ::memset(aDest, 0, (size_t)aCount);
  }

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
    //Egags. PL_strncasecmp is returning *very* negative numbers.
    //Some folks expect -1,0,1, so let's temper it's enthusiasm.
    if(result<0) 
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

    void main( void ) {
      printf( "%s\n\nTokens:\n", string );
      // Establish string and get the first token:
      char* newStr;
      token = nsCRT::strtok( string, seps, &newStr );   
      while( token != NULL ) {
        // While there are tokens in "string"
        printf( " %s\n", token );
        // Get next token:
        token = nsCRT::strtok( newStr, seps, &newStr );
      }
    }
    * WARNING - STRTOK WHACKS str THE FIRST TIME IT IS CALLED *
    * MAKE A COPY OF str IF YOU NEED TO USE IT AFTER strtok() *
  */
  static char* strtok(char* str, const char* delims, char* *newStr); 

  /// Like strlen except for ucs2 strings
  static PRUint32 strlen(const PRUnichar* s);

  /// Like strcmp except for ucs2 strings
  static PRInt32 strcmp(const PRUnichar* s1, const PRUnichar* s2);
  /// Like strcmp except for ucs2 strings
  static PRInt32 strncmp(const PRUnichar* s1, const PRUnichar* s2,
                         PRUint32 aMaxLen);

  /// Like strcasecmp except for ucs2 strings
  static PRInt32 strcasecmp(const PRUnichar* s1, const PRUnichar* s2);
  /// Like strncasecmp except for ucs2 strings
  static PRInt32 strncasecmp(const PRUnichar* s1, const PRUnichar* s2,
                             PRUint32 aMaxLen);

  /// Like strcmp with a char* and a ucs2 string
  static PRInt32 strcmp(const PRUnichar* s1, const char* s2);
  /// Like strncmp with a char* and a ucs2 string
  static PRInt32 strncmp(const PRUnichar* s1, const char* s2,
                         PRUint32 aMaxLen);

  /// Like strcasecmp with a char* and a ucs2 string
  static PRInt32 strcasecmp(const PRUnichar* s1, const char* s2);
  /// Like strncasecmp with a char* and a ucs2 string
  static PRInt32 strncasecmp(const PRUnichar* s1, const char* s2,
                             PRUint32 aMaxLen);

  // Note: uses new[] to allocate memory, so you must use delete[] to
  // free the memory
  static PRUnichar* strdup(const PRUnichar* str);

  static PRUnichar* strndup(const PRUnichar* str, PRUint32 len);

  static void free(PRUnichar* str) {
  	nsCppSharedAllocator<PRUnichar> shared_allocator;
  	shared_allocator.deallocate(str, 0 /*we never new or kept the size*/);
  }

  // Computes the hashcode for a c-string, returns the string length as an added bonus.
  static PRUint32 HashCode(const char* str, PRUint32* resultingStrLen = nsnull);

  // Computes the hashcode for a ucs2 string, returns the string length as an added bonus.
  static PRUint32 HashCode(const PRUnichar* str, PRUint32* resultingStrLen = nsnull);

  // Computes the hashcode for a buffer with a specified length.
  static PRUint32 BufferHashCode(const char* str, PRUint32 strLen);

  /// String to integer.
  static PRInt32 atoi( const PRUnichar *string );

  // String to longlong
  static PRInt64 atoll(const char *str);

  static PRUnichar ToUpper(PRUnichar aChar);

  static PRUnichar ToLower(PRUnichar aChar);
  
  static PRBool IsUpper(PRUnichar aChar);

  static PRBool IsLower(PRUnichar aChar);

  static char ToUpper(char aChar);

  static char ToLower(char aChar);
  
  static PRBool IsUpper(char aChar);

  static PRBool IsLower(char aChar);

  static PRBool IsAscii(PRUnichar aChar);
  static PRBool IsAscii(const PRUnichar* aString);
  static PRBool IsAsciiAlpha(PRUnichar aChar);
  static PRBool IsAsciiDigit(PRUnichar aChar);
  static PRBool IsAsciiSpace(PRUnichar aChar);
  static PRBool IsAscii(const char* aString);
};

#define FF '\014'
#define TAB '\011'

#define CRSTR "\015"
#define LFSTR "\012"
#define CRLF "\015\012"     /* A CR LF equivalent string */

#endif /* nsCRT_h___ */
