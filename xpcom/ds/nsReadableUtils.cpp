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


template <class CharT> class CalculateLength
  {
    public:
      typedef CharT value_type;

      CalculateLength() : mDistance(0) { }
      size_t GetDistance() const       { return mDistance; }

      PRUint32 write( const CharT*, PRUint32 N )
                                       { mDistance += N; return N; }
    private:
      size_t mDistance;
  };

template <class CharT>
inline
size_t
Distance_Impl( const nsReadingIterator<CharT>& aStart,
          const nsReadingIterator<CharT>& aEnd )
  {
    CalculateLength<CharT> sink;
    nsReadingIterator<CharT> fromBegin(aStart);
    copy_string(fromBegin, aEnd, sink);
    return sink.GetDistance();
  }

NS_COM
size_t
Distance( const nsReadingIterator<PRUnichar>& aStart, const nsReadingIterator<PRUnichar>& aEnd )
  {
    return Distance_Impl(aStart, aEnd);
  }

NS_COM
size_t
Distance( const nsReadingIterator<char>& aStart, const nsReadingIterator<char>& aEnd )
  {
    return Distance_Impl(aStart, aEnd);
  }



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
CopyUnicodeTo( const nsAReadableString& aSource, PRUint32 aSrcOffset, PRUnichar* aDest, PRUint32 aLength )
  {
    nsReadingIterator<PRUnichar> fromBegin, fromEnd;
    PRUnichar* toBegin = aDest;    
    copy_string(aSource.BeginReading(fromBegin).advance( PRInt32(aSrcOffset) ), aSource.BeginReading(fromEnd).advance( PRInt32(aSrcOffset+aLength) ), toBegin);
    return aDest;
  }

NS_COM 
void 
CopyUnicodeTo( const nsReadingIterator<PRUnichar>& aSrcStart,
               const nsReadingIterator<PRUnichar>& aSrcEnd,
               nsAWritableString& aDest )
  {
    nsWritingIterator<PRUnichar> writer;
    aDest.SetLength(Distance(aSrcStart, aSrcEnd));
    aDest.BeginWriting(writer);
    nsReadingIterator<PRUnichar> fromBegin(aSrcStart);
    
    copy_string(fromBegin, aSrcEnd, writer);
  }

