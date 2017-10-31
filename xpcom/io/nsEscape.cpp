/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsEscape.h"

#include "mozilla/ArrayUtils.h"
#include "mozilla/BinarySearch.h"
#include "mozilla/CheckedInt.h"
#include "nsTArray.h"
#include "nsCRT.h"
#include "plstr.h"
#include "nsASCIIMask.h"

static const char hexCharsUpper[] = "0123456789ABCDEF";
static const char hexCharsUpperLower[] = "0123456789ABCDEFabcdef";

static const int netCharType[256] =
// clang-format off
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
     0,
  };

/* decode % escaped hex codes into character values
 */
#define UNHEX(C) \
    ((C >= '0' && C <= '9') ? C - '0' : \
     ((C >= 'A' && C <= 'F') ? C - 'A' + 10 : \
     ((C >= 'a' && C <= 'f') ? C - 'a' + 10 : 0)))
// clang-format on

#define IS_OK(C) (netCharType[((unsigned int)(C))] & (aFlags))
#define HEX_ESCAPE '%'

static const uint32_t ENCODE_MAX_LEN = 6; // %uABCD

static uint32_t
AppendPercentHex(char* aBuffer, unsigned char aChar)
{
  uint32_t i = 0;
  aBuffer[i++] = '%';
  aBuffer[i++] = hexCharsUpper[aChar >> 4]; // high nibble
  aBuffer[i++] = hexCharsUpper[aChar & 0xF]; // low nibble
  return i;
}

static uint32_t
AppendPercentHex(char16_t* aBuffer, char16_t aChar)
{
  uint32_t i = 0;
  aBuffer[i++] = '%';
  if (aChar & 0xff00) {
    aBuffer[i++] = 'u';
    aBuffer[i++] = hexCharsUpper[aChar >> 12]; // high-byte high nibble
    aBuffer[i++] = hexCharsUpper[(aChar >> 8) & 0xF]; // high-byte low nibble
  }
  aBuffer[i++] = hexCharsUpper[(aChar >> 4) & 0xF]; // low-byte high nibble
  aBuffer[i++] = hexCharsUpper[aChar & 0xF]; // low-byte low nibble
  return i;
}

//----------------------------------------------------------------------------------------
char*
nsEscape(const char* aStr, size_t aLength, size_t* aOutputLength,
         nsEscapeMask aFlags)
