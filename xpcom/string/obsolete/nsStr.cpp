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
 *   Rick Gessner <rickg@netscape.com> (original author)
 *   Scott Collins <scc@mozilla.org>
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
  
#include "nsStr.h"
#include "bufferRoutines.h"
#include <stdio.h>  //only used for printf
#include "nsCRT.h"
#include "nsDeque.h"
 
/******************************************************************************************
  MODULE NOTES:

  This file contains the nsStr data structure.
  This general purpose buffer management class is used as the basis for our strings.
  It's benefits include:
    1. An efficient set of library style functions for manipulating nsStrs
    2. Support for 1 and 2 byte character strings (which can easily be increased to n)
    3. Unicode awareness and interoperability.

*******************************************************************************************/

//static const char* kCallFindChar =  "For better performance, call FindChar() for targets whose length==1.";
//static const char* kCallRFindChar = "For better performance, call RFindChar() for targets whose length==1.";

static const  PRUnichar gCommonEmptyBuffer[1] = {0};
static PRBool gStringAcquiredMemory = PR_TRUE;

/**
 * This method initializes all the members of the nsStr structure 
 *
 * @update	gess10/30/98
 * @param 
 * @return
 */
void nsStr::Initialize(nsStr& aDest,eCharSize aCharSize) {
  aDest.mStr=(char*)gCommonEmptyBuffer;
  aDest.mLength=0;
  aDest.mCapacity=0;
  aDest.mCharSize=aCharSize;
  aDest.mOwnsBuffer=0;
}

/**
 * This method initializes all the members of the nsStr structure
 * @update	gess10/30/98
 * @param 
 * @return
 */
void nsStr::Initialize(nsStr& aDest,char* aCString,PRUint32 aCapacity,PRUint32 aLength,eCharSize aCharSize,PRBool aOwnsBuffer){
  aDest.mStr=(aCString) ? aCString : (char*)gCommonEmptyBuffer;
  aDest.mLength=aLength;
  aDest.mCapacity=aCapacity;
  aDest.mCharSize=aCharSize;
  aDest.mOwnsBuffer=aOwnsBuffer;
}


/**
 * This member destroys the memory buffer owned by an nsStr object (if it actually owns it)
 * @update	gess10/30/98
 * @param 
 * @return
 */
void nsStr::Destroy(nsStr& aDest) {
  if((aDest.mStr) && (aDest.mStr!=(char*)gCommonEmptyBuffer)) {
    Free(aDest);
  }
}


/**
 * This method gets called when the internal buffer needs
 * to grow to a given size. The original contents are not preserved.
 * @update  gess 3/30/98
 * @param   aNewLength -- new capacity of string in charSize units
 * @return  void
 */
PRBool nsStr::EnsureCapacity(nsStr& aString,PRUint32 aNewLength) {
  PRBool result=PR_TRUE;
  if(aNewLength>aString.mCapacity) {
    result=Realloc(aString,aNewLength);
    if(aString.mStr)
      AddNullTerminator(aString);
  }
  return result;
}

/**
 * This method gets called when the internal buffer needs
 * to grow to a given size. The original contents ARE preserved.
 * @update  gess 3/30/98
 * @param   aNewLength -- new capacity of string in charSize units
 * @return  void
 */
PRBool nsStr::GrowCapacity(nsStr& aDest,PRUint32 aNewLength) {
  PRBool result=PR_TRUE;
  if(aNewLength>aDest.mCapacity) {
    nsStr theTempStr;
    nsStr::Initialize(theTempStr,eCharSize(aDest.mCharSize));

#ifndef NS_USE_OLD_STRING_ALLOCATION_STRATEGY
      // the new strategy is, allocate exact size, double on grows
    if ( aDest.mCapacity ) {
      PRUint32 newCapacity = aDest.mCapacity;
      while ( newCapacity < aNewLength )
        newCapacity <<= 1;
      aNewLength = newCapacity;
    }
#endif

    result=EnsureCapacity(theTempStr,aNewLength);
    if(result) {
      if(aDest.mLength) {
        StrAppend(theTempStr,aDest,0,aDest.mLength);        
      } 
      Free(aDest);
      aDest.mStr = theTempStr.mStr;
      theTempStr.mStr=0; //make sure to null this out so that you don't lose the buffer you just stole...
      aDest.mLength=theTempStr.mLength;
      aDest.mCapacity=theTempStr.mCapacity;
      aDest.mOwnsBuffer=theTempStr.mOwnsBuffer;
    }
  }
  return result;
}

