/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsURLHelper_h__
#define nsURLHelper_h__

#include "nsString.h"
#include "nsTArray.h"
#include "nsASCIIMask.h"

class nsIFile;
class nsIURLParser;

enum netCoalesceFlags {
  NET_COALESCE_NORMAL = 0,

  /**
   * retains /../ that reach above dir root (useful for FTP
   * servers in which the root of the FTP URL is not necessarily
   * the root of the FTP filesystem).
   */
  NET_COALESCE_ALLOW_RELATIVE_ROOT = 1 << 0,

  /**
   * recognizes /%2F and // as markers for the root directory
   * and handles them properly.
   */
  NET_COALESCE_DOUBLE_SLASH_IS_ROOT = 1 << 1
};

//----------------------------------------------------------------------------
// This module contains some private helper functions related to URL parsing.
//----------------------------------------------------------------------------

/* shutdown frees URL parser */
void net_ShutdownURLHelper();
#ifdef XP_MACOSX
void net_ShutdownURLHelperOSX();
#endif

/* access URL parsers */
nsIURLParser* net_GetAuthURLParser();
nsIURLParser* net_GetNoAuthURLParser();
nsIURLParser* net_GetStdURLParser();

/* convert between nsIFile and file:// URL spec
 * net_GetURLSpecFromFile does an extra stat, so callers should
 * avoid it if possible in favor of net_GetURLSpecFromActualFile
 * and net_GetURLSpecFromDir */
nsresult net_GetURLSpecFromFile(nsIFile*, nsACString&);
nsresult net_GetURLSpecFromDir(nsIFile*, nsACString&);
nsresult net_GetURLSpecFromActualFile(nsIFile*, nsACString&);
nsresult net_GetFileFromURLSpec(const nsACString&, nsIFile**);

/* extract file path components from file:// URL */
nsresult net_ParseFileURL(const nsACString& inURL, nsACString& outDirectory,
                          nsACString& outFileBaseName,
                          nsACString& outFileExtension);

/* handle .. in dirs while resolving URLs (path is UTF-8) */
void net_CoalesceDirs(netCoalesceFlags flags, char* path);

/**
 * Check if a URL is absolute
 *
 * @param inURL     URL spec
 * @return true if the given spec represents an absolute URL
 */
bool net_IsAbsoluteURL(const nsACString& uri);

/**
 * Extract URI-Scheme if possible
 *
 * @param inURI     URI spec
 * @param scheme    scheme copied to this buffer on return. Is lowercase.
 */
nsresult net_ExtractURLScheme(const nsACString& inURI, nsACString& scheme);

/* check that the given scheme conforms to RFC 2396 */
bool net_IsValidScheme(const nsACString& scheme);

/**
 * This function strips out all C0 controls and space at the beginning and end
 * of the URL and filters out \r, \n, \t from the middle of the URL.  This makes
 * it safe to call on things like javascript: urls or data: urls, where we may
 * in fact run into whitespace that is not properly encoded.
 *
 * @param input the URL spec we want to filter
 * @param result the out param to write to if filtering happens
 */
void net_FilterURIString(const nsACString& input, nsACString& result);

/**
 * This function performs character stripping just like net_FilterURIString,
 * with the added benefit of also performing percent escaping of dissallowed
 * characters, all in one pass. Saving one pass is very important when operating
 * on really large strings.
 *
 * @param aInput the URL spec we want to filter
 * @param aFlags the flags which control which characters we escape
 * @param aFilterMask a mask of characters that should excluded from the result
 * @param aResult the out param to write to if filtering happens
 */
nsresult net_FilterAndEscapeURI(const nsACString& aInput, uint32_t aFlags,
                                const ASCIIMaskArray& aFilterMask,
                                nsACString& aResult);

#if defined(XP_WIN)
/**
 * On Win32 and OS/2 system's a back-slash in a file:// URL is equivalent to a
 * forward-slash.  This function maps any back-slashes to forward-slashes.
 *
 * @param aURL
 *        The URL string to normalize (UTF-8 encoded).  This can be a
 *        relative URL segment.
 * @param aResultBuf
 *        The resulting string is appended to this string.  If the input URL
 *        is already normalized, then aResultBuf is unchanged.
 *
 * @returns false if aURL is already normalized.  Otherwise, returns true.
 */
