
#include "nsString2x.h"
#include "nsStringx.h"
#include "nsBufferManager.h"
#include <stdio.h>  
#include "nsAutoStringImpl.h"

  //******************************************
  // Ctor's
  //******************************************

nsCString::nsCString() : mStringValue() {
}
  
  //call this to copy construct nsCString
nsCString::nsCString(const nsCString& aString,PRInt32 aLength) : mStringValue() {
  Assign(aString,aLength,0);
}

  //call this to construct from nsString
nsCString::nsCString(const nsString& aString,PRInt32 aLength) : mStringValue() {
  Assign(aString,aLength,0);
}

  //call this version for char*'s....
nsCString::nsCString(const char* aString,PRInt32 aLength) : mStringValue() {
  Assign(aString,aLength,0);
}

  //call this version for PRUnichar*'s....
nsCString::nsCString(const PRUnichar* aString,PRInt32 aLength) : mStringValue() {
  nsStringValueImpl<PRUnichar> theString(const_cast<PRUnichar*>(aString),aLength);
  SVAssign<char,PRUnichar>(mStringValue,theString,theString.mLength,0);
}

  //call this version for a single char of type char...
nsCString::nsCString(const char aChar) : mStringValue() {
  Append(aChar);
}

  //call this version for all other ABT versions of readable strings
nsCString::nsCString(const nsAReadableString &aString) : mStringValue() {
  Assign(aString);
}

  //call this version for stack-based string buffers...
nsCString::nsCString(const nsStackBuffer<char> &aBuffer) : mStringValue(aBuffer) {
}

nsCString::~nsCString() { }


  //******************************************
  // Assigment operators
  //******************************************

nsCString& nsCString::operator=(const nsCString& aString) {
  if(aString.mStringValue.mBuffer!=mStringValue.mBuffer) {
    Assign(aString);
  }
  return *this;
}

nsCString& nsCString::operator=(const nsString &aString) {
  Assign(aString);
  return *this;
}

nsCString& nsCString::operator=(const char *aString) {
  if(mStringValue.mBuffer!=aString) {
    nsStringValueImpl<char> theStringValue(const_cast<char*>(aString));
    SVAssign(mStringValue,theStringValue,theStringValue.mLength,0);
  }
  return *this;
} 

nsCString& nsCString::operator=(const PRUnichar *aString) {
  nsStringValueImpl<PRUnichar> theStringValue(const_cast<PRUnichar*>(aString));
  Assign(aString);
  return *this;
}

nsCString& nsCString::operator=(const char aChar) {
  Assign(aChar);
  return *this;
} 


  //******************************************
  // Here are the accessor methods...
  //******************************************


nsresult nsCString::SetCapacity(PRUint32 aCapacity) {
  if(aCapacity>mStringValue.mCapacity) {
    SVGrowCapacity<char>(mStringValue,aCapacity);
    SVAddNullTerminator<char>(mStringValue);
  }
  return NS_OK;
}

char& nsCString::operator[](PRUint32 aOffset)  {
  static char gSharedChar=0;
  return (mStringValue.mBuffer) ? mStringValue.mBuffer[aOffset] : gSharedChar;
}

//  operator const char* const() {return mStringValue.mBuffer;}


PRBool nsCString::SetCharAt(char aChar,PRUint32 anIndex) {
  PRBool result=PR_FALSE;
  if(anIndex<mStringValue.mLength){
    mStringValue.mBuffer[anIndex]=(char)aChar;
    result=PR_TRUE;
  }
  return result;
}

  //these aren't the real deal, but serve as a placeholder for us to implement iterators.
nsAReadableStringIterator* nsCString::First() {return new nsAReadableStringIterator(); }
nsAReadableStringIterator* nsCString::Last() {return new nsAReadableStringIterator(); }


  //******************************************
  // Here are the Assignment methods, 
  // and other mutators...
  //******************************************

nsresult nsCString::Truncate(PRUint32 anOffset) {
  SVTruncate<char>(mStringValue,anOffset);
  return NS_OK;
}

/*
 *  This method assign from nsCString
 *  
 *  @update  rickg 03.01.2000
 *  @param  aString -- source String to be inserted into this
 *  @param  aLength -- number of chars to be copied from aCopy
 *  @param  aSrcOffset -- insertion position within this str
 *  @return this
 */
