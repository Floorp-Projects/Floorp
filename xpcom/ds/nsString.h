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

#ifndef _nsCString_
#define _nsCString_

#include "nsString2.h"
#include "prtypes.h"
#include "nscore.h"
#include <stdio.h>
#include "nsStr.h"
#include "nsIAtom.h"

#ifdef NEW_STRING_APIS
#include "nsAWritableString.h"
#else
  #define NS_LITERAL_STRING(s)  (s)
  #define NS_LITERAL_CSTRING(s) (s)

  #define NS_DEF_DERIVED_STRING_OPERATOR_PLUS(_StringT, _CharT)
  #define NS_DEF_2_STRING_STRING_OPERATOR_PLUS(_String1T, _String2T, _CharT)

  inline
  char
  nsLiteralChar( char c )
    {
      return c;
    }
  
  inline
  PRUnichar
  nsLiteralPRUnichar( PRUnichar c )
    {
      return c;
    }
#endif


class NS_COM nsSubsumeCStr;

class NS_COM nsCString :
#ifdef NEW_STRING_APIS
  public nsAWritableCString,
#endif
  public nsStr {

#ifdef NEW_STRING_APIS
protected:
  virtual const void* Implementation() const { return "nsCString"; }
  virtual const char* GetReadableFragment( nsReadableFragment<char>&, nsFragmentRequest, PRUint32 ) const;
  virtual char* GetWritableFragment( nsWritableFragment<char>&, nsFragmentRequest, PRUint32 );
#endif

public: 
  /**
   * Default constructor. 
   */
  nsCString();

  /**
   * This is our copy constructor 
   * @param   reference to another nsCString
   */
  nsCString(const nsCString& aString);   

#ifdef NEW_STRING_APIS
  explicit nsCString( const nsAReadableCString& );

  nsCString(const char*);
  nsCString(const char*, PRInt32);
#else
  /**
   * This constructor accepts an isolatin string
   * @param   aCString is a ptr to a 1-byte cstr
   */
  nsCString(const char* aCString,PRInt32 aLength=-1);

  /**
   * This constructor accepts a unichar string
   * @param   aCString is a ptr to a 2-byte cstr
   */
//nsCString(const PRUnichar* aString,PRInt32 aLength=-1);

  /**
   * This is a copy constructor that accepts an nsStr
   * @param   reference to another nsCString
   */
//nsCString(const nsStr&);
#endif

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
  virtual PRUint32 Length() const { return mLength; }

  /**
   * Retrieve the size of this string
   * @return string length
   */
  virtual void SizeOf(nsISizeOfHandler* aHandler, PRUint32* aResult) const;


  /**
   * Call this method if you want to force a different string capacity
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
    Accessor methods...
   *********************************************************************/


   /**
     * Retrieve const ptr to internal buffer; DO NOT TRY TO FREE IT!
     */
  const char* GetBuffer(void) const;


#ifndef NEW_STRING_APIS
   /**
     * Get nth character.
     */
  PRUnichar operator[](PRUint32 anIndex) const;
  PRUnichar CharAt(PRUint32 anIndex) const;
  PRUnichar First(void) const;
  PRUnichar Last(void) const;
#endif

  PRBool SetCharAt(PRUnichar aChar,PRUint32 anIndex);

  /**********************************************************************
    String creation methods...
   *********************************************************************/

#ifndef NEW_STRING_APIS
  /**
   * Create a new string by appending given string to this
   * @param   aString -- 2nd string to be appended
   * @return  new string
   */
  nsSubsumeCStr operator+(const nsCString& aString);

  /**
   * create a new string by adding this to the given char*.
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
  void ToLowerCase(nsCString& aString) const;

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
  void ToUpperCase(nsCString& aString) const;


  /**
   *  This method is used to remove all occurances of the
   *  characters found in aSet from this string.
   *  
   *  @param  aSet -- characters to be cut from this
   *  @return *this 
   */
  void StripChars(const char* aSet);
  void StripChar(PRUnichar aChar,PRInt32 anOffset=0);
  void StripChar(char aChar,PRInt32 anOffset=0) { StripChar((PRUnichar) (unsigned char)aChar,anOffset); }

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
  void ReplaceChar(PRUnichar aOldChar,PRUnichar aNewChar);
  void ReplaceChar(const char* aSet,PRUnichar aNewChar);

  void ReplaceSubstring(const nsCString& aTarget,const nsCString& aNewValue);
  void ReplaceSubstring(const char* aTarget,const char* aNewValue);

  PRInt32 CountChar(PRUnichar aChar);

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
//#ifndef STANDALONE_STRING_TESTS
  operator char*() {return mStr;}
  operator const char*() const {return (const char*)mStr;}
//#endif

  /**
   * This method constructs a new nsCString that is a clone
   * of this string.
   * 
   */
  nsCString* ToNewString() const;

  /**
   * Creates an ISOLatin1 clone of this string
   * Note that calls to this method should be matched with calls to Recycle().
   * @return  ptr to new isolatin1 string
   */
  char* ToNewCString() const;

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
   *  NOTE:    IFF you pass -1 as aCount, then your buffer must be null terminated.
   *
   * @return  this
   */

  nsCString& operator=( const nsCString& aString )                  { Assign(aString); return *this; }
#ifdef NEW_STRING_APIS
  nsCString& operator=( const nsAReadableCString& aReadable )       { Assign(aReadable); return *this; }
  nsCString& operator=( const nsPromiseReadable<char>& aReadable )  { Assign(aReadable); return *this; }
  nsCString& operator=( const char* aPtr )                          { Assign(aPtr); return *this; }
  nsCString& operator=( char aChar )                                { Assign(aChar); return *this; }
#endif

  void AssignWithConversion(const PRUnichar*,PRInt32=-1);
  void AssignWithConversion( const nsString& aString );
#ifdef NEW_STRING_APIS
  void AssignWithConversion( const nsAReadableString& aString );
#endif
  void AssignWithConversion(PRUnichar);

#ifndef NEW_STRING_APIS
  nsCString& Assign(const char* aString,PRInt32 aCount=-1);
  nsCString& Assign(char aChar);

  nsCString& Assign(const nsStr& aString,PRInt32 aCount=-1);
//nsCString& Assign(const PRUnichar* aString,PRInt32 aCount=-1) { AssignWithConversion(aString, aCount); return *this; }
//nsCString& Assign(PRUnichar aChar)                            { AssignWithConversion(aChar); return *this; }

  /**
   * here come a bunch of assignment operators...
   * @param   aString: string to be added to this
   * @return  this
   */
//nsCString& operator=(PRUnichar aChar)           {AssignWithConversion(aChar); return *this;}
  nsCString& operator=(char aChar)                {return Assign(aChar);}
//nsCString& operator=(const PRUnichar* aString)  {AssignWithConversion(aString); return *this;}

//nsCString& operator=(const nsStr& aString)      {return Assign(aString);}
  nsCString& operator=(const char* aCString)      {return Assign(aCString);}

    // Yes, I know this makes assignment from a |nsSubsumeString| not do the special thing
    //  |nsSubsumeString| needs to go away
  #if defined(AIX) || defined(XP_OS2_VACPP)
  nsCString& operator=(const nsSubsumeCStr& aSubsumeString);  // AIX and VAC++ requires a const here
  #else
  nsCString& operator=(nsSubsumeCStr& aSubsumeString);
  #endif
#endif
 

  /*
   *  Appends n characters from given string to this,
   *  
   *  @param   aString is the source to be appended to this
   *  @param   aCount -- number of chars to copy; -1 tells us to compute the strlen for you
   *  NOTE:    IFF you pass -1 as aCount, then your buffer must be null terminated.
   *
   *  @return  number of chars copied
   */

  void AppendWithConversion(const nsString&, PRInt32=-1);
  void AppendWithConversion(PRUnichar aChar);
#ifdef NEW_STRING_APIS
  void AppendWithConversion( const nsAReadableString& aString );
#endif
  void AppendWithConversion(const PRUnichar*, PRInt32=-1);
  // Why no |AppendWithConversion(const PRUnichar*, PRInt32)|? --- now I know, because implicit construction hid the need for this routine
  void AppendInt(PRInt32 aInteger,PRInt32 aRadix=10); //radix=8,10 or 16
  void AppendFloat( double aFloat );

#ifdef NEW_STRING_APIS
  virtual void do_AppendFromReadable( const nsAReadableCString& );
#endif

#ifndef NEW_STRING_APIS
//nsCString& Append(const nsStr& aString,PRInt32 aCount=-1);
  nsCString& Append(const nsCString& aString,PRInt32 aCount);
  nsCString& Append(const nsCString& aString)           {return Append(aString,(PRInt32)aString.mLength);}
  nsCString& Append(const char* aString,PRInt32 aCount=-1);
  nsCString& Append(char aChar);

//nsCString& Append(PRUnichar aChar)                    {AppendWithConversion(aChar); return *this;}
//nsCString& Append(PRInt32 aInteger,PRInt32 aRadix=10) {AppendInt(aInteger,aRadix); return *this;}
//nsCString& Append(float aFloat)                       {AppendFloat(aFloat); return *this;}

  /**
   * Here's a bunch of methods that append varying types...
   * @param   various...
   * @return  this
   */
  nsCString& operator+=(const nsCString& aString) {return Append(aString,(PRInt32)aString.mLength);}
  nsCString& operator+=(const char* aCString)     {return Append(aCString);}

//nsCString& operator+=(const PRUnichar aChar)    {return Append(aChar);}
  nsCString& operator+=(const char aChar)         {return Append(aChar);}
//nsCString& operator+=(const int anInt)          {return Append(anInt,10);}
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

  void InsertWithConversion(PRUnichar aChar,PRUint32 anOffset);
  // Why no |InsertWithConversion(PRUnichar*)|?

#ifdef NEW_STRING_APIS
  virtual void do_InsertFromReadable( const nsAReadableCString&, PRUint32 );
#endif

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
  void Insert(const nsCString& aCopy,PRUint32 anOffset,PRInt32 aCount=-1);

  /**
   * Insert a given string into this string at
   * a specified offset.
   *
   * @param   aString* to be inserted into this string
   * @param   anOffset is insert pos in str 
   * @return  the number of chars inserted into this string
   */
  void Insert(const char* aChar,PRUint32 anOffset,PRInt32 aCount=-1);

  /**
   * Insert a single char into this string at
   * a specified offset.
   *
   * @param   character to be inserted into this string
   * @param   anOffset is insert pos in str 
   * @return  the number of chars inserted into this string
   */
//void Insert(PRUnichar aChar,PRUint32 anOffset)  {InsertWithConversion(aChar,anOffset);}
  void Insert(char aChar,PRUint32 anOffset);
#endif

  /*
   *  This method is used to cut characters in this string
   *  starting at anOffset, continuing for aCount chars.
   *  
   *  @param  anOffset -- start pos for cut operation
   *  @param  aCount -- number of chars to be cut
   *  @return *this
   */
#ifndef NEW_STRING_APIS
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
   * @param   aCount tells us how many iterations to make starting at the given offset
   * @return  offset in string, or -1 (kNotFound)
   */
  PRInt32 RFind(const char* aCString,PRBool aIgnoreCase=PR_FALSE,PRInt32 anOffset=-1,PRInt32 aCount=-1) const;
  PRInt32 RFind(const nsStr& aString,PRBool aIgnoreCase=PR_FALSE,PRInt32 anOffset=-1,PRInt32 aCount=-1) const;
  PRInt32 RFind(const PRUnichar* aString,PRBool aIgnoreCase=PR_FALSE,PRInt32 anOffset=-1,PRInt32 aCount=-1) const;


  /**
   *  Search for given char within this string
   *  
   *  @param   aString is substring to be sought in this
   *  @param   anOffset tells us where in this strig to start searching
   *  @param   aIgnoreCase selects case sensitivity
   *  @return  find pos in string, or -1 (kNotFound)
   */
  PRInt32 RFindChar(PRUnichar aChar,PRBool aIgnoreCase=PR_FALSE,PRInt32 anOffset=-1,PRInt32 aCount=-1) const;

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
   * @update  gess 7/27/98
   * @param   S is the string to be compared
   * @param   aIgnoreCase tells us how to treat case
   * @param   aCount tells us how many chars to compare
   * @return  -1,0,1
   */
  PRInt32 CompareWithConversion(const PRUnichar* aString,PRBool aIgnoreCase=PR_FALSE,PRInt32 aCount=-1) const;
  PRInt32 CompareWithConversion(const char* aString,PRBool aIgnoreCase=PR_FALSE,PRInt32 aCount=-1) const;

  PRBool  EqualsWithConversion(const nsString &aString,PRBool aIgnoreCase=PR_FALSE,PRInt32 aCount=-1) const;
  PRBool  EqualsWithConversion(const char* aString,PRBool aIgnoreCase=PR_FALSE,PRInt32 aCount=-1) const;
  PRBool  EqualsWithConversion(const PRUnichar* aString,PRBool aIgnoreCase=PR_FALSE,PRInt32 aCount=-1) const;

  PRBool  EqualsIgnoreCase(const char* aString,PRInt32 aCount=-1) const;
  PRBool  EqualsIgnoreCase(const PRUnichar* aString,PRInt32 aCount=-1) const;

#ifndef NEW_STRING_APIS
//virtual PRInt32 Compare(const PRUnichar* aString,PRBool aIgnoreCase=PR_FALSE,PRInt32 aCount=-1) const {
//  return CompareWithConversion(aString,aIgnoreCase,aCount);
//}
//virtual PRInt32 Compare(const char* aString,PRBool aIgnoreCase=PR_FALSE,PRInt32 aCount=-1) const {
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
   * @param  aCount  -- number of chars in given string you want to compare
   * @return TRUE if equal
   */
  PRBool  Equals(const nsString &aString,PRBool aIgnoreCase=PR_FALSE,PRInt32 aCount=-1) const {
    return EqualsWithConversion(aString,aIgnoreCase,aCount);
  }

  PRBool  Equals(const char* aString,PRBool aIgnoreCase=PR_FALSE,PRInt32 aCount=-1) const {
    return EqualsWithConversion(aString,aIgnoreCase,aCount);
  }

//PRBool  Equals(const PRUnichar* aString,PRBool aIgnoreCase=PR_FALSE,PRInt32 aCount=-1) const {
//  return EqualsWithConversion(aString,aIgnoreCase,aCount);
//}

  PRBool  Equals(const nsStr& aString,PRBool aIgnoreCase=PR_FALSE,PRInt32 aCount=-1) const;

  PRBool  EqualsIgnoreCase(const nsStr& aString) const;

  /**
   * These methods compare a given string type to this one
   * @param aString is the string to be compared to this
   * @return  TRUE or FALSE
   */
  PRBool  operator==(const nsStr &aString) const;
  PRBool  operator==(const char* aString) const;
//PRBool  operator==(const PRUnichar* aString) const;

  /**
   * These methods perform a !compare of a given string type to this 
   * @param aString is the string to be compared to this
   * @return  TRUE 
   */
  PRBool  operator!=(const nsStr &aString) const;
  PRBool  operator!=(const char* aString) const;
//PRBool  operator!=(const PRUnichar* aString) const;

  /**
   * These methods test if a given string is < than this
   * @param aString is the string to be compared to this
   * @return  TRUE or FALSE
   */
  PRBool  operator<(const nsStr &aString) const;
  PRBool  operator<(const char* aString) const;
//PRBool  operator<(const PRUnichar* aString) const;

  /**
   * These methods test if a given string is > than this
   * @param aString is the string to be compared to this
   * @return  TRUE or FALSE
   */
  PRBool  operator>(const nsStr &S) const;
  PRBool  operator>(const char* aString) const;
//PRBool  operator>(const PRUnichar* aString) const;

  /**
   * These methods test if a given string is <= than this
   * @param aString is the string to be compared to this
   * @return  TRUE or FALSE
   */
  PRBool  operator<=(const nsStr &S) const;
  PRBool  operator<=(const char* aString) const;
//PRBool  operator<=(const PRUnichar* aString) const;

  /**
   * These methods test if a given string is >= than this
   * @param aString is the string to be compared to this
   * @return  TRUE or FALSE
   */
  PRBool  operator>=(const nsStr &S) const;
  PRBool  operator>=(const char* aString) const;
//PRBool  operator>=(const PRUnichar* aString) const;
#endif // !defined(NEW_STRING_APIS)

  void    DebugDump(void) const;


  static  void        Recycle(nsCString* aString);
  static  nsCString*  CreateString(void);

private:
    // NOT TO BE IMPLEMENTED
    //  these signatures help clients not accidentally call the wrong thing helped by C++ automatic integral promotion
#ifdef NEW_STRING_APIS
  void operator=( PRUnichar );
#endif
  void AssignWithConversion( char );
  void AssignWithConversion( const char*, PRInt32=-1 );
  void AppendWithConversion( char );
  void InsertWithConversion( char, PRUint32 );
};