/**
 * Replaces the contents of aDest with aSource, up to aCount of chars.
 * @update	gess10/30/98
 * @param   aDest is the nsStr that gets changed.
 * @param   aSource is where chars are copied from
 * @param   aCount is the number of chars copied from aSource
 */
void nsStr::StrAssign(nsStr& aDest,const nsStr& aSource,PRUint32 anOffset,PRInt32 aCount){
  if(&aDest!=&aSource){
    StrTruncate(aDest,0);
    StrAppend(aDest,aSource,anOffset,aCount);
  }
}

/**
 * This method appends the given nsStr to this one. Note that we have to 
 * pay attention to the underlying char-size of both structs.
 * @update	gess10/30/98
 * @param   aDest is the nsStr to be manipulated
 * @param   aSource is where char are copied from
 * @aCount  is the number of bytes to be copied 
 */
void nsStr::StrAppend(nsStr& aDest,const nsStr& aSource,PRUint32 anOffset,PRInt32 aCount){
  if(anOffset<aSource.mLength){
    PRUint32 theRealLen=(aCount<0) ? aSource.mLength : MinInt(aCount,aSource.mLength);
    PRUint32 theLength=(anOffset+theRealLen<aSource.mLength) ? theRealLen : (aSource.mLength-anOffset);
    if(0<theLength){
      
      PRBool isBigEnough=PR_TRUE;
      if(aDest.mLength+theLength > aDest.mCapacity) {
        isBigEnough=GrowCapacity(aDest,aDest.mLength+theLength);
      }

      if(isBigEnough) {
        //now append new chars, starting at offset
        (*gCopyChars[aSource.mCharSize][aDest.mCharSize])(aDest.mStr,aDest.mLength,aSource.mStr,anOffset,theLength);

        aDest.mLength+=theLength;
        AddNullTerminator(aDest);
        NSSTR_SEEN(aDest);
      }
    }
  }
}


/**
 * This method inserts up to "aCount" chars from a source nsStr into a dest nsStr.
 * @update	gess10/30/98
 * @param   aDest is the nsStr that gets changed
 * @param   aDestOffset is where in aDest the insertion is to occur
 * @param   aSource is where chars are copied from
 * @param   aSrcOffset is where in aSource chars are copied from
 * @param   aCount is the number of chars from aSource to be inserted into aDest
 */
void nsStr::StrInsert( nsStr& aDest,PRUint32 aDestOffset,const nsStr& aSource,PRUint32 aSrcOffset,PRInt32 aCount){
  //there are a few cases for insert:
  //  1. You're inserting chars into an empty string (assign)
  //  2. You're inserting onto the end of a string (append)
  //  3. You're inserting onto the 1..n-1 pos of a string (the hard case).
  if(0<aSource.mLength){
    if(aDest.mLength){
      if(aDestOffset<aDest.mLength){
        PRInt32 theRealLen=(aCount<0) ? aSource.mLength : MinInt(aCount,aSource.mLength);
        PRInt32 theLength=(aSrcOffset+theRealLen<aSource.mLength) ? theRealLen : (aSource.mLength-aSrcOffset);

        if(aSrcOffset<aSource.mLength) {
            //here's the only new case we have to handle. 
            //chars are really being inserted into our buffer...

          if(aDest.mLength+theLength > aDest.mCapacity) {
            nsStr theTempStr;
            nsStr::Initialize(theTempStr,eCharSize(aDest.mCharSize));

            PRBool isBigEnough=EnsureCapacity(theTempStr,aDest.mLength+theLength);  //grow the temp buffer to the right size

            if(isBigEnough) {
              if(aDestOffset) {
                StrAppend(theTempStr,aDest,0,aDestOffset); //first copy leftmost data...
              } 
              
              StrAppend(theTempStr,aSource,aSrcOffset,theLength); //next copy inserted (new) data
            
              PRUint32 theRemains=aDest.mLength-aDestOffset;
              if(theRemains) {
                StrAppend(theTempStr,aDest,aDestOffset,theRemains); //next copy rightmost data
              }

              Free(aDest);
              aDest.mStr = theTempStr.mStr;
              theTempStr.mStr=0; //make sure to null this out so that you don't lose the buffer you just stole...
              aDest.mCapacity=theTempStr.mCapacity;
              aDest.mOwnsBuffer=theTempStr.mOwnsBuffer;
            }

          }

          else {
              //shift the chars right by theDelta...
            (*gShiftChars[aDest.mCharSize][KSHIFTRIGHT])(aDest.mStr,aDest.mLength,aDestOffset,theLength);
      
            //now insert new chars, starting at offset
            (*gCopyChars[aSource.mCharSize][aDest.mCharSize])(aDest.mStr,aDestOffset,aSource.mStr,aSrcOffset,theLength);
          }

            //finally, make sure to update the string length...
          aDest.mLength+=theLength;
          AddNullTerminator(aDest);
          NSSTR_SEEN(aDest);
        }//if
        //else nothing to do!
      }
      else StrAppend(aDest,aSource,0,aCount);
    }
    else StrAppend(aDest,aSource,0,aCount);
  }
}


