/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/*
 * The contents of this file are subject to the Mozilla Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/MPL/
 * 
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 * 
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 2000 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s):
 *   Scott Collins <scc@mozilla.org> (original author)
 *   Johnny Stenbeck <jst@netscape.com>
 *
 */

#ifndef nsReadableUtils_h___
#define nsReadableUtils_h___

  /**
   * I guess all the routines in this file are all mis-named.
   * According to our conventions, they should be |NS_xxx|.
   */

#ifndef nsAString_h___
#include "nsAString.h"
#endif

#ifndef nsAStringGenerator_h___
#include "nsAStringGenerator.h"
#endif


NS_COM size_t Distance( const nsAString::const_iterator&, const nsAString::const_iterator& );
NS_COM size_t Distance( const nsACString::const_iterator&, const nsACString::const_iterator& );


NS_COM void CopyUCS2toASCII( const nsAString& aSource, nsACString& aDest );
NS_COM void CopyASCIItoUCS2( const nsACString& aSource, nsAString& aDest );

  /**
   * Returns a new |char| buffer containing a zero-terminated copy of |aSource|.
   *
   * Allocates and returns a new |char| buffer which you must free with |nsMemory::Free|.
   * Performs a lossy encoding conversion by chopping 16-bit wide characters down to 8-bits wide while copying |aSource| to your new buffer.
   * This conversion is not well defined; but it reproduces legacy string behavior.
   * The new buffer is zero-terminated, but that may not help you if |aSource| contains embedded nulls.
   *
   * @param aSource a 16-bit wide string
   * @return a new |char| buffer you must free with |nsMemory::Free|.
   */
NS_COM char* ToNewCString( const nsAString& aSource );


  /**
   * Returns a new |char| buffer containing a zero-terminated copy of |aSource|.
   *
   * Allocates and returns a new |char| buffer which you must free with |nsMemory::Free|.
   * The new buffer is zero-terminated, but that may not help you if |aSource| contains embedded nulls.
   *
   * @param aSource an 8-bit wide string
   * @return a new |char| buffer you must free with |nsMemory::Free|.
   */
NS_COM char* ToNewCString( const nsACString& aSource );

  /**
   * Returns a new |char| buffer containing a zero-terminated copy of |aSource|.
   *
   * Allocates and returns a new |char| buffer which you must free with |nsMemory::Free|.
   * Performs a encoding conversion by converting 16-bit wide characters down to UTF8 encoded 8-bits wide string copying |aSource| to your new buffer.
   * The new buffer is zero-terminated, but that may not help you if |aSource| contains embedded nulls.
   *
   * @param aSource a 16-bit wide string
   * @return a new |char| buffer you must free with |nsMemory::Free|.
   */

NS_COM char* ToNewUTF8String( const nsAString& aSource );


  /**
   * Returns a new |PRUnichar| buffer containing a zero-terminated copy of |aSource|.
   *
   * Allocates and returns a new |char| buffer which you must free with |nsMemory::Free|.
   * The new buffer is zero-terminated, but that may not help you if |aSource| contains embedded nulls.
   *
   * @param aSource a 16-bit wide string
   * @return a new |PRUnichar| buffer you must free with |nsMemory::Free|.
   */
NS_COM PRUnichar* ToNewUnicode( const nsAString& aSource );


  /**
   * Returns a new |PRUnichar| buffer containing a zero-terminated copy of |aSource|.
   *
   * Allocates and returns a new |char| buffer which you must free with |nsMemory::Free|.
   * Performs an encoding conversion by 0-padding 8-bit wide characters up to 16-bits wide while copying |aSource| to your new buffer.
   * This conversion is not well defined; but it reproduces legacy string behavior.
   * The new buffer is zero-terminated, but that may not help you if |aSource| contains embedded nulls.
   *
   * @param aSource an 8-bit wide string
   * @return a new |PRUnichar| buffer you must free with |nsMemory::Free|.
   */
NS_COM PRUnichar* ToNewUnicode( const nsACString& aSource );

  /**
   * Copies |aLength| 16-bit characters from the start of |aSource| to the
   * |PRUnichar| buffer |aDest|.
   *
   * After this operation |aDest| is not null terminated.
   *
   * @param aSource a 16-bit wide string
   * @param aSrcOffset start offset in the source string
   * @param aDest a |PRUnichar| buffer
   * @param aLength the number of 16-bit characters to copy
   * @return pointer to destination buffer - identical to |aDest|
   */
NS_COM PRUnichar* CopyUnicodeTo( const nsAString& aSource,
                                 PRUint32 aSrcOffset,
                                 PRUnichar* aDest,
                                 PRUint32 aLength );


  /**
   * Copies 16-bit characters between iterators |aSrcStart| and
   * |aSrcEnd| to the writable string |aDest|. Similar to the
   * |nsString::Mid| method.
   *
   * After this operation |aDest| is not null terminated.
   *
   * @param aSrcStart start source iterator
   * @param aSrcEnd end source iterator
   * @param aDest destination for the copy
   */
NS_COM void CopyUnicodeTo( const nsAString::const_iterator& aSrcStart,
                           const nsAString::const_iterator& aSrcEnd,
                           nsAString& aDest );

  /**
   * Appends 16-bit characters between iterators |aSrcStart| and
   * |aSrcEnd| to the writable string |aDest|. 
   *
   * After this operation |aDest| is not null terminated.
   *
   * @param aSrcStart start source iterator
   * @param aSrcEnd end source iterator
   * @param aDest destination for the copy
   */
