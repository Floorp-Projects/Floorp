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

/*
 * nsString.h --- rickg's original strings of 1-byte chars, |nsCString|
 * and |nsCAutoString|; these classes will be replaced by the new
 * shared-buffer string (see bug #53065)
 *
 * This file will one day be _only_ a compatibility header for clients
 * using the names |ns[C]String| et al ... which we probably want to
 * support forever.
 */

#ifndef nsString_h__
#define nsString_h__

#ifndef nsString2_h__
#include "nsString2.h"
#endif

class NS_COM nsCString :
  public nsAFlatCString,
  public nsStr {

public:
  friend class nsString;
  friend NS_COM void ToUpperCase( nsCString& );
  friend NS_COM void ToLowerCase( nsCString& );

protected:
  virtual const nsBufferHandle<char>* GetFlatBufferHandle() const;
  virtual const char* GetReadableFragment( nsReadableFragment<char>&, nsFragmentRequest, PRUint32 ) const;
  virtual char* GetWritableFragment( nsWritableFragment<char>&, nsFragmentRequest, PRUint32 );

public:
  virtual const char* get() const { return mStr; }

  PRBool IsEmpty() const { return mLength == 0; }

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
   *  This method is used to remove all occurances of the
   *  characters found in aSet from this string.
   *  
   *  @param  aSet -- characters to be cut from this
   *  @return *this 
   */
  void StripChars(const char* aSet);
  void StripChar(char aChar,PRInt32 anOffset=0);

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
  void ReplaceChar(char aOldChar,char aNewChar);
  void ReplaceChar(const char* aSet,char aNewChar);

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
  void CompressWhitespace( PRBool aEliminateLeading=PR_TRUE,PRBool aEliminateTrailing=PR_TRUE);

  /**********************************************************************
    string conversion methods...
   *********************************************************************/

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
  void AppendWithConversion( const nsAString& aString );
  void AppendWithConversion(const PRUnichar*, PRInt32=-1);
  // Why no |AppendWithConversion(const PRUnichar*, PRInt32)|? --- now I know, because implicit construction hid the need for this routine
  void AppendInt(PRInt32 aInteger,PRInt32 aRadix=10); //radix=8,10 or 16
  void AppendFloat( double aFloat );


#if 0
  virtual void do_AppendFromReadable( const nsACString& );
  virtual void do_InsertFromReadable( const nsACString&, PRUint32 );
#endif


  // Takes ownership of aPtr, sets the current length to aLength if specified.
  void Adopt( char* aPtr, PRInt32 aLength = -1 );

  /*
    |Left|, |Mid|, and |Right| are annoying signatures that seem better almost
    any _other_ way than they are now.  Consider these alternatives
    
    aWritable = aReadable.Left(17);   // ...a member function that returns a |Substring|
    aWritable = Left(aReadable, 17);  // ...a global function that returns a |Substring|
    Left(aReadable, 17, aWritable);   // ...a global function that does the assignment
    
    as opposed to the current signature
    
    aReadable.Left(aWritable, 17);    // ...a member function that does the assignment
    
    or maybe just stamping them out in favor of |Substring|, they are just duplicate functionality
            
    aWritable = Substring(aReadable, 0, 17);
  */
            
  size_type  Left( self_type&, size_type ) const;
  size_type  Mid( self_type&, PRUint32, PRUint32 ) const;
  size_type  Right( self_type&, size_type ) const;

  
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
  PRInt32 Find(const nsCString& aString,PRBool aIgnoreCase=PR_FALSE,PRInt32 anOffset=0,PRInt32 aCount=-1) const;
  PRInt32 Find(const char* aString,PRBool aIgnoreCase=PR_FALSE,PRInt32 anOffset=0,PRInt32 aCount=-1) const;
  /**
   *  Search for given char within this string
   *  
   *  @param   aString is substring to be sought in this
   *  @param   anOffset tells us where in this strig to start searching
   *  @param   aIgnoreCase selects case sensitivity
   *  @param   aCount tells us how many iterations to make starting at the given offset
   *  @return  find pos in string, or -1 (kNotFound)
   */
  PRInt32 FindChar(PRUnichar aChar,PRInt32 anOffset=0,PRInt32 aCount=-1) const;

  /**
   * This method searches this string for the first character
   * found in the given charset
   * @param aString contains set of chars to be found
   * @param anOffset tells us where to start searching in this
   * @return -1 if not found, else the offset in this
   */
  PRInt32 FindCharInSet(const char* aString,PRInt32 anOffset=0) const;
  PRInt32 FindCharInSet(const nsCString& aString,PRInt32 anOffset=0) const;


  /**
   * This methods scans the string backwards, looking for the given string
   * @param   aString is substring to be sought in this
   * @param   aIgnoreCase tells us whether or not to do caseless compare
   * @param   aCount tells us how many iterations to make starting at the given offset
   * @return  offset in string, or -1 (kNotFound)
   */
  PRInt32 RFind(const char* aCString,PRBool aIgnoreCase=PR_FALSE,PRInt32 anOffset=-1,PRInt32 aCount=-1) const;
  PRInt32 RFind(const nsCString& aString,PRBool aIgnoreCase=PR_FALSE,PRInt32 anOffset=-1,PRInt32 aCount=-1) const;

  /**
   *  Search for given char within this string
   *  
   *  @param   aString is substring to be sought in this
   *  @param   anOffset tells us where in this strig to start searching
   *  @param   aIgnoreCase selects case sensitivity
   *  @return  find pos in string, or -1 (kNotFound)
   */
  PRInt32 RFindChar(PRUnichar aChar,PRInt32 anOffset=-1,PRInt32 aCount=-1) const;

  /**
   * This method searches this string for the last character
   * found in the given string
   * @param aString contains set of chars to be found
   * @param anOffset tells us where to start searching in this
   * @return -1 if not found, else the offset in this
   */
  PRInt32 RFindCharInSet(const char* aString,PRInt32 anOffset=-1) const;
  PRInt32 RFindCharInSet(const nsCString& aString,PRInt32 anOffset=-1) const;



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
  PRInt32 Compare(const char* aString,PRBool aIgnoreCase=PR_FALSE,PRInt32 aCount=-1) const;

  PRBool  EqualsWithConversion(const char* aString,PRBool aIgnoreCase=PR_FALSE,PRInt32 aCount=-1) const;

  PRBool  EqualsIgnoreCase(const char* aString,PRInt32 aCount=-1) const;

private:
    // NOT TO BE IMPLEMENTED
    //  these signatures help clients not accidentally call the wrong thing helped by C++ automatic integral promotion
  void operator=( PRUnichar );
  void AssignWithConversion( const char*, PRInt32=-1 );
  void InsertWithConversion( char, PRUint32 );
};

