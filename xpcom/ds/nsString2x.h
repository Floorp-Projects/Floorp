/*****************************************************************************************************************
 *  
 * This template provides the basic implementation for nsString.
 *
 * Contributor(s):
 *   Rick Gessner <rickg@netscape.com>
 * 
 * History:
 *
 *  02.29.2000: Original files (rickg)
 *  03.02.2000: Expand the API's to be a bit more realistic (rickg): 
 *                 1. Flesh out the interface to be compatible with old library 
 *                 2. Enable both nsString-based interfaces and nsAReadableString interfaces 
 *                 3. Build enough infrastructure to test nsString(s) and nsImmutableString constructions.
 *  03.03.2000: Finished mapping original nsString functionality to template form (rickg)
 *  
 *****************************************************************************************************************/


#ifndef _NS_STRING_
#define _NS_STRING_

#include "nsStringValue.h"
#include "nsBufferManager.h"


class nsCString;

class nsString: public nsAReadableString {
public:

  //******************************************
  // Ctor's
  //******************************************
  
  nsString();
  
    //copy constructor
  nsString(const nsString& aString,PRInt32 aLength=-1);

    //call this version for nsCString
  nsString(const nsCString& aString,PRInt32 aLength=-1);

    //call this version for prunichar*'s....
  nsString(const PRUnichar* aString,PRInt32 aLength=-1) ;

    //call this version for a single char of type char...
  nsString(const char aChar);

    //call this version for a single char of type prunichar...
  nsString(const PRUnichar aChar);

    //call this version for char's....
  nsString(const char* aString,PRInt32 aLength=-1) ;

    //call this version for all other ABT versions of readable strings
  nsString(const nsAReadableString &aString) ;

    //call this version for stack-based string buffers...
  nsString(const nsStackBuffer<PRUnichar> &aBuffer);

  virtual ~nsString();

  
  //******************************************
  // Assigment operators
  //******************************************
  
    
  nsString& operator=(const nsString& aString);

  nsString& operator=(const nsCString& aString);

  nsString& operator=(const PRUnichar* aString);

  nsString& operator=(const char* aString);

  nsString& operator=(const char aChar);

  nsString& operator=(const PRUnichar aChar);


  //******************************************
  // Here are the accessor methods...
  //******************************************

  virtual nsresult SetLength(PRUint32 aLength) {
    Truncate(aLength);
    return NS_OK;
  }

  virtual nsresult SetCapacity(PRUint32 aCapacity);

  PRUnichar* GetUnicode() { return mStringValue.mBuffer;}

  PRUnichar& operator[](PRUint32 aOffset);  

//  operator const char* const() {return mStringValue.mBuffer;}

  PRUint32  Length() const {return mStringValue.mLength;}  
  
  size_t    GetCharSize() const {return sizeof(char);}
  PRBool    IsUnicode() const {return PRBool(sizeof(PRUnichar)==sizeof(char));}
  
  PRBool    IsEmpty(void) const {return PRBool(mStringValue.mLength==0);}

  PRUnichar CharAt(PRUint32 anIndex) const {
    return (anIndex<mStringValue.mLength) ? mStringValue.mBuffer[anIndex] : 0;
  }

  PRBool SetCharAt(PRUnichar aChar,PRUint32 anIndex);

  PRUnichar First(void) const {
    return CharAt(0);
  } 
  
  PRUnichar Last(void) const {
    return CharAt(mStringValue.mLength-1);
  }


    //these aren't the real deal, but serve as a placeholder for us to implement iterators.
  virtual nsAReadableStringIterator* First();
  virtual nsAReadableStringIterator* Last();


  //******************************************
  // Here are the Assignment methods, 
  // and other mutators...
  //******************************************

  virtual nsresult Truncate(PRUint32 anOffset=0);


