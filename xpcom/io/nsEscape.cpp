/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

//  First checked in on 98/12/03 by John R. McMullen, derived from net.h/mkparse.c.

#include "nsEscape.h"
#include "nsMemory.h"
#include "nsCRT.h"
#include "nsReadableUtils.h"

const int netCharType[256] =
/*  Bit 0       xalpha      -- the alphas
**  Bit 1       xpalpha     -- as xalpha but
**                             converts spaces to plus and plus to %2B
**  Bit 3 ...   path        -- as xalphas but doesn't escape '/'
*/
  /* 0 1 2 3 4 5 6 7 8 9 A B C D E F */
  {  0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,   /* 0x */
     0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,   /* 1x */
     0,0,0,0,0,0,0,0,0,0,7,4,0,7,7,4,   /* 2x   !"#$%&'()*+,-./  */
     7,7,7,7,7,7,7,7,7,7,0,0,0,0,0,0,   /* 3x  0123456789:;<=>?  */
     0,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,   /* 4x  @ABCDEFGHIJKLMNO  */
     /* bits for '@' changed from 7 to 0 so '@' can be escaped   */
     /* in usernames and passwords in publishing.                */
     7,7,7,7,7,7,7,7,7,7,7,0,0,0,0,7,   /* 5X  PQRSTUVWXYZ[\]^_  */
     0,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,   /* 6x  `abcdefghijklmno  */
     7,7,7,7,7,7,7,7,7,7,7,0,0,0,0,0,   /* 7X  pqrstuvwxyz{\}~  DEL */
     0, };

/* decode % escaped hex codes into character values
 */
#define UNHEX(C) \
    ((C >= '0' && C <= '9') ? C - '0' : \
     ((C >= 'A' && C <= 'F') ? C - 'A' + 10 : \
     ((C >= 'a' && C <= 'f') ? C - 'a' + 10 : 0)))


#define IS_OK(C) (netCharType[((unsigned int)(C))] & (aFlags))
#define HEX_ESCAPE '%'

//----------------------------------------------------------------------------------------
static char*
nsEscapeCount(const char* aStr, nsEscapeMask aFlags, size_t* aOutLen)
//----------------------------------------------------------------------------------------
{
  if (!aStr) {
    return 0;
  }

  size_t len = 0;
  size_t charsToEscape = 0;
  static const char hexChars[] = "0123456789ABCDEF";

  const unsigned char* src = (const unsigned char*)aStr;
  while (*src) {
    len++;
    if (!IS_OK(*src++)) {
      charsToEscape++;
    }
  }

  // calculate how much memory should be allocated
  // original length + 2 bytes for each escaped character + terminating '\0'
  // do the sum in steps to check for overflow
  size_t dstSize = len + 1 + charsToEscape;
  if (dstSize <= len) {
    return 0;
  }
  dstSize += charsToEscape;
  if (dstSize < len) {
    return 0;
  }

  // fail if we need more than 4GB
  // size_t is likely to be long unsigned int but nsMemory::Alloc(size_t)
  // calls NS_Alloc_P(size_t) which calls PR_Malloc(uint32_t), so there is
  // no chance to allocate more than 4GB using nsMemory::Alloc()
  if (dstSize > UINT32_MAX) {
    return 0;
  }

  char* result = (char*)nsMemory::Alloc(dstSize);
  if (!result) {
    return 0;
  }

  unsigned char* dst = (unsigned char*)result;
  src = (const unsigned char*)aStr;
  if (aFlags == url_XPAlphas) {
    for (size_t i = 0; i < len; ++i) {
      unsigned char c = *src++;
      if (IS_OK(c)) {
        *dst++ = c;
      } else if (c == ' ') {
        *dst++ = '+';  /* convert spaces to pluses */
      } else {
        *dst++ = HEX_ESCAPE;
        *dst++ = hexChars[c >> 4];  /* high nibble */
        *dst++ = hexChars[c & 0x0f];  /* low nibble */
      }
    }
  } else {
    for (size_t i = 0; i < len; ++i) {
      unsigned char c = *src++;
      if (IS_OK(c)) {
        *dst++ = c;
      } else {
        *dst++ = HEX_ESCAPE;
        *dst++ = hexChars[c >> 4];  /* high nibble */
        *dst++ = hexChars[c & 0x0f];  /* low nibble */
      }
    }
  }

  *dst = '\0';     /* tack on eos */
  if (aOutLen) {
    *aOutLen = dst - (unsigned char*)result;
  }
  return result;
}

