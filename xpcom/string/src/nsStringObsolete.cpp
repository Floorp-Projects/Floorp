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

#include "nsString.h"


  /**
   * nsTString obsolete API support
   */

#if MOZ_STRING_WITH_OBSOLETE_API

#include "nsDependentString.h"
#include "nsDependentSubstring.h"
#include "nsReadableUtils.h"
#include "nsCRT.h"
#include "nsUTF8Utils.h"
#include "prdtoa.h"
#include "prprf.h"

/* ***** BEGIN RICKG BLOCK *****
 *
 * NOTE: This section of code was extracted from rickg's bufferRoutines.h file.
 *       For the most part it remains unmodified.  We want to eliminate (or at
 *       least clean up) this code at some point.  If you find the formatting
 *       in this section somewhat inconsistent, don't blame me! ;-)
 */

// XXXdarin what is wrong with STDC's tolower?
inline char
ascii_tolower(char aChar)
{
  if (aChar >= 'A' && aChar <= 'Z')
    return aChar + ('a' - 'A');
  return aChar;
}

//-----------------------------------------------------------------------------
//
//  This set of methods is used to search a buffer looking for a char.
//


/**
 *  This methods cans the given buffer for the given char
 *  
 *  @update  gess 02/17/00
 *  @param   aDest is the buffer to be searched
 *  @param   aDestLength is the size (in char-units, not bytes) of the buffer
 *  @param   anOffset is the start pos to begin searching
 *  @param   aChar is the target character we're looking for
 *  @param   aCount tells us how many characters to iterate through (which may be different than aLength); -1 means use full length.
 *  @return  index of pos if found, else -1 (kNotFound)
 */
static PRInt32
FindChar1(const char* aDest,PRUint32 aDestLength,PRInt32 anOffset,const PRUnichar aChar,PRInt32 aCount) {

  if(anOffset < 0)
    anOffset=0;

  if(aCount < 0)
    aCount = (PRInt32)aDestLength;

  if((aChar < 256) && (0 < aDestLength) && ((PRUint32)anOffset < aDestLength)) {

    //We'll only search if the given aChar is within the normal ascii a range,
    //(Since this string is definitely within the ascii range).
    
    if(0<aCount) {

      const char* left= aDest+anOffset;
      const char* last= left+aCount;
      const char* max = aDest+aDestLength;
      const char* end = (last<max) ? last : max;

      PRInt32 theMax = end-left;
      if(0<theMax) {
        
        unsigned char theChar = (unsigned char) aChar;
        const char* result=(const char*)memchr(left, (int)theChar, theMax);
        
        if(result)
          return result-aDest;
        
      }
    }
  }

  return kNotFound;
}


/**
 *  This methods cans the given buffer for the given char
 *  
 *  @update  gess 3/25/98
 *  @param   aDest is the buffer to be searched
 *  @param   aDestLength is the size (in char-units, not bytes) of the buffer
 *  @param   anOffset is the start pos to begin searching
 *  @param   aChar is the target character we're looking for
 *  @param   aCount tells us how many characters to iterate through (which may be different than aLength); -1 means use full length.
 *  @return  index of pos if found, else -1 (kNotFound)
 */
static PRInt32
FindChar2(const PRUnichar* aDest,PRUint32 aDestLength,PRInt32 anOffset,const PRUnichar aChar,PRInt32 aCount) {

  if(anOffset < 0)
    anOffset=0;

  if(aCount < 0)
    aCount = (PRInt32)aDestLength;

  if((0<aDestLength) && ((PRUint32)anOffset < aDestLength)) {
 
    if(0<aCount) {

      const PRUnichar* root = aDest;
      const PRUnichar* left = root+anOffset;
      const PRUnichar* last = left+aCount;
      const PRUnichar* max  = root+aDestLength;
      const PRUnichar* end  = (last<max) ? last : max;

      while(left<end){
        
        if(*left==aChar)
          return (left-root);
        
        ++left;
      }
    }
  }

  return kNotFound;
}


/**
 *  This methods cans the given buffer (in reverse) for the given char
 *  
 *  @update  gess 02/17/00
 *  @param   aDest is the buffer to be searched
 *  @param   aDestLength is the size (in char-units, not bytes) of the buffer
 *  @param   anOffset is the start pos to begin searching
 *  @param   aChar is the target character we're looking for
 *  @param   aCount tells us how many characters to iterate through (which may be different than aLength); -1 means use full length.
 *  @return  index of pos if found, else -1 (kNotFound)
 */