bool net_NormalizeFileURL(const nsACString& aURL, nsCString& aResultBuf);
#endif

/*****************************************************************************
 * generic string routines follow (XXX move to someplace more generic).
 */

/* convert to lower case */
void net_ToLowerCase(char* str, uint32_t length);
void net_ToLowerCase(char* str);

/**
 * returns pointer to first character of |str| in the given set.  if not found,
 * then |end| is returned.  stops prematurely if a null byte is encountered,
 * and returns the address of the null byte.
 */
char* net_FindCharInSet(const char* iter, const char* stop, const char* set);

/**
 * returns pointer to first character of |str| NOT in the given set.  if all
 * characters are in the given set, then |end| is returned.  if '\0' is not
 * included in |set|, then stops prematurely if a null byte is encountered,
 * and returns the address of the null byte.
 */
char* net_FindCharNotInSet(const char* iter, const char* stop, const char* set);

/**
 * returns pointer to last character of |str| NOT in the given set.  if all
 * characters are in the given set, then |str - 1| is returned.
 */
char* net_RFindCharNotInSet(const char* stop, const char* iter,
                            const char* set);

/**
 * Parses a content-type header and returns the content type and
 * charset (if any).  aCharset is not modified if no charset is
 * specified in anywhere in aHeaderStr.  In that case (no charset
 * specified), aHadCharset is set to false.  Otherwise, it's set to
 * true.  Note that aContentCharset can be empty even if aHadCharset
 * is true.
 *
 * This parsing is suitable for HTTP request.  Use net_ParseContentType
 * for parsing this header in HTTP responses.
 */
void net_ParseRequestContentType(const nsACString& aHeaderStr,
                                 nsACString& aContentType,
                                 nsACString& aContentCharset,
                                 bool* aHadCharset);

/**
 * Parses a content-type header and returns the content type and
 * charset (if any).  aCharset is not modified if no charset is
 * specified in anywhere in aHeaderStr.  In that case (no charset
 * specified), aHadCharset is set to false.  Otherwise, it's set to
 * true.  Note that aContentCharset can be empty even if aHadCharset
 * is true.
 */
void net_ParseContentType(const nsACString& aHeaderStr,
                          nsACString& aContentType, nsACString& aContentCharset,
                          bool* aHadCharset);
/**
 * As above, but also returns the start and end indexes for the charset
 * parameter in aHeaderStr.  These are indices for the entire parameter, NOT
 * just the value.  If there is "effectively" no charset parameter (e.g. if an
 * earlier type with one is overridden by a later type without one),
 * *aHadCharset will be true but *aCharsetStart will be set to -1.  Note that
 * it's possible to have aContentCharset empty and *aHadCharset true when
 * *aCharsetStart is nonnegative; this corresponds to charset="".
 */
void net_ParseContentType(const nsACString& aHeaderStr,
                          nsACString& aContentType, nsACString& aContentCharset,
                          bool* aHadCharset, int32_t* aCharsetStart,
                          int32_t* aCharsetEnd);

/* inline versions */

/* remember the 64-bit platforms ;-) */
#define NET_MAX_ADDRESS ((char*)UINTPTR_MAX)

inline char* net_FindCharInSet(const char* str, const char* set) {
  return net_FindCharInSet(str, NET_MAX_ADDRESS, set);
}
inline char* net_FindCharNotInSet(const char* str, const char* set) {
  return net_FindCharNotInSet(str, NET_MAX_ADDRESS, set);
}
inline char* net_RFindCharNotInSet(const char* str, const char* set) {
  return net_RFindCharNotInSet(str, str + strlen(str), set);
}

/**
 * This function returns true if the given hostname does not include any
 * restricted characters.  Otherwise, false is returned.
 */
bool net_IsValidHostName(const nsACString& host);

/**
 * Checks whether the IPv4 address is valid according to RFC 3986 section 3.2.2.
 */
bool net_IsValidIPv4Addr(const nsACString& aAddr);

/**
 * Checks whether the IPv6 address is valid according to RFC 3986 section 3.2.2.
 */
bool net_IsValidIPv6Addr(const nsACString& aAddr);

