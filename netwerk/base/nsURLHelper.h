/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsURLHelper_h__
#define nsURLHelper_h__

#include "nsString.h"

class nsIFile;
class nsIURLParser;

enum netCoalesceFlags
{
  NET_COALESCE_NORMAL = 0,

  /**
   * retains /../ that reach above dir root (useful for FTP
   * servers in which the root of the FTP URL is not necessarily
   * the root of the FTP filesystem).
   */
  NET_COALESCE_ALLOW_RELATIVE_ROOT = 1<<0,

  /**
   * recognizes /%2F and // as markers for the root directory
   * and handles them properly.
   */
  NET_COALESCE_DOUBLE_SLASH_IS_ROOT = 1<<1
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
nsIURLParser * net_GetAuthURLParser();
nsIURLParser * net_GetNoAuthURLParser();
nsIURLParser * net_GetStdURLParser();

/* convert between nsIFile and file:// URL spec 
 * net_GetURLSpecFromFile does an extra stat, so callers should
 * avoid it if possible in favor of net_GetURLSpecFromActualFile
 * and net_GetURLSpecFromDir */
nsresult net_GetURLSpecFromFile(nsIFile *, nsACString &);
nsresult net_GetURLSpecFromDir(nsIFile *, nsACString &);
nsresult net_GetURLSpecFromActualFile(nsIFile *, nsACString &);
nsresult net_GetFileFromURLSpec(const nsACString &, nsIFile **);

/* extract file path components from file:// URL */
nsresult net_ParseFileURL(const nsACString &inURL,
                                      nsACString &outDirectory,
                                      nsACString &outFileBaseName,
                                      nsACString &outFileExtension);

/* handle .. in dirs while resolving URLs (path is UTF-8) */
void net_CoalesceDirs(netCoalesceFlags flags, char* path);

/**
 * Resolves a relative path string containing "." and ".."
 * with respect to a base path (assumed to already be resolved). 
 * For example, resolving "../../foo/./bar/../baz.html" w.r.t.
 * "/a/b/c/d/e/" yields "/a/b/c/foo/baz.html". Attempting to 
 * ascend above the base results in the NS_ERROR_MALFORMED_URI
 * exception. If basePath is null, it treats it as "/".
 *
 * @param relativePath  a relative URI
 * @param basePath      a base URI
 *
 * @return a new string, representing canonical uri
 */
nsresult net_ResolveRelativePath(const nsACString &relativePath,
                                             const nsACString &basePath,
                                             nsACString &result);

/**
 * Check if a URL is absolute
 *
 * @param inURL     URL spec
 * @return true if the given spec represents an absolute URL
 */
bool net_IsAbsoluteURL(const nsACString& inURL);

/**
 * Extract URI-Scheme if possible
 *
 * @param inURI     URI spec
 * @param scheme    scheme copied to this buffer on return (may be null)
 */
nsresult net_ExtractURLScheme(const nsACString &inURI,
                              nsACString &scheme);

/* check that the given scheme conforms to RFC 2396 */
bool net_IsValidScheme(const char *scheme, uint32_t schemeLen);

inline bool net_IsValidScheme(const nsAFlatCString &scheme)
{
    return net_IsValidScheme(scheme.get(), scheme.Length());
}

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
bool net_NormalizeFileURL(const nsACString &aURL,
                                        nsCString &aResultBuf);
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
char * net_FindCharInSet(const char *str, const char *end, const char *set);

/**
 * returns pointer to first character of |str| NOT in the given set.  if all
 * characters are in the given set, then |end| is returned.  if '\0' is not
 * included in |set|, then stops prematurely if a null byte is encountered,
 * and returns the address of the null byte.
 */
char * net_FindCharNotInSet(const char *str, const char *end, const char *set);

/**
 * returns pointer to last character of |str| NOT in the given set.  if all
 * characters are in the given set, then |str - 1| is returned.
 */
char * net_RFindCharNotInSet(const char *str, const char *end, const char *set);

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
void net_ParseRequestContentType(const nsACString &aHeaderStr,
                                 nsACString       &aContentType,
                                 nsACString       &aContentCharset,
                                 bool*          aHadCharset);

/**
 * Parses a content-type header and returns the content type and
 * charset (if any).  aCharset is not modified if no charset is
 * specified in anywhere in aHeaderStr.  In that case (no charset
 * specified), aHadCharset is set to false.  Otherwise, it's set to
 * true.  Note that aContentCharset can be empty even if aHadCharset
 * is true.
 */
void net_ParseContentType(const nsACString &aHeaderStr,
                          nsACString       &aContentType,
                          nsACString       &aContentCharset,
                          bool*          aHadCharset);
/**
 * As above, but also returns the start and end indexes for the charset
 * parameter in aHeaderStr.  These are indices for the entire parameter, NOT
 * just the value.  If there is "effectively" no charset parameter (e.g. if an
 * earlier type with one is overridden by a later type without one),
 * *aHadCharset will be true but *aCharsetStart will be set to -1.  Note that
 * it's possible to have aContentCharset empty and *aHadCharset true when
 * *aCharsetStart is nonnegative; this corresponds to charset="".
 */
void net_ParseContentType(const nsACString &aHeaderStr,
                          nsACString       &aContentType,
                          nsACString       &aContentCharset,
                          bool             *aHadCharset,
                          int32_t          *aCharsetStart,
                          int32_t          *aCharsetEnd);

/* inline versions */

/* remember the 64-bit platforms ;-) */
#define NET_MAX_ADDRESS (((char*)0)-1)

inline char *net_FindCharInSet(const char *str, const char *set)
{
    return net_FindCharInSet(str, NET_MAX_ADDRESS, set);
}
inline char *net_FindCharNotInSet(const char *str, const char *set)
{
    return net_FindCharNotInSet(str, NET_MAX_ADDRESS, set);
}
inline char *net_RFindCharNotInSet(const char *str, const char *set)
{
    return net_RFindCharNotInSet(str, str + strlen(str), set);
}

/**
 * This function returns true if the given hostname does not include any
 * restricted characters.  Otherwise, false is returned.
 */
bool net_IsValidHostName(const nsCSubstring &host);

/**
 * Checks whether the IPv4 address is valid according to RFC 3986 section 3.2.2.
 */
bool net_IsValidIPv4Addr(const char *addr, int32_t addrLen);

/**
 * Checks whether the IPv6 address is valid according to RFC 3986 section 3.2.2.
 */
bool net_IsValidIPv6Addr(const char *addr, int32_t addrLen);


/**
 * Returns the max length of a URL. The default is 1048576 (1 MB).
 * Can be changed by pref "network.standard-url.max-length"
 */
int32_t net_GetURLMaxLength();

#endif // !nsURLHelper_h__