/**
 * This method deletes up to aCount chars from aDest
 * @update	gess10/30/98
 * @param   aDest is the nsStr to be manipulated
 * @param   aDestOffset is where in aDest deletion is to occur
 * @param   aCount is the number of chars to be deleted in aDest
 */
void nsStr::Delete(nsStr& aDest,PRUint32 aDestOffset,PRUint32 aCount){
  if(aDestOffset<aDest.mLength){

    PRUint32 theDelta=aDest.mLength-aDestOffset;
    PRUint32 theLength=(theDelta<aCount) ? theDelta : aCount;

    if(aDestOffset+theLength<aDest.mLength) {

      //if you're here, it means we're cutting chars out of the middle of the string...
      //so shift the chars left by theLength...
      (*gShiftChars[aDest.mCharSize][KSHIFTLEFT])(aDest.mStr,aDest.mLength,aDestOffset,theLength);
      aDest.mLength-=theLength;
      AddNullTerminator(aDest);
      NSSTR_SEEN(aDest);
    }
    else StrTruncate(aDest,aDestOffset);
  }//if
}

/**
 * This method truncates the given nsStr at given offset
 * @update	gess10/30/98
 * @param   aDest is the nsStr to be truncated
 * @param   aDestOffset is where in aDest truncation is to occur
 */
void nsStr::StrTruncate(nsStr& aDest,PRUint32 aDestOffset){
  if(aDest.mCapacity && aDestOffset<=aDest.mCapacity){
    aDest.mLength=aDestOffset;
    AddNullTerminator(aDest);
    NSSTR_SEEN(aDest);
  }
}


/**
 * This method forces the given string to upper or lowercase
 * @update	gess1/7/99
 * @param   aDest is the string you're going to change
 * @param   aToUpper: if TRUE, then we go uppercase, otherwise we go lowercase
 * @return
 */
void nsStr::ChangeCase(nsStr& aDest,PRBool aToUpper) {
  // somehow UnicharUtil return failed, fallback to the old ascii only code
  gCaseConverters[aDest.mCharSize](aDest.mStr,aDest.mLength,aToUpper);
}

/**
 * This method removes characters from the given set from this string.
 * NOTE: aSet is a char*, and it's length is computed using strlen, which assumes null termination.
 *
 * @update	gess 11/7/99
 * @param aDest
 * @param aSet
 * @param aEliminateLeading
 * @param aEliminateTrailing
 * @return nothing
 */
