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
  GENERAL STRING ISSUES:

    1. nsStrings and nsAutoString are always null terminated. 
    2. If you try to set a null char (via SetChar()) a new length is set
    3. nsCStrings can be upsampled into nsString without data loss
    4. Char searching is faster than string searching. Use char interfaces
       if your needs will allow it.
    5. It's easy to use the stack for nsAutostring buffer storage (fast too!).
       See the CBufDescriptor class in nsStr.h
    6. It's ONLY ok to provide non-null-terminated buffers to Append() and Insert()
       provided you specify a 0<n value for the optional count argument.
    7. Downsampling from nsString to nsCString is lossy -- don't do it!

 ***********************************************************************/


/***********************************************************************
  MODULE NOTES:

  This version of the nsString class offers many improvements over the
  original version:
    1. Wide and narrow chars
    2. Allocators
    3. Much smarter autostrings
    4. Subsumable strings
    5. Memory pools and recycling
 ***********************************************************************/


#ifndef _nsCString_
#define _nsCString_

#include "nsString2.h"
#include "prtypes.h"
#include "nscore.h"
#include <stdio.h>
#include "nsCRT.h"
#include "nsStr.h"
#include "nsIAtom.h"


class NS_COM nsSubsumeCStr;

class NS_COM nsCString : public nsStr {

  public: 

/**
 * Default constructor. Note that we actually allocate a small buffer
 * to begin with. This is because the "philosophy" of the string class
 * was to allow developers direct access to the underlying buffer for
 * performance reasons. 
 */
nsCString(nsIMemoryAgent* anAgent=0);


/**
 * This constructor accepts an isolatin string
 * @param   aCString is a ptr to a 1-byte cstr
 */
nsCString(const char* aCString,PRInt32 aLength=-1,nsIMemoryAgent* anAgent=0);

/**
 * This constructor accepts a unichar string
 * @param   aCString is a ptr to a 2-byte cstr
 */
nsCString(const PRUnichar* aString,PRInt32 aLength=-1,nsIMemoryAgent* anAgent=0);

/**
 * This is a copy constructor that accepts an nsStr
 * @param   reference to another nsCString
 */
nsCString(const nsStr&,nsIMemoryAgent* anAgent=0);

/**
 * This is our copy constructor 
 * @param   reference to another nsCString
 */
nsCString(const nsCString& aString);   

/**
 * This constructor takes a subsumestr
 * @param   reference to subsumestr
 */
nsCString(nsSubsumeCStr& aSubsumeStr);   

/**
 * Destructor
 * 
 */
virtual ~nsCString();    

/**
 * Retrieve the length of this string
 * @return string length
 */
inline PRInt32 Length() const { return (PRInt32)mLength; }

/**
 * Retrieve the size of this string
 * @return string length
 */
virtual void SizeOf(nsISizeOfHandler* aHandler) const;


/**
 * Call this method if you want to force a different string capacity
 * @update	gess7/30/98
 * @param   aLength -- contains new length for mStr
 * @return
 */
void SetLength(PRUint32 aLength) { 
  Truncate(aLength);
}

/**
 * Sets the new length of the string.
 * @param   aLength is new string length.
 * @return  nada
 */
void SetCapacity(PRUint32 aLength);
/**
 * This method truncates this string to given length.
 *
 * @param   anIndex -- new length of string
 * @return  nada
 */
void Truncate(PRInt32 anIndex=0);


/**
 *  Determine whether or not the characters in this
 *  string are in sorted order.
 *  
 *  @return  TRUE if ordered.
 */
PRBool IsOrdered(void) const;


/**
 *  Determine whether or not this string has a length of 0
 *  
 *  @return  TRUE if empty.
 */
PRBool IsEmpty(void) const {
  return PRBool(0==mLength);
}

/**********************************************************************
  Accessor methods...
 *********************************************************************/

const char* GetBuffer(void) const;