nsresult nsCString::Assign(const nsCString& aString,PRInt32 aLength,PRUint32 aSrcOffset) {
  nsresult result=NS_OK;
  if(mStringValue.mBuffer!=aString.mStringValue.mBuffer){
    Truncate();
    result=Append(aString,aLength,aSrcOffset);
  }
  return result;
}


/*
 *  This method assign from PRUnichar*
 *  
 *  @update  rickg 03.01.2000
 *  @param  aString -- source String to be inserted into this
 *  @param  aLength -- number of chars to be copied from aCopy
 *  @param  aSrcOffset -- insertion position within this str
 *  @return this
 */
#if 0
nsresult nsCString::Assign(const PRUnichar* aString,PRInt32 aLength,PRUint32 aSrcOffset) {
  Truncate();
  return Append(aString,aLength,aSrcOffset);
}
#endif

/*
 *  This method assign from char*
 *  
 *  @update  rickg 03.01.2000
 *  @param  aString -- source String to be inserted into this
 *  @param  aLength -- number of chars to be copied from aCopy
 *  @param  aSrcOffset -- insertion position within this str
 *  @return this
 */
nsresult nsCString::Assign(const char* aString,PRInt32 aLength,PRUint32 aSrcOffset) {
  nsresult result=NS_OK;
  if(mStringValue.mBuffer!=aString){
    Truncate();
    result=Append(aString,aLength,aSrcOffset);
  }
  return result;
}


/*
 *  This method assign from nsAReadable
 *  
 *  @update  rickg 03.01.2000
 *  @param  aString -- source String to be inserted into this
 *  @param  aLength -- number of chars to be copied from aCopy
 *  @param  aSrcOffset -- insertion position within this str
 *  @return this
 */
nsresult nsCString::Assign(const nsAReadableString &aString,PRInt32 aLength,PRUint32 aSrcOffset) {
  Truncate();
  return SVAppendReadable<char>(mStringValue,aString,aLength,aSrcOffset);
}

//assign from a char
nsresult nsCString::Assign(char aChar) {
  Truncate();
  return Append(aChar);
}


  //***************************************
  //  Here come the append methods...
  //***************************************


nsresult Append(const nsStringValueImpl<char> &aString,PRInt32 aLength,PRUint32 aSrcOffset) {
  return NS_OK;
}


/*
 *  This method appends an nsCString
 *  
 *  @update  rickg 03.01.2000
 *  @param  aString -- source String to be inserted into this
 *  @param  aLength -- number of chars to be copied from aCopy
 *  @param  aSrcOffset -- insertion position within this str
 *  @return this
 */
nsresult nsCString::Append(const nsCString &aString,PRInt32 aLength,PRUint32 aSrcOffset) {
  return SVAppend< char, char > (mStringValue,aString.mStringValue,aLength,aSrcOffset);
}

/*
 *  This method appends an nsString
 *  
 *  @update  rickg 03.01.2000
 *  @param  aString -- source String to be inserted into this
 *  @param  aLength -- number of chars to be copied from aCopy
 *  @param  aSrcOffset -- insertion position within this str
 *  @return this
 */
nsresult nsCString::Append(const nsString &aString,PRInt32 aLength,PRUint32 aSrcOffset) {
  return SVAppend< char, PRUnichar > (mStringValue,aString.mStringValue,aString.mStringValue.mLength,aSrcOffset);
}

  //append from a char*
nsresult nsCString::Append(const char* aString,PRInt32 aLength,PRUint32 aSrcOffset) {
  nsresult result=NS_OK;
  if(aString) {
    nsStringValueImpl<char> theStringValue(const_cast<char*>(aString),aLength);
    result=SVAppend< char, char > (mStringValue,theStringValue,aLength,aSrcOffset);
  }
  return result;
}

  //append from a char
nsresult nsCString::Append(const char aChar) {
  char theBuffer[]={aChar,0};
  nsStringValueImpl<char> theStringValue(theBuffer,1);
  return SVAppend< char, char > (mStringValue,theStringValue,1,0);
}

  //append from a PRUnichar
