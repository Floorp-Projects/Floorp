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

    The nsStr struct is a simple structure (no methods) that contains
    the necessary info to be described as a string. This simple struct
    is manipulated by the static methods provided in this class. 
    (Which effectively makes this a library that works on structs).

    There are also object-based versions called nsString and nsAutoString
    which use nsStr but makes it look at feel like an object. 

 ***********************************************************************/

/***********************************************************************
  ASSUMPTIONS:

    1. nsStrings and nsAutoString are always null terminated. 
    2. If you try to set a null char (via SetChar()) a new length is set
    3. nsCStrings can be upsampled into nsString without data loss
    4. Char searching is faster than string searching. Use char interfaces
       if your needs will allow it.
    5. It's easy to use the stack for nsAutostring buffer storage (fast too!).
       See the CBufDescriptor class in this file.
    6. It's ONLY ok to provide non-null-terminated buffers to Append() and Insert()
       provided you specify a 0<n value for the optional count argument.
    7. Downsampling from nsString to nsCString is lossy -- avoid it if possible!
    8. Calls to ToNewCString() and ToNewUnicode() should be matched with calls to Recycle().

 ***********************************************************************/


/**********************************************************************************
  
 AND NOW FOR SOME GENERAL DOCUMENTATION ON STRING USAGE...

    The fundamental datatype in the string library is nsStr. It's a structure that
    provides the buffer storage and meta-info. It also provides a C-style library
    of functions for direct manipulation (for those of you who prefer K&R to Bjarne). 

    Here's a diagram of the class hierarchy: 

      nsStr
        |___nsString
        |      |
        |      ------nsAutoString
        |
        |___nsCString
               |
               ------nsCAutoString

    Why so many string classes? The 4 variants give you the control you need to 
    determine the best class for your purpose. There are 2 dimensions to this 
    flexibility: 1) stack vs. heap; and 2) 1-byte chars vs. 2-byte chars.

    Note: While nsAutoString and nsCAutoString begin life using stack-based storage,
          they may not stay that way. Like all nsString classes, autostrings will
          automatically grow to contain the data you provide. When autostrings
          grow beyond their intrinsic buffer, they switch to heap based allocations.
          (We avoid alloca to avoid considerable platform difficulties; see the 
           GNU documentation for more details).

    I should also briefly mention that all the string classes use a "memory agent" 
    object to perform memory operations. This class proxies the standard nsAllocator
    for actual memory calls, but knows the structure of nsStr making heap operations
    more localized.
    

  CHOOSING A STRING CLASS:

    In order to choose a string class for you purpose, use this handy table:

                        heap-based    stack-based
                    -----------------------------------
        ascii data  |   nsCString     nsCAutoString   |
                    |----------------------------------
      unicode data  |    nsString      nsAutoString   |
                    ----------------------------------- 
    
  
    Note: The i18n folks will stenously object if we get too carried away with the
          use of nsCString's that pass interface boundaries. Try to limit your
          use of these to external interfaces that demand them, or for your own
          private purposes in cases where they'll never be seen by humans. 

    
  PERFORMANCE CONSIDERATIONS:

    Here are a few tricks to know in order to get better string performance: 
    
      1) Try to limit conversions between ascii and unicode; By sticking with nsString
         wherever possible your code will be i18n-compliant.


      2) Preallocating your string buffer cuts down trips to the allocator. So if you
         have need for an arbitrarily large buffer, pre-size it like this:

         {
           nsString mBuffer;
           mBuffer.SetCapacity(aReasonableSize);
         }

      3) Allocating nsAutoString or nsCAutoString on the heap is memory inefficient
         (after all, the whole point is to avoid a heap allocation of the buffer).


      4) Consider using nsString to write into your arbitrarily-sized stack buffers, rather
         than it's own buffers.

         For example, let's say you're going to call printf() to emit pretty-printed debug output 
         of your object. You know from experience that the pretty-printed version of your object 
         exceeds the capacity of an autostring. Ignoring memory considerations, you could simply 
         use nsCString, appending the stringized version of each of your class's data members. 
         This will probably result in calls to the heap manager. 

         But there's a way to do this without necessarily having to call the heap manager.
         All you do is declare a stack based buffer and instruct nsCString to use that instead 
         of it's own internal buffer by using the CBufDescriptor class:
         
         {
           char theBuffer[256];
           CBufDescritor theBufDecriptor( theBuffer, PR_TRUE, sizeof(theBuffer), 0);
           nsCAutoString s3( theBufDescriptor );
           s3="HELLO, my name is inigo montoya, you killed my father, prepare to die!.";
         }

         The assignment statment to s3 will cause the given string to be written to your
         stack-based buffer via the normal nsString interfaces. Cool, huh?  Note however
         that just like any other nsString use, if you write more data than will fit in
         the buffer, nsString *will* go to the heap. 
     

 **********************************************************************************/


#ifndef _nsStr
#define _nsStr

#include "nscore.h"
#include "nsIAllocator.h"

//----------------------------------------------------------------------------------------

enum  eCharSize {eOneByte=0,eTwoByte=1};
#define kDefaultCharSize eTwoByte
#define kRadix10        (10)
#define kRadix16        (16)
#define kAutoDetect     (100)
#define kRadixUnknown   (kAutoDetect+1)
const   PRInt32 kDefaultStringSize = 32;
const   PRInt32 kNotFound = -1;


class nsIMemoryAgent;