//----------------------------------------------------------------------------------------
{
  if (!aStr) {
    return nullptr;
  }

  size_t charsToEscape = 0;

  const unsigned char* src = (const unsigned char*)aStr;
  for (size_t i = 0; i < aLength; ++i) {
    if (!IS_OK(src[i])) {
      charsToEscape++;
    }
  }

  // calculate how much memory should be allocated
  // original length + 2 bytes for each escaped character + terminating '\0'
  // do the sum in steps to check for overflow
  size_t dstSize = aLength + 1 + charsToEscape;
  if (dstSize <= aLength) {
    return nullptr;
  }
  dstSize += charsToEscape;
  if (dstSize < aLength) {
    return nullptr;
  }

  // fail if we need more than 4GB
  if (dstSize > UINT32_MAX) {
    return nullptr;
  }

  char* result = (char*)moz_xmalloc(dstSize);
  if (!result) {
    return nullptr;
  }

  unsigned char* dst = (unsigned char*)result;
  src = (const unsigned char*)aStr;
  if (aFlags == url_XPAlphas) {
    for (size_t i = 0; i < aLength; ++i) {
      unsigned char c = *src++;
      if (IS_OK(c)) {
        *dst++ = c;
      } else if (c == ' ') {
        *dst++ = '+';  /* convert spaces to pluses */
      } else {
        *dst++ = HEX_ESCAPE;
        *dst++ = hexCharsUpper[c >> 4];  /* high nibble */
        *dst++ = hexCharsUpper[c & 0x0f];  /* low nibble */
      }
    }
  } else {
    for (size_t i = 0; i < aLength; ++i) {
      unsigned char c = *src++;
      if (IS_OK(c)) {
        *dst++ = c;
      } else {
        *dst++ = HEX_ESCAPE;
        *dst++ = hexCharsUpper[c >> 4];  /* high nibble */
        *dst++ = hexCharsUpper[c & 0x0f];  /* low nibble */
      }
    }
  }

  *dst = '\0';     /* tack on eos */
  if (aOutputLength) {
    *aOutputLength = dst - (unsigned char*)result;
  }

  return result;
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

    if (*src != HEX_ESCAPE || PL_strpbrk(pc1, hexCharsUpperLower) == 0 ||
        PL_strpbrk(pc2, hexCharsUpperLower) == 0) {
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

void
nsAppendEscapedHTML(const nsACString& aSrc, nsACString& aDst)
{
  // Preparation: aDst's length will increase by at least aSrc's length. If the
  // addition overflows, we skip this, which is fine, and we'll likely abort
  // while (infallibly) appending due to aDst becoming too large.
  mozilla::CheckedInt<nsACString::size_type> newCapacity = aDst.Length();
  newCapacity += aSrc.Length();
  if (newCapacity.isValid()) {
    aDst.SetCapacity(newCapacity.value());
  }

  for (auto cur = aSrc.BeginReading(); cur != aSrc.EndReading(); cur++) {
    if (*cur == '<') {
      aDst.AppendLiteral("&lt;");
    } else if (*cur == '>') {
      aDst.AppendLiteral("&gt;");
    } else if (*cur == '&') {
      aDst.AppendLiteral("&amp;");
    } else if (*cur == '"') {
      aDst.AppendLiteral("&quot;");
    } else if (*cur == '\'') {
      aDst.AppendLiteral("&#39;");
    } else {
      aDst.Append(*cur);
    }
  }
}

//----------------------------------------------------------------------------------------
//
// The following table encodes which characters needs to be escaped for which
// parts of an URL.  The bits are the "url components" in the enum EscapeMask,
// see nsEscape.h.
//
// esc_Scheme        =     1
// esc_Username      =     2
// esc_Password      =     4
// esc_Host          =     8
// esc_Directory     =    16
// esc_FileBaseName  =    32
// esc_FileExtension =    64
// esc_Param         =   128
// esc_Query         =   256
// esc_Ref           =   512

static const uint32_t EscapeChars[256] =
// clang-format off
//   0    1    2    3    4    5    6    7    8    9    A    B    C    D    E    F
{
     0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,  // 0x
     0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,  // 1x
     0,1023,   0, 512,1023,   0,1023, 624,1023,1023,1023,1023,1023,1023, 953, 784,  // 2x   !"#$%&'()*+,-./
  1023,1023,1023,1023,1023,1023,1023,1023,1023,1023,1008,1008,   0,1008,   0, 768,  // 3x  0123456789:;<=>?
  1008,1023,1023,1023,1023,1023,1023,1023,1023,1023,1023,1023,1023,1023,1023,1023,  // 4x  @ABCDEFGHIJKLMNO
  1023,1023,1023,1023,1023,1023,1023,1023,1023,1023,1023,1008, 896,1008, 896,1023,  // 5x  PQRSTUVWXYZ[\]^_
   384,1023,1023,1023,1023,1023,1023,1023,1023,1023,1023,1023,1023,1023,1023,1023,  // 6x  `abcdefghijklmno
  1023,1023,1023,1023,1023,1023,1023,1023,1023,1023,1023, 896,1012, 896,1023,   0,  // 7x  pqrstuvwxyz{|}~ DEL
     0                                                                              // 80 to FF are zero
};
// clang-format on

static uint16_t dontNeedEscape(unsigned char aChar, uint32_t aFlags)
{
  return EscapeChars[(uint32_t)aChar] & aFlags;
}
static uint16_t dontNeedEscape(uint16_t aChar, uint32_t aFlags)
{
  return aChar < mozilla::ArrayLength(EscapeChars) ?
    (EscapeChars[(uint32_t)aChar]  & aFlags) : 0;
}

//----------------------------------------------------------------------------------------

/**
 * Templated helper for URL escaping a portion of a string.
 *
 * @param aPart The pointer to the beginning of the portion of the string to
 *  escape.
 * @param aPartLen The length of the string to escape.
 * @param aFlags Flags used to configure escaping. @see EscapeMask
 * @param aResult String that has the URL escaped portion appended to. Only
 *  altered if the string is URL escaped or |esc_AlwaysCopy| is specified.
 * @param aDidAppend Indicates whether or not data was appended to |aResult|.
 * @return NS_ERROR_INVALID_ARG, NS_ERROR_OUT_OF_MEMORY on failure.
 */
template<class T>
static nsresult
T_EscapeURL(const typename T::char_type* aPart, size_t aPartLen,
            uint32_t aFlags, const ASCIIMaskArray* aFilterMask,
            T& aResult, bool& aDidAppend)
{
  typedef nsCharTraits<typename T::char_type> traits;
  typedef typename traits::unsigned_char_type unsigned_char_type;
  static_assert(sizeof(*aPart) == 1 || sizeof(*aPart) == 2,
                "unexpected char type");

  if (!aPart) {
    NS_NOTREACHED("null pointer");
    return NS_ERROR_INVALID_ARG;
  }

  bool forced = !!(aFlags & esc_Forced);
  bool ignoreNonAscii = !!(aFlags & esc_OnlyASCII);
  bool ignoreAscii = !!(aFlags & esc_OnlyNonASCII);
  bool writing = !!(aFlags & esc_AlwaysCopy);
  bool colon = !!(aFlags & esc_Colon);

  auto src = reinterpret_cast<const unsigned_char_type*>(aPart);

  typename T::char_type tempBuffer[100];
  unsigned int tempBufferPos = 0;

  bool previousIsNonASCII = false;
  for (size_t i = 0; i < aPartLen; ++i) {
    unsigned_char_type c = *src++;

    // If there is a filter, we wish to skip any characters which match it.
    // This is needed so we don't perform an extra pass just to extract the
    // filtered characters.
    if (aFilterMask && mozilla::ASCIIMask::IsMasked(*aFilterMask, c)) {
      if (!writing) {
        if (!aResult.Append(aPart, i, mozilla::fallible)) {
          return NS_ERROR_OUT_OF_MEMORY;
        }
        writing = true;
      }
      continue;
    }

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
    if ((dontNeedEscape(c, aFlags) || (c == HEX_ESCAPE && !forced)
         || (c > 0x7f && ignoreNonAscii)
         || (c > 0x20 && c < 0x7f && ignoreAscii))
        && !(c == ':' && colon)
        && !(previousIsNonASCII && c == '|' && !ignoreNonAscii)) {
      if (writing) {
        tempBuffer[tempBufferPos++] = c;
      }
    } else { /* do the escape magic */
      if (!writing) {
        if (!aResult.Append(aPart, i, mozilla::fallible)) {
          return NS_ERROR_OUT_OF_MEMORY;
        }
        writing = true;
      }
      uint32_t len = ::AppendPercentHex(tempBuffer + tempBufferPos, c);
      tempBufferPos += len;
      MOZ_ASSERT(len <= ENCODE_MAX_LEN, "potential buffer overflow");
    }

    // Flush the temp buffer if it doesnt't have room for another encoded char.
    if (tempBufferPos >= mozilla::ArrayLength(tempBuffer) - ENCODE_MAX_LEN) {
      NS_ASSERTION(writing, "should be writing");
      if (!aResult.Append(tempBuffer, tempBufferPos, mozilla::fallible)) {
        return NS_ERROR_OUT_OF_MEMORY;
      }
      tempBufferPos = 0;
    }

    previousIsNonASCII = (c > 0x7f);
  }
  if (writing) {
    if (!aResult.Append(tempBuffer, tempBufferPos, mozilla::fallible)) {
      return NS_ERROR_OUT_OF_MEMORY;
    }
  }
  aDidAppend = writing;
  return NS_OK;
}

bool
NS_EscapeURL(const char* aPart, int32_t aPartLen, uint32_t aFlags,
             nsACString& aResult)
{
  if (aPartLen < 0) {
    aPartLen = strlen(aPart);
  }

  bool result = false;
  nsresult rv = T_EscapeURL(aPart, aPartLen, aFlags, nullptr, aResult, result);
  if (NS_FAILED(rv)) {
    ::NS_ABORT_OOM(aResult.Length() * sizeof(nsACString::char_type));
  }

  return result;
}

nsresult
NS_EscapeURL(const nsACString& aStr, uint32_t aFlags, nsACString& aResult,
             const mozilla::fallible_t&)
{
  bool appended = false;
  nsresult rv = T_EscapeURL(aStr.Data(), aStr.Length(), aFlags, nullptr, aResult, appended);
  if (NS_FAILED(rv)) {
    aResult.Truncate();
    return rv;
  }

  if (!appended) {
    aResult = aStr;
  }

  return rv;
}

nsresult
NS_EscapeAndFilterURL(const nsACString& aStr, uint32_t aFlags,
                      const ASCIIMaskArray* aFilterMask,
                      nsACString& aResult, const mozilla::fallible_t&)
{
  bool appended = false;
  nsresult rv = T_EscapeURL(aStr.Data(), aStr.Length(), aFlags, aFilterMask, aResult, appended);
  if (NS_FAILED(rv)) {
    aResult.Truncate();
    return rv;
  }

  if (!appended) {
    aResult = aStr;
  }

  return rv;
}

const nsAString&
NS_EscapeURL(const nsAString& aStr, uint32_t aFlags, nsAString& aResult)
{
  bool result = false;
  nsresult rv = T_EscapeURL<nsAString>(aStr.Data(), aStr.Length(), aFlags, nullptr, aResult, result);

  if (NS_FAILED(rv)) {
    ::NS_ABORT_OOM(aResult.Length() * sizeof(nsAString::char_type));
  }

  if (result) {
    return aResult;
  }
  return aStr;
}

// Starting at aStr[aStart] find the first index in aStr that matches any
// character in aForbidden. Return false if not found.
static bool
FindFirstMatchFrom(const nsString& aStr, size_t aStart,
                   const nsTArray<char16_t>& aForbidden, size_t* aIndex)
{
  const size_t len = aForbidden.Length();
  for (size_t j = aStart, l = aStr.Length(); j < l; ++j) {
    size_t unused;
    if (mozilla::BinarySearch(aForbidden, 0, len, aStr[j], &unused)) {
      *aIndex = j;
      return true;
    }
  }
  return false;
}

const nsAString&
NS_EscapeURL(const nsString& aStr, const nsTArray<char16_t>& aForbidden,
             nsAString& aResult)
{
  bool didEscape = false;
  for (size_t i = 0, strLen = aStr.Length(); i < strLen; ) {
    size_t j;
    if (MOZ_UNLIKELY(FindFirstMatchFrom(aStr, i, aForbidden, &j))) {
      if (i == 0) {
        didEscape = true;
        aResult.Truncate();
        aResult.SetCapacity(aStr.Length());
      }
      if (j != i) {
        // The substring from 'i' up to 'j' that needs no escaping.
        aResult.Append(nsDependentSubstring(aStr, i, j - i));
      }
      char16_t buffer[ENCODE_MAX_LEN];
      uint32_t bufferLen = ::AppendPercentHex(buffer, aStr[j]);
      MOZ_ASSERT(bufferLen <= ENCODE_MAX_LEN, "buffer overflow");
      aResult.Append(buffer, bufferLen);
      i = j + 1;
    } else {
      if (MOZ_UNLIKELY(didEscape)) {
        // The tail of the string that needs no escaping.
        aResult.Append(nsDependentSubstring(aStr, i, strLen - i));
      }
      break;
    }
  }
  if (MOZ_UNLIKELY(didEscape)) {
    return aResult;
  }
  return aStr;
}

#define ISHEX(c) memchr(hexCharsUpperLower, c, sizeof(hexCharsUpperLower)-1)

bool
NS_UnescapeURL(const char* aStr, int32_t aLen, uint32_t aFlags,
               nsACString& aResult)
{
  if (!aStr) {
    NS_NOTREACHED("null pointer");
    return false;
  }

  MOZ_ASSERT(aResult.IsEmpty(),
             "Passing a non-empty string as an out parameter!");

  if (aLen < 0) {
    aLen = strlen(aStr);
  }

  bool ignoreNonAscii = !!(aFlags & esc_OnlyASCII);
  bool ignoreAscii = !!(aFlags & esc_OnlyNonASCII);
  bool writing = !!(aFlags & esc_AlwaysCopy);
  bool skipControl = !!(aFlags & esc_SkipControl);
  bool skipInvalidHostChar = !!(aFlags & esc_Host);

  if (writing) {
    aResult.SetCapacity(aLen);
  }

  const char* last = aStr;
  const char* p = aStr;

  for (int i = 0; i < aLen; ++i, ++p) {
    if (*p == HEX_ESCAPE && i < aLen - 2) {
      unsigned char c1 = *((unsigned char*)p + 1);
      unsigned char c2 = *((unsigned char*)p + 2);
      unsigned char u = (UNHEX(c1) << 4) + UNHEX(c2);
      if (ISHEX(c1) && ISHEX(c2) &&
          (!skipInvalidHostChar || dontNeedEscape(u, aFlags) || c1 >= '8') &&
          ((c1 < '8' && !ignoreAscii) || (c1 >= '8' && !ignoreNonAscii)) &&
          !(skipControl &&
            (c1 < '2' || (c1 == '7' && (c2 == 'f' || c2 == 'F'))))) {
        if (!writing) {
          writing = true;
          aResult.SetCapacity(aLen);
        }
        if (p > last) {
          aResult.Append(last, p - last);
          last = p;
        }
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
