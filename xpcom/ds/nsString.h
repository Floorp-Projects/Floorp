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

    A. There are two philosophies to building string classes:
        1. Hide the underlying buffer & offer API's allow indirect iteration
        2. Reveal underlying buffer, risk corruption, but gain performance
        
       We chose the second option for performance reasons. 

    B Our internal buffer always holds capacity+1 bytes.
 ***********************************************************************/


#ifndef _NSSTRING
#define _NSSTRING


#include "prtypes.h"
#include "nscore.h"
#include "nsIAtom.h"
#include <iostream.h>
#include <stdio.h>
class nsISizeOfHandler;

class NS_BASE nsString {
  public: 

/**
 * Default constructor. Note that we actually allocate a small buffer
 * to begin with. This is because the "philosophy" of the string class
 * was to allow developers direct access to the underlying buffer for
 * performance reasons. 
 */
nsString();

/**
 * This constructor accepts an isolatin string
 * @param   an ascii is a ptr to a 1-byte cstr
 */
nsString(const char* aCString);

/**
 * This is our copy constructor 
 * @param   reference to another nsString
 */
nsString(const nsString&);

/**
 * Constructor from a unicode string
 * @param   anicodestr pts to a unicode string
 */
nsString(const PRUnichar* aUnicode);    

/**
 * Virtual Destructor
 */
virtual ~nsString();


/**
 * Retrieve the length of this string
 * @return string length
 */
PRInt32 Length() const { return mLength; }


/**
 * Sets the new length of the string.
 * @param   aLength is new string length.
 * @return  nada
 */
void SetLength(PRInt32 aLength);

/**
 * This method truncates this string to given length.
 *
 * @param   anIndex -- new length of string
 * @return  nada
 */
void Truncate(PRInt32 anIndex=0);


/**
 * This method gets called when the internal buffer needs
 * to grow to a given size.
 * @param   aNewLength -- new capacity of string  
 * @return  void
 */
virtual void EnsureCapacityFor(PRInt32 aNewLength);

/**
 * 
 * @param 
 */
virtual void SizeOf(nsISizeOfHandler* aHandler) const;

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

/**
 * Retrieve pointer to internal string value
 * @return  PRUnichar* to internal string
 */
const PRUnichar* GetUnicode(void) const;

/**
 * 
 * @param 
 * @return
 */
operator const PRUnichar*() const;

/**
 * Retrieve unicode char at given index
 * @param   offset into string
 * @return  PRUnichar* to internal string
 */
PRUnichar operator()(PRInt32 anIndex) const;

/**
 * Retrieve reference to unicode char at given index
 * @param   offset into string
 * @return  PRUnichar& from internal string
 */
PRUnichar& operator[](PRInt32 anIndex) const;

/**
 * Retrieve reference to unicode char at given index
 * @param   offset into string
 * @return  PRUnichar& from internal string
 */
PRUnichar& CharAt(PRInt32 anIndex) const;

/**
 * Retrieve reference to first unicode char in string
 * @return  PRUnichar from internal string
 */
PRUnichar& First() const; 

/**
 * Retrieve reference to last unicode char in string
 * @return  PRUnichar from internal string
 */
PRUnichar& Last() const;

PRBool SetCharAt(PRUnichar aChar,PRInt32 anIndex);


/**********************************************************************
  String creation methods...
 *********************************************************************/

/**
 * Create a new string by appending given string to this
 * @param   aString -- 2nd string to be appended
 * @return  new string
 */
nsString operator+(const nsString& aString);

/**
 * create a new string by adding this to the given buffer.
 * @param   aCString is a ptr to cstring to be added to this
 * @return  newly created string
 */
nsString operator+(const char* aCString);

/**
 * create a new string by adding this to the given char.
 * @param   aChar is a char to be added to this
 * @return  newly created string
 */
nsString operator+(char aChar);

/**
 * create a new string by adding this to the given buffer.
 * @param   aStr unichar buffer to be added to this
 * @return  newly created string
 */
nsString operator+(const PRUnichar* aBuffer);

/**
 * create a new string by adding this to the given char.
 * @param   aChar is a unichar to be added to this
 * @return  newly created string
 */
nsString operator+(PRUnichar aChar);

/**
 * Converts all chars in internal string to lower
 */
void ToLowerCase();

/**
 * Converts all chars in given string to lower
 */
void ToLowerCase(nsString& aString) const;

/**
 * Converts all chars in given string to upper
 */
void ToUpperCase();

/**
 * Converts all chars in given string to UCS2
 * which ensure that the lower 256 chars are correct.
 */
void ToUCS2(PRInt32 aStartOffset);

/**
 * Converts all chars in internal string to upper
 */
void ToUpperCase(nsString& aString) const;

/**
 * Creates a duplicate clone (ptr) of this string.
 * @return  ptr to clone of this string
 */
nsString* ToNewString() const;

/**
 * Creates an ascii clone of this string
 * NOTE: This string is allocated with new; YOU MUST deallocate with delete[]!
 * @return  ptr to new c-String string
 */
char* ToNewCString() const;

/**
 * Copies data from internal buffer onto given char* buffer
 * @param aBuf is the buffer where data is stored
 * @param aBuflength is the max # of chars to move to buffer
 * @return ptr to given buffer
 */
char* ToCString(char* aBuf,PRInt32 aBufLength) const;

/**
 * Copies contents of this onto given string.
 * @param   aString to hold copy of this
 * @return  nada.
 */
void Copy(nsString& aString) const;

/**
 * Creates an unichar clone of this string
 * @return  ptr to new unichar string
 */
PRUnichar* ToNewUnicode() const;

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
PRInt32 ToInteger(PRInt32* aErrorCode,PRInt32 aRadix=10) const;

/**********************************************************************
  String manipulation methods...                
 *********************************************************************/

/**
 * assign given PRUnichar* to this string
 * @param   aStr: buffer to be assigned to this 
 * @param   alength is the length of the given str (or -1)
            if you want me to determine its length
 * @return  this
 */
nsString& SetString(const PRUnichar* aStr,PRInt32 aLength=-1);

/**
 * assign given char* to this string
 * @param   aCString: buffer to be assigned to this 
 * @param   alength is the length of the given str (or -1)
            if you want me to determine its length
 * @return  this
 */
nsString& SetString(const char* aCString,PRInt32 aLength=-1);

/**
 * assign given string to this one
 * @param   aString: string to be added to this
 * @return  this
 */
nsString& operator=(const nsString& aString);

/**
 * assign given char* to this string
 * @param   aCString: buffer to be assigned to this 
 * @return  this
 */
nsString& operator=(const char* aCString);

/**
 * assign given char to this string
 * @param   aChar: char to be assignd to this
 * @return  this
 */
nsString& operator=(char aChar);

/**
 * assign given unichar* to this string
 * @param   aBuffer: unichar buffer to be assigned to this 
 * @return  this
 */
nsString& operator=(const PRUnichar* aBuffer);

/**
 * assign given char to this string
 * @param   aChar: char to be assignd to this
 * @return  this
 */
nsString& operator=(PRUnichar aChar);

/**
 * append given string to this string
 * @param   aString : string to be appended to this
 * @return  this
 */
nsString& operator+=(const nsString& aString);

/**
 * append given buffer to this string
 * @param   aCString: buffer to be appended to this
 * @return  this
 */
nsString& operator+=(const char* aCString);

/**
 * append given buffer to this string
 * @param   aBuffer: buffer to be appended to this
 * @return  this
 */
nsString& operator+=(const PRUnichar* aBuffer);

/**
 * append given char to this string
 * @param   aChar: char to be appended to this
 * @return  this
 */
nsString& operator+=(PRUnichar aChar);

/**
 * append given string to this string
 * @param   aString : string to be appended to this
 * @param   alength is the length of the given str (or -1)
            if you want me to determine its length
 * @return  this
 */
nsString& Append(const nsString& aString,PRInt32 aLength=-1);

/**
 * append given string to this string
 * @param   aString : string to be appended to this
 * @param   alength is the length of the given str (or -1)
            if you want me to determine its length
 * @return  this
 */
nsString& Append(const char* aCString,PRInt32 aLength=-1);

/**
 * append given string to this string
 * @param   aString : string to be appended to this
 * @return  this
 */
nsString& Append(char aChar);

/**
 * append given unichar buffer to this string
 * @param   aString : string to be appended to this
 * @param   alength is the length of the given str (or -1)
            if you want me to determine its length
 * @return  this
 */
nsString& Append(const PRUnichar* aBuffer,PRInt32 aLength=-1);

/**
 * append given unichar character to this string
 * @param   aChar is the char to be appended to this
 * @return  this
 */
nsString& Append(PRUnichar aChar);

/**
 * Append an integer onto this string
 * @param aInteger is the int to be appended
 * @param aRadix specifies 8,10,16
 * @return this
 */
nsString& Append(PRInt32 aInteger,PRInt32 aRadix); //radix=8,10 or 16

/**
 * Append a float value onto this string
 * @param aFloat is the float to be appended
 * @return this
 */
nsString& Append(float aFloat);
              
/*
 *  Copies n characters from this string to given string,
 *  starting at the leftmost offset.
 *  
 *  
 *  @param   aCopy -- Receiving string
 *  @param   aCount -- number of chars to copy
 *  @return  number of chars copied
 */
PRInt32 Left(nsString& aCopy,PRInt32 aCount) const;

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
PRInt32 Mid(nsString& aCopy,PRInt32 anOffset,PRInt32 aCount) const;

/*
 *  Copies n characters from this string to given string,
 *  starting at rightmost char.
 *  
 *  
 *  @param  aCopy -- Receiving string
 *  @param  aCount -- number of chars to copy
 *  @return number of chars copied
 */
PRInt32 Right(nsString& aCopy,PRInt32 aCount) const;

/*
 *  This method inserts n chars from given string into this
 *  string at str[anOffset].
 *  
 *  @param  aCopy -- String to be inserted into this
 *  @param  anOffset -- insertion position within this str
 *  @param  aCount -- number of chars to be copied from aCopy
 *  @return number of chars inserted into this.
 */
PRInt32 Insert(const nsString& aCopy,PRInt32 anOffset,PRInt32 aCount=-1);

/**
 * Insert a single unicode char into this string at
 * a specified offset.
 *
 * @param   aChar char to be inserted into this string
 * @param   anOffset is insert pos in str 
 * @return  the number of chars inserted into this string
 */
PRInt32 Insert(PRUnichar aChar,PRInt32 anOffset);

/*
 *  This method is used to cut characters in this string
 *  starting at anOffset, continuing for aCount chars.
 *  
 *  @param  anOffset -- start pos for cut operation
 *  @param  aCount -- number of chars to be cut
 *  @return *this
 */
nsString& Cut(PRInt32 anOffset,PRInt32 aCount);

/**
 *  This method is used to remove all occurances of the
 *  characters found in aSet from this string.
 *  
 *  @param  aSet -- characters to be cut from this
 *  @return *this 
 */
nsString& StripChars(const char* aSet);

/**
 *  This method is used to replace all occurances of the
 *  given source char with the given dest char
 *  
 *  @param  
 *  @return *this 
 */
nsString& ReplaceChar(PRUnichar aSourceChar, PRUnichar aDestChar);

/**
 *  This method strips whitespace throughout the string
 *  
 *  @return  this
 */
nsString& StripWhitespace();

/**
 *  This method trims characters found in aTrimSet from
 *  either end of the underlying string.
 *  
 *  @param   aTrimSet -- contains chars to be trimmed from
 *           both ends
 *  @return  this
 */
nsString& Trim(const char* aSet,
               PRBool aEliminateLeading=PR_TRUE,
               PRBool aEliminateTrailing=PR_TRUE);

/**
 *  This method strips whitespace from string.
 *  You can control whether whitespace is yanked from
 *  start and end of string as well.
 *  
 *  @param   aEliminateLeading controls stripping of leading ws
 *  @param   aEliminateTrailing controls stripping of trailing ws
 *  @return  this
 */
nsString& CompressWhitespace( PRBool aEliminateLeading=PR_TRUE,
                              PRBool aEliminateTrailing=PR_TRUE);

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
PRInt32 Find(const char* aString) const;
PRInt32 Find(const PRUnichar* aString) const;
PRInt32 Find(const nsString& aString) const;

/**
 *  Search for given char within this string
 *  
 *  @param   aChar - char to be found
 *  @return  offset in string, or -1 (kNotFound)
 */
PRInt32 Find(PRUnichar aChar,PRInt32 offset=0) const;

/**
 * This method searches this string for the first character
 * found in the given string
 * @param aString contains set of chars to be found
 * @param anOffset tells us where to start searching in this
 * @return -1 if not found, else the offset in this
 */
PRInt32 FindCharInSet(const char* aString,PRInt32 anOffset=0) const;
PRInt32 FindCharInSet(nsString& aString,PRInt32 anOffset=0) const;

/**
 * This method searches this string for the last character
 * found in the given string
 * @param aString contains set of chars to be found
 * @param anOffset tells us where to start searching in this
 * @return -1 if not found, else the offset in this
 */
PRInt32 RFindCharInSet(const char* aString,PRInt32 anOffset=0) const;
PRInt32 RFindCharInSet(nsString& aString,PRInt32 anOffset=0) const;


/**
 * This methods scans the string backwards, looking for the given string
 * @param   aString is substring to be sought in this
 * @param   aIgnoreCase tells us whether or not to do caseless compare
 * @return  offset in string, or -1 (kNotFound)
 */
PRInt32 RFind(const char* aCString,PRBool aIgnoreCase=PR_FALSE) const;
PRInt32 RFind(const PRUnichar* aString,PRBool aIgnoreCase=PR_FALSE) const;
PRInt32 RFind(const nsString& aString,PRBool aIgnoreCase=PR_FALSE) const;

/**
 * This methods scans the string backwards, looking for the given char
 * @param   char is the char to be sought in this
 * @param   aIgnoreCase tells us whether or not to do caseless compare
 * @return  offset in string, or -1 (kNotFound)
 */
PRInt32 RFind(PRUnichar aChar,PRBool aIgnoreCase=PR_FALSE) const;

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
virtual PRInt32 Compare(const nsString &aString,PRBool aIgnoreCase=PR_FALSE) const;
virtual PRInt32 Compare(const char *aString,PRBool aIgnoreCase=PR_FALSE,PRInt32 aLength=-1) const;
virtual PRInt32 Compare(const PRUnichar *aString,PRBool aIgnoreCase=PR_FALSE,PRInt32 aLength=-1) const;

/**
 * These methods compare a given string type to this one
 * @param aString is the string to be compared to this
 * @return  TRUE or FALSE
 */
PRBool  operator==(const nsString &aString) const;
PRBool  operator==(const char *aString) const;
PRBool  operator==(const PRUnichar* aString) const;
PRBool  operator==(PRUnichar* aString) const;

/**
 * These methods perform a !compare of a given string type to this 
 * @param aString is the string to be compared to this
 * @return  TRUE 
 */
PRBool  operator!=(const nsString &aString) const;
PRBool  operator!=(const char *aString) const;
PRBool  operator!=(const PRUnichar* aString) const;

/**
 * These methods test if a given string is < than this
 * @param aString is the string to be compared to this
 * @return  TRUE or FALSE
 */
PRBool  operator<(const nsString &aString) const;
PRBool  operator<(const char *aString) const;
PRBool  operator<(const PRUnichar* aString) const;

/**
 * These methods test if a given string is > than this
 * @param aString is the string to be compared to this
 * @return  TRUE or FALSE
 */
PRBool  operator>(const nsString &S) const;
PRBool  operator>(const char *aCString) const;
PRBool  operator>(const PRUnichar* aString) const;

/**
 * These methods test if a given string is <= than this
 * @param aString is the string to be compared to this
 * @return  TRUE or FALSE
 */
PRBool  operator<=(const nsString &S) const;
PRBool  operator<=(const char *aCString) const;
PRBool  operator<=(const PRUnichar* aString) const;

/**
 * These methods test if a given string is >= than this
 * @param aString is the string to be compared to this
 * @return  TRUE or FALSE
 */
PRBool  operator>=(const nsString &S) const;
PRBool  operator>=(const char* aCString) const;
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
PRBool  Equals(const nsString& aString) const;
PRBool  Equals(const char* aString,PRInt32 aLength=-1) const;   
PRBool  Equals(const nsIAtom *aAtom) const;


/**
 * Compares to unichar string ptrs to each other
 * @param s1 is a ptr to a unichar buffer
 * @param s2 is a ptr to a unichar buffer
 * @return  TRUE if they match
 */
PRBool  Equals(const PRUnichar* s1, const PRUnichar* s2) const;

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
PRBool  EqualsIgnoreCase(const nsString& aString) const;
PRBool  EqualsIgnoreCase(const char* aString,PRInt32 aLength=-1) const;
PRBool  EqualsIgnoreCase(const nsIAtom *aAtom) const;

/**
 * Compares to unichar string ptrs to each other without respect to case
 * @param s1 is a ptr to a unichar buffer
 * @param s2 is a ptr to a unichar buffer
 * @return  TRUE if they match
 */
PRBool  EqualsIgnoreCase(const PRUnichar* s1, const PRUnichar* s2) const;


static void   SelfTest();
virtual void          DebugDump(ostream& aStream) const;