void nsStr::Trim(nsStr& aDest,const char* aSet,PRBool aEliminateLeading,PRBool aEliminateTrailing){

  if((aDest.mLength>0) && aSet){
    PRInt32 theIndex=-1;
    PRInt32 theMax=aDest.mLength;
    PRInt32 theSetLen=nsCRT::strlen(aSet);

    if(aEliminateLeading) {
      while(++theIndex<=theMax) {
        PRUnichar theChar=GetCharAt(aDest,theIndex);
        PRInt32 thePos=gFindChars[eOneByte](aSet,theSetLen,0,theChar,PR_FALSE,theSetLen);
        if(kNotFound==thePos)
          break;
      }
      if(0<theIndex) {
        if(theIndex<theMax) {
          Delete(aDest,0,theIndex);
        }
        else StrTruncate(aDest,0);
      }
    }

    if(aEliminateTrailing) {
      theIndex=aDest.mLength;
      PRInt32 theNewLen=theIndex;
      while(--theIndex>=0) {
        PRUnichar theChar=GetCharAt(aDest,theIndex);  //read at end now...
        PRInt32 thePos=gFindChars[eOneByte](aSet,theSetLen,0,theChar,PR_FALSE,theSetLen);
        if(kNotFound<thePos) 
          theNewLen=theIndex;
        else break;
      }
      if(theNewLen<theMax) {
        StrTruncate(aDest,theNewLen);
      }
    }

  }
}

/**
 * 
 * @update	gess1/7/99
 * @param 
 * @return
 */
void nsStr::CompressSet(nsStr& aDest,const char* aSet,PRBool aEliminateLeading,PRBool aEliminateTrailing){
  Trim(aDest,aSet,aEliminateLeading,aEliminateTrailing);
  PRUint32 aNewLen=gCompressChars[aDest.mCharSize](aDest.mStr,aDest.mLength,aSet);
  aDest.mLength=aNewLen;
  NSSTR_SEEN(aDest);
}


/**
 * 
 * @update	gess1/7/99
 * @param 
 * @return
 */
void nsStr::StripChars(nsStr& aDest,const char* aSet){
  if((0<aDest.mLength) && (aSet)) {
    PRUint32 aNewLen=gStripChars[aDest.mCharSize](aDest.mStr,aDest.mLength,aSet);
    aDest.mLength=aNewLen;
    NSSTR_SEEN(aDest);
  }
}


  /**************************************************************
    Searching methods...
   **************************************************************/


/**
 *  This searches aDest for a given substring
 *  
 *  @update  gess 2/04/00: added aCount argument to restrict search
 *  @param   aDest string to search
 *  @param   aTarget is the substring you're trying to find.
 *  @param   aIgnorecase indicates case sensitivity of search
 *  @param   anOffset tells us where to start the search
 *  @param   aCount tells us how many iterations to make from offset; -1 means the full length of the string
 *  @return  index in aDest where member of aSet occurs, or -1 if not found
 */
PRInt32 nsStr::FindSubstr(const nsStr& aDest,const nsStr& aTarget, PRBool aIgnoreCase,PRInt32 anOffset,PRInt32 aCount) {
  PRInt32 theMaxPos = aDest.mLength-aTarget.mLength;  //this is the last pos that is feasible for starting the search, with given lengths...

  if(0<=theMaxPos) {

    if(anOffset<0)
      anOffset=0;

    if((0<aDest.mLength) && (anOffset<=theMaxPos) && (aTarget.mLength)) {

      if(aCount<0)
        aCount = MaxInt(theMaxPos,1);
      
      if(0<aCount) {

        PRInt32     aDelta= (aDest.mCharSize == eOneByte) ? 1 : 2;
        const char* root  = aDest.mStr; 
        const char* left  = root+(anOffset*aDelta);
        const char* last  = left+((aCount)*aDelta);
        const char* max   = root+(theMaxPos*aDelta);
        const char* right = (last<max) ? last : max;

        while(left<=right){
          PRInt32 cmp=(*gCompare[aDest.mCharSize][aTarget.mCharSize])(left,aTarget.mStr,aTarget.mLength,aIgnoreCase);
          if(0==cmp) {
            return (left-root)/aDelta;
          }
          left+=aDelta;
        } //while

      } //if 
    }
  } //if

  return kNotFound;
}


/**
 *  This searches aDest for a given character 
 *  
 *  @update  gess 2/04/00: added aCount argument to restrict search
 *  @param   aDest string to search
 *  @param   char is the character you're trying to find.
 *  @param   aIgnorecase indicates case sensitivity of search
 *  @param   anOffset tells us where to start the search
 *  @param   aCount tell us how many chars to search from offset
 *  @return  index in aDest where member of aSet occurs, or -1 if not found
 */
