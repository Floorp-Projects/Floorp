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
 *  03.10.2000: Fixed off-by-1 error in StripChar() (rickg)
 *  03.20.2000: Ran regressions by comparing operations against original nsString (rickg)
 *  
 * To Do:
 *
 *  1. Use shared buffer for empty strings
 *  2. Test refcounting
 *  3. Implement copy semantics between autostring and nsCSubsumeString to ensure
 *     that actual character copying occurs if necessary.
 *
 *************************************************************************************/

#ifndef NS_BUFFERMANAGER_
#define NS_BUFFERMANAGER_


#include "nsStringValue.h"
#include "nslog.h"

NS_IMPL_LOG(nsBufferManagerLog, 0)
#define PRINTF NS_LOG_PRINTF(nsBufferManagerLog)
#define FLUSH  NS_LOG_FLUSH(nsBufferManagerLog)


#define NS_MEMORY_ERROR 1000


// uncomment the following line to caught nsString char* casting problem
//#define DEBUG_ILLEGAL_CAST_UP
//#define DEBUG_ILLEGAL_CAST_DOWN

#if  defined(DEBUG_ILLEGAL_CAST_UP) || defined(DEBUG_ILLEGAL_CAST_DOWN) 

static PRBool track_illegal = PR_TRUE;
static PRBool track_latin1  = PR_TRUE;

#ifdef XP_UNIX
#include "nsTraceRefcnt.h"
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
        nsTraceRefcnt::WalkTheStack(mFile);
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
        nsTraceRefcnt::WalkTheStack(mFile);
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


/*************************************************************************************
 *
 *  These methods offer the low level buffer manipulation routines used throughout
 *  the string hierarchy. T1 and T2 will be types of stringvalues. (rickg)
 *
 *************************************************************************************/


template <class CharType>
inline void SVAddNullTerminator( nsStringValueImpl<CharType>& aDest) {
  if(aDest.mBuffer) {
    aDest.mBuffer[aDest.mLength]=0;
  }
}

template <class CharType>
inline CharType ToLower(CharType aChar) {
  if((aChar>='A') && (aChar<='Z'))
    aChar+=32;
  return aChar;
}

static const PRUnichar gCommonEmptyBuffer[1] = {0};
static nsresult gStringAcquiredMemory = NS_OK;