static PRInt32
RFindChar1(const char* aDest,PRUint32 aDestLength,PRInt32 anOffset,const PRUnichar aChar,PRInt32 aCount) {

  if(anOffset < 0)
    anOffset=(PRInt32)aDestLength-1;

  if(aCount < 0)
    aCount = PRInt32(aDestLength);

  if((aChar<256) && (0 < aDestLength) && ((PRUint32)anOffset < aDestLength)) {

    //We'll only search if the given aChar is within the normal ascii a range,
    //(Since this string is definitely within the ascii range).
 
    if(0 < aCount) {

      const char* rightmost = aDest + anOffset;  
      const char* min       = rightmost - aCount + 1;
      const char* leftmost  = (min<aDest) ? aDest: min;

      char theChar=(char)aChar;
      while(leftmost <= rightmost){
        
        if((*rightmost) == theChar)
          return rightmost - aDest;
        
        --rightmost;
      }
    }
  }

  return kNotFound;
}


/**
 *  This methods cans the given buffer for the given char
 *  
 *  @update  gess 3/25/98
 *  @param   aDest is the buffer to be searched
 *  @param   aDestLength is the size (in char-units, not bytes) of the buffer
 *  @param   anOffset is the start pos to begin searching
 *  @param   aChar is the target character we're looking for
 *  @param   aCount tells us how many characters to iterate through (which may be different than aLength); -1 means use full length.
 *  @return  index of pos if found, else -1 (kNotFound)
 */
static PRInt32
RFindChar2(const PRUnichar* aDest,PRUint32 aDestLength,PRInt32 anOffset,const PRUnichar aChar,PRInt32 aCount) {

  if(anOffset < 0)
    anOffset=(PRInt32)aDestLength-1;

  if(aCount < 0)
    aCount = PRInt32(aDestLength);

  if((0 < aDestLength) && ((PRUint32)anOffset < aDestLength)) {
 
    if(0 < aCount) {

      const PRUnichar* root      = aDest;
      const PRUnichar* rightmost = root + anOffset;  
      const PRUnichar* min       = rightmost - aCount + 1;
      const PRUnichar* leftmost  = (min<root) ? root: min;
      
      while(leftmost <= rightmost){
        
        if((*rightmost) == aChar)
          return rightmost - root;
        
        --rightmost;
      }
    }
  }

  return kNotFound;
}

//-----------------------------------------------------------------------------
//
//  This set of methods is used to compare one buffer onto another.  The
//  functions are differentiated by the size of source and dest character
//  sizes.  WARNING: Your destination buffer MUST be big enough to hold all the
//  source bytes.  We don't validate these ranges here (this should be done in
//  higher level routines).
//


/**
 * This method compares the data in one buffer with another
 * @update	gess 01/04/99
 * @param   aStr1 is the first buffer to be compared
 * @param   aStr2 is the 2nd buffer to be compared
 * @param   aCount is the number of chars to compare
 * @param   aIgnorecase tells us whether to use a case-sensitive comparison
 * @return  -1,0,1 depending on <,==,>
 */
static
#ifdef __SUNPRO_CC
inline
#endif /* __SUNPRO_CC */
PRInt32
Compare1To1(const char* aStr1,const char* aStr2,PRUint32 aCount,PRBool aIgnoreCase){ 
  PRInt32 result=0;
  if(aIgnoreCase)
    result=PRInt32(PL_strncasecmp(aStr1, aStr2, aCount));
  else 
    result=nsCharTraits<char>::compare(aStr1,aStr2,aCount);

      // alien comparisons may return out-of-bound answers
      //  instead of the -1, 0, 1 expected by most clients
  if ( result < -1 )
    result = -1;
  else if ( result > 1 )
    result = 1;
  return result;
}

/**
 * This method compares the data in one buffer with another
 * @update	gess 01/04/99
 * @param   aStr1 is the first buffer to be compared
 * @param   aStr2 is the 2nd buffer to be compared
 * @param   aCount is the number of chars to compare
 * @param   aIgnorecase tells us whether to use a case-sensitive comparison
 * @return  -1,0,1 depending on <,==,>
 */
static 
#ifdef __SUNPRO_CC
inline
#endif /* __SUNPRO_CC */
PRInt32
Compare2To2(const PRUnichar* aStr1,const PRUnichar* aStr2,PRUint32 aCount){
  PRInt32 result;
  
  if ( aStr1 && aStr2 )
    result = nsCharTraits<PRUnichar>::compare(aStr1, aStr2, aCount);

      // The following cases are rare and survivable caller errors.
      //  Two null pointers are equal, but any string, even 0 length
      //  is greater than a null pointer.  It might not really matter,
      //  but we pick something reasonable anyway.
  else if ( !aStr1 && !aStr2 )
    result = 0;
  else if ( aStr1 )
    result = 1;
  else
    result = -1;

      // alien comparisons may give answers outside the -1, 0, 1 expected by callers
  if ( result < -1 )
    result = -1;
  else if ( result > 1 )
    result = 1;
  return result;
}


