
/*************************************************************************************
 *  
 * MODULES NOTES:
 *
 *  1. See nsStringImpl.h
 *  2. We still have the ownership flag (bool) as a seperate data member. This will
 *     get merged into the length or capacity field after things stabilize a bit.
 *     (Likely only a few more days).
 *  
 *
 * FILE NOTES:
 *
 *  02.29.2000: Original files (rickg)
 *  03.02.2000: Flesh out the interface to be compatible with old library (rickg)
 *  
 *************************************************************************************/


#ifndef NSSTRINGVALUE_
#define NSSTRINGVALUE_
#include <string.h>

typedef int PRInt32;
typedef unsigned int PRUint32;
typedef short int PRUnichar;
typedef int nsresult;
typedef char PRBool;
#define NS_OK 0
#define PR_TRUE 1
#define PR_FALSE  0
static const int kDefaultStringSize=32;



/***************************************************************************
 *
 *  This is ABT from which the basic stringvalues are derived. (rickg)
 *
 ***************************************************************************/
class nsStringValue  {
public:
  virtual void AddRef(void) =0;
  virtual void Release(void) =0;
};


/***************************************************************************
 *
 *  This is the templatized base class from which stringvalues are derived. (rickg)
 *
 ***************************************************************************/
template <class CharType>
class nsStringValueImpl : nsStringValue {
public:

  nsStringValueImpl() : nsStringValue() {
    mRefCount=1;
    mBuffer=0;
    mLength=mCapacity=mOwnsBuffer=0;
  }

  nsStringValueImpl(const nsStringValueImpl& aCopy) : nsStringValue() {
    operator=(aCopy);
  }

  nsStringValueImpl(CharType* theString,PRInt32 aLength=-1,PRInt32 aCapacity=-1) : nsStringValue() {
    mRefCount=1;
    mOwnsBuffer=0;
    if(theString) {
      mLength = (-1==aLength) ? stringlen(theString) : aLength;
      mCapacity=mLength+1;
      mBuffer=theString;
    }
    else {
      mBuffer=0;
      mLength=mCapacity=mOwnsBuffer=0;
    }
  }

  operator=(const nsStringValueImpl<CharType>& aCopy) {
    mRefCount=1;
    mBuffer=aCopy.mBuffer;
    mLength=aCopy.mLength;
    mCapacity=aCopy.mCapacity;
    mOwnsBuffer=aCopy.mOwnsBuffer;
  }

  operator CharType*() {return mBuffer;}

  virtual void AddRef(void) {mRefCount++;}
  virtual void Release(void){--mRefCount;}

  virtual void*     GetBuffer() {return mBuffer;}
  virtual PRUint32  GetLength() {return mLength;}
  virtual size_t    GetCharSize() {return sizeof(CharType);}

public:
  PRInt32   mRefCount;
  CharType* mBuffer;
  PRInt32   mLength;
  PRInt32   mCapacity;
  PRBool    mOwnsBuffer;
};



#endif
