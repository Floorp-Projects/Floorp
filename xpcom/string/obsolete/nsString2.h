/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: NPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.1 (the "License"); you may not use this file except in
 * compliance with the License. You may obtain a copy of the License at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is 
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Rick Gessner <rickg@netscape.com> (original author)
 *   Scott Collins <scc@mozilla.org>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or 
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the NPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the NPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

/* nsString2.h --- rickg's original strings of 2-byte chars, |nsString| and |nsAutoString|;
    these classes will be replaced by the new shared-buffer string (see bug #53065)
 */



#ifndef _nsString_
#define _nsString_

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

#include "prtypes.h"
#include "nscore.h"
#include <stdio.h>
#include "nsString.h"
#include "nsIAtom.h"
#include "nsStr.h"
#include "nsCRT.h"

#ifndef nsAFlatString_h___
#include "nsAFlatString.h"
#endif
#include "nsXPIDLString.h"

#ifdef STANDALONE_MI_STRING_TESTS
  class nsAFlatString { public: virtual ~nsAString() { } };
#endif

class nsISizeOfHandler;


class NS_COM nsString :
  public nsAFlatString,
  public nsStr {

protected:
  virtual const nsBufferHandle<PRUnichar>* GetFlatBufferHandle() const;
  virtual const PRUnichar* GetReadableFragment( nsReadableFragment<PRUnichar>&, nsFragmentRequest, PRUint32 ) const;
  virtual PRUnichar* GetWritableFragment( nsWritableFragment<PRUnichar>&, nsFragmentRequest, PRUint32 );

public:
  virtual const PRUnichar* get() const;

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

  explicit nsString(const nsAString&);

  explicit nsString(const PRUnichar*);
  nsString(const PRUnichar*, PRInt32);


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


  /**********************************************************************
    Getters/Setters...
   *********************************************************************/

   /**
     * Set nth character.
     */
  PRBool SetCharAt(PRUnichar aChar,PRUint32 anIndex);



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
  nsString& operator=( const nsAString& aReadable )                           { Assign(aReadable); return *this; }
//nsString& operator=( const nsPromiseReadable<PRUnichar>& aReadable )        { Assign(aReadable); return *this; }
  nsString& operator=( const PRUnichar* aPtr )                                { Assign(aPtr); return *this; }
  nsString& operator=( PRUnichar aChar )                                      { Assign(aChar); return *this; }

  void AssignWithConversion(char);
  void AssignWithConversion(const char*);
  void AssignWithConversion(const char*, PRInt32);


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


//void InsertWithConversion(char);
  void InsertWithConversion(const char*, PRUint32, PRInt32=-1);


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
  /* a hack to make sure things that used to compile continue to compile
     even on compilers that don't have proper |explicit| support */
  inline PRInt32
  CompareWithConversion(const nsXPIDLString& aString, PRBool aIgnoreCase=PR_FALSE, PRInt32 aCount=-1) const
    {
      return CompareWithConversion(aString.get(), aIgnoreCase, aCount);
    }

  PRBool  EqualsWithConversion(const nsString &aString,PRBool aIgnoreCase=PR_FALSE,PRInt32 aCount=-1) const;
  PRBool  EqualsWithConversion(const char* aString,PRBool aIgnoreCase=PR_FALSE,PRInt32 aCount=-1) const;
  PRBool  EqualsWithConversion(const PRUnichar* aString,PRBool aIgnoreCase=PR_FALSE,PRInt32 aCount=-1) const;
  /* a hack to make sure things that used to compile continue to compile
     even on compilers that don't have proper |explicit| support */
  inline PRBool
  EqualsWithConversion(const nsXPIDLString &aString, PRBool aIgnoreCase=PR_FALSE, PRInt32 aCount=-1) const
    {
      return EqualsWithConversion(aString.get(), aIgnoreCase, aCount);
    }


  PRBool  EqualsAtom(/*FIX: const */nsIAtom* anAtom,PRBool aIgnoreCase) const;   

  PRBool  EqualsIgnoreCase(const nsString& aString) const;
  PRBool  EqualsIgnoreCase(const char* aString,PRInt32 aCount=-1) const;
  PRBool  EqualsIgnoreCase(/*FIX: const */nsIAtom *aAtom) const;


  /**
   *  Determine if given buffer is plain ascii
   *  
   *  @param   aBuffer -- if null, then we test *this, otherwise we test given buffer
   *  @return  TRUE if is all ascii chars or if strlen==0
   */
  PRBool IsASCII(const PRUnichar* aBuffer=0);

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

#ifdef DEBUG
  /**
   * Retrieve the size of this string
   * @return string length
   */
  virtual void SizeOf(nsISizeOfHandler* aHandler, PRUint32* aResult) const;
#endif

private:
    // NOT TO BE IMPLEMENTED
    //  these signatures help clients not accidentally call the wrong thing helped by C++ automatic integral promotion
  void operator=( char );
  void AssignWithConversion( PRUnichar );
  void AssignWithConversion( const PRUnichar*, PRInt32=-1 );
  void AppendWithConversion( PRUnichar );
  void AppendWithConversion( const PRUnichar*, PRInt32=-1 );
  void InsertWithConversion( const PRUnichar*, PRUint32, PRInt32=-1 );
};