nsresult nsCString::Append(const PRUnichar aChar) {
  PRUnichar theBuffer[]={aChar,0};
  nsStringValueImpl<PRUnichar> theStringValue(theBuffer,1);
  return SVAppend< char,PRUnichar> (mStringValue,theStringValue,1,0);
}

  //append from a PRUnichar*
#if 0
nsresult nsCString::Append(const PRUnichar* aString,PRInt32 aLength,PRUint32 aSrcOffset) {
  nsresult result=NS_OK;
  if(aString) {
    nsStringValueImpl<PRUnichar> theStringValue(const_cast<PRUnichar*>(aString),aLength);
    result=SVAppend< char,PRUnichar> (mStringValue,theStringValue,aLength,aSrcOffset);
  }
  return result;
}
#endif

  //append from an nsAReadableString (the ABT)
nsresult nsCString::Append(const nsAReadableString &aString,PRInt32 aLength,PRUint32 aSrcOffset) {
  return SVAppendReadable<char> (mStringValue,aString,aLength,aSrcOffset);
}

  //append an integer
nsresult nsCString::Append(PRInt32 anInteger,PRInt32 aRadix) {
  nsresult result=NS_OK;

  PRUint32 theInt=(PRUint32)anInteger;

  char buf[]={'0',0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0};

  PRInt32 radices[] = {1000000000,268435456};
  PRInt32 mask1=radices[16==aRadix];

  PRInt32 charpos=0;
  if(anInteger<0) {
    theInt*=-1;
    if(10==aRadix) {
      buf[charpos++]='-';
    }
    else theInt=(int)~(theInt-1);
  }

  PRBool isfirst=PR_TRUE;
  while(mask1>=1) {
    PRInt32 theDiv=theInt/mask1;
    if((theDiv) || (!isfirst)) {
      buf[charpos++]="0123456789abcdef"[theDiv];
      isfirst=PR_FALSE;
    }
    theInt-=theDiv*mask1;
    mask1/=aRadix;
  }
  return Append(buf);

  return result;
}

  //Append a float
nsresult nsCString::Append(float aFloat) {
  nsresult result=NS_OK;
  char buf[40];
  sprintf(buf,"%g",aFloat);

  nsStringValueImpl<char> theStringValue(buf);
  Append(theStringValue);

  return result;
}


  //***************************************
  //  Here come a few deletion methods...
  //***************************************

nsresult nsCString::Cut(PRUint32 anOffset,PRInt32 aCount) {
  return SVDelete<char>(mStringValue,anOffset,aCount);
}

nsresult nsCString::Trim(const char* aTrimSet,PRBool aEliminateLeading,PRBool aEliminateTrailing,PRBool aIgnoreQuotes) {
  return SVTrim<char>(mStringValue,aTrimSet,aEliminateLeading,aEliminateTrailing);
}

/**
 *  This method strips chars in given set from string.
 *  You can control whether chars are yanked from
 *  start and end of string as well.
 *  
 * @update  rickg 03.01.2000
 *  @param   aEliminateLeading controls stripping of leading ws
 *  @param   aEliminateTrailing controls stripping of trailing ws
 *  @return  this
 */
nsresult nsCString::CompressSet(const char* aSet, char aChar,PRBool aEliminateLeading,PRBool aEliminateTrailing){
  nsStringValueImpl<char> theSet(const_cast<char*>(aSet));
  return SVCompressSet<char>(mStringValue,theSet,aChar,aEliminateLeading,aEliminateTrailing);
}

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
nsresult nsCString::CompressWhitespace( PRBool aEliminateLeading,PRBool aEliminateTrailing){
  return CompressSet(kWhitespace,' ',aEliminateLeading,aEliminateTrailing);
}


  //***************************************
  //  Here come a wad of insert methods...
  //***************************************

nsresult nsCString::Insert(const nsCString& aString,PRUint32 anOffset,PRInt32 aCount) {
  return SVInsert<char>(mStringValue,anOffset,aString.mStringValue,aCount,anOffset);
}

/**
 * Insert a char* into this string at a specified offset.
 *
 * @update  rickg 03.01.2000
 * @param   char* aCString to be inserted into this string
 * @param   anOffset is insert pos in str 
 * @param   aLength -- length of given buffer or -1 if you want me to compute length.
 * NOTE:    IFF you pass -1 as aCount, then your buffer must be null terminated.
 *
 * @return  this
 */
