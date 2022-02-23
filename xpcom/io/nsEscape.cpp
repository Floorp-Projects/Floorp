/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsEscape.h"

#include "mozilla/ArrayUtils.h"
#include "mozilla/BinarySearch.h"
#include "mozilla/CheckedInt.h"
#include "mozilla/TextUtils.h"
#include "nsTArray.h"
#include "nsCRT.h"
#include "nsASCIIMask.h"

static const char hexCharsUpper[] = "0123456789ABCDEF";
static const char hexCharsUpperLower[] = "0123456789ABCDEFabcdef";

static const unsigned char netCharType[256] =
    // clang-format off
/*  Bit 0       xalpha      -- the alphas
**  Bit 1       xpalpha     -- as xalpha but
**                             converts spaces to plus and plus to %2B
**  Bit 3 ...   path        -- as xalphas but doesn't escape '/'
**  Bit 4 ...   NSURL-ref   -- extra encoding for Apple NSURL compatibility.
**                             This encoding set is used on encoded URL ref
**                             components before converting a URL to an NSURL
**                             so we don't include '%' to avoid double encoding.
*/
  /*   0   1   2   3   4   5   6   7   8   9   A   B   C   D   E   F */
  {  0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0, /* 0x */
     0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0, /* 1x */
  /*       !   "   #   $   %   &   '   (   )   *   +   ,   -   .   /        */
     0x0,0x8,0x0,0x0,0x8,0x8,0x8,0x8,0x8,0x8,0xf,0xc,0x8,0xf,0xf,0xc, /* 2x */
  /*   0   1   2   3   4   5   6   7   8   9   :   ;   <   =   >   ?        */
     0xf,0xf,0xf,0xf,0xf,0xf,0xf,0xf,0xf,0xf,0x8,0x8,0x0,0x8,0x0,0x8, /* 3x */
  /*   @   A   B   C   D   E   F   G   H   I   J   K   L   M   N   O        */
     0x8,0xf,0xf,0xf,0xf,0xf,0xf,0xf,0xf,0xf,0xf,0xf,0xf,0xf,0xf,0xf, /* 4x */
     /* bits for '@' changed from 7 to 0 so '@' can be escaped   */
     /* in usernames and passwords in publishing.                */
  /*   P   Q   R   S   T   U   V   W   X   Y   Z   [   \   ]   ^   _        */
     0xf,0xf,0xf,0xf,0xf,0xf,0xf,0xf,0xf,0xf,0xf,0x0,0x0,0x0,0x0,0xf, /* 5x */
  /*   `   a   b   c   d   e   f   g   h   i   j   k   l   m   n   o        */
     0x0,0xf,0xf,0xf,0xf,0xf,0xf,0xf,0xf,0xf,0xf,0xf,0xf,0xf,0xf,0xf, /* 6x */
  /*   p   q   r   s   t   u   v   w   x   y   z   {   |   }   ~ DEL        */
     0xf,0xf,0xf,0xf,0xf,0xf,0xf,0xf,0xf,0xf,0xf,0x0,0x0,0x0,0x8,0x0, /* 7x */
     0x0,
  };

/* decode % escaped hex codes into character values
 */
#define UNHEX(C) \
    ((C >= '0' && C <= '9') ? C - '0' : \
     ((C >= 'A' && C <= 'F') ? C - 'A' + 10 : \
     ((C >= 'a' && C <= 'f') ? C - 'a' + 10 : 0)))
// clang-format on

#define IS_OK(C) (netCharType[((unsigned char)(C))] & (aFlags))
#define HEX_ESCAPE '%'

static const uint32_t ENCODE_MAX_LEN = 6;  // %uABCD

static uint32_t AppendPercentHex(char* aBuffer, unsigned char aChar) {
  uint32_t i = 0;
  aBuffer[i++] = '%';
  aBuffer[i++] = hexCharsUpper[aChar >> 4];   // high nibble
  aBuffer[i++] = hexCharsUpper[aChar & 0xF];  // low nibble
  return i;
}