inline
nsCString::size_type
nsCString::Left( nsACString& aResult, size_type aLengthToCopy ) const
  {
    return Mid(aResult, 0, aLengthToCopy);
  }

inline
nsCString::size_type
nsCString::Right( self_type& aResult, size_type aLengthToCopy ) const
  {
    size_type myLength = Length();
    aLengthToCopy = NS_MIN(myLength, aLengthToCopy);
    return Mid(aResult, myLength-aLengthToCopy, aLengthToCopy);
  }

// NS_DEF_STRING_COMPARISON_OPERATORS(nsCString, char)
// NS_DEF_DERIVED_STRING_OPERATOR_PLUS(nsCString, char)


/**************************************************************
  Here comes the AutoString class which uses internal memory
  (typically found on the stack) for its default buffer.
  If the buffer needs to grow, it gets reallocated on the heap.
 **************************************************************/

class NS_COM nsCAutoString : public nsCString {
public: 

    virtual ~nsCAutoString() {}

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

    char mBuffer[kDefaultStringSize];
};

// NS_DEF_DERIVED_STRING_OPERATOR_PLUS(nsCAutoString, char)

/**
 * A helper class that converts a UTF-16 string to UTF-8
 */
class NS_COM NS_ConvertUTF16toUTF8
      : public nsCAutoString
    /*
      ...
    */
  {
    public:
      explicit NS_ConvertUTF16toUTF8( const PRUnichar* aString );
      NS_ConvertUTF16toUTF8( const PRUnichar* aString, PRUint32 aLength );
      explicit NS_ConvertUTF16toUTF8( const nsAString& aString );
      explicit NS_ConvertUTF16toUTF8( const nsASingleFragmentString& aString );

    protected:
      void Init( const PRUnichar* aString, PRUint32 aLength );

    private:
        // NOT TO BE IMPLEMENTED
      NS_ConvertUTF16toUTF8( char );
  };


/**
 * A helper class that converts a UTF-16 string to ASCII in a lossy manner
 */
class NS_COM NS_LossyConvertUTF16toASCII
      : public nsCAutoString
    /*
      ...
    */
  {
    public:
      explicit
      NS_LossyConvertUTF16toASCII( const PRUnichar* aString )
        {
          AppendWithConversion( aString, ~PRUint32(0) /* MAXINT */);
        }

      NS_LossyConvertUTF16toASCII( const PRUnichar* aString, PRUint32 aLength )
        {
          AppendWithConversion( aString, aLength );
        }

      explicit NS_LossyConvertUTF16toASCII( const nsAString& aString );

    private:
        // NOT TO BE IMPLEMENTED
      NS_LossyConvertUTF16toASCII( char );
  };

// Backward compatibility
typedef NS_ConvertUTF16toUTF8 NS_ConvertUCS2toUTF8;
typedef NS_LossyConvertUTF16toASCII NS_LossyConvertUCS2toASCII;

#endif /* !defined(nsString_h__) */