 /**
   * Get nth character.
   */
PRUnichar operator[](PRUint32 anIndex) const;
PRUnichar CharAt(PRUint32 anIndex) const;
PRUnichar First(void) const;
PRUnichar Last(void) const;

PRBool SetCharAt(PRUnichar aChar,PRUint32 anIndex);


/**********************************************************************
  String creation methods...
 *********************************************************************/

/**
 * Create a new string by appending given string to this
 * @param   aString -- 2nd string to be appended
 * @return  new string
 */
nsSubsumeCStr operator+(const nsCString& aString);

/**
 * create a new string by adding this to the given buffer.
 * @param   aCString is a ptr to cstring to be added to this
 * @return  newly created string
 */
nsSubsumeCStr operator+(const char* aCString);


/**
 * create a new string by adding this to the given char.
 * @param   aChar is a char to be added to this
 * @return  newly created string
 */
nsSubsumeCStr operator+(PRUnichar aChar);
nsSubsumeCStr operator+(char aChar);


/**********************************************************************
  Lexomorphic transforms...
 *********************************************************************/

/**
 * Converts chars in this to lowercase
 * @update	gess 7/27/98
 */
void ToLowerCase();


/**
 * Converts chars in this to lowercase, and
 * stores them in aOut
 * @update	gess 7/27/98
 * @param   aOut is a string to contain result
 */
void ToLowerCase(nsCString& aString) const;

/**
 * Converts chars in this to uppercase
 * @update	gess 7/27/98
 */
void ToUpperCase();

/**
 * Converts chars in this to lowercase, and
 * stores them in a given output string
 * @update	gess 7/27/98
 * @param   aOut is a string to contain result
 */
void ToUpperCase(nsCString& aString) const;


/**
 *  This method is used to remove all occurances of the
 *  characters found in aSet from this string.
 *  
 *  @param  aSet -- characters to be cut from this
 *  @return *this 
 */
nsCString& StripChars(const char* aSet);
nsCString& StripChar(PRUnichar aChar);

/**
 *  This method strips whitespace throughout the string
 *  
 *  @return  this
 */
nsCString& StripWhitespace();

/**
 *  swaps occurence of 1 string for another
 *  
 *  @return  this
 */
nsCString& ReplaceChar(PRUnichar aOldChar,PRUnichar aNewChar);
nsCString& ReplaceChar(const char* aSet,PRUnichar aNewChar);

PRInt32 CountChar(PRUnichar aChar);

/**
 *  This method trims characters found in aTrimSet from
 *  either end of the underlying string.
 *  
 *  @param   aTrimSet -- contains chars to be trimmed from
 *           both ends
 *  @return  this
 */
nsCString& Trim(const char* aSet,PRBool aEliminateLeading=PR_TRUE,PRBool aEliminateTrailing=PR_TRUE);

/**
 *  This method strips whitespace from string.
 *  You can control whether whitespace is yanked from
 *  start and end of string as well.
 *  
 *  @param   aEliminateLeading controls stripping of leading ws
 *  @param   aEliminateTrailing controls stripping of trailing ws
 *  @return  this
 */
nsCString& CompressSet(const char* aSet, PRUnichar aChar,PRBool aEliminateLeading=PR_TRUE,PRBool aEliminateTrailing=PR_TRUE);

/**
 *  This method strips whitespace from string.
 *  You can control whether whitespace is yanked from
 *  start and end of string as well.
 *  
 *  @param   aEliminateLeading controls stripping of leading ws
 *  @param   aEliminateTrailing controls stripping of trailing ws
 *  @return  this
 */
nsCString& CompressWhitespace( PRBool aEliminateLeading=PR_TRUE,PRBool aEliminateTrailing=PR_TRUE);

/**********************************************************************
  string conversion methods...
 *********************************************************************/