nsresult nsCString::Insert(const char* aString,PRUint32 anOffset,PRInt32 aLength){
  nsresult result=NS_OK;
  if(aString){

    nsStringValueImpl<char> theStringValue(const_cast<char*>(aString),aLength);
    
    if(0<theStringValue.mLength){
      result=SVInsert<char>(mStringValue,anOffset,theStringValue,theStringValue.mLength,0);
    }
  }
  return result;  
}


/**
 * Insert a single uni-char into this string at
 * a specified offset.
 *
 * @update  rickg 03.01.2000
 * @param   aChar char to be inserted into this string
 * @param   anOffset is insert pos in str 
 * @return  this
 */
nsresult nsCString::Insert(char aChar,PRUint32 anOffset){
  char theBuffer[]={aChar,0};
  nsStringValueImpl<char> theStringValue(theBuffer,1);

  return SVInsert<char>(mStringValue,anOffset,theStringValue,1,0);
}


  //*******************************************
  //  Here come inplace replacement  methods...
  //*******************************************

/**
 *  swaps occurence of 1 char for another
 *  
 *  @return  this
 */
nsresult nsCString::ReplaceChar(char anOldChar,char aNewChar){
  return SVReplaceChar<char>(mStringValue,anOldChar,aNewChar,mStringValue.mLength,0);
}

nsresult nsCString::ReplaceChar(const char* aSet,char aNewChar){
  nsStringValueImpl<char> theSet(const_cast<char*>(aSet));
  return SVReplaceCharsInSet<char>(mStringValue,theSet,aNewChar,mStringValue.mLength,0);
}

nsresult nsCString::ReplaceSubstring( const nsCString& aTarget,const nsCString& aNewValue){
  return SVReplace<char>(mStringValue,aTarget.mStringValue,aNewValue.mStringValue,mStringValue.mLength,0);
}


nsresult nsCString::ReplaceSubstring(const char* aTarget,const char* aNewValue) {
  nsresult result=NS_OK;

  const nsStringValueImpl<char> theTarget(const_cast<char*>(aTarget));
  const nsStringValueImpl<char> theNewValue(const_cast<char*>(aNewValue));

  return SVReplace<char>(mStringValue,theTarget,theNewValue,mStringValue.mLength,0);
}

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
nsresult nsCString::StripChar(char aChar,PRUint32 anOffset){
  return SVStripChar<char>(mStringValue,aChar,mStringValue.mLength,anOffset);
}

/**
 *  This method is used to remove all occurances of the
 *  given character from this string.
 *  
 *  @update  rickg 03.01.2000
 *  @param  aChar -- char to be stripped
 *  @param  anOffset -- where in this string to start stripping chars
 *  @return *this 
 */
nsresult nsCString::StripChar(PRInt32 anInt,PRUint32 anOffset){
  return SVStripChar<char>(mStringValue,(char)anInt,mStringValue.mLength,anOffset);
}

/**
 *  This method is used to remove all occurances of the
 *  characters found in aSet from this string.
 *  
 *  @update  rickg 03.01.2000
 *  @param  aSet -- characters to be cut from this
 *  @return *this 
 */
nsresult nsCString::StripChars(const char* aSet){
  nsStringValueImpl<char> theSet(const_cast<char*>(aSet));
  return SVStripCharsInSet<char>(mStringValue,aSet,mStringValue.mLength,0);
}

/**
 *  This method strips whitespace throughout the string
 *  
 *  @update  rickg 03.01.2000
 *  @return  this
 */
nsresult nsCString::StripWhitespace() {
  static const char* kWhitespace="\b\t\r\n ";
  return StripChars(kWhitespace);
}

  //**************************************************
  //  Here are some methods that extract substrings...
  //**************************************************

nsresult nsCString::Left(nsCString& aCopy,PRInt32 aCount) const {
  aCount = (aCount<0) ? mStringValue.mLength : MinInt(aCount,mStringValue.mLength);
  return aCopy.Assign(*this,aCount,0);
}

nsresult nsCString::Mid(nsCString& aCopy,PRUint32 anOffset,PRInt32 aCount) const {
  aCount = (aCount<0) ? mStringValue.mLength : MinInt(aCount,mStringValue.mLength);
  return aCopy.Assign(*this,aCount,anOffset);
}

