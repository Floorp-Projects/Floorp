/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
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
 * The Initial Developer of the Original Code is IBM Corporation.
 * Portions created by IBM Corporation are Copyright (C) 2003
 * IBM Corporation. All Rights Reserved.
 *
 * Contributor(s):
 *   Darin Fisher <darin@meer.net>
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

#ifndef nsString_h___
#define nsString_h___


#ifndef nsSubstring_h___
#include "nsSubstring.h"
#endif

#ifndef nsDependentSubstring_h___
#include "nsDependentSubstring.h"
#endif

#ifndef nsReadableUtils_h___
#include "nsReadableUtils.h"
#endif

#include NEW_H

  // enable support for the obsolete string API if not explicitly disabled
#ifndef MOZ_STRING_WITH_OBSOLETE_API
#define MOZ_STRING_WITH_OBSOLETE_API 1
#endif

#if MOZ_STRING_WITH_OBSOLETE_API
  // radix values for ToInteger/AppendInt
#define kRadix10        (10)
#define kRadix16        (16)
#define kAutoDetect     (100)
#define kRadixUnknown   (kAutoDetect+1)
#define IGNORE_CASE     (PR_TRUE)
#endif


  // declare nsString, et. al.
#include "string-template-def-unichar.h"
#include "nsTString.h"
#include "string-template-undef.h"

  // declare nsCString, et. al.
#include "string-template-def-char.h"
#include "nsTString.h"
#include "string-template-undef.h"


  /**
   * A helper class that converts a UTF-16 string to ASCII in a lossy manner
   */
class NS_LossyConvertUTF16toASCII : public nsCAutoString
  {
    public:
      explicit
      NS_LossyConvertUTF16toASCII( const PRUnichar* aString )
        {
          LossyAppendUTF16toASCII(aString, *this);
        }

      NS_LossyConvertUTF16toASCII( const PRUnichar* aString, PRUint32 aLength )
        {
          LossyAppendUTF16toASCII(Substring(aString, aString + aLength), *this);
        }

      explicit
      NS_LossyConvertUTF16toASCII( const nsAString& aString )
        {
          LossyAppendUTF16toASCII(aString, *this);
        }

    private:
        // NOT TO BE IMPLEMENTED
      NS_LossyConvertUTF16toASCII( char );
  };


class NS_ConvertASCIItoUTF16 : public nsAutoString
  {
    public:
      explicit
      NS_ConvertASCIItoUTF16( const char* aCString )
        {
          AppendASCIItoUTF16(aCString, *this);
        }

      NS_ConvertASCIItoUTF16( const char* aCString, PRUint32 aLength )
        {
          AppendASCIItoUTF16(Substring(aCString, aCString + aLength), *this);
        }

      explicit
      NS_ConvertASCIItoUTF16( const nsACString& aCString )
        {
          AppendASCIItoUTF16(aCString, *this);
        }

    private:
        // NOT TO BE IMPLEMENTED
      NS_ConvertASCIItoUTF16( PRUnichar );
  };


  /**
   * A helper class that converts a UTF-16 string to UTF-8
   */
class NS_ConvertUTF16toUTF8 : public nsCAutoString
  {
    public:
      explicit
      NS_ConvertUTF16toUTF8( const PRUnichar* aString )
        {
          AppendUTF16toUTF8(aString, *this);
        }

      NS_ConvertUTF16toUTF8( const PRUnichar* aString, PRUint32 aLength )
        {
          AppendUTF16toUTF8(Substring(aString, aString + aLength), *this);
        }

      explicit
      NS_ConvertUTF16toUTF8( const nsAString& aString )
        {
          AppendUTF16toUTF8(aString, *this);
        }

    private:
        // NOT TO BE IMPLEMENTED
      NS_ConvertUTF16toUTF8( char );
  };


class NS_ConvertUTF8toUTF16 : public nsAutoString
  {
    public:
      explicit
      NS_ConvertUTF8toUTF16( const char* aCString )
        {
          AppendUTF8toUTF16(aCString, *this);
        }

      NS_ConvertUTF8toUTF16( const char* aCString, PRUint32 aLength )
        {
          AppendUTF8toUTF16(Substring(aCString, aCString + aLength), *this);
        }

      explicit
      NS_ConvertUTF8toUTF16( const nsACString& aCString )
        {
          AppendUTF8toUTF16(aCString, *this);
        }

    private:
        // NOT TO BE IMPLEMENTED
      NS_ConvertUTF8toUTF16( PRUnichar );
  };


// the following are included/declared for backwards compatibility

typedef NS_ConvertUTF16toUTF8 NS_ConvertUCS2toUTF8;
typedef NS_LossyConvertUTF16toASCII NS_LossyConvertUCS2toASCII;
typedef NS_ConvertASCIItoUTF16 NS_ConvertASCIItoUCS2;
typedef NS_ConvertUTF8toUTF16 NS_ConvertUTF8toUCS2;
typedef nsAutoString nsVoidableString;

#ifndef nsDependentString_h___
#include "nsDependentString.h"
#endif

#ifndef nsLiteralString_h___
#include "nsLiteralString.h"
#endif

#ifndef nsPromiseFlatString_h___
#include "nsPromiseFlatString.h"
#endif

// need to include these for backwards compatibility
#include "nsMemory.h"
#include <string.h>
#include <stdio.h>
#include "plhash.h"

inline PRInt32 MinInt(PRInt32 x, PRInt32 y)
  {
    return NS_MIN(x, y);
  }

inline PRInt32 MaxInt(PRInt32 x, PRInt32 y)
  {
    return NS_MAX(x, y);
  }

/**
 * Deprecated: don't use |Recycle|, just call |nsMemory::Free| directly
 *
 * Return the given buffer to the heap manager. Calls allocator::Free()
 */
inline void Recycle( char* aBuffer) { nsMemory::Free(aBuffer); }
inline void Recycle( PRUnichar* aBuffer) { nsMemory::Free(aBuffer); }

#endif // !defined(nsString_h___)
