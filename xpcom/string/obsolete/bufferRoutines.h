/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: NPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.1 (the "License"); you may not use this file except in
 * compliance with the License. You may obtain a copy of the License at
 * http://www.mozilla.org/NPL/
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
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Pierre Phaneuf <pp@ludusdesign.com>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or 
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the NPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the NPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

/* bufferRoutines.h --- rickg's original string manipulation underpinnings;
    this code will be made obsolete by the new shared-buffer string (see bug #53065)
 */

#ifndef _BUFFERROUTINES_H
#define _BUFFERROUTINES_H

#ifndef nsStringDefines_h___
#include "nsStringDefines.h"
#endif

#ifndef nsCharTraits_h___
#include "nsCharTraits.h"
#endif

#include "nspr.h"
#include "plstr.h"
#include <ctype.h>

/******************************************************************************************
  MODULE NOTES:

  This file contains the workhorse copy and shift functions used in nsStrStruct.
  Ultimately, I plan to make the function pointers in this system available for 
  use by external modules. They'll be able to install their own "handlers".
  Not so, today though.

*******************************************************************************************/
#define KSHIFTLEFT  (0)
#define KSHIFTRIGHT (1)

// uncomment the following line to caught nsString char* casting problem
//#define DEBUG_ILLEGAL_CAST_UP
//#define DEBUG_ILLEGAL_CAST_DOWN

#if  defined(DEBUG_ILLEGAL_CAST_UP) || defined(DEBUG_ILLEGAL_CAST_DOWN) 

static PRBool track_illegal = PR_TRUE;
static PRBool track_latin1  = PR_TRUE;

#ifdef XP_UNIX
#include "nsTraceRefcntImpl.h"
class CTraceFile {
public:
   CTraceFile() {
      mFile = fopen("nsStringTrace.txt" , "a+");
   }
   ~CTraceFile() {
      fflush(mFile);
      fclose(mFile);
   }
   void ReportCastUp(const char* data, const char* msg)
   {
      if(mFile) {
        fprintf(mFile, "ERRORTEXT= %s\n", msg);
        fprintf(mFile, "BEGINDATA\n");
        const char* s=data;
        while(*s) {
           if(*s & 0x80) {
             fprintf(mFile, "[%2X]", (char)*s);
	   } else {
             fprintf(mFile, "%c", *s);
           }
           s++;
        }
        fprintf(mFile, "\n");
        fprintf(mFile, "ENDDATA\n");
        fprintf(mFile, "BEGINSTACK\n");
        nsTraceRefcntImpl::WalkTheStack(mFile);
        fprintf(mFile, "\n");
        fprintf(mFile, "ENDSTACK\n");
        fflush(mFile);
      }
   }
   void ReportCastDown(const PRUnichar* data, const char* msg)
   {
      if(mFile) {
        fprintf(mFile, "ERRORTEXT=%s\n", msg);
        fprintf(mFile, "BEGINDATA\n");
        const PRUnichar* s=data;
        while(*s) {
           if(*s & 0xFF80) {
             fprintf(mFile, "\\u%X", *s);
	   } else {
             fprintf(mFile, "%c", *s);
           }
           s++;
        }
        fprintf(mFile, "\n");
        fprintf(mFile, "ENDDATA\n");
        fprintf(mFile, "BEGINSTACK\n");
        nsTraceRefcntImpl::WalkTheStack(mFile);
        fprintf(mFile, "\n");
        fprintf(mFile, "ENDSTACK\n");
        fflush(mFile);
      }
   }
private:
   FILE* mFile;
};
static CTraceFile gTrace;
#define  TRACE_ILLEGAL_CAST_UP(c, s, m)     if(!(c)) gTrace.ReportCastUp(s,m);
#define  TRACE_ILLEGAL_CAST_DOWN(c, s, m)   if(!(c)) gTrace.ReportCastDown(s,m);

#else // XP_UNIX

#define  TRACE_ILLEGAL_CAST_UP(c, s, m)   NS_ASSERTION((c), (m))
#define  TRACE_ILLEGAL_CAST_DOWN(c, s, m) NS_ASSERTION((c), (m))

#endif //XP_UNIX

#endif

inline char ascii_tolower(char aChar)
{
  if (aChar >= 'A' && aChar <= 'Z')
    return aChar + ('a' - 'A');
  return aChar;
}