nsresult nsCString::Right(nsCString& aCopy,PRInt32 aCount) const {
  PRInt32 offset=MaxInt(mStringValue.mLength-aCount,0);
  return Mid(aCopy,offset,aCount);
}


  //*******************************************
  //  Here come the operator+=() methods...
  //*******************************************


nsCString& nsCString::operator+=(const char* aString) {
  Append(aString);
  return *this;
}

/*
nsCString& nsCString::operator+=(const PRUnichar* aString) {
  Append(aString);
  return *this;
}
*/

nsCString& nsCString::operator+=(const char aChar) {
  char theBuffer[]={aChar,0};
  Append(theBuffer,1);
  return *this;
}

/*
nsCString& nsCString::operator+=(const PRUnichar aChar) {
  PRUnichar theBuffer[]={aChar,0};
  Append(theBuffer,1);
  return *this;
}
*/

nsCString& nsCString::operator+=(const int anInt){
  Append(anInt,10);
  return *this;
}

void nsCString::ToLowerCase() {
  SVToLowerCase<char>(mStringValue);
}

void nsCString::ToLowerCase(nsCString &aString) const {
  aString=*this;
  aString.ToLowerCase();
}

void nsCString::ToUpperCase() {
  SVToUpperCase<char>(mStringValue);
}

void nsCString::ToUpperCase(nsCString &aString) const {
  aString=*this;
  aString.ToUpperCase();
}


PRInt32  nsCString::Compare(const nsCString &aString,PRBool aIgnoreCase,PRInt32 aCount) const {
  return SVCompare<char,char>(mStringValue,aString.mStringValue,aIgnoreCase,aCount);
}

PRInt32  nsCString::Compare(const char* aString,PRBool aIgnoreCase,PRInt32 aCount) const {
  nsStringValueImpl<char> theStringValue(const_cast<char*>(aString),aCount);
  return SVCompare<char,char>(mStringValue,theStringValue,aIgnoreCase,aCount);
}

PRInt32  nsCString::Compare(const PRUnichar* aString,PRBool aIgnoreCase,PRInt32 aCount) const {
  nsStringValueImpl<PRUnichar> theStringValue(const_cast<PRUnichar*>(aString),aCount);
  return SVCompare<char,PRUnichar>(mStringValue,theStringValue,aIgnoreCase,aCount);
}

PRBool  nsCString::Equals(const nsString &aString,PRBool aIgnoreCase,PRInt32 aCount) const {
  PRInt32 result=SVCompare<char,PRUnichar>(mStringValue,aString.mStringValue,aIgnoreCase,aCount);
  return PRBool(0==result);
}

PRBool  nsCString::Equals(const nsCString &aString,PRBool aIgnoreCase,PRInt32 aCount) const {
  PRInt32 result=SVCompare<char,char>(mStringValue,aString.mStringValue,aIgnoreCase,aCount);
  return PRBool(0==result);
}

PRBool  nsCString::Equals(const char* aString,PRBool aIgnoreCase,PRInt32 aCount) const {
  nsStringValueImpl<char> theStringValue(const_cast<char*>(aString),aCount);
  PRInt32 result=SVCompare<char,char>(mStringValue,theStringValue,aIgnoreCase,aCount);
  return PRBool(0==result);
}


PRBool  nsCString::Equals(const PRUnichar* aString,PRBool aIgnoreCase,PRInt32 aCount) const {
  nsStringValueImpl<PRUnichar> theStringValue(const_cast<PRUnichar*>(aString),aCount);
  PRInt32 result=SVCompare<char,PRUnichar>(mStringValue,theStringValue,aIgnoreCase,aCount);
  return PRBool(0==result);
}

PRBool nsCString::EqualsIgnoreCase(const char* s1, const char* s2) const {
  PRBool result=PR_FALSE;
  return result;
}

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
PRInt32 nsCString::Find(const nsString& aTarget,PRBool aIgnoreCase,PRUint32 anOffset,PRInt32 aRepCount) const {
  return SVFind<char,PRUnichar>(mStringValue,aTarget.mStringValue,aIgnoreCase,aRepCount,anOffset);
}

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
PRInt32 nsCString::Find(const nsCString &aString,PRBool aIgnoreCase,PRUint32 anOffset,PRInt32 aRepCount) const {
  return SVFind<char,char>(mStringValue,aString.mStringValue,aIgnoreCase,aRepCount,anOffset);
}

