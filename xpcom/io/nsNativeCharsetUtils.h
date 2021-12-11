/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
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
 *    for converting *FILENAMES* between bytes and UTF-16. They are not      *
 *    designed or tested for general encoding converter use.                 *
 *                                                                           *
 *    On Windows, these functions convert to and from the system's legacy    *
 *    code page, which cannot represent all of Unicode. Elsewhere, these     *
 *    convert to and from UTF-8.                                             *
 *                                                                           *
\*****************************************************************************/

#include "nsError.h"
#include "nsStringFwd.h"

/**
 * thread-safe conversion routines that do not depend on uconv libraries.
 */
nsresult NS_CopyNativeToUnicode(const nsACString& aInput, nsAString& aOutput);
nsresult NS_CopyUnicodeToNative(const nsAString& aInput, nsACString& aOutput);

/*
 * This function indicates whether the character encoding used in the file
 * system (more exactly what's used for |GetNativeFoo| and |SetNativeFoo|
 * of |nsIFile|) is UTF-8 or not. Knowing that helps us avoid an
 * unncessary encoding conversion in some cases. For instance, to get the leaf
 * name in UTF-8 out of nsIFile, we can just use |GetNativeLeafName| rather
 * than using |GetLeafName| and converting the result to UTF-8 if the file
 * system  encoding is UTF-8.
 */
inline constexpr bool NS_IsNativeUTF8() {
#ifdef XP_WIN
  return false;
#else
  return true;
#endif
}

#endif  // nsNativeCharsetUtils_h__
