/*************************************************************************************
 *  
 * MODULES NOTES:
 *
 *  1. See nsStringImpl.h
 *
 * FILE NOTES:
 *
 *  02.29.2000: Original files (rickg)
 *  03.02.2000: Flesh out the interface to be compatible with old library (rickg)
 *  
 *************************************************************************************/

#ifndef NS_BUFFERMANAGER_
#define NS_BUFFERMANAGER_


#include "nsStringValue.h"
#include <string.h>

class nsReadableString;
class nsReadableStringIterator;

/***************************************************************************
 *
 *  The following (hack) template functions will go away, but for now they
 *  provide basic string copying and appending for my prototype. (rickg)
 *
 ***************************************************************************/

inline size_t stringlen(const PRUnichar* theString) {
  const PRUnichar* theEnd=theString;
  while(*theEnd++);  //skip to end of aDest...
  size_t result=theEnd-theString-1;
  return result;
}

inline size_t stringlen(const char* theString) {
  return ::strlen(theString);
}


template <class CharType1,class CharType2>
void strncopy(CharType1* aDest,const CharType2* aSource, size_t aLength) {
  const CharType2* aLast=aSource+aLength;
  while(aSource<aLast) {
    *aDest++=(CharType1)*aSource++;
  }
}

template <class CharType1,class CharType2> 
void strcatn(CharType1* aDest,const CharType2* aSource, size_t aLength) {
  while(*aDest++);  //skip to end of aDest...
  strncopy(aDest,aSource,aLength);
}

/*************************************************************************************
 *
 *  These methods offer the low level buffer manipulation routines used throughout
 *  the string hierarchy. T1 and T2 will be types of stringvalues. (rickg)
 *
 *************************************************************************************/

template <class T1,class T2>
inline PRInt32 MinInt(T1 anInt1,T2 anInt2){
  return (anInt1<anInt2) ? anInt1 : anInt2;
}

template <class T1,class T2>
inline PRInt32 MaxInt(T1 anInt1,T2 anInt2){
  return (anInt1<anInt2) ? anInt2 : anInt1;
}

template <class CharType>
inline void SVAddNullTerminator( nsStringValueImpl<CharType>& aDest) {
  if(aDest.mBuffer) {
    aDest.mBuffer[aDest.mLength]=0;
  }
}


static const PRUnichar gCommonEmptyBuffer[1] = {0};
static nsresult gStringAcquiredMemory = NS_OK;

template <class CharType>
PRBool Alloc( nsStringValueImpl<CharType>& aDest, PRUint32 aCount) {

  static int mAllocCount=0;
  mAllocCount++;

  //we're given the acount value in charunits; now scale up to next multiple.
  PRUint32	theNewCapacity=kDefaultStringSize;
  while(theNewCapacity<aCount){ 
		theNewCapacity<<=1;
  }

  aDest.mCapacity=theNewCapacity;
  aDest.mBuffer = new CharType[1+aDest.mCapacity];

  // PRUint32 theSize=(theNewCapacity<<aDest.mCharSize);
  // aDest.mBuffer = (char*)nsAllocator::Alloc(theSize);

  if(aDest.mBuffer) {
    aDest.mOwnsBuffer=1;
    gStringAcquiredMemory=PR_TRUE;
  }
  else gStringAcquiredMemory=PR_FALSE;
  return gStringAcquiredMemory;
}


/**
 * Free memory from given stringvalueimpl
 *
 * @update  rickg 03.02.2000
 * @return  memory error (usually returns NS_OK)
 */
template <class CharType>
PRBool Free( nsStringValueImpl<CharType>& aDest){
  if(aDest.mBuffer){
    if(aDest.mOwnsBuffer){
      // nsAllocator::Free(aDest.mBuffer);
      delete [] aDest.mBuffer;
    }
    aDest.mBuffer=0;
    aDest.mOwnsBuffer=0;
    return PR_TRUE;
  }
  return PR_FALSE;
}


/**
 * Realloc memory from given stringvalueimpl
 *
 * @update  rickg 03.02.2000
 * @return  memory error (usually returns NS_OK)
 */