#ifdef NEW_STRING_APIS
inline
nsPromiseConcatenation<char>
operator+( const nsPromiseConcatenation<char>& lhs, const nsCString& rhs )
  {
    return nsPromiseConcatenation<char>(lhs, rhs);
  }
#endif

#ifdef NEW_STRING_APIS
NS_DEF_STRING_COMPARISON_OPERATORS(nsCString, char)
NS_DEF_DERIVED_STRING_OPERATOR_PLUS(nsCString, char);
#endif

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

    virtual ~nsCAutoString();

    nsCAutoString();
    nsCAutoString(const nsCString& );
    nsCAutoString(const nsAReadableCString& aString);
    nsCAutoString(const char* aString);
    nsCAutoString(const char* aString,PRInt32 aLength);
    nsCAutoString(const CBufDescriptor& aBuffer);

#ifndef NEW_STRING_APIS
//  nsCAutoString(const PRUnichar* aString,PRInt32 aLength=-1);
//  nsCAutoString(const nsStr& aString);
//  nsCAutoString(PRUnichar aChar);
#endif

#if defined(AIX) || defined(XP_OS2_VACPP)
    nsCAutoString(const nsSubsumeCStr& aSubsumeStr);  // AIX and VAC++ require a const
#else
    nsCAutoString(nsSubsumeCStr& aSubsumeStr);
