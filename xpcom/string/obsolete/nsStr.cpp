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

#ifndef nsCharTraits_h___
#include "nsCharTraits.h"
#endif

#include "nsStrPrivate.h"
#include "nsStr.h"
#include "bufferRoutines.h"
#include <stdio.h>  //only used for printf
#include "prdtoa.h"

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

#ifdef NS_STR_STATS
static PRBool gStringAcquiredMemory = PR_TRUE;
#endif

/**
 * This method initializes all the members of the nsStr structure 
 *
 * @update	gess10/30/98
 * @param 
 * @return
 */
void nsStrPrivate::Initialize(nsStr& aDest,eCharSize aCharSize) {
  aDest.mStr=(char*)gCommonEmptyBuffer;
  aDest.mLength=0;
  aDest.mCapacityAndFlags = 0;
  // handled by mCapacityAndFlags
  // aDest.SetInternalCapacity(0);
  // aDest.SetOwnsBuffer(PR_FALSE);
  aDest.SetCharSize(aCharSize);
}

/**
 * This method initializes all the members of the nsStr structure
 * @update	gess10/30/98
 * @param 
 * @return
 */
void nsStrPrivate::Initialize(nsStr& aDest,char* aCString,PRUint32 aCapacity,PRUint32 aLength,eCharSize aCharSize,PRBool aOwnsBuffer){
  aDest.mStr=(aCString) ? aCString : (char*)gCommonEmptyBuffer;
  aDest.mLength=aLength;
  aDest.mCapacityAndFlags = 0;
  aDest.SetInternalCapacity(aCapacity);
  aDest.SetCharSize(aCharSize);
  aDest.SetOwnsBuffer(aOwnsBuffer);
}

/**
 * This member destroys the memory buffer owned by an nsStr object (if it actually owns it)
 * @update	gess10/30/98
 * @param 
 * @return
 */
