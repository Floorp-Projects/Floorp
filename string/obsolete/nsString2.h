/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/*
 * The contents of this file are subject to the Netscape Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Original Author:
 *   Rick Gessner <rickg@netscape.com>
 *
 * Contributor(s): 
 *   Scott Collins <scc@mozilla.org>
 */



/***********************************************************************
  MODULE NOTES:

  See nsStr.h for a more general description of string classes.

  This version of the nsString class offers many improvements over the
  original version:
    1. Wide and narrow chars
    2. Allocators
    3. Much smarter autostrings
    4. Subsumable strings
 ***********************************************************************/

#ifndef NEW_STRING_APIS
  #define NEW_STRING_APIS 1
#endif //NEW_STRING_APIS

  // Need this to enable comparison profiling for a while
#ifdef OLD_STRING_APIS
  #undef NEW_STRING_APIS
#endif


#ifndef _nsString_
#define _nsString_

#include "prtypes.h"
#include "nscore.h"
#include <stdio.h>
#include "nsString.h"
#include "nsIAtom.h"
#include "nsStr.h"
#include "nsCRT.h"

#ifdef NEW_STRING_APIS
#include "nsAWritableString.h"
#endif

#ifdef STANDALONE_MI_STRING_TESTS
  class nsAReadableString { public: virtual ~nsAReadableString() { } };
  class nsAWritableString : public nsAReadableString { public: virtual ~nsAWritableString() { } };
#endif

class nsISizeOfHandler;


#define nsString2     nsString
#define nsAutoString2 nsAutoString

class NS_COM nsSubsumeStr;

class NS_COM nsString :
#if defined(NEW_STRING_APIS) || defined(STANDALONE_MI_STRING_TESTS)
  public nsAWritableString,
