/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/*
 * The contents of this file are subject to the Netscape Public License
 * Version 1.0 (the "NPL"); you may not use this file except in
 * compliance with the NPL.  You may obtain a copy of the NPL at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the NPL is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the NPL
 * for the specific language governing rights and limitations under the
 * NPL.
 *
 * The Initial Developer of this code under the NPL is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation.  All Rights
 * Reserved.
 */

/******************************************************************************************
  MODULE NOTES:

  This file contains the nsStr data structure.
  This general purpose buffer management class is used as the basis for our strings.
  It's benefits include:
    1. An efficient set of library style functions for manipulating nsStrs
    2. Support for 1 and 2 byte character strings (which can easily be increased to n)
    3. Unicode awareness and interoperability.

*******************************************************************************************/

#include "nsStr.h"
#include "bufferRoutines.h"
#include "stdio.h"  //only used for printf
#include "nsDeque.h"
#include "nsCRT.h"


static const char* kFoolMsg = "Error: Some fool overwrote the shared buffer.";

//----------------------------------------------------------------------------------------
//  The following is a memory agent who knows how to recycled (pool) freed memory...
//----------------------------------------------------------------------------------------

/**************************************************************
  Define the char* (pooled) deallocator class...
 **************************************************************/
class nsBufferDeallocator: public nsDequeFunctor{
public:
  virtual void* operator()(void* anObject) {
    char* aCString= (char*)anObject;
    delete [] aCString;
    return 0;
  }
};

/**
 * 
 * @update	gess10/30/98
 * @param 
 * @return
 */
class nsPoolingMemoryAgent : public nsMemoryAgent{
public:
  nsPoolingMemoryAgent()  {
    memset(mPools,0,sizeof(mPools));
  }

  ~nsPoolingMemoryAgent() {
    nsBufferDeallocator theDeallocator;
    int i=0;
    for(i=0;i<10;i++){
      if(mPools[i]){
        mPools[i]->ForEach(theDeallocator); //now delete the buffers
      }
      delete mPools[i];
      mPools[i]=0;
    }
  }

  virtual PRBool Alloc(nsStr& aDest,PRInt32 aCount) {
    
    //we're given the acount value in charunits; we have to scale up by the charsize.
    int     theShift=4;
    PRInt32 theNewCapacity=eDefaultSize;
    while(theNewCapacity<aCount){ 
      theNewCapacity<<=1;
      theShift++;
    } 
    
    aDest.mCapacity=theNewCapacity++;
    theShift=(theShift<<aDest.mMultibyte)-4;
    if((theShift<12) && (mPools[theShift])){
      aDest.mStr=(char*)mPools[theShift]->Pop();
    }
    if(!aDest.mStr) {
      //we're given the acount value in charunits; we have to scale up by the charsize.
      size_t theSize=(theNewCapacity<<aDest.mMultibyte);
      aDest.mStr=new char[theSize];    
    }
    aDest.mOwnsBuffer=1;
    return PR_TRUE;
    
  }

  virtual PRBool Free(nsStr& aDest){
    if(aDest.mStr){
      if(aDest.mOwnsBuffer){
        int theShift=1;
        unsigned int theValue=1;
        while((theValue<<=1)<aDest.mCapacity){ 
          theShift++;
        }
        theShift-=4;
        if(theShift<12){
          if(!mPools[theShift]){
            mPools[theShift]=new nsDeque(0);
          }
          mPools[theShift]->Push(aDest.mStr);
        }
        else delete [] aDest.mStr; //it's too big. Just delete it.
      }
      aDest.mStr=0;
      aDest.mOwnsBuffer=0;
      return PR_TRUE;
    }
    return PR_FALSE;
  }
  nsDeque* mPools[16]; 
};


/**
 * 
 * @update	gess10/30/98
 * @param 
 * @return
 */
char* GetSharedEmptyBuffer() {
  static char* gCommonEmptyBuffer=0;
  if(!gCommonEmptyBuffer) {
    const size_t theDfltSize=25;
    gCommonEmptyBuffer=new char[theDfltSize];
    if(gCommonEmptyBuffer){
      nsCRT::zero(gCommonEmptyBuffer,theDfltSize);
      gCommonEmptyBuffer[0]=0;
    }
    else {
      printf("%s\n","Memory allocation error!");
    }
  }
  return gCommonEmptyBuffer;
}

