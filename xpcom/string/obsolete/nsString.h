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

/* nsString.h --- rickg's original strings of 1-byte chars, |nsCString| and |nsCAutoString|;
    these classes will be replaced by the new shared-buffer string (see bug #53065)
 */


#ifndef _nsCString_
#define _nsCString_

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

#include "nsString2.h"
#include "prtypes.h"
#include "nscore.h"
#include <stdio.h>
#include "nsStr.h"
#include "nsIAtom.h"

#include "nsAString.h"
#include "nsXPIDLString.h"

/* this file will one day be _only_ a compatibility header for clients using the names
     |ns[C]String| et al ... which we probably want to support forever.
   In the mean time, people are used to getting the names |nsAReadable[C]String| and friends
     from here as well; so we must continue to include the other compatibility headers
     even though we don't use them ourself.
 */

#include "nsAWritableString.h"
  // for compatibility


class NS_COM nsCString :
  public nsAFlatCString,
  public nsStr {

protected:
  virtual const nsBufferHandle<char>* GetFlatBufferHandle() const;
  virtual const char* GetReadableFragment( nsReadableFragment<char>&, nsFragmentRequest, PRUint32 ) const;
  virtual char* GetWritableFragment( nsWritableFragment<char>&, nsFragmentRequest, PRUint32 );

public:
  virtual const char* get() const { return mStr; }

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

  explicit nsCString( const nsACString& );

  explicit nsCString(const char*);
  nsCString(const char*, PRInt32);

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

  /**********************************************************************
    Accessor methods...
   *********************************************************************/

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
   * Note that calls to this method should be matched with calls to
   * |nsMemory::Free|.
   * @return  ptr to new isolatin1 string
   */
  char* ToNewCString() const;

  /**
   * Creates a unicode clone of this string
   * Note that calls to this method should be matched with calls to
   * |nsMemory::Free|.
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
  nsCString& operator=( const nsACString& aReadable )               { Assign(aReadable); return *this; }
//nsCString& operator=( const nsPromiseReadable<char>& aReadable )  { Assign(aReadable); return *this; }
  nsCString& operator=( const char* aPtr )                          { Assign(aPtr); return *this; }
  nsCString& operator=( char aChar )                                { Assign(aChar); return *this; }

  void AssignWithConversion(const PRUnichar*,PRInt32=-1);
  void AssignWithConversion( const nsString& aString );
  void AssignWithConversion( const nsAString& aString );
  void AssignWithConversion(PRUnichar);

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
  void AppendWithConversion( const nsAString& aString );
  void AppendWithConversion(const PRUnichar*, PRInt32=-1);
  // Why no |AppendWithConversion(const PRUnichar*, PRInt32)|? --- now I know, because implicit construction hid the need for this routine
  void AppendInt(PRInt32 aInteger,PRInt32 aRadix=10); //radix=8,10 or 16
  void AppendFloat( double aFloat );


  void InsertWithConversion(PRUnichar aChar,PRUint32 anOffset);
  // Why no |InsertWithConversion(PRUnichar*)|?

#if 0
  virtual void do_AppendFromReadable( const nsACString& );
  virtual void do_InsertFromReadable( const nsACString&, PRUint32 );
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
  /* a hack to make sure things that used to compile continue to compile
     even on compilers that don't have proper |explicit| support */
  inline PRBool
  EqualsWithConversion(const nsXPIDLString &aString, PRBool aIgnoreCase=PR_FALSE, PRInt32 aCount=-1) const
    {
      return EqualsWithConversion(aString.get(), aIgnoreCase, aCount);
    }

  PRBool  EqualsIgnoreCase(const char* aString,PRInt32 aCount=-1) const;
  PRBool  EqualsIgnoreCase(const PRUnichar* aString,PRInt32 aCount=-1) const;


  void    DebugDump(void) const;

private:
    // NOT TO BE IMPLEMENTED
    //  these signatures help clients not accidentally call the wrong thing helped by C++ automatic integral promotion
  void operator=( PRUnichar );
  void AssignWithConversion( char );
  void AssignWithConversion( const char*, PRInt32=-1 );
  void AppendWithConversion( char );
  void InsertWithConversion( char, PRUint32 );
};

// NS_DEF_STRING_COMPARISON_OPERATORS(nsCString, char)
// NS_DEF_DERIVED_STRING_OPERATOR_PLUS(nsCString, char)

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
    explicit nsCAutoString(const nsCString& );
    explicit nsCAutoString(const nsACString& aString);
    explicit nsCAutoString(const char* aString);
    nsCAutoString(const char* aString,PRInt32 aLength);
    explicit nsCAutoString(const CBufDescriptor& aBuffer);

    nsCAutoString& operator=( const nsCAutoString& aString )              { Assign(aString); return *this; }
  private:
    void operator=( PRUnichar ); // NOT TO BE IMPLEMENTED
  public:
    nsCAutoString& operator=( const nsACString& aReadable )       { Assign(aReadable); return *this; }
//  nsCAutoString& operator=( const nsPromiseReadable<char>& aReadable )  { Assign(aReadable); return *this; }
    nsCAutoString& operator=( const char* aPtr )                          { Assign(aPtr); return *this; }
    nsCAutoString& operator=( char aChar )                                { Assign(aChar); return *this; }

    /**
     * Retrieve the size of this string
     * @return string length
     */
    virtual void SizeOf(nsISizeOfHandler* aHandler, PRUint32* aResult) const;
    
    char mBuffer[kDefaultStringSize];
};

// NS_DEF_DERIVED_STRING_OPERATOR_PLUS(nsCAutoString, char)

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
      explicit
      NS_ConvertUCS2toUTF8( const PRUnichar* aString )
        {
          Append( aString, ~PRUint32(0) /* MAXINT */);
        }

      NS_ConvertUCS2toUTF8( const PRUnichar* aString, PRUint32 aLength )
        {
          Append( aString, aLength );
        }

      explicit NS_ConvertUCS2toUTF8( const nsAString& aString );

    protected:
      void Append( const PRUnichar* aString, PRUint32 aLength );

    private:
        // NOT TO BE IMPLEMENTED
      NS_ConvertUCS2toUTF8( char );
      operator const char*() const;  // use |get()|
  };

#endif


