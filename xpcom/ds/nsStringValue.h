
/*************************************************************************************
 *  
 * MODULES NOTES:
 *
 *  1. See nsStringImpl.h
 *  2. We still have the ownership flag (bool) as a seperate data member. This will
 *     get merged into the length or capacity field after things stabilize a bit.
 *     (Likely only a few more days).
 *  
 * Contributor(s):
 *   Rick Gessner <rickg@netscape.com>
 * 
 * History: *
 *  02.29.2000: Original files (rickg)
 *  03.02.2000: Flesh out the interface to be compatible with old library (rickg)
 *  
 *************************************************************************************/


#ifndef NSSTRINGVALUE_
#define NSSTRINGVALUE_

#include "nsIAtom.h"
#include "nscore.h"
#include "nsIAllocator.h"
#include <string.h>

#if 0
typedef int PRInt32;
typedef unsigned int PRUint32;
typedef short int PRUnichar;
typedef int nsresult;
typedef char PRBool;

#define NS_OK 0
#define NS_MEMORY_ERROR 1000
#define PR_TRUE 1
#define PR_FALSE  0
#define NS_SUCCEEDED(x) (NS_OK==x)

#endif


typedef long int  _RefCountType;

static const int kRadix10=10;
static const int kRadix16=16;
static const int kAutoDetect=100;
static const int kRadixUnknown=kAutoDetect+1;

static const int kDefaultStringSize=64;
static const int kNotFound=-1;
#define IGNORE_CASE (PR_TRUE)

class nsAReadableString {
public:
  virtual PRUint32  Length() const =0; 

};

#define MODERN_CPP  //make this false for aix/solaris...

#ifdef MODERN_CPP
#define CONST_CAST(type,arg) const_cast<type>(arg)
#else
#define CONST_CAST(type,arg) (type)arg
#endif


/***************************************************************************
 *
 *  The following (hack) template functions will go away, but for now they
 *  provide basic string copying and appending for my prototype. (rickg)
 *
 ***************************************************************************/

template <class T1,class T2>
inline PRInt32 MinInt(T1 anInt1,T2 anInt2){
  return (anInt1<(T1)anInt2) ? anInt1 : anInt2;
}

template <class T1,class T2>
inline PRInt32 MaxInt(T1 anInt1,T2 anInt2){
  return (anInt1<(T1)anInt2) ? anInt2 : anInt1;
}

inline size_t stringlen(const PRUnichar* theString) {
  const PRUnichar* theEnd=theString;
  while(*theEnd++);  //skip to end of aDest...
  size_t result=theEnd-theString-1;
  return result;
}

inline size_t stringlen(const char* theString) {
  return ::strlen(theString);
}



//----------------------------------------------------------------------------------------


/***************************************************************************
 *
 *  This is the templatized base class from which stringvalues are derived. (rickg)
 *
 ***************************************************************************/
template <class CharType>
struct nsStackBuffer {
  nsStackBuffer(CharType *aBuffer,PRUint32 aLength,PRUint32 aCapacity) {
    mBuffer=aBuffer;
    mLength=aLength;
    mCapacity=aCapacity;
  }
  CharType* mBuffer;
  PRUint32  mLength;
  PRUint32  mCapacity;
};


#if 0
template <class CharType>
struct RCBuffer {

  void* operator new(size_t aSize) {
    CharType* theBuf = ::new CharType[aSize+1+sizeof(_RefCountType)];
    memset(theBuf,0xff,aSize+additionalBytes+1);
    return theBuf;
  }

  void operator delete(void *p, size_t aSize) {
    ::delete [] p;
  }

  _RefCountType mRefCount;
  CharType      mBuffer[1];

};
#endif


/***************************************************************************
 *
 *  This is the templatized base class from which stringvalues are derived. (rickg)
 *
 ***************************************************************************/
template <class CharType>
struct nsStringValueImpl {

  nsStringValueImpl()  {
    mBuffer=(CharType*)gCommonEmptyBuffer;
    mLength=mCapacity=0;
    mRefCount=2;  //so we don't ever try to free the shared buffer
  }

  nsStringValueImpl(const nsStringValueImpl<CharType>& aCopy) {
    mBuffer=aCopy.mBuffer;
    mLength=aCopy.mLength;
    mCapacity=aCopy.mCapacity;
    mRefCount=1;
  }

  nsStringValueImpl(const nsStringValueImpl<CharType>& aCopy,PRInt32 aLength){
    mBuffer=aCopy.mBuffer;
    mLength=aLength;
    mCapacity=aCopy.mCapacity;
    mRefCount=1;
  }

  nsStringValueImpl(CharType* theString,PRInt32 aLength=-1,PRUint32 aCapacity=0)  {
    if(theString) {
      mLength = (-1==aLength) ? stringlen(theString) : aLength;
      mCapacity=(0==aCapacity) ? mLength+1 : aCapacity;
      mBuffer=theString;
    }
    else {
      mBuffer=(CharType*)gCommonEmptyBuffer;
      mLength=mCapacity=0;
    }
    mRefCount=1;
  }

  nsStringValueImpl(const nsStackBuffer<CharType> &aBuffer)  {
    mCapacity=aBuffer.mCapacity;
    mLength=aBuffer.mLength;
    mBuffer=aBuffer.mBuffer;
    mRefCount=2; //set it to 2 so we don't try to free it.
  }

  void operator=(const nsStringValueImpl<CharType>& aCopy) {
    mBuffer=aCopy.mBuffer;
    mLength=aCopy.mLength;
    mCapacity=aCopy.mCapacity;
    mRefCount=aCopy.mRefCount;
  }

  void*     GetBuffer() {return mBuffer;}
  PRUint32  GetLength() {return mLength;}
  PRUint32  GetCapacity() {return mCapacity;}
  size_t    GetCharSize() {return sizeof(CharType);}

  CharType* mBuffer;
  PRUint32  mLength;
  PRUint32  mCapacity;
  PRUint32  mRefCount;
};



#endif