PRInt32 nsStr::FindChar(const nsStr& aDest,PRUnichar aChar, PRBool aIgnoreCase,PRInt32 anOffset,PRInt32 aCount) {
  return gFindChars[aDest.mCharSize](aDest.mStr,aDest.mLength,anOffset,aChar,aIgnoreCase,aCount);
}


/**
 *  This searches aDest for a character found in aSet. 
 *  
 *  @update  gess 3/25/98
 *  @param   aDest string to search
 *  @param   aSet contains a list of chars to be searched for
 *  @param   aIgnorecase indicates case sensitivity of search
 *  @param   anOffset tells us where to start the search
 *  @return  index in aDest where member of aSet occurs, or -1 if not found
 */
PRInt32 nsStr::FindCharInSet(const nsStr& aDest,const nsStr& aSet,PRBool aIgnoreCase,PRInt32 anOffset) {
  //NS_PRECONDITION(aSet.mLength!=1,kCallFindChar);

  PRInt32 index=(0<=anOffset) ? anOffset-1 : -1;
  PRInt32 thePos;

    //Note that the search is inverted here. We're scanning aDest, one char at a time
    //but doing the search against the given set. That's why we use 0 as the offset below.
  if((0<aDest.mLength) && (0<aSet.mLength)){
    while(++index<(PRInt32)aDest.mLength) {
      PRUnichar theChar=GetCharAt(aDest,index);
      thePos=gFindChars[aSet.mCharSize](aSet.mStr,aSet.mLength,0,theChar,aIgnoreCase,aSet.mLength);
      if(kNotFound!=thePos)
        return index;
    } //while
  }
  return kNotFound;
}

  /**************************************************************
    Reverse Searching methods...
   **************************************************************/


/**
 *  This searches aDest (in reverse) for a given substring
 *
 *  @update  gess 2/18/00
 *  @param   aDest string to search
 *  @param   aTarget is the substring you're trying to find.
 *  @param   aIgnorecase indicates case sensitivity of search
 *  @param   anOffset tells us where to start the search (counting from left)
 *  @param   aCount tell us how many iterations to perform from offset
 *  @return  index in aDest where member of aSet occurs, or -1 if not found
 */
PRInt32 nsStr::RFindSubstr(const nsStr& aDest,const nsStr& aTarget,PRBool aIgnoreCase,PRInt32 anOffset,PRInt32 aCount) {
  if(anOffset<0)
    anOffset=(PRInt32)aDest.mLength-1;

  if(aCount<0)
    aCount = aDest.mLength;

  if((0<aDest.mLength) && ((PRUint32)anOffset<aDest.mLength) && (aTarget.mLength)) {

    if(0<aCount) {

      PRInt32     aDelta    = (aDest.mCharSize == eOneByte) ? 1 : 2;
      const char* root      = aDest.mStr;
      const char* destLast  = root+(aDest.mLength*aDelta); //pts to last char in aDest (likely null)

      const char* rightmost = root+(anOffset*aDelta);
      const char* min       = rightmost-((aCount-1)*aDelta);

      const char* leftmost  = (min<root) ? root: min;

      while(leftmost<=rightmost) {
          //don't forget to divide by delta in next text (bug found by rhp)...
        if(aTarget.mLength<=PRUint32((destLast-rightmost)/aDelta)) {
          PRInt32 result=(*gCompare[aDest.mCharSize][aTarget.mCharSize])(rightmost,aTarget.mStr,aTarget.mLength,aIgnoreCase);

          if(0==result) {
            return (rightmost-root)/aDelta;
          }
        } //if
        rightmost-=aDelta;
      } //while
    }
  }

  return kNotFound;
}



/**
 *  This searches aDest (in reverse) for a given character 
 *  
 *  @update  gess 2/04/00
 *  @param   aDest string to search
 *  @param   char is the character you're trying to find.
 *  @param   aIgnorecase indicates case sensitivity of search
 *  @param   anOffset tells us where to start the search; -1 means start at very end (mLength)
 *  @param   aCount tell us how many iterations to perform from offset; -1 means use full length.
 *  @return  index in aDest where member of aSet occurs, or -1 if not found
 */
