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
 */

#include "nsReadableUtils.h"
#include "nsMemory.h"
#include "nsString.h"
#include "nsCRT.h"





  /**
   * A character sink that performs a |reinterpret_cast| style conversion between character types.
   */
template <class FromCharT, class ToCharT>
class LossyConvertEncoding
  {
    public:
      typedef FromCharT value_type;
 
      typedef FromCharT input_type;
      typedef ToCharT   output_type;

    public:
      LossyConvertEncoding( output_type* aDestination ) : mDestination(aDestination) { }

      PRUint32
      write( const input_type* aSource, PRUint32 aSourceLength )
        {
          const input_type* done_writing = aSource + aSourceLength;
          while ( aSource < done_writing )
            *mDestination++ = (output_type)(*aSource++);  // use old-style cast to mimic old |ns[C]String| behavior
          return aSourceLength;
        }

      void
      write_terminator()
        {
          *mDestination = output_type(0);
        }

    private:
      output_type* mDestination;
  };


NS_COM
void
CopyUCS2toASCII( const nsAReadableString& aSource, nsAWritableCString& aDest )
  {
      // right now, this won't work on multi-fragment destinations
    aDest.SetLength(aSource.Length());

    nsReadingIterator<PRUnichar> fromBegin, fromEnd;

    nsWritingIterator<char> toBegin;
    LossyConvertEncoding<PRUnichar, char> converter(aDest.BeginWriting(toBegin).get());
    
    copy_string(aSource.BeginReading(fromBegin), aSource.EndReading(fromEnd), converter);
  }

NS_COM
void
CopyASCIItoUCS2( const nsAReadableCString& aSource, nsAWritableString& aDest )
  {
      // right now, this won't work on multi-fragment destinations
    aDest.SetLength(aSource.Length());

    nsReadingIterator<char> fromBegin, fromEnd;

    nsWritingIterator<PRUnichar> toBegin;
    LossyConvertEncoding<char, PRUnichar> converter(aDest.BeginWriting(toBegin).get());

    copy_string(aSource.BeginReading(fromBegin), aSource.EndReading(fromEnd), converter);
  }


  /**
   * A helper function that allocates a buffer of the desired character type big enough to hold a copy of the supplied string (plus a zero terminator).
   *
   * @param aSource an string you will eventually be making a copy of
   * @return a new buffer (of the type specified by the second parameter) which you must free with |nsMemory::Free|.
   *
   */
template <class FromCharT, class ToCharT>
inline
ToCharT*
AllocateStringCopy( const basic_nsAReadableString<FromCharT>& aSource, ToCharT* )
  {
    return NS_STATIC_CAST(ToCharT*, nsMemory::Alloc((aSource.Length()+1) * sizeof(ToCharT)));
  }


NS_COM
char*
ToNewCString( const nsAReadableString& aSource )
  {
    char* result = AllocateStringCopy(aSource, (char*)0);

    nsReadingIterator<PRUnichar> fromBegin, fromEnd;
    LossyConvertEncoding<PRUnichar, char> converter(result);
    copy_string(aSource.BeginReading(fromBegin), aSource.EndReading(fromEnd), converter).write_terminator();
    return result;
  }

NS_COM
char*
ToNewUTF8String( const nsAReadableString& aSource )
  {
    NS_ConvertUCS2toUTF8 temp(aSource);

    char* result;
    if (temp.mOwnsBuffer) {
      // We allocated. Trick the string into not freeing its buffer to
      // avoid an extra allocation.
      result = temp.mStr;

      temp.mStr=0;
      temp.mOwnsBuffer = PR_FALSE;
    }
    else {
      // We didn't allocate a buffer, so we need to copy it out of the
      // nsCAutoString's storage.
      result = nsCRT::strdup(temp.mStr);
    }

    return result;
  }

NS_COM
char*
ToNewCString( const nsAReadableCString& aSource )
  {
    // no conversion needed, just allocate a buffer of the correct length and copy into it

    char* result = AllocateStringCopy(aSource, (char*)0);

    nsReadingIterator<char> fromBegin, fromEnd;
    char* toBegin = result;
    *copy_string(aSource.BeginReading(fromBegin), aSource.EndReading(fromEnd), toBegin) = char(0);
    return result;
  }