inline PRUnichar GetCharAt(const char* aString,PRUint32 anIndex) {
  return (PRUnichar)aString[anIndex];
}

//----------------------------------------------------------------------------------------
//
//  This set of methods is used to shift the contents of a char buffer.
//  The functions are differentiated by shift direction and the underlying charsize.
//

/**
 * This method shifts single byte characters left by a given amount from an given offset.
 * @update	gess 01/04/99
 * @param   aDest is a ptr to a cstring where left-shift is to be performed
 * @param   aLength is the known length of aDest
 * @param   anOffset is the index into aDest where shifting shall begin
 * @param   aCount is the number of chars to be "cut"
 */
void ShiftCharsLeft(char* aDest,PRUint32 aLength,PRUint32 anOffset,PRUint32 aCount);
void ShiftCharsLeft(char* aDest,PRUint32 aLength,PRUint32 anOffset,PRUint32 aCount) { 
  char*   dst = aDest+anOffset;
  char*   src = aDest+anOffset+aCount;

  memmove(dst,src,aLength-(aCount+anOffset));
}

/**
 * This method shifts single byte characters right by a given amount from an given offset.
 * @update	gess 01/04/99
 * @param   aDest is a ptr to a cstring where the shift is to be performed
 * @param   aLength is the known length of aDest
 * @param   anOffset is the index into aDest where shifting shall begin
 * @param   aCount is the number of chars to be "inserted"
 */
void ShiftCharsRight(char* aDest,PRUint32 aLength,PRUint32 anOffset,PRUint32 aCount);
void ShiftCharsRight(char* aDest,PRUint32 aLength,PRUint32 anOffset,PRUint32 aCount) { 
  char* src = aDest+anOffset;
  char* dst = aDest+anOffset+aCount;

  memmove(dst,src,aLength-anOffset);
}

/**
 * This method shifts unicode characters by a given amount from an given offset.
 * @update	gess 01/04/99
 * @param   aDest is a ptr to a cstring where the shift is to be performed
 * @param   aLength is the known length of aDest
 * @param   anOffset is the index into aDest where shifting shall begin
 * @param   aCount is the number of chars to be "cut"
 */
void ShiftDoubleCharsLeft(PRUnichar* aDest,PRUint32 aLength,PRUint32 anOffset,PRUint32 aCount);
void ShiftDoubleCharsLeft(PRUnichar* aDest,PRUint32 aLength,PRUint32 anOffset,PRUint32 aCount) { 
  PRUnichar* root= aDest;
  PRUnichar* dst = root+anOffset;
  PRUnichar* src = root+anOffset+aCount;

  memmove(dst,src,(aLength-(aCount+anOffset))*sizeof(PRUnichar));
}


/**
 * This method shifts unicode characters by a given amount from an given offset.
 * @update	gess 01/04/99
 * @param   aDest is a ptr to a cstring where the shift is to be performed
 * @param   aLength is the known length of aDest
 * @param   anOffset is the index into aDest where shifting shall begin
 * @param   aCount is the number of chars to be "inserted"
 */
void ShiftDoubleCharsRight(PRUnichar* aDest,PRUint32 aLength,PRUint32 anOffset,PRUint32 aCount);
void ShiftDoubleCharsRight(PRUnichar* aDest,PRUint32 aLength,PRUint32 anOffset,PRUint32 aCount) { 
  PRUnichar* root= aDest;
  PRUnichar* src = root+anOffset;
  PRUnichar* dst = root+anOffset+aCount;

  memmove(dst,src,sizeof(PRUnichar)*(aLength-anOffset));
}

//----------------------------------------------------------------------------------------
//
//  This set of methods is used to copy one buffer onto another.
//  The functions are differentiated by the size of source and dest character sizes.
//  WARNING: Your destination buffer MUST be big enough to hold all the source bytes.
//           We don't validate these ranges here (this should be done in higher level routines).
//


/**
 * Going 1 to 1 is easy, since we assume ascii. No conversions are necessary.
 * @update	gess 01/04/99
 * @param aDest is the destination buffer
 * @param aDestOffset is the pos to start copy to in the dest buffer
 * @param aSource is the source buffer
 * @param anOffset is the offset to start copying from in the source buffer
 * @param aCount is the (max) number of chars to copy
 */