/**
 *  search for given string within this char* string
 *  
 *  @update  rickg 03.01.2000
 *  @param   aString - substr to be found
 *  @param   aIgnoreCase tells us whether or not to do caseless compare
 *  @param   anOffset tells us where in this string to start searching
 *  @param   aRepCount tells us how many iterations to make starting at the given offset
 *  @return  offset in string, or -1 (kNotFound)
 */
PRInt32 nsCString::Find(const char* aString,PRBool aIgnoreCase,PRUint32 anOffset,PRInt32 aRepCount) const {
  nsStringValueImpl<char> theString(const_cast<char*>(aString));
  return SVFind<char,char>(mStringValue,theString,aIgnoreCase,aRepCount,anOffset);
}

/**
 *  search for given string within this PRUnichar* string
 *  
 *  @update  rickg 03.01.2000
 *  @param   aString - substr to be found
 *  @param   aIgnoreCase tells us whether or not to do caseless compare
 *  @param   anOffset tells us where in this string to start searching
 *  @param   aRepCount tells us how many iterations to make starting at the given offset
 *  @return  offset in string, or -1 (kNotFound)
 */
PRInt32 nsCString::Find(const PRUnichar* aString,PRBool aIgnoreCase,PRUint32 anOffset,PRInt32 aRepCount) const {

  nsStringValueImpl<PRUnichar> theString(const_cast<PRUnichar*>(aString));
  return SVFind<char,PRUnichar>(mStringValue,theString,aIgnoreCase,aRepCount,anOffset);
}

/**
 *  This method finds the offset of the first char in this string that is
 *  a member of the given charset, starting the search at anOffset
 *  
 *  @update  rickg 03.01.2000
 *  @param   aString contains set of chars to be found
 *  @param   anOffset -- where in this string to start searching
 *  @return  
 */
PRInt32 nsCString::FindCharInSet(const nsString& aString,PRUint32 anOffset) const{
  return SVFindCharInSet<char,PRUnichar>(mStringValue,aString.mStringValue,PR_FALSE,mStringValue.mLength,anOffset);
}


/**
 *  This method finds the offset of the first char in this string that is
 *  a member of the given charset, starting the search at anOffset
 *  
 *  @update  rickg 03.01.2000
 *  @param   aString contains set of chars to be found
 *  @param   anOffset -- where in this string to start searching
 *  @return  
 */
PRInt32 nsCString::FindCharInSet(const char *aString,PRUint32 anOffset) const{

  nsStringValueImpl<char> theStringValue(const_cast<char*>(aString));
  return SVFindCharInSet<char,char>(mStringValue,theStringValue,PR_FALSE,mStringValue.mLength,anOffset);
}

/**
 *  Search for a given char, starting at given offset
 *  
 *  @update  rickg 03.01.2000
 *  @param   aChar is the unichar to be sought
 *  @param   aIgnoreCase tells us whether or not to do caseless compare
 *  @param   anOffset tells us where in this string to start searching
 *  @param   aCount tells us how many iterations to make starting at the given offset
 *  @return  offset in string, or -1 (kNotFound)
 */
PRInt32 nsCString::FindChar(char aChar,PRBool aIgnoreCase,PRInt32 anOffset,PRInt32 aCount) const {
  return SVFindChar<char>(mStringValue,aChar,aIgnoreCase,aCount,anOffset);
}

/**
 *  Reverse search for given char within this string
 *  
 *  @update  gess 3/25/98
 *  @param   aChar - char to be found
 *  @param   aIgnoreCase tells us whether or not to do caseless compare
 *  @param   anOffset tells us where in this string to start searching (-1 means start at end)
 *  @param   aRepCount tells us how many iterations to make starting at the given offset
 *  @return  offset in string, or -1 (kNotFound)
 */
PRInt32 nsCString::RFindChar(char aChar,PRBool aIgnoreCase,PRInt32 anOffset,PRInt32 aRepCount) const{
  return SVRFindChar<char>(mStringValue,aChar,aIgnoreCase,aRepCount,anOffset);
}


