/*************************************************************************************
 *  
 * MODULES NOTES:
 *
 *  1. See nsStringImpl.h
 *
 * Contributor(s):
 *   Rick Gessner <rickg@netscape.com>
 * 
 * History:
 *
 *  02.29.2000: Original files (rickg)
 *  03.02.2000: Flesh out the interface to be compatible with old library (rickg)
 *  03.03.2000: Finished mapping original nsStr functionality to template form (rickg)
 *  
 *************************************************************************************/

#ifndef NS_BUFFERMANAGER_
#define NS_BUFFERMANAGER_


#include "nsStringValue.h"
#include <string.h>
#include "nsCRT.h"

class nsAReadableString;
class nsAReadableStringIterator;

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
  return (anInt1<(T1)anInt2) ? anInt1 : anInt2;
}

template <class T1,class T2>
inline PRInt32 MaxInt(T1 anInt1,T2 anInt2){
  return (anInt1<(T1)anInt2) ? anInt2 : anInt1;
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
inline nsresult SVAlloc( nsStringValueImpl<CharType>& aDest, PRUint32 aCount) {

  nsresult result=NS_OK;

  static int mAllocCount=0;
  mAllocCount++;

  //we're given the acount value in charunits; now scale up to next multiple.
  aDest.mCapacity=kDefaultStringSize;
  while(aDest.mCapacity<aCount){ 
		aDest.mCapacity<<=1;
  }

  aDest.mBuffer = new CharType[1+aDest.mCapacity];

  // PRUint32 theSize=(theNewCapacity<<aDest.mCharSize);
  // aDest.mBuffer = (char*)nsAllocator::Alloc(theSize);

  if(aDest.mBuffer) {
    aDest.mBuffer[0]=0;
    gStringAcquiredMemory=PR_TRUE;
  }
  else {
    gStringAcquiredMemory=PR_FALSE;
    result=NS_MEMORY_ERROR;
  }
  return result;
}


/**
 * Free memory from given stringvalueimpl
 *
 * @update  rickg 03.02.2000
 * @return  memory error (usually returns NS_OK)
 */
template <class CharType>
inline nsresult SVFree( nsStringValueImpl<CharType>& aDest){
  if(aDest.mBuffer){
    
    delete [] aDest.mBuffer;   // nsAllocator::Free(aDest.mBuffer);
    aDest.mBuffer=0;
  }
  return NS_OK;
}


/**
 * Realloc memory from given stringvalueimpl
 *
 * @update  rickg 03.02.2000
 * @return  memory error (usually returns NS_OK)
 */
template <class CharType>
inline nsresult SVRealloc(nsStringValueImpl<CharType>&aDest,PRUint32 aCount){

  nsStringValueImpl<CharType> temp(aDest);

  nsresult result=SVAlloc<CharType>(temp,aCount);
  if(NS_SUCCESS(result)) {
    SVFree<CharType>(aDest);
    aDest.mBuffer=temp.mBuffer;
    aDest.mCapacity=temp.mCapacity;
  }
  return result;
}

/**
 * Retrieve last memory error
 *
 * @update  rickg 03.02.2000
 * @return  memory error (usually returns NS_OK)
 */
static PRBool DidAcquireMemory(void) {
  return gStringAcquiredMemory;
}

//----------------------------------------------------------------------------------------

/**
 * This is the low-level buffer copying routine between 2 stringvalues.
 *
 * @update	rickg 03.01.2000
 * @param   aDest is the destination stringvalue
 * @param   aSource is the source stringvalue (you must account for offset yourself in calling routine)
 * @param   aLength tells us how many chars to copy from aSource
 */
template <class CharType1,class CharType2>
inline nsresult SVCopyBuffer_( CharType1* aDest, const CharType2* aSource, PRInt32 aLength) {

  if(sizeof(CharType1)==sizeof(CharType2)) {
    memcpy(aDest,aSource,aLength*sizeof(CharType1));
  }
  else {
    const CharType2* aLast=aSource+aLength;
    while(aSource<aLast) {
      *aDest++=(CharType1)*aSource++;
    }
  }

  return NS_OK;
}

//----------------------------------------------------------------------------------------

/**
 * This is the low-level buffer copying routine between 2 stringvalues.
 *
 * @update	rickg 03.01.2000
 * @param   aDest is the destination stringvalue
 * @param   aSource is the source stringvalue
 * @param   anOffset tells us where in aSource to begin copying from
 * @param   aLength tells us how many chars to copy from aSource
 */
template <class CharType1,class CharType2>
inline nsresult SVCopyBuffer( nsStringValueImpl<CharType1>& aDest,const nsStringValueImpl<CharType2>& aSource,PRInt32 aLength,PRInt32 aSrcOffset) {
  nsresult result=NS_OK;
  if((0<aSource.mLength)) {

    CharType1* theDest  = aDest.mBuffer;
    CharType2* theSource= aSource.mBuffer;
    CharType2* from     = theSource+aSrcOffset;

    result=SVCopyBuffer_<CharType1,CharType2>(aDest.mBuffer,aSource.mBuffer,aLength);

  }
  return result;
}

//----------------------------------------------------------------------------------------

/**
 * This method destroys the given stringvalue, and *MAY* 
 * deallocate it's memory depending on the setting
 * of the internal mOwnsBUffer flag.
 *
 * @update	rickg 03.01.2000
 * @param   aStringValue is the stringvalue to be destroyed
 */
template <class CharType>
inline nsresult SVDestroy(nsStringValueImpl<CharType> &aStringValue) {
  nsresult result=NS_OK;
  return result;
}

/**
 * These methods are where memory allocation/reallocation occur. 
 *
 * @update	rickg 03.01.2000
 * @param   aString is the stringvalue to be manipulated
 * @return  
 */
template <class CharType>
inline nsresult SVEnsureCapacity(nsStringValueImpl<CharType>& aString,PRUint32 aNewLength){

  nsresult result=NS_OK;
  if(aNewLength>aString.mCapacity) {
    result=SVRealloc< CharType>(aString,aNewLength);
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
inline nsresult SVGrowCapacity(nsStringValueImpl<CharType>& aDest,PRUint32 aNewLength){
  nsresult result=NS_OK;
  if(aNewLength>aDest.mCapacity) {

    nsStringValueImpl<CharType> theTempStr;
    nsresult result=SVAlloc<CharType>(theTempStr,aNewLength);
    if(NS_SUCCESS(result)) {

      if(0<aDest.mLength) {
        result=SVCopyBuffer_<CharType,CharType>(theTempStr.mBuffer,aDest.mBuffer,aDest.mLength);
        theTempStr.mLength=aDest.mLength;
        SVAddNullTerminator<CharType>(aDest);
      }
      
      result=SVFree<CharType>(aDest);
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
 * @update	rickg 03.01.2000
 * @param  aDest is the stringvalue to be modified
 * @param  aDestOffset tells us where in dest to start deleting
 * @param  aCount tells us the (max) # of chars to delete
 */
template <class CharType>
inline nsresult SVDelete( nsStringValueImpl<CharType>& aDest,PRUint32 aDestOffset,PRInt32 aCount){
  nsresult result=NS_OK;

  if((0<aCount) && (aDestOffset<aDest.mLength)){

    CharType* root=aDest.mBuffer;
    CharType* dst1=root+aDestOffset;
    CharType* dst2=dst1+aCount;
    CharType* end =root+aDest.mLength;
    CharType* last = (dst2<end) ? dst2 : end;

    if(last<end) {
      aDest.mLength-=(last-dst1);
      memmove(dst1,last,(end-last)*sizeof(CharType));
    }
    else aDest.mLength=aDestOffset;
    SVAddNullTerminator<CharType>(aDest);

  }
  return result;
}

/**
 * This method is used to truncate the given string.
 * The given allocator may choose to resize the str as well (but it's not likely).
 *
 * @update	rickg 03.01.2000
 * @param  aDest is the stringvalue to be modified
 * @param  aDestOffset tells us where in dest to truncate
 */
template <class CharType>
inline void SVTruncate( nsStringValueImpl<CharType>& aDest,PRUint32 aDestOffset) {
  if(aDestOffset<aDest.mLength){
    aDest.mLength=aDestOffset;
    SVAddNullTerminator<CharType>(aDest);
  }
};

/**
 * These methods are used to append content to the given stringvalue
 *
 * @update	rickg 03.01.2000
 * @param  aDest is the stringvalue to be modified
 * @param  aSource is the buffer to be copied from
 * @param  anOffset tells us where in source to start copying
 * @param  aCount tells us the (max) # of chars to copy
 */
template <class CharType1, class CharType2>
inline nsresult SVAppend( nsStringValueImpl<CharType1>& aDest,const nsStringValueImpl<CharType2>& aSource,PRInt32 aCount,PRInt32 aSrcOffset){

  nsresult result=NS_OK;

  if(aSrcOffset<(PRInt32)aSource.mLength){
    PRInt32 theRealLen=(aCount<0) ? aSource.mLength : MinInt(aCount,aSource.mLength);
    PRInt32 theLength=(aSrcOffset+theRealLen<(PRInt32)aSource.mLength) ? theRealLen : (aSource.mLength-aSrcOffset);
    if(0<theLength){
      
      PRBool isBigEnough=PR_TRUE;
      if(aDest.mLength+theLength > aDest.mCapacity) {
        result=SVGrowCapacity<CharType1>(aDest,aDest.mLength+theLength);
      }

      if(NS_SUCCESS(result)) {

        //now append new chars, starting at offset

        CharType1* theDest      = &aDest.mBuffer[aDest.mLength];
        CharType2* theSource    = &aSource.mBuffer[aSrcOffset];
        CharType2 *theLastChar  = theSource+theLength;

        result=SVCopyBuffer_<CharType1,CharType2>(theDest,theSource,theLength);

        aDest.mLength+=theLength;
      }
       SVAddNullTerminator<CharType1>(aDest);
    }
  }
  return result;
}

/**
 * This method is used to append content to the given stringvalue from an nsAReadableString
 *
 * @update	rickg 03.01.2000
 * @param  aDest is the stringvalue to be modified
 * @param  aSource is the readablestring to be copied from
 * @param  anOffset tells us where in source to start copying
 * @param  aCount tells us the (max) # of chars to copy
 */
template <class CharType1>
inline nsresult SVAppendReadable( nsStringValueImpl<CharType1>& aDest,const nsAReadableString& aSource,PRInt32 aCount,PRInt32 anSrcOffset){
  nsresult result=NS_OK;
  /*
      //I expect a pattern similar to this that could work on any readable string
      
    nsAReadableStringIterator theFirst=aSource.First();
    nsAReadableStringIterator theLast=aSource.Last();
    CharType1 *theDest=aDest.mBuffer;
    while(theFirst<theLast) {
      *theDest++=theFirst++;
    }
  */
  return result;
}

/**
 * These methods are used to assign contents of a source string to dest string
 *
 * @update	rickg 03.01.2000
 * @param  aDest is the stringvalue to be modified
 * @param  aSource is the buffer to be copied from
 * @param  anOffset tells us where in source to start copying
 * @param  aCount tells us the (max) # of chars to copy
 */

template <class CharType1, class CharType2>
inline nsresult SVAssign(nsStringValueImpl<CharType1>& aDest,const nsStringValueImpl<CharType2>& aSource,PRInt32 aCount,PRInt32 aSrcOffset) {
  nsresult result=NS_OK;
  if((void*)aDest.mBuffer!=(void*)aSource.mBuffer){
    SVTruncate(aDest,0);
    SVAppend(aDest,aSource,aCount,aSrcOffset);
  }
  return result;
}



/**
 * These methods are used to insert content from source string to the dest stringvalue
 *
 * @update	rickg 03.01.2000
 * @param  aDest is the stringvalue to be modified
 * @param  aDestOffset tells us where in dest to start insertion
 * @param  aSource is the buffer to be copied from
 * @param  aSrcOffset tells us where in source to start copying
 * @param  aCount tells us the (max) # of chars to insert
 */
template <class CharType1, class CharType2>
inline nsresult SVInsert( nsStringValueImpl<CharType1>& aDest,PRUint32 aDestOffset,const nsStringValueImpl<CharType2>& aSource,PRInt32 aCount,PRInt32 aSrcOffset) {
  nsresult result=NS_OK;

  //there are a few cases for insert:
  //  1. You're inserting chars into an empty string (assign)
  //  2. You're inserting onto the end of a string (append)
  //  3. You're inserting onto the 1..n-1 pos of a string (the hard case).
  if(0<aSource.mLength){
    if(aDest.mLength){
      if(aDestOffset<aDest.mLength){
        PRUint32 theRealLen=(aCount<0) ? aSource.mLength : MinInt(aCount,aSource.mLength);
        PRUint32 theLength=(aSrcOffset+theRealLen<aSource.mLength) ? theRealLen : (aSource.mLength-aSrcOffset);

        if(aSrcOffset<(PRInt32)aSource.mLength) {
            //here's the only new case we have to handle. 
            //chars are really being inserted into our buffer...

          if(aDest.mLength+theLength > aDest.mCapacity) {

            nsStringValueImpl<CharType1> theTempStr;
            result=SVEnsureCapacity<CharType1>(theTempStr,aDest.mLength+theLength);

            if(NS_SUCCESS(result)) {
              if(aDestOffset) {
                SVAppend(theTempStr,aDest,aDestOffset,0); //first copy leftmost data...
              } 
              
              SVAppend(theTempStr,aSource,aSource.mLength,0); //next copy inserted (new) data
            
              PRUint32 theRemains=aDest.mLength-aDestOffset;
              if(theRemains) {
                SVAppend(theTempStr,aDest,theRemains,aDestOffset); //next copy rightmost data
              }

              SVFree(aDest);
              aDest=theTempStr;
              theTempStr.mBuffer=0; //make sure to null this out so that you don't lose the buffer you just stole...
            }

          }

          else {
            CharType1* root=aDest.mBuffer;
            CharType1* end= root+aDest.mLength;
            CharType1* dst2 = root+aDestOffset;
            CharType1* dst1 = root+aDestOffset+aCount;

            memmove(dst1,dst2,sizeof(CharType1)*(end-dst2));
      
            nsStringValueImpl<CharType1> theString(dst2,theLength);
            result=SVCopyBuffer< CharType1, CharType2 >(theString,aSource,theLength,aSrcOffset);
              //finally, make sure to update the string length...
            aDest.mLength+=theLength;
            SVAddNullTerminator(aDest);
          }
        }//if
        //else nothing to do!
      }
      else SVAppend(aDest,aSource,aCount,0);
    }
    else SVAppend(aDest,aSource,aCount,0);
  }
  return result;
}


/**
 * This method compares characters strings of two types, and ignores case
 *
 * @update  rickg 03.01.2000
 * @param aLeft
 * @param aLast
 * @param aMax
 * @param anEnd
 * @param aTarget
 * @return  aDest<aSource=-1;aDest==aSource==0;aDest>aSource=1
 */
template <class CharType1,class CharType2>
inline PRInt32 SVCompareCase_(const CharType1* aLeft,const CharType1* anEnd,const CharType2* aTarget){

  while(aLeft<anEnd){
    PRUnichar theChar=(PRUnichar)nsCRT::ToUpper(*aLeft++);
    PRUnichar theTargetChar=(PRUnichar)nsCRT::ToUpper(*aTarget++);
    if(theChar<theTargetChar)
      return -1;
    else if(theTargetChar<theChar)
      return 1;
  } //while

  return 0;
}


/**
 * This method compares characters strings of two types (case sensitive)
 *
 * @update  rickg 03.01.2000
 * @param aLeft
 * @param aLast
 * @param aMax
 * @param anEnd
 * @param aTarget
 * @return  aDest<aSource=-1;aDest==aSource==0;aDest>aSource=1
 */
template <class CharType1,class CharType2>
inline PRInt32 SVCompare_(const CharType1* aLeft,const CharType1* anEnd,const CharType2* aTarget){

  while(aLeft<anEnd){
    CharType1 theChar=*aLeft++;
    CharType2 theTargetChar=*aTarget++;
    if(theChar<theTargetChar)
      return -1;
    else if(theTargetChar<theChar)
      return 1;
  } //while
  return 0;
}


/**
 * This method removes characters from the given set from this string.
 * NOTE: aSet is a char*, and it's length is computed using strlen, which assumes null termination.
 *
 * @update  rickg 03.01.2000
 * @param aDest
 * @param aString
 * @param aIgnoreCase
 * @param aCount --- tells us how many iterations to make.
 * @return  aDest<aSource=-1;aDest==aSource==0;aDest>aSource=1
 */
template <class CharType1,class CharType2>
inline PRInt32 SVCompare(const nsStringValueImpl<CharType1>& aDest,const nsStringValueImpl<CharType2>& aSource,PRBool aIgnoreCase, PRUint32 aCount){

  PRInt32 result=0;

  if(aCount) {
    PRUint32 minlen=(aSource.mLength<aDest.mLength) ? aSource.mLength : aDest.mLength;

    if(0==minlen) {
      if ((aDest.mLength == 0) && (aSource.mLength == 0))
        return 0;
      if (aDest.mLength == 0)
        return -1;
      return 1;
    }

    PRUint32 theCount = (aCount<0) ? minlen: MinInt(aCount,minlen);

    const CharType1* left= aDest.mBuffer;
    const CharType1* last= left+theCount;
    const CharType1* max = left+aDest.mLength;
    const CharType1* end = (last<max) ? last : max;
    const CharType2* target = aSource.mBuffer;

    if(aIgnoreCase)
      result = SVCompareCase_<CharType1,CharType2>(left,end,target) ;
    else result =SVCompare_<CharType1,CharType2>(left,end,target);

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
 * This method removes characters from the given set from this string.
 * NOTE: aSet is a char*, and it's length is computed using strlen, which assumes null termination.
 *
 * @update  rickg 03.01.2000
 * @param aDest
 * @param aTarget
 * @param aIgnoreCase
 * @param aCount : tells us how many iterations to make, starting at offset
 * @param aOffset: tells us where to start searching within aDest
 * @return nothing
 */
template <class CharType1,class CharType2>
inline PRInt32 SVFind(const nsStringValueImpl<CharType1>& aDest,const nsStringValueImpl<CharType2>& aTarget,PRBool aIgnoreCase,PRInt32 aCount,PRInt32 aSrcOffset) {

  PRUint32 minlen=(aTarget.mLength<aDest.mLength) ? aTarget.mLength : aDest.mLength;
  
  aCount = (-1==aCount) ? aDest.mLength-minlen : aCount;
  
  if((0<aCount) && minlen) {

    const CharType1* root = aDest.mBuffer;
    const CharType1* left = root+aSrcOffset;
    const CharType1* last = left+aCount;
    const CharType1* max  = root+aDest.mLength;
    const CharType1* end  = (last<max) ? last : max;

    PRInt32 cmp=0;

    if(aIgnoreCase) {
      while(left<=end) {
        cmp = SVCompareCase_<CharType1,CharType2>(left,left+minlen,aTarget.mBuffer) ;
        if(0==cmp) {
          return left-root;
        }
        left++;
      }
    }
    else {

      PRBool theFastWay = ((1==sizeof(CharType1)) && (1==sizeof(CharType2)));

      while(left<=end) {
        if(theFastWay) {
          size_t theCount=end-left;
          cmp = memcmp(left,aTarget.mBuffer,minlen);
        }
        else cmp =SVCompare_<CharType1,CharType2>(left,left+minlen,aTarget.mBuffer);
        if(0==cmp) {
          return left-root;
        }
        left++;
      }
    }
  }
  return kNotFound;

}

/**
 * This searches for the given char withing the given string
 * NOTE: aSet is a char*, and it's length is computed using strlen, which assumes null termination.
 *
 * @update  rickg 03.02.2000
 * @param aDest
 * @param aSet
 * @param aEliminateLeading
 * @param aEliminateTrailing
 * @return nothing
 */
template <class CharType>
PRInt32 SVFindChar(const nsStringValueImpl<CharType>& aDest,PRUnichar aChar,PRBool aIgnoreCase,PRInt32 aCount,PRInt32 aSrcOffset) {

  if(aSrcOffset<0)
    aSrcOffset=0;

  if(aCount<0)
    aCount = (PRInt32)aDest.mLength;

  size_t theCharSize=sizeof(CharType);
  if((2==theCharSize) || (aChar<256)) {
    if((0<aDest.mLength) && ((PRUint32)aSrcOffset<aDest.mLength)) {

      //We'll only search if the given aChar is within the normal ascii a range,
      //(Since this string is definitely within the ascii range).
    
      if(0<aCount) {

        const CharType* root= aDest.mBuffer;
        const CharType* left= root+aSrcOffset;
        const CharType* last= left+aCount;
        const CharType* max = left+aDest.mLength;
        const CharType* end = (last<max) ? last : max;

        if(aIgnoreCase) {
    
          CharType theChar=(CharType)nsCRT::ToUpper(aChar);
          while(left<end){
            if(nsCRT::ToUpper(*left)==theChar)
              return left-root;
            ++left;
          }
        }
        else if (1==theCharSize){

          PRInt32 theMax = end-left;
          if(0<theMax) {
            CharType theChar=(CharType)aChar;
            const char* result=(const char*)memchr(left, theChar, theMax);
            if(result) {
              return result-(char*)root;
            }
          }
        }
        else {
          while(left<end){
            if(*left==aChar)
              return (left-root);
            ++left;
          } //while
        } //else
      }
    } //if
  }//if

  return kNotFound;
}

/**
 * This searches for the given char withing the given string
 * NOTE: aSet is a char*, and it's length is computed using strlen, which assumes null termination.
 *
 * @update  rickg 03.02.2000
 * @param aDest
 * @param aSet
 * @param aEliminateLeading
 * @param aEliminateTrailing
 * @return nothing
 */
template <class CharType1,class CharType2>
inline PRInt32 SVFindCharInSet(const nsStringValueImpl<CharType1>& aDest,const nsStringValueImpl<CharType2>& aSet,PRBool aIgnoreCase,PRInt32 aCount,PRInt32 anOffset) {

  if(anOffset<0)
    anOffset=0;

  if(aCount<0)
    aCount = (PRInt32)aDest.mLength;

  if((0<aCount) && (0<aSet.mLength)){

    const CharType1* root = aDest.mBuffer;
    const CharType1* left = root+anOffset;
    const CharType1* last = left+aCount;
    const CharType1* max  = root+aDest.mLength;
    const CharType1* end  = (last<max) ? last : max;

    PRInt32 cmp=0;
    while(left<=end) {

      PRUnichar theChar=(PRUnichar)*left;

      PRInt32 thePos=SVFindChar<CharType2>(aSet,theChar,aIgnoreCase,aSet.mLength,0);
      if(kNotFound<thePos) {
        return left-root;
      }
      left++;
    }
  }
  return kNotFound;
}

/**
 * This method removes characters from the given set from this string.
 * NOTE: aSet is a char*, and it's length is computed using strlen, which assumes null termination.
 *
 * @update  rickg 03.01.2000
 * @param aDest
 * @param aSet
 * @param aEliminateLeading
 * @param aEliminateTrailing
 * @return nothing
 */
template <class CharType1,class CharType2>
inline PRInt32 SVRFind(const nsStringValueImpl<CharType1>& aDest,const nsStringValueImpl<CharType2>& aTarget,PRBool aIgnoreCase,PRInt32 aCount,PRInt32 aSrcOffset) {

  if(aSrcOffset<0)
    aSrcOffset=(PRInt32)aDest.mLength-1;

  if(aCount<0)
    aCount = aSrcOffset;

  size_t theCharSize=sizeof(CharType1);
  if((0<aTarget.mLength) && (0<aDest.mLength) && ((PRUint32)aSrcOffset<aDest.mLength)) {

    //We'll only search if the given aChar is within the normal ascii a range,
    //(Since this string is definitely within the ascii range).

    if(0<aCount) {

      const CharType1* root      = aDest.mBuffer;
      const CharType1* rightmost = root+aSrcOffset;  
      const CharType1* min       = rightmost-aCount+1;
      const CharType1* leftmost  = (min<root) ? root : min;

      PRInt32 cmp=0;
      while(leftmost<=rightmost) {

        if(aIgnoreCase)
          cmp = SVCompareCase_<CharType1,CharType2>(rightmost,rightmost+aTarget.mLength,aTarget.mBuffer) ;
        else cmp =SVCompare_<CharType1,CharType2>(rightmost,rightmost+aTarget.mLength,aTarget.mBuffer);

        if(0==cmp) {
          return rightmost-root;
        }
        rightmost--;
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
 *  @param   anOffset is the start pos to begin searching
 *  @param   aChar is the target character we're looking for
 *  @param   aIgnorecase tells us whether to use a case sensitive search
 *  @param   aCount tells us how many characters to iterate through (which may be different than aLength); -1 means use full length.
 *  @return  index of pos if found, else -1 (kNotFound)
 */
template <class CharType>
PRInt32 SVRFindChar(const nsStringValueImpl<CharType>& aDest,PRUnichar aChar,PRBool aIgnoreCase,PRInt32 aCount,PRInt32 aSrcOffset) {

  if(aSrcOffset<0)
    aSrcOffset=(PRInt32)aDest.mLength-1;

  if(aCount<0)
    aCount = aSrcOffset;

  size_t theCharSize=sizeof(CharType);
  if((2==theCharSize) || (aChar<256)) {
    if((0<aDest.mLength) && ((PRUint32)aSrcOffset<aDest.mLength)) {

      //We'll only search if the given aChar is within the normal ascii a range,
      //(Since this string is definitely within the ascii range).
 
      if(0<aCount) {

        const CharType* root      = aDest.mBuffer;
        const CharType* rightmost = root+aSrcOffset;  
        const CharType* min       = rightmost-aCount+1;
        const CharType* leftmost  = (min<root) ? root : min;

        CharType theChar=(CharType)aChar;
        if(aIgnoreCase) {
    
          while(leftmost<=rightmost){
            if(nsCRT::ToUpper(*rightmost)==theChar)
              return rightmost-root;
            --rightmost;
          }
        }
        else {

          while(leftmost<=rightmost){
            if((*rightmost)==theChar)
              return rightmost-root;
            --rightmost;
          }
        }
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
 *  @param   anOffset is the start pos to begin searching
 *  @param   aChar is the target character we're looking for
 *  @param   aIgnorecase tells us whether to use a case sensitive search
 *  @param   aCount tells us how many characters to iterate through (which may be different than aLength); -1 means use full length.
 *  @return  index of pos if found, else -1 (kNotFound)
 */
template <class CharType1,class CharType2>
inline PRInt32 SVRFindCharInSet(const nsStringValueImpl<CharType1>& aDest,const nsStringValueImpl<CharType2>& aSet,PRBool aIgnoreCase,PRInt32 aCount,PRInt32 aSrcOffset) {

  if(aSrcOffset<0)
    aSrcOffset=(PRInt32)aDest.mLength-1;

  if(aCount<0)
    aCount = aSrcOffset;

  if((0<aDest.mLength) && ((PRUint32)aSrcOffset<aDest.mLength)) {

    //We'll only search if the given aChar is within the normal ascii a range,
    //(Since this string is definitely within the ascii range).

    if(0<aCount) {

      const CharType1* root      = aDest.mBuffer;
      const CharType1* rightmost = root+aSrcOffset; 
  
      while(root<=--rightmost){

        CharType1 theChar=(aIgnoreCase) ? nsCRT::ToUpper(*rightmost) : *rightmost;
        PRInt32 thePos=SVFindChar<CharType2>(aSet,theChar,aIgnoreCase,aSet.mLength,0);
        if(kNotFound<thePos) {
          return rightmost-root;
        }
      }
    }
  }

  return kNotFound;
}

/**
 * This method removes characters from the given set from this string.
 * NOTE: aSet is a char*, and it's length is computed using strlen, which assumes null termination.
 *
 * @update  rickg 03.01.2000
 * @param aDest
 * @param aSet
 * @param aEliminateLeading
 * @param aEliminateTrailing
 * @return nothing
 */
template <class CharType>
nsresult SVTrim(nsStringValueImpl<CharType>& aDest,const char* aSet,PRBool aEliminateLeading,PRBool aEliminateTrailing){
  
  nsresult result=NS_OK;

  if(0<aDest.mLength) {
    CharType *root= aDest.mBuffer;
    CharType *end = root+aDest.mLength;

    nsStringValueImpl<char> theSet(const_cast<char*>(aSet));


    if(aEliminateTrailing) {

      while(root<=--end) {
        CharType theChar=*end;
        PRInt32 thePos=SVFindChar<char>(theSet,theChar,PR_FALSE,theSet.mLength,0);
        if(kNotFound==thePos) 
          break;
      }
      *(end+1)=0;
      aDest.mLength=(end-root)+1;
    }

    CharType *cp = root-1;

    if(aEliminateLeading) {
      while(++cp<end) {
        CharType theChar=*cp;
        PRInt32 thePos=SVFindChar<char>(theSet,theChar,PR_FALSE,theSet.mLength,0);
        if(kNotFound==thePos) 
          break;
      }
      size_t theCount=cp-root;
      if(0<theCount) {
        result=SVDelete(aDest,0,theCount);
      }
    }
  }

  return result;
}

/**
 * This method removes characters from the given set from this string.
 * NOTE: aSet is a char*, and it's length is computed using strlen, which assumes null termination.
 *
 * @update  rickg 03.01.2000
 * @param aDest
 * @param aSet
 * @param aEliminateLeading
 * @param aEliminateTrailing
 * @return nothing
 */
template <class CharType>
nsresult SVStripCharsInSet(nsStringValueImpl<CharType>& aDest,const char* aSet,PRInt32 aCount,PRInt32 aSrcOffset) {

  if((0<aDest.mLength) && aSet && (aSrcOffset<(PRInt32)aDest.mLength)){

    CharType* left= aDest.mBuffer+aSrcOffset;
    CharType* theWriteCP= left;
    const CharType* last= left+aCount;
    const CharType* max = left+aDest.mLength;
    const CharType* end = (last<max) ? last : max;

    nsStringValueImpl<char> theSet(const_cast<char*>(aSet));

    while(left<end){
      CharType theChar=*left;
      PRInt32 thePos=SVFindChar<char>(theSet,theChar,PR_FALSE,theSet.mLength,0);
      if(kNotFound==thePos) {
        *theWriteCP++=*left;
      }
      ++left;
    } //while
    *theWriteCP=0;
    aDest.mLength-=(left-theWriteCP);
  } //if
  return NS_OK;
}


/**
 * This method removes a given char the underlying stringvalue
 *
 * @update	rickg 03.01.2000
 * @param  aDest is the stringvalue to be modified
 * @param  anOffset tells us where in source to start copying
 * @param  aCount tells us the (max) # of chars to copy
 */
template <class CharType>
nsresult SVStripChar( nsStringValueImpl<CharType>& aDest, CharType aChar,PRInt32 aCount,PRInt32 anOffset) {

  nsresult result=NS_OK;

  if(anOffset<(PRInt32)aDest.mLength){

    CharType *theCP=&aDest.mBuffer[anOffset];
    CharType *theWriteCP=theCP;
    CharType *theEndCP=theCP+aDest.mLength;

    while(theCP<theEndCP) {
      if(*theCP!=aChar) {
        *theWriteCP++=*theCP;
      }
      theCP++;
    }
    *theWriteCP=0;
    aDest.mLength-=(theCP-theWriteCP);

  }
  return result;
}

/**
 * This method removes a given char the underlying stringvalue
 *
 * @update	rickg 03.01.2000
 * @param  aDest is the stringvalue to be modified
 * @param  anOffset tells us where in source to start copying
 * @param  aCount tells us the (max) # of chars to copy
 */
template <class CharType1,class CharType2>
nsresult SVReplace( nsStringValueImpl<CharType1> &aDest,
                    const nsStringValueImpl<CharType2> &aTarget, 
                    const nsStringValueImpl<CharType2> &aNewValue,
                    PRInt32 aCount,
                    PRInt32 anOffset) {

  nsresult result=NS_OK;

  if((aTarget.mLength) && (aNewValue.mLength)) {

    nsStringValueImpl<CharType1> theTempString;
    PRUint32 thePrevOffset=0;
    PRInt32 thePos=SVFind(aDest,aTarget,PR_FALSE,aDest.mLength,thePrevOffset);

    while(kNotFound<thePos) {
      if(0<thePos) {
        SVAppend(theTempString,aDest,thePos-thePrevOffset,thePrevOffset);
      }
      SVAppend(theTempString,aNewValue,aNewValue.mLength,0);
      thePrevOffset=thePos+aTarget.mLength;
      thePos=SVFind(aDest,aTarget,PR_FALSE,aDest.mLength-thePrevOffset,thePrevOffset);
    }
    if(thePrevOffset<aDest.mLength) {
      SVAppend(theTempString,aDest,aDest.mLength-thePrevOffset,thePrevOffset);
    }
    SVFree(aDest);
    aDest=theTempString;
    theTempString.mBuffer=0;
  }

  return result;
}


/**
 * This method removes a given char the underlying stringvalue
 *
 * @update	rickg 03.01.2000
 * @param  aDest is the stringvalue to be modified
 * @param  anOffset tells us where in source to start copying
 * @param  aCount tells us the (max) # of chars to copy
 */
template <class CharType>
nsresult SVReplaceChar( nsStringValueImpl<CharType>& aDest, CharType anOldChar, CharType aNewChar, PRInt32 aCount,PRInt32 anOffset) {

  nsresult result=NS_OK;

  if(anOffset<(PRInt32)aDest.mLength){

    CharType *theCP=&aDest.mBuffer[anOffset];
    CharType *theWriteCP=theCP;
    CharType *theEndCP=theCP+aDest.mLength;

    while(theCP<theEndCP) {
      if(*theCP==anOldChar) {
        *theCP=aNewChar;
      }
      theCP++;
    }

  }
  return result;
}

/**
 * This method removes characters from the given set from this string.
 * NOTE: aSet is a char*, and it's length is computed using strlen, which assumes null termination.
 *
 * @update  rickg 03.01.2000
 * @param aDest
 * @param aSet
 * @param aEliminateLeading
 * @param aEliminateTrailing
 * @return nothing
 */
template <class CharType>
nsresult SVReplaceCharsInSet(nsStringValueImpl<CharType>& aDest,const nsStringValueImpl<char> &aSet,PRUnichar aNewChar,PRInt32 aCount,PRInt32 anOffset) {

  if((0<aDest.mLength) && (anOffset<(PRInt32)aDest.mLength)){

    CharType* left= aDest.mBuffer+anOffset;
    const CharType* theWriteCP= left;
    const CharType* last= left+aCount;
    const CharType* max = left+aDest.mLength;
    const CharType* end = (last<max) ? last : max;

    nsStringValueImpl<char> theSet(aSet);

    while(left<end){
      CharType theChar=*left;
      PRInt32 thePos=SVFindChar(theSet,theChar,PR_FALSE,theSet.mLength,0);
      if(kNotFound!=thePos) {
        *left=(CharType)aNewChar;
      }
      ++left;
    } //while
  } //if

  return NS_OK;
}

/**
 * This method removes characters from the given set from this string.
 * NOTE: aSet is a char*, and it's length is computed using strlen, which assumes null termination.
 *
 * @update  rickg 03.01.2000
 * @param aDest
 * @param aSet
 * @param aEliminateLeading
 * @param aEliminateTrailing
 * @return nothing
 */
template <class CharType>
nsresult SVCompressSet(nsStringValueImpl<CharType>& aDest,nsStringValueImpl<char> &aSet, PRUnichar aChar,PRBool aEliminateLeading=PR_TRUE,PRBool aEliminateTrailing=PR_TRUE){

    nsresult result=NS_OK;

    if(0<aDest.mLength){

      result=SVReplaceCharsInSet<CharType>(aDest, aSet, aChar, aDest.mLength,0);
      SVTrim<CharType>(aDest,aSet,aEliminateLeading,aEliminateTrailing);

      CharType*  root = aDest.mBuffer;
      CharType*  end =  root + aDest.mLength;
      CharType*  to = root;

        //this code converts /n, /t, /r into normal space ' ';
        //it also compresses runs of whitespace down to a single char...
      PRUint32 aSetLen=strlen(aSet);
      while (root < end) {
        CharType theChar = *root++;
        if((255<theChar) || (kNotFound!=SVFindChar<char>(aSet,theChar,PR_FALSE,aSet.mLength,0))){
          *to++=theChar;
          while (root <= end) {
            theChar = *root++;
            PRInt32 thePos=SVFindChar<char>(aSet,theChar,PR_FALSE,aSet.mLength,0);
            if(kNotFound==thePos){
              *to++ = theChar;
              break;
            }
          }
        } else {
          *to++ = theChar;
        }
      }
      *to = 0;
      aDest.mLength = to - (CharType*)aDest.mBuffer;

    }

  return result;
}


template <class CharType>
void SVToLowerCase(nsStringValueImpl<CharType> &aString) {

  CharType *theCP=aString.mBuffer;
  CharType *theEndCP=theCP+aString.mLength;

  while(theCP<theEndCP) {
    CharType theChar=*theCP;
    if ((theChar >= 'A') && (theChar <= 'Z')) {
      *theCP = 'a' + (theChar - 'A');
    }
    theCP++;
  }

}

template <class CharType>
void SVToUpperCase(nsStringValueImpl<CharType> &aString) {
  CharType *theCP=aString.mBuffer;
  CharType *theEndCP=theCP+aString.mLength;

  while(theCP<theEndCP) {
    CharType theChar=*theCP;
    if ((theChar>= 'a') && (theChar<= 'z')) {
      *theCP= 'A' + (theChar - 'a');
    }
    theCP++;
  }

}

template <class CharType>
PRInt32 SVCountChar(const nsStringValueImpl<CharType> &aString,PRUnichar aChar) {  
  PRInt32 result=0;
  if(aString.mLength) {
    const CharType* cp=aString.mBuffer-1;
    const CharType* endcp=cp+aString.mLength;
    
    while(++cp<endcp) {
      if(*cp==aChar)
        result++;
    }
  }
  return result;
}

  //This will not work correctly for any unicode set other than ascii
template <class CharType>
void SVDebugDump(const nsStringValueImpl<CharType> &aString) { 
  if(aString.mLength) {
    
    CharType* cp=aString.mBuffer;
    CharType* endcp=cp+aString.mLength;

    char  theBuffer[256];
    char* outp=theBuffer;
    char* outendp=&theBuffer[255];

    while(++cp<endcp) {
      if(outp==outendp) {
        *outp=0; //write null
        printf("%s",theBuffer);
        outp=theBuffer;
      }
      else {
        *outp++=(char)*cp;
      }
    } //while
    *outp=0; //force a null
    printf("%s",theBuffer);
  }
}

/**
 * Perform string to int conversion.
 * @param   aErrorCode will contain error if one occurs
 * @param   aRadix tells us which radix to assume; kAutoDetect tells us to determine the radix for you.
 * @return  int rep of string value, and possible (out) error code
 */
template <class CharType>
PRInt32 SVToInteger(const nsStringValueImpl<CharType> &aString,PRInt32* anErrorCode,PRUint32 aRadix=kAutoDetect) {

  CharType*   cp=aString.mBuffer;
  PRInt32     theRadix = (kAutoDetect==aRadix) ? 10 : aRadix;
  PRInt32     result=0;
  PRBool      negate=PR_FALSE;
  PRUnichar   theChar=0;

  *anErrorCode=NS_ERROR_ILLEGAL_VALUE;

  if(cp) {

    //begin by skipping over leading chars that shouldn't be part of the number...
  
    CharType*   endcp=cp+aString.mLength;
    PRBool      done=PR_FALSE;
  
    while((cp<endcp) && (!done)){
      theChar=*cp;
      switch(*cp++) {
        case 'a': case 'b': case 'c': case 'd': case 'e': case 'f':
        case 'A': case 'B': case 'C': case 'D': case 'E': case 'F':
          theRadix=16;
          done=PR_TRUE;
          break;
        case '0': case '1': case '2': case '3': case '4': 
        case '5': case '6': case '7': case '8': case '9':
          done=PR_TRUE;
          break;
        case '-': 
          negate=PR_TRUE; //fall through...
          break;
        case 'X': case 'x': 
          theRadix=16;
          break; 
        default:
          break;
      } //switch
    }

      //if you don't have any valid chars, return 0, but set the error;
    if(cp<=endcp) {

      *anErrorCode = NS_OK;

        //now iterate the numeric chars and build our result
      CharType* first=--cp;  //in case we have to back up.

      while(cp<=endcp){
        theChar=*cp++;
        if(('0'<=theChar) && (theChar<='9')){
          result = (theRadix * result) + (theChar-'0');
        }
        else if((theChar>='A') && (theChar<='F')) {
          if(10==theRadix) {
            if(kAutoDetect==aRadix){
              theRadix=16;
              cp=first; //backup
              result=0;
            }
            else {
              *anErrorCode=NS_ERROR_ILLEGAL_VALUE;
              result=0;
              break;
            }
          }
          else {
            result = (theRadix * result) + ((theChar-'A')+10);
          }
        }
        else if((theChar>='a') && (theChar<='f')) {
          if(10==theRadix) {
            if(kAutoDetect==aRadix){
              theRadix=16;
              cp=first; //backup
              result=0;
            }
            else {
              *anErrorCode=NS_ERROR_ILLEGAL_VALUE;
              result=0;
              break;
            }
          }
          else {
            result = (theRadix * result) + ((theChar-'a')+10);
          }
        }
        else if(('X'==theChar) || ('x'==theChar) || ('#'==theChar) || ('+'==theChar)) {
          continue;
        }
        else {
          //we've encountered a char that's not a legal number or sign
          break;
        }
      } //while
      if(negate)
        result=-result;
    } //if
  }
  return result;
}

#endif