    operator char*() {return mStr;}
    operator const char*() const {return (const char*)mStr;}

/**
 * This method constructs a new nsCString on the stack that is a copy
 * of this string.
 * 
 */
nsCString* ToNewString() const;

/**
 * Creates an ISOLatin1 clone of this string
 * @return  ptr to new isolatin1 string
 */
char* ToNewCString() const;

/**
 * Creates a unicode clone of this string
 * @return  ptr to new unicode string
 */
PRUnichar* ToNewUnicode() const;

/**
 * Copies data from internal buffer onto given char* buffer
 * @param aBuf is the buffer where data is stored
 * @param aBuflength is the max # of chars to move to buffer
 * @return ptr to given buffer
 */
char* ToCString(char* aBuf,PRUint32 aBufLength,PRUint32 anOffset=0) const;

/**
 * Perform string to float conversion.
 * @param   aErrorCode will contain error if one occurs
 * @return  float rep of string value
 */
float ToFloat(PRInt32* aErrorCode) const;

/**
 * Try to derive the radix from the value contained in this string
 * @return  kRadix10, kRadix16 or kAutoDetect (meaning unknown)
 */
PRUint32  DetermineRadix(void);

/**
 * Perform string to int conversion.
 * @param   aErrorCode will contain error if one occurs
 * @return  int rep of string value
 */
PRInt32   ToInteger(PRInt32* aErrorCode,PRUint32 aRadix=kRadix10) const;


/**********************************************************************
  String manipulation methods...                
 *********************************************************************/

/**
 * Functionally equivalent to assign or operator=
 * 
 */
nsCString& SetString(const char* aString,PRInt32 aLength=-1) {return Assign(aString,aLength);}
nsCString& SetString(const nsStr& aString,PRInt32 aLength=-1) {return Assign(aString,aLength);}

/**
 * assign given string to this string
 * @param   aStr: buffer to be assigned to this 
 * @param   alength is the length of the given str (or -1)
            if you want me to determine its length
 * @return  this
 */
nsCString& Assign(const nsCString& aString,PRInt32 aCount=-1);
nsCString& Assign(const char* aString,PRInt32 aCount=-1);
nsCString& Assign(PRUnichar aChar);
nsCString& Assign(char aChar);

/**
 * here come a bunch of assignment operators...
 * @param   aString: string to be added to this
 * @return  this
 */
nsCString& operator=(const nsCString& aString) {return Assign(aString);}
nsCString& operator=(PRUnichar aChar) {return Assign(aChar);}
nsCString& operator=(char aChar) {return Assign(aChar);}
nsCString& operator=(const char* aCString) {return Assign(aCString);}
#ifdef AIX
nsCString& operator=(const nsSubsumeCStr& aSubsumeString);  // AIX requires a const here
#else
nsCString& operator=(nsSubsumeCStr& aSubsumeString);
#endif

/**
 * Here's a bunch of append mehtods for varying types...
 * @param   aString : string to be appended to this
 * @return  this
 */
nsCString& operator+=(const nsCString& aString){return Append(aString,aString.mLength);}
nsCString& operator+=(const char* aCString) {return Append(aCString);}
nsCString& operator+=(PRUnichar aChar){return Append(aChar);}
nsCString& operator+=(char aChar){return Append(aChar);}

/*
 *  Appends n characters from given string to this,
 *  This version computes the length of your given string
 *  
 *  @param   aString is the source to be appended to this
 *  @return  number of chars copied
 */
nsCString& Append(const nsCString& aString) {return Append(aString,aString.mLength);}
 

/*
 *  Appends n characters from given string to this,
 *  
 *  @param   aString is the source to be appended to this
 *  @param   aCount -- number of chars to copy; -1 tells us to compute the strlen for you
 *  @return  number of chars copied
 */
nsCString& Append(const nsCString& aString,PRInt32 aCount);
nsCString& Append(const char* aString,PRInt32 aCount=-1);
nsCString& Append(PRUnichar aChar);
nsCString& Append(char aChar);
nsCString& Append(PRInt32 aInteger,PRInt32 aRadix=10); //radix=8,10 or 16
nsCString& Append(float aFloat);
             
/*
 *  Copies n characters from this string to given string,
 *  starting at the leftmost offset.
 *  
 *  
 *  @param   aCopy -- Receiving string
 *  @param   aCount -- number of chars to copy
 *  @return  number of chars copied
 */
PRUint32 Left(nsCString& aCopy,PRInt32 aCount) const;

/*
 *  Copies n characters from this string to given string,
 *  starting at the given offset.
 *  
 *  
 *  @param   aCopy -- Receiving string
 *  @param   aCount -- number of chars to copy
 *  @param   anOffset -- position where copying begins
 *  @return  number of chars copied
 */
PRUint32 Mid(nsCString& aCopy,PRUint32 anOffset,PRInt32 aCount) const;

/*
 *  Copies n characters from this string to given string,
 *  starting at rightmost char.
 *  
 *  
 *  @param  aCopy -- Receiving string
 *  @param  aCount -- number of chars to copy
 *  @return number of chars copied
 */
PRUint32 Right(nsCString& aCopy,PRInt32 aCount) const;

/*
 *  This method inserts n chars from given string into this
 *  string at str[anOffset].
 *  
 *  @param  aCopy -- String to be inserted into this
 *  @param  anOffset -- insertion position within this str
 *  @param  aCount -- number of chars to be copied from aCopy
 *  @return number of chars inserted into this.
 */
nsCString& Insert(const nsCString& aCopy,PRUint32 anOffset,PRInt32 aCount=-1);

/**
 * Insert a given string into this string at
 * a specified offset.
 *
 * @param   aString* to be inserted into this string
 * @param   anOffset is insert pos in str 
 * @return  the number of chars inserted into this string
 */
nsCString& Insert(const char* aChar,PRUint32 anOffset,PRInt32 aCount=-1);

/**
 * Insert a single char into this string at
 * a specified offset.
 *
 * @param   character to be inserted into this string
 * @param   anOffset is insert pos in str 
 * @return  the number of chars inserted into this string
 */
nsCString& Insert(PRUnichar aChar,PRUint32 anOffset);
nsCString& Insert(char aChar,PRUint32 anOffset);

/*
 *  This method is used to cut characters in this string
 *  starting at anOffset, continuing for aCount chars.
 *  
 *  @param  anOffset -- start pos for cut operation
 *  @param  aCount -- number of chars to be cut
 *  @return *this
 */
nsCString& Cut(PRUint32 anOffset,PRInt32 aCount);


/**********************************************************************
  Searching methods...                
 *********************************************************************/
 
/**
 *  Search for given character within this string.
 *  This method does so by using a binary search,
 *  so your string HAD BETTER BE ORDERED!
 *  
 *  @param   aChar is the unicode char to be found
 *  @return  offset in string, or -1 (kNotFound)
 */
PRInt32 BinarySearch(PRUnichar aChar) const;

/**
 *  Search for given substring within this string
 *  
 *  @param   aString is substring to be sought in this
 *  @param   anOffset tells us where in this strig to start searching
 *  @return  offset in string, or -1 (kNotFound)
 */
PRInt32 Find(const nsStr& aString,PRBool aIgnoreCase=PR_FALSE,PRInt32 anOffset=-1) const;
PRInt32 Find(const char* aString,PRBool aIgnoreCase=PR_FALSE,PRInt32 anOffset=-1) const;
PRInt32 Find(const PRUnichar* aString,PRBool aIgnoreCase=PR_FALSE,PRInt32 anOffset=-1) const;

/**
 *  Search for given char within this string
 *  
 *  @param   aString is substring to be sought in this
 *  @param   anOffset tells us where in this strig to start searching
 *  @param   aIgnoreCase selects case sensitivity
 *  @return  find pos in string, or -1 (kNotFound)
 */
PRInt32 Find(PRUnichar aChar,PRInt32 offset=-1,PRBool aIgnoreCase=PR_FALSE) const;
PRInt32 FindChar(PRUnichar aChar,PRBool aIgnoreCase=PR_FALSE,PRInt32 anOffset=-1) const;

/**
 * This method searches this string for the first character
 * found in the given string
 * @param aString contains set of chars to be found
 * @param anOffset tells us where to start searching in this
 * @return -1 if not found, else the offset in this
 */
PRInt32 FindCharInSet(const char* aString,PRInt32 anOffset=-1) const;
PRInt32 FindCharInSet(const PRUnichar* aString,PRInt32 anOffset=-1) const;
PRInt32 FindCharInSet(const nsStr& aString,PRInt32 anOffset=-1) const;


/**
 * This methods scans the string backwards, looking for the given string
 * @param   aString is substring to be sought in this
 * @param   aIgnoreCase tells us whether or not to do caseless compare
 * @return  offset in string, or -1 (kNotFound)
 */
PRInt32 RFind(const char* aCString,PRBool aIgnoreCase=PR_FALSE,PRInt32 anOffset=-1) const;
PRInt32 RFind(const nsStr& aString,PRBool aIgnoreCase=PR_FALSE,PRInt32 anOffset=-1) const;
PRInt32 RFind(const PRUnichar* aString,PRBool aIgnoreCase=PR_FALSE,PRInt32 anOffset=-1) const;


/**
 *  Search for given char within this string
 *  
 *  @param   aString is substring to be sought in this
 *  @param   anOffset tells us where in this strig to start searching
 *  @param   aIgnoreCase selects case sensitivity
 *  @return  find pos in string, or -1 (kNotFound)
 */
PRInt32 RFind(PRUnichar aChar,PRInt32 offset=-1,PRBool aIgnoreCase=PR_FALSE) const;
PRInt32 RFindChar(PRUnichar aChar,PRBool aIgnoreCase=PR_FALSE,PRInt32 anOffset=-1) const;

/**
 * This method searches this string for the last character
 * found in the given string
 * @param aString contains set of chars to be found
 * @param anOffset tells us where to start searching in this
 * @return -1 if not found, else the offset in this
 */
PRInt32 RFindCharInSet(const char* aString,PRInt32 anOffset=-1) const;
PRInt32 RFindCharInSet(const PRUnichar* aString,PRInt32 anOffset=-1) const;
PRInt32 RFindCharInSet(const nsStr& aString,PRInt32 anOffset=-1) const;



/**********************************************************************
  Comparison methods...                
 *********************************************************************/

/**
 * Compares a given string type to this string. 
 * @update	gess 7/27/98
 * @param   S is the string to be compared
 * @param   aIgnoreCase tells us how to treat case
 * @param   aCount tells us how many chars to compare
 * @return  -1,0,1
 */
virtual PRInt32 Compare(const nsStr &aString,PRBool aIgnoreCase=PR_FALSE,PRInt32 aCount=-1) const;
virtual PRInt32 Compare(const char* aString,PRBool aIgnoreCase=PR_FALSE,PRInt32 aCount=-1) const;
virtual PRInt32 Compare(const PRUnichar* aString,PRBool aIgnoreCase=PR_FALSE,PRInt32 aCount=-1) const;

/**
 * These methods compare a given string type to this one
 * @param aString is the string to be compared to this
 * @return  TRUE or FALSE
 */
PRBool  operator==(const nsStr &aString) const;
PRBool  operator==(const char* aString) const;
PRBool  operator==(const PRUnichar* aString) const;

/**
 * These methods perform a !compare of a given string type to this 
 * @param aString is the string to be compared to this
 * @return  TRUE 
 */
PRBool  operator!=(const nsStr &aString) const;
PRBool  operator!=(const char* aString) const;
PRBool  operator!=(const PRUnichar* aString) const;

/**
 * These methods test if a given string is < than this
 * @param aString is the string to be compared to this
 * @return  TRUE or FALSE
 */
PRBool  operator<(const nsStr &aString) const;
PRBool  operator<(const char* aString) const;
PRBool  operator<(const PRUnichar* aString) const;

/**
 * These methods test if a given string is > than this
 * @param aString is the string to be compared to this
 * @return  TRUE or FALSE
 */
PRBool  operator>(const nsStr &S) const;
PRBool  operator>(const char* aString) const;
PRBool  operator>(const PRUnichar* aString) const;

/**
 * These methods test if a given string is <= than this
 * @param aString is the string to be compared to this
 * @return  TRUE or FALSE
 */
PRBool  operator<=(const nsStr &S) const;
PRBool  operator<=(const char* aString) const;
PRBool  operator<=(const PRUnichar* aString) const;

/**
 * These methods test if a given string is >= than this
 * @param aString is the string to be compared to this
 * @return  TRUE or FALSE
 */
PRBool  operator>=(const nsStr &S) const;
PRBool  operator>=(const char* aString) const;
PRBool  operator>=(const PRUnichar* aString) const;

/**
 * Compare this to given string; note that we compare full strings here.
 * The optional length argument just lets us know how long the given string is.
 * If you provide a length, it is compared to length of this string as an
 * optimization.
 * 
 * @param  aString -- the string to compare to this
 * @param  aCount  -- number of chars in given string you want to compare
 * @return TRUE if equal
 */
PRBool  Equals(const nsString &aString,PRBool aIgnoreCase=PR_FALSE,PRInt32 aCount=-1) const;
PRBool  Equals(const nsStr& aString,PRBool aIgnoreCase=PR_FALSE,PRInt32 aCount=-1) const;
PRBool  Equals(const char* aString,PRBool aIgnoreCase=PR_FALSE,PRInt32 aCount=-1) const;
PRBool  Equals(const PRUnichar* aString,PRBool aIgnoreCase=PR_FALSE,PRInt32 aCount=-1) const;

PRBool  EqualsIgnoreCase(const nsStr& aString) const;
PRBool  EqualsIgnoreCase(const char* aString,PRInt32 aCount=-1) const;
PRBool  EqualsIgnoreCase(const PRUnichar* aString,PRInt32 aCount=-1) const;


static  void        Recycle(nsCString* aString);
static  nsCString*  CreateString(void);


