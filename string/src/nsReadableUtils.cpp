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

#ifndef nsStringTraits_h___
#include "nsStringTraits.h"
#endif

  /**
   * this allocator definition, and the global functions to access it need to move
   * to their own file
   */

template <class CharT>
class XPCOM_StringAllocator
    : public nsStringAllocator<CharT>
  {
    public:
      virtual void Deallocate( CharT* ) const;
  };

template <class CharT>
void
XPCOM_StringAllocator<CharT>::Deallocate( CharT* aBuffer ) const
  {
    nsMemory::Free(aBuffer);
  }

NS_COM
nsStringAllocator<char>&
StringAllocator_char()
  {
    static XPCOM_StringAllocator<char> sStringAllocator_char;
    return sStringAllocator_char;
  }

NS_COM
nsStringAllocator<PRUnichar>&
StringAllocator_wchar_t()
  {
    static XPCOM_StringAllocator<PRUnichar> sStringAllocator_wchar_t;
    return sStringAllocator_wchar_t;
  }

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
Distance( const nsAString::const_iterator& aStart, const nsAString::const_iterator& aEnd )
  {
    return Distance_Impl(aStart, aEnd);
  }

NS_COM
size_t
Distance( const nsACString::const_iterator& aStart, const nsACString::const_iterator& aEnd )
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
CopyUCS2toASCII( const nsAString& aSource, nsACString& aDest )
  {
      // right now, this won't work on multi-fragment destinations
    aDest.SetLength(aSource.Length());

    nsAString::const_iterator fromBegin, fromEnd;

    nsACString::iterator toBegin;
    LossyConvertEncoding<PRUnichar, char> converter(aDest.BeginWriting(toBegin).get());
    
    copy_string(aSource.BeginReading(fromBegin), aSource.EndReading(fromEnd), converter);
  }