/**
 * This method compares the data in one buffer with another
 * @update	gess 01/04/99
 * @param   aStr1 is the first buffer to be compared
 * @param   aStr2 is the 2nd buffer to be compared
 * @param   aCount is the number of chars to compare
 * @param   aIgnorecase tells us whether to use a case-sensitive comparison
 * @return  -1,0,1 depending on <,==,>
 */
static
#ifdef __SUNPRO_CC
inline
#endif /* __SUNPRO_CC */
PRInt32
Compare2To1(const PRUnichar* aStr1,const char* aStr2,PRUint32 aCount,PRBool aIgnoreCase){
  const PRUnichar* s1 = aStr1;
  const char *s2 = aStr2;
  
  if (aStr1 && aStr2) {
    if (aCount != 0) {
      do {

        PRUnichar c1 = *s1++;
        PRUnichar c2 = PRUnichar((unsigned char)*s2++);
        
        if (c1 != c2) {
#ifdef NS_DEBUG
          // we won't warn on c1>=128 (the 2-byte value) because often
          // it is just fine to compare an constant, ascii value (i.e. "body")
          // against some non-ascii value (i.e. a unicode string that
          // was downloaded from a web page)
          if (aIgnoreCase && c2>=128)
            NS_WARNING("got a non-ASCII string, but we can't do an accurate case conversion!");
#endif

          // can't do case conversion on characters out of our range
          if (aIgnoreCase && c1<128 && c2<128) {

              c1 = ascii_tolower(char(c1));
              c2 = ascii_tolower(char(c2));
            
              if (c1 == c2) continue;
          }

          if (c1 < c2) return -1;
          return 1;
        }
      } while (--aCount);
    }
  }
  return 0;
}


/**
 * This method compares the data in one buffer with another
 * @update	gess 01/04/99
 * @param   aStr1 is the first buffer to be compared
 * @param   aStr2 is the 2nd buffer to be compared
 * @param   aCount is the number of chars to compare
 * @param   aIgnorecase tells us whether to use a case-sensitive comparison
 * @return  -1,0,1 depending on <,==,>
 */
inline PRInt32
Compare1To2(const char* aStr1,const PRUnichar* aStr2,PRUint32 aCount,PRBool aIgnoreCase){
  return Compare2To1(aStr2, aStr1, aCount, aIgnoreCase) * -1;
}


//-----------------------------------------------------------------------------
//
//  This set of methods is used compress char sequences in a buffer...
//


/**
 * This method compresses duplicate runs of a given char from the given buffer 
 *
 * @update	rickg 03.23.2000
 * @param   aString is the buffer to be manipulated
 * @param   aLength is the length of the buffer
 * @param   aSet tells us which chars to compress from given buffer
 * @param   aEliminateLeading tells us whether to strip chars from the start of the buffer
 * @param   aEliminateTrailing tells us whether to strip chars from the start of the buffer
 * @return  the new length of the given buffer
 */
static PRInt32
CompressChars1(char* aString,PRUint32 aLength,const char* aSet){ 

  char*  from = aString;
  char*  end =  aString + aLength;
  char*  to = from;

    //this code converts /n, /t, /r into normal space ' ';
    //it also compresses runs of whitespace down to a single char...
  if(aSet && aString && (0 < aLength)){
    PRUint32 aSetLen=strlen(aSet);

    while (from < end) {
      char theChar = *from++;
      
      *to++=theChar; //always copy this char...

      if((kNotFound!=FindChar1(aSet,aSetLen,0,theChar,aSetLen))){
        while (from < end) {
          theChar = *from++;
          if(kNotFound==FindChar1(aSet,aSetLen,0,theChar,aSetLen)){
            *to++ = theChar;
            break;
          }
        } //while
      } //if
    } //if
    *to = 0;
  }
  return to - aString;
}



/**
 * This method compresses duplicate runs of a given char from the given buffer 
 *
 * @update	rickg 03.23.2000
 * @param   aString is the buffer to be manipulated
 * @param   aLength is the length of the buffer
 * @param   aSet tells us which chars to compress from given buffer
 * @param   aEliminateLeading tells us whether to strip chars from the start of the buffer
 * @param   aEliminateTrailing tells us whether to strip chars from the start of the buffer
 * @return  the new length of the given buffer
 */