PRInt32 nsStr::RFindChar(const nsStr& aDest,PRUnichar aChar, PRBool aIgnoreCase,PRInt32 anOffset,PRInt32 aCount) {
  return gRFindChars[aDest.mCharSize](aDest.mStr,aDest.mLength,anOffset,aChar,aIgnoreCase,aCount);
}


/**
 *  This searches aDest (in reverese) for a character found in aSet. 
 *  
 *  @update  gess 3/25/98
 *  @param   aDest string to search
 *  @param   aSet contains a list of chars to be searched for
 *  @param   aIgnorecase indicates case sensitivity of search
 *  @param   anOffset tells us where to start the search
 *  @return  index in aDest where member of aSet occurs, or -1 if not found
 */
PRInt32 nsStr::RFindCharInSet(const nsStr& aDest,const nsStr& aSet,PRBool aIgnoreCase,PRInt32 anOffset) {
  //NS_PRECONDITION(aSet.mLength!=1,kCallRFindChar);

  PRInt32 index=(0<=anOffset) ? anOffset : aDest.mLength;
  PRInt32 thePos;

    //note that the search is inverted here. We're scanning aDest, one char at a time
    //but doing the search against the given set. That's why we use 0 as the offset below.
  if(0<aDest.mLength) {
    while(--index>=0) {
      PRUnichar theChar=GetCharAt(aDest,index);
      thePos=gFindChars[aSet.mCharSize](aSet.mStr,aSet.mLength,0,theChar,aIgnoreCase,aSet.mLength);
      if(kNotFound!=thePos)
        return index;
    } //while
  }
  return kNotFound;
}

/**
 * Compare source and dest strings, up to an (optional max) number of chars
 * @param   aDest is the first str to compare
 * @param   aSource is the second str to compare
 * @param   aCount -- if (-1), then we use length of longer string; if (0<aCount) then it gives the max # of chars to compare
 * @param   aIgnorecase tells us whether to search with case sensitivity
 * @return  aDest<aSource=-1;aDest==aSource==0;aDest>aSource=1
 */
PRInt32 nsStr::StrCompare(const nsStr& aDest,const nsStr& aSource,PRInt32 aCount,PRBool aIgnoreCase) {
  PRInt32 result=0;

  if(aCount) {
    PRInt32 minlen=(aSource.mLength<aDest.mLength) ? aSource.mLength : aDest.mLength;

    if(0==minlen) {
      if ((aDest.mLength == 0) && (aSource.mLength == 0))
        return 0;
      if (aDest.mLength == 0)
        return -1;
      return 1;
    }

    PRInt32 theCount = (aCount<0) ? minlen: MinInt(aCount,minlen);
    result=(*gCompare[aDest.mCharSize][aSource.mCharSize])(aDest.mStr,aSource.mStr,theCount,aIgnoreCase);

    if (0==result) {
      if(-1==aCount) {

          //Since the caller didn't give us a length to test, and minlen characters matched,
          //we have to assume that the longer string is greater.

        if (aDest.mLength != aSource.mLength) {
          //we think they match, but we've only compared minlen characters. 
          //if the string lengths are different, then they don't really match.
          result = (aDest.mLength<aSource.mLength) ? -1 : 1;
        }
      } //if
    }

  }
  return result;
}

/**
 * Overwrites the contents of dest at offset with contents of aSource
 * 
 * @param   aDest is the first str to compare
 * @param   aSource is the second str to compare
 * @param   aDestOffset is the offset within aDest where source should be copied
 * @return  error code
 */
void nsStr::Overwrite(nsStr& aDest,const nsStr& aSource,PRInt32 aDestOffset) {
  if(aDest.mLength && aSource.mLength) {
    if((aDest.mLength-aDestOffset)>=aSource.mLength) {
      //if you're here, then both dest and source have valid lengths
      //and there's enough room in dest (at offset) to contain source.
      (*gCopyChars[aSource.mCharSize][aDest.mCharSize])(aDest.mStr,aDestOffset,aSource.mStr,0,aSource.mLength);      
    }
  }
}

//----------------------------------------------------------------------------------------