  /*
   *  This method assign from a stringimpl 
   *  string at aString[anOffset].
   *  
   *  @update  rickg 03.01.2000
   *  @param  aString -- source String to be inserted into this
   *  @param  aLength -- number of chars to be copied from aCopy
   *  @param  aSrcOffset -- insertion position within this str
   *  @return this
   */
  virtual nsresult Assign(const nsString& aString,PRInt32 aLength=-1,PRUint32 aSrcOffset=0);

  virtual nsresult Assign(const PRUnichar* aString,PRInt32 aLength=-1,PRUint32 aSrcOffset=0);

  virtual nsresult Assign(const char* aString,PRInt32 aLength=-1,PRUint32 aSrcOffset=0);

    //assign from a stringValueImpl of a compatible type
  virtual nsresult Assign(const nsCString& aString,PRInt32 aLength=-1,PRUint32 aSrcOffset=0);

  virtual nsresult Assign(const nsAReadableString &aString,PRInt32 aLength=-1,PRUint32 aSrcOffset=0);

  virtual nsresult Assign(char aChar); 

  virtual nsresult Assign(PRUnichar aChar);

  /**
   * Functionally equivalent to assign or operator=, and deprecated!
   * 
   */
  nsresult SetString(const char* aString,PRInt32 aLength=-1) {return Assign(aString,aLength);}
  nsresult SetString(const PRUnichar* aString,PRInt32 aLength=-1) {return Assign(aString,aLength);}
  nsresult SetString(const nsString &aString,PRInt32 aLength=-1) {return Assign(aString,aLength);}

    //***************************************
    //  Here come the append methods...
    //***************************************

  /*
   *  This method appends a stringimpl starting at aString[aSrcOffset]
   *  
   *  @update  rickg 03.01.2000
   *  @param  aString -- source String to be inserted into this
   *  @param  aLength -- number of chars to be copied from aCopy
   *  @param  aSrcOffset -- insertion position within this str
   *  @return this
   */
  virtual nsresult Append(const nsString &aString,PRInt32 aLength=-1,PRUint32 aSrcOffset=0);

  virtual nsresult Append(const nsCString &aString,PRInt32 aLength=-1,PRUint32 aSrcOffset=0);

  virtual nsresult Append(const PRUnichar* aString,PRInt32 aLength=-1,PRUint32 aSrcOffset=0);

  virtual nsresult Append(const char* aString,PRInt32 aLength=-1,PRUint32 aSrcOffset=0);

  virtual nsresult Append(const PRUnichar aChar) ;

  virtual nsresult Append(const char aChar);

  virtual nsresult Append(const nsAReadableString &aString,PRInt32 aLength=-1,PRUint32 aSrcOffset=0);

  virtual nsresult Append(PRInt32 anInteger,PRInt32 aRadix=10);

  virtual nsresult Append(float aFloat);


    //***************************************
    //  Here come a few deletion methods...
    //***************************************

  nsresult Cut(PRUint32 anOffset,PRInt32 aCount);

  nsresult Trim(const char* aTrimSet,PRBool aEliminateLeading=PR_TRUE,PRBool aEliminateTrailing=PR_TRUE,PRBool aIgnoreQuotes=PR_FALSE);

  /**
   *  This method compresses multiple chars in a row into a single instance
   *  You can control whether chars are yanked from ends too.
   *  
   * @update  rickg 03.01.2000
   *  @param   aEliminateLeading controls stripping of leading ws
   *  @param   aEliminateTrailing controls stripping of trailing ws
   *  @return  this
   */
  nsresult CompressSet(const char* aSet, PRUnichar aChar,PRBool aEliminateLeading=PR_TRUE,PRBool aEliminateTrailing=PR_TRUE);

  /**
   *  This method strips whitespace from string.
   *  You can control whether whitespace is yanked from
   *  start and end of string as well.
   *  
   *  @update  rickg 03.01.2000
   *  @param   aEliminateLeading controls stripping of leading ws
   *  @param   aEliminateTrailing controls stripping of trailing ws
   *  @return  this
   */
  nsresult CompressWhitespace( PRBool aEliminateLeading=PR_TRUE,PRBool aEliminateTrailing=PR_TRUE);