//----------------------------------------------------------------------------------------

class NS_COM CBufDescriptor {
public:
  CBufDescriptor(char* aString,     PRBool aStackBased,PRUint32 aCapacity,PRInt32 aLength=-1);
  CBufDescriptor(PRUnichar* aString,PRBool aStackBased,PRUint32 aCapacity,PRInt32 aLength=-1);

  char*     mBuffer;
  eCharSize mCharSize;
  PRUint32  mCapacity;
  PRInt32   mLength;
  PRBool    mStackBased;

};

//----------------------------------------------------------------------------------------


struct NS_COM nsStr {  

 /**
  * This method initializes an nsStr for use
  *
  * @update	gess 01/04/99
  * @param  aString is the nsStr to be initialized
  * @param  aCharSize tells us the requested char size (1 or 2 bytes)
  */
  static void Initialize(nsStr& aDest,eCharSize aCharSize);

 /**
  * This method initializes an nsStr for use
  *
  * @update	gess 01/04/99
  * @param  aString is the nsStr to be initialized
  * @param  aCharSize tells us the requested char size (1 or 2 bytes)
  */
  static void Initialize(nsStr& aDest,char* aCString,PRUint32 aCapacity,PRUint32 aLength,eCharSize aCharSize,PRBool aOwnsBuffer);

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
  static void Delete(nsStr& aDest,PRUint32 aDestOffset,PRUint32 aCount,nsIMemoryAgent* anAgent=0);

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
  static void CompressSet(nsStr& aDest,const char* aSet,PRBool aEliminateLeading,PRBool aEliminateTrailing);

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
  static PRInt32 FindSubstr(const nsStr& aDest,const nsStr& aSource, PRBool aIgnoreCase,PRInt32 anOffset);
  static PRInt32 FindChar(const nsStr& aDest,PRUnichar aChar, PRBool aIgnoreCase,PRInt32 anOffset);
  static PRInt32 FindCharInSet(const nsStr& aDest,const nsStr& aSet,PRBool aIgnoreCase,PRInt32 anOffset);

  static PRInt32 RFindSubstr(const nsStr& aDest,const nsStr& aSource, PRBool aIgnoreCase,PRInt32 anOffset);
  static PRInt32 RFindChar(const nsStr& aDest,PRUnichar aChar, PRBool aIgnoreCase,PRInt32 anOffset);
  static PRInt32 RFindCharInSet(const nsStr& aDest,const nsStr& aSet,PRBool aIgnoreCase,PRInt32 anOffset);


  PRUint32        mLength;
  PRUint32        mCapacity;
  eCharSize       mCharSize;
  PRBool          mOwnsBuffer;

  union { 
    char*         mStr;
    PRUnichar*    mUStr;
  };
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

inline void AddNullTerminator(nsStr& aDest) {
  if(eTwoByte==aDest.mCharSize) 
    aDest.mUStr[aDest.mLength]=0;
  else aDest.mStr[aDest.mLength]=0;
}

/**
 * Return the given buffer to the heap manager. Calls allocator::Free()
 * @return string length
 */
inline void Recycle( char* aBuffer) { nsAllocator::Free(aBuffer); }
inline void Recycle( PRUnichar* aBuffer) { nsAllocator::Free(aBuffer); }

/**
* This method is used to access a given char in the given string
*
* @update	gess 01/04/99
* @param  aDest is the nsStr to be appended to
* @param  anIndex tells us where in dest to get the char from
* @return the given char, or 0 if anIndex is out of range
*/
inline PRUnichar GetCharAt(const nsStr& aDest,PRUint32 anIndex){
  if(anIndex<aDest.mLength)  {
    return (eTwoByte==aDest.mCharSize) ? aDest.mUStr[anIndex] : aDest.mStr[anIndex];
  }//if
  return 0;
}

//----------------------------------------------------------------------------------------

class nsIMemoryAgent {
public:
  virtual PRBool Alloc(nsStr& aString,PRUint32 aCount)=0;
  virtual PRBool Realloc(nsStr& aString,PRUint32 aCount)=0;
  virtual PRBool Free(nsStr& aString)=0;
};

class nsMemoryAgent : public nsIMemoryAgent {
public:

  virtual PRBool Alloc(nsStr& aDest,PRUint32 aCount) {

    static int mAllocCount=0;
    mAllocCount++;

    //we're given the acount value in charunits; now scale up to next multiple.
  	PRUint32	theNewCapacity=kDefaultStringSize;
    while(theNewCapacity<aCount){ 
		  theNewCapacity<<=1;
    }

    aDest.mCapacity=theNewCapacity++;
    PRUint32 theSize=(theNewCapacity<<aDest.mCharSize);
    aDest.mStr = (char*)nsAllocator::Alloc(theSize);

    aDest.mOwnsBuffer=1;
    return PR_TRUE;
  }

  virtual PRBool Free(nsStr& aDest){
    if(aDest.mStr){
      if(aDest.mOwnsBuffer){
        nsAllocator::Free(aDest.mStr);
      }
      aDest.mStr=0;
      aDest.mOwnsBuffer=0;
      return PR_TRUE;
    }
    return PR_FALSE;
  }

  virtual PRBool Realloc(nsStr& aDest,PRUint32 aCount){
    Free(aDest);
    return Alloc(aDest,aCount);
  }

};

char* GetSharedEmptyBuffer();
nsIMemoryAgent* GetDefaultAgent(void);

#endif