void CopyChars1To1(char* aDest,PRInt32 anDestOffset,const char* aSource,PRUint32 anOffset,PRUint32 aCount);
void CopyChars1To1(char* aDest,PRInt32 anDestOffset,const char* aSource,PRUint32 anOffset,PRUint32 aCount) { 

  char* dst = aDest+anDestOffset;
  char* src = (char*)aSource+anOffset;

  memcpy(dst,src,aCount);
} 

/**
 * Going 1 to 2 requires a conversion from ascii to unicode. This can be expensive.
 * @param aDest is the destination buffer
 * @param aDestOffset is the pos to start copy to in the dest buffer
 * @param aSource is the source buffer
 * @param anOffset is the offset to start copying from in the source buffer
 * @param aCount is the (max) number of chars to copy
 */
void CopyChars1To2(char* aDest,PRInt32 anDestOffset,const char* aSource,PRUint32 anOffset,PRUint32 aCount);
void CopyChars1To2(char* aDest,PRInt32 anDestOffset,const char* aSource,PRUint32 anOffset,PRUint32 aCount) { 

  PRUnichar* theDest=(PRUnichar*)aDest;
  PRUnichar* to   = theDest+anDestOffset;
  const unsigned char* first= (const unsigned char*)aSource+anOffset;
  const unsigned char* last = first+aCount;

#ifdef DEBUG_ILLEGAL_CAST_UP
  PRBool illegal= PR_FALSE;
#endif
  //now loop over characters, shifting them left...
  while(first < last) {
    *to=(PRUnichar)(*first);  
#ifdef DEBUG_ILLEGAL_CAST_UP
    if(track_illegal && track_latin1 && ((*to)& 0x80))
      illegal= PR_TRUE;
#endif
    to++;
    first++;
  }
#ifdef DEBUG_ILLEGAL_CAST_UP
  TRACE_ILLEGAL_CAST_UP((!illegal), aSource, "illegal cast up in CopyChars1To2");
#endif

}
 

/**
 * Going 2 to 1 requires a conversion from unicode down to ascii. This can be lossy.
 * @update	gess 01/04/99
 * @param aDest is the destination buffer
 * @param aDestOffset is the pos to start copy to in the dest buffer
 * @param aSource is the source buffer
 * @param anOffset is the offset to start copying from in the source buffer
 * @param aCount is the (max) number of chars to copy
 */
void CopyChars2To1(char* aDest,PRInt32 anDestOffset,const char* aSource,PRUint32 anOffset,PRUint32 aCount);
void CopyChars2To1(char* aDest,PRInt32 anDestOffset,const char* aSource,PRUint32 anOffset,PRUint32 aCount) { 
  char*             to   = aDest+anDestOffset;
  PRUnichar*        theSource=(PRUnichar*)aSource;
  const PRUnichar*  first= theSource+anOffset;
  const PRUnichar*  last = first+aCount;

#ifdef DEBUG_ILLEGAL_CAST_DOWN
  PRBool illegal= PR_FALSE;
#endif
  //now loop over characters, shifting them left...
  while(first < last) {
    if(*first < 256)
      *to=(char)*first;  
    else { 
      *to='.';
      NS_ASSERTION( (*first < 256), "data in U+0100-U+FFFF will be lost");
    }
#ifdef DEBUG_ILLEGAL_CAST_DOWN
    if(track_illegal) {
      if(track_latin1) {
        if(*first & 0xFF80)
          illegal = PR_TRUE;
      } else {
        if(*first & 0xFF00)
          illegal = PR_TRUE;
      } // track_latin1
    } // track_illegal
#endif
    to++;
    first++;
  }
#ifdef DEBUG_ILLEGAL_CAST_DOWN
  TRACE_ILLEGAL_CAST_DOWN((!illegal), theSource, "illegal cast down in CopyChars2To1");
#endif
}

/**
 * Going 2 to 2 is fast and efficient.
 * @update	gess 01/04/99
 * @param aDest is the destination buffer
 * @param aDestOffset is the pos to start copy to in the dest buffer
 * @param aSource is the source buffer
 * @param anOffset is the offset to start copying from in the source buffer
 * @param aCount is the (max) number of chars to copy
 */