NS_COM 
void 
AppendUnicodeTo( const nsReadingIterator<PRUnichar>& aSrcStart,
                 const nsReadingIterator<PRUnichar>& aSrcEnd,
                 nsAWritableString& aDest )
  {
    nsWritingIterator<PRUnichar> writer;
    PRUint32 oldLength = aDest.Length();
    aDest.SetLength(oldLength + Distance(aSrcStart, aSrcEnd));
    aDest.BeginWriting(writer).advance(oldLength);
    nsReadingIterator<PRUnichar> fromBegin(aSrcStart);
    
    copy_string(fromBegin, aSrcEnd, writer);
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
          for ( PRUint32 i=0; i<aSourceLength; ++i )
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
          for ( PRUint32 i=0; i<aSourceLength; ++i )
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


template <class CharT>
inline // probably wishful thinking
PRBool
FindInReadable_Impl( const basic_nsAReadableString<CharT>& aPattern,
                     nsReadingIterator<CharT>& aSearchStart,
                     nsReadingIterator<CharT>& aSearchEnd )
  {
    PRBool found_it = PR_FALSE;

      // only bother searching at all if we're given a non-empty range to search
    if ( aSearchStart != aSearchEnd )
      {
        nsReadingIterator<CharT> aPatternStart, aPatternEnd;
        aPattern.BeginReading(aPatternStart);
        aPattern.EndReading(aPatternEnd);

          // outer loop keeps searching till we find it or run out of string to search
        while ( !found_it )
          {
              // fast inner loop (that's what it's called, not what it is) looks for a potential match
            while ( aSearchStart != aSearchEnd && *aPatternStart != *aSearchStart )
              ++aSearchStart;

              // if we broke out of the `fast' loop because we're out of string ... we're done: no match
            if ( aSearchStart == aSearchEnd )
              break;

              // otherwise, we're at a potential match, let's see if we really hit one
            nsReadingIterator<CharT> testPattern(aPatternStart);
            nsReadingIterator<CharT> testSearch(aSearchStart);

              // slow inner loop verifies the potential match (found by the `fast' loop) at the current position
            for(;;)
              {
                  // we already compared the first character in the outer loop,
                  //  so we'll advance before the next comparison
                ++testPattern;
                ++testSearch;

                  // if we verified all the way to the end of the pattern, then we found it!
                if ( testPattern == aPatternEnd )
                  {
                    found_it = PR_TRUE;
                    aSearchEnd = testSearch; // return the exact found range through the parameters
                    break;
                  }

                  // if we got to end of the string we're searching before we hit the end of the
                  //  pattern, we'll never find what we're looking for
                if ( testSearch == aSearchEnd )
                  {
                    aSearchStart = aSearchEnd;
                    break;
                  }

                  // else if we mismatched ... it's time to advance to the next search position
                  //  and get back into the `fast' loop
                if ( *testPattern != *testSearch )
                  {
                    ++aSearchStart;
                    break;
                  }
              }
          }
      }

    return found_it;
  }

NS_COM
PRBool
FindInReadable( const nsAReadableString& aPattern, nsReadingIterator<PRUnichar>& aSearchStart, nsReadingIterator<PRUnichar>& aSearchEnd )
  {
    return FindInReadable_Impl(aPattern, aSearchStart, aSearchEnd);
  }

NS_COM
PRBool
FindInReadable( const nsAReadableCString& aPattern, nsReadingIterator<char>& aSearchStart, nsReadingIterator<char>& aSearchEnd )
  {
    return FindInReadable_Impl(aPattern, aSearchStart, aSearchEnd);
  }

  /**
   * This implementation is simple, but does too much work.
   * It searches the entire string from left to right, and returns the last match found, if any.
   * This implementation will be replaced when I get |reverse_iterator|s working.
   */
template <class CharT>
inline // probably wishful thinking
PRBool
RFindInReadable_Impl( const basic_nsAReadableString<CharT>& aPattern,
                      nsReadingIterator<CharT>& aSearchStart,
                      nsReadingIterator<CharT>& aSearchEnd )
  {
    PRBool found_it = PR_FALSE;

    nsReadingIterator<CharT> savedSearchEnd(aSearchEnd);
    nsReadingIterator<CharT> searchStart(aSearchStart), searchEnd(aSearchEnd);

    while ( searchStart != searchEnd )
      {
        if ( FindInReadable(aPattern, searchStart, searchEnd) )
          {
            found_it = PR_TRUE;

              // this is the best match so far, so remember it
            aSearchStart = searchStart;
            aSearchEnd = searchEnd;

              // ...and get ready to search some more
              //  (it's tempting to set |searchStart=searchEnd| ... but that misses overlapping patterns)
            ++searchStart;
            searchEnd = savedSearchEnd;
          }
      }

      // if we never found it, return an empty range
    if ( !found_it )
      aSearchStart = aSearchEnd;

    return found_it;
  }


NS_COM
PRBool
RFindInReadable( const nsAReadableString& aPattern, nsReadingIterator<PRUnichar>& aSearchStart, nsReadingIterator<PRUnichar>& aSearchEnd )
  {
    return RFindInReadable_Impl(aPattern, aSearchStart, aSearchEnd);
  }

NS_COM
PRBool
RFindInReadable( const nsAReadableCString& aPattern, nsReadingIterator<char>& aSearchStart, nsReadingIterator<char>& aSearchEnd )
  {
    return RFindInReadable_Impl(aPattern, aSearchStart, aSearchEnd);
  }



template <class CharT>
inline // probably wishful thinking
PRBool
FindCharInReadable_Impl( CharT aChar,
                         nsReadingIterator<CharT>& aSearchStart,
                         nsReadingIterator<CharT>& aSearchEnd )
  {
    while ( aSearchStart != aSearchEnd )
      {
        PRInt32 fragmentLength;
        if ( SameFragment(aSearchStart, aSearchEnd) ) 
          fragmentLength = aSearchEnd.get() - aSearchStart.get();
        else
          fragmentLength = aSearchStart.size_forward();

        const CharT* charFoundAt = nsCharTraits<CharT>::find(aSearchStart.get(), fragmentLength, aChar);
        if ( charFoundAt ) {
          aSearchStart.advance( charFoundAt - aSearchStart.get() );
          return PR_TRUE;
        }

        aSearchStart.advance(fragmentLength);
      }

    return PR_FALSE;
  }


NS_COM 
PRBool 
FindCharInReadable( PRUnichar aChar, nsReadingIterator<PRUnichar>& aSearchStart, nsReadingIterator<PRUnichar>& aSearchEnd )
  {
    return FindCharInReadable_Impl(aChar, aSearchStart, aSearchEnd);
  }

NS_COM 
PRBool 
FindCharInReadable( char aChar, nsReadingIterator<char>& aSearchStart, nsReadingIterator<char>& aSearchEnd )
  {
    return FindCharInReadable_Impl(aChar, aSearchStart, aSearchEnd);
  } 

template <class CharT>
PRUint32 
CountCharInReadable_Impl( const basic_nsAReadableString<CharT>& aStr,
                          CharT aChar )
{
  PRUint32 count = 0;
  nsReadingIterator<CharT> begin, end;
  
  aStr.BeginReading(begin);
  aStr.EndReading(end);
  
  while (begin != end) {
    if (*begin == aChar) {
      count++;
    }
    begin++;
  }

  return count;
}

NS_COM 
PRUint32 
CountCharInReadable( const nsAReadableString& aStr,
                     PRUnichar aChar )
{
  return CountCharInReadable_Impl(aStr, aChar);
}

NS_COM 
PRUint32 
CountCharInReadable( const nsAReadableCString& aStr,
                     char aChar )
{
  return CountCharInReadable_Impl(aStr, aChar);
}