  nsIMemoryAgent* mAgent;

};

extern NS_COM int fputs(const nsCString& aString, FILE* out);
//ostream& operator<<(ostream& aStream,const nsCString& aString);
//virtual void  DebugDump(ostream& aStream) const;


/**************************************************************
  Here comes the AutoString class which uses internal memory
  (typically found on the stack) for its default buffer.
  If the buffer needs to grow, it gets reallocated on the heap.
 **************************************************************/

class NS_COM nsCAutoString : public nsCString {
public: 

    nsCAutoString();
    nsCAutoString(const char* aString,PRInt32 aLength=-1);
    nsCAutoString(const CBufDescriptor& aBuffer);
    nsCAutoString(const PRUnichar* aString,PRInt32 aLength=-1);
    nsCAutoString(const nsStr& aString);
    nsCAutoString(const nsCAutoString& aString);

#ifdef AIX
    nsCAutoString(const nsSubsumeCStr& aSubsumeStr);  // AIX requires a const
#else
    nsCAutoString(nsSubsumeCStr& aSubsumeStr);
#endif // AIX
    nsCAutoString(PRUnichar aChar);
    virtual ~nsCAutoString();

    nsCAutoString& operator=(const nsCString& aString) {nsCString::Assign(aString); return *this;}
    nsCAutoString& operator=(const char* aCString) {nsCString::Assign(aCString); return *this;}
    nsCAutoString& operator=(PRUnichar aChar) {nsCString::Assign(aChar); return *this;}
    nsCAutoString& operator=(char aChar) {nsCString::Assign(aChar); return *this;}

    /**
     * Retrieve the size of this string
     * @return string length
     */
    virtual void SizeOf(nsISizeOfHandler* aHandler) const;
    
    char mBuffer[32];
};

/***************************************************************
  The subsumestr class is very unusual. 
  It differs from a normal string in that it doesn't use normal
  copy semantics when another string is assign to this. 
  Instead, it "steals" the contents of the source string.

  This is very handy for returning nsString classes as part of
  an operator+(...) for example, in that it cuts down the number
  of copy operations that must occur. 

  You should probably not use this class unless you really know
  what you're doing.
 ***************************************************************/
class NS_COM nsSubsumeCStr : public nsCString {
public:
  nsSubsumeCStr(nsStr& aString);
  nsSubsumeCStr(PRUnichar* aString,PRBool assumeOwnership,PRInt32 aLength=-1);
  nsSubsumeCStr(char* aString,PRBool assumeOwnership,PRInt32 aLength=-1);
};



#endif