#endif
  public nsStr {

#ifdef NEW_STRING_APIS
protected:
  virtual const void* Implementation() const { return "nsString"; }
  virtual const PRUnichar* GetReadableFragment( nsReadableFragment<PRUnichar>&, nsFragmentRequest, PRUint32 ) const;
  virtual PRUnichar* GetWritableFragment( nsWritableFragment<PRUnichar>&, nsFragmentRequest, PRUint32 );
#endif


public:
  /**
   * Default constructor. 
   */
  nsString();

  /**
   * This is our copy constructor 
   * @param   reference to another nsString
   */
  nsString(const nsString& aString);   

#ifdef NEW_STRING_APIS
  nsString(const nsAReadableString&);

  nsString(const PRUnichar*);
  nsString(const PRUnichar*, PRInt32);
#else
  /**
   * This constructor accepts a unichar string
   * @param   aCString is a ptr to a 2-byte cstr
   */
  nsString(const PRUnichar* aString,PRInt32 aCount=-1);

  /**
   * This constructor accepts an isolatin string
   * @param   aCString is a ptr to a 1-byte cstr
   */
//nsString(const char* aCString,PRInt32 aCount=-1);

  /**
   * This is a copy constructor that accepts an nsStr
   * @param   reference to another nsString
   */
//nsString(const nsStr&);
#endif


  /**
   * This constructor takes a subsumestr
   * @param   reference to subsumestr
   */
#if defined(AIX) || defined(XP_OS2_VACPP)
  nsString(const nsSubsumeStr& aSubsumeStr);   // AIX and VAC++ require a const here
#else
  nsString(nsSubsumeStr& aSubsumeStr);   
#endif

  /**
   * Destructor
   * 
   */
  virtual ~nsString();    

  /**
   * Retrieve the length of this string
   * @return string length
   */
  virtual PRUint32 Length() const { return mLength; }

  /**
   * Retrieve the size of this string
   * @return string length
   */
  virtual void SizeOf(nsISizeOfHandler* aHandler, PRUint32* aResult) const;


  /**
   * Call this method if you want to force a different string length
   * @update  gess7/30/98
   * @param   aLength -- contains new length for mStr
   * @return
   */
  void SetLength(PRUint32 aLength);

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
  void Truncate(PRUint32 anIndex=0) {
    NS_ASSERTION(anIndex<=mLength, "Can't use |Truncate()| to make a string longer.");
    if ( anIndex < mLength )
      SetLength(anIndex);
  }


  /**
   *  Determine whether or not the characters in this
   *  string are in store as 1 or 2 byte (unicode) strings.
   *  
   *  @return  TRUE if ordered.
   */
  PRBool IsUnicode(void) const {
    PRBool result=PRBool(mCharSize==eTwoByte);
    return result;
  }

#ifndef NEW_STRING_APIS
  /**
   *  Determine whether or not this string has a length of 0
   *  
   *  @return  TRUE if empty.
   */
  PRBool IsEmpty(void) const {
    return PRBool(0==mLength);
  }
#endif

  /**********************************************************************
    Getters/Setters...
   *********************************************************************/

   /**
     * Retrieve const ptr to internal buffer; DO NOT TRY TO FREE IT!
     */
  const char* GetBuffer(void) const;
  const PRUnichar* GetUnicode(void) const;


#ifndef NEW_STRING_APIS
   /**
     * Get nth character.
     */
  PRUnichar operator[](PRUint32 anIndex) const;
  PRUnichar CharAt(PRUint32 anIndex) const;
  PRUnichar First(void) const;
  PRUnichar Last(void) const;
#endif // !defined(NEW_STRING_APIS)
   /**
     * Set nth character.
     */
  PRBool SetCharAt(PRUnichar aChar,PRUint32 anIndex);


#ifndef NEW_STRING_APIS
  /**********************************************************************
    String concatenation methods...
   *********************************************************************/

  /**
   * Create a new string by appending given string to this
   * @param   aString -- 2nd string to be appended
   * @return  new subsumable string
   */
  nsSubsumeStr operator+(const nsStr& aString);
  nsSubsumeStr operator+(const nsString& aString);

  /**
   * create a new string by adding this to the given cstring
   * @param   aCString is a ptr to cstring to be added to this
   * @return  newly created string
   */
  nsSubsumeStr operator+(const char* aCString);

  /**
   * create a new string by adding this to the given prunichar*.
   * @param   aString is a ptr to UC-string to be added to this
   * @return  newly created string
   */
  nsSubsumeStr operator+(const PRUnichar* aString);

  /**
   * create a new string by adding this to the given char.
   * @param   aChar is a char to be added to this
   * @return  newly created string
   */
  nsSubsumeStr operator+(char aChar);

  /**
   * create a new string by adding this to the given char.
   * @param   aChar is a unichar to be added to this
   * @return  newly created string
   */
  nsSubsumeStr operator+(PRUnichar aChar);
#endif

  /**********************************************************************
    Lexomorphic transforms...
   *********************************************************************/


  /**
   * Converts chars in this to lowercase
   * @update  gess 7/27/98
   */
  void ToLowerCase();


  /**
   * Converts chars in this to lowercase, and
   * stores them in aOut
   * @update  gess 7/27/98
   * @param   aOut is a string to contain result
   */
  void ToLowerCase(nsString& aString) const;

  /**
   * Converts chars in this to uppercase
   * @update  gess 7/27/98
   */
  void ToUpperCase();

  /**
   * Converts chars in this to lowercase, and
   * stores them in a given output string
   * @update  gess 7/27/98
   * @param   aOut is a string to contain result
   */
  void ToUpperCase(nsString& aString) const;


  /**
   *  This method is used to remove all occurances of the
   *  characters found in aSet from this string.
   *  
   *  @param  aSet -- characters to be cut from this
   *  @return *this 
   */
  void StripChars( const char* aSet );
  void StripChar( PRUnichar aChar, PRInt32 anOffset=0 );
  void StripChar( char aChar, PRInt32 anOffset=0 )       { StripChar((PRUnichar) (unsigned char)aChar,anOffset); }
  void StripChar( PRInt32 anInt, PRInt32 anOffset=0 )    { StripChar((PRUnichar)anInt,anOffset); }

  /**
   *  This method strips whitespace throughout the string
   *  
   *  @return  this
   */
  void StripWhitespace();

  /**
   *  swaps occurence of 1 string for another
   *  
   *  @return  this
   */
  void ReplaceChar( PRUnichar anOldChar, PRUnichar aNewChar );
  void ReplaceChar( const char* aSet, PRUnichar aNewChar );

  void ReplaceSubstring( const nsString& aTarget, const nsString& aNewValue );
  void ReplaceSubstring( const PRUnichar* aTarget, const PRUnichar* aNewValue );

  PRInt32 CountChar( PRUnichar aChar );

  /**
   *  This method trims characters found in aTrimSet from
   *  either end of the underlying string.
   *  
   *  @param   aTrimSet -- contains chars to be trimmed from
   *           both ends
   *  @param   aEliminateLeading
   *  @param   aEliminateTrailing
   *  @param   aIgnoreQuotes
   *  @return  this
   */
  void Trim(const char* aSet,PRBool aEliminateLeading=PR_TRUE,PRBool aEliminateTrailing=PR_TRUE,PRBool aIgnoreQuotes=PR_FALSE);

  /**
   *  This method strips whitespace from string.
   *  You can control whether whitespace is yanked from
   *  start and end of string as well.
   *  
   *  @param   aEliminateLeading controls stripping of leading ws
   *  @param   aEliminateTrailing controls stripping of trailing ws
   *  @return  this
   */
  void CompressSet(const char* aSet, PRUnichar aChar,PRBool aEliminateLeading=PR_TRUE,PRBool aEliminateTrailing=PR_TRUE);

  /**
   *  This method strips whitespace from string.
   *  You can control whether whitespace is yanked from
   *  start and end of string as well.
   *  
   *  @param   aEliminateLeading controls stripping of leading ws
   *  @param   aEliminateTrailing controls stripping of trailing ws
   *  @return  this
   */
  void CompressWhitespace( PRBool aEliminateLeading=PR_TRUE,PRBool aEliminateTrailing=PR_TRUE);

  /**********************************************************************
    string conversion methods...
   *********************************************************************/

  /**
   * This method constructs a new nsString is a clone of this string.
   * 
   */
  nsString* ToNewString() const;

  /**
   * Creates an ISOLatin1 clone of this string
   * Note that calls to this method should be matched with calls to Recycle().
   * @return  ptr to new isolatin1 string
   */
  char* ToNewCString() const;

  /**
   * Creates an UTF8 clone of this string
   * Note that calls to this method should be matched with calls to Recycle().
   * @return  ptr to new null-terminated UTF8 string
   */
  char* ToNewUTF8String() const;

  /**
   * Creates a unicode clone of this string
   * Note that calls to this method should be matched with calls to Recycle().
   * @return  ptr to new unicode string
   */
  PRUnichar* ToNewUnicode() const;

  /**
   * Copies data from internal buffer onto given char* buffer
   * NOTE: This only copies as many chars as will fit in given buffer (clips)
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
   * Perform string to int conversion.
   * @param   aErrorCode will contain error if one occurs
   * @param   aRadix tells us which radix to assume; kAutoDetect tells us to determine the radix for you.
   * @return  int rep of string value, and possible (out) error code
   */
  PRInt32   ToInteger(PRInt32* aErrorCode,PRUint32 aRadix=kRadix10) const;


  /**********************************************************************
    String manipulation methods...                
   *********************************************************************/

  /**
   * assign given string to this string
   * @param   aStr: buffer to be assigned to this 
   * @param   aCount is the length of the given str (or -1) if you want me to determine its length
   *  NOTE:   IFF you pass -1 as aCount, then your buffer must be null terminated.

   * @return  this
   */

  nsString& operator=( const nsString& aString )                              { Assign(aString); return *this; }
#ifdef NEW_STRING_APIS
  nsString& operator=( const nsAReadableString& aReadable )                   { Assign(aReadable); return *this; }
  nsString& operator=( const nsPromiseReadable<PRUnichar>& aReadable )        { Assign(aReadable); return *this; }
  nsString& operator=( const PRUnichar* aPtr )                                { Assign(aPtr); return *this; }
  nsString& operator=( PRUnichar aChar )                                      { Assign(aChar); return *this; }
#endif

  void AssignWithConversion(char);
#ifdef NEW_STRING_APIS
  void AssignWithConversion(const char*);
  void AssignWithConversion(const char*, PRInt32);
#else
  void AssignWithConversion(const char*, PRInt32=-1);

  nsString& Assign(const PRUnichar* aString,PRInt32 aCount=-1);
  nsString& Assign(PRUnichar aChar);

  nsString& Assign(const nsStr& aString,PRInt32 aCount=-1);
//nsString& Assign(const char* aString,PRInt32 aCount=-1)       { AssignWithConversion(aString, aCount); return *this; }
//nsString& Assign(char aChar)                                  { AssignWithConversion(aChar); return *this; }

  /**
   * here come a bunch of assignment operators...
   * @param   aString: string to be added to this
   * @return  this
   */
  nsString& operator=(PRUnichar aChar)          {Assign(aChar); return *this;}
//nsString& operator=(char aChar)               {AssignWithConversion(aChar); return *this;}
//nsString& operator=(const char* aCString)     {AssignWithConversion(aCString); return *this;}

//nsString& operator=(const nsStr& aString)     {return Assign(aString);}
  nsString& operator=(const PRUnichar* aString) {return Assign(aString);}

    // Yes, I know this makes assignment from a |nsSubsumeString| not do the special thing
    //  |nsSubsumeString| needs to go away
  #if defined(AIX) || defined(XP_OS2_VACPP)
  nsString& operator=(const nsSubsumeStr& aSubsumeString);  // AIX and VAC++ requires a const here
  #else
  nsString& operator=(nsSubsumeStr& aSubsumeString);
  #endif
#endif


  /*
   *  Appends n characters from given string to this,
   *  This version computes the length of your given string
   *  
   *  @param   aString is the source to be appended to this
   *  @return  number of chars copied
   */

  void AppendInt(PRInt32, PRInt32=10); //radix=8,10 or 16
  void AppendFloat(double);
  void AppendWithConversion(const char*, PRInt32=-1);
  void AppendWithConversion(char);

  virtual void do_AppendFromElement( PRUnichar );

#ifndef NEW_STRING_APIS
  /*
   *  Appends n characters from given string to this,
   *  
   *  @param   aString is the source to be appended to this
   *  @param   aCount -- number of chars to copy; -1 tells us to compute the strlen for you
   *  NOTE:    IFF you pass -1 as aCount, then your buffer must be null terminated.
   *  @return  number of chars copied
   */
//nsString& Append(const nsStr& aString,PRInt32 aCount);
//nsString& Append(const nsStr& aString)                    {return Append(aString,(PRInt32)aString.mLength);}
  nsString& Append(const nsString& aString,PRInt32 aCount);
  nsString& Append(const nsString& aString)                 {return Append(aString,(PRInt32)aString.mLength);}
 
  nsString& Append(const PRUnichar* aString,PRInt32 aCount=-1);
  nsString& Append(PRUnichar aChar);

//nsString& Append(const char* aString,PRInt32 aCount=-1)   { AppendWithConversion(aString,aCount); return *this; }
//nsString& Append(char aChar)                              { AppendWithConversion(aChar); return *this; }
//nsString& Append(PRInt32 aInteger,PRInt32 aRadix=10)      { AppendInt(aInteger,aRadix); return *this; }
//nsString& Append(float aFloat)                            { AppendFloat(aFloat); return *this; }

  /**
   * Here's a bunch of methods that append varying types...
   * @param   various...
   * @return  this
   */
//nsString& operator+=(const char* aCString)        {return Append(aCString);}
//nsString& operator+=(const char aChar)            {return Append((PRUnichar) (unsigned char)aChar);}
  nsString& operator+=(const PRUnichar aChar)       {return Append(aChar);}
//nsString& operator+=(const int anInt)             {return Append(anInt,10);}

//nsString& operator+=(const nsStr& aString)        {return Append(aString,(PRInt32)aString.mLength);}
  nsString& operator+=(const nsString& aString)     {return Append(aString,(PRInt32)aString.mLength);}
  nsString& operator+=(const PRUnichar* aUCString)  {return Append(aUCString);}
#endif
             
  /*
   *  Copies n characters from this string to given string,
   *  starting at the leftmost offset.
   *  
   *  
   *  @param   aCopy -- Receiving string
   *  @param   aCount -- number of chars to copy
   *  @return  number of chars copied
   */
  PRUint32 Left(nsString& aCopy,PRInt32 aCount) const;

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
  PRUint32 Mid(nsString& aCopy,PRUint32 anOffset,PRInt32 aCount) const;

  /*
   *  Copies n characters from this string to given string,
   *  starting at rightmost char.
   *  
   *  
   *  @param  aCopy -- Receiving string
   *  @param  aCount -- number of chars to copy
   *  @return number of chars copied
   */
  PRUint32 Right(nsString& aCopy,PRInt32 aCount) const;


//void InsertWithConversion(char);
  void InsertWithConversion(const char*, PRUint32, PRInt32=-1);
#ifndef NEW_STRING_APIS
  /*
   *  This method inserts n chars from given string into this
   *  string at str[anOffset].
   *  
   *  @param  aCopy -- String to be inserted into this
   *  @param  anOffset -- insertion position within this str
   *  @param  aCount -- number of chars to be copied from aCopy
   *  NOTE:    IFF you pass -1 as aCount, then your buffer must be null terminated.
   *
   *  @return number of chars inserted into this.
   */
  void Insert(const nsString& aCopy,PRUint32 anOffset,PRInt32 aCount=-1);

  /**
   * Insert a given string into this string at
   * a specified offset.
   *
   * @param   aString* to be inserted into this string
   * @param   anOffset is insert pos in str 
   * @return  the number of chars inserted into this string
   */
//nsString& Insert(const char* aChar,PRUint32 anOffset,PRInt32 aCount=-1) {InsertWithConversion(aChar, anOffset, aCount); return *this;}
  void Insert(const PRUnichar* aChar,PRUint32 anOffset,PRInt32 aCount=-1);

  /**
   * Insert a single char into this string at
   * a specified offset.
   *
   * @param   character to be inserted into this string
   * @param   anOffset is insert pos in str 
   * @return  the number of chars inserted into this string
   */
//nsString& Insert(char aChar,PRUint32 anOffset)        {InsertWithConversion(aChar,anOffset); return *this;}
  void Insert(PRUnichar aChar,PRUint32 anOffset);
#endif


#ifndef NEW_STRING_APIS
  /*
   *  This method is used to cut characters in this string
   *  starting at anOffset, continuing for aCount chars.
   *  
   *  @param  anOffset -- start pos for cut operation
   *  @param  aCount -- number of chars to be cut
   *  @return *this
   */
  void Cut(PRUint32 anOffset,PRInt32 aCount);
#endif


  /**********************************************************************
    Searching methods...                
   *********************************************************************/
 
  /**
   *  Search for given substring within this string
   *  
   *  @param   aString is substring to be sought in this
   *  @param   aIgnoreCase selects case sensitivity
   *  @param   anOffset tells us where in this strig to start searching
   *  @param   aCount tells us how many iterations to make starting at the given offset
   *  @return  offset in string, or -1 (kNotFound)
   */
  PRInt32 Find(const nsString& aString,PRBool aIgnoreCase=PR_FALSE,PRInt32 anOffset=0,PRInt32 aCount=-1) const;
  PRInt32 Find(const nsStr& aString,PRBool aIgnoreCase=PR_FALSE,PRInt32 anOffset=0,PRInt32 aCount=-1) const;
  PRInt32 Find(const char* aString,PRBool aIgnoreCase=PR_FALSE,PRInt32 anOffset=0,PRInt32 aCount=-1) const;
  PRInt32 Find(const PRUnichar* aString,PRBool aIgnoreCase=PR_FALSE,PRInt32 anOffset=0,PRInt32 aCount=-1) const;


  /**
   *  Search for given char within this string
   *  
   *  @param   aString is substring to be sought in this
   *  @param   anOffset tells us where in this strig to start searching
   *  @param   aIgnoreCase selects case sensitivity
   *  @param   aCount tells us how many iterations to make starting at the given offset
   *  @return  find pos in string, or -1 (kNotFound)
   */
  //PRInt32 Find(PRUnichar aChar,PRInt32 offset=-1,PRBool aIgnoreCase=PR_FALSE) const;
  PRInt32 FindChar(PRUnichar aChar,PRBool aIgnoreCase=PR_FALSE,PRInt32 anOffset=0,PRInt32 aCount=-1) const;

  /**
   * This method searches this string for the first character
   * found in the given charset
   * @param aString contains set of chars to be found
   * @param anOffset tells us where to start searching in this
   * @return -1 if not found, else the offset in this
   */
  PRInt32 FindCharInSet(const char* aString,PRInt32 anOffset=0) const;
  PRInt32 FindCharInSet(const PRUnichar* aString,PRInt32 anOffset=0) const;
  PRInt32 FindCharInSet(const nsStr& aString,PRInt32 anOffset=0) const;


  /**
   * This methods scans the string backwards, looking for the given string
   * @param   aString is substring to be sought in this
   * @param   aIgnoreCase tells us whether or not to do caseless compare
   * @param   anOffset tells us where in this strig to start searching (counting from left)
   * @param   aCount tells us how many iterations to make starting at the given offset
   * @return  offset in string, or -1 (kNotFound)
   */
  PRInt32 RFind(const char* aCString,PRBool aIgnoreCase=PR_FALSE,PRInt32 anOffset=-1,PRInt32 aCount=-1) const;
  PRInt32 RFind(const nsString& aString,PRBool aIgnoreCase=PR_FALSE,PRInt32 anOffset=-1,PRInt32 aCount=-1) const;
  PRInt32 RFind(const nsStr& aString,PRBool aIgnoreCase=PR_FALSE,PRInt32 anOffset=-1,PRInt32 aCount=-1) const;
  PRInt32 RFind(const PRUnichar* aString,PRBool aIgnoreCase=PR_FALSE,PRInt32 anOffset=-1,PRInt32 aCount=-1) const;


  /**
   *  Search for given char within this string
   *  
   *  @param   aString is substring to be sought in this
   *  @param   anOffset tells us where in this strig to start searching (counting from left)
   *  @param   aIgnoreCase selects case sensitivity
   *  @param   aCount tells us how many iterations to make starting at the given offset
   *  @return  find pos in string, or -1 (kNotFound)
   */
  //PRInt32 RFind(PRUnichar aChar,PRInt32 offset=-1,PRBool aIgnoreCase=PR_FALSE) const;
  PRInt32 RFindChar(PRUnichar aChar,PRBool aIgnoreCase=PR_FALSE,PRInt32 anOffset=-1,PRInt32 aCount=-1) const;

  /**
   * This method searches this string for the last character
   * found in the given string
   * @param aString contains set of chars to be found
   *  @param   anOffset tells us where in this strig to start searching (counting from left)
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
   * @update  gess 7/27/98
   * @param   S is the string to be compared
   * @param   aIgnoreCase tells us how to treat case
   * @param   aCount tells us how many chars to compare
   * @return  -1,0,1
   */

  PRInt32 CompareWithConversion(const char* aString,PRBool aIgnoreCase=PR_FALSE,PRInt32 aCount=-1) const;
  PRInt32 CompareWithConversion(const nsString& aString,PRBool aIgnoreCase=PR_FALSE,PRInt32 aCount=-1) const;
  PRInt32 CompareWithConversion(const PRUnichar* aString,PRBool aIgnoreCase=PR_FALSE,PRInt32 aCount=-1) const;

  PRBool  EqualsWithConversion(const nsString &aString,PRBool aIgnoreCase=PR_FALSE,PRInt32 aCount=-1) const;
  PRBool  EqualsWithConversion(const char* aString,PRBool aIgnoreCase=PR_FALSE,PRInt32 aCount=-1) const;
  PRBool  EqualsWithConversion(const PRUnichar* aString,PRBool aIgnoreCase=PR_FALSE,PRInt32 aCount=-1) const;
  PRBool  EqualsAtom(/*FIX: const */nsIAtom* anAtom,PRBool aIgnoreCase) const;   

  PRBool  EqualsIgnoreCase(const nsString& aString) const;
  PRBool  EqualsIgnoreCase(const char* aString,PRInt32 aCount=-1) const;
  PRBool  EqualsIgnoreCase(/*FIX: const */nsIAtom *aAtom) const;

#ifndef NEW_STRING_APIS
//virtual PRInt32 Compare(const char* aString,PRBool aIgnoreCase=PR_FALSE,PRInt32 aCount=-1) const {
//  return CompareWithConversion(aString,aIgnoreCase,aCount);
//}

//virtual PRInt32 Compare(const nsString& aString,PRBool aIgnoreCase=PR_FALSE,PRInt32 aCount=-1) const {
//  return CompareWithConversion(aString,aIgnoreCase,aCount);
//}

//virtual PRInt32 Compare(const PRUnichar* aString,PRBool aIgnoreCase=PR_FALSE,PRInt32 aCount=-1) const {
//  return CompareWithConversion(aString,aIgnoreCase,aCount);
//}

  virtual PRInt32 Compare(const nsStr &aString,PRBool aIgnoreCase=PR_FALSE,PRInt32 aCount=-1) const;


  /**
   * Compare this to given string; note that we compare full strings here.
   * The optional length argument just lets us know how long the given string is.
   * If you provide a length, it is compared to length of this string as an
   * optimization.
   * 
   * @param  aString -- the string to compare to this
   * @param  aCount  -- number of chars to be compared.
   * @return TRUE if equal
   */
  PRBool  Equals(const nsString &aString,PRBool aIgnoreCase=PR_FALSE,PRInt32 aCount=-1) const {
    return EqualsWithConversion(aString,aIgnoreCase,aCount);
  }

//PRBool  Equals(const char* aString,PRBool aIgnoreCase=PR_FALSE,PRInt32 aCount=-1) const {
//  return EqualsWithConversion(aString,aIgnoreCase,aCount);
//}

  PRBool  Equals(const PRUnichar* aString,PRBool aIgnoreCase=PR_FALSE,PRInt32 aCount=-1) const {
    return EqualsWithConversion(aString,aIgnoreCase,aCount);
  }

//PRBool  Equals(/*FIX: const */nsIAtom* anAtom,PRBool aIgnoreCase) const { return EqualsAtom(anAtom, aIgnoreCase); }
  PRBool  Equals(const PRUnichar* s1, const PRUnichar* s2,PRBool aIgnoreCase=PR_FALSE) const;
  PRBool  Equals(const nsStr& aString,PRBool aIgnoreCase=PR_FALSE,PRInt32 aCount=-1) const;

  PRBool  EqualsIgnoreCase(const PRUnichar* s1, const PRUnichar* s2) const;

  /**
   * These methods compare a given string type to this one
   * @param aString is the string to be compared to this
   * @return  TRUE or FALSE
   */
  PRBool  operator==(const nsString &aString) const;
//PRBool  operator==(const nsStr &aString) const;
//PRBool  operator==(const char *aString) const;
  PRBool  operator==(const PRUnichar* aString) const;

  /**
   * These methods perform a !compare of a given string type to this 
   * @param aString is the string to be compared to this
   * @return  TRUE 
   */
  PRBool  operator!=(const nsString &aString) const;
//PRBool  operator!=(const nsStr &aString) const;
//PRBool  operator!=(const char* aString) const;
  PRBool  operator!=(const PRUnichar* aString) const;

  /**
   * These methods test if a given string is < than this
   * @param aString is the string to be compared to this
   * @return  TRUE or FALSE
   */
  PRBool  operator<(const nsString &aString) const;
//PRBool  operator<(const nsStr &aString) const;
//PRBool  operator<(const char* aString) const;
  PRBool  operator<(const PRUnichar* aString) const;

  /**
   * These methods test if a given string is > than this
   * @param aString is the string to be compared to this
   * @return  TRUE or FALSE
   */
  PRBool  operator>(const nsString &aString) const;
//PRBool  operator>(const nsStr &S) const;
//PRBool  operator>(const char* aString) const;
  PRBool  operator>(const PRUnichar* aString) const;

  /**
   * These methods test if a given string is <= than this
   * @param aString is the string to be compared to this
   * @return  TRUE or FALSE
   */
  PRBool  operator<=(const nsString &aString) const;
//PRBool  operator<=(const nsStr &S) const;
//PRBool  operator<=(const char* aString) const;
  PRBool  operator<=(const PRUnichar* aString) const;

  /**
   * These methods test if a given string is >= than this
   * @param aString is the string to be compared to this
   * @return  TRUE or FALSE
   */
  PRBool  operator>=(const nsString &aString) const;
//PRBool  operator>=(const nsStr &S) const;
//PRBool  operator>=(const char* aString) const;
  PRBool  operator>=(const PRUnichar* aString) const;
#endif // !defined(NEW_STRING_APIS)

  /**
   *  Determine if given buffer is plain ascii
   *  
   *  @param   aBuffer -- if null, then we test *this, otherwise we test given buffer
   *  @return  TRUE if is all ascii chars or if strlen==0
   */
  PRBool IsASCII(const PRUnichar* aBuffer=0);

  void    DebugDump(void) const;

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

  static  void        Recycle(nsString* aString);
  static  nsString*  CreateString(void);

private:
    // NOT TO BE IMPLEMENTED
    //  these signatures help clients not accidentally call the wrong thing helped by C++ automatic integral promotion
#ifdef NEW_STRING_APIS
  void operator=( char );
#endif
  void AssignWithConversion( PRUnichar );
  void AssignWithConversion( const PRUnichar*, PRInt32=-1 );
  void AppendWithConversion( PRUnichar );
  void AppendWithConversion( const PRUnichar*, PRInt32=-1 );
  void InsertWithConversion( const PRUnichar*, PRUint32, PRInt32=-1 );
};

