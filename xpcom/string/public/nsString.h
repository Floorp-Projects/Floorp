/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsString_h___
#define nsString_h___

#include "mozilla/Attributes.h"

#ifndef nsSubstring_h___
#include "nsSubstring.h"
#endif

#ifndef nsDependentSubstring_h___
#include "nsDependentSubstring.h"
#endif

#ifndef nsReadableUtils_h___
#include "nsReadableUtils.h"
#endif

#include <new>

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
#define IGNORE_CASE     (true)
#endif


  // declare nsString, et. al.
#include "string-template-def-unichar.h"
#include "nsTString.h"
#include "string-template-undef.h"

  // declare nsCString, et. al.
#include "string-template-def-char.h"
#include "nsTString.h"
#include "string-template-undef.h"

static_assert(sizeof(char16_t) == 2, "size of char16_t must be 2");
static_assert(sizeof(nsString::char_type) == 2,
              "size of nsString::char_type must be 2");
static_assert(nsString::char_type(-1) > nsString::char_type(0),
              "nsString::char_type must be unsigned");
static_assert(sizeof(nsCString::char_type) == 1,
              "size of nsCString::char_type must be 1");


  /**
   * A helper class that converts a UTF-16 string to ASCII in a lossy manner
   */
class NS_LossyConvertUTF16toASCII : public nsAutoCString
  {
    public:
      explicit
      NS_LossyConvertUTF16toASCII( const char16_t* aString )
        {
          LossyAppendUTF16toASCII(aString, *this);
        }

      NS_LossyConvertUTF16toASCII( const char16_t* aString, uint32_t aLength )
        {
          LossyAppendUTF16toASCII(Substring(aString, aLength), *this);
        }

#ifdef MOZ_USE_CHAR16_WRAPPER
      explicit
      NS_LossyConvertUTF16toASCII( char16ptr_t aString )
        : NS_LossyConvertUTF16toASCII(static_cast<const char16_t*>(aString)) {}

      NS_LossyConvertUTF16toASCII( char16ptr_t aString, uint32_t aLength )
        : NS_LossyConvertUTF16toASCII(static_cast<const char16_t*>(aString), aLength) {}
#endif

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

      NS_ConvertASCIItoUTF16( const char* aCString, uint32_t aLength )
        {
          AppendASCIItoUTF16(Substring(aCString, aLength), *this);
        }

      explicit
      NS_ConvertASCIItoUTF16( const nsACString& aCString )
        {
          AppendASCIItoUTF16(aCString, *this);
        }

    private:
        // NOT TO BE IMPLEMENTED
      NS_ConvertASCIItoUTF16( char16_t );
  };


  /**
   * A helper class that converts a UTF-16 string to UTF-8
   */
class NS_ConvertUTF16toUTF8 : public nsAutoCString
  {
    public:
      explicit
      NS_ConvertUTF16toUTF8( const char16_t* aString )
        {
          AppendUTF16toUTF8(aString, *this);
        }

      NS_ConvertUTF16toUTF8( const char16_t* aString, uint32_t aLength )
        {
          AppendUTF16toUTF8(Substring(aString, aLength), *this);
        }

#ifdef MOZ_USE_CHAR16_WRAPPER
      NS_ConvertUTF16toUTF8( char16ptr_t aString ) : NS_ConvertUTF16toUTF8(static_cast<const char16_t*>(aString)) {}

      NS_ConvertUTF16toUTF8( char16ptr_t aString, uint32_t aLength )
        : NS_ConvertUTF16toUTF8(static_cast<const char16_t*>(aString), aLength) {}
#endif

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

      NS_ConvertUTF8toUTF16( const char* aCString, uint32_t aLength )
        {
          AppendUTF8toUTF16(Substring(aCString, aLength), *this);
        }

      explicit
      NS_ConvertUTF8toUTF16( const nsACString& aCString )
        {
          AppendUTF8toUTF16(aCString, *this);
        }

    private:
        // NOT TO BE IMPLEMENTED
      NS_ConvertUTF8toUTF16( char16_t );
  };


#ifdef MOZ_USE_CHAR16_WRAPPER

inline char16_t*
wwc(wchar_t *str)
{
    return reinterpret_cast<char16_t*>(str);
}

inline wchar_t*
wwc(char16_t *str)
{
    return reinterpret_cast<wchar_t*>(str);
}

#else

inline char16_t*
wwc(char16_t *str)
{
    return str;
}

#endif

// the following are included/declared for backwards compatibility
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

/**
 * Deprecated: don't use |Recycle|, just call |nsMemory::Free| directly
 *
 * Return the given buffer to the heap manager. Calls allocator::Free()
 */
inline void Recycle( char* aBuffer) { nsMemory::Free(aBuffer); }
inline void Recycle( char16_t* aBuffer) { nsMemory::Free(aBuffer); }

#endif // !defined(nsString_h___)
