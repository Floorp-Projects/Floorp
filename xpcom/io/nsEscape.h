/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/*	First checked in on 98/12/03 by John R. McMullen, derived from net.h/mkparse.c. */

#ifndef _ESCAPE_H_
#define _ESCAPE_H_

#include "prtypes.h"
#include "nscore.h"
#include "nsError.h"
#include "nsString.h"

/**
 * Valid mask values for nsEscape
 * Note: these values are copied in nsINetUtil.idl. Any changes should be kept
 * in sync.
 */
typedef enum {
 	url_All       = 0         /**< %-escape every byte unconditionally */
,	url_XAlphas   = PR_BIT(0) /**< Normal escape - leave alphas intact, escape the rest */
,	url_XPAlphas  = PR_BIT(1) /**< As url_XAlphas, but convert spaces (0x20) to '+' and plus to %2B */
,	url_Path      = PR_BIT(2) /**< As url_XAlphas, but don't escape slash ('/') */
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
char * nsEscape(const char * str, nsEscapeMask mask);

char * nsUnescape(char * str);
	/* decode % escaped hex codes into character values,
	 * modifies the parameter, returns the same buffer
	 */

PRInt32 nsUnescapeCount (char * str);
	/* decode % escaped hex codes into character values,
	 * modifies the parameter buffer, returns the length of the result
	 * (result may contain \0's).
	 */

char *
nsEscapeHTML(const char * string);

PRUnichar *
nsEscapeHTML2(const PRUnichar *aSourceBuffer,
              PRInt32 aSourceBufferLen = -1);
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
  esc_Scheme         = PR_BIT(0),
  esc_Username       = PR_BIT(1),
  esc_Password       = PR_BIT(2),
  esc_Host           = PR_BIT(3),
  esc_Directory      = PR_BIT(4),
  esc_FileBaseName   = PR_BIT(5),
  esc_FileExtension  = PR_BIT(6),
  esc_FilePath       = esc_Directory | esc_FileBaseName | esc_FileExtension,
  esc_Param          = PR_BIT(7),
  esc_Query          = PR_BIT(8),
  esc_Ref            = PR_BIT(9),
  /** special flags **/
  esc_Minimal        = esc_Scheme | esc_Username | esc_Password | esc_Host | esc_FilePath | esc_Param | esc_Query | esc_Ref, 
  esc_Forced         = PR_BIT(10), /* forces escaping of existing escape sequences */
  esc_OnlyASCII      = PR_BIT(11), /* causes non-ascii octets to be skipped */
  esc_OnlyNonASCII   = PR_BIT(12), /* causes _graphic_ ascii octets (0x20-0x7E) 
                                    * to be skipped when escaping. causes all
                                    * ascii octets (<= 0x7F) to be skipped when unescaping */
  esc_AlwaysCopy     = PR_BIT(13), /* copy input to result buf even if escaping is unnecessary */
  esc_Colon          = PR_BIT(14), /* forces escape of colon */
  esc_SkipControl    = PR_BIT(15)  /* skips C0 and DEL from unescaping */
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
bool NS_EscapeURL(const char *str,
                           PRInt32 len,
                           PRUint32 flags,
                           nsACString &result);

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
bool NS_UnescapeURL(const char *str,
                             PRInt32 len,
                             PRUint32 flags,
                             nsACString &result);

/** returns resultant string length **/
inline PRInt32 NS_UnescapeURL(char *str) {
    return nsUnescapeCount(str);
}

/**
 * String friendly versions...
 */
inline const nsCSubstring &
NS_EscapeURL(const nsCSubstring &str, PRUint32 flags, nsCSubstring &result) {
    if (NS_EscapeURL(str.Data(), str.Length(), flags, result))
        return result;
    return str;
}
inline const nsCSubstring &
NS_UnescapeURL(const nsCSubstring &str, PRUint32 flags, nsCSubstring &result) {
    if (NS_UnescapeURL(str.Data(), str.Length(), flags, result))
        return result;
    return str;
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
  if (! esc)
    return false;
  aEscaped.Adopt(esc);
  return true;
}

/**
 * Inline unescape of mutable string object.
 */
inline nsCString &
NS_UnescapeURL(nsCString &str)
{
    str.SetLength(nsUnescapeCount(str.BeginWriting()));
    return str;
}

#endif //  _ESCAPE_H_