#ifdef NEW_STRING_APIS
NS_DEF_STRING_COMPARISON_OPERATORS(nsString, PRUnichar)
NS_DEF_DERIVED_STRING_OPERATOR_PLUS(nsString, PRUnichar)
#endif

#ifdef NEW_STRING_APIS
inline
nsPromiseConcatenation<PRUnichar>
operator+( const nsPromiseConcatenation<PRUnichar>& lhs, const nsString& rhs )
  {
    return nsPromiseConcatenation<PRUnichar>(lhs, rhs);
  }
#endif

extern NS_COM int fputs(const nsString& aString, FILE* out);
//ostream& operator<<(ostream& aStream,const nsString& aString);
//virtual void  DebugDump(ostream& aStream) const;

/**************************************************************
  Here comes the AutoString class which uses internal memory
  (typically found on the stack) for its default buffer.
  If the buffer needs to grow, it gets reallocated on the heap.
 **************************************************************/

class NS_COM nsAutoString : public nsString {
public: 

    virtual ~nsAutoString();
    nsAutoString();
    nsAutoString(const nsAutoString& aString);
    nsAutoString(const nsString& aString);
    nsAutoString(const PRUnichar* aString);
    nsAutoString(const PRUnichar* aString,PRInt32 aLength);
    nsAutoString(PRUnichar aChar);
    nsAutoString(const CBufDescriptor& aBuffer);    

#ifndef NEW_STRING_APIS
//  nsAutoString(const char* aCString,PRInt32 aLength=-1);
//  nsAutoString(const nsStr& aString);
#endif

#if defined(AIX) || defined(XP_OS2_VACPP)
    nsAutoString(const nsSubsumeStr& aSubsumeStr);  // AIX and VAC++ requires a const
#else
    nsAutoString(nsSubsumeStr& aSubsumeStr);
#endif // AIX || XP_OS2_VACPP


