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


/***********************************************************************
  MODULE NOTES:

    1. There are two philosophies to building string classes:
        A. Hide the underlying buffer & offer API's allow indirect iteration
        B. Reveal underlying buffer, risk corruption, but gain performance
        
       We chose the option B for performance reasons. 

    2  Our internal buffer always holds capacity+1 bytes.
    3. Note that our internal format for this class makes our memory 
       layout compatible with BStrings.

    The nsStr struct is a simple structure (no methods) that contains
    the necessary info to be described as a string. This simple struct
    is manipulated by the static methods provided in this class. 
    (Which effectively makes this a library that works on structs).

    There are also object-based versions called nsString and nsAutoString
    which use nsStr but makes it look at feel like an object. 

 ***********************************************************************/


#ifndef _nsStr
#define _nsStr

#include "prtypes.h"
#include "nscore.h"

//----------------------------------------------------------------------------------------

enum  eCharSize {eOneByte=0,eTwoByte=1};
#define kDefaultCharSize eTwoByte


union UStrPtr { 
  char*       mCharBuf;
  PRUnichar*  mUnicharBuf;
}; 

/**************************************************************************
  Here comes the nsBufDescriptor class which describes buffer properties.
 **************************************************************************/

struct nsBufDescriptor {
  nsBufDescriptor(char* aBuffer,PRUint32 aBufferSize,eCharSize aCharSize,PRBool aOwnsBuffer) {
    mStr.mCharBuf=aBuffer;
    mMultibyte=aCharSize;
    mCapacity=(aBufferSize>>mMultibyte)-1;
    mOwnsBuffer=aOwnsBuffer;
  }

  PRUint32        mCapacity;
  PRBool          mOwnsBuffer;
  eCharSize       mMultibyte;
  UStrPtr         mStr;
};


class nsIMemoryAgent;

//----------------------------------------------------------------------------------------


struct nsStr {  

 /**
  * This method initializes an nsStr for use
  *
  * @update	gess 01/04/99
  * @param  aString is the nsStr to be initialized
  * @param  aCharSize tells us the requested char size (1 or 2 bytes)
  */
  static void Initialize(nsStr& aDest,eCharSize aCharSize);

 /**
  * This method destroys the given nsStr, and *MAY* 
  * deallocate it's memory depending on the setting
  * of the internal mOwnsBUffer flag.
  *
  * @update	gess 01/04/99
  * @param  aString is the nsStr to be manipulated
  * @param  anAgent is the allocator to be used to the nsStr
  */
  static void Destroy(nsStr& aDest,nsIMemoryAgent* anAgent=0); 

 /**
  * These methods are where memory allocation/reallocation occur. 
  *
  * @update	gess 01/04/99
  * @param  aString is the nsStr to be manipulated
  * @param  anAgent is the allocator to be used on the nsStr
  * @return  
  */
  static void EnsureCapacity(nsStr& aString,PRUint32 aNewLength,nsIMemoryAgent* anAgent=0);
  static void GrowCapacity(nsStr& aString,PRUint32 aNewLength,nsIMemoryAgent* anAgent=0);

 /**
  * These methods are used to append content to the given nsStr
  *
  * @update	gess 01/04/99
  * @param  aDest is the nsStr to be appended to
  * @param  aSource is the buffer to be copied from
  * @param  anOffset tells us where in source to start copying
  * @param  aCount tells us the (max) # of chars to copy
  * @param  anAgent is the allocator to be used for alloc/free operations
  */
  static void Append(nsStr& aDest,const nsStr& aSource,PRUint32 anOffset,PRInt32 aCount,nsIMemoryAgent* anAgent=0);

 /**
  * These methods are used to assign contents of a source string to dest string
  *
  * @update	gess 01/04/99
  * @param  aDest is the nsStr to be appended to
  * @param  aSource is the buffer to be copied from
  * @param  anOffset tells us where in source to start copying
  * @param  aCount tells us the (max) # of chars to copy
  * @param  anAgent is the allocator to be used for alloc/free operations
  */
  static void Assign(nsStr& aDest,const nsStr& aSource,PRUint32 anOffset,PRInt32 aCount,nsIMemoryAgent* anAgent=0);