  protected:

typedef PRUnichar chartype;

            chartype*       mStr;
            PRInt32         mLength;
            PRInt32         mCapacity;
#ifdef  RICKG_DEBUG
		static	PRBool					mSelfTested;
#endif
};

ostream& operator<<(ostream& os,nsString& aString);
extern NS_BASE int fputs(const nsString& aString, FILE* out);

//----------------------------------------------------------------------

/**
 * A version of nsString which is designed to be used as an automatic
 * variable.  It attempts to operate out of a fixed size internal
 * buffer until too much data is added; then a dynamic buffer is
 * allocated and grown as necessary.
 */
// XXX template this with a parameter for the size of the buffer?
class NS_BASE nsAutoString : public nsString {
public:
                nsAutoString();
                nsAutoString(const nsString& other);
                nsAutoString(const nsAutoString& other);
                nsAutoString(PRUnichar aChar);
                nsAutoString(const char* aCString);
                nsAutoString(const PRUnichar* us, PRInt32 uslen = -1);
  virtual       ~nsAutoString();

  nsAutoString& operator=(const nsString& aString) {nsString::operator=(aString); return *this;}
  nsAutoString& operator=(const nsAutoString& aString) {nsString::operator=(aString); return *this;}
  nsAutoString& operator=(const char* aCString) {nsString::operator=(aCString); return *this;}
  nsAutoString& operator=(char aChar) {nsString::operator=(aChar); return *this;}
  nsAutoString& operator=(const PRUnichar* aBuffer) {nsString::operator=(aBuffer); return *this;}
  nsAutoString& operator=(PRUnichar aChar) {nsString::operator=(aChar); return *this;}

  virtual void  SizeOf(nsISizeOfHandler* aHandler) const;

  static  void  SelfTest();

protected:
  virtual void EnsureCapacityFor(PRInt32 aNewLength);

  chartype mBuf[32];
};

ostream& operator<<(ostream& os,nsAutoString& aString);

#endif

