
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
#include <string.h>

typedef int PRInt32;
typedef unsigned int PRUint32;
typedef short int PRUnichar;
typedef int nsresult;
typedef char PRBool;
#define NS_OK 0
#define NS_MEMORY_ERROR 1000
#define PR_TRUE 1
#define PR_FALSE  0
#define NS_SUCCESS(x) (NS_OK==x)

static const char* kWhitespace="\b\t\r\n ";
static const char* kPossibleNull ="Possible null being inserted into string.";

static const int kRadix10=10;
static const int kRadix16=16;
static const int kAutoDetect=100;
static const int kRadixUnknown=kAutoDetect+1;

static const int kDefaultStringSize=32;
static const int kNotFound=-1;

/***************************************************************************
 *
 *  The following is the basic interface nsString searching...
 *
 *  We will merge this into nsAReadableString very soon, but I've kept it
 *  seperate until I can map and factor the original nsString methods.
 *
 ***************************************************************************/
class nsSearchableString {
public:

  //We need to decide how deep (and generic) this API can go...

};

/***************************************************************************
 *
 *  This isn't intended as a real class, but only to illustrate that for 
 *  nsAReadableString, we want a common access pattern to get at the underlying
 *  buffer. Iterators make perfect sense, but it will take a real implementation.
 *
 *  Note: This class is intended to replace direct calls to GetBuffer()
 *  in cases where iteration on an nsAReadableString is taking place. 
 *
 ***************************************************************************/
class nsAReadableStringIterator {
public:
  nsAReadableStringIterator() { }
};


/***************************************************************************
 *
 *  The following is the basic interface anyone who wants to be passed
 *  as a string should define.
 *
 ***************************************************************************/
class nsAReadableString : public nsSearchableString {
public:
  virtual PRUint32  Length() const  =0;
  virtual size_t    GetCharSize() const =0;

//  virtual PRUnichar operator[](PRUint32) = 0;

  virtual nsAReadableStringIterator* First() = 0; //These are here so that nsReadable strings can offer a standard mechanism for
  virtual nsAReadableStringIterator* Last() = 0;  //callers to access underlying data. Feel free to replace with a better pattern.

//by moving to iterators, we can support segmented content models...
//virtual nsStringIterator  GetIterator() = 0;

//virtual PRBool    IsEmpty(void) = 0;

    //etc.
};


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
    mLength=mCapacity=0;
  }

  nsStringValueImpl(const nsStringValueImpl& aCopy) : nsStringValue() {
    mRefCount=1;
    mBuffer=aCopy.mBuffer;
    mLength=aCopy.mLength;
    mCapacity=aCopy.mCapacity;
  }

  nsStringValueImpl(const nsStringValueImpl& aCopy,PRInt32 aLength) : nsStringValue() {
    mRefCount=1;
    mBuffer=aCopy.mBuffer;
    mLength=aLength;
    mCapacity=aCopy.mCapacity;
  }

  nsStringValueImpl(CharType* theString,PRInt32 aLength=-1,PRInt32 aCapacity=-1) : nsStringValue() {
    mRefCount=1;
    if(theString) {
      mLength = (-1==aLength) ? stringlen(theString) : aLength;
      mCapacity=mLength+1;
      mBuffer=theString;
    }
    else {
      mBuffer=0;
      mLength=mCapacity=0;
    }
  }

  nsStringValueImpl(const nsStackBuffer<CharType> &aBuffer) : nsStringValue() {
    mRefCount=2;
    mCapacity=aBuffer.mCapacity;
    mLength=aBuffer.mLength;
    mBuffer=aBuffer.mBuffer;
  }

  operator=(const nsStringValueImpl<CharType>& aCopy) {
    mRefCount=1;
    mBuffer=aCopy.mBuffer;
    mLength=aCopy.mLength;
    mCapacity=aCopy.mCapacity;
  }

  operator CharType*() {return mBuffer;}

  virtual void AddRef(void) {mRefCount++;}
  virtual void Release(void){--mRefCount;}

  virtual void*     GetBuffer() {return mBuffer;}
  virtual PRUint32  GetLength() {return mLength;}
  virtual size_t    GetCharSize() {return sizeof(CharType);}

public:
  CharType* mBuffer;
  PRUint32  mRefCount;
  PRUint32  mLength;
  PRUint32  mCapacity;
};



#endif