template <class CharType>
PRBool Realloc(nsStringValueImpl<CharType>&aDest,PRUint32 aCount){

  nsStringValueImpl<CharType> temp(aDest);

  PRBool result=Alloc<CharType>(temp,aCount);
  if(result) {
    Free<CharType>(aDest);
    aDest.mBuffer=temp.mBuffer;
    aDest.mCapacity=temp.mCapacity;
    aDest.mOwnsBuffer=temp.mOwnsBuffer;
  }
  return result;
}

/**
 * Retrieve last memory error
 *
 * @update  rickg 10/11/99
 * @return  memory error (usually returns NS_OK)
 */
PRBool DidAcquireMemory(void) {
  return gStringAcquiredMemory;
}

//----------------------------------------------------------------------------------------

/**
 * This is the low-level buffer copying routine between 2 stringvalues.
 *
 * @update	rickg 03/01/00
 * @param   aDest is the destination stringvalue
 * @param   aSource is the source stringvalue
 * @param   anOffset tells us where in aSource to begin copying from
 * @param   aLength tells us how many chars to copy from aSource
 */
template <class CharType1,class CharType2>
void SVCopyBuffer( nsStringValueImpl<CharType1>& aDest,const nsStringValueImpl<CharType1>& aSource,PRInt32 anOffset,PRInt32 aLength) {
  if((0<aSource.mLength)) {
    strncopy(aDest.mBuffer,&aSource.mBuffer[anOffset],aLength);  //WAY WRONG, but good enough for now...
  }
}

//----------------------------------------------------------------------------------------

/**
 * This method destroys the given stringvalue, and *MAY* 
 * deallocate it's memory depending on the setting
 * of the internal mOwnsBUffer flag.
 *
 * @update	rickg 03/01/00
 * @param   aStringValue is the stringvalue to be destroyed
 */
template <class CharType>
void SVDestroy(nsStringValueImpl<CharType> &aStringValue) {
}

/**
 * These methods are where memory allocation/reallocation occur. 
 *
 * @update	rickg 03/01/00
 * @param   aString is the stringvalue to be manipulated
 * @param   anAgent is the allocator to be used on the stringvalue
 * @return  
 */
template <class CharType>
PRBool SVEnsureCapacity(nsStringValueImpl<CharType>& aString,PRInt32 aNewLength){

  PRBool result=PR_TRUE;
  if(aNewLength>aString.mCapacity) {
    result=Realloc< CharType>(aString,aNewLength);
    SVAddNullTerminator<CharType>(aString);
  }
  return result;

}

/**
 * Attempt to grow the capacity of the given stringvalueimp by given amount
 * (It's possible the length is already larger, so this can fall out.)
 *
 * @update  rickg 03.02.2000
 * @return  memory error (usually returns NS_OK)
 */
template <class CharType>
PRBool SVGrowCapacity(nsStringValueImpl<CharType>& aDest,PRInt32 aNewLength){
  PRBool result=PR_TRUE;
  if(aNewLength>aDest.mCapacity) {

    nsStringValueImpl<CharType> theTempStr;

    result=SVEnsureCapacity<CharType>(theTempStr,aNewLength);
    if(result) {
      if(aDest.mLength) {

        SVCopyBuffer< CharType, CharType >(aDest,theTempStr,0,theTempStr.mLength);
      } 
      Free<CharType>(aDest);
      aDest=theTempStr;
      theTempStr.mBuffer=0; //make sure to null this out so that you don't lose the buffer you just stole...
    }
  }
  return result;
}

/**
 * This method deletes chars from the given str. 
 * The given allocator may choose to resize the str as well.
 *
 * @update	rickg 03/01/00
 * @param  aDest is the stringvalue to be modified
 * @param  aDestOffset tells us where in dest to start deleting
 * @param  aCount tells us the (max) # of chars to delete
 * @param  anAgent is the allocator to be used for alloc/free operations
 */
template <class CharType>
void SVDelete( nsStringValueImpl<CharType>& aDest,PRInt32 aDestOffset,PRUint32 aCount){
}

/**
 * This method is used to truncate the given string.
 * The given allocator may choose to resize the str as well (but it's not likely).
 *
 * @update	rickg 03/01/00
 * @param  aDest is the stringvalue to be modified
 * @param  aDestOffset tells us where in dest to truncate
 */