    nsAutoString& operator=( const nsAutoString& aString )                          { Assign(aString); return *this; }
#ifdef NEW_STRING_APIS
  private:
    void operator=( char ); // NOT TO BE IMPLEMENTED
  public:
    nsAutoString& operator=( const nsAReadableString& aReadable )                   { Assign(aReadable); return *this; }
    nsAutoString& operator=( const nsPromiseReadable<PRUnichar>& aReadable )        { Assign(aReadable); return *this; }
    nsAutoString& operator=( const PRUnichar* aPtr )                                { Assign(aPtr); return *this; }
    nsAutoString& operator=( PRUnichar aChar )                                      { Assign(aChar); return *this; }
#else
    nsAutoString& operator=(const nsStr& aString) {nsString::Assign(aString); return *this;}
//  nsAutoString& operator=(const char* aCString) {nsString::Assign(aCString); return *this;}
//  nsAutoString& operator=(char aChar) {nsString::Assign(aChar); return *this;}
    nsAutoString& operator=(const PRUnichar* aBuffer) {nsString::Assign(aBuffer); return *this;}
    nsAutoString& operator=(PRUnichar aChar) {nsString::Assign(aChar); return *this;}
#endif

    /**
     * Retrieve the size of this string
     * @return string length
     */
    virtual void SizeOf(nsISizeOfHandler* aHandler, PRUint32* aResult) const;
    