/**
 * Returns the default status text for a given HTTP status code (useful if HTTP2
 * does not provide one, for instance).
 */
bool net_GetDefaultStatusTextForCode(uint16_t aCode, nsACString& aOutText);

namespace mozilla {
/**
 * A class for handling form-urlencoded query strings.
 *
 * Manages an ordered list of name-value pairs, and allows conversion from and
 * to the string representation.
 *
 * In addition, there are static functions for handling one-shot use cases.
 */
class URLParams final {
 public:
  /**
   * \brief Parses a query string and calls a parameter handler for each
   * name/value pair. The parameter handler can stop processing early by
   * returning false.
   *
   * \param aInput the query string to parse
   * \param aParamHandler the parameter handler as desribed above
   * \tparam ParamHandler a function type compatible with signature
   * bool(nsString, nsString)
   *
   * \return false if the parameter handler returned false for any parameter,
   * true otherwise
   */
  template <typename ParamHandler>
  static bool Parse(const nsACString& aInput, ParamHandler aParamHandler) {
    const char* start = aInput.BeginReading();
    const char* const end = aInput.EndReading();

    while (start != end) {
      nsAutoString decodedName;
      nsAutoString decodedValue;

      if (!ParseNextInternal(start, end, &decodedName, &decodedValue)) {
        continue;
      }

      if (!aParamHandler(std::move(decodedName), std::move(decodedValue))) {
        return false;
      }
    }
    return true;
  }

  /**
   * \brief Parses a query string and returns the value of a single parameter
   * specified by name.
   *
   * If there are multiple parameters with the same name, the value of the first
   * is returned.
   *
   * \param aInput the query string to parse
   * \param aName the name of the parameter to extract
   * \param[out] aValue will be assigned the parameter value, set to void if
   * there is no match \return true iff there was a parameter with with name
   * \paramref aName
   */
  static bool Extract(const nsACString& aInput, const nsAString& aName,
                      nsAString& aValue);

  /**
   * \brief Resets the state of this instance and parses a new query string.
   *
   * \param aInput the query string to parse
   */
  void ParseInput(const nsACString& aInput);

  /**
   * Serializes the current state to a query string.
   *
   * \param[out] aValue will be assigned the result of the serialization
   * \param aEncode If this is true, the serialization will encode the string.
   */
  void Serialize(nsAString& aValue, bool aEncode) const;

  void Get(const nsAString& aName, nsString& aRetval);

  void GetAll(const nsAString& aName, nsTArray<nsString>& aRetval);

  /**
   * \brief Sets the value of a given parameter.
   *
   * If one or more parameters of the name exist, the value of the first is
   * replaced, and all further parameters of the name are deleted. Otherwise,
   * the behaviour is the same as \ref Append.
   */
  void Set(const nsAString& aName, const nsAString& aValue);

  void Append(const nsAString& aName, const nsAString& aValue);

  bool Has(const nsAString& aName);

  bool Has(const nsAString& aName, const nsAString& aValue);

  /**
   * \brief Deletes all parameters with the given name.
   */
  void Delete(const nsAString& aName);

  void Delete(const nsAString& aName, const nsAString& aValue);

  void DeleteAll() { mParams.Clear(); }

  uint32_t Length() const { return mParams.Length(); }

  const nsAString& GetKeyAtIndex(uint32_t aIndex) const {
    MOZ_ASSERT(aIndex < mParams.Length());
    return mParams[aIndex].mKey;
  }

  const nsAString& GetValueAtIndex(uint32_t aIndex) const {
    MOZ_ASSERT(aIndex < mParams.Length());
    return mParams[aIndex].mValue;
  }

  /**
   * \brief Performs a stable sort of the parameters, maintaining the order of
   * multiple parameters with the same name.
   */
  void Sort();

 private:
  static void DecodeString(const nsACString& aInput, nsAString& aOutput);
  static void ConvertString(const nsACString& aInput, nsAString& aOutput);
  static bool ParseNextInternal(const char*& aStart, const char* aEnd,
                                nsAString* aOutDecodedName,
                                nsAString* aOutDecodedValue);

  struct Param {
    nsString mKey;
    nsString mValue;
  };

  nsTArray<Param> mParams;
};
}  // namespace mozilla

#endif  // !nsURLHelper_h__
