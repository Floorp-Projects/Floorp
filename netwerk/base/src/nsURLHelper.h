/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * The contents of this file are subject to the Netscape Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is Andreas Otte.
 *
 * Contributor(s): 
 */

#ifndef _nsURLHelper_h_
#define _nsURLHelper_h_

#include "prtypes.h"
#include "nscore.h"
#include "nsCRT.h"
#include "nsString.h"

#ifdef __cplusplus
extern "C" {
#endif

/* encode characters into % escaped hexcodes */

/* use the following masks to specify which 
   part of an URL you want to escape: 

   url_Scheme        =     1
   url_Username      =     2
   url_Password      =     4
   url_Host          =     8
   url_Directory     =    16
   url_FileBaseName  =    32
   url_FileExtension =    64
   url_Param         =   128
   url_Query         =   256
   url_Ref           =   512
*/

/* by default this function will not escape parts of a string
   that already look escaped, which means it already includes 
   a valid hexcode. This is done to avoid multiple escapes of
   a string. Use the following mask to force escaping of a 
   string:
 
   url_Forced        =  1024
*/
NS_NET nsresult nsURLEscape (const char* str, PRInt16 mask, nsCString &result);

/* helper call function */
NS_NET nsresult nsAppendURLEscapedString(nsCString& originalStr, const char* str, PRInt16 mask);

/* decode % escaped hex codes into character values */
NS_NET nsresult nsURLUnescape(char* str, char ** result);

/* Get port from string */ 
NS_NET PRInt32 ExtractPortFrom(const char* src);

/* Extract string out of another */
NS_NET nsresult ExtractString(char* i_Src, char* *o_Dest, PRUint32 length);

/* Duplicate string */
NS_NET nsresult DupString(char* *o_Dest, const char* i_Src);

/* handle .. in dirs */
NS_NET void CoaleseDirs(char* io_Path);

/* convert to lower case */
NS_NET void ToLowerCase(char* str);

/* Extract URI-Scheme if possible */
NS_NET nsresult ExtractURLScheme(const char* inURI, PRUint32 *startPos, 
                                 PRUint32 *endPos, char* *scheme);

/* Convert the URI string to a known case (or nsIURI::UNKNOWN) */
NS_NET PRUint32 SchemeTypeFor(const char* iScheme);

#ifdef __cplusplus
}
#endif

#endif