    //***************************************
    //  Here come a wad of insert methods...
    //***************************************

  /*
   *  This method inserts n chars from given string into this
   *  string at str[anOffset].
   *  
   *  @update  rickg 03.01.2000
   *  @param  aString -- source String to be inserted into this
   *  @param  anOffset -- insertion position within this str
   *  @param  aCount -- number of chars to be copied from aCopy
   *  @return this
   */
  nsresult Insert(const nsString& aString,PRUint32 anOffset=0,PRInt32 aCount=-1);

  nsresult Insert(const char* aString,PRUint32 anOffset=0,PRInt32 aLength=-1);

  nsresult Insert(const PRUnichar* aString,PRUint32 anOffset=0,PRInt32 aLength=-1);

  nsresult Insert(char aChar,PRUint32 anOffset=0);

  
    //*******************************************
    //  Here come inplace replacement  methods...
    //*******************************************

  /**
   *  swaps occurence of 1 char for another
   *  
   *  @return  this
   */
  nsresult ReplaceChar(char anOldChar,char aNewChar);

  nsresult ReplaceChar(const char* aSet,char aNewChar);

  nsresult ReplaceSubstring( const nsString& aTarget,const nsString& aNewValue);

  nsresult ReplaceSubstring(const char* aTarget,const char* aNewValue);


  //*******************************************
    //  Here come the stripchar methods...
    //*******************************************

  /**
   *  This method is used to remove all occurances of the
   *  given character from this string.
   *  
   *  @update  rickg 03.01.2000
   *  @param  aChar -- char to be stripped
   *  @param  anOffset -- where in this string to start stripping chars
   *  @return *this 
   */
  nsresult StripChar(char aChar,PRUint32 anOffset=0);

  nsresult StripChar(PRInt32 anInt,PRUint32 anOffset=0);

  nsresult StripChars(const char* aSet);

  nsresult StripWhitespace();

    //**************************************************
    //  Here are some methods that extract substrings...
    //**************************************************

  nsresult Left(nsString& aCopy,PRInt32 aCount) const;

  nsresult Mid(nsString& aCopy,PRUint32 anOffset,PRInt32 aCount) const;

  nsresult Right(nsString& aCopy,PRInt32 aCount) const;


    //*******************************************
    //  Here come the operator+=() methods...
    //*******************************************

  nsString& operator+=(const nsString& aString);

  nsString& operator+=(const char* aString);
  
  nsString& operator+=(const PRUnichar* aString);
  
  nsString& operator+=(const char aChar);

  nsString& operator+=(const PRUnichar aChar);
  
  nsString& operator+=(const int anInt);


  /***********************************
    Comparison methods...
   ***********************************/

  PRBool operator==(const nsString& aString) const {return Equals(aString);}      
  PRBool operator==(const nsCString& aString) const {return Equals(aString);}      
  PRBool operator==(const char* aString) const {return Equals(aString);}
  PRBool operator==(const PRUnichar* aString) const {return Equals(aString);}

  PRBool operator!=(const nsString& aString) const {return PRBool(Compare(aString)!=0);}
  PRBool operator!=(const nsCString& aString) const {return PRBool(Compare(aString)!=0);}
  PRBool operator!=(const char* aString) const {return PRBool(Compare(aString)!=0);}
  PRBool operator!=(const PRUnichar* aString) const {return PRBool(Compare(aString)!=0);}

  PRBool operator<(const nsString& aString) const {return PRBool(Compare(aString)<0);}
  PRBool operator<(const nsCString& aString) const {return PRBool(Compare(aString)<0);}
  PRBool operator<(const char* aString) const {return PRBool(Compare(aString)<0);}
  PRBool operator<(const PRUnichar* aString) const {return PRBool(Compare(aString)<0);}

