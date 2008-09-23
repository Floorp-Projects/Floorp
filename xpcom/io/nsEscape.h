/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
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
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either of the GNU General Public License Version 2 or later (the "GPL"),
 * or the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
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
 	url_All       = 0         /**< %-escape every byte uncondtionally */
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
NS_COM char * nsEscape(const char * str, nsEscapeMask mask);

NS_COM char * nsUnescape(char * str);
	/* decode % escaped hex codes into character values,
	 * modifies the parameter, returns the same buffer
	 */

NS_COM PRInt32 nsUnescapeCount (char * str);
	/* decode % escaped hex codes into character values,
	 * modifies the parameter buffer, returns the length of the result
	 * (result may contain \0's).
	 */

NS_COM char *
nsEscapeHTML(const char * string);

NS_COM PRUnichar *
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
NS_COM PRBool NS_EscapeURL(const char *str,
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
NS_COM PRBool NS_UnescapeURL(const char *str,
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
inline PRBool
NS_Escape(const nsCString& aOriginal, nsCString& aEscaped,
          nsEscapeMask aMask)
{
  char* esc = nsEscape(aOriginal.get(), aMask);
  if (! esc)
    return PR_FALSE;
  aEscaped.Adopt(esc);
  return PR_TRUE;
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