    char mBuffer[kDefaultStringSize<<eTwoByte];
};

#ifdef NEW_STRING_APIS
NS_DEF_DERIVED_STRING_OPERATOR_PLUS(nsAutoString, PRUnichar)
#endif

class NS_COM NS_ConvertASCIItoUCS2
      : public nsAutoString
    /*
      ...
    */
  {
    public:
      NS_ConvertASCIItoUCS2( const char* );
      NS_ConvertASCIItoUCS2( const char*, PRUint32 );
      NS_ConvertASCIItoUCS2( char );
#if 0
#ifdef NEW_STRING_APIS
      NS_ConvertASCIItoUCS2( const nsAReadableCString& );
#else
      class nsCString;
      NS_ConvertASCIItoUCS2( const nsCString& );
#endif
#endif

#ifdef NEW_STRING_APIS
      operator const PRUnichar*() const
        {
          return GetUnicode();
        }

      operator nsLiteralString() const
        {
          return nsLiteralString(mUStr, mLength);
        }
#endif

    private:
        // NOT TO BE IMPLEMENTED
      NS_ConvertASCIItoUCS2( PRUnichar );
  };

#define NS_ConvertToString NS_ConvertASCIItoUCS2

#if 0
inline
nsAutoString
NS_ConvertToString( const char* aCString )
  {
    nsAutoString result;
    result.AssignWithConversion(aCString);
    return result;
  }