static PRInt32
CompressChars2(PRUnichar* aString,PRUint32 aLength,const char* aSet){ 

  PRUnichar*  from = aString;
  PRUnichar*  end =  from + aLength;
  PRUnichar*  to = from;

    //this code converts /n, /t, /r into normal space ' ';
    //it also compresses runs of whitespace down to a single char...
  if(aSet && aString && (0 < aLength)){
    PRUint32 aSetLen=strlen(aSet);

    while (from < end) {
      PRUnichar theChar = *from++;
      
      *to++=theChar; //always copy this char...

      if((theChar<256) && (kNotFound!=FindChar1(aSet,aSetLen,0,theChar,aSetLen))){
        while (from < end) {
          theChar = *from++;
          if(kNotFound==FindChar1(aSet,aSetLen,0,theChar,aSetLen)){
            *to++ = theChar;
            break;
          }
        } //while
      } //if
    } //if
    *to = 0;
  }
  return to - (PRUnichar*)aString;
}

/**
 * This method strips chars in a given set from the given buffer 
 *
 * @update	gess 01/04/99
 * @param   aString is the buffer to be manipulated
 * @param   aLength is the length of the buffer
 * @param   aSet tells us which chars to compress from given buffer
 * @param   aEliminateLeading tells us whether to strip chars from the start of the buffer
 * @param   aEliminateTrailing tells us whether to strip chars from the start of the buffer
 * @return  the new length of the given buffer
 */
static PRInt32
StripChars1(char* aString,PRUint32 aLength,const char* aSet){ 

  // XXXdarin this code should defer writing until necessary.

  char*  to   = aString;
  char*  from = aString-1;
  char*  end  = aString + aLength;

  if(aSet && aString && (0 < aLength)){
    PRUint32 aSetLen=strlen(aSet);
    while (++from < end) {
      char theChar = *from;
      if(kNotFound==FindChar1(aSet,aSetLen,0,theChar,aSetLen)){
        *to++ = theChar;
      }
    }
    *to = 0;
  }
  return to - (char*)aString;
}


/**
 * This method strips chars in a given set from the given buffer 
 *
 * @update	gess 01/04/99
 * @param   aString is the buffer to be manipulated
 * @param   aLength is the length of the buffer
 * @param   aSet tells us which chars to compress from given buffer
 * @param   aEliminateLeading tells us whether to strip chars from the start of the buffer
 * @param   aEliminateTrailing tells us whether to strip chars from the start of the buffer
 * @return  the new length of the given buffer
 */
static PRInt32
StripChars2(PRUnichar* aString,PRUint32 aLength,const char* aSet){ 

  // XXXdarin this code should defer writing until necessary.

  PRUnichar*  to   = aString;
  PRUnichar*  from = aString-1;
  PRUnichar*  end  = to + aLength;

  if(aSet && aString && (0 < aLength)){
    PRUint32 aSetLen=strlen(aSet);
    while (++from < end) {
      PRUnichar theChar = *from;
      //Note the test for ascii range below. If you have a real unicode char, 
      //and you're searching for chars in the (given) ascii string, there's no
      //point in doing the real search since it's out of the ascii range.
      if((255<theChar) || (kNotFound==FindChar1(aSet,aSetLen,0,theChar,aSetLen))){
        *to++ = theChar;
      }
    }
    *to = 0;
  }
  return to - (PRUnichar*)aString;
}

/* ***** END RICKG BLOCK ***** */

static const char* kWhitespace="\b\t\r\n ";

// This function is used to implement FindCharInSet and friends
template <class CharT>
#ifndef __SUNPRO_CC
static
#endif /* !__SUNPRO_CC */
CharT
GetFindInSetFilter( const CharT* set)
  {
    CharT filter = ~CharT(0); // All bits set
    while (*set) {
      filter &= ~(*set);
      ++set;
    }
    return filter;
  }

// This template class is used by our code to access rickg's buffer routines.
template <class CharT> struct nsBufferRoutines {};

NS_SPECIALIZE_TEMPLATE
struct nsBufferRoutines<char>
  {
    static
    PRInt32 compare( const char* a, const char* b, PRUint32 max, PRBool ic )
      {
        return Compare1To1(a, b, max, ic);
      }

    static
    PRInt32 compare( const char* a, const PRUnichar* b, PRUint32 max, PRBool ic )
      {
        return Compare1To2(a, b, max, ic);
      }

    static
    PRInt32 find_char( const char* s, PRUint32 max, PRInt32 offset, const PRUnichar c, PRInt32 count )
      {
        return FindChar1(s, max, offset, c, count);
      }

    static
    PRInt32 rfind_char( const char* s, PRUint32 max, PRInt32 offset, const PRUnichar c, PRInt32 count )
      {
        return RFindChar1(s, max, offset, c, count);
      }

    static
    char get_find_in_set_filter( const char* set )
      {
        return GetFindInSetFilter(set);
      }

    static
    PRInt32 strip_chars( char* s, PRUint32 len, const char* set )
      {
        return StripChars1(s, len, set);
      }

    static
    PRInt32 compress_chars( char* s, PRUint32 len, const char* set ) 
      {
        return CompressChars1(s, len, set);
      }
  };