template <class CharType>
void SVTruncate( nsStringValueImpl<CharType>& aDest,PRInt32 aDestOffset) {
  if(aDestOffset<aDest.mLength){
    aDest.mLength=aDestOffset;
    SVAddNullTerminator<CharType>(aDest);
  }
};

/**
 * These methods are used to append content to the given stringvalue
 *
 * @update	rickg 03/01/00
 * @param  aDest is the stringvalue to be modified
 * @param  aSource is the buffer to be copied from
 * @param  anOffset tells us where in source to start copying
 * @param  aCount tells us the (max) # of chars to copy
 * @param  anAgent is the allocator to be used for alloc/free operations
 */
template <class CharType1, class CharType2>
void SVAppend( nsStringValueImpl<CharType1>& aDest,const nsStringValueImpl<CharType2>& aSource,PRInt32 anOffset,PRInt32 aCount){
  if(anOffset<aSource.mLength){
    PRInt32 theRealLen=(aCount<0) ? aSource.mLength : MinInt(aCount,aSource.mLength);
    PRInt32 theLength=(anOffset+theRealLen<aSource.mLength) ? theRealLen : (aSource.mLength-anOffset);
    if(0<theLength){
      
      PRBool isBigEnough=PR_TRUE;
      if(aDest.mLength+theLength > aDest.mCapacity) {
        isBigEnough=SVGrowCapacity<CharType1>(aDest,aDest.mLength+theLength);
      }

      if(isBigEnough) {
        //now append new chars, starting at offset

        CharType1 *theDestChar=&aDest.mBuffer[aDest.mLength];
        CharType2 *theSourceChar=&aSource.mBuffer[anOffset];
        const CharType2 *theLastChar=theSourceChar+theLength;
        while(theSourceChar<theLastChar) {
          *theDestChar++=(CharType1)*theSourceChar++;
        }

        aDest.mLength+=theLength;
        SVAddNullTerminator<CharType1>(aDest);
      }
    }
  }
}

/**
 * This method is used to append content to the given stringvalue from an nsReadableString
 *
 * @update	rickg 03/01/00
 * @param  aDest is the stringvalue to be modified
 * @param  aSource is the readablestring to be copied from
 * @param  anOffset tells us where in source to start copying
 * @param  aCount tells us the (max) # of chars to copy
 * @param  anAgent is the allocator to be used for alloc/free operations
 */
template <class CharType1>
void SVAppendReadable( nsStringValueImpl<CharType1>& aDest,const nsReadableString& aSource,PRInt32 anOffset,PRInt32 aCount){
  /*
      //I expect a pattern similar to this that could work on any readable string
      
    nsReadableStringIterator theFirst=aSource.First();
    nsReadableStringIterator theLast=aSource.Last();
    CharType1 *theDest=aDest.mBuffer;
    while(theFirst<theLast) {
      *theDest++=theFirst++;
    }
  */
}

/**
 * These methods are used to assign contents of a source string to dest string
 *
 * @update	rickg 03/01/00
 * @param  aDest is the stringvalue to be modified
 * @param  aSource is the buffer to be copied from
 * @param  anOffset tells us where in source to start copying
 * @param  aCount tells us the (max) # of chars to copy
 * @param  anAgent is the allocator to be used for alloc/free operations
 */

template <class CharType1, class CharType2>
void SVAssign( nsStringValueImpl<CharType1>& aDest,const nsStringValueImpl<CharType2>& aSource,PRInt32 anOffset,PRInt32 aCount){
  if(aDest.mBuffer!=aSource.mBuffer){
    SVTruncate(aDest,0);
    SVAppend(aDest,aSource,anOffset,aCount);
  }
}

/**
 * These methods are used to insert content from source string to the dest stringvalue
 *
 * @update	rickg 03/01/00
 * @param  aDest is the stringvalue to be modified
 * @param  aDestOffset tells us where in dest to start insertion
 * @param  aSource is the buffer to be copied from
 * @param  aSrcOffset tells us where in source to start copying
 * @param  aCount tells us the (max) # of chars to insert
 * @param  anAgent is the allocator to be used for alloc/free operations
 */
template <class CharType1, class CharType2>
void SVInsert( nsStringValueImpl<CharType1>& aDest,PRInt32 aDestOffset,const nsStringValueImpl<CharType2>& aSource,PRInt32 aSrcOffset,PRInt32 aCount){
}

#endif

