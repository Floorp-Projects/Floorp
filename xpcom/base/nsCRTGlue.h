/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsCRTGlue_h__
#define nsCRTGlue_h__

#include "nscore.h"

/**
 * Scan a string for the first character that is *not* in a set of
 * delimiters.  If the string is only delimiter characters, the end of the
 * string is returned.
 *
 * @param aDelims The set of delimiters (null-terminated)
 * @param aStr    The string to search (null-terminated)
 */
const char* NS_strspnp(const char* aDelims, const char* aStr);

/**
 * Tokenize a string. This function is similar to the strtok function in the
 * C standard library, but it does not use static variables to maintain state
 * and is therefore thread and reentrancy-safe.
 *
 * Any leading delimiters in str are skipped. Then the string is scanned
 * until an additional delimiter or end-of-string is found. The final
 * delimiter is set to '\0'.
 *
 * @param aDelims The set of delimiters.
 * @param aStr    The string to search. This is an in-out parameter; it is
 *                reset to the end of the found token + 1, or to the
 *                end-of-string if there are no more tokens.
 * @return        The token. If no token is found (the string is only
 *                delimiter characters), nullptr is returned.
 */
char* NS_strtok(const char* aDelims, char** aStr);

/**
 * "strlen" for char16_t strings
 */
uint32_t NS_strlen(const char16_t* aString);

/**
 * "strcmp" for char16_t strings
 */
int NS_strcmp(const char16_t* aStrA, const char16_t* aStrB);

/**
 * "strncmp" for char16_t strings
 */
int NS_strncmp(const char16_t* aStrA, const char16_t* aStrB, size_t aLen);

/**
 * "strdup" for char16_t strings, uses the moz_xmalloc allocator.
 */
char16_t* NS_strdup(const char16_t* aString);

/**
 * "strdup", but using the moz_xmalloc allocator.
 */
char* NS_strdup(const char* aString);

/**
 * strndup for char16_t or char strings (normal strndup is not available on
 * windows). This function will ensure that the new string is
 * null-terminated. Uses the moz_xmalloc allocator.
 *
 * CharT may be either char16_t or char.
 */
template<typename CharT>
CharT* NS_strndup(const CharT* aString, uint32_t aLen);

// The following case-conversion methods only deal in the ascii repertoire
// A-Z and a-z

// semi-private data declarations... don't use these directly.
class nsLowerUpperUtils
{
public:
  static const unsigned char kLower2Upper[256];
  static const unsigned char kUpper2Lower[256];
};

inline char
NS_ToUpper(char aChar)
{
  return (char)nsLowerUpperUtils::kLower2Upper[(unsigned char)aChar];
}

inline char
NS_ToLower(char aChar)
{
  return (char)nsLowerUpperUtils::kUpper2Lower[(unsigned char)aChar];
}

bool NS_IsUpper(char aChar);
bool NS_IsLower(char aChar);

constexpr bool
NS_IsAscii(char16_t aChar)
{
  return (0x0080 > aChar);
}

bool NS_IsAscii(const char16_t* aString);
bool NS_IsAscii(const char* aString);
bool NS_IsAscii(const char* aString, uint32_t aLength);

// These three functions are `constexpr` alternatives to NS_IsAscii. It should
// only be used for compile-time computation because it uses recursion.
// XXX: once support for GCC 4.9 is dropped, this function should be removed
// and NS_IsAscii should be made `constexpr`.
constexpr bool
NS_ConstExprIsAscii(const char16_t* aString)
{
  return !*aString ? true :
    !NS_IsAscii(*aString) ? false : NS_ConstExprIsAscii(aString + 1);
}

constexpr bool
NS_ConstExprIsAscii(const char* aString)
{
  return !*aString ? true :
    !NS_IsAscii(*aString) ? false : NS_ConstExprIsAscii(aString + 1);
}

constexpr bool
NS_ConstExprIsAscii(const char* aString, uint32_t aLength)
{
  return aLength == 0 ? true :
    !NS_IsAscii(*aString) ? false :
    NS_ConstExprIsAscii(aString + 1, aLength - 1);
}

constexpr bool
NS_IsAsciiWhitespace(char16_t aChar)
{
  return aChar == ' ' ||
         aChar == '\r' ||
         aChar == '\n' ||
         aChar == '\t';
}

#ifndef XPCOM_GLUE_AVOID_NSPR
void NS_MakeRandomString(char* aBuf, int32_t aBufLen);
#endif

#define FF '\f'
#define TAB '\t'

#define CRSTR "\015"
#define LFSTR "\012"
#define CRLF "\015\012"     /* A CR LF equivalent string */

// We use the most restrictive filesystem as our default set of illegal filename
// characters. This is currently Windows.
#define OS_FILE_ILLEGAL_CHARACTERS "/:*?\"<>|"
// We also provide a list of all known file path separators for all filesystems.
// This can be used in replacement of FILE_PATH_SEPARATOR when you need to
// identify or replace all known path separators.
#define KNOWN_PATH_SEPARATORS "\\/"

#if defined(XP_MACOSX)
  #define FILE_PATH_SEPARATOR        "/"
#elif defined(XP_WIN)
  #define FILE_PATH_SEPARATOR        "\\"
#elif defined(XP_UNIX)
  #define FILE_PATH_SEPARATOR        "/"
#else
  #error need_to_define_your_file_path_separator_and_maybe_illegal_characters
#endif

// Not all these control characters are illegal in all OSs, but we don't really
// want them appearing in filenames
#define CONTROL_CHARACTERS     "\001\002\003\004\005\006\007" \
                           "\010\011\012\013\014\015\016\017" \
                           "\020\021\022\023\024\025\026\027" \
                           "\030\031\032\033\034\035\036\037"

#define FILE_ILLEGAL_CHARACTERS CONTROL_CHARACTERS OS_FILE_ILLEGAL_CHARACTERS

#endif // nsCRTGlue_h__
