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
#include "nsCRT.h"
#include "nsDeque.h"

 
static const char* kFoolMsg = "Error: Some fool overwrote the shared buffer.";
//static const char* kCallFindChar =  "For better performance, call FindChar() for targets whose length==1.";
//static const char* kCallRFindChar = "For better performance, call RFindChar() for targets whose length==1.";

//----------------------------------------------------------------------------------------
//  The following is a memory agent who knows how to recycled (pool) freed memory...
//----------------------------------------------------------------------------------------

/**************************************************************
  Define the char* (pooled) deallocator class...
 **************************************************************/

/*
class nsBufferDeallocator: public nsDequeFunctor{
public:
  virtual void* operator()(void* anObject) {
    char* aCString= (char*)anObject;
    delete [] aCString;
    return 0;
  }
};

class nsPoolingMemoryAgent : public nsMemoryAgent{
public:
  nsPoolingMemoryAgent()  {
    memset(mPools,0,sizeof(mPools));
  }

  virtual ~nsPoolingMemoryAgent() {
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

  virtual PRBool Alloc(nsStr& aDest,PRUint32 aCount) {
    
    //we're given the acount value in charunits; we have to scale up by the charsize.
    int       theShift=4;
    PRUint32  theNewCapacity=eDefaultSize;
    while(theNewCapacity<aCount){ 
      theNewCapacity<<=1;
      theShift++;
    } 
    
    aDest.mCapacity=theNewCapacity++;
    theShift=(theShift<<aDest.mCharSize)-4;
    if((theShift<12) && (mPools[theShift])){
      aDest.mStr=(char*)mPools[theShift]->Pop();
    }
    if(!aDest.mStr) {
      //we're given the acount value in charunits; we have to scale up by the charsize.
      size_t theSize=(theNewCapacity<<aDest.mCharSize);
      aDest.mStr=new char[theSize];    
    }
    aDest.mOwnsBuffer=1;
    return PRBool(aDest.mStr!=0);
    
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

*/

static char* gCommonEmptyBuffer=0;
/**
 * 
 * @update	gess10/30/98
 * @param 
 * @return
 */
