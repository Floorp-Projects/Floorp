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

  This class provides a 1-byte ASCII string implementation that shares
  a common API with all other strImpl derivatives. 
 ***********************************************************************/


#ifndef _nsString2
#define _nsString2

//#define nsString2     nsString
//#define nsAutoString2 nsAutoString


#include "prtypes.h"
#include "nscore.h"
#include <iostream.h>
#include <stdio.h>
#include "nsCRT.h"

#include "nsStr.h"
#include <iostream.h>
#include <stdio.h>
#include "nsIAtom.h"

class nsISizeOfHandler;


class NS_BASE nsString2 : public nsStr {

  public: 

/**
 * Default constructor. Note that we actually allocate a small buffer
 * to begin with. This is because the "philosophy" of the string class
 * was to allow developers direct access to the underlying buffer for
 * performance reasons. 
 */
nsString2(eCharSize aCharSize=kDefaultCharSize,nsIMemoryAgent* anAgent=0);


/**
 * This constructor accepts an isolatin string
 * @param   aCString is a ptr to a 1-byte cstr
 */
nsString2(const char* aCString,eCharSize aCharSize=kDefaultCharSize,nsIMemoryAgent* anAgent=0);

/**
 * This constructor accepts a unichar string
 * @param   aCString is a ptr to a 2-byte cstr
 */
nsString2(const PRUnichar* aString,eCharSize aCharSize=kDefaultCharSize,nsIMemoryAgent* anAgent=0);

/**
 * This is a copy constructor that accepts an nsStr
 * @param   reference to another nsString2
 */
nsString2(const nsStr&,eCharSize aCharSize=kDefaultCharSize,nsIMemoryAgent* anAgent=0);

/**
 * This is our copy constructor 
 * @param   reference to another nsString2
 */
nsString2(const nsString2& aString);   

/**
 * Destructor
 * 
 */
virtual ~nsString2();    

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
  SetCapacity(aLength);
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

/**********************************************************************
  Accessor methods...
 *********************************************************************/

char* GetBuffer(void) const;
PRUnichar* GetUnicode(void) const;
operator PRUnichar*() const {return GetUnicode();}