template <class CharType>
inline nsresult SVAlloc( nsStringValueImpl<CharType>& aDest, PRUint32 aCount) {

  nsresult result=NS_OK;


  //we're given the acount value in charunits; now scale up to next multiple.
  aDest.mCapacity=kDefaultStringSize;
  while(aDest.mCapacity<aCount){ 
		aDest.mCapacity<<=1;
  }

  aDest.mBuffer = new CharType[1+aDest.mCapacity];

  // PRUint32 theSize=(theNewCapacity<<aDest.mCharSize);
  // aDest.mBuffer = (char*)nsMemory::Alloc(theSize);

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
  aDest.mRefCount--;
  
  if((aDest.mBuffer) && (!aDest.mRefCount)){    
    delete [] aDest.mBuffer;   // nsMemory::Free(aDest.mBuffer);
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
  nsresult result=SVAlloc(temp,aCount);
  if(NS_SUCCEEDED(result)) {

    if(aDest.mBuffer) {
      if(0<aDest.mLength) {
        memcpy(temp.mBuffer,aDest.mBuffer,aDest.mLength*sizeof(CharType));
        temp.mLength=aDest.mLength;        
      }
      SVFree(aDest);
    }
    aDest=temp;
    aDest.mBuffer[aDest.mLength]=0;
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
 * @param   aCount tells us how many chars to copy from aSource
 */
inline nsresult SVCopyBuffer_( char* aDest, const char* aSource, PRUint32 aCount) {
  memcpy(aDest,aSource,aCount);
  return NS_OK;
}

inline nsresult SVCopyBuffer_( PRUnichar* aDest, const PRUnichar* aSource, PRUint32 aCount) {
  memcpy(aDest,aSource,aCount*sizeof(PRUnichar));
  return NS_OK;
}

/**
 * This is the low-level buffer copying routine between 2 stringvalues.
 *
 * @update	rickg 03.01.2000
 * @param   aDest is the destination stringvalue
 * @param   aSource is the source stringvalue (you must account for offset yourself in calling routine)
 * @param   aCount tells us how many chars to copy from aSource
 */
inline nsresult SVCopyBuffer_( char* aDest, const PRUnichar* aSource, PRUint32 aCount) {
  const PRUnichar* aLast=aSource+aCount;

#ifdef  DEBUG_ILLEGAL_CAST_UP
  int i=0;
  for(i=0;i<aCount;i++){
    if(aSource[i]>255) {
      NS_ASSERTION( (aSource[i] < 256), "data in U+0100-U+FFFF will be lost");
      if(track_illegal) {
        if(track_latin1) {
          if(aSource[i] & 0xFF80)
            illegal = PR_TRUE;
        } else {
          if(aSource[i] & 0xFF00)
            illegal = PR_TRUE;
        } // track_latin1
      } // track_illegal
      break;
    }

  }
#endif

  while(aSource<aLast) {
    *aDest++=(char)*aSource++;
  }
  return NS_OK;
}

inline nsresult SVCopyBuffer_( PRUnichar* aDest, const char* aSource, PRUint32 aCount) {
  const char* aLast=aSource+aCount;
  while(aSource<aLast) {
    *aDest++=(PRUnichar)*aSource++;
  }

#ifdef  DEBUG_ILLEGAL_CAST_UP
  int i=0;
  for(i=0;i<aCount;i++){
    if(track_illegal && track_latin1 && ((aDest[i])& 0x80)) {
      TRACE_ILLEGAL_CAST_UP((!illegal), aSource, "illegal cast up in copying 1-to-2 byte buffers.");
      break;
    }

  }
#endif

  return NS_OK;
}




//----------------------------------------------------------------------------------------


/**
 * This method deletes chars from the given str. 
 * The given allocator may choose to resize the str as well.
 *
 * @update	rickg 03.01.2000
 * @param  aDest is the stringvalue to be modified
 * @param  aDestOffset tells us where in dest to start deleting
 * @param  aCount tells us the (max) # of chars to delete (-1 means delete the rest of the string starting at offset)
 */
template <class CharType>
inline nsresult SVDelete( nsStringValueImpl<CharType>& aDest,PRUint32 anOffset,PRInt32 aCount=-1){
  nsresult result=NS_OK;

  PRInt32 theLength =(aCount<0) ? aDest.mLength : aCount;

  CharType* root_cp   = aDest.mBuffer;
  CharType* null_cp   = root_cp+aDest.mLength;
  CharType* offset_cp = root_cp+anOffset;   //dst1

  if(offset_cp<null_cp) {
      
      //Proceed only if the offset is in range

    CharType* max_cp  = offset_cp+aCount;   //dst2

    if(max_cp<null_cp) {
      aDest.mLength-=(max_cp-offset_cp);
      memmove(offset_cp,max_cp,(null_cp-max_cp)*sizeof(CharType));
    }
    else aDest.mLength=anOffset;
    aDest.mBuffer[aDest.mLength]=0;
  }

  return result;
}

/**
 * This method is used to truncate the given string.
 * The given allocator may choose to resize the str as well (but it's not likely).
 *
 * @update	rickg 03.01.2000
 * @param  aDest is the stringvalue to be modified
 * @param  anOffset tells us where in dest to truncate
 */
template <class CharType>
inline void SVTruncate( nsStringValueImpl<CharType>& aDest,PRUint32 anOffset=0) {
  if(anOffset<aDest.mLength){
    aDest.mLength=anOffset;
    aDest.mBuffer[aDest.mLength]=0;
  }
}

/**
 * These methods are used to append content to the given stringvalue
 *
 * @update	rickg 03.01.2000
 * @param  aDest is the stringvalue to be modified
 * @param  aSource is the buffer to be copied from
 * @param  anOffset tells us where in source to start copying
 * @param  aCount tells us the (max) # of chars to append
 */
template <class CharType1, class CharType2>
inline nsresult SVAppend( nsStringValueImpl<CharType1>& aDest,const nsStringValueImpl<CharType2>& aSource,PRInt32 aCount=-1,PRUint32 aSrcOffset=0){

  nsresult result=NS_OK;

  if(aSrcOffset<aSource.mLength){
    PRInt32 theRealLen=(aCount<0) ? aSource.mLength : (aCount<(PRInt32)aSource.mLength) ? aCount : aSource.mLength;
    PRInt32 theLength=(aSrcOffset+theRealLen<(PRInt32)aSource.mLength) ? theRealLen : (aSource.mLength-aSrcOffset);
    if(0<theLength){
      
      PRBool isBigEnough=PR_TRUE;
      if(aDest.mLength+theLength > aDest.mCapacity) {
        result=SVRealloc(aDest,aDest.mLength+theLength);
      }

      if(NS_SUCCEEDED(result)) {

        //now append new chars, starting at offset

        CharType1       *to_cp    = aDest.mBuffer+aDest.mLength;
        const CharType2 *from_cp  = aSource.mBuffer+aSrcOffset;

        SVCopyBuffer_(to_cp,from_cp,theLength);

        aDest.mLength+=theLength;
        aDest.mBuffer[aDest.mLength]=0;
      }

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
inline nsresult SVAppendReadable( nsStringValueImpl<CharType1>& aDest,const nsAReadableString& aSource,PRInt32 aCount,PRUint32 anSrcOffset){
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
inline nsresult SVAssign(nsStringValueImpl<CharType1>& aDest,const nsStringValueImpl<CharType2>& aSource,PRInt32 aCount=-1,PRUint32 aSrcOffset=0) {
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
inline nsresult SVInsert( nsStringValueImpl<CharType1>& aDest,PRUint32 aDestOffset,const nsStringValueImpl<CharType2>& aSource,PRInt32 aCount=-1,PRUint32 aSrcOffset=0) {
  nsresult result=NS_OK;

  //there are a few cases for insert:
  //  1. You're inserting chars into an empty string (assign)
  //  2. You're inserting onto the end of a string (append)
  //  3. You're inserting onto the 1..n-1 pos of a string (the hard case).
  if(0<aSource.mLength){
    if(aDest.mLength){
      if(aDestOffset<aDest.mLength){
        PRUint32 theRealLen=(aCount<0) ? aSource.mLength : (aCount<(PRInt32)aSource.mLength) ? aCount : aSource.mLength;

        PRUint32 theLength=(aSrcOffset+theRealLen<aSource.mLength) ? theRealLen : (aSource.mLength-aSrcOffset);

        if(aSrcOffset<(PRInt32)aSource.mLength) {
            //here's the only new case we have to handle. 
            //chars are really being inserted into our buffer...

          CharType2 *from_src=aSource.mBuffer+aSrcOffset;

          if(aDest.mLength+theLength > aDest.mCapacity) {

            nsStringValueImpl<CharType1> theTempStr;
            result=SVAlloc(theTempStr,aDest.mLength+theLength);

            if(NS_SUCCEEDED(result)) {

              CharType1 *to_cp=theTempStr.mBuffer;

              memcpy(to_cp,aDest.mBuffer,aDestOffset*sizeof(CharType1)); //copy front of existing string...

              to_cp+=aDestOffset;
              SVCopyBuffer_(to_cp,from_src,theLength);  //now insert the new data...

              PRUint32 theRemains=aDest.mLength-aDestOffset;
              if(theRemains) {
                CharType1 *from_dst=to_cp;
                to_cp+=theLength;
                memcpy(to_cp,from_dst,theRemains*sizeof(CharType1));
              }

              SVFree(aDest);
              aDest=theTempStr;
              theTempStr.mBuffer=0; //make sure to null this out so that you don't lose the buffer you just stole...
            }

          }

          else {
              //if you're here, it's because we have enough room to insert the new chars into the buffer
              //that we already possess. 

            CharType1* root_cp  = aDest.mBuffer;
            CharType1* end_cp   = root_cp+aDest.mLength;
            CharType1* offset_cp= root_cp+aDestOffset;
            CharType1* max_cp   = offset_cp+aCount;

            memmove(max_cp,offset_cp,sizeof(CharType1)*(end_cp-offset_cp));
              
            SVCopyBuffer_(offset_cp,from_src,theLength);

            //finally, make sure to update the string length...
            aDest.mLength+=theLength;
            aDest.mBuffer[aDest.mLength]=0;
          }
        }//if
        //else nothing to do!
      }
      else {
        SVAppend(aDest,aSource,aCount,0);
      }
    }
    else {
      SVAppend(aDest,aSource,aCount,0);
    }
  }
  return result;
}


/**
 * This method compares characters strings of two types, and ignores case
 *
 * @update  rickg 03.01.2000
 * @param aLeft
 * @param anEnd
 * @param aTarget
 * @return  aDest<aSource=-1;aDest==aSource==0;aDest>aSource=1
 */
template <class CharType1,class CharType2>
inline PRInt32 SVCompareCase_(const CharType1* aLeft,const CharType1* anEnd,const CharType2* aTarget){

  while(aLeft<anEnd){
    PRUnichar theChar=(PRUnichar)ToLower(*aLeft++);
    PRUnichar theTargetChar=(PRUnichar)ToLower(*aTarget++);
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
 * @param anEnd
 * @param aTarget
 * @return  aDest<aSource=-1;aDest==aSource==0;aDest>aSource=1
 */
inline PRInt32 SVCompare_(const char* aLeft,const char* anEnd,const char* aTarget){
  size_t theCount=anEnd-aLeft;
  return memcmp(aLeft,aTarget,theCount);
}

/**
 * This method compares characters strings of two types (case sensitive)
 *
 * @update  rickg 03.01.2000
 * @param aLeft
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
 * @param aSource
 * @param aIgnoreCase
 * @param aCount --- tells us how many iterations to make.
 * @return  aDest<aSource=-1;aDest==aSource==0;aDest>aSource=1
 */
template <class CharType1,class CharType2>
inline PRInt32 SVCompare(const nsStringValueImpl<CharType1>& aDest,const nsStringValueImpl<CharType2>& aSource,PRBool aIgnoreCase, PRUint32 aCount){

  PRInt32 result=0;

  if((void*)aDest.mBuffer!=(void*)aSource.mBuffer) {
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

      const CharType1* root_cp  = aDest.mBuffer;
      const CharType1* last_cp  = root_cp+theCount;
      const CharType1* null_cp  = root_cp+aDest.mLength;
      const CharType1* end_cp = (last_cp<null_cp) ? last_cp : null_cp;
      const CharType2* target = aSource.mBuffer;

      if(aIgnoreCase)
        result = SVCompareCase_(root_cp,end_cp,target) ;
      else result =SVCompare_(root_cp,end_cp,target);

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
  }
  return result;

}

template <class CharType>
inline PRInt32 SVCompareChars(const nsStringValueImpl<CharType>& aLHS,const nsStringValueImpl<CharType>& aRHS,PRBool aIgnoreCase,PRInt32 aCount) {

  PRInt32 result=0;

  if(aLHS.mBuffer!=aRHS.mBuffer) {
    if(!aIgnoreCase) {
      PRInt32 aMax = (aLHS.mLength<aRHS.mLength) ? aLHS.mLength : aRHS.mLength;
      if((0<aCount) && (aCount<aMax))
        aMax=aCount;

      result=memcmp(aLHS.mBuffer,aRHS.mBuffer,aMax*sizeof(CharType));

      if(0==result) {
        if(-1==aCount) {
          if (aLHS.mLength != aRHS.mLength) {
            result = (aLHS.mLength<aRHS.mLength) ? -1 : 1;
          }
        } //if
      } //if
    }
    else result=SVCompare(aLHS,aRHS,aIgnoreCase,aCount);
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
inline PRInt32 SVFindOrig(const nsStringValueImpl<CharType1>& aDest,const nsStringValueImpl<CharType2>& aTarget,PRBool aIgnoreCase,PRInt32 aCount=-1,PRUint32 aSrcOffset=0) {

  PRUint32 minlen=(aTarget.mLength<aDest.mLength) ? aTarget.mLength : aDest.mLength;
  
  aCount = (-1==aCount) ? aDest.mLength-minlen+1 : aCount;
  
  if((0<aCount) && minlen) {

    const CharType1 *root_cp    = aDest.mBuffer;
    const CharType1 *offset_cp  = root_cp+aSrcOffset;  
    const CharType1 *null_cp    = root_cp+aDest.mLength;

    if(offset_cp<null_cp) {
      PRInt32 cmp=0;

      const CharType1* last_cp  = offset_cp+aCount;
      const CharType1* end_cp   = (last_cp<null_cp) ? last_cp : null_cp;

      if(aIgnoreCase) {
        while(offset_cp<=end_cp) {
          cmp = SVCompareCase_(offset_cp,offset_cp+minlen,aTarget.mBuffer) ;
          if(0==cmp) {
            return offset_cp-root_cp;
          }
          offset_cp++;
        }
      }
      else {

        while(offset_cp<=null_cp) {
          cmp=SVCompare_(offset_cp,offset_cp+minlen,aTarget.mBuffer);
          if(0==cmp) {
            return offset_cp-root_cp;
          }
          offset_cp++;
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
 * @param aTarget
 * @param aIgnoreCase
 * @param aCount : tells us how many iterations to make, starting at offset
 * @param aOffset: tells us where to start searching within aDest
 * @return nothing
 */
template <class CharType1,class CharType2>
inline PRInt32 SVFind(const nsStringValueImpl<CharType1>& aDest,const nsStringValueImpl<CharType2>& aTarget,PRBool aIgnoreCase,PRInt32 aCount=-1,PRUint32 aSrcOffset=0) {

  PRUint32 minlen=(aTarget.mLength<aDest.mLength) ? aTarget.mLength : aDest.mLength;
  
  aCount = (-1==aCount) ? aDest.mLength-minlen+1 : aCount;
  
  if((0<aCount) && minlen) {

    const CharType1* root_cp    = aDest.mBuffer;
    const CharType1* offset_cp  = root_cp+aSrcOffset;  
    const CharType1* null_cp    = root_cp+aDest.mLength;

    if(offset_cp<null_cp) {
      PRInt32 cmp=0;

      const CharType1* last_cp  = offset_cp+aCount;
      const CharType1* end_cp   = (last_cp<null_cp) ? last_cp : null_cp;

      if(aIgnoreCase) {
        while(offset_cp<=end_cp) {
          cmp = SVCompareCase_(offset_cp,offset_cp+minlen,aTarget.mBuffer) ;
          if(0==cmp) {
            return offset_cp-root_cp;
          }
          offset_cp++;
        }
      }
      else {

        //XXX Hack alert. I'm only playing around here with a lookaside buffer until I can get around to implementing
        //    a NMP search suitable for unicode chars. 

        const CharType1*  theLATable[256];
        PRInt32     theLACount=0;

        const CharType1 *leftLA=offset_cp;
        const CharType1 *endLA=end_cp;
        const CharType1 *lastLA=last_cp;

        const CharType2 targetFirst=aTarget.mBuffer[0];
        const CharType2 targetLast=aTarget.mBuffer[aTarget.mLength-1];
        size_t countLA=aTarget.mLength-1;

        while(leftLA<=endLA) {
          if((*leftLA==targetFirst) && (*(leftLA+countLA)==targetLast)) {
            theLATable[theLACount++]=leftLA;
          }
          leftLA++;
        }
        theLATable[theLACount]=0;


        PRInt32 theLAIndex=0;
        while(offset_cp=theLATable[theLAIndex++]) {
          cmp =SVCompare_(offset_cp,offset_cp+minlen,aTarget.mBuffer);
          if(0==cmp) {
            return offset_cp-root_cp;
          }
        } //while
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
PRInt32 SVFindChar(const nsStringValueImpl<CharType>& aDest,PRUnichar aChar,PRBool aIgnoreCase,PRInt32 aCount=-1,PRUint32 aSrcOffset=0) {

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

        const CharType* root_cp   = aDest.mBuffer;
        const CharType* offset_cp = root_cp+aSrcOffset;
        const CharType* last_cp   = offset_cp+aCount;
        const CharType* max_cp    = offset_cp+aDest.mLength;
        const CharType* end_cp    = (last_cp<max_cp) ? last_cp : max_cp;

        if(aIgnoreCase) {
    
          PRUnichar theChar=ToLower(aChar);
          while(offset_cp<end_cp){
            if(ToLower(*offset_cp)==theChar)
              return offset_cp-root_cp;
            ++offset_cp;
          }
        }
        else if (1==theCharSize){

          PRInt32 theMax = end_cp-offset_cp;
          if(0<theMax) {
            CharType theChar=(CharType)aChar;
            const char* result=(const char*)memchr(offset_cp, theChar, theMax);
            if(result) {
              return result-(char*)root_cp;
            }
          }
        }
        else {
          while(offset_cp<end_cp){
            if(*offset_cp==aChar)
              return (offset_cp-root_cp);
            ++offset_cp;
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

    const CharType1* root_cp    = aDest.mBuffer;
    const CharType1* offset_cp  = root_cp+anOffset;
    const CharType1* last_cp    = offset_cp+aCount;
    const CharType1* nullcp     = root_cp+aDest.mLength;
    const CharType1* end_cp     = (last_cp<nullcp) ? last_cp : nullcp;

    PRInt32 cmp=0;
    while(offset_cp<=end_cp) {

      PRUnichar theChar=(PRUnichar)*offset_cp;

      PRInt32 thePos=SVFindChar(aSet,theChar,aIgnoreCase,aSet.mLength,0);
      if(kNotFound<thePos) {
        return offset_cp-root_cp;
      }
      offset_cp++;
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

      const CharType1* root_cp   = aDest.mBuffer;
      const CharType1* first_cp  = root_cp+aSrcOffset;  
      const CharType1* last_cp   = first_cp+aTarget.mLength;  
      const CharType1* min_cp    = first_cp-aCount+1;
      const CharType1* left_cp   = (min_cp<root_cp) ? root_cp : min_cp;

      PRInt32 cmp=0;
      while(left_cp<=first_cp) {

        if(aIgnoreCase)
          cmp = SVCompareCase_(first_cp,last_cp,aTarget.mBuffer) ;
        else cmp =SVCompare_(first_cp,last_cp,aTarget.mBuffer);

        if(0==cmp) {
          return first_cp-root_cp;
        }
        first_cp--;
        last_cp--;
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

      const CharType* root_cp   = aDest.mBuffer;
      const CharType* right_cp  = root_cp+aSrcOffset;  
      const CharType* min_cp    = right_cp-aCount+1;
      const CharType* left_cp   = (min_cp<root_cp) ? root_cp : min_cp;

        if(aIgnoreCase) {

          PRUnichar theChar=ToLower(aChar);
          
          while(left_cp<=right_cp){
            if(ToLower(*right_cp)==theChar)
              return right_cp-root_cp;
            --right_cp;
          }
        }
        else {

          CharType theChar=(CharType)aChar;

          while(left_cp<=right_cp){
            if((*right_cp)==theChar)
              return right_cp-root_cp;
            --right_cp;
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
 *  @param   aSrcOffset
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

      const CharType1* root_cp    = aDest.mBuffer;
      const CharType1* right_cp   = root_cp+aSrcOffset; 
  
      while(root_cp<=--right_cp){

        CharType1 theChar=(aIgnoreCase) ? ToLower(*right_cp) : *right_cp;
        PRInt32 thePos=SVFindChar(aSet,theChar,aIgnoreCase,aSet.mLength,0);
        if(kNotFound<thePos) {
          return right_cp-root_cp;
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
    CharType *root_cp = aDest.mBuffer;
    CharType *end_cp  = root_cp+aDest.mLength;

    nsStringValueImpl<char> theSet((char*)aSet);


    if(aEliminateTrailing) {

      while(root_cp<end_cp) {
        CharType theChar=*(end_cp-1);
        PRInt32 thePos=SVFindChar(theSet,theChar,PR_FALSE,theSet.mLength,0);
        if(kNotFound==thePos) 
          break;
        --end_cp;
      }
      aDest.mLength=(end_cp-root_cp);
      aDest.mBuffer[aDest.mLength]=0;
    }

    CharType *cp = root_cp-1;

    if(aEliminateLeading) {
      while(++cp<end_cp) {
        CharType theChar=*cp;
        PRInt32 thePos=SVFindChar(theSet,theChar,PR_FALSE,theSet.mLength,0);
        if(kNotFound==thePos) 
          break;
      }
      size_t theCount=cp-root_cp;
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
nsresult SVStripCharsInSet(nsStringValueImpl<CharType>& aDest,const char* aSet,PRInt32 aCount,PRInt32 anOffset) {

  if((0<aDest.mLength) && aSet && (anOffset<(PRInt32)aDest.mLength)){

    CharType  *left_cp      = aDest.mBuffer+anOffset;
    CharType  *theWriteCP   = left_cp;
    const CharType  *last_cp= left_cp+aCount;
    const CharType  *max_cp = left_cp+aDest.mLength;
    const CharType  *end_cp = (last_cp<max_cp) ? last_cp : max_cp;

    nsStringValueImpl<char> theSet((char*)aSet);

    while(left_cp<end_cp){
      CharType theChar=*left_cp;
      PRInt32 thePos=SVFindChar(theSet,theChar,PR_FALSE,theSet.mLength,0);
      if(kNotFound==thePos) {
        *theWriteCP++=*left_cp;
      }
      ++left_cp;
    } //while
    *theWriteCP=0;
    aDest.mLength-=(left_cp-theWriteCP);
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

    CharType *root_cp=aDest.mBuffer;
    CharType *theCP=root_cp+anOffset;
    CharType *end_cp=root_cp+aDest.mLength;
    CharType *theWriteCP=theCP;

    while(theCP<end_cp) {
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
 * @param  aRepCount tells us the (max) # of iterations
 */
template <class CharType1,class CharType2>
nsresult SVReplace( nsStringValueImpl<CharType1> &aDest,
                    const nsStringValueImpl<CharType2> &aTarget, 
                    const nsStringValueImpl<CharType2> &aNewValue,
                    PRInt32 aRepCount,
                    PRUint32 anOffset) {

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
 * This method removes characters from the given set from this string.
 * NOTE: aSet is a char*, and it's length is computed using strlen, which assumes null termination.
 *
 * @update  rickg 03.01.2000
 * @param aDest
 * @param aSet
 * @param aNewChar
 * @param aCount
 * @param anoffset
 * @return nothing
 */
template <class CharType>
nsresult SVReplaceCharsInSet(nsStringValueImpl<CharType>& aDest,const char* aSet,PRUnichar aNewChar,PRInt32 aCount,PRUint32 anOffset) {

  if((0<aDest.mLength) && (anOffset<aDest.mLength)){

    PRInt32  theCount = (-1==aCount) ? aDest.mLength : aCount;

    CharType *root_cp=aDest.mBuffer;
    CharType *offset_cp=root_cp+anOffset;  //was theCP
    CharType *last_cp=offset_cp+aCount;
    CharType *null_cp=root_cp+theCount;
    CharType *end_cp=(last_cp<null_cp) ? last_cp : null_cp;

    nsStringValueImpl<char> theSet((char*)aSet);
    while(offset_cp<end_cp){
      CharType theChar=*offset_cp;
      PRInt32 thePos=SVFindChar(theSet,theChar,PR_FALSE,theSet.mLength,0);
      if(kNotFound!=thePos) {
        *offset_cp=(CharType)aNewChar;
      }
      ++offset_cp;
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
nsresult SVCompressSet(nsStringValueImpl<CharType>& aDest,const char* aSet, PRUnichar aChar,PRBool aEliminateLeading=PR_TRUE,PRBool aEliminateTrailing=PR_TRUE){

    nsresult result=NS_OK;

    if(0<aDest.mLength){

      result=SVReplaceCharsInSet(aDest, aSet, aChar, aDest.mLength,0);
      SVTrim(aDest,aSet,aEliminateLeading,aEliminateTrailing);

      CharType  *root_cp  = aDest.mBuffer;
      CharType  *end_cp   = root_cp + aDest.mLength;
      CharType  *to_cp    = root_cp;

      nsStringValueImpl<char> theSet((char*)aSet);

        //this code converts /n, /t, /r into normal space ' ';
        //it also compresses runs of whitespace down to a single char...
      PRUint32 aSetLen=strlen(aSet);
      if(aSetLen) {

        while (root_cp < end_cp) {
          CharType theChar = *root_cp++;

          *to_cp++=theChar; //always copy this char
          if((theChar<256) && (kNotFound<SVFindChar(theSet,theChar,PR_FALSE,theSet.mLength,0))){
            while (root_cp <= end_cp) {
              theChar = *root_cp++;
              PRInt32 thePos=SVFindChar(theSet,theChar,PR_FALSE,theSet.mLength,0);
              if(kNotFound==thePos){
                *to_cp++ = theChar;
                break;
              }
            }
          }
        } //while

        *to_cp = 0;
        aDest.mLength = to_cp - (CharType*)aDest.mBuffer;
      }

    }

  return result;
}


template <class CharType>
void SVToLowerCase(nsStringValueImpl<CharType> &aString) {

  CharType *theCP=aString.mBuffer;
  CharType *end_cp=theCP+aString.mLength;

  while(theCP<end_cp) {
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
  CharType *end_cp=theCP+aString.mLength;

  while(theCP<end_cp) {
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

  const CharType* cp=aString.mBuffer;
  const CharType* end_cp=cp+aString.mLength;
    
  while(cp<end_cp) {
    if(*cp==aChar) {
      result++;
    }
    cp++;
  }
  return result;
}

  //This will not work correctly for any unicode set other than ascii
template <class CharType>
void SVDebugDump(const nsStringValueImpl<CharType> &aString) { 

    
  CharType* cp=aString.mBuffer;
  CharType* end_cp=cp+aString.mLength;

  char  theBuffer[256]={0};
  char* outp=theBuffer;
  char* outendp=&theBuffer[255];

  while(cp<end_cp) {
    if(outp==outendp) {
      *outp=0; //write null
      PRINTF("%s",theBuffer);
      outp=theBuffer;
      cp--;
    }
    else {
      *outp++=(char)*cp;
    }
    cp++;
  } //while
  *outp=0; //force a null
  PRINTF("%s",theBuffer);

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

    theRadix = (kAutoDetect==aRadix) ? theRadix : aRadix;
    *anErrorCode = NS_OK;

      //now iterate the numeric chars and build our result
    CharType* first=--cp;  //in case we have to back up.

    while(cp<endcp){
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
  }
  return result;
}

#endif