NS_SPECIALIZE_TEMPLATE
struct nsBufferRoutines<PRUnichar>
  {
    static
    PRInt32 compare( const PRUnichar* a, const PRUnichar* b, PRUint32 max, PRBool ic )
      {
        NS_ASSERTION(!ic, "no case-insensitive compare here");
        return Compare2To2(a, b, max);
      }

    static
    PRInt32 compare( const PRUnichar* a, const char* b, PRUint32 max, PRBool ic )
      {
        return Compare2To1(a, b, max, ic);
      }

    static
    PRInt32 find_char( const PRUnichar* s, PRUint32 max, PRInt32 offset, const PRUnichar c, PRInt32 count )
      {
        return FindChar2(s, max, offset, c, count);
      }

    static
    PRInt32 rfind_char( const PRUnichar* s, PRUint32 max, PRInt32 offset, const PRUnichar c, PRInt32 count )
      {
        return RFindChar2(s, max, offset, c, count);
      }

    static
    PRUnichar get_find_in_set_filter( const PRUnichar* set )
      {
        return GetFindInSetFilter(set);
      }

    static
    PRUnichar get_find_in_set_filter( const char* set )
      {
        return (~PRUnichar(0)^~char(0)) | GetFindInSetFilter(set);
      }

    static
    PRInt32 strip_chars( PRUnichar* s, PRUint32 max, const char* set )
      {
        return StripChars2(s, max, set);
      }

    static
    PRInt32 compress_chars( PRUnichar* s, PRUint32 len, const char* set ) 
      {
        return CompressChars2(s, len, set);
      }
  };

//-----------------------------------------------------------------------------

template <class L, class R>
#ifndef __SUNPRO_CC
static
#endif /* !__SUNPRO_CC */
PRInt32
FindSubstring( const L* big, PRUint32 bigLen,
               const R* little, PRUint32 littleLen,
               PRBool ignoreCase )
  {
    if (littleLen > bigLen)
      return kNotFound;

    PRInt32 i, max = PRInt32(bigLen - littleLen);
    for (i=0; i<=max; ++i, ++big)
      {
        if (nsBufferRoutines<L>::compare(big, little, littleLen, ignoreCase) == 0)
          return i;
      }

    return kNotFound;
  }

template <class L, class R>
#ifndef __SUNPRO_CC
static
#endif /* !__SUNPRO_CC */
PRInt32
RFindSubstring( const L* big, PRUint32 bigLen,
                const R* little, PRUint32 littleLen,
                PRBool ignoreCase )
  {
    if (littleLen > bigLen)
      return kNotFound;

    PRInt32 i, max = PRInt32(bigLen - littleLen);

    const L* iter = big + max;
    for (i=max; iter >= big; --i, --iter)
      {
        if (nsBufferRoutines<L>::compare(iter, little, littleLen, ignoreCase) == 0)
          return i;
      }

    return kNotFound;
  }

template <class CharT, class SetCharT>
#ifndef __SUNPRO_CC
static
#endif /* !__SUNPRO_CC */
PRInt32
FindCharInSet( const CharT* data, PRUint32 dataLen, const SetCharT* set )
  {
    CharT filter = nsBufferRoutines<CharT>::get_find_in_set_filter(set);

    const CharT* end = data + dataLen; 
    for (const CharT* iter = data; iter < end; ++iter)
      {
        CharT currentChar = *iter;
        if (currentChar & filter)
          continue; // char is not in filter set; go on with next char.

        // test all chars
        const SetCharT* charInSet = set;
        CharT setChar = CharT(*charInSet);
        while (setChar)
          {
            if (setChar == currentChar)
              return iter - data; // found it!  return index of the found char.

            setChar = CharT(*(++charInSet));
          }
      }
    return kNotFound;
  }

template <class CharT, class SetCharT>
#ifndef __SUNPRO_CC
static
#endif /* !__SUNPRO_CC */
PRInt32
RFindCharInSet( const CharT* data, PRUint32 dataLen, const SetCharT* set )
  {
    CharT filter = nsBufferRoutines<CharT>::get_find_in_set_filter(set);

    for (const CharT* iter = data + dataLen - 1; iter >= data; --iter)
      {
        CharT currentChar = *iter;
        if (currentChar & filter)
          continue; // char is not in filter set; go on with next char.

        // test all chars
        const CharT* charInSet = set;
        CharT setChar = *charInSet;
        while (setChar)
          {
            if (setChar == currentChar)
              return iter - data; // found it!  return index of the found char.

            setChar = *(++charInSet);
          }
      }
    return kNotFound;
  }