 /**
   * Get nth character.
   */
PRUnichar operator[](int anIndex) const;
PRUnichar CharAt(int anIndex) const;
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
nsString2 operator+(const nsStr& aString);

/**
 * Create a new string by appending given string to this
 * @param   aString -- 2nd string to be appended
 * @return  new string
 */
nsString2 operator+(const nsString2& aString);

/**
 * create a new string by adding this to the given buffer.
 * @param   aCString is a ptr to cstring to be added to this
 * @return  newly created string
 */
nsString2 operator+(const char* aCString);

/**
 * create a new string by adding this to the given wide buffer.
 * @param   aString is a ptr to UC-string to be added to this
 * @return  newly created string
 */
nsString2 operator+(const PRUnichar* aString);

/**
 * create a new string by adding this to the given char.
 * @param   aChar is a char to be added to this
 * @return  newly created string
 */
nsString2 operator+(char aChar);

/**
 * create a new string by adding this to the given char.
 * @param   aChar is a unichar to be added to this
 * @return  newly created string
 */
nsString2 operator+(PRUnichar aChar);

/**********************************************************************
  Lexomorphic transforms...
 *********************************************************************/

/**
 * Converts all chars in given string to UCS2
 * which ensure that the lower 256 chars are correct.
 */
void ToUCS2(PRUint32 aStartOffset);

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
void ToLowerCase(nsString2& aString) const;

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
void ToUpperCase(nsString2& aString) const;


/**
 *  This method is used to remove all occurances of the
 *  characters found in aSet from this string.
 *  
 *  @param  aSet -- characters to be cut from this
 *  @return *this 
 */
nsString2& StripChars(const char* aSet);

/**
 *  This method strips whitespace throughout the string
 *  
 *  @return  this
 */
nsString2& StripWhitespace();

/**
 *  swaps occurence of 1 string for another
 *  
 *  @return  this
 */
nsString2& ReplaceChar(PRUnichar aSourceChar,PRUnichar aDestChar);

/**
 *  This method trims characters found in aTrimSet from
 *  either end of the underlying string.
 *  
 *  @param   aTrimSet -- contains chars to be trimmed from
 *           both ends
 *  @return  this
 */
nsString2& Trim(const char* aSet,PRBool aEliminateLeading=PR_TRUE,PRBool aEliminateTrailing=PR_TRUE);

/**
 *  This method strips whitespace from string.
 *  You can control whether whitespace is yanked from
 *  start and end of string as well.
 *  
 *  @param   aEliminateLeading controls stripping of leading ws
 *  @param   aEliminateTrailing controls stripping of trailing ws
 *  @return  this
 */
nsString2& CompressSet(const char* aSet, char aChar, PRBool aEliminateLeading=PR_TRUE,PRBool aEliminateTrailing=PR_TRUE);

/**
 *  This method strips whitespace from string.
 *  You can control whether whitespace is yanked from
 *  start and end of string as well.
 *  
 *  @param   aEliminateLeading controls stripping of leading ws
 *  @param   aEliminateTrailing controls stripping of trailing ws
 *  @return  this
 */
nsString2& CompressWhitespace( PRBool aEliminateLeading=PR_TRUE,PRBool aEliminateTrailing=PR_TRUE);

/**********************************************************************
  string conversion methods...
 *********************************************************************/

/**
 * This method constructs a new nsString2 on the stack that is a copy
 * of this string.
 * 
 */
nsString2* ToNewString() const;

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
char* ToCString(char* aBuf,PRUint32 aBufLength) const;

/**
 * Perform string to float conversion.
 * @param   aErrorCode will contain error if one occurs
 * @return  float rep of string value
 */
float ToFloat(PRInt32* aErrorCode) const;

/**
 * Perform string to int conversion.
 * @param   aErrorCode will contain error if one occurs
 * @return  int rep of string value
 */
PRInt32 ToInteger(PRInt32* aErrorCode,PRUint32 aRadix=10) const;


/**********************************************************************
  String manipulation methods...                
 *********************************************************************/

/**
 * Functionally equivalent to assign or operator=
 * 
 */
nsString2& SetString(const char* aString,PRInt32 aLength=-1) {return Assign(aString,aLength);}
nsString2& SetString(const PRUnichar* aString,PRInt32 aLength=-1) {return Assign(aString,aLength);}
nsString2& SetString(const nsString2& aString,PRInt32 aLength=-1) {return Assign(aString,aLength);}
void Copy(nsString2& aString) const;

/**
 * assign given string to this string
 * @param   aStr: buffer to be assigned to this 
 * @param   alength is the length of the given str (or -1)
            if you want me to determine its length
 * @return  this
 */
nsString2& Assign(const nsString2& aString,PRInt32 aCount=-1);
nsString2& Assign(const nsStr& aString,PRInt32 aCount=-1);
nsString2& Assign(const char* aString,PRInt32 aCount=-1);
nsString2& Assign(const PRUnichar* aString,PRInt32 aCount=-1);
nsString2& Assign(char aChar);
nsString2& Assign(PRUnichar aChar);

/**
 * here come a bunch of assignment operators...
 * @param   aString: string to be added to this
 * @return  this
 */
nsString2& operator=(const nsString2& aString) {return Assign(aString);}
nsString2& operator=(const nsStr& aString) {return Assign(aString);}
nsString2& operator=(char aChar) {return Assign(aChar);}
nsString2& operator=(PRUnichar aChar) {return Assign(aChar);}
nsString2& operator=(const char* aCString) {return Assign(aCString);}
nsString2& operator=(const PRUnichar* aString) {return Assign(aString);}

/**
 * Here's a bunch of append mehtods for varying types...
 * @param   aString : string to be appended to this
 * @return  this
 */
nsString2& operator+=(const nsStr& aString){return Append(aString,aString.mLength);}
nsString2& operator+=(const nsString2& aString){return Append(aString,aString.mLength);}
nsString2& operator+=(const char* aCString) {return Append(aCString);}
//nsString2& operator+=(char aChar){return Append(aChar);}
nsString2& operator+=(const PRUnichar* aUCString) {return Append(aUCString);}
nsString2& operator+=(PRUnichar aChar){return Append(aChar);}

/*
 *  Appends n characters from given string to this,
 *  This version computes the length of your given string
 *  
 *  @param   aString is the source to be appended to this
 *  @return  number of chars copied
 */
nsString2& Append(const nsStr& aString) {return Append(aString,aString.mLength);}
nsString2& Append(const nsString2& aString) {return Append(aString,aString.mLength);}
nsString2& Append(const char* aString) {if(aString) {Append(aString,nsCRT::strlen(aString));} return *this;}
nsString2& Append(const PRUnichar* aString) {if(aString) {Append(aString,nsCRT::strlen(aString));} return *this;}

/*
 *  Appends n characters from given string to this,
 *  
 *  @param   aString is the source to be appended to this
 *  @param   aCount -- number of chars to copy
 *  @return  number of chars copied
 */
nsString2& Append(const nsStr& aString,PRInt32 aCount);
nsString2& Append(const nsString2& aString,PRInt32 aCount);
nsString2& Append(const char* aString,PRInt32 aCount);
nsString2& Append(const PRUnichar* aString,PRInt32 aCount);
nsString2& Append(char aChar);
nsString2& Append(PRUnichar aChar);
nsString2& Append(PRInt32 aInteger,PRInt32 aRadix=10); //radix=8,10 or 16
nsString2& Append(float aFloat);
              
/*
 *  Copies n characters from this string to given string,
 *  starting at the leftmost offset.
 *  
 *  
 *  @param   aCopy -- Receiving string
 *  @param   aCount -- number of chars to copy
 *  @return  number of chars copied
 */
PRUint32 Left(nsString2& aCopy,PRInt32 aCount) const;

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
PRUint32 Mid(nsString2& aCopy,PRUint32 anOffset,PRInt32 aCount) const;

/*
 *  Copies n characters from this string to given string,
 *  starting at rightmost char.
 *  
 *  
 *  @param  aCopy -- Receiving string
 *  @param  aCount -- number of chars to copy
 *  @return number of chars copied
 */
PRUint32 Right(nsString2& aCopy,PRInt32 aCount) const;

/*
 *  This method inserts n chars from given string into this
 *  string at str[anOffset].
 *  
 *  @param  aCopy -- String to be inserted into this
 *  @param  anOffset -- insertion position within this str
 *  @param  aCount -- number of chars to be copied from aCopy
 *  @return number of chars inserted into this.
 */
nsString2& Insert(nsString2& aCopy,PRUint32 anOffset,PRInt32 aCount=-1);

/**
 * Insert a given string into this string at
 * a specified offset.
 *
 * @param   aString* to be inserted into this string
 * @param   anOffset is insert pos in str 
 * @return  the number of chars inserted into this string
 */
nsString2& Insert(const char* aChar,PRUint32 anOffset,PRInt32 aCount=-1);
nsString2& Insert(const PRUnichar* aChar,PRUint32 anOffset,PRInt32 aCount=-1);

/**
 * Insert a single char into this string at
 * a specified offset.
 *
 * @param   character to be inserted into this string
 * @param   anOffset is insert pos in str 
 * @return  the number of chars inserted into this string
 */
//nsString2& Insert(char aChar,PRUint32 anOffset);
nsString2& Insert(PRUnichar aChar,PRUint32 anOffset);

/*
 *  This method is used to cut characters in this string
 *  starting at anOffset, continuing for aCount chars.
 *  
 *  @param  anOffset -- start pos for cut operation
 *  @param  aCount -- number of chars to be cut
 *  @return *this
 */
nsString2& Cut(PRUint32 anOffset,PRInt32 aCount);


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
 *  @return  offset in string, or -1 (kNotFound)
 */
PRInt32 Find(const nsStr& aString,PRBool aIgnoreCase=PR_FALSE) const;
PRInt32 Find(const char* aString,PRBool aIgnoreCase=PR_FALSE) const;
PRInt32 Find(const PRUnichar* aString,PRBool aIgnoreCase=PR_FALSE) const;
PRInt32 Find(PRUnichar aChar,PRBool aIgnoreCase=PR_FALSE,PRUint32 offset=0) const;

/**
 * This method searches this string for the first character
 * found in the given string
 * @param aString contains set of chars to be found
 * @param anOffset tells us where to start searching in this
 * @return -1 if not found, else the offset in this
 */
PRInt32 FindCharInSet(const char* aString,PRUint32 anOffset=0) const;
PRInt32 FindCharInSet(const PRUnichar* aString,PRUint32 anOffset=0) const;
PRInt32 FindCharInSet(const nsString2& aString,PRUint32 anOffset=0) const;

/**
 * This method searches this string for the last character
 * found in the given string
 * @param aString contains set of chars to be found
 * @param anOffset tells us where to start searching in this
 * @return -1 if not found, else the offset in this
 */
PRInt32 RFindCharInSet(const char* aString,PRUint32 anOffset=0) const;
PRInt32 RFindCharInSet(const PRUnichar* aString,PRUint32 anOffset=0) const;
PRInt32 RFindCharInSet(const nsString2& aString,PRUint32 anOffset=0) const;


/**
 * This methods scans the string backwards, looking for the given string
 * @param   aString is substring to be sought in this
 * @param   aIgnoreCase tells us whether or not to do caseless compare
 * @return  offset in string, or -1 (kNotFound)
 */
PRInt32 RFind(const char* aCString,PRBool aIgnoreCase=PR_FALSE) const;
PRInt32 RFind(const nsStr& aString,PRBool aIgnoreCase=PR_FALSE) const;
PRInt32 RFind(const PRUnichar* aString,PRBool aIgnoreCase=PR_FALSE) const;
PRInt32 RFind(PRUnichar aChar,PRBool aIgnoreCase=PR_FALSE,PRUint32 offset=0) const;

/**********************************************************************
  Comparison methods...                
 *********************************************************************/

/**
 * Compares a given string type to this string. 
 * @update	gess 7/27/98
 * @param   S is the string to be compared
 * @param   aIgnoreCase tells us how to treat case
 * @return  -1,0,1
 */
virtual PRInt32 Compare(const nsStr &aString,PRBool aIgnoreCase=PR_FALSE,PRInt32 aLength=-1) const;
virtual PRInt32 Compare(const char* aString,PRBool aIgnoreCase=PR_FALSE,PRInt32 aLength=-1) const;
virtual PRInt32 Compare(const PRUnichar* aString,PRBool aIgnoreCase=PR_FALSE,PRInt32 aLength=-1) const;

/**
 * These methods compare a given string type to this one
 * @param aString is the string to be compared to this
 * @return  TRUE or FALSE
 */
PRBool  operator==(const nsStr &aString) const;
PRBool  operator==(const char *aString) const;
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
 * @param  aLength -- optional length of given string.
 * @return TRUE if equal
 */
PRBool  Equals(const nsStr& aString,PRBool aIgnoreCase=PR_FALSE) const;
PRBool  Equals(const char* aString,PRBool aIgnoreCase=PR_FALSE) const;
PRBool  Equals(const char* aString,PRUint32 aCount,PRBool aIgnoreCase) const;   
PRBool  Equals(const PRUnichar* aString,PRBool aIgnoreCase=PR_FALSE) const;
PRBool  Equals(const PRUnichar* aString,PRUint32 aCount,PRBool aIgnoreCase) const;   
PRBool  Equals(const nsIAtom* anAtom,PRBool aIgnoreCase) const;   
PRBool  Equals(const PRUnichar* s1, const PRUnichar* s2,PRBool aIgnoreCase=PR_FALSE) const;

PRBool  EqualsIgnoreCase(const nsString2& aString) const;
PRBool  EqualsIgnoreCase(const char* aString,PRInt32 aLength=-1) const;
PRBool  EqualsIgnoreCase(const nsIAtom *aAtom) const;
PRBool  EqualsIgnoreCase(const PRUnichar* s1, const PRUnichar* s2) const;


/**
 *  Determine if given char is a valid space character
 *  
 *  @param   aChar is character to be tested
 *  @return  TRUE if is valid space char
 */
static  PRBool IsSpace(PRUnichar ch);

/**
 *  Determine if given char in valid alpha range
 *  
 *  @param   aChar is character to be tested
 *  @return  TRUE if in alpha range
 */
static  PRBool IsAlpha(PRUnichar ch);

/**
 *  Determine if given char is valid digit
 *  
 *  @param   aChar is character to be tested
 *  @return  TRUE if char is a valid digit
 */
static  PRBool IsDigit(PRUnichar ch);


static void   SelfTest();
virtual void  DebugDump(ostream& aStream) const;

#ifdef  NS_DEBUG
  static	PRBool	mSelfTested;
#endif