char* GetSharedEmptyBuffer() {
  if(!gCommonEmptyBuffer) {
    const size_t theDfltSize=5;
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
 * This method initializes all the members of the nsStr structure 
 *
 * @update	gess10/30/98
 * @param 
 * @return
 */
void nsStr::Initialize(nsStr& aDest,eCharSize aCharSize) {
  aDest.mStr=GetSharedEmptyBuffer();
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
  aDest.mStr=(aCString) ? aCString : GetSharedEmptyBuffer();
  aDest.mLength=aLength;
  aDest.mCapacity=aCapacity;
  aDest.mCharSize=aCharSize;
  aDest.mOwnsBuffer=aOwnsBuffer;
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
 * This member destroys the memory buffer owned by an nsStr object (if it actually owns it)
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
    nsStr::Initialize(theTempStr,aDest.mCharSize);

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
  if(&aDest!=&aSource){
    Truncate(aDest,0,anAgent);
    Append(aDest,aSource,anOffset,aCount,anAgent);
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
void nsStr::Append(nsStr& aDest,const nsStr& aSource,PRUint32 anOffset,PRInt32 aCount,nsIMemoryAgent* anAgent){
  if(anOffset<aSource.mLength){
    PRUint32 theRealLen=(aCount<0) ? aSource.mLength : MinInt(aCount,aSource.mLength);
    PRUint32 theLength=(anOffset+theRealLen<aSource.mLength) ? theRealLen : (aSource.mLength-anOffset);
    if(0<theLength){
      if(aDest.mLength+theLength > aDest.mCapacity) {
        GrowCapacity(aDest,aDest.mLength+theLength,anAgent);
      }

      //now append new chars, starting at offset
      (*gCopyChars[aSource.mCharSize][aDest.mCharSize])(aDest.mStr,aDest.mLength,aSource.mStr,anOffset,theLength);

      aDest.mLength+=theLength;
    }
  }
  AddNullTerminator(aDest);
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
    if(aDest.mLength){
      if(aDestOffset<aDest.mLength){
        PRInt32 theRealLen=(aCount<0) ? aSource.mLength : MinInt(aCount,aSource.mLength);
        PRInt32 theLength=(aSrcOffset+theRealLen<aSource.mLength) ? theRealLen : (aSource.mLength-aSrcOffset);

        if(aSrcOffset<aSource.mLength) {
            //here's the only new case we have to handle. 
            //chars are really being inserted into our buffer...
          GrowCapacity(aDest,aDest.mLength+theLength,anAgent);

            //shift the chars right by theDelta...
          (*gShiftChars[aDest.mCharSize][KSHIFTRIGHT])(aDest.mStr,aDest.mLength,aDestOffset,theLength);
      
          //now insert new chars, starting at offset
          (*gCopyChars[aSource.mCharSize][aDest.mCharSize])(aDest.mStr,aDestOffset,aSource.mStr,aSrcOffset,theLength);

          //finally, make sure to update the string length...
          aDest.mLength+=theLength;
          AddNullTerminator(aDest);

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
      (*gShiftChars[aDest.mCharSize][KSHIFTLEFT])(aDest.mStr,aDest.mLength,aDestOffset,theLength);
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
 * 
 * @update	gess1/7/99
 * @param 
 * @return
 */
void nsStr::Trim(nsStr& aDest,const char* aSet,PRBool aEliminateLeading,PRBool aEliminateTrailing){

  if((aDest.mLength>0) && aSet){
    PRInt32 theIndex=-1;
    PRInt32 theMax=aDest.mLength;
    PRInt32 theSetLen=nsCRT::strlen(aSet);

    if(aEliminateLeading) {
      while(++theIndex<=theMax) {
        PRUnichar theChar=GetCharAt(aDest,theIndex);
        PRInt32 thePos=gFindChars[eOneByte](aSet,theSetLen,0,theChar,PR_FALSE);
        if(kNotFound==thePos)
          break;
      }
      if(0<theIndex) {
        if(theIndex<theMax) {
          Delete(aDest,0,theIndex,0);
        }
        else Truncate(aDest,0);
      }
    }

    if(aEliminateTrailing) {
      theIndex=aDest.mLength;
      PRInt32 theNewLen=theIndex;
      while(--theIndex>0) {
        PRUnichar theChar=GetCharAt(aDest,theIndex);  //read at end now...
        PRInt32 thePos=gFindChars[eOneByte](aSet,theSetLen,0,theChar,PR_FALSE);
        if(kNotFound<thePos) 
          theNewLen=theIndex;
        else break;
      }
      if(theNewLen<theMax) {
        Truncate(aDest,theNewLen);
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
}

  /**************************************************************
    Searching methods...
   **************************************************************/


/**
 *  This searches aDest for a given substring
 *  
 *  @update  gess 3/25/98
 *  @param   aDest string to search
 *  @param   aTarget is the substring you're trying to find.
 *  @param   aIgnorecase indicates case sensitivity of search
 *  @param   anOffset tells us where to start the search
 *  @return  index in aDest where member of aSet occurs, or -1 if not found
 */
PRInt32 nsStr::FindSubstr(const nsStr& aDest,const nsStr& aTarget, PRBool aIgnoreCase,PRInt32 anOffset) {

  PRInt32 result=kNotFound;
  
  if((0<aDest.mLength) && (anOffset<(PRInt32)aDest.mLength)) {
    PRInt32 theMax=aDest.mLength-aTarget.mLength;
    PRInt32 index=(0<=anOffset) ? anOffset : 0;
    
    if((aDest.mLength>=aTarget.mLength) && (aTarget.mLength>0) && (index>=0)){
      PRInt32 theTargetMax=aTarget.mLength;
      while(index<=theMax) {
        PRInt32 theSubIndex=-1;
        PRBool  matches=PR_TRUE;
        while((++theSubIndex<theTargetMax) && (matches)){
          PRUnichar theChar=(aIgnoreCase) ? nsCRT::ToLower(GetCharAt(aDest,index+theSubIndex)) : GetCharAt(aDest,index+theSubIndex);
          PRUnichar theTargetChar=(aIgnoreCase) ? nsCRT::ToLower(GetCharAt(aTarget,theSubIndex)) : GetCharAt(aTarget,theSubIndex);
          matches=PRBool(theChar==theTargetChar);
        }
        if(matches) { 
          result=index;
          break;
        }
        index++;
      } //while
    }//if
  }//if
  return result;
}


/**
 *  This searches aDest for a given character 
 *  
 *  @update  gess 3/25/98
 *  @param   aDest string to search
 *  @param   char is the character you're trying to find.
 *  @param   aIgnorecase indicates case sensitivity of search
 *  @param   anOffset tells us where to start the search
 *  @return  index in aDest where member of aSet occurs, or -1 if not found
 */
PRInt32 nsStr::FindChar(const nsStr& aDest,PRUnichar aChar, PRBool aIgnoreCase,PRInt32 anOffset) {
  PRInt32 result=kNotFound;
  if((0<aDest.mLength) && (anOffset<(PRInt32)aDest.mLength)) {
    PRUint32 index=(0<=anOffset) ? (PRUint32)anOffset : 0;
    result=gFindChars[aDest.mCharSize](aDest.mStr,aDest.mLength,index,aChar,aIgnoreCase);
  }
  return result;
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

  PRInt32 index=(0<=anOffset) ? anOffset-1 : -1;
  PRInt32 thePos;

    //Note that the search is inverted here. We're scanning aDest, one char at a time
    //but doing the search against the given set. That's why we use 0 as the offset below.
  if((0<aDest.mLength) && (0<aSet.mLength)){
    while(++index<(PRInt32)aDest.mLength) {
      PRUnichar theChar=GetCharAt(aDest,index);
      thePos=gFindChars[aSet.mCharSize](aSet.mStr,aSet.mLength,0,theChar,aIgnoreCase);
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
 *  @update  gess 3/25/98
 *  @param   aDest string to search
 *  @param   aTarget is the substring you're trying to find.
 *  @param   aIgnorecase indicates case sensitivity of search
 *  @param   anOffset tells us where to start the search (counting from left)
 *  @return  index in aDest where member of aSet occurs, or -1 if not found
 */
PRInt32 nsStr::RFindSubstr(const nsStr& aDest,const nsStr& aTarget, PRBool aIgnoreCase,PRInt32 anOffset) {

  PRInt32 result=kNotFound;

  if((0<aDest.mLength) && (anOffset<(PRInt32)aDest.mLength)) {
    PRInt32 index=(0<=anOffset) ? anOffset : aDest.mLength-1;

    if((aDest.mLength>=aTarget.mLength) && (aTarget.mLength>0) && (index>=0)){

      nsStr theCopy;
      nsStr::Initialize(theCopy,eOneByte);
      nsStr::Assign(theCopy,aTarget,0,aTarget.mLength,0);
      if(aIgnoreCase){
        nsStr::ChangeCase(theCopy,PR_FALSE); //force to lowercase
      }
    
      PRInt32   theTargetMax=theCopy.mLength;
      while(index>=0) {
        PRInt32 theSubIndex=-1;
        PRBool  matches=PR_FALSE;
        if(index+theCopy.mLength<=aDest.mLength) {
          matches=PR_TRUE;
          while((++theSubIndex<theTargetMax) && (matches)){
            PRUnichar theDestChar=(aIgnoreCase) ? nsCRT::ToLower(GetCharAt(aDest,index+theSubIndex)) : GetCharAt(aDest,index+theSubIndex);
            PRUnichar theTargetChar=GetCharAt(theCopy,theSubIndex);
            matches=PRBool(theDestChar==theTargetChar);
          } //while
        } //if
        if(matches) {
          result=index;
          break;
        }
        index--;
      } //while
      nsStr::Destroy(theCopy,0);
    }//if
  }//if
  return result;
}
 

/**
 *  This searches aDest (in reverse) for a given character 
 *  
 *  @update  gess 3/25/98
 *  @param   aDest string to search
 *  @param   char is the character you're trying to find.
 *  @param   aIgnorecase indicates case sensitivity of search
 *  @param   anOffset tells us where to start the search
 *  @return  index in aDest where member of aSet occurs, or -1 if not found
 */
PRInt32 nsStr::RFindChar(const nsStr& aDest,PRUnichar aChar, PRBool aIgnoreCase,PRInt32 anOffset) {
  PRInt32 result=kNotFound;
  if((0<aDest.mLength) && (anOffset<(PRInt32)aDest.mLength)) {
    PRUint32 index=(0<=anOffset) ? anOffset : aDest.mLength-1;
    result=gRFindChars[aDest.mCharSize](aDest.mStr,aDest.mLength,index,aChar,aIgnoreCase);
  }
  return result;
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

  PRInt32 index=(0<=anOffset) ? anOffset : aDest.mLength;
  PRInt32 thePos;

    //note that the search is inverted here. We're scanning aDest, one char at a time
    //but doing the search against the given set. That's why we use 0 as the offset below.
  if(0<aDest.mLength) {
    while(--index>=0) {
      PRUnichar theChar=GetCharAt(aDest,index);
      thePos=gFindChars[aSet.mCharSize](aSet.mStr,aSet.mLength,0,theChar,aIgnoreCase);
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
PRInt32 nsStr::Compare(const nsStr& aDest,const nsStr& aSource,PRInt32 aCount,PRBool aIgnoreCase) {
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

    PRInt32 maxlen=(aSource.mLength<aDest.mLength) ? aDest.mLength : aSource.mLength;
    aCount = (aCount<0) ? maxlen : MinInt(aCount,maxlen);
    result=(*gCompare[aDest.mCharSize][aSource.mCharSize])(aDest.mStr,aSource.mStr,aCount,aIgnoreCase);
  }
  return result;
}

//----------------------------------------------------------------------------------------

CBufDescriptor::CBufDescriptor(char* aString,PRBool aStackBased,PRUint32 aCapacity,PRInt32 aLength) { 
  mBuffer=aString;
  mCharSize=eOneByte;
  mStackBased=aStackBased;
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
  if(aString && aCapacity>1) {
    mCapacity=aCapacity-1;
    mLength=(-1==aLength) ? nsCRT::strlen(aString) : aLength;
    if(mLength>PRInt32(mCapacity))
      mLength=mCapacity;
  }
}


//----------------------------------------------------------------------------------------