static uint32_t AppendPercentHex(char16_t* aBuffer, char16_t aChar) {
  uint32_t i = 0;
  aBuffer[i++] = '%';
  if (aChar & 0xff00) {
    aBuffer[i++] = 'u';
    aBuffer[i++] = hexCharsUpper[aChar >> 12];         // high-byte high nibble
    aBuffer[i++] = hexCharsUpper[(aChar >> 8) & 0xF];  // high-byte low nibble
  }
  aBuffer[i++] = hexCharsUpper[(aChar >> 4) & 0xF];  // low-byte high nibble
  aBuffer[i++] = hexCharsUpper[aChar & 0xF];         // low-byte low nibble
  return i;
}

//----------------------------------------------------------------------------------------
char* nsEscape(const char* aStr, size_t aLength, size_t* aOutputLength,
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

  unsigned char* dst = (unsigned char*)result;
  src = (const unsigned char*)aStr;
  if (aFlags == url_XPAlphas) {
    for (size_t i = 0; i < aLength; ++i) {
      unsigned char c = *src++;
      if (IS_OK(c)) {
        *dst++ = c;
      } else if (c == ' ') {
        *dst++ = '+'; /* convert spaces to pluses */
      } else {
        *dst++ = HEX_ESCAPE;
        *dst++ = hexCharsUpper[c >> 4];   /* high nibble */
        *dst++ = hexCharsUpper[c & 0x0f]; /* low nibble */
      }
    }
  } else {
    for (size_t i = 0; i < aLength; ++i) {
      unsigned char c = *src++;
      if (IS_OK(c)) {
        *dst++ = c;
      } else {
        *dst++ = HEX_ESCAPE;
        *dst++ = hexCharsUpper[c >> 4];   /* high nibble */
        *dst++ = hexCharsUpper[c & 0x0f]; /* low nibble */
      }
    }
  }

  *dst = '\0'; /* tack on eos */
  if (aOutputLength) {
    *aOutputLength = dst - (unsigned char*)result;
  }

  return result;
}

//----------------------------------------------------------------------------------------
char* nsUnescape(char* aStr)
//----------------------------------------------------------------------------------------
{
  nsUnescapeCount(aStr);
  return aStr;
}

