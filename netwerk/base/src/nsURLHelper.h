/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
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
 * Andreas Otte.
 * Portions created by the Initial Developer are Copyright (C) 2000
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
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

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
NS_HIDDEN_(void) net_ShutdownURLHelper();

/* access URL parsers */
NS_HIDDEN_(nsIURLParser *) net_GetAuthURLParser();
NS_HIDDEN_(nsIURLParser *) net_GetNoAuthURLParser();
NS_HIDDEN_(nsIURLParser *) net_GetStdURLParser();

/* convert between nsIFile and file:// URL spec */
NS_HIDDEN_(nsresult) net_GetURLSpecFromFile(nsIFile *, nsACString &);
NS_HIDDEN_(nsresult) net_GetFileFromURLSpec(const nsACString &, nsIFile **);

/* extract file path components from file:// URL */
NS_HIDDEN_(nsresult) net_ParseFileURL(const nsACString &inURL,
                                      nsACString &outDirectory,
                                      nsACString &outFileBaseName,
                                      nsACString &outFileExtension);

/* handle .. in dirs while resolving URLs */
NS_HIDDEN_(void) net_CoalesceDirs(netCoalesceFlags flags, char* path);

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
NS_HIDDEN_(nsresult) net_ResolveRelativePath(const nsACString &relativePath,
                                             const nsACString &basePath,
                                             nsACString &result);

/**
 * Extract URI-Scheme if possible
 *
 * @param inURI     URI spec
 * @param startPos  start of scheme (may be null)
 * @param endPos    end of scheme; index of colon (may be null)
 * @param scheme    scheme copied to this buffer on return (may be null)
 */
NS_HIDDEN_(nsresult) net_ExtractURLScheme(const nsACString &inURI,
                                          PRUint32 *startPos, 
                                          PRUint32 *endPos,
                                          nsACString *scheme = nsnull);

/* check that the given scheme conforms to RFC 2396 */
NS_HIDDEN_(PRBool) net_IsValidScheme(const char *scheme, PRUint32 schemeLen);

inline PRBool net_IsValidScheme(const nsAFlatCString &scheme)
{
    return net_IsValidScheme(scheme.get(), scheme.Length());
}

/**
 * Filter out whitespace from a URI string.  The input is the |str|
 * pointer. |result| is written to if and only if there is whitespace that has
 * to be filtered out.  The return value is true if and only if |result| is
 * written to.
 *
 * This function strips out all whitespace at the beginning and end of the URL
 * and strips out \r, \n, \t from the middle of the URL.  This makes it safe to
 * call on things like javascript: urls or data: urls, where we may in fact run
 * into whitespace that is not properly encoded.
 *
 * @param str the pointer to the string to filter.  Must be non-null.
 * @param result the out param to write to if filtering happens
 * @return whether result was written to
 */
NS_HIDDEN_(PRBool) net_FilterURIString(const char *str, nsACString& result);

/*****************************************************************************
 * generic string routines follow (XXX move to someplace more generic).
 */

/* convert to lower case */
NS_HIDDEN_(void) net_ToLowerCase(char* str, PRUint32 length);
NS_HIDDEN_(void) net_ToLowerCase(char* str);

/**
 * returns pointer to first character of |str| in the given set.  if not found,
 * then |end| is returned.  stops prematurely if a null byte is encountered,
 * and returns the address of the null byte.
 */
NS_HIDDEN_(char *) net_FindCharInSet(const char *str, const char *end, const char *set);

/**
 * returns pointer to first character of |str| NOT in the given set.  if all
 * characters are in the given set, then |end| is returned.  if '\0' is not
 * included in |set|, then stops prematurely if a null byte is encountered,
 * and returns the address of the null byte.
 */
NS_HIDDEN_(char *) net_FindCharNotInSet(const char *str, const char *end, const char *set);

/**
 * returns pointer to last character of |str| in the given set.  if not found,
 * then |str - 1| is returned.
 */
NS_HIDDEN_(char *) net_RFindCharInSet(const char *str, const char *end, const char *set);

/**
 * returns pointer to last character of |str| NOT in the given set.  if all
 * characters are in the given set, then |str - 1| is returned.
 */
NS_HIDDEN_(char *) net_RFindCharNotInSet(const char *str, const char *end, const char *set);

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
inline char *net_RFindCharInSet(const char *str, const char *set)
{
    return net_RFindCharInSet(str, str + strlen(str), set);
}
inline char *net_RFindCharNotInSet(const char *str, const char *set)
{
    return net_RFindCharNotInSet(str, str + strlen(str), set);
}

#endif // !nsURLHelper_h__
