/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

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
    return true;
#else
    return false;
#endif
}
#endif


/**
 * internal
 */
void NS_StartupNativeCharsetUtils();
void NS_ShutdownNativeCharsetUtils();

#endif // nsNativeCharsetUtils_h__