 /**
  * These methods are used to insert content from source string to the dest nsStr
  *
  * @update	gess 01/04/99
  * @param  aDest is the nsStr to be appended to
  * @param  aDestOffset tells us where in dest to start insertion
  * @param  aSource is the buffer to be copied from
  * @param  aSrcOffset tells us where in source to start copying
  * @param  aCount tells us the (max) # of chars to insert
  * @param  anAgent is the allocator to be used for alloc/free operations
  */
  static void Insert( nsStr& aDest,PRUint32 aDestOffset,const nsStr& aSource,PRUint32 aSrcOffset,PRInt32 aCount,nsIMemoryAgent* anAgent=0);

 /**
  * This method deletes chars from the given str. 
  * The given allocator may choose to resize the str as well.
  *
  * @update	gess 01/04/99
  * @param  aDest is the nsStr to be deleted from
  * @param  aDestOffset tells us where in dest to start deleting
  * @param  aCount tells us the (max) # of chars to delete
  * @param  anAgent is the allocator to be used for alloc/free operations
  */
  static void Delete(nsStr& aDest,PRUint32 aDestOffset,PRInt32 aCount,nsIMemoryAgent* anAgent=0);

 /**
  * This method is used to truncate the given string.
  * The given allocator may choose to resize the str as well (but it's not likely).
  *
  * @update	gess 01/04/99
  * @param  aDest is the nsStr to be appended to
  * @param  aDestOffset tells us where in dest to start insertion
  * @param  aSource is the buffer to be copied from
  * @param  aSrcOffset tells us where in source to start copying
  * @param  anAgent is the allocator to be used for alloc/free operations
  */
  static void Truncate(nsStr& aDest,PRUint32 aDestOffset,nsIMemoryAgent* anAgent=0);

 /**
  * This method is used to perform a case conversion on the given string
  *
  * @update	gess 01/04/99
  * @param  aDest is the nsStr to be case shifted
  * @param  toUpper tells us to go upper vs. lower
  */
  static void ChangeCase(nsStr& aDest,PRBool aToUpper);

  /**
   * This method removes chars (given in aSet) from the given buffer 
   *
   * @update	gess 01/04/99
   * @param   aString is the buffer to be manipulated
   * @param   aDestOffset is starting pos in buffer for manipulation
   * @param   aCount is the number of chars to compare
   * @param   aSet tells us which chars to remove from given buffer
   */
  static void StripChars(nsStr& aDest,PRUint32 aDestOffset,PRInt32 aCount,const char* aCharSet);

  /**
   * This method trims chars (given in aSet) from the edges of given buffer 
   *
   * @update	gess 01/04/99
   * @param   aDest is the buffer to be manipulated
   * @param   aSet tells us which chars to remove from given buffer
   * @param   aEliminateLeading tells us whether to strip chars from the start of the buffer
   * @param   aEliminateTrailing tells us whether to strip chars from the start of the buffer
   */
  static void Trim(nsStr& aDest,const char* aSet,PRBool aEliminateLeading,PRBool aEliminateTrailing);
  
  /**
   * This method compresses duplicate runs of a given char from the given buffer 
   *
   * @update	gess 01/04/99
   * @param   aDest is the buffer to be manipulated
   * @param   aSet tells us which chars to compress from given buffer
   * @param   aChar is the replacement char
   * @param   aEliminateLeading tells us whether to strip chars from the start of the buffer
   * @param   aEliminateTrailing tells us whether to strip chars from the start of the buffer
   */
  static void CompressSet(nsStr& aDest,const char* aSet,PRUint32 aChar,PRBool aEliminateLeading,PRBool aEliminateTrailing);

  /**
   * This method compares the data bewteen two nsStr's 
   *
   * @update	gess 01/04/99
   * @param   aStr1 is the first buffer to be compared
   * @param   aStr2 is the 2nd buffer to be compared
   * @param   aCount is the number of chars to compare
   * @param   aIgnorecase tells us whether to use a case-sensitive comparison
   * @return  -1,0,1 depending on <,==,>
   */
  static PRInt32  Compare(const nsStr& aDest,const nsStr& aSource,PRInt32 aCount,PRBool aIgnoreCase);

 /**
  * These methods scan the given string for 1 or more chars in a given direction
  *
  * @update	gess 01/04/99
  * @param  aDest is the nsStr to be searched to
  * @param  aSource (or aChar) is the substr we're looking to find
  * @param  aIgnoreCase tells us whether to search in a case-sensitive manner
  * @param  anOffset tells us where in the dest string to start searching
  * @return the index of the source (substr) in dest, or -1 (kNotFound) if not found.
  */
  static PRInt32 FindSubstr(const nsStr& aDest,const nsStr& aSource, PRBool aIgnoreCase,PRUint32 anOffset);
  static PRInt32 FindChar(const nsStr& aDest,PRUnichar aChar, PRBool aIgnoreCase,PRUint32 anOffset);
  static PRInt32 FindCharInSet(const nsStr& aDest,const nsStr& aSet,PRBool aIgnoreCase,PRUint32 anOffset);