NS_COM
PRUnichar*
ToNewUnicode( const nsAReadableString& aSource )
  {
    // no conversion needed, just allocate a buffer of the correct length and copy into it

    PRUnichar* result = AllocateStringCopy(aSource, (PRUnichar*)0);

    nsReadingIterator<PRUnichar> fromBegin, fromEnd;
    PRUnichar* toBegin = result;
    *copy_string(aSource.BeginReading(fromBegin), aSource.EndReading(fromEnd), toBegin) = PRUnichar(0);
    return result;
  }

NS_COM
PRUnichar*
ToNewUnicode( const nsAReadableCString& aSource )
  {
    PRUnichar* result = AllocateStringCopy(aSource, (PRUnichar*)0);

    nsReadingIterator<char> fromBegin, fromEnd;
    LossyConvertEncoding<char, PRUnichar> converter(result);
    copy_string(aSource.BeginReading(fromBegin), aSource.EndReading(fromEnd), converter).write_terminator();
    return result;
  }

NS_COM
PRUnichar*
CopyUnicodeTo( const nsAReadableString& aSource, PRUnichar* aDest, PRUint32 aLength )
  {
    nsReadingIterator<PRUnichar> fromBegin, fromEnd;
    PRUnichar* toBegin = aDest;
    copy_string(aSource.BeginReading(fromBegin), aSource.BeginReading(fromEnd).advance( PRInt32(aLength) ), toBegin);
    return aDest;
  }

NS_COM
PRBool
IsASCII( const nsAReadableString& aString )
  {
    static const PRUnichar NOT_ASCII = PRUnichar(~0x007F);


    // Don't want to use |copy_string| for this task, since we can stop at the first non-ASCII character

    nsReadingIterator<PRUnichar> done_reading;
    aString.EndReading(done_reading);

      // for each chunk of |aString|...
    PRUint32 fragmentLength = 0;
    nsReadingIterator<PRUnichar> iter;
    for ( aString.BeginReading(iter); iter != done_reading; iter.advance( PRInt32(fragmentLength) ) )
      {
        fragmentLength = PRUint32(iter.size_forward());
        const PRUnichar* c = iter.get();
        const PRUnichar* fragmentEnd = c + fragmentLength;

          // for each character in this chunk...
        while ( c < fragmentEnd )
          if ( *c++ & NOT_ASCII )
            return PR_FALSE;
      }

    return PR_TRUE;
  }



  /**
   * A character sink for case conversion.
   */
template <class CharT>
class ConvertToUpperCase
  {
    public:
      typedef CharT value_type;

      PRUint32
      write( const CharT* aSource, PRUint32 aSourceLength )
        {
          for ( int i=0; i<aSourceLength; ++i )
            NS_CONST_CAST(CharT*, aSource)[i] = nsCRT::ToUpper(aSource[i]);
          return aSourceLength;
        }
  };

NS_COM
void
ToUpperCase( nsAWritableString& aString )
  {
    nsAWritableString::iterator fromBegin, fromEnd;
    ConvertToUpperCase<PRUnichar> converter;
    copy_string(aString.BeginWriting(fromBegin), aString.EndWriting(fromEnd), converter);
  }

NS_COM
void
ToUpperCase( nsAWritableCString& aCString )
  {
    nsAWritableCString::iterator fromBegin, fromEnd;
    ConvertToUpperCase<char> converter;
    copy_string(aCString.BeginWriting(fromBegin), aCString.EndWriting(fromEnd), converter);
  }


  /**
   * A character sink for case conversion.
   */
template <class CharT>
class ConvertToLowerCase
  {
    public:
      typedef CharT value_type;

      PRUint32
      write( const CharT* aSource, PRUint32 aSourceLength )
        {
          for ( int i=0; i<aSourceLength; ++i )
            NS_CONST_CAST(CharT*, aSource)[i] = nsCRT::ToLower(aSource[i]);
          return aSourceLength;
        }
  };

NS_COM
void
ToLowerCase( nsAWritableString& aString )
  {
    nsAWritableString::iterator fromBegin, fromEnd;
    ConvertToLowerCase<PRUnichar> converter;
    copy_string(aString.BeginWriting(fromBegin), aString.EndWriting(fromEnd), converter);
  }

NS_COM
void
ToLowerCase( nsAWritableCString& aCString )
  {
    nsAWritableCString::iterator fromBegin, fromEnd;
    ConvertToLowerCase<char> converter;
    copy_string(aCString.BeginWriting(fromBegin), aCString.EndWriting(fromEnd), converter);
  }