/**
 * 
 * @update	gess10/30/98
 * @param 
 * @return
 */
void nsStr::Initialize(nsStr& aDest,eCharSize aCharSize) {
  aDest.mStr=GetSharedEmptyBuffer();
  aDest.mLength=0;
  aDest.mCapacity=0;
  aDest.mMultibyte=aCharSize;
  aDest.mOwnsBuffer=0;
  aDest.mUnused=0;
  NS_ASSERTION(aDest.mStr[0]==0,kFoolMsg);
}

/**
 * 
 * @update	gess10/30/98
 * @param 
 * @return
 */
void nsStr::Initialize(nsStr& aDest,char* aCString,PRUint32 aCapacity,PRUint32 aLength,eCharSize aCharSize,PRBool aOwnsBuffer){
  aDest.mStr=(aCString) ? aCString : GetSharedEmptyBuffer();
  aDest.mLength=aLength;
  aDest.mCapacity=aCapacity;
  aDest.mMultibyte=aCharSize;
  aDest.mOwnsBuffer=aOwnsBuffer;
  aDest.mUnused=0;
}

/**
 * 
 * @update	gess10/30/98
 * @param 
 * @return
 */
nsIMemoryAgent* GetDefaultAgent(void){
//  static nsPoolingMemoryAgent gDefaultAgent;
  static nsMemoryAgent gDefaultAgent;
  return (nsIMemoryAgent*)&gDefaultAgent;
}

/**
 * 
 * @update	gess10/30/98
 * @param 
 * @return
 */
void nsStr::Destroy(nsStr& aDest,nsIMemoryAgent* anAgent) {
  if((aDest.mStr) && (aDest.mStr!=GetSharedEmptyBuffer())) {
    if(!anAgent)
      anAgent=GetDefaultAgent();
    
    if(anAgent) {
      anAgent->Free(aDest);
    }
    else{
      printf("%s\n","Leak occured in nsStr.");
    }
  }
}


/**
 * This method gets called when the internal buffer needs
 * to grow to a given size. The original contents are not preserved.
 * @update  gess 3/30/98
 * @param   aNewLength -- new capacity of string in charSize units
 * @return  void
 */
void nsStr::EnsureCapacity(nsStr& aString,PRUint32 aNewLength,nsIMemoryAgent* anAgent) {
  if(aNewLength>aString.mCapacity) {
    nsIMemoryAgent* theAgent=(anAgent) ? anAgent : GetDefaultAgent();
    theAgent->Realloc(aString,aNewLength);
    AddNullTerminator(aString);
  }
}

/**
 * This method gets called when the internal buffer needs
 * to grow to a given size. The original contents ARE preserved.
 * @update  gess 3/30/98
 * @param   aNewLength -- new capacity of string in charSize units
 * @return  void
 */
void nsStr::GrowCapacity(nsStr& aDest,PRUint32 aNewLength,nsIMemoryAgent* anAgent) {
  if(aNewLength>aDest.mCapacity) {
    nsStr theTempStr;
    nsStr::Initialize(theTempStr,(eCharSize)aDest.mMultibyte);

    nsIMemoryAgent* theAgent=(anAgent) ? anAgent : GetDefaultAgent();
    EnsureCapacity(theTempStr,aNewLength,theAgent);

    if(aDest.mLength) {
      Append(theTempStr,aDest,0,aDest.mLength,theAgent);        
    } 
    theAgent->Free(aDest);
    aDest.mStr = theTempStr.mStr;
    theTempStr.mStr=0; //make sure to null this out so that you don't lose the buffer you just stole...
    aDest.mLength=theTempStr.mLength;
    aDest.mCapacity=theTempStr.mCapacity;
    aDest.mOwnsBuffer=theTempStr.mOwnsBuffer;
  }
}

/**
 * Replaces the contents of aDest with aSource, up to aCount of chars.
 * @update	gess10/30/98
 * @param   aDest is the nsStr that gets changed.
 * @param   aSource is where chars are copied from
 * @param   aCount is the number of chars copied from aSource
 */
void nsStr::Assign(nsStr& aDest,const nsStr& aSource,PRUint32 anOffset,PRInt32 aCount,nsIMemoryAgent* anAgent){
  Truncate(aDest,0,anAgent);
  Append(aDest,aSource,anOffset,aCount,anAgent);
}