//----------------------------------------------------------------------------------------
char*
nsEscape(const char* aStr, nsEscapeMask aFlags)
//----------------------------------------------------------------------------------------
{
  if (!aStr) {
    return nullptr;
  }
  return nsEscapeCount(aStr, aFlags, nullptr);
}

//----------------------------------------------------------------------------------------
char*
nsUnescape(char* aStr)
//----------------------------------------------------------------------------------------
{
  nsUnescapeCount(aStr);
  return aStr;
}

//----------------------------------------------------------------------------------------
int32_t
nsUnescapeCount(char* aStr)
//----------------------------------------------------------------------------------------
{
  char* src = aStr;
  char* dst = aStr;
  static const char hexChars[] = "0123456789ABCDEFabcdef";

  char c1[] = " ";
  char c2[] = " ";
  char* const pc1 = c1;
  char* const pc2 = c2;

  if (!*src) {
    // A null string was passed in.  Nothing to escape.
    // Returns early as the string might not actually be mutable with
    // length 0.
    return 0;
  }

  while (*src) {
    c1[0] = *(src + 1);
    if (*(src + 1) == '\0') {
      c2[0] = '\0';
    } else {
      c2[0] = *(src + 2);
    }

    if (*src != HEX_ESCAPE || PL_strpbrk(pc1, hexChars) == 0 ||
        PL_strpbrk(pc2, hexChars) == 0) {
      *dst++ = *src++;
    } else {
      src++; /* walk over escape */
      if (*src) {
        *dst = UNHEX(*src) << 4;
        src++;
      }
      if (*src) {
        *dst = (*dst + UNHEX(*src));
        src++;
      }
      dst++;
    }
  }

  *dst = 0;
  return (int)(dst - aStr);

} /* NET_UnEscapeCnt */


char*
nsEscapeHTML(const char* aString)
{
  char* rv = nullptr;
  /* XXX Hardcoded max entity len. The +1 is for the trailing null. */
  uint32_t len = strlen(aString);
  if (len >= (UINT32_MAX / 6)) {
    return nullptr;
  }

  rv = (char*)NS_Alloc((6 * len) + 1);
  char* ptr = rv;

  if (rv) {
    for (; *aString != '\0'; ++aString) {
      if (*aString == '<') {
        *ptr++ = '&';
        *ptr++ = 'l';
        *ptr++ = 't';
        *ptr++ = ';';
      } else if (*aString == '>') {
        *ptr++ = '&';
        *ptr++ = 'g';
        *ptr++ = 't';
        *ptr++ = ';';
      } else if (*aString == '&') {
        *ptr++ = '&';
        *ptr++ = 'a';
        *ptr++ = 'm';
        *ptr++ = 'p';
        *ptr++ = ';';
      } else if (*aString == '"') {
        *ptr++ = '&';
        *ptr++ = 'q';
        *ptr++ = 'u';
        *ptr++ = 'o';
        *ptr++ = 't';
        *ptr++ = ';';
      } else if (*aString == '\'') {
        *ptr++ = '&';
        *ptr++ = '#';
        *ptr++ = '3';
        *ptr++ = '9';
        *ptr++ = ';';
      } else {
        *ptr++ = *aString;
      }
    }
    *ptr = '\0';
  }

  return rv;
}