void CopyChars2To2(char* aDest,PRInt32 anDestOffset,const char* aSource,PRUint32 anOffset,PRUint32 aCount);
void CopyChars2To2(char* aDest,PRInt32 anDestOffset,const char* aSource,PRUint32 anOffset,PRUint32 aCount) { 
  PRUnichar* theDest=(PRUnichar*)aDest;
  PRUnichar* to   = theDest+anDestOffset;
  PRUnichar* theSource=(PRUnichar*)aSource;
  PRUnichar* from= theSource+anOffset;

  if(aCount == 1)
    *to = *from;
  else
    memcpy((void*)to,(void*)from,aCount*sizeof(PRUnichar));
}


//--------------------------------------------------------------------------------------


typedef void (*CopyChars)(char* aDest,PRInt32 anDestOffset,const char* aSource,PRUint32 anOffset,PRUint32 aCount);

CopyChars gCopyChars[2][2]={
  {&CopyChars1To1,&CopyChars1To2},
  {&CopyChars2To1,&CopyChars2To2}
};


//----------------------------------------------------------------------------------------
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
inline PRInt32 FindChar1(const char* aDest,PRUint32 aDestLength,PRInt32 anOffset,const PRUnichar aChar,PRInt32 aCount) {

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
inline PRInt32 FindChar2(const PRUnichar* aDest,PRUint32 aDestLength,PRInt32 anOffset,const PRUnichar aChar,PRInt32 aCount) {

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

inline PRInt32 RFindChar1(const char* aDest,PRUint32 aDestLength,PRInt32 anOffset,const PRUnichar aChar,PRInt32 aCount) {

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
inline PRInt32 RFindChar2(const PRUnichar* aDest,PRUint32 aDestLength,PRInt32 anOffset,const PRUnichar aChar,PRInt32 aCount) {

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

//----------------------------------------------------------------------------------------
//
//  This set of methods is used to compare one buffer onto another.
//  The functions are differentiated by the size of source and dest character sizes.
//  WARNING: Your destination buffer MUST be big enough to hold all the source bytes.
//           We don't validate these ranges here (this should be done in higher level routines).
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
static inline PRInt32 Compare1To1(const char* aStr1,const char* aStr2,PRUint32 aCount,PRBool aIgnoreCase);
PRInt32 Compare1To1(const char* aStr1,const char* aStr2,PRUint32 aCount,PRBool aIgnoreCase){ 
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
PRInt32 Compare2To2(const PRUnichar* aStr1,const PRUnichar* aStr2,PRUint32 aCount);
PRInt32 Compare2To2(const PRUnichar* aStr1,const PRUnichar* aStr2,PRUint32 aCount){
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
static PRInt32 Compare2To1(const PRUnichar* aStr1,const char* aStr2,PRUint32 aCount,PRBool aIgnoreCase);
PRInt32 Compare2To1(const PRUnichar* aStr1,const char* aStr2,PRUint32 aCount,PRBool aIgnoreCase){
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
static inline PRInt32 Compare1To2(const char* aStr1,const PRUnichar* aStr2,PRUint32 aCount,PRBool aIgnoreCase);
PRInt32 Compare1To2(const char* aStr1,const PRUnichar* aStr2,PRUint32 aCount,PRBool aIgnoreCase){
  return Compare2To1(aStr2, aStr1, aCount, aIgnoreCase) * -1;
}


//----------------------------------------------------------------------------------------
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
PRInt32 CompressChars1(char* aString,PRUint32 aLength,const char* aSet);
PRInt32 CompressChars1(char* aString,PRUint32 aLength,const char* aSet){ 

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
PRInt32 CompressChars2(PRUnichar* aString,PRUint32 aLength,const char* aSet);
PRInt32 CompressChars2(PRUnichar* aString,PRUint32 aLength,const char* aSet){ 

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
PRInt32 StripChars1(char* aString,PRUint32 aLength,const char* aSet);
PRInt32 StripChars1(char* aString,PRUint32 aLength,const char* aSet){ 

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
PRInt32 StripChars2(PRUnichar* aString,PRUint32 aLength,const char* aSet);
PRInt32 StripChars2(PRUnichar* aString,PRUint32 aLength,const char* aSet){ 

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

#endif