PRBool nsStr::Alloc(nsStr& aDest,PRUint32 aCount) {

  static int mAllocCount=0;
  mAllocCount++;

#ifdef NS_USE_OLD_STRING_ALLOCATION_STRATEGY
  //we're given the acount value in charunits; now scale up to next multiple.
  PRUint32	theNewCapacity=kDefaultStringSize;
  while(theNewCapacity<aCount){ 
		theNewCapacity<<=1;
  }

  aDest.mCapacity=theNewCapacity++;
  PRUint32 theSize=(theNewCapacity<<aDest.mCharSize);
  aDest.mStr = (char*)nsMemory::Alloc(theSize);
#else
    // the new strategy is, allocate exact size, double on grows
  aDest.mCapacity = aCount;
  aDest.mStr = (char*)nsMemory::Alloc((aCount+1)<<aDest.mCharSize);
#endif

  if(aDest.mStr) {
    aDest.mOwnsBuffer=1;
    gStringAcquiredMemory=PR_TRUE;
  }
  else gStringAcquiredMemory=PR_FALSE;
  return gStringAcquiredMemory;
}

PRBool nsStr::Free(nsStr& aDest){
  if(aDest.mStr){
    if(aDest.mOwnsBuffer){
      nsMemory::Free(aDest.mStr);
    }
    aDest.mStr=0;
    aDest.mOwnsBuffer=0;
    return PR_TRUE;
  }
  return PR_FALSE;
}

PRBool nsStr::Realloc(nsStr& aDest,PRUint32 aCount){

  nsStr temp;
  memcpy(&temp,&aDest,sizeof(aDest));

  PRBool result=Alloc(temp,aCount);
  if(result) {
    Free(aDest);
    aDest.mStr=temp.mStr;
    aDest.mCapacity=temp.mCapacity;
    aDest.mOwnsBuffer=temp.mOwnsBuffer;
  }
  return result;
}

/**
 * Retrieve last memory error
 *
 * @update  gess 10/11/99
 * @return  memory error (usually returns PR_TRUE)
 */
PRBool nsStr::DidAcquireMemory(void) {
  return gStringAcquiredMemory;
}

//----------------------------------------------------------------------------------------

CBufDescriptor::CBufDescriptor(char* aString,PRBool aStackBased,PRUint32 aCapacity,PRInt32 aLength) { 
  mBuffer=aString;
  mCharSize=eOneByte;
  mStackBased=aStackBased;
  mIsConst=PR_FALSE;
  mLength=mCapacity=0;
  if(aString && aCapacity>1) {
    mCapacity=aCapacity-1;
    mLength=(-1==aLength) ? strlen(aString) : aLength;
    if(mLength>PRInt32(mCapacity))
      mLength=mCapacity;
  }
}

CBufDescriptor::CBufDescriptor(const char* aString,PRBool aStackBased,PRUint32 aCapacity,PRInt32 aLength) { 
  mBuffer=(char*)aString;
  mCharSize=eOneByte;
  mStackBased=aStackBased;
  mIsConst=PR_TRUE;
  mLength=mCapacity=0;
  if(aString && aCapacity>1) {
    mCapacity=aCapacity-1;
    mLength=(-1==aLength) ? strlen(aString) : aLength;
    if(mLength>PRInt32(mCapacity))
      mLength=mCapacity;
  }
}


CBufDescriptor::CBufDescriptor(PRUnichar* aString,PRBool aStackBased,PRUint32 aCapacity,PRInt32 aLength) { 
  mBuffer=(char*)aString;
  mCharSize=eTwoByte;
  mStackBased=aStackBased;
  mLength=mCapacity=0;
  mIsConst=PR_FALSE;
  if(aString && aCapacity>1) {
    mCapacity=aCapacity-1;
    mLength=(-1==aLength) ? nsCRT::strlen(aString) : aLength;
    if(mLength>PRInt32(mCapacity))
      mLength=mCapacity;
  }
}

CBufDescriptor::CBufDescriptor(const PRUnichar* aString,PRBool aStackBased,PRUint32 aCapacity,PRInt32 aLength) { 
  mBuffer=(char*)aString;
  mCharSize=eTwoByte;
  mStackBased=aStackBased;
  mLength=mCapacity=0;
  mIsConst=PR_TRUE;
  if(aString && aCapacity>1) {
    mCapacity=aCapacity-1;
    mLength=(-1==aLength) ? nsCRT::strlen(aString) : aLength;
    if(mLength>PRInt32(mCapacity))
      mLength=mCapacity;
  }
}