char16_t*
nsEscapeHTML2(const char16_t* aSourceBuffer, int32_t aSourceBufferLen)
{
  // Calculate the length, if the caller didn't.
  if (aSourceBufferLen < 0) {
    aSourceBufferLen = NS_strlen(aSourceBuffer);
  }

  /* XXX Hardcoded max entity len. */
  if (uint32_t(aSourceBufferLen) >=
      ((UINT32_MAX - sizeof(char16_t)) / (6 * sizeof(char16_t)))) {
    return nullptr;
  }

  char16_t* resultBuffer = (char16_t*)nsMemory::Alloc(
    aSourceBufferLen * 6 * sizeof(char16_t) + sizeof(char16_t('\0')));
  char16_t* ptr = resultBuffer;

  if (resultBuffer) {
    int32_t i;

    for (i = 0; i < aSourceBufferLen; ++i) {
      if (aSourceBuffer[i] == '<') {
        *ptr++ = '&';
        *ptr++ = 'l';
        *ptr++ = 't';
        *ptr++ = ';';
      } else if (aSourceBuffer[i] == '>') {
        *ptr++ = '&';
        *ptr++ = 'g';
        *ptr++ = 't';
        *ptr++ = ';';
      } else if (aSourceBuffer[i] == '&') {
        *ptr++ = '&';
        *ptr++ = 'a';
        *ptr++ = 'm';
        *ptr++ = 'p';
        *ptr++ = ';';
      } else if (aSourceBuffer[i] == '"') {
        *ptr++ = '&';
        *ptr++ = 'q';
        *ptr++ = 'u';
        *ptr++ = 'o';
        *ptr++ = 't';
        *ptr++ = ';';
      } else if (aSourceBuffer[i] == '\'') {
        *ptr++ = '&';
        *ptr++ = '#';
        *ptr++ = '3';
        *ptr++ = '9';
        *ptr++ = ';';
      } else {
        *ptr++ = aSourceBuffer[i];
      }
    }
    *ptr = 0;
  }

  return resultBuffer;
}

//----------------------------------------------------------------------------------------

const int EscapeChars[256] =
/*   0    1    2    3    4    5    6    7    8    9    A    B    C    D    E    F */
{
     0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,  /* 0x */
     0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,  /* 1x */
     0,1023,   0, 512,1023,   0,1023,   0,1023,1023,1023,1023,1023,1023, 953, 784,  /* 2x   !"#$%&'()*+,-./  */
  1023,1023,1023,1023,1023,1023,1023,1023,1023,1023,1008,1008,   0,1008,   0, 768,  /* 3x  0123456789:;<=>?  */
  1008,1023,1023,1023,1023,1023,1023,1023,1023,1023,1023,1023,1023,1023,1023,1023,  /* 4x  @ABCDEFGHIJKLMNO  */
  1023,1023,1023,1023,1023,1023,1023,1023,1023,1023,1023, 896, 896, 896, 896,1023,  /* 5x  PQRSTUVWXYZ[\]^_  */
     0,1023,1023,1023,1023,1023,1023,1023,1023,1023,1023,1023,1023,1023,1023,1023,  /* 6x  `abcdefghijklmno  */
  1023,1023,1023,1023,1023,1023,1023,1023,1023,1023,1023, 896,1012, 896,1023,   0,  /* 7x  pqrstuvwxyz{|}~   */
     0    /* 8x  DEL               */
};

#define NO_NEED_ESC(C) (EscapeChars[((unsigned int)(C))] & (aFlags))

//----------------------------------------------------------------------------------------

/* returns an escaped string */

/* use the following flags to specify which
   part of an URL you want to escape:

   esc_Scheme        =     1
   esc_Username      =     2
   esc_Password      =     4
   esc_Host          =     8
   esc_Directory     =    16
   esc_FileBaseName  =    32
   esc_FileExtension =    64
   esc_Param         =   128
   esc_Query         =   256
   esc_Ref           =   512
*/

/* by default this function will not escape parts of a string
   that already look escaped, which means it already includes
   a valid hexcode. This is done to avoid multiple escapes of
   a string. Use the following flags to force escaping of a
   string:

   esc_Forced        =  1024
*/