NS_COM void AppendUnicodeTo( const nsAString::const_iterator& aSrcStart,
                             const nsAString::const_iterator& aSrcEnd,
                             nsAString& aDest );

  /**
   * Returns |PR_TRUE| if |aString| contains only ASCII characters, that is, characters in the range (0x00, 0x7F).
   *
   * @param aString a 16-bit wide string to scan
   */
NS_COM PRBool IsASCII( const nsAString& aString );



  /**
   * Converts case in place in the argument string.
   */
NS_COM void ToUpperCase( nsAString& );
NS_COM void ToUpperCase( nsACString& );

NS_COM void ToLowerCase( nsAString& );
NS_COM void ToLowerCase( nsACString& );

  /**
   * Finds the leftmost occurance of |aPattern|, if any in the range |aSearchStart|..|aSearchEnd|.
   *
   * Returns |PR_TRUE| if a match was found, and adjusts |aSearchStart| and |aSearchEnd| to
   * point to the match.  If no match was found, returns |PR_FALSE| and makes |aSearchStart == aSearchEnd|.
   *
   * Currently, this is equivalent to the O(m*n) implementation previously on |ns[C]String|.
   * If we need something faster, then we can implement that later.
   */
NS_COM PRBool FindInReadable( const nsAString& aPattern, nsAString::const_iterator&, nsAString::const_iterator& );
NS_COM PRBool FindInReadable( const nsACString& aPattern, nsACString::const_iterator&, nsACString::const_iterator& );

NS_COM PRBool CaseInsensitiveFindInReadable( const nsAString& aPattern, nsAString::const_iterator&, nsAString::const_iterator& );
NS_COM PRBool CaseInsensitiveFindInReadable( const nsACString& aPattern, nsACString::const_iterator&, nsACString::const_iterator& );


  /**
   * Finds the rightmost occurance of |aPattern| 
   * Returns |PR_TRUE| if a match was found, and adjusts |aSearchStart| and |aSearchEnd| to
   * point to the match.  If no match was found, returns |PR_FALSE| and makes |aSearchStart == aSearchEnd|.
   *
   * Currently, this is equivalent to the O(m*n) implementation previously on |ns[C]String|.
   * If we need something faster, then we can implement that later.
   */
NS_COM PRBool RFindInReadable( const nsAString& aPattern, nsAString::const_iterator&, nsAString::const_iterator& );
NS_COM PRBool RFindInReadable( const nsACString& aPattern, nsACString::const_iterator&, nsACString::const_iterator& );

   /**
   * Finds the leftmost occurance of |aChar|, if any in the range 
   * |aSearchStart|..|aSearchEnd|.
   *
   * Returns |PR_TRUE| if a match was found, and adjusts |aSearchStart| to
   * point to the match.  If no match was found, returns |PR_FALSE| and 
   * makes |aSearchStart == aSearchEnd|.
   */
NS_COM PRBool FindCharInReadable( PRUnichar aChar, nsAString::const_iterator& aSearchStart, const nsAString::const_iterator& aSearchEnd );
NS_COM PRBool FindCharInReadable( char aChar, nsACString::const_iterator& aSearchStart, const nsACString::const_iterator& aSearchEnd );

    /**
    * Finds the number of occurences of |aChar| in the string |aStr|
    */
NS_COM PRUint32 CountCharInReadable( const nsAString& aStr,
                                     PRUnichar aChar );
NS_COM PRUint32 CountCharInReadable( const nsACString& aStr,
                                     char aChar );


  /*
    |nsSubstituteC?String|:
      this is currently a naive implementation leveraging |FindInReadable|.  I have a better
      algorithm in mind -- Gonnet, Baeza-Yates `Shift-Or' searching which is linear and simple
      to implement (not quite as simple as re-using |FindInReadable|, though :-).
   */

class NS_COM nsSubstituteString
    : public nsAStringGenerator
  {
    public:
      nsSubstituteString( const nsAString& aText, const nsAString& aPattern, const nsAString& aReplacement )
          : mText(aText),
            mPattern(aPattern),
            mReplacement(aReplacement),
            mNumberOfMatches(-1)  // |-1| means `don't know'
        {
          // nothing else to do here
        }

      virtual PRUnichar* operator()( PRUnichar* aDestBuffer ) const;
      virtual PRUint32 Length() const;
      virtual PRUint32 MaxLength() const;
      virtual PRBool IsDependentOn( const nsAString& ) const;

    private:
      void CountMatches() const;

    private:
      const nsAString&  mText;
      const nsAString&  mPattern;
      const nsAString&  mReplacement;
      /* mutable */ PRInt32 mNumberOfMatches;
  };

class NS_COM nsSubstituteCString
    : public nsACStringGenerator
  {
    public:
      nsSubstituteCString( const nsACString& aText, const nsACString& aPattern, const nsACString& aReplacement )
          : mText(aText),
            mPattern(aPattern),
            mReplacement(aReplacement),
            mNumberOfMatches(-1)  // |-1| means `don't know'
        {
          // nothing else to do here
        }

      virtual char* operator()( char* aDestBuffer ) const;
      virtual PRUint32 Length() const;
      virtual PRUint32 MaxLength() const;
      virtual PRBool IsDependentOn( const nsACString& ) const;

    private:
      void CountMatches() const;

    private:
      const nsACString&  mText;
      const nsACString&  mPattern;
      const nsACString&  mReplacement;
      /* mutable */ PRInt32 mNumberOfMatches;
  };





#endif // !defined(nsReadableUtils_h___)
