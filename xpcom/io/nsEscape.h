/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: NPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.1 (the "License"); you may not use this file except in
 * compliance with the License. You may obtain a copy of the License at
 * http://www.mozilla.org/NPL/
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
 * either the GNU General Public License Version 2 or later (the "GPL"), or 
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the NPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the NPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

/*	First checked in on 98/12/03 by John R. McMullen, derived from net.h/mkparse.c. */

#ifndef _ESCAPE_H_
#define _ESCAPE_H_

#include "prtypes.h"
#include "nscore.h"
#include "nsError.h"
#include "nsString.h"

/* valid mask values for NET_Escape() and NET_EscapedSize(). */
typedef enum {
	url_XAlphas		= (1<<0)
,	url_XPAlphas	= (1<<1)
,	url_Path		= (1<<2)
} nsEscapeMask;

#ifdef __cplusplus
extern "C" {
#endif
NS_COM char * nsEscape(const char * str, nsEscapeMask mask);
	/* Caller must use nsCRT::free() on the result */

NS_COM char * nsUnescape(char * str);
	/* decode % escaped hex codes into character values,
	 * modifies the parameter, returns the same buffer
	 */

NS_COM char * nsEscapeCount(const char * str, PRInt32 len, nsEscapeMask mask, PRInt32* out_len);
	/* Like nsEscape, but if out_len is non-null, return result string length
	 * in *out_len, and uses len instead of NUL termination.
	 * Caller must use nsCRT::free() on the result.
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


  /**
     * Constants for the mask in the call to nsStdEscape
     */

enum EscapeMask {
  esc_Scheme        = 1,
  esc_Username      = 2,
  esc_Password      = 4,
  esc_Host          = 8,
  esc_Directory     = 16,
  esc_FileBaseName  = 32,
  esc_FileExtension = 64,
  esc_Param         = 128,
  esc_Query         = 256,
  esc_Ref           = 512,
  esc_Forced        = 1024
};

NS_COM nsresult nsStdEscape(const char* str, PRInt16 mask, nsCString &result);
NS_COM nsresult nsStdUnescape(char* str, char **result);


#ifdef __cplusplus
}
#endif



#endif //  _ESCAPE_H_
