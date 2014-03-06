/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

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

/* ***** BEGIN RICKG BLOCK *****
 *
 * NOTE: This section of code was extracted from rickg's bufferRoutines.h file.
 *       For the most part it remains unmodified.  We want to eliminate (or at
 *       least clean up) this code at some point.  If you find the formatting
 *       in this section somewhat inconsistent, don't blame me! ;-)
 */

// avoid STDC's tolower since it may do weird things with non-ASCII bytes
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
static int32_t
FindChar1(const char* aDest,uint32_t aDestLength,int32_t anOffset,const char16_t aChar,int32_t aCount) {

  if(anOffset < 0)
    anOffset=0;

  if(aCount < 0)
    aCount = (int32_t)aDestLength;

  if((aChar < 256) && (0 < aDestLength) && ((uint32_t)anOffset < aDestLength)) {

    //We'll only search if the given aChar is within the normal ascii a range,
    //(Since this string is definitely within the ascii range).
    
    if(0<aCount) {

      const char* left= aDest+anOffset;
      const char* last= left+aCount;
      const char* max = aDest+aDestLength;
      const char* end = (last<max) ? last : max;

      int32_t theMax = end-left;
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
static int32_t
FindChar2(const char16_t* aDest,uint32_t aDestLength,int32_t anOffset,const char16_t aChar,int32_t aCount) {

  if(anOffset < 0)
    anOffset=0;

  if(aCount < 0)
    aCount = (int32_t)aDestLength;

  if((0<aDestLength) && ((uint32_t)anOffset < aDestLength)) {
 
    if(0<aCount) {

      const char16_t* root = aDest;
      const char16_t* left = root+anOffset;
      const char16_t* last = left+aCount;
      const char16_t* max  = root+aDestLength;
      const char16_t* end  = (last<max) ? last : max;

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

static int32_t
RFindChar1(const char* aDest,uint32_t aDestLength,int32_t anOffset,const char16_t aChar,int32_t aCount) {

  if(anOffset < 0)
    anOffset=(int32_t)aDestLength-1;

  if(aCount < 0)
    aCount = int32_t(aDestLength);

  if((aChar<256) && (0 < aDestLength) && ((uint32_t)anOffset < aDestLength)) {

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
static int32_t
RFindChar2(const char16_t* aDest,uint32_t aDestLength,int32_t anOffset,const char16_t aChar,int32_t aCount) {

  if(anOffset < 0)
    anOffset=(int32_t)aDestLength-1;

  if(aCount < 0)
    aCount = int32_t(aDestLength);

  if((0 < aDestLength) && ((uint32_t)anOffset < aDestLength)) {
 
    if(0 < aCount) {

      const char16_t* root      = aDest;
      const char16_t* rightmost = root + anOffset;  
      const char16_t* min       = rightmost - aCount + 1;
      const char16_t* leftmost  = (min<root) ? root: min;
      
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
 * @param   aIgnoreCase tells us whether to use a case-sensitive comparison
 * @return  -1,0,1 depending on <,==,>
 */
static
#ifdef __SUNPRO_CC
inline
#endif /* __SUNPRO_CC */
int32_t
Compare1To1(const char* aStr1,const char* aStr2,uint32_t aCount,bool aIgnoreCase){ 
  int32_t result=0;
  if(aIgnoreCase)
    result=int32_t(PL_strncasecmp(aStr1, aStr2, aCount));
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
 * @param   aIgnoreCase tells us whether to use a case-sensitive comparison
 * @return  -1,0,1 depending on <,==,>
 */
static 
#ifdef __SUNPRO_CC
inline
#endif /* __SUNPRO_CC */
int32_t
Compare2To2(const char16_t* aStr1,const char16_t* aStr2,uint32_t aCount){
  int32_t result;
  
  if ( aStr1 && aStr2 )
    result = nsCharTraits<char16_t>::compare(aStr1, aStr2, aCount);

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
 * @param   aIgnoreCase tells us whether to use a case-sensitive comparison
 * @return  -1,0,1 depending on <,==,>
 */
static
#ifdef __SUNPRO_CC
inline
#endif /* __SUNPRO_CC */
int32_t
Compare2To1(const char16_t* aStr1,const char* aStr2,uint32_t aCount,bool aIgnoreCase){
  const char16_t* s1 = aStr1;
  const char *s2 = aStr2;
  
  if (aStr1 && aStr2) {
    if (aCount != 0) {
      do {

        char16_t c1 = *s1++;
        char16_t c2 = char16_t((unsigned char)*s2++);
        
        if (c1 != c2) {
#ifdef DEBUG
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
 * @param   aIgnoreCase tells us whether to use a case-sensitive comparison
 * @return  -1,0,1 depending on <,==,>
 */
inline int32_t
Compare1To2(const char* aStr1,const char16_t* aStr2,uint32_t aCount,bool aIgnoreCase){
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
static int32_t
CompressChars1(char* aString,uint32_t aLength,const char* aSet){ 

  char*  from = aString;
  char*  end =  aString + aLength;
  char*  to = from;

    //this code converts /n, /t, /r into normal space ' ';
    //it also compresses runs of whitespace down to a single char...
  if(aSet && aString && (0 < aLength)){
    uint32_t aSetLen=strlen(aSet);

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
static int32_t
CompressChars2(char16_t* aString,uint32_t aLength,const char* aSet){ 

  char16_t*  from = aString;
  char16_t*  end =  from + aLength;
  char16_t*  to = from;

    //this code converts /n, /t, /r into normal space ' ';
    //it also compresses runs of whitespace down to a single char...
  if(aSet && aString && (0 < aLength)){
    uint32_t aSetLen=strlen(aSet);

    while (from < end) {
      char16_t theChar = *from++;
      
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
  return to - (char16_t*)aString;
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
static int32_t
StripChars1(char* aString,uint32_t aLength,const char* aSet){ 

  // XXX(darin): this code should defer writing until necessary.

  char*  to   = aString;
  char*  from = aString-1;
  char*  end  = aString + aLength;

  if(aSet && aString && (0 < aLength)){
    uint32_t aSetLen=strlen(aSet);
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
static int32_t
StripChars2(char16_t* aString,uint32_t aLength,const char* aSet){ 

  // XXX(darin): this code should defer writing until necessary.

  char16_t*  to   = aString;
  char16_t*  from = aString-1;
  char16_t*  end  = to + aLength;

  if(aSet && aString && (0 < aLength)){
    uint32_t aSetLen=strlen(aSet);
    while (++from < end) {
      char16_t theChar = *from;
      //Note the test for ascii range below. If you have a real unicode char, 
      //and you're searching for chars in the (given) ascii string, there's no
      //point in doing the real search since it's out of the ascii range.
      if((255<theChar) || (kNotFound==FindChar1(aSet,aSetLen,0,theChar,aSetLen))){
        *to++ = theChar;
      }
    }
    *to = 0;
  }
  return to - (char16_t*)aString;
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

template <>
struct nsBufferRoutines<char>
  {
    static
    int32_t compare( const char* a, const char* b, uint32_t max, bool ic )
      {
        return Compare1To1(a, b, max, ic);
      }

    static
    int32_t compare( const char* a, const char16_t* b, uint32_t max, bool ic )
      {
        return Compare1To2(a, b, max, ic);
      }

    static
    int32_t find_char( const char* s, uint32_t max, int32_t offset, const char16_t c, int32_t count )
      {
        return FindChar1(s, max, offset, c, count);
      }

    static
    int32_t rfind_char( const char* s, uint32_t max, int32_t offset, const char16_t c, int32_t count )
      {
        return RFindChar1(s, max, offset, c, count);
      }

    static
    char get_find_in_set_filter( const char* set )
      {
        return GetFindInSetFilter(set);
      }

    static
    int32_t strip_chars( char* s, uint32_t len, const char* set )
      {
        return StripChars1(s, len, set);
      }

    static
    int32_t compress_chars( char* s, uint32_t len, const char* set ) 
      {
        return CompressChars1(s, len, set);
      }
  };

template <>
struct nsBufferRoutines<char16_t>
  {
    static
    int32_t compare( const char16_t* a, const char16_t* b, uint32_t max, bool ic )
      {
        NS_ASSERTION(!ic, "no case-insensitive compare here");
        return Compare2To2(a, b, max);
      }

    static
    int32_t compare( const char16_t* a, const char* b, uint32_t max, bool ic )
      {
        return Compare2To1(a, b, max, ic);
      }

    static
    int32_t find_char( const char16_t* s, uint32_t max, int32_t offset, const char16_t c, int32_t count )
      {
        return FindChar2(s, max, offset, c, count);
      }

    static
    int32_t rfind_char( const char16_t* s, uint32_t max, int32_t offset, const char16_t c, int32_t count )
      {
        return RFindChar2(s, max, offset, c, count);
      }

    static
    char16_t get_find_in_set_filter( const char16_t* set )
      {
        return GetFindInSetFilter(set);
      }

    static
    char16_t get_find_in_set_filter( const char* set )
      {
        return (~char16_t(0)^~char(0)) | GetFindInSetFilter(set);
      }

    static
    int32_t strip_chars( char16_t* s, uint32_t max, const char* set )
      {
        return StripChars2(s, max, set);
      }

    static
    int32_t compress_chars( char16_t* s, uint32_t len, const char* set ) 
      {
        return CompressChars2(s, len, set);
      }
  };

//-----------------------------------------------------------------------------

template <class L, class R>
#ifndef __SUNPRO_CC
static
#endif /* !__SUNPRO_CC */
int32_t
FindSubstring( const L* big, uint32_t bigLen,
               const R* little, uint32_t littleLen,
               bool ignoreCase )
  {
    if (littleLen > bigLen)
      return kNotFound;

    int32_t i, max = int32_t(bigLen - littleLen);
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
int32_t
RFindSubstring( const L* big, uint32_t bigLen,
                const R* little, uint32_t littleLen,
                bool ignoreCase )
  {
    if (littleLen > bigLen)
      return kNotFound;

    int32_t i, max = int32_t(bigLen - littleLen);

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
int32_t
FindCharInSet( const CharT* data, uint32_t dataLen, const SetCharT* set )
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
int32_t
RFindCharInSet( const CharT* data, uint32_t dataLen, const SetCharT* set )
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
   * this method changes the meaning of |offset| and |count|:
   * 
   * upon return,
   *   |offset| specifies start of search range
   *   |count| specifies length of search range
   */ 
static void
Find_ComputeSearchRange( uint32_t bigLen, uint32_t littleLen, int32_t& offset, int32_t& count )
  {
    // |count| specifies how many iterations to make from |offset|

    if (offset < 0)
      {
        offset = 0;
      }
    else if (uint32_t(offset) > bigLen)
      {
        count = 0;
        return;
      }

    int32_t maxCount = bigLen - offset;
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
RFind_ComputeSearchRange( uint32_t bigLen, uint32_t littleLen, int32_t& offset, int32_t& count )
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

    int32_t start = offset - count + 1;
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

int32_t
nsString::Find( const nsAFlatString& aString, int32_t aOffset, int32_t aCount ) const
  {
    // this method changes the meaning of aOffset and aCount:
    Find_ComputeSearchRange(mLength, aString.Length(), aOffset, aCount);

    int32_t result = FindSubstring(mData + aOffset, aCount, static_cast<const char16_t*>(aString.get()), aString.Length(), false);
    if (result != kNotFound)
      result += aOffset;
    return result;
  }

int32_t
nsString::Find( const char16_t* aString, int32_t aOffset, int32_t aCount ) const
  {
    return Find(nsDependentString(aString), aOffset, aCount);
  }

int32_t
nsString::RFind( const nsAFlatString& aString, int32_t aOffset, int32_t aCount ) const
  {
    // this method changes the meaning of aOffset and aCount:
    RFind_ComputeSearchRange(mLength, aString.Length(), aOffset, aCount);

    int32_t result = RFindSubstring(mData + aOffset, aCount, static_cast<const char16_t*>(aString.get()), aString.Length(), false);
    if (result != kNotFound)
      result += aOffset;
    return result;
  }

int32_t
nsString::RFind( const char16_t* aString, int32_t aOffset, int32_t aCount ) const
  {
    return RFind(nsDependentString(aString), aOffset, aCount);
  }

int32_t
nsString::FindCharInSet( const char16_t* aSet, int32_t aOffset ) const
  {
    if (aOffset < 0)
      aOffset = 0;
    else if (aOffset >= int32_t(mLength))
      return kNotFound;
    
    int32_t result = ::FindCharInSet(mData + aOffset, mLength - aOffset, aSet);
    if (result != kNotFound)
      result += aOffset;
    return result;
  }

void
nsString::ReplaceChar( const char16_t* aSet, char16_t aNewChar )
  {
    if (!EnsureMutable()) // XXX do this lazily?
      NS_ABORT_OOM(mLength);

    char16_t* data = mData;
    uint32_t lenRemaining = mLength;

    while (lenRemaining)
      {
        int32_t i = ::FindCharInSet(data, lenRemaining, aSet);
        if (i == kNotFound)
          break;

        data[i++] = aNewChar;
        data += i;
        lenRemaining -= i;
      }
  }


  /**
   * nsTString::Compare,CompareWithConversion,etc.
   */

int32_t
nsCString::Compare( const char* aString, bool aIgnoreCase, int32_t aCount ) const
  {
    uint32_t strLen = char_traits::length(aString);

    int32_t maxCount = int32_t(XPCOM_MIN(mLength, strLen));

    int32_t compareCount;
    if (aCount < 0 || aCount > maxCount)
      compareCount = maxCount;
    else
      compareCount = aCount;

    int32_t result =
        nsBufferRoutines<char>::compare(mData, aString, compareCount, aIgnoreCase);

    if (result == 0 &&
          (aCount < 0 || strLen < uint32_t(aCount) || mLength < uint32_t(aCount)))
      {
        // Since the caller didn't give us a length to test, or strings shorter
        // than aCount, and compareCount characters matched, we have to assume
        // that the longer string is greater.

        if (mLength != strLen)
          result = (mLength < strLen) ? -1 : 1;
      }
    return result;
  }

bool
nsString::EqualsIgnoreCase( const char* aString, int32_t aCount ) const
  {
    uint32_t strLen = nsCharTraits<char>::length(aString);

    int32_t maxCount = int32_t(XPCOM_MIN(mLength, strLen));

    int32_t compareCount;
    if (aCount < 0 || aCount > maxCount)
      compareCount = maxCount;
    else
      compareCount = aCount;

    int32_t result =
        nsBufferRoutines<char16_t>::compare(mData, aString, compareCount, true);

    if (result == 0 &&
          (aCount < 0 || strLen < uint32_t(aCount) || mLength < uint32_t(aCount)))
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
   * nsTString::ToDouble
   */

double
nsCString::ToDouble(nsresult* aErrorCode) const
  {
    double res = 0.0;
    if (mLength > 0)
      {
        char *conv_stopped;
        const char *str = mData;
        // Use PR_strtod, not strtod, since we don't want locale involved.
        res = PR_strtod(str, &conv_stopped);
        if (conv_stopped == str+mLength)
          *aErrorCode = NS_OK;
        else // Not all the string was scanned
          *aErrorCode = NS_ERROR_ILLEGAL_VALUE;
      }
    else
      {
        // The string was too short (0 characters)
        *aErrorCode = NS_ERROR_ILLEGAL_VALUE;
      }
    return res;
  }

double
nsString::ToDouble(nsresult* aErrorCode) const
  {
    return NS_LossyConvertUTF16toASCII(*this).ToDouble(aErrorCode);
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

#endif // !MOZ_STRING_WITH_OBSOLETE_API