  PRBool operator>(const nsString& aString) const {return PRBool(Compare(aString)>0);}
  PRBool operator>(const nsCString& aString) const {return PRBool(Compare(aString)>0);}
  PRBool operator>(const char* aString) const {return PRBool(Compare(aString)>0);}
  PRBool operator>(const PRUnichar* aString) const {return PRBool(Compare(aString)>0);}

  PRBool operator<=(const nsString& aString) const {return PRBool(Compare(aString)<=0);}
  PRBool operator<=(const nsCString& aString) const {return PRBool(Compare(aString)<=0);}
  PRBool operator<=(const char* aString) const {return PRBool(Compare(aString)<=0);}
  PRBool operator<=(const PRUnichar* aString) const {return PRBool(Compare(aString)<=0);}

  PRBool operator>=(const nsString& aString) const {return PRBool(Compare(aString)>=0);}
  PRBool operator>=(const nsCString& aString) const {return PRBool(Compare(aString)>=0);}
  PRBool operator>=(const char* aString) const {return PRBool(Compare(aString)>=0);}
  PRBool operator>=(const PRUnichar* aString) const {return PRBool(Compare(aString)>=0);}


  PRInt32  Compare(const nsString &aString,PRBool aIgnoreCase=PR_FALSE,PRInt32 aCount=-1) const;

  PRInt32  Compare(const nsCString &aString,PRBool aIgnoreCase=PR_FALSE,PRInt32 aCount=-1) const;

  PRInt32  Compare(const char* aString,PRBool aIgnoreCase=PR_FALSE,PRInt32 aCount=-1) const;
  
  PRInt32  Compare(const PRUnichar* aString,PRBool aIgnoreCase=PR_FALSE,PRInt32 aCount=-1) const ;

  PRBool  Equals(const nsString &aString,PRBool aIgnoreCase=PR_FALSE,PRInt32 aCount=-1) const;

  PRBool  Equals(const nsCString &aString,PRBool aIgnoreCase=PR_FALSE,PRInt32 aCount=-1) const;

  PRBool  Equals(const char* aString,PRBool aIgnoreCase=PR_FALSE,PRInt32 aCount=-1) const;

  PRBool  Equals(const PRUnichar* aString,PRBool aIgnoreCase=PR_FALSE,PRInt32 aCount=-1) const;

  PRBool  EqualsIgnoreCase(const nsString &aString) const {return Equals(aString,PR_TRUE);}
  PRBool  EqualsIgnoreCase(const nsCString &aString) const {return Equals(aString,PR_TRUE);}
  PRBool  EqualsIgnoreCase(const char* aString,PRInt32 aCount=-1) const {return Equals(aString,PR_TRUE);}
  PRBool  EqualsIgnoreCase(const PRUnichar* s1, const PRUnichar* s2) const;

  // PRBool  Equals(/*FIX: const */nsIAtom* anAtom,PRBool aIgnoreCase) const;   


  /***************************************
    These are string searching methods...
   ***************************************/

  /**
   *  search for given string within this string
   *  
   *  @update  rickg 03.01.2000
   *  @param   aString - substr to be found
   *  @param   aIgnoreCase tells us whether or not to do caseless compare
   *  @param   anOffset tells us where in this string to start searching
   *  @param   aRepCount tells us how many iterations to make starting at the given offset
   *  @return  offset in string, or -1 (kNotFound)
   */
  PRInt32 Find(const nsString& aTarget,PRBool aIgnoreCase=PR_FALSE,PRUint32 anOffset=0,PRInt32 aRepCount=-1) const;
  PRInt32 Find(const nsCString &aString,PRBool aIgnoreCase=PR_FALSE,PRUint32 anOffset=0,PRInt32 aRepCount=-1) const;
  PRInt32 Find(const char* aString,PRBool aIgnoreCase=PR_FALSE,PRUint32 anOffset=0,PRInt32 aRepCount=-1) const;
  PRInt32 Find(const PRUnichar* aString,PRBool aIgnoreCase=PR_FALSE,PRUint32 anOffset=0,PRInt32 aRepCount=-1) const;
  PRInt32 FindChar(char aChar,PRBool aIgnoreCase=PR_FALSE,PRInt32 anOffset=-1,PRInt32 aRepCount=-1) const ;
  