#endif // AIX || XP_OS2_VACPP


    nsCAutoString& operator=( const nsCAutoString& aString )              { Assign(aString); return *this; }
#ifdef NEW_STRING_APIS
  private:
    void operator=( PRUnichar ); // NOT TO BE IMPLEMENTED
  public:
    nsCAutoString& operator=( const nsAReadableCString& aReadable )       { Assign(aReadable); return *this; }
    nsCAutoString& operator=( const nsPromiseReadable<char>& aReadable )  { Assign(aReadable); return *this; }
    nsCAutoString& operator=( const char* aPtr )                          { Assign(aPtr); return *this; }
    nsCAutoString& operator=( char aChar )                                { Assign(aChar); return *this; }
#else
    nsCAutoString& operator=(const nsCString& aString) {nsCString::Assign(aString); return *this;}
    nsCAutoString& operator=(const char* aCString) {nsCString::Assign(aCString); return *this;}
//  nsCAutoString& operator=(const PRUnichar* aString) {nsCString::Assign(aString); return *this;}
//  nsCAutoString& operator=(PRUnichar aChar) {nsCString::Assign(aChar); return *this;}
    nsCAutoString& operator=(char aChar) {nsCString::Assign(aChar); return *this;}
#endif

    /**
     * Retrieve the size of this string
     * @return string length
     */
    virtual void SizeOf(nsISizeOfHandler* aHandler, PRUint32* aResult) const;
    
    char mBuffer[kDefaultStringSize];
};

