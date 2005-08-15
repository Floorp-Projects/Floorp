/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
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
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 2000
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Scott Collins <scc@mozilla.org> (original author)
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either of the GNU General Public License Version 2 or later (the "GPL"),
 * or the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
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

#include "nsReadableUtils.h"
#include "nsMemory.h"
#include "nsString.h"
#include "nsUTF8Utils.h"

NS_COM
void
LossyCopyUTF16toASCII( const nsAString& aSource, nsACString& aDest )
  {
    aDest.Truncate();
    LossyAppendUTF16toASCII(aSource, aDest);
  }

NS_COM
void
CopyASCIItoUTF16( const nsACString& aSource, nsAString& aDest )
  {
    aDest.Truncate();
    AppendASCIItoUTF16(aSource, aDest);
  }

NS_COM
void
LossyCopyUTF16toASCII( const PRUnichar* aSource, nsACString& aDest )
  {
    aDest.Truncate();
    if (aSource) {
      LossyAppendUTF16toASCII(nsDependentString(aSource), aDest);
    }
  }

NS_COM
void
CopyASCIItoUTF16( const char* aSource, nsAString& aDest )
  {
    aDest.Truncate();
    if (aSource) {
      AppendASCIItoUTF16(nsDependentCString(aSource), aDest);
    }
  }

NS_COM
void
CopyUTF16toUTF8( const nsAString& aSource, nsACString& aDest )
  {
    aDest.Truncate();
    AppendUTF16toUTF8(aSource, aDest);
  }

NS_COM
void
CopyUTF8toUTF16( const nsACString& aSource, nsAString& aDest )
  {
    aDest.Truncate();
    AppendUTF8toUTF16(aSource, aDest);
  }

NS_COM
void
CopyUTF16toUTF8( const PRUnichar* aSource, nsACString& aDest )
  {
    aDest.Truncate();
    AppendUTF16toUTF8(aSource, aDest);
  }

NS_COM
void
CopyUTF8toUTF16( const char* aSource, nsAString& aDest )
  {
    aDest.Truncate();
    AppendUTF8toUTF16(aSource, aDest);
  }

NS_COM
void
LossyAppendUTF16toASCII( const nsAString& aSource, nsACString& aDest )
  {
    PRUint32 old_dest_length = aDest.Length();
    aDest.SetLength(old_dest_length + aSource.Length());

    nsAString::const_iterator fromBegin, fromEnd;

    nsACString::iterator dest;
    aDest.BeginWriting(dest);

    dest.advance(old_dest_length);

      // right now, this won't work on multi-fragment destinations
    LossyConvertEncoding<PRUnichar, char> converter(dest.get());
    
    copy_string(aSource.BeginReading(fromBegin), aSource.EndReading(fromEnd), converter);
  }

NS_COM
void
AppendASCIItoUTF16( const nsACString& aSource, nsAString& aDest )
  {
    PRUint32 old_dest_length = aDest.Length();
    aDest.SetLength(old_dest_length + aSource.Length());

    nsACString::const_iterator fromBegin, fromEnd;

    nsAString::iterator dest;
    aDest.BeginWriting(dest);

    dest.advance(old_dest_length);

      // right now, this won't work on multi-fragment destinations
    LossyConvertEncoding<char, PRUnichar> converter(dest.get());

    copy_string(aSource.BeginReading(fromBegin), aSource.EndReading(fromEnd), converter);
  }

NS_COM
void
LossyAppendUTF16toASCII( const PRUnichar* aSource, nsACString& aDest )
  {
    if (aSource) {
      LossyAppendUTF16toASCII(nsDependentString(aSource), aDest);
    }
  }

NS_COM
void
AppendASCIItoUTF16( const char* aSource, nsAString& aDest )
  {
    if (aSource) {
      AppendASCIItoUTF16(nsDependentCString(aSource), aDest);
    }
  }

