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

#ifndef nsAWritableString_h___
#include "nsAWritableString.h"
#endif

NS_COM size_t Distance( const nsReadingIterator<PRUnichar>&, const nsReadingIterator<PRUnichar>& );
NS_COM size_t Distance( const nsReadingIterator<char>&, const nsReadingIterator<char>& );



NS_COM void CopyUCS2toASCII( const nsAReadableString& aSource, nsAWritableCString& aDest );
NS_COM void CopyASCIItoUCS2( const nsAReadableCString& aSource, nsAWritableString& aDest );

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
NS_COM char* ToNewCString( const nsAReadableString& aSource );


  /**
   * Returns a new |char| buffer containing a zero-terminated copy of |aSource|.
   *
   * Allocates and returns a new |char| buffer which you must free with |nsMemory::Free|.
   * The new buffer is zero-terminated, but that may not help you if |aSource| contains embedded nulls.
   *
   * @param aSource an 8-bit wide string
   * @return a new |char| buffer you must free with |nsMemory::Free|.
   */
NS_COM char* ToNewCString( const nsAReadableCString& aSource );

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

NS_COM char* ToNewUTF8String( const nsAReadableString& aSource );


  /**
   * Returns a new |PRUnichar| buffer containing a zero-terminated copy of |aSource|.
   *
   * Allocates and returns a new |char| buffer which you must free with |nsMemory::Free|.
   * The new buffer is zero-terminated, but that may not help you if |aSource| contains embedded nulls.
   *
   * @param aSource a 16-bit wide string
   * @return a new |PRUnichar| buffer you must free with |nsMemory::Free|.
   */
NS_COM PRUnichar* ToNewUnicode( const nsAReadableString& aSource );


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
NS_COM PRUnichar* ToNewUnicode( const nsAReadableCString& aSource );

  /**
   * Copies |aLength| 16-bit characters from the start of |aSource| to the
   * |PRUnichar| buffer |aDest|.
   *
   * After this operation |aDest| is not null terminated.
   *
   * @param aSource a 16-bit wide string
   * @param aDest a |PRUnichar| buffer
   * @param aLength the number of 16-bit characters to copy
   * @return pointer to destination buffer - identical to |aDest|
   */
NS_COM PRUnichar* CopyUnicodeTo( const nsAReadableString& aSource,
                                 PRUnichar* aDest,
                                 PRUint32 aLength );


  /**
   * Returns |PR_TRUE| if |aString| contains only ASCII characters, that is, characters in the range (0x00, 0x7F).
   *
   * @param aString a 16-bit wide string to scan
   */
NS_COM PRBool IsASCII( const nsAReadableString& aString );



  /**
   * Converts case in place in the argument string.
   */
NS_COM void ToUpperCase( nsAWritableString& );
NS_COM void ToUpperCase( nsAWritableCString& );

NS_COM void ToLowerCase( nsAWritableString& );
NS_COM void ToLowerCase( nsAWritableCString& );

#endif // !defined(nsReadableUtils_h___)