/**
 * This is a copy of |PR_cnvtf| with a bug fixed.  (The second argument
 * of PR_dtoa is 2 rather than 1.)
 *
 * XXXdarin if this is the right thing, then why wasn't it fixed in NSPR?!?
 */
void 
Modified_cnvtf(char *buf, int bufsz, int prcsn, double fval)
{
  PRIntn decpt, sign, numdigits;
  char *num, *nump;
  char *bufp = buf;
  char *endnum;

  /* If anything fails, we store an empty string in 'buf' */
  num = (char*)malloc(bufsz);
  if (num == NULL) {
    buf[0] = '\0';
    return;
  }
  if (PR_dtoa(fval, 2, prcsn, &decpt, &sign, &endnum, num, bufsz)
      == PR_FAILURE) {
    buf[0] = '\0';
    goto done;
  }
  numdigits = endnum - num;
  nump = num;

  /*
   * The NSPR code had a fancy way of checking that we weren't dealing
   * with -0.0 or -NaN, but I'll just use < instead.
   * XXX Should we check !isnan(fval) as well?  Is it portable?  We
   * probably don't need to bother since NAN isn't portable.
   */
  if (sign && fval < 0.0f) {
    *bufp++ = '-';
  }

  if (decpt == 9999) {
    while ((*bufp++ = *nump++) != 0) {} /* nothing to execute */
    goto done;
  }

  if (decpt > (prcsn+1) || decpt < -(prcsn-1) || decpt < -5) {
    *bufp++ = *nump++;
    if (numdigits != 1) {
      *bufp++ = '.';
    }

    while (*nump != '\0') {
      *bufp++ = *nump++;
    }
    *bufp++ = 'e';
    PR_snprintf(bufp, bufsz - (bufp - buf), "%+d", decpt-1);
  }
  else if (decpt >= 0) {
    if (decpt == 0) {
      *bufp++ = '0';
    }
    else {
      while (decpt--) {
        if (*nump != '\0') {
          *bufp++ = *nump++;
        }
        else {
          *bufp++ = '0';
        }
      }
    }
    if (*nump != '\0') {
      *bufp++ = '.';
      while (*nump != '\0') {
        *bufp++ = *nump++;
      }
    }
    *bufp++ = '\0';
  }
  else if (decpt < 0) {
    *bufp++ = '0';
    *bufp++ = '.';
    while (decpt++) {
      *bufp++ = '0';
    }

    while (*nump != '\0') {
      *bufp++ = *nump++;
    }
    *bufp++ = '\0';
  }
done:
  free(num);
}

  /**
   * this method changes the meaning of |offset| and |count|:
   * 
   * upon return,
   *   |offset| specifies start of search range
   *   |count| specifies length of search range
   */ 
static void
Find_ComputeSearchRange( PRUint32 bigLen, PRUint32 littleLen, PRInt32& offset, PRInt32& count )
  {
    // |count| specifies how many iterations to make from |offset|

    if (offset < 0)
      {
        offset = 0;
      }
    else if (PRUint32(offset) > bigLen)
      {
        count = 0;
        return;
      }

    PRInt32 maxCount = bigLen - offset;
    if (count < 0 || count > maxCount)
      {
        count = maxCount;
      } 
    else
      {
        count += littleLen;
        if (count > maxCount)
          count = maxCount;
      }
  }

  /**
   * this method changes the meaning of |offset| and |count|:
   *
   * upon entry,
   *   |offset| specifies the end point from which to search backwards
   *   |count| specifies the number of iterations from |offset|
   * 
   * upon return,
   *   |offset| specifies start of search range
   *   |count| specifies length of search range
   *
   *
   * EXAMPLE
   * 
   *                            + -- littleLen=4 -- +
   *                            :                   :
   *   |____|____|____|____|____|____|____|____|____|____|____|____|
   *                            :                                  :
   *                         offset=5                           bigLen=12
   *
   *   if count = 4, then we expect this function to return offset = 2 and
   *   count = 7.
   *
   */ 
static void
RFind_ComputeSearchRange( PRUint32 bigLen, PRUint32 littleLen, PRInt32& offset, PRInt32& count )
  {
    if (littleLen > bigLen)
      {
        offset = 0;
        count = 0;
        return;
      }

    if (offset < 0)
      offset = bigLen - littleLen;
    if (count < 0)
      count = offset + 1;

    PRInt32 start = offset - count + 1;
    if (start < 0)
      start = 0;

    count = offset + littleLen - start;
    offset = start;
  }

//-----------------------------------------------------------------------------

  // define nsString obsolete methods
#include "string-template-def-unichar.h"
#include "nsTStringObsolete.cpp"
#include "string-template-undef.h"

  // define nsCString obsolete methods
#include "string-template-def-char.h"
#include "nsTStringObsolete.cpp"
#include "string-template-undef.h"