NS_COM
void
CopyASCIItoUCS2( const nsACString& aSource, nsAString& aDest )
  {
      // right now, this won't work on multi-fragment destinations
    aDest.SetLength(aSource.Length());

    nsACString::const_iterator fromBegin, fromEnd;

    nsAString::iterator toBegin;
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
template <class FromStringT, class ToCharT>
inline
ToCharT*
AllocateStringCopy( const FromStringT& aSource, ToCharT* )
  {
    return NS_STATIC_CAST(ToCharT*, nsMemory::Alloc((aSource.Length()+1) * sizeof(ToCharT)));
  }


NS_COM
char*
ToNewCString( const nsAString& aSource )
  {
    char* result = AllocateStringCopy(aSource, (char*)0);

    nsAString::const_iterator fromBegin, fromEnd;
    LossyConvertEncoding<PRUnichar, char> converter(result);
    copy_string(aSource.BeginReading(fromBegin), aSource.EndReading(fromEnd), converter).write_terminator();
    return result;
  }

NS_COM
char*
ToNewUTF8String( const nsAString& aSource )
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
ToNewCString( const nsACString& aSource )
  {
    // no conversion needed, just allocate a buffer of the correct length and copy into it

    char* result = AllocateStringCopy(aSource, (char*)0);

    nsACString::const_iterator fromBegin, fromEnd;
    char* toBegin = result;
    *copy_string(aSource.BeginReading(fromBegin), aSource.EndReading(fromEnd), toBegin) = char(0);
    return result;
  }

NS_COM
PRUnichar*
ToNewUnicode( const nsAString& aSource )
  {
    // no conversion needed, just allocate a buffer of the correct length and copy into it

    PRUnichar* result = AllocateStringCopy(aSource, (PRUnichar*)0);

    nsAString::const_iterator fromBegin, fromEnd;
    PRUnichar* toBegin = result;
    *copy_string(aSource.BeginReading(fromBegin), aSource.EndReading(fromEnd), toBegin) = PRUnichar(0);
    return result;
  }

NS_COM
PRUnichar*
ToNewUnicode( const nsACString& aSource )
  {
    PRUnichar* result = AllocateStringCopy(aSource, (PRUnichar*)0);

    nsACString::const_iterator fromBegin, fromEnd;
    LossyConvertEncoding<char, PRUnichar> converter(result);
    copy_string(aSource.BeginReading(fromBegin), aSource.EndReading(fromEnd), converter).write_terminator();
    return result;
  }

NS_COM
PRUnichar*
CopyUnicodeTo( const nsAString& aSource, PRUint32 aSrcOffset, PRUnichar* aDest, PRUint32 aLength )
  {
    nsAString::const_iterator fromBegin, fromEnd;
    PRUnichar* toBegin = aDest;    
    copy_string(aSource.BeginReading(fromBegin).advance( PRInt32(aSrcOffset) ), aSource.BeginReading(fromEnd).advance( PRInt32(aSrcOffset+aLength) ), toBegin);
    return aDest;
  }

NS_COM 
void 
CopyUnicodeTo( const nsAString::const_iterator& aSrcStart,
               const nsAString::const_iterator& aSrcEnd,
               nsAString& aDest )
  {
    nsAString::iterator writer;
    aDest.SetLength(Distance(aSrcStart, aSrcEnd));
    aDest.BeginWriting(writer);
    nsAString::const_iterator fromBegin(aSrcStart);
    
    copy_string(fromBegin, aSrcEnd, writer);
  }

NS_COM 
void 
AppendUnicodeTo( const nsAString::const_iterator& aSrcStart,
                 const nsAString::const_iterator& aSrcEnd,
                 nsAString& aDest )
  {
    nsAString::iterator writer;
    PRUint32 oldLength = aDest.Length();
    aDest.SetLength(oldLength + Distance(aSrcStart, aSrcEnd));
    aDest.BeginWriting(writer).advance(oldLength);
    nsAString::const_iterator fromBegin(aSrcStart);
    
    copy_string(fromBegin, aSrcEnd, writer);
  }

NS_COM
PRBool
IsASCII( const nsAString& aString )
  {
    static const PRUnichar NOT_ASCII = PRUnichar(~0x007F);


    // Don't want to use |copy_string| for this task, since we can stop at the first non-ASCII character

    nsAString::const_iterator done_reading;
    aString.EndReading(done_reading);

      // for each chunk of |aString|...
    PRUint32 fragmentLength = 0;
    nsAString::const_iterator iter;
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
ToUpperCase( nsAString& aString )
  {
    nsAString::iterator fromBegin, fromEnd;
    ConvertToUpperCase<PRUnichar> converter;
    copy_string(aString.BeginWriting(fromBegin), aString.EndWriting(fromEnd), converter);
  }

NS_COM
void
ToUpperCase( nsACString& aCString )
  {
    nsACString::iterator fromBegin, fromEnd;
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
ToLowerCase( nsAString& aString )
  {
    nsAString::iterator fromBegin, fromEnd;
    ConvertToLowerCase<PRUnichar> converter;
    copy_string(aString.BeginWriting(fromBegin), aString.EndWriting(fromEnd), converter);
  }

NS_COM
void
ToLowerCase( nsACString& aCString )
  {
    nsACString::iterator fromBegin, fromEnd;
    ConvertToLowerCase<char> converter;
    copy_string(aCString.BeginWriting(fromBegin), aCString.EndWriting(fromEnd), converter);
  }

template <class StringT, class IteratorT, class Comparator>
PRBool
FindInReadable_Impl( const StringT& aPattern, IteratorT& aSearchStart, IteratorT& aSearchEnd, const Comparator& compare )
  {
    PRBool found_it = PR_FALSE;

      // only bother searching at all if we're given a non-empty range to search
    if ( aSearchStart != aSearchEnd )
      {
        IteratorT aPatternStart, aPatternEnd;
        aPattern.BeginReading(aPatternStart);
        aPattern.EndReading(aPatternEnd);

          // outer loop keeps searching till we find it or run out of string to search
        while ( !found_it )
          {
              // fast inner loop (that's what it's called, not what it is) looks for a potential match
            while ( aSearchStart != aSearchEnd && !compare(*aPatternStart, *aSearchStart) )
              ++aSearchStart;

              // if we broke out of the `fast' loop because we're out of string ... we're done: no match
            if ( aSearchStart == aSearchEnd )
              break;

              // otherwise, we're at a potential match, let's see if we really hit one
            IteratorT testPattern(aPatternStart);
            IteratorT testSearch(aSearchStart);

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
                if ( !compare(*testPattern, *testSearch) )
                  {
                    ++aSearchStart;
                    break;
                  }
              }
          }
      }

    return found_it;
  }


class CaseSensitivePRUnicharComparator
  {
    public:
      PRBool operator()( PRUnichar lhs, PRUnichar rhs ) const { return lhs == rhs; }
  };

class CaseSensitiveCharComparator
  {
    public:
      PRBool operator()( char lhs, char rhs ) const { return lhs == rhs; }
  };

class CaseInsensitivePRUnicharComparator
  {
    public:
      PRBool operator()( PRUnichar lhs, PRUnichar rhs ) const { return nsCRT::ToUpper(lhs) == nsCRT::ToUpper(rhs); }
  };

class CaseInsensitiveCharComparator
  {
    public:
      PRBool operator()( char lhs, char rhs ) const { return nsCRT::ToUpper(lhs) == nsCRT::ToUpper(rhs); }
  };

NS_COM
PRBool
FindInReadable( const nsAString& aPattern, nsAString::const_iterator& aSearchStart, nsAString::const_iterator& aSearchEnd )
  {
    return FindInReadable_Impl(aPattern, aSearchStart, aSearchEnd, CaseSensitivePRUnicharComparator());
  }

NS_COM
PRBool
FindInReadable( const nsACString& aPattern, nsACString::const_iterator& aSearchStart, nsACString::const_iterator& aSearchEnd )
  {
    return FindInReadable_Impl(aPattern, aSearchStart, aSearchEnd, CaseSensitiveCharComparator());
  }

NS_COM
PRBool
CaseInsensitiveFindInReadable( const nsAString& aPattern, nsAString::const_iterator& aSearchStart, nsAString::const_iterator& aSearchEnd )
  {
    return FindInReadable_Impl(aPattern, aSearchStart, aSearchEnd, CaseInsensitivePRUnicharComparator());
  }

NS_COM
PRBool
CaseInsensitiveFindInReadable( const nsACString& aPattern, nsACString::const_iterator& aSearchStart, nsACString::const_iterator& aSearchEnd )
  {
    return FindInReadable_Impl(aPattern, aSearchStart, aSearchEnd, CaseInsensitiveCharComparator());
  }

  /**
   * This implementation is simple, but does too much work.
   * It searches the entire string from left to right, and returns the last match found, if any.
   * This implementation will be replaced when I get |reverse_iterator|s working.
   */
NS_COM
PRBool
RFindInReadable( const nsAString& aPattern, nsAString::const_iterator& aSearchStart, nsAString::const_iterator& aSearchEnd )
  {
    PRBool found_it = PR_FALSE;

    nsAString::const_iterator savedSearchEnd(aSearchEnd);
    nsAString::const_iterator searchStart(aSearchStart), searchEnd(aSearchEnd);

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
RFindInReadable( const nsACString& aPattern, nsACString::const_iterator& aSearchStart, nsACString::const_iterator& aSearchEnd )
  {
    PRBool found_it = PR_FALSE;

    nsACString::const_iterator savedSearchEnd(aSearchEnd);
    nsACString::const_iterator searchStart(aSearchStart), searchEnd(aSearchEnd);

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

PRBool
nsSubstituteString::IsDependentOn( const nsAString& aString ) const
  {
    return mText.IsDependentOn(aString) || mPattern.IsDependentOn(aString) || mReplacement.IsDependentOn(aString);
  }

PRUint32
nsSubstituteString::MaxLength() const
  {
    PRInt32 numberOfMatches = mNumberOfMatches;

      // if we don't know exactly how long the result will be,
      //  calculate the longest possible result
    if ( numberOfMatches < 0 )
      {
        if ( mReplacement.Length() <= mPattern.Length() )
          numberOfMatches = 0;  // substitutions shrink the result, so worst case is none
        else
          numberOfMatches = PRInt32(mText.Length() / mPattern.Length());
                  // substitutions grow the result, so worst case is the maximum number of times |mPattern| can be found
      }

    PRInt32 costPerMatch = PRInt32(mReplacement.Length()) - PRInt32(mPattern.Length());
    return mText.Length() + (numberOfMatches * costPerMatch);
  }

void
nsSubstituteString::CountMatches() const
  {
    nsAString::const_iterator textEnd;
    nsAString::const_iterator searchEnd = mText.EndReading(textEnd);

    nsAString::const_iterator searchStart;
    mText.BeginReading(searchStart);

    PRInt32 numberOfMatches = 0;
    while ( FindInReadable(mPattern, searchStart, searchEnd) )
      {
        ++numberOfMatches;
        searchStart = searchEnd;
        searchEnd = textEnd;
      }

    NS_CONST_CAST(nsSubstituteString*, this)->mNumberOfMatches = numberOfMatches;
  }

PRUint32
nsSubstituteString::Length() const
  {
    if ( mNumberOfMatches < 0 )
      CountMatches();
    return MaxLength();
  }

PRUnichar*
nsSubstituteString::operator()( PRUnichar* aDestBuffer ) const
  {
    nsAString::const_iterator replacementEnd;
    mReplacement.EndReading(replacementEnd);

    nsAString::const_iterator textEnd;
    nsAString::const_iterator searchEnd = mText.EndReading(textEnd);

    nsAString::const_iterator uncopiedStart;
    nsAString::const_iterator searchStart = mText.BeginReading(uncopiedStart);

    while ( FindInReadable(mPattern, searchStart, searchEnd) )
      {
        // |searchStart| and |searchEnd| now bracket the match
 
          // copy everything up to this match
        copy_string(uncopiedStart, searchStart, aDestBuffer);  // updates |aDestBuffer|

          // copy the replacement
        nsAString::const_iterator replacementStart;
        copy_string(mReplacement.BeginReading(replacementStart), replacementEnd, aDestBuffer);

          // start searching from where the current match ends
        uncopiedStart = searchStart = searchEnd;
        searchEnd = textEnd;
      }

      // copy everything after the final (if any) match
    copy_string(uncopiedStart, textEnd, aDestBuffer);
    return aDestBuffer;
  }

PRBool
nsSubstituteCString::IsDependentOn( const nsACString& aString ) const
  {
    return mText.IsDependentOn(aString) || mPattern.IsDependentOn(aString) || mReplacement.IsDependentOn(aString);
  }

PRUint32
nsSubstituteCString::MaxLength() const
  {
    PRInt32 numberOfMatches = mNumberOfMatches;

      // if we don't know exactly how long the result will be,
      //  calculate the longest possible result
    if ( numberOfMatches < 0 )
      {
        if ( mReplacement.Length() <= mPattern.Length() )
          numberOfMatches = 0;  // substitutions shrink the result, so worst case is none
        else
          numberOfMatches = PRInt32(mText.Length() / mPattern.Length());
                  // substitutions grow the result, so worst case is the maximum number of times |mPattern| can be found
      }

    PRInt32 costPerMatch = PRInt32(mReplacement.Length()) - PRInt32(mPattern.Length());
    return mText.Length() + (numberOfMatches * costPerMatch);
  }

void
nsSubstituteCString::CountMatches() const
  {
    nsACString::const_iterator textEnd;
    nsACString::const_iterator searchEnd = mText.EndReading(textEnd);

    nsACString::const_iterator searchStart;
    mText.BeginReading(searchStart);

    PRInt32 numberOfMatches = 0;
    while ( FindInReadable(mPattern, searchStart, searchEnd) )
      {
        ++numberOfMatches;
        searchStart = searchEnd;
        searchEnd = textEnd;
      }

    NS_CONST_CAST(nsSubstituteCString*, this)->mNumberOfMatches = numberOfMatches;
  }

PRUint32
nsSubstituteCString::Length() const
  {
    if ( mNumberOfMatches < 0 )
      CountMatches();
    return MaxLength();
  }

char*
nsSubstituteCString::operator()( char* aDestBuffer ) const
  {
    nsACString::const_iterator replacementEnd;
    mReplacement.EndReading(replacementEnd);

    nsACString::const_iterator textEnd;
    nsACString::const_iterator searchEnd = mText.EndReading(textEnd);

    nsACString::const_iterator uncopiedStart;
    nsACString::const_iterator searchStart = mText.BeginReading(uncopiedStart);

    while ( FindInReadable(mPattern, searchStart, searchEnd) )
      {
        // |searchStart| and |searchEnd| now bracket the match
 
          // copy everything up to this match
        copy_string(uncopiedStart, searchStart, aDestBuffer);  // updates |aDestBuffer|

          // copy the replacement
        nsACString::const_iterator replacementStart;
        copy_string(mReplacement.BeginReading(replacementStart), replacementEnd, aDestBuffer);

          // start searching from where the current match ends
        uncopiedStart = searchStart = searchEnd;
        searchEnd = textEnd;
      }

      // copy everything after the final (if any) match
    copy_string(uncopiedStart, textEnd, aDestBuffer);
    return aDestBuffer;
  }

NS_COM 
PRBool 
FindCharInReadable( PRUnichar aChar, nsAString::const_iterator& aSearchStart, const nsAString::const_iterator& aSearchEnd )
  {
    while ( aSearchStart != aSearchEnd )
      {
        PRInt32 fragmentLength;
        if ( SameFragment(aSearchStart, aSearchEnd) ) 
          fragmentLength = aSearchEnd.get() - aSearchStart.get();
        else
          fragmentLength = aSearchStart.size_forward();

        const PRUnichar* charFoundAt = nsCharTraits<PRUnichar>::find(aSearchStart.get(), fragmentLength, aChar);
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
FindCharInReadable( char aChar, nsACString::const_iterator& aSearchStart, const nsACString::const_iterator& aSearchEnd )
  {
    while ( aSearchStart != aSearchEnd )
      {
        PRInt32 fragmentLength;
        if ( SameFragment(aSearchStart, aSearchEnd) ) 
          fragmentLength = aSearchEnd.get() - aSearchStart.get();
        else
          fragmentLength = aSearchStart.size_forward();

        const char* charFoundAt = nsCharTraits<char>::find(aSearchStart.get(), fragmentLength, aChar);
        if ( charFoundAt ) {
          aSearchStart.advance( charFoundAt - aSearchStart.get() );
          return PR_TRUE;
        }

        aSearchStart.advance(fragmentLength);
      }

    return PR_FALSE;
  } 

NS_COM 
PRUint32 
CountCharInReadable( const nsAString& aStr,
                     PRUnichar aChar )
{
  PRUint32 count = 0;
  nsAString::const_iterator begin, end;
  
  aStr.BeginReading(begin);
  aStr.EndReading(end);
  
  while (begin != end) {
    if (*begin == aChar) {
      ++count;
    }
    ++begin;
  }

  return count;
}

NS_COM 
PRUint32 
CountCharInReadable( const nsACString& aStr,
                     char aChar )
{
  PRUint32 count = 0;
  nsACString::const_iterator begin, end;
  
  aStr.BeginReading(begin);
  aStr.EndReading(end);
  
  while (begin != end) {
    if (*begin == aChar) {
      ++count;
    }
    ++begin;
  }

  return count;
}