NS_COM
void
AppendUTF16toUTF8( const nsAString& aSource, nsACString& aDest )
  {
    nsAString::const_iterator source_start, source_end;
    CalculateUTF8Size calculator;
    copy_string(aSource.BeginReading(source_start),
                aSource.EndReading(source_end), calculator);

    PRUint32 count = calculator.Size();

    if (count)
      {
        PRUint32 old_dest_length = aDest.Length();

        // Grow the buffer if we need to.
        aDest.SetLength(old_dest_length + count);

        nsACString::iterator dest;
        aDest.BeginWriting(dest);

        dest.advance(old_dest_length);

        if (count <= (PRUint32)dest.size_forward())
          {
            // aDest has enough room in the fragment just past the end
            // of its old data that it can hold what we're about to
            // append. Append using copy_string().

            // All ready? Time to convert

            ConvertUTF16toUTF8 converter(dest.get());
            copy_string(aSource.BeginReading(source_start),
                        aSource.EndReading(source_end), converter);

            if (converter.Size() != count)
              {
                NS_ERROR("Input invalid or incorrect length was calculated");

                aDest.SetLength(old_dest_length);
              }
          }
        else
          {
            // This isn't the fastest way to do this, but it gets
            // complicated to convert UTF16 into a fragmented UTF8
            // string, so we'll take the easy way out here in this
            // rare situation.

            aDest.Replace(old_dest_length, count,
                          NS_ConvertUTF16toUTF8(aSource));
          }
      }
  }

NS_COM
void
AppendUTF8toUTF16( const nsACString& aSource, nsAString& aDest )
  {
    nsACString::const_iterator source_start, source_end;
    CalculateUTF8Length calculator;
    copy_string(aSource.BeginReading(source_start),
                aSource.EndReading(source_end), calculator);

    PRUint32 count = calculator.Length();

    if (count)
      {
        PRUint32 old_dest_length = aDest.Length();

        // Grow the buffer if we need to.
        aDest.SetLength(old_dest_length + count);

        nsAString::iterator dest;
        aDest.BeginWriting(dest);

        dest.advance(old_dest_length);

        if (count <= (PRUint32)dest.size_forward())
          {
            // aDest has enough room in the fragment just past the end
            // of its old data that it can hold what we're about to
            // append. Append using copy_string().

            // All ready? Time to convert

            ConvertUTF8toUTF16 converter(dest.get());
            copy_string(aSource.BeginReading(source_start),
                        aSource.EndReading(source_end), converter);

            if (converter.Length() != count)
              {
                NS_ERROR("Input wasn't UTF8 or incorrect length was calculated");
                aDest.SetLength(old_dest_length);
              }
          }
        else
          {
            // This isn't the fastest way to do this, but it gets
            // complicated to convert parts of a UTF8 string into a
            // UTF16 string, so we'll take the easy way out here in
            // this rare situation.

            aDest.Replace(old_dest_length, count,
                          NS_ConvertUTF8toUTF16(aSource));
          }
      }
  }

NS_COM
void
AppendUTF16toUTF8( const PRUnichar* aSource, nsACString& aDest )
  {
    if (aSource) {
      AppendUTF16toUTF8(nsDependentString(aSource), aDest);
    }
  }

NS_COM
void
AppendUTF8toUTF16( const char* aSource, nsAString& aDest )
  {
    if (aSource) {
      AppendUTF8toUTF16(nsDependentCString(aSource), aDest);
    }
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
    if (!result)
      return nsnull;

    nsAString::const_iterator fromBegin, fromEnd;
    LossyConvertEncoding<PRUnichar, char> converter(result);
    copy_string(aSource.BeginReading(fromBegin), aSource.EndReading(fromEnd), converter).write_terminator();
    return result;
  }

NS_COM
char*
ToNewUTF8String( const nsAString& aSource, PRUint32 *aUTF8Count )
  {
    nsAString::const_iterator start, end;
    CalculateUTF8Size calculator;
    copy_string(aSource.BeginReading(start), aSource.EndReading(end),
                calculator);

    if (aUTF8Count)
      *aUTF8Count = calculator.Size();

    char *result = NS_STATIC_CAST(char*,
        nsMemory::Alloc(calculator.Size() + 1));
    if (!result)
      return nsnull;

    ConvertUTF16toUTF8 converter(result);
    copy_string(aSource.BeginReading(start), aSource.EndReading(end),
                converter).write_terminator();
    NS_ASSERTION(calculator.Size() == converter.Size(), "length mismatch");

    return result;
  }

