/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/*
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
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s):
 *   Scott Collins <scc@netscape.com>
 */

#ifndef _nsAWritableString_h__
#define _nsAWritableString_h__

  // WORK IN PROGRESS

  // See also...
#include "nsAReadableString.h"


/*
  This file defines the abstract interfaces |nsAWritableString| and
  |nsAWritableCString|.

  |nsAWritableString| is a string of |PRUnichar|s.  |nsAWritableCString| (note the
  'C') is a string of |char|s.
*/

template <class CharT>
class basic_nsAWritableString
      : public basic_nsAReadableString<CharT>
    /*
      ...
    */
  {
    public:
      typedef typename basic_nsAReadableString<CharT>::size_type  size_type;

    protected:
      typedef typename basic_nsAReadableString<CharT>::FragmentRequest  FragmentRequest;
      typedef typename basic_nsAReadableString<CharT>::ConstFragment    ConstFragment;

      struct Fragment
        {
          CharT* mStart;
          CharT* mEnd;

          basic_nsAWritableString<CharT>* mOwningString;
          void*                           mFragmentIdentifier;
        };

      using basic_nsAReadableString<CharT>::GetFragment;
      virtual PRBool GetFragment( Fragment&, FragmentRequest ) = 0;

    public:
      virtual void SetCapacity( size_type ) = 0;
      virtual void SetLength( size_type ) = 0;


        // ...a synonym for |SetLength()|
      void
      Truncate( size_type aNewLength=0 )
        {
          SetLength(aNewLength);
        }


      // virtual PRBool SetCharAt( char_type, index_type ) = 0;

      void ToLowerCase();
      void ToUpperCase();

      // void StripChars( const CharT* aSet );
      // void StripChar( ... );
      // void StripWhitespace();
      // void ReplaceChar( ... );
      // void ReplaceSubstring( ... );
      // void Trim( ... );
      // void CompressSet( ... );
      // void CompareWhitespace( ... );

      // Assign
      // Append( ... )
      // Insert

      // SetString
      // operator=( ... )
      // operator+=( ... )
  };

// operator>>
// getline (maybe)

typedef basic_nsAWritableString<PRUnichar> nsAWritableString;
typedef basic_nsAWritableString<char>      nsAWritableCString;

#endif // !defined(_nsAWritableString_h__)