//----------------------------------------------------------------------------------------

PRUint32
nsStr::HashCode(const nsStr& aDest)
{
	if (aDest.mCharSize == eTwoByte)
    return nsCRT::HashCode(aDest.mUStr);
  else 
    return nsCRT::HashCode(aDest.mStr);
}

#ifdef NS_STR_STATS

#include <ctype.h>

#ifdef XP_MAC
#define isascii(c)      ((unsigned)(c) < 0x80)
#endif

void
nsStr::Print(const nsStr& aDest, FILE* out, PRBool truncate)
{
  PRInt32 printLen = (PRInt32)aDest.mLength;

  if (aDest.mCharSize == eOneByte) {
    const char* chars = aDest.mStr;
    while (printLen-- && (!truncate || *chars != '\n')) {
      fputc(*chars++, out);
    }
  }
  else {
    const PRUnichar* chars = aDest.mUStr;
    while (printLen-- && (!truncate || *chars != '\n')) {
      if (isascii(*chars))
        fputc((char)(*chars++), out);
      else
        fputc('-', out);
    }
  }
}

////////////////////////////////////////////////////////////////////////////////
// String Usage Statistics Routines

static PLHashTable* gStringInfo = nsnull;
PRLock* gStringInfoLock = nsnull;
PRBool gNoStringInfo = PR_FALSE;

nsStringInfo::nsStringInfo(nsStr& str)
  : mCount(0)
{
  nsStr::Initialize(mStr, str.mCharSize);
  nsStr::StrAssign(mStr, str, 0, -1);
//  nsStr::Print(mStr, stdout);
//  fputc('\n', stdout);
}

PR_EXTERN(PRHashNumber)
nsStr_Hash(const void* key)
{
  nsStr* str = (nsStr*)key;
  return nsStr::HashCode(*str);
}

PR_EXTERN(PRIntn)
nsStr_Compare(const void *v1, const void *v2)
{
  nsStr* str1 = (nsStr*)v1;
  nsStr* str2 = (nsStr*)v2;
  return nsStr::StrCompare(*str1, *str2, -1, PR_FALSE) == 0;
}

nsStringInfo*
nsStringInfo::GetInfo(nsStr& str)
{
  if (gStringInfo == nsnull) {
    gStringInfo = PL_NewHashTable(1024,
                                  nsStr_Hash,
                                  nsStr_Compare,
                                  PL_CompareValues,
                                  NULL, NULL);
    gStringInfoLock = PR_NewLock();
  }
  PR_Lock(gStringInfoLock);
  nsStringInfo* info =
    (nsStringInfo*)PL_HashTableLookup(gStringInfo, &str);
  if (info == NULL) {
    gNoStringInfo = PR_TRUE;
    info = new nsStringInfo(str);
    if (info) {
      PLHashEntry* e = PL_HashTableAdd(gStringInfo, &info->mStr, info);
      if (e == NULL) {
        delete info;
        info = NULL;
      }
    }
    gNoStringInfo = PR_FALSE;
  }
  PR_Unlock(gStringInfoLock);
  return info;
}

void
nsStringInfo::Seen(nsStr& str)
{
  if (!gNoStringInfo) {
    nsStringInfo* info = GetInfo(str);
    info->mCount++;
  }
}

void
nsStringInfo::Report(FILE* out)
{
  if (gStringInfo) {
    fprintf(out, "\n== String Stats\n");
    PL_HashTableEnumerateEntries(gStringInfo, nsStringInfo::ReportEntry, out);
  }
}

PRIntn
nsStringInfo::ReportEntry(PLHashEntry *he, PRIntn i, void *arg)
{
  nsStringInfo* entry = (nsStringInfo*)he->value;
  FILE* out = (FILE*)arg;
  
  fprintf(out, "%d ==> (%d) ", entry->mCount, entry->mStr.mLength);
  nsStr::Print(entry->mStr, out, PR_TRUE);
  fputc('\n', out);
  return HT_ENUMERATE_NEXT;
}

#endif // NS_STR_STATS

////////////////////////////////////////////////////////////////////////////////