  nsIMemoryAgent* mAgent;

};

extern NS_BASE int fputs(const nsString2& aString, FILE* out);
ostream& operator<<(ostream& os,nsString2& aString);


/**************************************************************
  Here comes the AutoString class which uses internal memory
  (typically found on the stack) for its default buffer.
  If the buffer needs to grow, it gets reallocated on the heap.
 **************************************************************/


class NS_BASE nsAutoString2 : public nsString2 {
public: 

    nsAutoString2(eCharSize aCharSize=kDefaultCharSize);
    nsAutoString2(nsBufDescriptor& anExtBuffer,const char* aCString);


    nsAutoString2(const char* aCString,eCharSize aCharSize=kDefaultCharSize);
    nsAutoString2(char* aCString,PRUint32 aLength,eCharSize aCharSize=kDefaultCharSize,PRBool assumeOwnership=PR_FALSE);
    nsAutoString2(const PRUnichar* aString,eCharSize aCharSize=kDefaultCharSize);
    nsAutoString2(PRUnichar* aString,PRUint32 aLength,eCharSize aCharSize=kDefaultCharSize,PRBool assumeOwnership=PR_FALSE);
    
    nsAutoString2(const nsStr& aString,eCharSize aCharSize=kDefaultCharSize);
    nsAutoString2(const nsString2& aString,eCharSize aCharSize=kDefaultCharSize);
    nsAutoString2(const nsAutoString2& aString,eCharSize aCharSize=kDefaultCharSize);
    nsAutoString2(PRUnichar aChar,eCharSize aCharSize=kDefaultCharSize);
    virtual ~nsAutoString2();

    nsAutoString2& operator=(const nsString2& aString) {nsString2::operator=(aString); return *this;}
    nsAutoString2& operator=(const nsStr& aString) {nsString2::Assign(aString); return *this;}
    nsAutoString2& operator=(const nsAutoString2& aString) {nsString2::operator=(aString); return *this;}
    nsAutoString2& operator=(const char* aCString) {nsString2::operator=(aCString); return *this;}
    nsAutoString2& operator=(char aChar) {nsString2::operator=(aChar); return *this;}
    nsAutoString2& operator=(const PRUnichar* aBuffer) {nsString2::operator=(aBuffer); return *this;}
    nsAutoString2& operator=(PRUnichar aChar) {nsString2::operator=(aChar); return *this;}

    /**
     * Retrieve the size of this string
     * @return string length
     */
    virtual void SizeOf(nsISizeOfHandler* aHandler) const;
    
    char mBuffer[32];
};


#endif