//----------------------------------------------------------------------------------------
int32_t nsUnescapeCount(char* aStr)
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

    if (*src != HEX_ESCAPE || strpbrk(pc1, hexCharsUpperLower) == nullptr ||
        strpbrk(pc2, hexCharsUpperLower) == nullptr) {
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

void nsAppendEscapedHTML(const nsACString& aSrc, nsACString& aDst) {
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

template <size_t N>
static constexpr void AddUnescapedChars(const char (&aChars)[N],
                                        uint32_t aFlags,
                                        std::array<uint32_t, 256>& aTable) {
  for (size_t i = 0; i < N - 1; ++i) {
    aTable[static_cast<unsigned char>(aChars[i])] |= aFlags;
  }
}

static constexpr std::array<uint32_t, 256> BuildEscapeChars() {
  constexpr uint32_t kAllModes = esc_Scheme | esc_Username | esc_Password |
                                 esc_Host | esc_Directory | esc_FileBaseName |
                                 esc_FileExtension | esc_Param | esc_Query |
                                 esc_Ref | esc_ExtHandler;

  std::array<uint32_t, 256> table{0};

  // Alphanumerics shouldn't be escaped in all escape modes.
  AddUnescapedChars("0123456789", kAllModes, table);
  AddUnescapedChars("ABCDEFGHIJKLMNOPQRSTUVWXYZ", kAllModes, table);
  AddUnescapedChars("abcdefghijklmnopqrstuvwxyz", kAllModes, table);
  AddUnescapedChars("!$&()*+,-_~", kAllModes, table);

  // Extra characters which aren't escaped in particular escape modes.
  AddUnescapedChars(".", esc_Scheme, table);
  // esc_Username has no additional unescaped characters.
  AddUnescapedChars("|", esc_Password, table);
  AddUnescapedChars(".", esc_Host, table);
  AddUnescapedChars("'./:;=@[]|", esc_Directory, table);
  AddUnescapedChars("'.:;=@[]|", esc_FileBaseName, table);
  AddUnescapedChars("':;=@[]|", esc_FileExtension, table);
  AddUnescapedChars(".:;=@[\\]^`{|}", esc_Param, table);
  AddUnescapedChars("./:;=?@[\\]^`{|}", esc_Query, table);
  AddUnescapedChars("#'./:;=?@[\\]^{|}", esc_Ref, table);
  AddUnescapedChars("#'./:;=?@[]", esc_ExtHandler, table);

  return table;
}

static constexpr std::array<uint32_t, 256> EscapeChars = BuildEscapeChars();

static bool dontNeedEscape(unsigned char aChar, uint32_t aFlags) {
  return EscapeChars[(size_t)aChar] & aFlags;
}
static bool dontNeedEscape(uint16_t aChar, uint32_t aFlags) {
  return aChar < EscapeChars.size() ? (EscapeChars[(size_t)aChar] & aFlags)
                                    : false;
}

// Temporary static assert to make sure that the rewrite to using
// `BuildEscapeChars` didn't change the final array in memory.
// It will be removed in Bug 1750945.

static_assert([]() constexpr {
  constexpr uint32_t OldEscapeChars[256] =
      // clang-format off
//     0      1      2      3      4      5      6      7      8      9      A      B      C      D      E      F
{
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,     0,     0,     0,     0,     0,     0,  // 0x
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,     0,     0,     0,     0,     0,     0,  // 1x
       0,132095,     0,131584,132095,     0,132095,131696,132095,132095,132095,132095,132095,132095,132025,131856,  // 2x   !"#$%&'()*+,-./
  132095,132095,132095,132095,132095,132095,132095,132095,132095,132095,132080,132080,     0,132080,     0,131840,  // 3x  0123456789:;<=>?
  132080,132095,132095,132095,132095,132095,132095,132095,132095,132095,132095,132095,132095,132095,132095,132095,  // 4x  @ABCDEFGHIJKLMNO
  132095,132095,132095,132095,132095,132095,132095,132095,132095,132095,132095,132080,   896,132080,   896,132095,  // 5x  PQRSTUVWXYZ[\]^_
     384,132095,132095,132095,132095,132095,132095,132095,132095,132095,132095,132095,132095,132095,132095,132095,  // 6x  `abcdefghijklmno
  132095,132095,132095,132095,132095,132095,132095,132095,132095,132095,132095,   896,  1012,   896,132095,     0,  // 7x  pqrstuvwxyz{|}~ DEL
       0                                                                                                            // 80 to FF are zero
};
  // clang-format on

  for (size_t i = 0; i < EscapeChars.size(); ++i) {
    if (OldEscapeChars[i] != EscapeChars[i]) {
      return false;
    }
  }
  return true;
}());

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
template <class T>
static nsresult T_EscapeURL(const typename T::char_type* aPart, size_t aPartLen,
                            uint32_t aFlags, const ASCIIMaskArray* aFilterMask,
                            T& aResult, bool& aDidAppend) {
  typedef nsCharTraits<typename T::char_type> traits;
  typedef typename traits::unsigned_char_type unsigned_char_type;
  static_assert(sizeof(*aPart) == 1 || sizeof(*aPart) == 2,
                "unexpected char type");

  if (!aPart) {
    MOZ_ASSERT_UNREACHABLE("null pointer");
    return NS_ERROR_INVALID_ARG;
  }

  bool forced = !!(aFlags & esc_Forced);
  bool ignoreNonAscii = !!(aFlags & esc_OnlyASCII);
  bool ignoreAscii = !!(aFlags & esc_OnlyNonASCII);
  bool writing = !!(aFlags & esc_AlwaysCopy);
  bool colon = !!(aFlags & esc_Colon);
  bool spaces = !!(aFlags & esc_Spaces);

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
    // 0x20..0x7e are the valid ASCII characters.
    if ((dontNeedEscape(c, aFlags) || (c == HEX_ESCAPE && !forced) ||
         (c > 0x7f && ignoreNonAscii) ||
         (c >= 0x20 && c < 0x7f && ignoreAscii)) &&
        !(c == ':' && colon) && !(c == ' ' && spaces) &&
        !(previousIsNonASCII && c == '|' && !ignoreNonAscii)) {
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

bool NS_EscapeURL(const char* aPart, int32_t aPartLen, uint32_t aFlags,
                  nsACString& aResult) {
  size_t partLen;
  if (aPartLen < 0) {
    partLen = strlen(aPart);
  } else {
    partLen = aPartLen;
  }

  return NS_EscapeURLSpan(mozilla::Span(aPart, partLen), aFlags, aResult);
}

bool NS_EscapeURLSpan(mozilla::Span<const char> aStr, uint32_t aFlags,
                      nsACString& aResult) {
  bool appended = false;
  nsresult rv = T_EscapeURL(aStr.Elements(), aStr.Length(), aFlags, nullptr,
                            aResult, appended);
  if (NS_FAILED(rv)) {
    ::NS_ABORT_OOM(aResult.Length() * sizeof(nsACString::char_type));
  }

  return appended;
}

nsresult NS_EscapeURL(const nsACString& aStr, uint32_t aFlags,
                      nsACString& aResult, const mozilla::fallible_t&) {
  bool appended = false;
  nsresult rv = T_EscapeURL(aStr.Data(), aStr.Length(), aFlags, nullptr,
                            aResult, appended);
  if (NS_FAILED(rv)) {
    aResult.Truncate();
    return rv;
  }

  if (!appended) {
    aResult = aStr;
  }

  return rv;
}

nsresult NS_EscapeAndFilterURL(const nsACString& aStr, uint32_t aFlags,
                               const ASCIIMaskArray* aFilterMask,
                               nsACString& aResult,
                               const mozilla::fallible_t&) {
  bool appended = false;
  nsresult rv = T_EscapeURL(aStr.Data(), aStr.Length(), aFlags, aFilterMask,
                            aResult, appended);
  if (NS_FAILED(rv)) {
    aResult.Truncate();
    return rv;
  }

  if (!appended) {
    if (!aResult.Assign(aStr, mozilla::fallible)) {
      return NS_ERROR_OUT_OF_MEMORY;
    }
  }

  return rv;
}

const nsAString& NS_EscapeURL(const nsAString& aStr, uint32_t aFlags,
                              nsAString& aResult) {
  bool result = false;
  nsresult rv = T_EscapeURL<nsAString>(aStr.Data(), aStr.Length(), aFlags,
                                       nullptr, aResult, result);

  if (NS_FAILED(rv)) {
    ::NS_ABORT_OOM(aResult.Length() * sizeof(nsAString::char_type));
  }

  if (result) {
    return aResult;
  }
  return aStr;
}

// Starting at aStr[aStart] find the first index in aStr that matches any
// character that is forbidden by aFunction. Return false if not found.
static bool FindFirstMatchFrom(const nsString& aStr, size_t aStart,
                               const std::function<bool(char16_t)>& aFunction,
                               size_t* aIndex) {
  for (size_t j = aStart, l = aStr.Length(); j < l; ++j) {
    if (aFunction(aStr[j])) {
      *aIndex = j;
      return true;
    }
  }
  return false;
}

const nsAString& NS_EscapeURL(const nsString& aStr,
                              const std::function<bool(char16_t)>& aFunction,
                              nsAString& aResult) {
  bool didEscape = false;
  for (size_t i = 0, strLen = aStr.Length(); i < strLen;) {
    size_t j;
    if (MOZ_UNLIKELY(FindFirstMatchFrom(aStr, i, aFunction, &j))) {
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

bool NS_UnescapeURL(const char* aStr, int32_t aLen, uint32_t aFlags,
                    nsACString& aResult) {
  bool didAppend = false;
  nsresult rv =
      NS_UnescapeURL(aStr, aLen, aFlags, aResult, didAppend, mozilla::fallible);
  if (rv == NS_ERROR_OUT_OF_MEMORY) {
    ::NS_ABORT_OOM(aLen * sizeof(nsACString::char_type));
  }

  return didAppend;
}

nsresult NS_UnescapeURL(const char* aStr, int32_t aLen, uint32_t aFlags,
                        nsACString& aResult, bool& aDidAppend,
                        const mozilla::fallible_t&) {
  if (!aStr) {
    MOZ_ASSERT_UNREACHABLE("null pointer");
    return NS_ERROR_INVALID_ARG;
  }

  MOZ_ASSERT(aResult.IsEmpty(),
             "Passing a non-empty string as an out parameter!");

  uint32_t len;
  if (aLen < 0) {
    size_t stringLength = strlen(aStr);
    if (stringLength >= UINT32_MAX) {
      return NS_ERROR_OUT_OF_MEMORY;
    }
    len = stringLength;
  } else {
    len = aLen;
  }

  bool ignoreNonAscii = !!(aFlags & esc_OnlyASCII);
  bool ignoreAscii = !!(aFlags & esc_OnlyNonASCII);
  bool writing = !!(aFlags & esc_AlwaysCopy);
  bool skipControl = !!(aFlags & esc_SkipControl);
  bool skipInvalidHostChar = !!(aFlags & esc_Host);

  unsigned char* destPtr;
  uint32_t destPos;

  if (writing) {
    if (!aResult.SetLength(len, mozilla::fallible)) {
      return NS_ERROR_OUT_OF_MEMORY;
    }
    destPos = 0;
    destPtr = reinterpret_cast<unsigned char*>(aResult.BeginWriting());
  }

  const char* last = aStr;
  const char* end = aStr + len;

  for (const char* p = aStr; p < end; ++p) {
    if (*p == HEX_ESCAPE && p + 2 < end) {
      unsigned char c1 = *((unsigned char*)p + 1);
      unsigned char c2 = *((unsigned char*)p + 2);
      unsigned char u = (UNHEX(c1) << 4) + UNHEX(c2);
      if (mozilla::IsAsciiHexDigit(c1) && mozilla::IsAsciiHexDigit(c2) &&
          (!skipInvalidHostChar || dontNeedEscape(u, aFlags) || c1 >= '8') &&
          ((c1 < '8' && !ignoreAscii) || (c1 >= '8' && !ignoreNonAscii)) &&
          !(skipControl &&
            (c1 < '2' || (c1 == '7' && (c2 == 'f' || c2 == 'F'))))) {
        if (MOZ_UNLIKELY(!writing)) {
          writing = true;
          if (!aResult.SetLength(len, mozilla::fallible)) {
            return NS_ERROR_OUT_OF_MEMORY;
          }
          destPos = 0;
          destPtr = reinterpret_cast<unsigned char*>(aResult.BeginWriting());
        }
        if (p > last) {
          auto toCopy = p - last;
          memcpy(destPtr + destPos, last, toCopy);
          destPos += toCopy;
          MOZ_ASSERT(destPos <= len);
          last = p;
        }
        destPtr[destPos] = u;
        destPos += 1;
        MOZ_ASSERT(destPos <= len);
        p += 2;
        last += 3;
      }
    }
  }
  if (writing && last < end) {
    auto toCopy = end - last;
    memcpy(destPtr + destPos, last, toCopy);
    destPos += toCopy;
    MOZ_ASSERT(destPos <= len);
  }

  if (writing) {
    aResult.Truncate(destPos);
  }

  aDidAppend = writing;
  return NS_OK;
}
