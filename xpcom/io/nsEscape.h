/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/*	First checked in on 98/12/03 by John R. McMullen, derived from net.h/mkparse.c. */

#ifndef _ESCAPE_H_
#define _ESCAPE_H_

#include "nscore.h"
#include "nsError.h"
#include "nsString.h"

/**
 * Valid mask values for nsEscape
 * Note: these values are copied in nsINetUtil.idl. Any changes should be kept
 * in sync.
 */
typedef enum {
  url_All       = 0,       // %-escape every byte unconditionally
  url_XAlphas   = 1u << 0, // Normal escape - leave alphas intact, escape the rest
  url_XPAlphas  = 1u << 1, // As url_XAlphas, but convert spaces (0x20) to '+' and plus to %2B
  url_Path      = 1u << 2  // As url_XAlphas, but don't escape slash ('/')
} nsEscapeMask;

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Escape the given string according to mask
 * @param str The string to escape
 * @param mask How to escape the string
 * @return A newly allocated escaped string that must be free'd with
 *         nsCRT::free, or null on failure
 */
char* nsEscape(const char* aStr, nsEscapeMask aMask);

char* nsUnescape(char* aStr);
/* decode % escaped hex codes into character values,
 * modifies the parameter, returns the same buffer
 */

int32_t nsUnescapeCount(char* aStr);
/* decode % escaped hex codes into character values,
 * modifies the parameter buffer, returns the length of the result
 * (result may contain \0's).
 */

char*
nsEscapeHTML(const char* aString);

char16_t*
nsEscapeHTML2(const char16_t* aSourceBuffer,
              int32_t aSourceBufferLen = -1);
/*
 * Escape problem char's for HTML display
 */


#ifdef __cplusplus
}
#endif


/**
 * NS_EscapeURL/NS_UnescapeURL constants for |flags| parameter:
 *
 * Note: These values are copied to nsINetUtil.idl
 *       Any changes should be kept in sync
 */
enum EscapeMask {
  /** url components **/
  esc_Scheme         = 1u << 0,
  esc_Username       = 1u << 1,
  esc_Password       = 1u << 2,
  esc_Host           = 1u << 3,
  esc_Directory      = 1u << 4,
  esc_FileBaseName   = 1u << 5,
  esc_FileExtension  = 1u << 6,
  esc_FilePath       = esc_Directory | esc_FileBaseName | esc_FileExtension,
  esc_Param          = 1u << 7,
  esc_Query          = 1u << 8,
  esc_Ref            = 1u << 9,
  /** special flags **/
  esc_Minimal        = esc_Scheme | esc_Username | esc_Password | esc_Host | esc_FilePath | esc_Param | esc_Query | esc_Ref,
  esc_Forced         = 1u << 10, /* forces escaping of existing escape sequences */
  esc_OnlyASCII      = 1u << 11, /* causes non-ascii octets to be skipped */
  esc_OnlyNonASCII   = 1u << 12, /* causes _graphic_ ascii octets (0x20-0x7E)
                                    * to be skipped when escaping. causes all
                                    * ascii octets (<= 0x7F) to be skipped when unescaping */
  esc_AlwaysCopy     = 1u << 13, /* copy input to result buf even if escaping is unnecessary */
  esc_Colon          = 1u << 14, /* forces escape of colon */
  esc_SkipControl    = 1u << 15  /* skips C0 and DEL from unescaping */
};

/**
 * NS_EscapeURL
 *
 * Escapes invalid char's in an URL segment.  Has no side-effect if the URL
 * segment is already escaped.  Otherwise, the escaped URL segment is appended
 * to |result|.
 *
 * @param  str     url segment string
 * @param  len     url segment string length (-1 if unknown)
 * @param  flags   url segment type flag
 * @param  result  result buffer, untouched if part is already escaped
 *
 * @return TRUE if escaping was performed, FALSE otherwise.
 */
bool NS_EscapeURL(const char* aStr,
                  int32_t aLen,
                  uint32_t aFlags,
                  nsACString& aResult);

/**
 * Expands URL escape sequences... beware embedded null bytes!
 *
 * @param  str     url string to unescape
 * @param  len     length of |str|
 * @param  flags   only esc_OnlyNonASCII, esc_SkipControl and esc_AlwaysCopy
 *                 are recognized
 * @param  result  result buffer, untouched if |str| is already unescaped
 *
 * @return TRUE if unescaping was performed, FALSE otherwise.
 */
bool NS_UnescapeURL(const char* aStr,
                    int32_t aLen,
                    uint32_t aFlags,
                    nsACString& aResult);

/** returns resultant string length **/
inline int32_t
NS_UnescapeURL(char* aStr)
{
  return nsUnescapeCount(aStr);
}

/**
 * String friendly versions...
 */
inline const nsCSubstring&
NS_EscapeURL(const nsCSubstring& aStr, uint32_t aFlags, nsCSubstring& aResult)
{
  if (NS_EscapeURL(aStr.Data(), aStr.Length(), aFlags, aResult)) {
    return aResult;
  }
  return aStr;
}
inline const nsCSubstring&
NS_UnescapeURL(const nsCSubstring& aStr, uint32_t aFlags, nsCSubstring& aResult)
{
  if (NS_UnescapeURL(aStr.Data(), aStr.Length(), aFlags, aResult)) {
    return aResult;
  }
  return aStr;
}

/**
 * CString version of nsEscape. Returns true on success, false
 * on out of memory. To reverse this function, use NS_UnescapeURL.
 */
inline bool
NS_Escape(const nsCString& aOriginal, nsCString& aEscaped,
          nsEscapeMask aMask)
{
  char* esc = nsEscape(aOriginal.get(), aMask);
  if (! esc) {
    return false;
  }
  aEscaped.Adopt(esc);
  return true;
}

/**
 * Inline unescape of mutable string object.
 */
inline nsCString&
NS_UnescapeURL(nsCString& aStr)
{
  aStr.SetLength(nsUnescapeCount(aStr.BeginWriting()));
  return aStr;
}

#endif //  _ESCAPE_H_