//-----------------------------------------------------------------------------

// specialized methods:

PRInt32
nsString::Find( const nsAFlatString& aString, PRInt32 aOffset, PRInt32 aCount ) const
  {
    // this method changes the meaning of aOffset and aCount:
    Find_ComputeSearchRange(mLength, aString.Length(), aOffset, aCount);

    PRInt32 result = FindSubstring(mData + aOffset, aCount, aString.get(), aString.Length(), PR_FALSE);
    if (result != kNotFound)
      result += aOffset;
    return result;
  }

PRInt32
nsString::Find( const PRUnichar* aString, PRInt32 aOffset, PRInt32 aCount ) const
  {
    return Find(nsDependentString(aString), aOffset, aCount);
  }

PRInt32
nsString::RFind( const nsAFlatString& aString, PRInt32 aOffset, PRInt32 aCount ) const
  {
    // this method changes the meaning of aOffset and aCount:
    RFind_ComputeSearchRange(mLength, aString.Length(), aOffset, aCount);

    PRInt32 result = RFindSubstring(mData + aOffset, aCount, aString.get(), aString.Length(), PR_FALSE);
    if (result != kNotFound)
      result += aOffset;
    return result;
  }

PRInt32
nsString::RFind( const PRUnichar* aString, PRInt32 aOffset, PRInt32 aCount ) const
  {
    return RFind(nsDependentString(aString), aOffset, aCount);
  }

PRInt32
nsString::FindCharInSet( const PRUnichar* aSet, PRInt32 aOffset ) const
  {
    if (aOffset < 0)
      aOffset = 0;
    else if (aOffset >= PRInt32(mLength))
      return kNotFound;
    
    PRInt32 result = ::FindCharInSet(mData + aOffset, mLength - aOffset, aSet);
    if (result != kNotFound)
      result += aOffset;
    return result;
  }


  /**
   * nsTString::Compare,CompareWithConversion,etc.
   */

PRInt32
nsCString::Compare( const char* aString, PRBool aIgnoreCase, PRInt32 aCount ) const
  {
    PRUint32 strLen = char_traits::length(aString);

    PRInt32 maxCount = PRInt32(NS_MIN(mLength, strLen));

    PRInt32 compareCount;
    if (aCount < 0 || aCount > maxCount)
      compareCount = maxCount;
    else
      compareCount = aCount;

    PRInt32 result =
        nsBufferRoutines<char>::compare(mData, aString, compareCount, aIgnoreCase);

    if (result == 0 &&
          (aCount < 0 || strLen < PRUint32(aCount) || mLength < PRUint32(aCount)))
      {
        // Since the caller didn't give us a length to test, or strings shorter
        // than aCount, and compareCount characters matched, we have to assume
        // that the longer string is greater.

        if (mLength != strLen)
          result = (mLength < strLen) ? -1 : 1;
      }
    return result;
  }

PRBool
nsString::EqualsIgnoreCase( const char* aString, PRInt32 aCount ) const
  {
    PRUint32 strLen = nsCharTraits<char>::length(aString);

    PRInt32 maxCount = PRInt32(NS_MIN(mLength, strLen));

    PRInt32 compareCount;
    if (aCount < 0 || aCount > maxCount)
      compareCount = maxCount;
    else
      compareCount = aCount;

    PRInt32 result =
        nsBufferRoutines<PRUnichar>::compare(mData, aString, compareCount, PR_TRUE);

    if (result == 0 &&
          (aCount < 0 || strLen < PRUint32(aCount) || mLength < PRUint32(aCount)))
      {
        // Since the caller didn't give us a length to test, or strings shorter
        // than aCount, and compareCount characters matched, we have to assume
        // that the longer string is greater.

        if (mLength != strLen)
          result = 1; // Arbitrarily using any number != 0
      }
    return result == 0;
  }

  /**
   * ToCString, ToFloat, ToInteger
   */

char*
nsString::ToCString( char* aBuf, PRUint32 aBufLength, PRUint32 aOffset ) const
  {
      // because the old implementation checked aBuf
    if (!(aBuf && aBufLength > 0 && aOffset <= mLength))
      return nsnull;

    PRUint32 maxCount = NS_MIN(aBufLength-1, mLength - aOffset);

    LossyConvertEncoding<PRUnichar, char> converter(aBuf);
    converter.write(mData + aOffset, maxCount);
    converter.write_terminator();
    return aBuf;
  }