  PRInt32 FindCharInSet(const nsString& aString,PRUint32 anOffset=0) const;
  PRInt32 FindCharInSet(const char *aString,PRUint32 anOffset=0) const;
  PRInt32 FindCharInSet(const PRUnichar *aString,PRUint32 anOffset=0) const;

  PRInt32 RFind(const nsString& aString,PRBool aIgnoreCase=PR_FALSE,PRInt32 anOffset=-1,PRInt32 aRepCount=-1) const;
  PRInt32 RFind(const nsCString& aString,PRBool aIgnoreCase=PR_FALSE,PRInt32 anOffset=-1,PRInt32 aRepCount=-1) const;
  PRInt32 RFind(const char* aString,PRBool aIgnoreCase=PR_FALSE,PRInt32 anOffset=-1,PRInt32 aRepCount=-1) const;
  PRInt32 RFind(const PRUnichar* aString,PRBool aIgnoreCase=PR_FALSE,PRInt32 anOffset=-1,PRInt32 aRepCount=-1) const;
  PRInt32 RFindChar(char aChar,PRBool aIgnoreCase=PR_FALSE,PRInt32 anOffset=-1,PRInt32 aRepCount=-1) const;

  PRInt32 RFindCharInSet(const nsString& aString,PRInt32 anOffset=-1) const ;
  PRInt32 RFindCharInSet(const PRUnichar* aString,PRInt32 anOffset=-1) const;
  PRInt32 RFindCharInSet(const char* aString,PRInt32 anOffset=-1) const;

  /***************************************
    These convert to a different type
   ***************************************/

  static void  Recycle(nsString* aString);

  static nsString* CreateString(void);

  nsString* ToNewString() const;

  char* ToNewCString() const;

  char* ToNewUTF8String() const;

  PRUnichar* ToNewUnicode() const;

  char* ToCString(char* aBuf,PRUint32 aBufLength,PRUint32 anOffset=0) const;

  //******************************************
  // Utility methods
  //******************************************

  PRInt32 CountChar(PRUnichar aChar);
  
    //This will not work correctly for any unicode set other than ascii
  void DebugDump(void) const;

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
  PRInt32 ToInteger(PRInt32* anErrorCode,PRUint32 aRadix=kAutoDetect) const;

  void ToLowerCase();

  void ToLowerCase(nsString &aString) const;

  void ToUpperCase();

  void ToUpperCase(nsString &aString) const;


protected:
  
  nsresult Append(const nsStringValueImpl<char> &aString,PRInt32 aLength=-1,PRUint32 aSrcOffset=0);

  nsStringValueImpl<PRUnichar>  mStringValue;

  friend class nsCString;
};




/*****************************************************************
  Now we declare the nsAutoString class
 *****************************************************************/


class nsAutoString : public nsString {
public:
  
  nsAutoString() : nsString() {
    memset(mInternalBuffer,0,sizeof(mInternalBuffer));
    mStringValue.mCapacity=kDefaultStringSize;
    mStringValue.mBuffer=mInternalBuffer;
  }
  
    //call this version nsAutoString derivatives...
  nsAutoString(const nsAutoString& aString,PRInt32 aLength=-1) : nsString() {
    memset(mInternalBuffer,0,sizeof(mInternalBuffer));
    mStringValue.mCapacity=kDefaultStringSize;
    mStringValue.mBuffer=mInternalBuffer;
    Assign(aString,aLength);
  }

