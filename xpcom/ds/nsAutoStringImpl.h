/*************************************************************************************
 *
 * DEVELOPMENT NOTES:
 *
 *  1. See nsStringImpl.h
 *
 *  2. I've used the uncommon template style of including a base type.
 *     The purpose is to allow callers to vary the underlying size of
 *     the buffer as a local optimization. It may turn out to be a
 *     bad idea, but for now I'm more focused on getting a workable
 *     autostring. Please be patient.
 *
 * FILE NOTES:
 *
 *  02.29.2000: Original files (rickg)
 *  03.02.2000: Flesh out the interface to be compatible with old library (rickg)
 *  
 *************************************************************************************/

#ifndef NS_AUTOSTRING_
#define NS_AUTOSTRING_

#include "nsStringImpl.h"

/********************************************************************
 *  
 * This template provides the basic implementation for nsAutoString,
 * nsAutoCString and so on. Note the typedefs at the bottom of this file.
 *
 ********************************************************************/

template <class CharType, int DefaultBufferSize=32>
class nsAutoStringImpl : public nsStringImpl<CharType> {
public:
  
    //default constructor
  nsAutoStringImpl() : nsStringImpl<CharType> () {
    memset(mInternalBuffer,0,sizeof(mInternalBuffer));
    mStringValue.mCapacity=DefaultBufferSize;
    mStringValue.mBuffer=mInternalBuffer;
  }
  
    //copy constructor
  nsAutoStringImpl(const nsAutoStringImpl& aCopy,PRInt32 aLength=-1) : nsStringImpl<CharType>() {
    memset(mInternalBuffer,0,sizeof(mInternalBuffer));
    mStringValue.mCapacity=DefaultBufferSize;
    mStringValue.mBuffer=mInternalBuffer;
    Assign(aCopy);
  }

    //call this version for all ABT versions of readable strings
  nsAutoStringImpl(const nsReadableString &aCopy,PRInt32 aLength=-1) : nsStringImpl<CharType> () {
    memset(mInternalBuffer,0,sizeof(mInternalBuffer));
    mStringValue.mCapacity=DefaultBufferSize;
    mStringValue.mBuffer=mInternalBuffer;
    Assign(aCopy);
  }

    //call this version for ptrs to char-strings that match the internal char type
  nsAutoStringImpl(const CharType* theString, PRInt32 aLength=-1) : nsStringImpl<CharType> () {
    memset(mInternalBuffer,0,sizeof(mInternalBuffer));
    mStringValue.mCapacity=DefaultBufferSize;
    mStringValue.mBuffer=mInternalBuffer;
    Assign(theString,aLength);
  }

  virtual ~nsAutoStringImpl() { }
  
  nsAutoStringImpl& operator=(const nsAutoStringImpl& aCopy) {
    if(aCopy.mStringValue.mBuffer!=mStringValue.mBuffer) {
      Assign(aCopy);
    }
    return *this;
  }

  nsAutoStringImpl& operator=(const CharType* aString) {
    if(aCopy.mStringValue.mBuffer!=aString) {
      Assign(aCopy);
    }
    return *this;
  }
  
protected:
  CharType mInternalBuffer[DefaultBufferSize];
};


#endif