inline
nsAutoString
NS_ConvertToString( const char* aCString, PRUint32 aLength )
  {
    nsAutoString result;
    result.AssignWithConversion(aCString, aLength);
    return result;
  }
#endif


class NS_COM NS_ConvertUTF8toUCS2
      : public nsAutoString
  {
    public:
      NS_ConvertUTF8toUCS2( const char* aCString )
        {
          Init( aCString, ~PRUint32(0) /* MAXINT */ );
        }

      NS_ConvertUTF8toUCS2( const char* aCString, PRUint32 aLength )
        {
          Init( aCString, aLength );
        }

      NS_ConvertUTF8toUCS2( char aChar )
        {
          Init( &aChar, 1 );
        }

      operator const PRUnichar*() const
        {
          return GetUnicode();
        }

    protected:
      void Init( const char* aCString, PRUint32 aLength );

    private:
      NS_ConvertUTF8toUCS2( PRUnichar );
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
class NS_COM nsSubsumeStr : public nsString {
public:
  nsSubsumeStr();
  nsSubsumeStr(nsStr& aString);
  nsSubsumeStr(PRUnichar* aString,PRBool assumeOwnership,PRInt32 aLength=-1);
  nsSubsumeStr(char* aString,PRBool assumeOwnership,PRInt32 aLength=-1);
  int Subsume(PRUnichar* aString,PRBool assumeOwnership,PRInt32 aLength=-1);

  nsSubsumeStr& operator=( const nsSubsumeStr& aReadable )                        { Assign(aReadable); return *this; }
#ifdef NEW_STRING_APIS
  nsSubsumeStr& operator=( const nsAReadableString& aReadable )                   { Assign(aReadable); return *this; }
  nsSubsumeStr& operator=( const nsPromiseReadable<PRUnichar>& aReadable )        { Assign(aReadable); return *this; }
  nsSubsumeStr& operator=( const PRUnichar* aPtr )                                { Assign(aPtr); return *this; }
  nsSubsumeStr& operator=( PRUnichar aChar )                                      { Assign(aChar); return *this; }
private:
  void operator=( char ); // NOT TO BE IMPLEMENTED
#endif
};


#endif