#if 0//def NEW_STRING_APIS
NS_DEF_STRING_COMPARISON_OPERATORS(nsCAutoString, char)
#endif

#ifdef NEW_STRING_APIS
NS_DEF_DERIVED_STRING_OPERATOR_PLUS(nsCAutoString, char)
#endif

/**
 * A helper class that converts a UCS2 string to UTF8
 */
class NS_COM NS_ConvertUCS2toUTF8
      : public nsCAutoString
    /*
      ...
    */
  {
    public:
      NS_ConvertUCS2toUTF8( const PRUnichar* aString )
        {
          Append( aString, ~PRUint32(0) /* MAXINT */);
        }

      NS_ConvertUCS2toUTF8( const PRUnichar* aString, PRUint32 aLength )
        {
          Append( aString, aLength );
        }

      NS_ConvertUCS2toUTF8( PRUnichar aChar )
        {
          Append( &aChar, 1 );
        }

#ifdef NEW_STRING_APIS
      NS_ConvertUCS2toUTF8( const nsAReadableString& aString );
#endif

      operator const char*() const
        {
          return GetBuffer();
        }

    protected:
      void Append( const PRUnichar* aString, PRUint32 aLength );

    private:
        // NOT TO BE IMPLEMENTED
      NS_ConvertUCS2toUTF8( char );
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

  nsSubsumeCStr& operator=( const nsSubsumeCStr& aString )              { Assign(aString); return *this; }
#ifdef NEW_STRING_APIS
  nsSubsumeCStr& operator=( const nsAReadableCString& aReadable )       { Assign(aReadable); return *this; }
  nsSubsumeCStr& operator=( const nsPromiseReadable<char>& aReadable )  { Assign(aReadable); return *this; }
  nsSubsumeCStr& operator=( const char* aPtr )                          { Assign(aPtr); return *this; }
  nsSubsumeCStr& operator=( char aChar )                                { Assign(aChar); return *this; }
private:
  void operator=( PRUnichar ); // NOT TO BE IMPLEMENTED
#endif
};


#endif