float
nsCString::ToFloat(PRInt32* aErrorCode) const
  {
    float res = 0.0f;
    if (mLength > 0)
      {
        char *conv_stopped;
        const char *str = mData;
        // Use PR_strtod, not strtod, since we don't want locale involved.
        res = (float)PR_strtod(str, &conv_stopped);
        if (conv_stopped == str+mLength)
          *aErrorCode = (PRInt32) NS_OK;
        else // Not all the string was scanned
          *aErrorCode = (PRInt32) NS_ERROR_ILLEGAL_VALUE;
      }
    else
      {
        // The string was too short (0 characters)
        *aErrorCode = (PRInt32) NS_ERROR_ILLEGAL_VALUE;
      }
    return res;
  }

float
nsString::ToFloat(PRInt32* aErrorCode) const
  {
    float res = 0.0f;
    char buf[100];
    if (mLength > 0 && mLength < sizeof(buf))
      {
        char *conv_stopped;
        const char *str = ToCString(buf, sizeof(buf));
        // Use PR_strtod, not strtod, since we don't want locale involved.
        res = (float)PR_strtod(str, &conv_stopped);
        if (conv_stopped == str+mLength)
          *aErrorCode = (PRInt32) NS_OK;
        else // Not all the string was scanned
          *aErrorCode = (PRInt32) NS_ERROR_ILLEGAL_VALUE;
      }
    else
      {
        // The string was too short (0 characters) or too long (sizeof(buf))
        *aErrorCode = (PRInt32) NS_ERROR_ILLEGAL_VALUE;
      }
    return res;
  }


  /**
   * nsTString::AssignWithConversion
   */

void
nsCString::AssignWithConversion( const nsAString& aData )
  {
    LossyCopyUTF16toASCII(aData, *this);
  }

void
nsString::AssignWithConversion( const nsACString& aData )
  {
    CopyASCIItoUTF16(aData, *this);
  }


  /**
   * nsTString::AppendWithConversion
   */

void
nsCString::AppendWithConversion( const nsAString& aData )
  {
    LossyAppendUTF16toASCII(aData, *this);
  }

void
nsString::AppendWithConversion( const nsACString& aData )
  {
    AppendASCIItoUTF16(aData, *this);
  }


  /**
   * nsTString::AppendInt
   */

void
nsCString::AppendInt( PRInt32 aInteger, PRInt32 aRadix )
  {
    char buf[20];
    const char* fmt;
    switch (aRadix) {
      case 8:
        fmt = "%o";
        break;
      case 10:
        fmt = "%d";
        break;
      default:
        NS_ASSERTION(aRadix == 16, "Invalid radix!");
        fmt = "%x";
    }
    PR_snprintf(buf, sizeof(buf), fmt, aInteger);
    Append(buf);
  }

void
nsString::AppendInt( PRInt32 aInteger, PRInt32 aRadix )
  {
    char buf[20];
    const char* fmt;
    switch (aRadix) {
      case 8:
        fmt = "%o";
        break;
      case 10:
        fmt = "%d";
        break;
      default:
        NS_ASSERTION(aRadix == 16, "Invalid radix!");
        fmt = "%x";
    }
    PR_snprintf(buf, sizeof(buf), fmt, aInteger);
    AppendASCIItoUTF16(buf, *this);
  }

void
nsCString::AppendInt( PRInt64 aInteger, PRInt32 aRadix )
  {
    char buf[30];
    const char* fmt;
    switch (aRadix) {
      case 8:
        fmt = "%llo";
        break;
      case 10:
        fmt = "%lld";
        break;
      default:
        NS_ASSERTION(aRadix == 16, "Invalid radix!");
        fmt = "%llx";
    }
    PR_snprintf(buf, sizeof(buf), fmt, aInteger);
    Append(buf);
  }

void
nsString::AppendInt( PRInt64 aInteger, PRInt32 aRadix )
  {
    char buf[30];
    const char* fmt;
    switch (aRadix) {
      case 8:
        fmt = "%llo";
        break;
      case 10:
        fmt = "%lld";
        break;
      default:
        NS_ASSERTION(aRadix == 16, "Invalid radix!");
        fmt = "%llx";
    }
    PR_snprintf(buf, sizeof(buf), fmt, aInteger);
    AppendASCIItoUTF16(buf, *this);
  }

  /**
   * nsTString::AppendFloat
   */

void
nsCString::AppendFloat( double aFloat )
  {
    char buf[40];
    // Use Modified_cnvtf, which is locale-insensitive, instead of the
    // locale-sensitive PR_snprintf or sprintf(3)
    Modified_cnvtf(buf, sizeof(buf), 6, aFloat);
    Append(buf);
  }

void
nsString::AppendFloat( double aFloat )
  {
    char buf[40];
    // Use Modified_cnvtf, which is locale-insensitive, instead of the
    // locale-sensitive PR_snprintf or sprintf(3)
    Modified_cnvtf(buf, sizeof(buf), 6, aFloat);
    AppendWithConversion(buf);
  }

#endif // !MOZ_STRING_WITH_OBSOLETE_API