NS_COM
char*
ToNewCString( const nsACString& aSource )
  {
    // no conversion needed, just allocate a buffer of the correct length and copy into it

    char* result = AllocateStringCopy(aSource, (char*)0);
    if (!result)
      return nsnull;

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
    if (!result)
      return nsnull;

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
    if (!result)
      return nsnull;

    nsACString::const_iterator fromBegin, fromEnd;
    LossyConvertEncoding<char, PRUnichar> converter(result);
    copy_string(aSource.BeginReading(fromBegin), aSource.EndReading(fromEnd), converter).write_terminator();
    return result;
  }

NS_COM
PRUnichar*
UTF8ToNewUnicode( const nsACString& aSource, PRUint32 *aUTF16Count )
  {
    nsACString::const_iterator start, end;
    CalculateUTF8Length calculator;
    copy_string(aSource.BeginReading(start), aSource.EndReading(end),
                calculator);

    if (aUTF16Count)
      *aUTF16Count = calculator.Length();

    PRUnichar *result = NS_STATIC_CAST(PRUnichar*,
        nsMemory::Alloc(sizeof(PRUnichar) * (calculator.Length() + 1)));
    if (!result)
      return nsnull;

    ConvertUTF8toUTF16 converter(result);
    copy_string(aSource.BeginReading(start), aSource.EndReading(end),
                converter).write_terminator();
    NS_ASSERTION(calculator.Length() == converter.Length(), "length mismatch");

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

NS_COM
PRBool
IsASCII( const nsACString& aString )
  {
    static const char NOT_ASCII = char(~0x7F);


    // Don't want to use |copy_string| for this task, since we can stop at the first non-ASCII character

    nsACString::const_iterator done_reading;
    aString.EndReading(done_reading);

      // for each chunk of |aString|...
    PRUint32 fragmentLength = 0;
    nsACString::const_iterator iter;
    for ( aString.BeginReading(iter); iter != done_reading; iter.advance( PRInt32(fragmentLength) ) )
      {
        fragmentLength = PRUint32(iter.size_forward());
        const char* c = iter.get();
        const char* fragmentEnd = c + fragmentLength;

          // for each character in this chunk...
        while ( c < fragmentEnd )
          if ( *c++ & NOT_ASCII )
            return PR_FALSE;
      }

    return PR_TRUE;
  }

NS_COM
PRBool
IsUTF8( const nsACString& aString )
  {
    nsReadingIterator<char> done_reading;
    aString.EndReading(done_reading);

    PRInt32 state = 0;
    PRBool overlong = PR_FALSE;
    PRBool surrogate = PR_FALSE;
    PRBool nonchar = PR_FALSE;
    PRUint16 olupper = 0; // overlong byte upper bound.
    PRUint16 slower = 0;  // surrogate byte lower bound.

      // for each chunk of |aString|...
    PRUint32 fragmentLength = 0;
    nsReadingIterator<char> iter;

    for ( aString.BeginReading(iter); iter != done_reading; iter.advance( PRInt32(fragmentLength) ) )
      {
        fragmentLength = PRUint32(iter.size_forward());
        const char* ptr = iter.get();
        const char* fragmentEnd = ptr + fragmentLength;

          // for each character in this chunk...
        while ( ptr < fragmentEnd )
          {
            PRUint8 c;
            
            if (0 == state)
              {
                c = *ptr++;

                if ( UTF8traits::isASCII(c) ) 
                  continue;

                if ( c <= 0xC1 ) // [80-BF] where not expected, [C0-C1] for overlong.
                  return PR_FALSE;
                else if ( UTF8traits::is2byte(c) ) 
                    state = 1;
                else if ( UTF8traits::is3byte(c) ) 
                  {
                    state = 2;
                    if ( c == 0xE0 ) // to exclude E0[80-9F][80-BF] 
                      {
                        overlong = PR_TRUE;
                        olupper = 0x9F;
                      }
                    else if ( c == 0xED ) // ED[A0-BF][80-BF] : surrogate codepoint
                      {
                        surrogate = PR_TRUE;
                        slower = 0xA0;
                      }
                    else if ( c == 0xEF ) // EF BF [BE-BF] : non-character
                      nonchar = PR_TRUE;
                  }
                else if ( c <= 0xF4 ) // XXX replace /w UTF8traits::is4byte when it's updated to exclude [F5-F7].(bug 199090)
                  {
                    state = 3;
                    nonchar = PR_TRUE;
                    if ( c == 0xF0 ) // to exclude F0[80-8F][80-BF]{2}
                      {
                        overlong = PR_TRUE;
                        olupper = 0x8F;
                      }
                    else if ( c == 0xF4 ) // to exclude F4[90-BF][80-BF] 
                      {
                        // actually not surrogates but codepoints beyond 0x10FFFF
                        surrogate = PR_TRUE;
                        slower = 0x90;
                      }
                  }
                else
                  return PR_FALSE; // Not UTF-8 string
              }
              
              while (ptr < fragmentEnd && state)
                {
                  c = *ptr++;
                  --state;

                  // non-character : EF BF [BE-BF] or F[0-7] [89AB]F BF [BE-BF]
                  if ( nonchar &&  ( !state &&  c < 0xBE ||
                       state == 1 && c != 0xBF  ||
                       state == 2 && 0x0F != (0x0F & c) ))
                     nonchar = PR_FALSE;

                  if ( !UTF8traits::isInSeq(c) || overlong && c <= olupper || 
                       surrogate && slower <= c || nonchar && !state )
                    return PR_FALSE; // Not UTF-8 string
                  overlong = surrogate = PR_FALSE;
                }
            }
        }
    return !state; // state != 0 at the end indicates an invalid UTF-8 seq. 
  }

  /**
   * A character sink for in-place case conversion.
   */
class ConvertToUpperCase
  {
    public:
      typedef char value_type;

      PRUint32
      write( const char* aSource, PRUint32 aSourceLength )
        {
          char* cp = NS_CONST_CAST(char*,aSource);
          const char* end = aSource + aSourceLength;
          while (cp != end) {
            char ch = *cp;
            if ((ch >= 'a') && (ch <= 'z'))
              *cp = ch - ('a' - 'A');
            ++cp;
          }
          return aSourceLength;
        }
  };

#ifdef MOZ_V1_STRING_ABI
NS_COM
void
ToUpperCase( nsACString& aCString )
  {
    nsACString::iterator fromBegin, fromEnd;
    ConvertToUpperCase converter;
    copy_string(aCString.BeginWriting(fromBegin), aCString.EndWriting(fromEnd), converter);
  }
#endif

NS_COM
void
ToUpperCase( nsCSubstring& aCString )
  {
    ConvertToUpperCase converter;
    char* start;
    converter.write(aCString.BeginWriting(start), aCString.Length());
  }

  /**
   * A character sink for copying with case conversion.
   */
class CopyToUpperCase
  {
    public:
      typedef char value_type;

      CopyToUpperCase( nsACString::iterator& aDestIter )
        : mIter(aDestIter)
        {
        }

      PRUint32
      write( const char* aSource, PRUint32 aSourceLength )
        {
          PRUint32 len = PR_MIN(PRUint32(mIter.size_forward()), aSourceLength);
          char* cp = mIter.get();
          const char* end = aSource + len;
          while (aSource != end) {
            char ch = *aSource;
            if ((ch >= 'a') && (ch <= 'z'))
              *cp = ch - ('a' - 'A');
            else
              *cp = ch;
            ++aSource;
            ++cp;
          }
          mIter.advance(len);
          return len;
        }

    protected:
      nsACString::iterator& mIter;
  };

NS_COM
void
ToUpperCase( const nsACString& aSource, nsACString& aDest )
  {
    nsACString::const_iterator fromBegin, fromEnd;
    nsACString::iterator toBegin;
    aDest.SetLength(aSource.Length());
    CopyToUpperCase converter(aDest.BeginWriting(toBegin));
    copy_string(aSource.BeginReading(fromBegin), aSource.EndReading(fromEnd), converter);
  }

  /**
   * A character sink for case conversion.
   */
class ConvertToLowerCase
  {
    public:
      typedef char value_type;

      PRUint32
      write( const char* aSource, PRUint32 aSourceLength )
        {
          char* cp = NS_CONST_CAST(char*,aSource);
          const char* end = aSource + aSourceLength;
          while (cp != end) {
            char ch = *cp;
            if ((ch >= 'A') && (ch <= 'Z'))
              *cp = ch + ('a' - 'A');
            ++cp;
          }
          return aSourceLength;
        }
  };

#ifdef MOZ_V1_STRING_ABI
NS_COM
void
ToLowerCase( nsACString& aCString )
  {
    nsACString::iterator fromBegin, fromEnd;
    ConvertToLowerCase converter;
    copy_string(aCString.BeginWriting(fromBegin), aCString.EndWriting(fromEnd), converter);
  }
#endif

NS_COM
void
ToLowerCase( nsCSubstring& aCString )
  {
    ConvertToLowerCase converter;
    char* start;
    converter.write(aCString.BeginWriting(start), aCString.Length());
  }

  /**
   * A character sink for copying with case conversion.
   */
class CopyToLowerCase
  {
    public:
      typedef char value_type;

      CopyToLowerCase( nsACString::iterator& aDestIter )
        : mIter(aDestIter)
        {
        }

      PRUint32
      write( const char* aSource, PRUint32 aSourceLength )
        {
          PRUint32 len = PR_MIN(PRUint32(mIter.size_forward()), aSourceLength);
          char* cp = mIter.get();
          const char* end = aSource + len;
          while (aSource != end) {
            char ch = *aSource;
            if ((ch >= 'A') && (ch <= 'Z'))
              *cp = ch + ('a' - 'A');
            else
              *cp = ch;
            ++aSource;
            ++cp;
          }
          mIter.advance(len);
          return len;
        }

    protected:
      nsACString::iterator& mIter;
  };

NS_COM
void
ToLowerCase( const nsACString& aSource, nsACString& aDest )
  {
    nsACString::const_iterator fromBegin, fromEnd;
    nsACString::iterator toBegin;
    aDest.SetLength(aSource.Length());
    CopyToLowerCase converter(aDest.BeginWriting(toBegin));
    copy_string(aSource.BeginReading(fromBegin), aSource.EndReading(fromEnd), converter);
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
            while ( aSearchStart != aSearchEnd &&
                    compare(*aPatternStart, *aSearchStart) )
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
                if ( compare(*testPattern, *testSearch) )
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
FindInReadable( const nsAString& aPattern, nsAString::const_iterator& aSearchStart, nsAString::const_iterator& aSearchEnd, const nsStringComparator& aComparator )
  {
    return FindInReadable_Impl(aPattern, aSearchStart, aSearchEnd, aComparator);
  }

NS_COM
PRBool
FindInReadable( const nsACString& aPattern, nsACString::const_iterator& aSearchStart, nsACString::const_iterator& aSearchEnd, const nsCStringComparator& aComparator)
  {
    return FindInReadable_Impl(aPattern, aSearchStart, aSearchEnd, aComparator);
  }

NS_COM
PRBool
CaseInsensitiveFindInReadable( const nsACString& aPattern, nsACString::const_iterator& aSearchStart, nsACString::const_iterator& aSearchEnd )
  {
    return FindInReadable_Impl(aPattern, aSearchStart, aSearchEnd, nsCaseInsensitiveCStringComparator());
  }

  /**
   * This implementation is simple, but does too much work.
   * It searches the entire string from left to right, and returns the last match found, if any.
   * This implementation will be replaced when I get |reverse_iterator|s working.
   */
NS_COM
PRBool
RFindInReadable( const nsAString& aPattern, nsAString::const_iterator& aSearchStart, nsAString::const_iterator& aSearchEnd, const nsStringComparator& aComparator)
  {
    PRBool found_it = PR_FALSE;

    nsAString::const_iterator savedSearchEnd(aSearchEnd);
    nsAString::const_iterator searchStart(aSearchStart), searchEnd(aSearchEnd);

    while ( searchStart != searchEnd )
      {
        if ( FindInReadable(aPattern, searchStart, searchEnd, aComparator) )
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
RFindInReadable( const nsACString& aPattern, nsACString::const_iterator& aSearchStart, nsACString::const_iterator& aSearchEnd, const nsCStringComparator& aComparator)
  {
    PRBool found_it = PR_FALSE;

    nsACString::const_iterator savedSearchEnd(aSearchEnd);
    nsACString::const_iterator searchStart(aSearchStart), searchEnd(aSearchEnd);

    while ( searchStart != searchEnd )
      {
        if ( FindInReadable(aPattern, searchStart, searchEnd, aComparator) )
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
FindCharInReadable( PRUnichar aChar, nsAString::const_iterator& aSearchStart, const nsAString::const_iterator& aSearchEnd )
  {
    PRInt32 fragmentLength = aSearchEnd.get() - aSearchStart.get();

    const PRUnichar* charFoundAt = nsCharTraits<PRUnichar>::find(aSearchStart.get(), fragmentLength, aChar);
    if ( charFoundAt ) {
      aSearchStart.advance( charFoundAt - aSearchStart.get() );
      return PR_TRUE;
    }

    aSearchStart.advance(fragmentLength);
    return PR_FALSE;
  }

NS_COM 
PRBool 
FindCharInReadable( char aChar, nsACString::const_iterator& aSearchStart, const nsACString::const_iterator& aSearchEnd )
  {
    PRInt32 fragmentLength = aSearchEnd.get() - aSearchStart.get();

    const char* charFoundAt = nsCharTraits<char>::find(aSearchStart.get(), fragmentLength, aChar);
    if ( charFoundAt ) {
      aSearchStart.advance( charFoundAt - aSearchStart.get() );
      return PR_TRUE;
    }

    aSearchStart.advance(fragmentLength);
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

NS_COM PRBool
StringBeginsWith( const nsAString& aSource, const nsAString& aSubstring,
                  const nsStringComparator& aComparator )
  {
    nsAString::size_type src_len = aSource.Length(),
                         sub_len = aSubstring.Length();
    if (sub_len > src_len)
      return PR_FALSE;
    return Substring(aSource, 0, sub_len).Equals(aSubstring, aComparator);
  }

NS_COM PRBool
StringBeginsWith( const nsACString& aSource, const nsACString& aSubstring,
                  const nsCStringComparator& aComparator )
  {
    nsACString::size_type src_len = aSource.Length(),
                          sub_len = aSubstring.Length();
    if (sub_len > src_len)
      return PR_FALSE;
    return Substring(aSource, 0, sub_len).Equals(aSubstring, aComparator);
  }

NS_COM PRBool
StringEndsWith( const nsAString& aSource, const nsAString& aSubstring,
                const nsStringComparator& aComparator )
  {
    nsAString::size_type src_len = aSource.Length(),
                         sub_len = aSubstring.Length();
    if (sub_len > src_len)
      return PR_FALSE;
    return Substring(aSource, src_len - sub_len, sub_len).Equals(aSubstring,
                                                                 aComparator);
  }

NS_COM PRBool
StringEndsWith( const nsACString& aSource, const nsACString& aSubstring,
                const nsCStringComparator& aComparator )
  {
    nsACString::size_type src_len = aSource.Length(),
                          sub_len = aSubstring.Length();
    if (sub_len > src_len)
      return PR_FALSE;
    return Substring(aSource, src_len - sub_len, sub_len).Equals(aSubstring,
                                                                 aComparator);
  }



static const PRUnichar empty_buffer[1] = { '\0' };

NS_COM const nsAFlatString& EmptyString()
  {
    static const nsDependentString sEmpty(empty_buffer);

    return sEmpty;
  }

NS_COM const nsAFlatCString& EmptyCString()
  {
    static const nsDependentCString sEmpty((const char *)empty_buffer);

    return sEmpty;
  }