PRInt32 nsCString::RFind(const char* aString,PRBool aIgnoreCase,PRInt32 anOffset,PRInt32 aRepCount) const{
  nsStringValueImpl<char> theStringValue(const_cast<char*>(aString));
  return SVRFind<char,char>(mStringValue,theStringValue,aIgnoreCase,aRepCount,anOffset);
}
  

/**
 *  This method finds (in reverse) the offset of the first char in this string that is
 *  a member of the given charset, starting the search at anOffset
 *  
 *  @update  rickg 03.01.2000
 *  @param   aString contains set of chars to be found
 *  @param   anOffset -- where in this string to start searching
 *  @return  
 */
PRInt32 nsCString::RFindCharInSet(const nsCString& aString,PRInt32 anOffset) const {
  return SVRFindCharInSet<char,char>(mStringValue,aString.mStringValue,PR_FALSE,mStringValue.mLength,anOffset);
}


/**
 *  This method finds (in reverse) the offset of the first char in this string that is
 *  a member of the given charset, starting the search at anOffset
 *  
 *  @update  rickg 03.01.2000
 *  @param   aString contains set of chars to be found
 *  @param   anOffset -- where in this string to start searching
 *  @return  
 */
PRInt32 nsCString::RFindCharInSet(const char* aString,PRInt32 anOffset) const{
  nsStringValueImpl<char> theStringValue(const_cast<char*>(aString));
  return SVRFindCharInSet<char,char>(mStringValue,theStringValue,PR_FALSE,mStringValue.mLength,anOffset);
}

/***************************************
  These convert to a different type
 ***************************************/

void nsCString::Recycle(nsCString* aString) {
  //NOT IMPLEMENTED
}

nsCString* nsCString::CreateString(void) {
  return 0; //NOT IMPLEMENTED
}

/**
 * This method constructs a new nsString is a clone of this string.
 */
nsCString* nsCString::ToNewString() const {
  return 0; //NOT IMPLEMENTED
}


char* nsCString::ToNewCString() const {
  return 0; //NOT IMPLEMENTED
}

char* nsCString::ToNewUTF8String() const {
  return 0; //NOT IMPLEMENTED
}

PRUnichar* nsCString::ToNewUnicode() const {
  return 0; //NOT IMPLEMENTED
}

char* nsCString::ToCString(char* aBuf,PRUint32 aBufLength,PRUint32 anOffset) const {
  int len=(mStringValue.mLength<aBufLength) ? mStringValue.mLength : aBufLength;
  for(int i=0;i<len;i++){
    aBuf[i]=(char)mStringValue.mBuffer[i];
  }
  aBuf[len]=0;
  return aBuf;
}


PRInt32 nsCString::CountChar(char aChar) {  
  return SVCountChar<char>(mStringValue,aChar);
}

  //******************************************
  // Utility methods...
  //******************************************

  
  //This will not work correctly for any unicode set other than ascii
void nsCString::DebugDump(void) const { 
  SVDebugDump<char>(mStringValue);
}

/**
 * Perform string to float conversion.
 * @param   aErrorCode will contain error if one occurs
 * @return  float rep of string value
 */
float nsCString::ToFloat(PRInt32* aErrorCode) const {
  char buf[100];
  if (mStringValue.mLength > PRInt32(sizeof(buf)-1)) {
    *aErrorCode = (PRInt32) NS_ERROR_ILLEGAL_VALUE;
    return 0.0f;
  }
  char* cp = ToCString(buf, sizeof(buf));
  float f = (float) PR_strtod(cp, &cp);
  if (*cp != 0) {
    *aErrorCode = (PRInt32) NS_ERROR_ILLEGAL_VALUE;
  }
  *aErrorCode = (PRInt32) NS_OK;
  return f;
}

/**
 * Perform string to int conversion.
 * @param   aErrorCode will contain error if one occurs
 * @param   aRadix tells us which radix to assume; kAutoDetect tells us to determine the radix for you.
 * @return  int rep of string value, and possible (out) error code
 */
PRInt32 nsCString::ToInteger(PRInt32* anErrorCode,PRUint32 aRadix) const{
  return SVToInteger<char>(mStringValue,anErrorCode,aRadix);
}


