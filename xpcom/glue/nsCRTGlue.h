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
 * @param delims The set of delimiters (null-terminated)
 * @param str    The string to search (null-terminated)
 */
NS_COM_GLUE const char*
NS_strspnp(const char *delims, const char *str);

/**
 * Tokenize a string. This function is similar to the strtok function in the
 * C standard library, but it does not use static variables to maintain state
 * and is therefore thread and reentrancy-safe.
 *
 * Any leading delimiters in str are skipped. Then the string is scanned
 * until an additional delimiter or end-of-string is found. The final
 * delimiter is set to '\0'.
 *
 * @param delims The set of delimiters.
 * @param str    The string to search. This is an in-out parameter; it is
 *               reset to the end of the found token + 1, or to the
 *               end-of-string if there are no more tokens.
 * @return       The token. If no token is found (the string is only
 *               delimiter characters), nullptr is returned.
 */
NS_COM_GLUE char*
NS_strtok(const char *delims, char **str);

/**
 * "strlen" for char16_t strings
 */
NS_COM_GLUE uint32_t
NS_strlen(const char16_t *aString);

/**
 * "strcmp" for char16_t strings
 */
NS_COM_GLUE int
NS_strcmp(const char16_t *a, const char16_t *b);

/**
 * "strdup" for char16_t strings, uses the NS_Alloc allocator.
 */
NS_COM_GLUE char16_t*
NS_strdup(const char16_t *aString);

/**
 * "strdup", but using the NS_Alloc allocator.
 */
NS_COM_GLUE char*
NS_strdup(const char *aString);

/**
 * strndup for char16_t strings... this function will ensure that the
 * new string is null-terminated. Uses the NS_Alloc allocator.
 */
NS_COM_GLUE char16_t*
NS_strndup(const char16_t *aString, uint32_t aLen);

// The following case-conversion methods only deal in the ascii repertoire
// A-Z and a-z

// semi-private data declarations... don't use these directly.
class NS_COM_GLUE nsLowerUpperUtils {
public:
  static const unsigned char kLower2Upper[256];
  static const unsigned char kUpper2Lower[256];
};

inline char NS_ToUpper(char aChar)
{
  return (char)nsLowerUpperUtils::kLower2Upper[(unsigned char)aChar];
}

inline char NS_ToLower(char aChar)
{
  return (char)nsLowerUpperUtils::kUpper2Lower[(unsigned char)aChar];
}
  
NS_COM_GLUE bool NS_IsUpper(char aChar);
NS_COM_GLUE bool NS_IsLower(char aChar);

NS_COM_GLUE bool NS_IsAscii(char16_t aChar);
NS_COM_GLUE bool NS_IsAscii(const char16_t* aString);
NS_COM_GLUE bool NS_IsAsciiAlpha(char16_t aChar);
NS_COM_GLUE bool NS_IsAsciiDigit(char16_t aChar);
NS_COM_GLUE bool NS_IsAsciiWhitespace(char16_t aChar);
NS_COM_GLUE bool NS_IsAscii(const char* aString);
NS_COM_GLUE bool NS_IsAscii(const char* aString, uint32_t aLength);

#ifndef XPCOM_GLUE_AVOID_NSPR
NS_COM_GLUE void NS_MakeRandomString(char *buf, int32_t bufLen);
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
