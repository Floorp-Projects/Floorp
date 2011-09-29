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
 * The Original Code is Mozilla.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 2002
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Darin Fisher <darin@netscape.com>
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

#ifndef nsNativeCharsetUtils_h__
#define nsNativeCharsetUtils_h__


/*****************************************************************************\
 *                                                                           *
 *                             **** NOTICE ****                              *
 *                                                                           *
 *             *** THESE ARE NOT GENERAL PURPOSE CONVERTERS ***              *
 *                                                                           *
 *    NS_CopyNativeToUnicode / NS_CopyUnicodeToNative should only be used    *
 *    for converting *FILENAMES* between native and unicode. They are not    *
 *    designed or tested for general encoding converter use.                 *
 *                                                                           *
\*****************************************************************************/

/**
 * thread-safe conversion routines that do not depend on uconv libraries.
 */
nsresult NS_CopyNativeToUnicode(const nsACString &input, nsAString  &output);
nsresult NS_CopyUnicodeToNative(const nsAString  &input, nsACString &output);

/* 
 * This function indicates whether the character encoding used in the file
 * system (more exactly what's used for |GetNativeFoo| and |SetNativeFoo|
 * of |nsILocalFile|) is UTF-8 or not. Knowing that helps us avoid an 
 * unncessary encoding conversion in some cases. For instance, to get the leaf
 * name in UTF-8 out of nsILocalFile, we can just use |GetNativeLeafName| rather
 * than using |GetLeafName| and converting the result to UTF-8 if the file 
 * system  encoding is UTF-8.
 * On Unix (but not on Mac OS X), it depends on the locale and is not known
 * in advance (at the compilation time) so that this function needs to be 
 * a real function. On Mac OS X it's always UTF-8 while on Windows 
 * and other platforms (e.g. OS2), it's never UTF-8.  
 */
#if defined(XP_UNIX) && !defined(XP_MACOSX) && !defined(ANDROID)
bool NS_IsNativeUTF8();
#else
inline bool NS_IsNativeUTF8()
{
#if defined(XP_MACOSX) || defined(ANDROID)
    return PR_TRUE;
#else
    return PR_FALSE;
#endif
}
#endif


/**
 * internal
 */
void NS_StartupNativeCharsetUtils();
void NS_ShutdownNativeCharsetUtils();

#endif // nsNativeCharsetUtils_h__
