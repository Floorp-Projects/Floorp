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
 * The Original Code is Mozilla.
 * 
 * The Initial Developer of the Original Code is Netscape
 * Communications.  Portions created by Netscape Communications are
 * Copyright (C) 2001 by Netscape Communications.  All
 * Rights Reserved.
 * 
 * Contributor(s): 
 *   Scott Collins <scc@mozilla.org> (original author)
 */

#ifndef nsDependentSubstring_h___
#define nsDependentSubstring_h___

#ifndef nsAString_h___
#include "nsAString.h"
#endif

#ifndef nsStringTraits_h___
#include "nsStringTraits.h"
#endif



  //
  // nsDependentSubstring
  //

class NS_COM nsDependentSubstring
      : public nsAPromiseString
    /*
      NOT FOR USE BY HUMANS (mostly)

      ...not unlike |nsDependentConcatenation|.  Instances of this class exist only as anonymous
      temporary results from |Substring()|.  Like |nsDependentConcatenation|, this class only
      holds a pointer, no string data of its own.  It does its magic by overriding and forwarding
      calls to |GetReadableFragment()|.
    */
  {
    typedef nsAString                   string_type;
    typedef string_type::const_iterator const_iterator;

    protected:
      virtual const PRUnichar* GetReadableFragment( nsReadableFragment<PRUnichar>&, nsFragmentRequest, PRUint32 ) const;
      virtual       PRUnichar* GetWritableFragment( nsWritableFragment<PRUnichar>&, nsFragmentRequest, PRUint32 ) { return 0; }

    public:
      nsDependentSubstring( const string_type& aString, PRUint32 aStartPos, PRUint32 aLength )
          : mString(aString),
            mStartPos( NS_MIN(aStartPos, aString.Length()) ),
            mLength( NS_MIN(aLength, aString.Length()-mStartPos) )
        {
          // nothing else to do here
        }

      nsDependentSubstring( const const_iterator& aStart, const const_iterator& aEnd )
          : mString(aStart.string())
        {
          const_iterator zeroPoint;
          mString.BeginReading(zeroPoint);
          mStartPos = Distance(zeroPoint, aStart);
          mLength = Distance(aStart, aEnd);
        }

      // nsDependentSubstring( const nsDependentSubstring& ); // auto-generated copy-constructor should be OK
      // ~nsDependentSubstring();                           // auto-generated destructor OK

    private:
        // NOT TO BE IMPLEMENTED
      void operator=( const nsDependentSubstring& );        // we're immutable, you can't assign into a substring

    public:
      virtual PRUint32 Length() const;
      virtual PRBool IsDependentOn( const string_type& aString ) const { return mString.IsDependentOn(aString); }

    private:
      const string_type&  mString;
      PRUint32            mStartPos;
      PRUint32            mLength;
  };

class NS_COM nsDependentCSubstring
      : public nsAPromiseCString
    /*
      NOT FOR USE BY HUMANS (mostly)

      ...not unlike |nsDependentConcatenation|.  Instances of this class exist only as anonymous
      temporary results from |Substring()|.  Like |nsDependentConcatenation|, this class only
      holds a pointer, no string data of its own.  It does its magic by overriding and forwarding
      calls to |GetReadableFragment()|.
    */
  {
    typedef nsACString                  string_type;
    typedef string_type::const_iterator const_iterator;

    protected:
      virtual const char* GetReadableFragment( nsReadableFragment<char>&, nsFragmentRequest, PRUint32 ) const;
      virtual       char* GetWritableFragment( nsWritableFragment<char>&, nsFragmentRequest, PRUint32 ) { return 0; }

    public:
      nsDependentCSubstring( const string_type& aString, PRUint32 aStartPos, PRUint32 aLength )
          : mString(aString),
            mStartPos( NS_MIN(aStartPos, aString.Length()) ),
            mLength( NS_MIN(aLength, aString.Length()-mStartPos) )
        {
          // nothing else to do here
        }

      nsDependentCSubstring( const const_iterator& aStart, const const_iterator& aEnd )
          : mString(aStart.string())
        {
          const_iterator zeroPoint;
          mString.BeginReading(zeroPoint);
          mStartPos = Distance(zeroPoint, aStart);
          mLength = Distance(aStart, aEnd);
        }

      // nsDependentCSubstring( const nsDependentCSubstring& ); // auto-generated copy-constructor should be OK
      // ~nsDependentCSubstring();                            // auto-generated destructor OK

    private:
        // NOT TO BE IMPLEMENTED
      void operator=( const nsDependentCSubstring& );         // we're immutable, you can't assign into a substring

    public:
      virtual PRUint32 Length() const;
      virtual PRBool IsDependentOn( const string_type& aString ) const { return mString.IsDependentOn(aString); }

    private:
      const string_type&  mString;
      PRUint32            mStartPos;
      PRUint32            mLength;
  };







inline
const nsDependentCSubstring
Substring( const nsACString& aString, PRUint32 aStartPos, PRUint32 aSubstringLength )
  {
    return nsDependentCSubstring(aString, aStartPos, aSubstringLength);
  }

inline
const nsDependentSubstring
Substring( const nsAString& aString, PRUint32 aStartPos, PRUint32 aSubstringLength )
  {
    return nsDependentSubstring(aString, aStartPos, aSubstringLength);
  }

inline
const nsDependentCSubstring
Substring( const nsACString::const_iterator& aStart, const nsACString::const_iterator& aEnd )
  {
    return nsDependentCSubstring(aStart, aEnd);
  }

inline
const nsDependentSubstring
Substring( const nsAString::const_iterator& aStart, const nsAString::const_iterator& aEnd )
  {
    return nsDependentSubstring(aStart, aEnd);
  }


#endif /* !defined(nsDependentSubstring_h___) */