bool
NS_EscapeURL(const char* aPart, int32_t aPartLen, uint32_t aFlags,
             nsACString& aResult)
{
  if (!aPart) {
    NS_NOTREACHED("null pointer");
    return false;
  }

  static const char hexChars[] = "0123456789ABCDEF";
  if (aPartLen < 0) {
    aPartLen = strlen(aPart);
  }
  bool forced = !!(aFlags & esc_Forced);
  bool ignoreNonAscii = !!(aFlags & esc_OnlyASCII);
  bool ignoreAscii = !!(aFlags & esc_OnlyNonASCII);
  bool writing = !!(aFlags & esc_AlwaysCopy);
  bool colon = !!(aFlags & esc_Colon);

  const unsigned char* src = (const unsigned char*)aPart;

  char tempBuffer[100];
  unsigned int tempBufferPos = 0;

  bool previousIsNonASCII = false;
  for (int i = 0; i < aPartLen; ++i) {
    unsigned char c = *src++;

    // if the char has not to be escaped or whatever follows % is
    // a valid escaped string, just copy the char.
    //
    // Also the % will not be escaped until forced
    // See bugzilla bug 61269 for details why we changed this
    //
    // And, we will not escape non-ascii characters if requested.
    // On special request we will also escape the colon even when
    // not covered by the matrix.
    // ignoreAscii is not honored for control characters (C0 and DEL)
    //
    // And, we should escape the '|' character when it occurs after any
    // non-ASCII character as it may be aPart of a multi-byte character.
    //
    // 0x20..0x7e are the valid ASCII characters. We also escape spaces
    // (0x20) since they are not legal in URLs.
    if ((NO_NEED_ESC(c) || (c == HEX_ESCAPE && !forced)
         || (c > 0x7f && ignoreNonAscii)
         || (c > 0x20 && c < 0x7f && ignoreAscii))
        && !(c == ':' && colon)
        && !(previousIsNonASCII && c == '|' && !ignoreNonAscii)) {
      if (writing) {
        tempBuffer[tempBufferPos++] = c;
      }
    } else { /* do the escape magic */
      if (!writing) {
        aResult.Append(aPart, i);
        writing = true;
      }
      tempBuffer[tempBufferPos++] = HEX_ESCAPE;
      tempBuffer[tempBufferPos++] = hexChars[c >> 4]; /* high nibble */
      tempBuffer[tempBufferPos++] = hexChars[c & 0x0f]; /* low nibble */
    }

    if (tempBufferPos >= sizeof(tempBuffer) - 4) {
      NS_ASSERTION(writing, "should be writing");
      tempBuffer[tempBufferPos] = '\0';
      aResult += tempBuffer;
      tempBufferPos = 0;
    }

    previousIsNonASCII = (c > 0x7f);
  }
  if (writing) {
    tempBuffer[tempBufferPos] = '\0';
    aResult += tempBuffer;
  }
  return writing;
}

#define ISHEX(c) memchr(hexChars, c, sizeof(hexChars)-1)

bool
NS_UnescapeURL(const char* aStr, int32_t aLen, uint32_t aFlags,
               nsACString& aResult)
{
  if (!aStr) {
    NS_NOTREACHED("null pointer");
    return false;
  }

  if (aLen < 0) {
    aLen = strlen(aStr);
  }

  bool ignoreNonAscii = !!(aFlags & esc_OnlyASCII);
  bool ignoreAscii = !!(aFlags & esc_OnlyNonASCII);
  bool writing = !!(aFlags & esc_AlwaysCopy);
  bool skipControl = !!(aFlags & esc_SkipControl);

  static const char hexChars[] = "0123456789ABCDEFabcdef";

  const char* last = aStr;
  const char* p = aStr;

  for (int i = 0; i < aLen; ++i, ++p) {
    //printf("%c [i=%d of aLen=%d]\n", *p, i, aLen);
    if (*p == HEX_ESCAPE && i < aLen - 2) {
      unsigned char* p1 = (unsigned char*)p + 1;
      unsigned char* p2 = (unsigned char*)p + 2;
      if (ISHEX(*p1) && ISHEX(*p2) &&
          ((*p1 < '8' && !ignoreAscii) || (*p1 >= '8' && !ignoreNonAscii)) &&
          !(skipControl &&
            (*p1 < '2' || (*p1 == '7' && (*p2 == 'f' || *p2 == 'F'))))) {
        //printf("- p1=%c p2=%c\n", *p1, *p2);
        writing = true;
        if (p > last) {
          //printf("- p=%p, last=%p\n", p, last);
          aResult.Append(last, p - last);
          last = p;
        }
        char u = (UNHEX(*p1) << 4) + UNHEX(*p2);
        //printf("- u=%c\n", u);
        aResult.Append(u);
        i += 2;
        p += 2;
        last += 3;
      }
    }
  }
  if (writing && last < aStr + aLen) {
    aResult.Append(last, aStr + aLen - last);
  }

  return writing;
}