/**
 * This method appends the given nsStr to this one. Note that we have to 
 * pay attention to the underlying char-size of both structs.
 * @update	gess10/30/98
 * @param   aDest is the nsStr to be manipulated
 * @param   aSource is where char are copied from
 * @aCount  is the number of bytes to be copied 
 */
void nsStr::Append(nsStr& aDest,const nsStr& aSource,PRUint32 anOffset,PRInt32 aCount,nsIMemoryAgent* anAgent){
  if(anOffset<aSource.mLength){
    PRUint32 theRealLen=(aCount<0) ? aSource.mLength : MinInt(aCount,aSource.mLength);
    PRUint32 theLength=(anOffset+theRealLen<aSource.mLength) ? theRealLen : (aSource.mLength-anOffset);
    if(0<theLength){
      if(aDest.mLength+theLength > aDest.mCapacity) {
        GrowCapacity(aDest,aDest.mLength+theLength,anAgent);
      }

      //now append new chars, starting at offset
      (*gCopyChars[aSource.mMultibyte][aDest.mMultibyte])(aDest.mStr,aDest.mLength,aSource.mStr,anOffset,theLength);

      aDest.mLength+=theLength;
      AddNullTerminator(aDest);
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
void nsStr::Insert( nsStr& aDest,PRUint32 aDestOffset,const nsStr& aSource,PRUint32 aSrcOffset,PRInt32 aCount,nsIMemoryAgent* anAgent){
  //there are a few cases for insert:
  //  1. You're inserting chars into an empty string (assign)
  //  2. You're inserting onto the end of a string (append)
  //  3. You're inserting onto the 1..n-1 pos of a string (the hard case).
  if(0<aSource.mLength){
    ToRange(aDestOffset,0,aDest.mLength);
    if(aDest.mLength){
      if(aDestOffset<aDest.mLength){
        //ToRange(aSrcOffset,0,aSource.mLength-1);
        PRInt32 theRealLen=(aCount<0) ? aSource.mLength : MinInt(aCount,aSource.mLength);
        PRInt32 theLength=(aSrcOffset+theRealLen<aSource.mLength) ? theRealLen : (aSource.mLength-aSrcOffset);

        if(aSrcOffset<aSource.mLength) {
            //here's the only new case we have to handle. 
            //chars are really being inserted into our buffer...
          GrowCapacity(aDest,aDest.mLength+theLength,anAgent);

            //shift the chars right by theDelta...
          (*gShiftChars[aDest.mMultibyte][PR_TRUE])(aDest.mStr,aDest.mLength,aDestOffset,theLength);
      
          //now insert new chars, starting at offset
          (*gCopyChars[aSource.mMultibyte][aDest.mMultibyte])(aDest.mStr,aDestOffset,aSource.mStr,aSrcOffset,theLength);

          //finally, make sure to update the string length...
          aDest.mLength+=theLength;

        }//if
        //else nothing to do!
      }
      else Append(aDest,aSource,0,aCount,anAgent);
    }
    else Append(aDest,aSource,0,aCount,anAgent);
  }
}


/**
 * This method deletes up to aCount chars from aDest
 * @update	gess10/30/98
 * @param   aDest is the nsStr to be manipulated
 * @param   aDestOffset is where in aDest deletion is to occur
 * @param   aCount is the number of chars to be deleted in aDest
 */
void nsStr::Delete(nsStr& aDest,PRUint32 aDestOffset,PRUint32 aCount,nsIMemoryAgent* anAgent){
  if(aDestOffset<aDest.mLength){

    PRUint32 theDelta=aDest.mLength-aDestOffset;
    PRUint32 theLength=(theDelta<aCount) ? theDelta : aCount;

    if(aDestOffset+theLength<aDest.mLength) {

      //if you're here, it means we're cutting chars out of the middle of the string...
      //so shift the chars left by theLength...
      (*gShiftChars[aDest.mMultibyte][PR_FALSE])(aDest.mStr,aDest.mLength,aDestOffset,theLength);
      aDest.mLength-=theLength;
    }
    else Truncate(aDest,aDestOffset,anAgent);
  }//if
}

/**
 * This method truncates the given nsStr at given offset
 * @update	gess10/30/98
 * @param   aDest is the nsStr to be truncated
 * @param   aDestOffset is where in aDest truncation is to occur
 */
void nsStr::Truncate(nsStr& aDest,PRUint32 aDestOffset,nsIMemoryAgent* anAgent){
  if(aDestOffset<aDest.mLength){
    aDest.mLength=aDestOffset;
    AddNullTerminator(aDest);
  }
}


/**
 * 
 * @update	gess1/7/99
 * @param 
 * @return
 */
void nsStr::ChangeCase(nsStr& aDest,PRBool aToUpper) {
  // somehow UnicharUtil return failed, fallback to the old ascii only code
  gCaseConverters[aDest.mMultibyte](aDest.mStr,aDest.mLength,aToUpper);
}

/**
 * 
 * @update	gess1/7/99
 * @param 
 * @return
 */
void nsStr::StripChars(nsStr& aDest,PRUint32 aDestOffset,PRInt32 aCount,const char* aCharSet){
  PRUint32 aNewLen=gStripChars[aDest.mMultibyte](aDest.mStr,aDestOffset,aCount,aCharSet);
  aDest.mLength=aNewLen;
}


/**
 * 
 * @update	gess1/7/99
 * @param 
 * @return
 */
void nsStr::Trim(nsStr& aDest,const char* aSet,PRBool aEliminateLeading,PRBool aEliminateTrailing){
  PRUint32 aNewLen=gTrimChars[aDest.mMultibyte](aDest.mStr,aDest.mLength,aSet,aEliminateLeading,aEliminateTrailing);
  aDest.mLength=aNewLen;
}

/**
 * 
 * @update	gess1/7/99
 * @param 
 * @return
 */
void nsStr::CompressSet(nsStr& aDest,const char* aSet,PRUint32 aChar,PRBool aEliminateLeading,PRBool aEliminateTrailing){
  PRUint32 aNewLen=gCompressChars[aDest.mMultibyte](aDest.mStr,aDest.mLength,aSet,aChar,aEliminateLeading,aEliminateTrailing);
  aDest.mLength=aNewLen;
}

  /**************************************************************
    Searching methods...
   **************************************************************/


PRInt32 nsStr::FindSubstr(const nsStr& aDest,const nsStr& aTarget, PRBool aIgnoreCase,PRUint32 anOffset) {
  PRInt32 index=anOffset-1;
  PRInt32 theMax=aDest.mLength-aTarget.mLength;
  if((aDest.mLength>0) && (aTarget.mLength>0)){
    PRInt32 theNewStartPos=-1;
    PRUnichar theFirstTargetChar=GetCharAt(aTarget,0);
    PRUnichar theLastTargetChar=GetCharAt(aTarget,aTarget.mLength-1);
    PRInt32   theTargetMax=aTarget.mLength;
    while(++index<=theMax) {
      PRInt32 theSubIndex=-1;
      PRBool  matches=PR_TRUE;
      while((++theSubIndex<theTargetMax) && (matches)){
        PRUnichar theChar=GetCharAt(aDest,index+theSubIndex);
        if(theSubIndex>0) {
          if(theFirstTargetChar==theChar){
            PRUnichar theDestJumpChar=GetCharAt(aDest,index+theTargetMax);
            if(theDestJumpChar==theLastTargetChar) {
              theNewStartPos=index; //this lets us jump ahead during our search where possible.
            }//if
          }//if
        }//if
        PRUnichar theTargetChar=GetCharAt(aTarget,theSubIndex);
        matches=PRBool(theChar==theTargetChar);
      }
      if(matches)
        return index;
      if(-1<theNewStartPos){
        index=theNewStartPos-1;
      }
    }
  }//if
  return kNotFound;
}


/**
 * 
 * @update	gess1/7/99
 * @param 
 * @return
 */
PRInt32 nsStr::FindChar(const nsStr& aDest,const PRUnichar aChar, PRBool aIgnoreCase,PRUint32 anOffset) {
  PRInt32 result=gFindChars[aDest.mMultibyte](aDest.mStr,aDest.mLength,0,aChar,aIgnoreCase);
  return result;
}


/**
 *  
 *  
 *  @update  gess 3/25/98
 *  @param   
 *  @return  
 */
PRInt32 nsStr::FindCharInSet(const nsStr& aDest,const nsStr& aSet,PRBool aIgnoreCase,PRUint32 anOffset) {
  PRUint32 index=anOffset-1;
  PRInt32  thePos;

  while(++index<aDest.mLength) {
    PRUnichar theChar=GetCharAt(aDest,index);
    thePos=gFindChars[aSet.mMultibyte](aSet.mStr,aSet.mLength,0,theChar,aIgnoreCase);
    if(kNotFound!=thePos)
      return index;
  } //while
  return kNotFound;
}

  /**************************************************************
    Reverse Searching methods...
   **************************************************************/


PRInt32 nsStr::RFindSubstr(const nsStr& aDest,const nsStr& aTarget, PRBool aIgnoreCase,PRUint32 anOffset) {
  PRInt32 index=(anOffset ? anOffset : aDest.mLength-aTarget.mLength+1);
  if((aDest.mLength>0) && (aTarget.mLength>0)){
    PRInt32 theNewStartPos=-1;
    PRUnichar theFirstTargetChar=GetCharAt(aTarget,0);
    PRUnichar theLastTargetChar=GetCharAt(aTarget,aTarget.mLength-1);
    PRInt32   theTargetMax=aTarget.mLength;

    while(index--) {
      PRInt32 theSubIndex=-1;
      PRBool  matches=PR_TRUE;
    
      if(anOffset+aTarget.mLength<=aDest.mLength) {
        while((++theSubIndex<theTargetMax) && (matches)){
          PRUnichar theChar=GetCharAt(aDest,index+theSubIndex);
          if(theSubIndex>0) {
            if(theFirstTargetChar==theChar){
              PRUnichar theDestJumpChar=GetCharAt(aDest,index+theTargetMax);
              if(theDestJumpChar==theLastTargetChar) {
                theNewStartPos=index; //this lets us jump ahead during our search where possible.
              }//if
            }//if
          }//if
          PRUnichar theTargetChar=GetCharAt(aTarget,theSubIndex);
          matches=PRBool(theChar==theTargetChar);
        } //while
      } //if
      if(matches)
        return index;
      if(-1<theNewStartPos){
        index=theNewStartPos-1;
      }
    }
  }//if
  return kNotFound;
}


/**
 * 
 * @update	gess1/7/99
 * @param 
 * @return
 */
PRInt32 nsStr::RFindChar(const nsStr& aDest,const PRUnichar aChar, PRBool aIgnoreCase,PRUint32 anOffset) {
  PRInt32 result=gRFindChars[aDest.mMultibyte](aDest.mStr,aDest.mLength,0,aChar,aIgnoreCase);
  return result;
}

/**
 *  
 *  
 *  @update  gess 3/25/98
 *  @param   
 *  @return  
 */
PRInt32 nsStr::RFindCharInSet(const nsStr& aDest,const nsStr& aSet,PRBool aIgnoreCase,PRUint32 anOffset) {
  PRUint32 offset=aDest.mLength-anOffset;
  PRInt32  thePos;

  while(--offset>=0) {
    PRUnichar theChar=GetCharAt(aDest,offset);
    thePos=gRFindChars[aSet.mMultibyte](aSet.mStr,aSet.mLength,0,theChar,aIgnoreCase);
    if(kNotFound!=thePos)
      return offset;
  } //while
  return kNotFound;
}


/**
 * 
 * @update	gess11/12/98
 * @param 
 * @return  aDest<aSource=-1;aDest==aSource==0;aDest>aSource=1
 */
PRInt32 nsStr::Compare(const nsStr& aDest,const nsStr& aSource,PRInt32 aCount,PRBool aIgnoreCase) {
  int minlen=(aSource.mLength<aDest.mLength) ? aSource.mLength : aDest.mLength;

  if(0==minlen) {
    if ((aDest.mLength == 0) && (aSource.mLength == 0))
      return 0;
    if (aDest.mLength == 0)
      return -1;
    return 1;
  }

  int maxlen=(aSource.mLength<aDest.mLength) ? aDest.mLength : aSource.mLength;
  PRInt32 result=(*gCompare[aDest.mMultibyte][aSource.mMultibyte])(aDest.mStr,aSource.mStr,maxlen,aIgnoreCase);
  return result;
}