  //call this version for nsString,nsCString and the autostrings
  nsAutoString(const nsString& aString,PRInt32 aLength=-1) : nsString() {
    memset(mInternalBuffer,0,sizeof(mInternalBuffer));
    mStringValue.mCapacity=kDefaultStringSize;
    mStringValue.mBuffer=mInternalBuffer;
    Assign(aString,aLength);
  }

    //call this version with nsStringValueImpls (start of COW)
  nsAutoString(const nsCString& aString) : nsString(aString) {
  }

    //call this version for char*'s....
  nsAutoString(const char* aString,PRInt32 aLength=-1) : nsString() {
    memset(mInternalBuffer,0,sizeof(mInternalBuffer));

    nsStringValueImpl<char> theString(const_cast<char*>(aString),aLength);
    mStringValue.mCapacity=kDefaultStringSize;
    mStringValue.mBuffer=mInternalBuffer;
    SVAssign<PRUnichar,char>(mStringValue,theString,aLength,0);
  }

    //call this version for a single char of type char...
  nsAutoString(const char aChar) : nsString() {
    char theBuffer[]={aChar,0};

    memset(mInternalBuffer,0,sizeof(mInternalBuffer));
    mStringValue.mCapacity=kDefaultStringSize;
    mStringValue.mBuffer=mInternalBuffer;

    nsStringValueImpl<char> theString(theBuffer,1);
    Assign(theString,1,0);
  }


    //call this version for PRUnichar*'s....
  nsAutoString(const PRUnichar* aString,PRInt32 aLength=-1) : nsString() {
    memset(mInternalBuffer,0,sizeof(mInternalBuffer));
    mStringValue.mCapacity=kDefaultStringSize;
    mStringValue.mBuffer=mInternalBuffer;
    nsStringValueImpl<PRUnichar> theString(const_cast<PRUnichar*>(aString),aLength);
    SVAssign<PRUnichar,PRUnichar>(mStringValue,theString,theString.mLength,0);
  }


    //call this version for all other ABT versions of readable strings
  nsAutoString(const nsAReadableString &aString) : nsString() {
    memset(mInternalBuffer,0,sizeof(mInternalBuffer));
    mStringValue.mCapacity=kDefaultStringSize;
    mStringValue.mBuffer=mInternalBuffer;
    Assign(aString);
  }

  nsAutoString(const nsStackBuffer<PRUnichar> &aBuffer) : nsString() {

    memset(mInternalBuffer,0,sizeof(mInternalBuffer));

    mStringValue.mRefCount=2;
    mStringValue.mLength=aBuffer.mLength;
    mStringValue.mCapacity=aBuffer.mCapacity;
    mStringValue.mBuffer=aBuffer.mBuffer;
    
  }


  virtual ~nsAutoString() { }

  
  nsAutoString& operator=(const nsAutoString& aCopy) {
    if(aCopy.mStringValue.mBuffer!=mStringValue.mBuffer) {
      Assign(aCopy);
    }
    return *this;
  }
  
  nsAutoString& operator=(const nsString& aString) {
    Assign(aString);
    return *this;
  }

  nsAutoString& operator=(const PRUnichar* aString) {
    if(mStringValue.mBuffer!=aString) {
      nsStringValueImpl<PRUnichar> theStringValue(const_cast<PRUnichar*>(aString));
      Assign(aString);
    }
    return *this;
  } 

  nsAutoString& operator=(const char* aString) {
    nsStringValueImpl<char> theStringValue(const_cast<char*>(aString));
    SVAssign<PRUnichar,char>(mStringValue,theStringValue,theStringValue.mLength,0);
    return *this;
  }

  nsAutoString& operator=(const char aChar) {
    Assign(aChar);
    return *this;
  } 

protected:
  PRUnichar mInternalBuffer[kDefaultStringSize+1];
};

#endif