void nsStrPrivate::Destroy(nsStr& aDest) {
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
PRBool nsStrPrivate::EnsureCapacity(nsStr& aString,PRUint32 aNewLength) {
  PRBool result=PR_TRUE;
  if(aNewLength>aString.GetCapacity()) {
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
PRBool nsStrPrivate::GrowCapacity(nsStr& aDest,PRUint32 aNewLength) {
  PRBool result=PR_TRUE;
  if(aNewLength>aDest.GetCapacity()) {
    nsStr theTempStr;
    nsStrPrivate::Initialize(theTempStr,eCharSize(aDest.GetCharSize()));

      // the new strategy is, allocate exact size, double on grows
    if ( aDest.GetCapacity() ) {
      PRUint32 newCapacity = aDest.GetCapacity();
      while ( newCapacity < aNewLength )
        newCapacity <<= 1;
      aNewLength = newCapacity;
    }

    result=EnsureCapacity(theTempStr,aNewLength);
    if(result) {
      if(aDest.mLength) {
        StrAppend(theTempStr,aDest,0,aDest.mLength);        
      } 
      Free(aDest);
      aDest.mStr = theTempStr.mStr;
      theTempStr.mStr=0; //make sure to null this out so that you don't lose the buffer you just stole...
      aDest.mLength=theTempStr.mLength;
      aDest.SetInternalCapacity(theTempStr.GetCapacity());
      aDest.SetOwnsBuffer(theTempStr.GetOwnsBuffer());
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
void nsStrPrivate::StrAssign(nsStr& aDest,const nsStr& aSource,PRUint32 anOffset,PRInt32 aCount){
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
void nsStrPrivate::StrAppend(nsStr& aDest,const nsStr& aSource,PRUint32 anOffset,PRInt32 aCount){
  if(anOffset<aSource.mLength){
    PRUint32 theRealLen=(aCount<0) ? aSource.mLength : MinInt(aCount,aSource.mLength);
    PRUint32 theLength=(anOffset+theRealLen<aSource.mLength) ? theRealLen : (aSource.mLength-anOffset);
    if(0<theLength){
      
      PRBool isBigEnough=PR_TRUE;
      if(aDest.mLength+theLength > aDest.GetCapacity()) {
        isBigEnough=GrowCapacity(aDest,aDest.mLength+theLength);
      }

      if(isBigEnough) {
        //now append new chars, starting at offset
        (*gCopyChars[aSource.GetCharSize()][aDest.GetCharSize()])(aDest.mStr,aDest.mLength,aSource.mStr,anOffset,theLength);

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

PRInt32 nsStrPrivate::GetSegmentLength(const nsStr& aSource,
                                    PRUint32 aSrcOffset, PRInt32 aCount)
{
  PRInt32 theRealLen=(aCount<0) ? aSource.mLength : MinInt(aCount,aSource.mLength);
  PRInt32 theLength=(aSrcOffset+theRealLen<aSource.mLength) ? theRealLen : (aSource.mLength-aSrcOffset);

  return theLength;
}

void nsStrPrivate::AppendForInsert(nsStr& aDest, PRUint32 aDestOffset, const nsStr& aSource, PRUint32 aSrcOffset, PRInt32 theLength) {
  nsStr theTempStr;
  nsStrPrivate::Initialize(theTempStr,eCharSize(aDest.GetCharSize()));

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
    aDest.SetInternalCapacity(theTempStr.GetCapacity());
    aDest.SetOwnsBuffer(theTempStr.GetOwnsBuffer());
  }
}

void nsStrPrivate::StrInsert1into1( nsStr& aDest,PRUint32 aDestOffset,const nsStr& aSource,PRUint32 aSrcOffset,PRInt32 aCount){
  NS_ASSERTION(aSource.GetCharSize() == eOneByte, "Must be 1 byte");
  NS_ASSERTION(aDest.GetCharSize() == eOneByte, "Must be 1 byte");
  //there are a few cases for insert:
  //  1. You're inserting chars into an empty string (assign)
  //  2. You're inserting onto the end of a string (append)
  //  3. You're inserting onto the 1..n-1 pos of a string (the hard case).
  if(0<aSource.mLength){
    if(aDest.mLength){
      if(aDestOffset<aDest.mLength){
        PRInt32 theLength = GetSegmentLength(aSource, aSrcOffset, aCount);

        if(aSrcOffset<aSource.mLength) {
            //here's the only new case we have to handle. 
            //chars are really being inserted into our buffer...

          if(aDest.mLength+theLength > aDest.GetCapacity())
            AppendForInsert(aDest, aDestOffset, aSource, aSrcOffset, theLength);
          else {
            //shift the chars right by theDelta...
            ShiftCharsRight(aDest.mStr, aDest.mLength, aDestOffset, theLength);
      
            //now insert new chars, starting at offset
            CopyChars1To1(aDest.mStr, aDestOffset, aSource.mStr, aSrcOffset, theLength);
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

void nsStrPrivate::StrInsert1into2( nsStr& aDest,PRUint32 aDestOffset,const nsStr& aSource,PRUint32 aSrcOffset,PRInt32 aCount){
  NS_ASSERTION(aSource.GetCharSize() == eOneByte, "Must be 1 byte");
  NS_ASSERTION(aDest.GetCharSize() == eTwoByte, "Must be 2 byte");
  //there are a few cases for insert:
  //  1. You're inserting chars into an empty string (assign)
  //  2. You're inserting onto the end of a string (append)
  //  3. You're inserting onto the 1..n-1 pos of a string (the hard case).
  if(0<aSource.mLength){
    if(aDest.mLength){
      if(aDestOffset<aDest.mLength){
        PRInt32 theLength = GetSegmentLength(aSource, aSrcOffset, aCount);

        if(aSrcOffset<aSource.mLength) {
            //here's the only new case we have to handle. 
            //chars are really being inserted into our buffer...

          if(aDest.mLength+theLength > aDest.GetCapacity())
            AppendForInsert(aDest, aDestOffset, aSource, aSrcOffset, theLength);
          else {
            //shift the chars right by theDelta...
            ShiftDoubleCharsRight(aDest.mUStr, aDest.mLength, aDestOffset, theLength);
      
            //now insert new chars, starting at offset
            CopyChars1To2(aDest.mStr,aDestOffset,aSource.mStr,aSrcOffset,theLength);
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

void nsStrPrivate::StrInsert2into2( nsStr& aDest,PRUint32 aDestOffset,const nsStr& aSource,PRUint32 aSrcOffset,PRInt32 aCount){
  NS_ASSERTION(aSource.GetCharSize() == eTwoByte, "Must be 1 byte");
  NS_ASSERTION(aDest.GetCharSize() == eTwoByte, "Must be 2 byte");
  //there are a few cases for insert:
  //  1. You're inserting chars into an empty string (assign)
  //  2. You're inserting onto the end of a string (append)
  //  3. You're inserting onto the 1..n-1 pos of a string (the hard case).
  if(0<aSource.mLength){
    if(aDest.mLength){
      if(aDestOffset<aDest.mLength){
        PRInt32 theLength = GetSegmentLength(aSource, aSrcOffset, aCount);

        if(aSrcOffset<aSource.mLength) {
            //here's the only new case we have to handle. 
            //chars are really being inserted into our buffer...

          if(aDest.mLength+theLength > aDest.GetCapacity())
            AppendForInsert(aDest, aDestOffset, aSource, aSrcOffset, theLength);
          else {
            
            //shift the chars right by theDelta...
            ShiftDoubleCharsRight(aDest.mUStr, aDest.mLength, aDestOffset, theLength);
      
            //now insert new chars, starting at offset
            CopyChars2To2(aDest.mStr,aDestOffset,aSource.mStr,aSrcOffset,theLength);
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


void nsStrPrivate::Delete1(nsStr& aDest,PRUint32 aDestOffset,PRUint32 aCount){
  NS_ASSERTION(aDest.GetCharSize() == eOneByte, "Must be 1 byte");
  
  if(aDestOffset<aDest.mLength){
    
    PRUint32 theLength=GetDeleteLength(aDest, aDestOffset, aCount);
    
    if(aDestOffset+theLength<aDest.mLength) {

      //if you're here, it means we're cutting chars out of the middle of the string...
      //so shift the chars left by theLength...
      ShiftCharsLeft(aDest.mStr,aDest.mLength,aDestOffset,theLength);
      aDest.mLength-=theLength;
      AddNullTerminator(aDest);
      NSSTR_SEEN(aDest);
    }
    else StrTruncate(aDest,aDestOffset);
  }//if
}

void nsStrPrivate::Delete2(nsStr& aDest,PRUint32 aDestOffset,PRUint32 aCount){
  
  NS_ASSERTION(aDest.GetCharSize() == eTwoByte, "Must be 2 byte");
  
  if(aDestOffset<aDest.mLength){
    
    PRUint32 theLength=GetDeleteLength(aDest, aDestOffset, aCount);
    
    if(aDestOffset+theLength<aDest.mLength) {

      //if you're here, it means we're cutting chars out of the middle of the string...
      //so shift the chars left by theLength...
      ShiftDoubleCharsLeft(aDest.mUStr,aDest.mLength,aDestOffset,theLength);
      aDest.mLength-=theLength;
      AddNullTerminator(aDest);
      NSSTR_SEEN(aDest);
    }
    else StrTruncate(aDest,aDestOffset);
  }//if
}

PRInt32 nsStrPrivate::GetDeleteLength(const nsStr& aDest, PRUint32 aDestOffset, PRUint32 aCount)
{
    PRUint32 theDelta=aDest.mLength-aDestOffset;
    PRUint32 theLength=(theDelta<aCount) ? theDelta : aCount;

    return theLength;
}

/**
 * This method truncates the given nsStr at given offset
 * @update	gess10/30/98
 * @param   aDest is the nsStr to be truncated
 * @param   aDestOffset is where in aDest truncation is to occur
 */
void nsStrPrivate::StrTruncate(nsStr& aDest,PRUint32 aDestOffset){
  if(aDest.GetCapacity() && aDestOffset<=aDest.GetCapacity()){
    aDest.mLength=aDestOffset;
    AddNullTerminator(aDest);
    NSSTR_SEEN(aDest);
  }
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
void nsStrPrivate::Trim(nsStr& aDest,const char* aSet,PRBool aEliminateLeading,PRBool aEliminateTrailing){

  if((aDest.mLength>0) && aSet){
    PRInt32 theIndex=-1;
    PRInt32 theMax=aDest.mLength;
    PRInt32 theSetLen=strlen(aSet);

    if(aEliminateLeading) {
      while(++theIndex<=theMax) {
        PRUnichar theChar=aDest.GetCharAt(theIndex);
        PRInt32 thePos=::FindChar1(aSet,theSetLen,0,theChar,theSetLen);
        if(kNotFound==thePos)
          break;
      }
      
      if(0<theIndex) {
        if(theIndex<theMax) {
          if (aDest.GetCharSize() == eOneByte)
            Delete1(aDest,0,theIndex);
          else
            Delete2(aDest,0,theIndex);
        }
        else StrTruncate(aDest,0);
      }
    }

    if(aEliminateTrailing) {
      theIndex=aDest.mLength;
      PRInt32 theNewLen=theIndex;
      while(--theIndex>=0) {
        PRUnichar theChar=aDest.GetCharAt(theIndex);  //read at end now...
        PRInt32 thePos=::FindChar1(aSet,theSetLen,0,theChar,theSetLen);
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
void nsStrPrivate::CompressSet1(nsStr& aDest,const char* aSet,PRBool aEliminateLeading,PRBool aEliminateTrailing){
  NS_ASSERTION(aDest.GetCharSize() == eOneByte, "Must be 1 byte");
  
  Trim(aDest,aSet,aEliminateLeading,aEliminateTrailing);
  PRUint32 aNewLen=CompressChars1(aDest.mStr,aDest.mLength,aSet);
  aDest.mLength=aNewLen;
  NSSTR_SEEN(aDest);
}

void nsStrPrivate::CompressSet2(nsStr& aDest,const char* aSet,PRBool aEliminateLeading,PRBool aEliminateTrailing){
  NS_ASSERTION(aDest.GetCharSize() == eTwoByte, "Must be 2 bytes");
  
  Trim(aDest,aSet,aEliminateLeading,aEliminateTrailing);
  PRUint32 aNewLen=CompressChars2(aDest.mUStr,aDest.mLength,aSet);
  aDest.mLength=aNewLen;
  NSSTR_SEEN(aDest);
}


/**
 * 
 * @update	gess1/7/99
 * @param 
 * @return
 */
void nsStrPrivate::StripChars1(nsStr& aDest,const char* aSet){
  NS_ASSERTION(aDest.GetCharSize() == eOneByte, "Must be 1 byte");
  
  if((0<aDest.mLength) && (aSet)) {
    PRUint32 aNewLen=::StripChars1(aDest.mStr, aDest.mLength, aSet);
    aDest.mLength=aNewLen;
    NSSTR_SEEN(aDest);
  }
}

void nsStrPrivate::StripChars2(nsStr& aDest,const char* aSet){
  NS_ASSERTION(aDest.GetCharSize() == eTwoByte, "Must be 2 bytes");
  
  if((0<aDest.mLength) && (aSet)) {
    PRUint32 aNewLen=::StripChars2(aDest.mUStr, aDest.mLength, aSet);
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

PRInt32 nsStrPrivate::FindSubstr1in1(const nsStr& aDest,const nsStr& aTarget, PRBool aIgnoreCase,PRInt32 anOffset,PRInt32 aCount) {

  NS_ASSERTION(aDest.GetCharSize() == eOneByte, "Must be 1 byte");
  NS_ASSERTION(aTarget.GetCharSize() == eOneByte, "Must be 1 byte");
  
  PRInt32 theMaxPos = aDest.mLength-aTarget.mLength;  //this is the last pos that is feasible for starting the search, with given lengths...

  if (theMaxPos<0) return kNotFound;

  if(anOffset<0)
    anOffset=0;

  if((aDest.mLength<=0) || (anOffset>theMaxPos) || (aTarget.mLength==0))
    return kNotFound;
  
  if(aCount<0)
    aCount = MaxInt(theMaxPos,1);

  if (aCount <= 0)
    return kNotFound;

  const char* root  = aDest.mStr; 
  const char* left  = root + anOffset;
  const char* last  = left + aCount;
  const char* max   = root + theMaxPos;
  
  const char* right = (last<max) ? last : max;

  
  while(left<=right){
    PRInt32 cmp=Compare1To1(left,aTarget.mStr,aTarget.mLength,aIgnoreCase);
    if(0==cmp) {
      return (left-root);
    }
    left++;
  } //while

  return kNotFound;
}

PRInt32 nsStrPrivate::FindSubstr1in2(const nsStr& aDest,const nsStr& aTarget, PRBool aIgnoreCase,PRInt32 anOffset,PRInt32 aCount) {
  
  NS_ASSERTION(aDest.GetCharSize() == eTwoByte, "Must be 2 byte");
  NS_ASSERTION(aTarget.GetCharSize() == eOneByte, "Must be 1 byte");
  
  PRInt32 theMaxPos = aDest.mLength-aTarget.mLength;  //this is the last pos that is feasible for starting the search, with given lengths...

  if (theMaxPos<0) return kNotFound;

  if(anOffset<0)
    anOffset=0;

  if((aDest.mLength<=0) || (anOffset>theMaxPos) || (aTarget.mLength==0))
    return kNotFound;
  
  if(aCount<0)
    aCount = MaxInt(theMaxPos,1);

  if (aCount <= 0)
    return kNotFound;

  const PRUnichar* root  = aDest.mUStr; 
  const PRUnichar* left  = root+anOffset;
  const PRUnichar* last  = left+aCount;
  const PRUnichar* max   = root+theMaxPos;
  const PRUnichar* right = (last<max) ? last : max;

  
  while(left<=right){
    PRInt32 cmp=Compare2To1(left,aTarget.mStr,aTarget.mLength,aIgnoreCase);
    if(0==cmp) {
      return (left-root);
    }
    left++;
  } //while


  return kNotFound;
}

PRInt32 nsStrPrivate::FindSubstr2in2(const nsStr& aDest,const nsStr& aTarget, PRInt32 anOffset,PRInt32 aCount) {
  
  NS_ASSERTION(aDest.GetCharSize() == eTwoByte, "Must be 2 byte");
  NS_ASSERTION(aTarget.GetCharSize() == eTwoByte, "Must be 2 byte");
  
  PRInt32 theMaxPos = aDest.mLength-aTarget.mLength;  //this is the last pos that is feasible for starting the search, with given lengths...

  if (theMaxPos<0) return kNotFound;

  if(anOffset<0)
    anOffset=0;

  if((aDest.mLength<=0) || (anOffset>theMaxPos) || (aTarget.mLength==0))
    return kNotFound;
  
  if(aCount<0)
    aCount = MaxInt(theMaxPos,1);

  if (aCount <= 0)
    return kNotFound;

  const PRUnichar* root  = aDest.mUStr; 
  const PRUnichar* left  = root+anOffset;
  const PRUnichar* last  = left+aCount;
  const PRUnichar* max   = root+theMaxPos;
  const PRUnichar* right = (last<max) ? last : max;
  
  while(left<=right){
    PRInt32 cmp=Compare2To2(left,aTarget.mUStr,aTarget.mLength);
    if(0==cmp) {
      return (left-root);
    }
    left++;
  } //while


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

PRInt32 nsStrPrivate::FindChar1(const nsStr& aDest,PRUnichar aChar, PRInt32 anOffset,PRInt32 aCount) {
  NS_ASSERTION(aDest.GetCharSize() == eOneByte, "Must be 1 byte");
  return ::FindChar1(aDest.mStr,aDest.mLength,anOffset,aChar,aCount);
}

PRInt32 nsStrPrivate::FindChar2(const nsStr& aDest,PRUnichar aChar, PRInt32 anOffset,PRInt32 aCount) {
  NS_ASSERTION(aDest.GetCharSize() == eTwoByte, "Must be 2 byte");
  return ::FindChar2(aDest.mUStr,aDest.mLength,anOffset,aChar,aCount);
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
PRInt32 nsStrPrivate::RFindSubstr1in1(const nsStr& aDest,const nsStr& aTarget,PRBool aIgnoreCase,PRInt32 anOffset,PRInt32 aCount) {

  NS_ASSERTION(aDest.GetCharSize() == eOneByte, "Must be 1 byte");
  NS_ASSERTION(aTarget.GetCharSize() == eOneByte, "Must be 1 byte");
  
  if(anOffset<0)
    anOffset=(PRInt32)aDest.mLength-1;

  if(aCount<0)
    aCount = aDest.mLength;

  if ((aDest.mLength <= 0) || (PRUint32(anOffset)>=aDest.mLength) || (aTarget.mLength==0))
    return kNotFound;
  
  if (aCount<=0)
    return kNotFound;
  
  const char* root      = aDest.mStr;
  const char* destLast  = root+aDest.mLength; //pts to last char in aDest (likely null)
  
  const char* rightmost = root+anOffset;
  const char* min       = rightmost-aCount + 1;
  
  const char* leftmost  = (min<root) ? root: min;

  while(leftmost<=rightmost) {
    //don't forget to divide by delta in next text (bug found by rhp)...
    if(aTarget.mLength<=PRUint32(destLast-rightmost)) {
      PRInt32 result=Compare1To1(rightmost,aTarget.mStr,aTarget.mLength,aIgnoreCase);
      
      if(0==result) {
        return (rightmost-root);
      }
    } //if
    rightmost--;
  } //while

  return kNotFound;
}

PRInt32 nsStrPrivate::RFindSubstr1in2(const nsStr& aDest,const nsStr& aTarget,PRBool aIgnoreCase,PRInt32 anOffset,PRInt32 aCount) {

  NS_ASSERTION(aDest.GetCharSize() == eTwoByte, "Must be 2 byte");
  NS_ASSERTION(aTarget.GetCharSize() == eOneByte, "Must be 1 byte");
  
  if(anOffset<0)
    anOffset=(PRInt32)aDest.mLength-1;

  if(aCount<0)
    aCount = aDest.mLength;

  if ((aDest.mLength <= 0) || (PRUint32(anOffset)>=aDest.mLength) || (aTarget.mLength==0))
    return kNotFound;
  
  if (aCount<=0)
    return kNotFound;
  
  const PRUnichar* root      = aDest.mUStr;
  const PRUnichar* destLast  = root+aDest.mLength; //pts to last char in aDest (likely null)
  
  const PRUnichar* rightmost = root+anOffset;
  const PRUnichar* min       = rightmost-aCount+1;
  
  const PRUnichar* leftmost  = (min<root) ? root: min;

  while(leftmost<=rightmost) {
    //don't forget to divide by delta in next text (bug found by rhp)...
    if(aTarget.mLength<=PRUint32(destLast-rightmost)) {
      PRInt32 result = Compare2To1(rightmost,aTarget.mStr,aTarget.mLength,aIgnoreCase);
      
      if(0==result) {
        return rightmost-root;
      }
    } //if
    rightmost--;
  } //while

  return kNotFound;
}

PRInt32 nsStrPrivate::RFindSubstr2in2(const nsStr& aDest,const nsStr& aTarget,PRInt32 anOffset,PRInt32 aCount) {

  NS_ASSERTION(aDest.GetCharSize() == eTwoByte, "Must be 2 byte");
  NS_ASSERTION(aTarget.GetCharSize() == eTwoByte, "Must be 2 byte");
  
  if(anOffset<0)
    anOffset=(PRInt32)aDest.mLength-1;

  if(aCount<0)
    aCount = aDest.mLength;

  if ((aDest.mLength <= 0) || (PRUint32(anOffset)>=aDest.mLength) || (aTarget.mLength==0))
    return kNotFound;
  
  if (aCount<=0)
    return kNotFound;
  
  const PRUnichar* root      = aDest.mUStr;
  const PRUnichar* destLast  = root+aDest.mLength; //pts to last char in aDest (likely null)
  
  const PRUnichar* rightmost = root+anOffset;
  const PRUnichar* min       = rightmost-aCount+1;
  
  const PRUnichar* leftmost  = (min<root) ? root: min;

  while(leftmost<=rightmost) {
    //don't forget to divide by delta in next text (bug found by rhp)...
    if(aTarget.mLength<=PRUint32(destLast-rightmost)) {
      PRInt32 result = Compare2To2(rightmost,aTarget.mUStr,aTarget.mLength);
      
      if(0==result) {
        return (rightmost-root);
      }
    } //if
    rightmost--;
  } //while

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
PRInt32 nsStrPrivate::RFindChar1(const nsStr& aDest,PRUnichar aChar, PRInt32 anOffset,PRInt32 aCount) {
  NS_ASSERTION(aDest.GetCharSize() == eOneByte, "Must be 1 bytes");
  return ::RFindChar1(aDest.mStr,aDest.mLength,anOffset,aChar,aCount);
}
PRInt32 nsStrPrivate::RFindChar2(const nsStr& aDest,PRUnichar aChar, PRInt32 anOffset,PRInt32 aCount) {
  NS_ASSERTION(aDest.GetCharSize() == eTwoByte, "Must be 2 bytes");
  return ::RFindChar2(aDest.mUStr,aDest.mLength,anOffset,aChar,aCount);
}

// from the start of the old nsStrPrivate::StrCompare - now used as helper
// routines for nsStrPrivate::Compare1To1 and so forth
static inline PRInt32
GetCompareCount(const PRInt32 aDestLength, const PRInt32 aSourceLength,
                PRInt32 aCount)
{
  PRInt32 minlen=(aSourceLength<aDestLength) ? aSourceLength : aDestLength;

  if(0==minlen) {
    if ((aDestLength == 0) && (aSourceLength == 0))
      return 0;
    if (aDestLength == 0)
      return -1;
    return 1;
  }

  PRInt32 theCount = (aCount<0) ? minlen: MinInt(aCount,minlen);
    
  return theCount;
}

static inline PRInt32
TranslateCompareResult(const PRInt32 aDestLength, const PRInt32& aSourceLength, PRInt32 result, PRInt32 aCount)
{

  if (0==result && (-1==aCount ||
                    aDestLength < aCount ||
                    aSourceLength < aCount)) {

    //Since the caller didn't give us a length to test, or strings shorter
    //than aCount, and minlen characters matched, we have to assume that the
    //longer string is greater.

    if (aDestLength != aSourceLength) {
      //we think they match, but we've only compared minlen characters. 
      //if the string lengths are different, then they don't really match.
      result = (aDestLength<aSourceLength) ? -1 : 1;
    }
  }
  
  return result;
}

/**
 * Compare source and dest strings, up to an (optional max) number of chars
 * @param   aDest is the first str to compare
 * @param   aSource is the second str to compare
 * @param   aCount -- if (-1), then we use length of longer string; if (0<aCount) then it gives the max # of chars to compare
 * @param   aIgnorecase tells us whether to search with case sensitivity
 * @return  aDest<aSource=-1;aDest==aSource==0;aDest>aSource=1
 */
PRInt32 nsStrPrivate::StrCompare1To1(const nsStr& aDest,const nsStr& aSource,PRInt32 aCount,PRBool aIgnoreCase) {
  NS_ASSERTION(aDest.GetCharSize() == eOneByte, "Must be 1 byte");
  NS_ASSERTION(aSource.GetCharSize() == eOneByte, "Must be 1 byte");
  if (aCount) {
    PRInt32 theCount = GetCompareCount(aDest.mLength, aSource.mLength, aCount);
    PRInt32 result = Compare1To1(aDest.mStr, aSource.mStr, theCount, aIgnoreCase);
    result = TranslateCompareResult(aDest.mLength, aSource.mLength, result, aCount);
    return result;
  }
  
  return 0;
}

PRInt32 nsStrPrivate::StrCompare2To1(const nsStr& aDest,const nsStr& aSource,PRInt32 aCount,PRBool aIgnoreCase) {
  NS_ASSERTION(aDest.GetCharSize() == eTwoByte, "Must be 2 byte");
  NS_ASSERTION(aSource.GetCharSize() == eOneByte, "Must be 1 byte");
  
  if (aCount) {
    PRInt32 theCount = GetCompareCount(aDest.mLength, aSource.mLength, aCount);
    PRInt32 result = Compare2To1(aDest.mUStr, aSource.mStr, theCount, aIgnoreCase);
    result = TranslateCompareResult(aDest.mLength, aSource.mLength, result, aCount);
    return result;
  }
  
  return 0;
}

PRInt32 nsStrPrivate::StrCompare2To2(const nsStr& aDest,const nsStr& aSource,PRInt32 aCount) {
  NS_ASSERTION(aDest.GetCharSize() == eTwoByte, "Must be 2 byte");
  NS_ASSERTION(aSource.GetCharSize() == eTwoByte, "Must be 2 byte");
  
  if (aCount) {
    PRInt32 theCount = GetCompareCount(aDest.mLength, aSource.mLength, aCount);
    PRInt32 result = Compare2To2(aDest.mUStr, aSource.mUStr, theCount);
    result = TranslateCompareResult(aDest.mLength, aSource.mLength, result, aCount);
    return result;
  }
  
  return 0;
}

/**
 * Overwrites the contents of dest at offset with contents of aSource
 * 
 * @param   aDest is the first str to compare
 * @param   aSource is the second str to compare
 * @param   aDestOffset is the offset within aDest where source should be copied
 * @return  error code
 */
void nsStrPrivate::Overwrite(nsStr& aDest,const nsStr& aSource,PRInt32 aDestOffset) {
  if(aDest.mLength && aSource.mLength) {
    if((aDest.mLength-aDestOffset)>=aSource.mLength) {
      //if you're here, then both dest and source have valid lengths
      //and there's enough room in dest (at offset) to contain source.
      (*gCopyChars[aSource.GetCharSize()][aDest.GetCharSize()])(aDest.mStr,aDestOffset,aSource.mStr,0,aSource.mLength);      
    }
  }
}

//----------------------------------------------------------------------------------------
// allocate the given bytes, not including the null terminator
PRBool nsStrPrivate::Alloc(nsStr& aDest,PRUint32 aCount) {

  // the new strategy is, allocate exact size, double on grows
  aDest.SetInternalCapacity(aCount);
  aDest.mStr = (char*)nsMemory::Alloc((aCount+1)<<aDest.GetCharSize());

  if(aDest.mStr)
    aDest.SetOwnsBuffer(PR_TRUE);
  
#ifdef NS_STR_STATS
  gStringAcquiredMemory = (aDest.mStr != nsnull);
#endif
  
  return (aDest.mStr != nsnull);
}

PRBool nsStrPrivate::Free(nsStr& aDest){
  if(aDest.mStr){
    if(aDest.GetOwnsBuffer()){
      nsMemory::Free(aDest.mStr);
    }
    aDest.mStr=0;
    aDest.SetOwnsBuffer(PR_FALSE);
    return PR_TRUE;
  }
  return PR_FALSE;
}

PRBool nsStrPrivate::Realloc(nsStr& aDest,PRUint32 aCount){

  nsStr temp;
  memcpy(&temp,&aDest,sizeof(aDest));

  PRBool result=Alloc(temp,aCount);
  if(result) {
    Free(aDest);
    aDest.mStr=temp.mStr;
    aDest.SetInternalCapacity(temp.GetCapacity());
    aDest.SetOwnsBuffer(temp.GetOwnsBuffer());
  }
  return result;
}

#ifdef NS_STR_STATS
/**
 * Retrieve last memory error
 *
 * @update  gess 10/11/99
 * @return  memory error (usually returns PR_TRUE)
 */
PRBool nsStrPrivate::DidAcquireMemory(void) {
  return gStringAcquiredMemory;
}
#endif

//----------------------------------------------------------------------------------------

CBufDescriptor::CBufDescriptor(char* aString,PRBool aStackBased,PRUint32 aCapacity,PRInt32 aLength) { 
  mBuffer=aString;
  mCharSize=eOneByte;
  mStackBased=aStackBased;
  mIsConst=PR_FALSE;
  mLength=mCapacity=0;
  if(aString && aCapacity>1) {
    mCapacity=aCapacity-1;
    mLength=(-1==aLength) ? nsCharTraits<char>::length(aString) : aLength;
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
    mLength=(-1==aLength) ? nsCharTraits<char>::length(aString) : aLength;
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
    mLength=(-1==aLength) ? nsCharTraits<PRUnichar>::length(aString) : aLength;
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
    mLength=(-1==aLength) ? nsCharTraits<PRUnichar>::length(aString) : aLength;
    if(mLength>PRInt32(mCapacity))
      mLength=mCapacity;
  }
}

//----------------------------------------------------------------------------------------

PRUint32
nsStrPrivate::HashCode(const nsStr& aDest)
{
  PRUint32 h = 0;
  if (aDest.GetCharSize() == eTwoByte) {
    const PRUnichar* s = aDest.mUStr;

    if (!s) return h;
    
    PRUnichar c;
    while ( (c = *s++) )
      h = (h>>28) ^ (h<<4) ^ c;
    return h;
  } else {
    const char* s = aDest.mStr;
      
    if (!s) return h;
      
    unsigned char c;
    while ( (c = *s++) )
      h = (h>>28) ^ (h<<4) ^ c;
    return h;
  }
}

/**
 * This is a copy of |PR_cnvtf| with a bug fixed.  (The second argument
 * of PR_dtoa is 2 rather than 1.)
 */
void 
nsStrPrivate::cnvtf(char *buf, int bufsz, int prcsn, double fval)
{
  PRIntn decpt, sign, numdigits;
  char *num, *nump;
  char *bufp = buf;
  char *endnum;

  /* If anything fails, we store an empty string in 'buf' */
  num = (char*)PR_MALLOC(bufsz);
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
  PR_DELETE(num);
}

#ifdef NS_STR_STATS

#include <ctype.h>

#ifdef XP_MAC
#define isascii(c)      ((unsigned)(c) < 0x80)
#endif

void
nsStrPrivate::Print(const nsStr& aDest, FILE* out, PRBool truncate)
{
  PRInt32 printLen = (PRInt32)aDest.mLength;

  if (aDest.GetCharSize() == eOneByte) {
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
  nsStrPrivate::Initialize(mStr, str.GetCharSize());
  nsStrPrivate::StrAssign(mStr, str, 0, -1);
//  nsStrPrivate::Print(mStr, stdout);
//  fputc('\n', stdout);
}

PR_EXTERN(PRHashNumber)
nsStr_Hash(const void* key)
{
  nsStr* str = (nsStr*)key;
  return nsStrPrivate::HashCode(*str);
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
  nsStrPrivate::Print(entry->mStr, out, PR_TRUE);
  fputc('\n', out);
  return HT_ENUMERATE_NEXT;
}

#endif // NS_STR_STATS

////////////////////////////////////////////////////////////////////////////////