// NS_DEF_STRING_COMPARISON_OPERATORS(nsString, PRUnichar)
// NS_DEF_DERIVED_STRING_OPERATOR_PLUS(nsString, PRUnichar)

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
    explicit nsAutoString(const nsAString& aString);
    explicit nsAutoString(const nsString& aString);
    explicit nsAutoString(const PRUnichar* aString);
    nsAutoString(const PRUnichar* aString,PRInt32 aLength);
    explicit nsAutoString(PRUnichar aChar);
    explicit nsAutoString(const CBufDescriptor& aBuffer);    

    nsAutoString& operator=( const nsAutoString& aString )                          { Assign(aString); return *this; }
  private:
    void operator=( char ); // NOT TO BE IMPLEMENTED
  public:
    nsAutoString& operator=( const nsAString& aReadable )                           { Assign(aReadable); return *this; }
//  nsAutoString& operator=( const nsPromiseReadable<PRUnichar>& aReadable )        { Assign(aReadable); return *this; }
    nsAutoString& operator=( const PRUnichar* aPtr )                                { Assign(aPtr); return *this; }
    nsAutoString& operator=( PRUnichar aChar )                                      { Assign(aChar); return *this; }

#ifdef DEBUG
    /**
     * Retrieve the size of this string
     * @return string length
     */
    virtual void SizeOf(nsISizeOfHandler* aHandler, PRUint32* aResult) const;
#endif
    
    char mBuffer[kDefaultStringSize<<eTwoByte];
};

// NS_DEF_DERIVED_STRING_OPERATOR_PLUS(nsAutoString, PRUnichar)

class NS_COM NS_ConvertASCIItoUCS2
      : public nsAutoString
    /*
      ...
    */
  {
    public:
      explicit NS_ConvertASCIItoUCS2( const char* );
      NS_ConvertASCIItoUCS2( const char*, PRUint32 );

#if 0
      operator const nsDependentString() const
        {
          return nsDependentString(mUStr, mLength);
        }
#endif

    private:
        // NOT TO BE IMPLEMENTED
      NS_ConvertASCIItoUCS2( PRUnichar );
  };

class NS_COM NS_ConvertUTF8toUCS2
      : public nsAutoString
  {
    public:
      explicit
      NS_ConvertUTF8toUCS2( const char* aCString )
        {
          Init( aCString, ~PRUint32(0) /* MAXINT */ );
        }

      NS_ConvertUTF8toUCS2( const char* aCString, PRUint32 aLength )
        {
          Init( aCString, aLength );
        }

    protected:
      void Init( const char* aCString, PRUint32 aLength );

    private:
      NS_ConvertUTF8toUCS2( PRUnichar );
  };

#endif


