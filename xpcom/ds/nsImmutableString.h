/********************************************************************************************
 *
 * MODULES NOTES:
 *
 *  1. See nsStringImpl.h
 *  2. I've included an mBuffer in this class data member as a fast-up implementation.
 *     I'll get rid of this shortly, reducing the members to only the mLength member. (rickg)
 * 
 * FILE NOTES:
 *
 *  02.29.2000: Original files (rickg)
 *  03.02.2000: Flesh out the interface to be compatible with old library (rickg)
 *  
 ********************************************************************************************/


#ifndef NS_IMMUTABLESTRING_
#define NS_IMMUTABLESTRING_

#include "nsStringImpl.h"


/**
 *  Immutable strings are intesting because their buffers get allocated
 *  at the same time as the actual string is allocated. (rickg)
 *
 **/
template <class CharType1>
class nsImmutableImpl : public nsReadableString {
public:

    //construct from ptr to char* of internal type
  static nsImmutableImpl* Create(const CharType1* aBuffer) {
    size_t theBufSize=(aBuffer) ? stringlen(aBuffer) : 0;
    nsImmutableImpl<CharType1> *result= new(theBufSize) nsImmutableImpl<CharType1>(aBuffer,theBufSize);
    return result;
  }

    //copy construct
  static nsImmutableImpl* Create(const nsImmutableImpl<CharType1>& aCopy) {
    return 0;
  }

    //construct for all other ABT versions of readable strings
  static nsImmutableImpl* Create(const nsReadableString& aCopy) {
    return 0;
  }

  ~nsImmutableImpl() {
  }

  //******************************************
  // From the stringbasics interface...
  //******************************************

  virtual void*     GetBuffer() {return mValue.mBuffer;}
  virtual PRUint32  Length() {return mValue.mLength;}
  virtual size_t    GetCharSize() {return sizeof(CharType1);}

  //******************************************
  // Here are the accessor methods...
  //******************************************
  
  virtual nsReadableStringIterator* First() {return new nsReadableStringIterator(); }
  virtual nsReadableStringIterator* Last() {return new nsReadableStringIterator(); }

  virtual PRUnichar  operator[](PRUint32 aOffset)  {
    return 0; //(*mBuffer)[aOffset];
  }

  /***********************************
    These come from stringsearcher
   ***********************************/

  virtual PRInt32 Find(const char* aTarget) {
    return -1;
  }

  virtual PRInt32 FindChar(PRUnichar aChar) {
    return -1;
  }

    //overloaded in order to free the object and the
    //extra bytes for data that we allocated in new()
  void operator delete(void *p, size_t aSize) {
    if(p) {
      nsImmutableImpl* theImmutable=static_cast<nsImmutableImpl*>(p);
      size_t theBufLen=theImmutable->mValue.mLength+1;
      ::delete [] p;
    }
  }

protected:

  /**
   *  Overloaded in order to get extra bytes for data.
   *  This needs to use our standard nsAllocator (rickg)
   *
   **/
  void* operator new(size_t aSize,size_t additionalBytes) {
    CharType1* theBuf = ::new CharType1[aSize+additionalBytes+1];
    memset(theBuf,0xff,aSize+additionalBytes+1);
    return theBuf;
  }

private:

  nsImmutableImpl() {
  }

    //construct for ptr to internal char*
  nsImmutableImpl(const CharType1* aBuffer,size_t aSize) {
    mValue.mLength=aSize;
    mValue.mBuffer=(CharType1*)this+sizeof(nsImmutableString);
    strncopy(mValue.mBuffer,aBuffer,mValue.mLength+1);
  }


    //copy construct 
  nsImmutableImpl(const nsImmutableImpl<CharType1>& aString) {
  }

    //construct from reference to another (generic) readableString
    //We can't assume very much about the readableString implementation,
    //and should rely upon generics (like iterators) to do the work
  nsImmutableImpl(const nsReadableString& aString) {
  }

  struct nsImmutableStringValue {
    PRUint32    mLength;  //data should follow
    CharType1*  mBuffer;  //points to first char in data section of this object; this will soon go away
  }
  mValue;
};


  /***********************************
    Define the concrete immutables...
   ***********************************/
typedef nsImmutableImpl<PRUnichar> nsImmutableString;
typedef nsImmutableImpl<char> nsImmutableCString;



#endif