  static PRInt32 RFindSubstr(const nsStr& aDest,const nsStr& aSource, PRBool aIgnoreCase,PRUint32 anOffset);
  static PRInt32 RFindChar(const nsStr& aDest,PRUnichar aChar, PRBool aIgnoreCase,PRUint32 anOffset);
  static PRInt32 RFindCharInSet(const nsStr& aDest,const nsStr& aSet,PRBool aIgnoreCase,PRUint32 anOffset);

 /**
  * This method is used to access a given char in the given string
  *
  * @update	gess 01/04/99
  * @param  aDest is the nsStr to be appended to
  * @param  anIndex tells us where in dest to get the char from
  * @return the given char, or 0 if anIndex is out of range
  */
  static PRUnichar GetCharAt(const nsStr& aDest,PRUint32 anIndex);

  PRUint32        mLength   :   30;
  eCharSize       mMultibyte :   2;
  PRUint32        mCapacity:    30;
  PRUint32        mOwnsBuffer:  1;
  PRUint32        mUnused:      1;
  UStrPtr         mStr;
};

/**************************************************************
  A couple of tiny helper methods used in the string classes.
 **************************************************************/

inline PRInt32 MinInt(PRInt32 anInt1,PRInt32 anInt2){
  return (anInt1<anInt2) ? anInt1 : anInt2;
}

inline PRInt32 MaxInt(PRInt32 anInt1,PRInt32 anInt2){
  return (anInt1<anInt2) ? anInt2 : anInt1;
}

inline void ToRange(PRInt32& aValue,PRInt32 aMin,PRInt32 aMax){
  if(aValue<aMin)
    aValue=aMin;
  else if(aValue>aMax)
    aValue=aMax;
}

inline void ToRange(PRUint32& aValue,PRUint32 aMin,PRUint32 aMax){
  if(aValue<aMin)
    aValue=aMin;
  else if(aValue>aMax)
    aValue=aMax;
}

inline void AddNullTerminator(nsStr& aDest) {
  if(eTwoByte==aDest.mMultibyte) 
    aDest.mStr.mUnicharBuf[aDest.mLength]=0;
  else aDest.mStr.mCharBuf[aDest.mLength]=0;
}

//----------------------------------------------------------------------------------------

class nsIMemoryAgent {
public:
  virtual PRBool Alloc(nsStr& aString,PRInt32 aCount)=0;
  virtual PRBool Realloc(nsStr& aString,PRInt32 aCount)=0;
  virtual PRBool Free(nsStr& aString)=0;
};

class nsMemoryAgent : public nsIMemoryAgent {
  enum eDelta{eGrowthDelta=8};
public:
  virtual PRBool Alloc(nsStr& aDest,PRInt32 aCount) {
    
    //we're given the acount value in charunits; we have to scale up by the charsize.

    PRInt32 theNewCapacity;
    if (aDest.mCapacity > 64) {
      // When the string starts getting large, double the capacity as we grow.
      theNewCapacity = aDest.mCapacity * 2;
      if (theNewCapacity < aCount) {
        theNewCapacity = aDest.mCapacity + aCount;
      }
    } else {
      // When the string is small, keep it's capacity a multiple of kGrowthDelta
      PRInt32 unitDelta=(aCount/eGrowthDelta)+1;
      theNewCapacity=unitDelta*eGrowthDelta;
    }

    aDest.mCapacity=theNewCapacity++;
    size_t theSize=(theNewCapacity<<aDest.mMultibyte);
    aDest.mStr.mCharBuf=new char[theSize];
    aDest.mOwnsBuffer=1;
    return PR_TRUE;
  }

  virtual PRBool Free(nsStr& aDest){
    if(aDest.mStr.mCharBuf){
      if(aDest.mOwnsBuffer){
        delete [] aDest.mStr.mCharBuf;
      }
      aDest.mStr.mCharBuf=0;
      aDest.mOwnsBuffer=0;
      return PR_TRUE;
    }
    return PR_FALSE;
  }

  virtual PRBool Realloc(nsStr& aDest,PRInt32 aCount){
    Free(aDest);
    return Alloc(aDest,aCount);
  }

};


#endif

