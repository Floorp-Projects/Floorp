
#include "nsString2x.h"
#include "nsStringx.h"
#include "nsBufferManager.h"
#include <stdio.h>  
#include "nsAutoStringImpl.h"

  //******************************************
  // Ctor's
  //******************************************

nsString::nsString() : mStringValue() {
}
  
  //call this version for nsString,nsString and the autostrings
nsString::nsString(const nsString& aString,PRInt32 aLength) : mStringValue() {
  Assign(aString,aLength,0);
}

  //call this version with nsCString
nsString::nsString(const nsCString &aString,PRInt32 aLength) : mStringValue() {
  Assign(aString,aLength);
}

  //call this version for char*'s....
nsString::nsString(const PRUnichar* aString,PRInt32 aLength) : mStringValue() {
  Assign(aString,aLength,0);
}

  //call this version for a single char of type char...
nsString::nsString(const char aChar) : mStringValue() {
  Append(aChar);
}


  //call this version for PRUnichar*'s....
nsString::nsString(const char* aString,PRInt32 aLength) : mStringValue() {
  nsStringValueImpl<char> theString(const_cast<char*>(aString),aLength);
  SVAssign(mStringValue,theString,theString.mLength,0);
}

  //call this version for all other ABT versions of readable strings
nsString::nsString(const nsAReadableString &aString) : mStringValue() {
  Assign(aString);
}

  //call this version for stack-based string buffers...
nsString::nsString(const nsStackBuffer<PRUnichar> &aBuffer) : mStringValue(aBuffer) {
}

nsString::~nsString() { }


  //******************************************
  // Assigment operators
  //******************************************

nsString& nsString::operator=(const nsString& aString) {
  if(aString.mStringValue.mBuffer!=mStringValue.mBuffer) {
    Assign(aString);
  }
  return *this;
}

nsString& nsString::operator=(const nsCString &aString) {
  Assign(aString);
  return *this;
}

nsString& nsString::operator=(const PRUnichar *aString) {
  if(mStringValue.mBuffer!=aString) {
    nsStringValueImpl<PRUnichar> theStringValue(const_cast<PRUnichar*>(aString));
    Assign(aString);
  }
  return *this;
} 

nsString& nsString::operator=(const char *aString) {
  nsStringValueImpl<char> theStringValue(const_cast<char*>(aString));
  SVAssign(mStringValue,theStringValue,theStringValue.mLength,0);
  return *this;
}

nsString& nsString::operator=(const char aChar) {
  Assign(aChar);
  return *this;
} 

nsString& nsString::operator=(const PRUnichar aChar) {
  Assign(aChar);
  return *this;
} 

  //******************************************
  // Here are the accessor methods...
  //******************************************


nsresult nsString::SetCapacity(PRUint32 aCapacity) {
  if(aCapacity>mStringValue.mCapacity) {
    SVGrowCapacity<PRUnichar>(mStringValue,aCapacity);
    SVAddNullTerminator<PRUnichar>(mStringValue);
  }
  return NS_OK;
}

PRUnichar& nsString::operator[](PRUint32 aOffset)  {
  static PRUnichar gSharedChar=0;
  return (mStringValue.mBuffer) ? mStringValue.mBuffer[aOffset] : gSharedChar;
}

//  operator const char* const() {return mStringValue.mBuffer;}


PRBool nsString::SetCharAt(PRUnichar aChar,PRUint32 anIndex) {
  PRBool result=PR_FALSE;
  if(anIndex<mStringValue.mLength){
    mStringValue.mBuffer[anIndex]=(char)aChar;
    result=PR_TRUE;
  }
  return result;
}

  //these aren't the real deal, but serve as a placeholder for us to implement iterators.
nsAReadableStringIterator* nsString::First() {return new nsAReadableStringIterator(); }
nsAReadableStringIterator* nsString::Last() {return new nsAReadableStringIterator(); }


  //******************************************
  // Here are the Assignment methods, 
  // and other mutators...
  //******************************************

nsresult nsString::Truncate(PRUint32 anOffset) {
  SVTruncate<PRUnichar>(mStringValue,anOffset);
  return NS_OK;
}

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
nsresult nsString::Assign(const nsString& aString,PRInt32 aLength,PRUint32 aSrcOffset) {
  nsresult result=NS_OK;
  if(mStringValue.mBuffer!=aString.mStringValue.mBuffer){
    Truncate();
    result=Append(aString,aLength,aSrcOffset);
  }
  return result;
}

  //assign from a compatible string pointer
nsresult nsString::Assign(const PRUnichar* aString,PRInt32 aLength,PRUint32 aSrcOffset) {
  nsresult result=NS_OK;
  if(mStringValue.mBuffer!=aString){
    Truncate();
    result=Append(aString,aLength,aSrcOffset);
  }
  return result;
}


  //assign from a compatible string pointer
nsresult nsString::Assign(const char* aString,PRInt32 aLength,PRUint32 aSrcOffset) {
  Truncate();
  return Append(aString,aLength,aSrcOffset);
}

  //assign from a stringValueImpl of a compatible type
nsresult nsString::Assign(const nsCString& aString,PRInt32 aLength,PRUint32 aSrcOffset) {
  nsresult result=NS_OK;
  Truncate();
  return Append(aString,aLength,aSrcOffset);
}

//assign from an nsAReadableString (the ABT)
nsresult nsString::Assign(const nsAReadableString &aString,PRInt32 aLength,PRUint32 aSrcOffset) {
  Truncate();
  return SVAppendReadable<PRUnichar>(mStringValue,aString,aLength,aSrcOffset);
}

//assign from a char
nsresult nsString::Assign(char aChar) {
  Truncate();
  return Append(aChar);
}

//assign from a PRUnichar
nsresult nsString::Assign(PRUnichar aChar) {
  Truncate();
  return Append(aChar);
}

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
nsresult nsString::Append(const nsString &aString,PRInt32 aLength,PRUint32 aSrcOffset) {
  aLength = (aLength<0) ? aString.mStringValue.mLength : MinInt(aLength,aString.mStringValue.mLength);
  return SVAppend< PRUnichar, PRUnichar > (mStringValue,aString.mStringValue,aLength,aSrcOffset);
}

  //append from a type compatible string pointer
nsresult nsString::Append(const char* aString,PRInt32 aLength,PRUint32 aSrcOffset) {
  nsresult result=NS_OK;
  if(aString) {
    if(aLength<0) aLength=nsCRT::strlen(aString);
    nsStringValueImpl<char> theStringValue(const_cast<char*>(aString),aLength);
    result=SVAppend< PRUnichar, char > (mStringValue,theStringValue,aLength,aSrcOffset);
  }
  return result;
}

  //append from alternative type string pointer
nsresult nsString::Append(const char aChar) {
  char theBuffer[]={aChar,0};
  nsStringValueImpl<char> theStringValue(theBuffer,1);
  return SVAppend< PRUnichar, char > (mStringValue,theStringValue,1,0);
}

  //append from alternative type string pointer
nsresult nsString::Append(const PRUnichar aChar) {
  PRUnichar theBuffer[]={aChar,0};
  nsStringValueImpl<PRUnichar> theStringValue(theBuffer,1);
  return SVAppend< PRUnichar,PRUnichar> (mStringValue,theStringValue,1,0);
}

  //append from alternative type string pointer
nsresult nsString::Append(const PRUnichar* aString,PRInt32 aLength,PRUint32 aSrcOffset) {
  nsresult result=NS_OK;
  if(aString) {
    nsStringValueImpl<PRUnichar> theStringValue(const_cast<PRUnichar*>(aString),aLength);
    result=SVAppend< PRUnichar,PRUnichar> (mStringValue,theStringValue,aLength,aSrcOffset);
  }
  return result;
}

  //append from an nsCString
nsresult nsString::Append(const nsCString &aString,PRInt32 aLength,PRUint32 aSrcOffset) {
  aLength = (aLength<0) ? aString.mStringValue.mLength : MinInt(aLength,aString.mStringValue.mLength);
  return SVAppend< PRUnichar, char > (mStringValue,aString.mStringValue,aLength,aSrcOffset);
}

  //append from an nsAReadableString (the ABT)
nsresult nsString::Append(const nsAReadableString &aString,PRInt32 aLength,PRUint32 aSrcOffset) {
  aLength = (aLength<0) ? aString.Length() : MinInt(aLength,aString.Length());
  return SVAppendReadable<PRUnichar> (mStringValue,aString,aLength,aSrcOffset);
}

  //append an integer
nsresult nsString::Append(PRInt32 anInteger,PRInt32 aRadix) {
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
nsresult nsString::Append(float aFloat) {
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

nsresult nsString::Cut(PRUint32 anOffset,PRInt32 aCount) {
  return SVDelete<PRUnichar>(mStringValue,anOffset,aCount);
}

nsresult nsString::Trim(const char* aTrimSet,PRBool aEliminateLeading,PRBool aEliminateTrailing,PRBool aIgnoreQuotes) {
  return SVTrim<PRUnichar>(mStringValue,aTrimSet,aEliminateLeading,aEliminateTrailing);
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
nsresult nsString::CompressSet(const char* aSet, PRUnichar aChar,PRBool aEliminateLeading,PRBool aEliminateTrailing){
  nsStringValueImpl<char> theSet(const_cast<char*>(aSet));
  return SVCompressSet<PRUnichar>(mStringValue,theSet,aChar,aEliminateLeading,aEliminateTrailing);
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
nsresult nsString::CompressWhitespace( PRBool aEliminateLeading,PRBool aEliminateTrailing){
  return CompressSet(kWhitespace,' ',aEliminateLeading,aEliminateTrailing);
}


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
nsresult nsString::Insert(const nsString& aCopy,PRUint32 anOffset,PRInt32 aCount) {
  return SVInsert<PRUnichar>(mStringValue,anOffset,aCopy.mStringValue,aCount,anOffset);
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
nsresult nsString::Insert(const char* aString,PRUint32 anOffset,PRInt32 aLength){
  nsresult result=NS_OK;
  if(aString){

    nsStringValueImpl<char> theStringValue(const_cast<char*>(aString),aLength);
    
    if(0<theStringValue.mLength){
      result=SVInsert<PRUnichar>(mStringValue,anOffset,theStringValue,theStringValue.mLength,0);
    }
  }
  return result;  
}

/**
 * Insert a PRUnichar* into this string at a specified offset.
 *
 * @update  rickg 03.01.2000
 * @param   char* aCString to be inserted into this string
 * @param   anOffset is insert pos in str 
 * @param   aLength -- length of given buffer or -1 if you want me to compute length.
 * NOTE:    IFF you pass -1 as aCount, then your buffer must be null terminated.
 *
 * @return  this
 */
nsresult nsString::Insert(const PRUnichar* aString,PRUint32 anOffset,PRInt32 aLength){
  nsresult result=NS_OK;
  if(aString){

    nsStringValueImpl<PRUnichar> theStringValue(const_cast<PRUnichar*>(aString),aLength);
    
    if(0<theStringValue.mLength){
      result=SVInsert<PRUnichar>(mStringValue,anOffset,theStringValue,theStringValue.mLength,0);
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
nsresult nsString::Insert(char aChar,PRUint32 anOffset){
  char theBuffer[]={aChar,0};
  nsStringValueImpl<char> theStringValue(theBuffer,1);

  return SVInsert<PRUnichar>(mStringValue,anOffset,theStringValue,1,0);
}


  //*******************************************
  //  Here come inplace replacement  methods...
  //*******************************************

/**
 *  swaps occurence of 1 char for another
 *  
 *  @return  this
 */
nsresult nsString::ReplaceChar(char anOldChar,char aNewChar){
  return SVReplaceChar<PRUnichar>(mStringValue,anOldChar,aNewChar,mStringValue.mLength,0);
}

nsresult nsString::ReplaceChar(const char* aSet,char aNewChar){
  nsStringValueImpl<char> theSet(const_cast<char*>(aSet));
  return SVReplaceCharsInSet<PRUnichar>(mStringValue,theSet,aNewChar,mStringValue.mLength,0);
}

nsresult nsString::ReplaceSubstring( const nsString& aTarget,const nsString& aNewValue){
  return SVReplace<PRUnichar>(mStringValue,aTarget.mStringValue,aNewValue.mStringValue,mStringValue.mLength,0);
}


nsresult nsString::ReplaceSubstring(const char* aTarget,const char* aNewValue) {
  nsresult result=NS_OK;

  const nsStringValueImpl<char> theTarget(const_cast<char*>(aTarget));
  const nsStringValueImpl<char> theNewValue(const_cast<char*>(aNewValue));

  return SVReplace<PRUnichar>(mStringValue,theTarget,theNewValue,mStringValue.mLength,0);
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
nsresult nsString::StripChar(char aChar,PRUint32 anOffset){
  return SVStripChar<PRUnichar>(mStringValue,aChar,mStringValue.mLength,anOffset);
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
nsresult nsString::StripChar(PRInt32 anInt,PRUint32 anOffset){
  return SVStripChar<PRUnichar>(mStringValue,(PRUnichar)anInt,mStringValue.mLength,anOffset);
}

/**
 *  This method is used to remove all occurances of the
 *  characters found in aSet from this string.
 *  
 *  @update  rickg 03.01.2000
 *  @param  aSet -- characters to be cut from this
 *  @return *this 
 */
nsresult nsString::StripChars(const char* aSet){
  nsStringValueImpl<char> theSet(const_cast<char*>(aSet));
  return SVStripCharsInSet<PRUnichar>(mStringValue,aSet,mStringValue.mLength,0);
}

/**
 *  This method strips whitespace throughout the string
 *  
 *  @update  rickg 03.01.2000
 *  @return  this
 */
nsresult nsString::StripWhitespace() {
  static const char* kWhitespace="\b\t\r\n ";
  return StripChars(kWhitespace);
}

  //**************************************************
  //  Here are some methods that extract substrings...
  //**************************************************

nsresult nsString::Left(nsString& aCopy,PRInt32 aCount) const {
  aCount = (aCount<0) ? mStringValue.mLength : MinInt(aCount,mStringValue.mLength);
  return aCopy.Assign(*this,aCount,0);
}

nsresult nsString::Mid(nsString& aCopy,PRUint32 anOffset,PRInt32 aCount) const {
  aCount = (aCount<0) ? mStringValue.mLength : MinInt(aCount,mStringValue.mLength);
  return aCopy.Assign(*this,aCount,anOffset);
}

nsresult nsString::Right(nsString& aCopy,PRInt32 aCount) const {
  PRInt32 offset=MaxInt(mStringValue.mLength-aCount,0);
  return Mid(aCopy,offset,aCount);
}


  //*******************************************
  //  Here come the operator+=() methods...
  //*******************************************

nsString& nsString::operator+=(const nsString& aString){
  SVAppend(mStringValue,aString.mStringValue,aString.mStringValue.mLength,0);
  return *this;
}

nsString& nsString::operator+=(const char* aString) {
  Append(aString);
  return *this;
}

nsString& nsString::operator+=(const PRUnichar* aString) {
  Append(aString);
  return *this;
}

nsString& nsString::operator+=(const char aChar) {
  char theBuffer[]={aChar,0};
  Append(theBuffer,1);
  return *this;
}

nsString& nsString::operator+=(const PRUnichar aChar) {
  PRUnichar theBuffer[]={aChar,0};
  Append(theBuffer,1);
  return *this;
}

nsString& nsString::operator+=(const int anInt){
  Append(anInt,10);
  return *this;
}

void nsString::ToLowerCase() {
  SVToLowerCase<PRUnichar>(mStringValue);
}

void nsString::ToLowerCase(nsString &aString) const {
  aString=*this;
  aString.ToLowerCase();
}

void nsString::ToUpperCase() {
  SVToUpperCase<PRUnichar>(mStringValue);
}

void nsString::ToUpperCase(nsString &aString) const {
  aString=*this;
  aString.ToUpperCase();
}


PRInt32  nsString::Compare(const nsString &aString,PRBool aIgnoreCase,PRInt32 aCount) const {
  return SVCompare<PRUnichar,PRUnichar>(mStringValue,aString.mStringValue,aIgnoreCase,aCount);
}

PRInt32  nsString::Compare(const nsCString &aString,PRBool aIgnoreCase,PRInt32 aCount) const {
  return SVCompare<PRUnichar,char>(mStringValue,aString.mStringValue,aIgnoreCase,aCount);
}

PRInt32  nsString::Compare(const char* aString,PRBool aIgnoreCase,PRInt32 aCount) const {
  nsStringValueImpl<char> theStringValue(const_cast<char*>(aString),aCount);
  return SVCompare<PRUnichar,char>(mStringValue,theStringValue,aIgnoreCase,aCount);
}

PRInt32  nsString::Compare(const PRUnichar* aString,PRBool aIgnoreCase,PRInt32 aCount) const {
  nsStringValueImpl<PRUnichar> theStringValue(const_cast<PRUnichar*>(aString),aCount);
  return SVCompare<PRUnichar,PRUnichar>(mStringValue,theStringValue,aIgnoreCase,aCount);
}

PRBool  nsString::Equals(const nsString &aString,PRBool aIgnoreCase,PRInt32 aCount) const {
  PRInt32 result=SVCompare<PRUnichar,PRUnichar>(mStringValue,aString.mStringValue,aIgnoreCase,aCount);
  return PRBool(0==result);
}

PRBool  nsString::Equals(const nsCString &aString,PRBool aIgnoreCase,PRInt32 aCount) const {
  PRInt32 result=SVCompare<PRUnichar,char>(mStringValue,aString.mStringValue,aIgnoreCase,aCount);
  return PRBool(0==result);
}

PRBool  nsString::Equals(const char* aString,PRBool aIgnoreCase,PRInt32 aCount) const {
  nsStringValueImpl<char> theStringValue(const_cast<char*>(aString),aCount);
  PRInt32 result=SVCompare<PRUnichar,char>(mStringValue,theStringValue,aIgnoreCase,aCount);
  return PRBool(0==result);
}


PRBool  nsString::Equals(const PRUnichar* aString,PRBool aIgnoreCase,PRInt32 aCount) const {
  nsStringValueImpl<PRUnichar> theStringValue(const_cast<PRUnichar*>(aString),aCount);
  PRInt32 result=SVCompare<PRUnichar,PRUnichar>(mStringValue,theStringValue,aIgnoreCase,aCount);
  return PRBool(0==result);
}

PRBool nsString::EqualsIgnoreCase(const PRUnichar* s1, const PRUnichar* s2) const {
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
PRInt32 nsString::Find(const nsString& aTarget,PRBool aIgnoreCase,PRUint32 anOffset,PRInt32 aRepCount) const {
  return SVFind<PRUnichar,PRUnichar>(mStringValue,aTarget.mStringValue,aIgnoreCase,aRepCount,anOffset);
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
PRInt32 nsString::Find(const nsCString &aString,PRBool aIgnoreCase,PRUint32 anOffset,PRInt32 aRepCount) const {
  return SVFind<PRUnichar,char>(mStringValue,aString.mStringValue,aIgnoreCase,aRepCount,anOffset);
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
PRInt32 nsString::Find(const char* aString,PRBool aIgnoreCase,PRUint32 anOffset,PRInt32 aRepCount) const {
  nsStringValueImpl<char> theString(const_cast<char*>(aString));
  return SVFind<PRUnichar,char>(mStringValue,theString,aIgnoreCase,aRepCount,anOffset);
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
PRInt32 nsString::Find(const PRUnichar* aString,PRBool aIgnoreCase,PRUint32 anOffset,PRInt32 aRepCount) const {

  nsStringValueImpl<PRUnichar> theString(const_cast<PRUnichar*>(aString));
  return SVFind<PRUnichar,PRUnichar>(mStringValue,theString,aIgnoreCase,aRepCount,anOffset);
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
PRInt32 nsString::FindCharInSet(const nsString& aString,PRUint32 anOffset) const{
  return SVFindCharInSet<PRUnichar,PRUnichar>(mStringValue,aString.mStringValue,PR_FALSE,mStringValue.mLength,anOffset);
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
PRInt32 nsString::FindCharInSet(const char *aString,PRUint32 anOffset) const{

  nsStringValueImpl<char> theStringValue(const_cast<char*>(aString));
  return SVFindCharInSet<PRUnichar,char>(mStringValue,theStringValue,PR_FALSE,mStringValue.mLength,anOffset);
}

/**
 *  This method finds the offset of the first char in this string that is
 *  a member of the given altcharset, starting the search at anOffset
 *  
 *  @update  rickg 03.01.2000
 *  @param   aString contains set of chars to be found
 *  @param   anOffset -- where in this string to start searching
 *  @return  
 */
PRInt32 nsString::FindCharInSet(const PRUnichar *aString,PRUint32 anOffset) const{

  nsStringValueImpl<PRUnichar> theStringValue(const_cast<PRUnichar*>(aString));
  return SVFindCharInSet<PRUnichar,PRUnichar>(mStringValue,theStringValue,PR_FALSE,mStringValue.mLength,anOffset);
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
PRInt32 nsString::FindChar(char aChar,PRBool aIgnoreCase,PRInt32 anOffset,PRInt32 aCount) const {
  return SVFindChar<PRUnichar>(mStringValue,aChar,aIgnoreCase,aCount,anOffset);
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
PRInt32 nsString::RFindChar(char aChar,PRBool aIgnoreCase,PRInt32 anOffset,PRInt32 aRepCount) const{
  return SVRFindChar<PRUnichar>(mStringValue,aChar,aIgnoreCase,aRepCount,anOffset);
}

/**
 *  Reverse search for given string within this string
 *  
 *  @update  gess 3/25/98
 *  @param   aString - substr to be found
 *  @param   aIgnoreCase tells us whether or not to do caseless compare
 *  @param   anOffset tells us where in this string to start searching
 *  @param   aRepCount tells us how many iterations to make starting at the given offset
 *  @return  offset in string, or -1 (kNotFound)
 */
PRInt32 nsString::RFind(const nsString& aString,PRBool aIgnoreCase,PRInt32 anOffset,PRInt32 aRepCount) const{
  PRInt32 theMaxPos = mStringValue.mLength-aString.mStringValue.mLength;  //this is the last pos that is feasible for starting the search, with given lengths...
  PRInt32 result=kNotFound;

  if(0<=theMaxPos) {
    result=SVRFind<PRUnichar,PRUnichar>(mStringValue,aString.mStringValue,aIgnoreCase,aRepCount,anOffset);
  }
  return result;
}

PRInt32 nsString::RFind(const char* aString,PRBool aIgnoreCase,PRInt32 anOffset,PRInt32 aRepCount) const{
  nsStringValueImpl<char> theStringValue(const_cast<char*>(aString));
  return SVRFind<PRUnichar,char>(mStringValue,theStringValue,aIgnoreCase,aRepCount,anOffset);
}
  

PRInt32 nsString::RFind(const PRUnichar* aString,PRBool aIgnoreCase,PRInt32 anOffset,PRInt32 aRepCount) const{
  nsStringValueImpl<PRUnichar> theStringValue(const_cast<PRUnichar*>(aString));
  return SVRFind<PRUnichar,PRUnichar>(mStringValue,theStringValue,aIgnoreCase,aRepCount,anOffset);
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
PRInt32 nsString::RFindCharInSet(const nsString& aString,PRInt32 anOffset) const {
  return SVRFindCharInSet<PRUnichar,PRUnichar>(mStringValue,aString.mStringValue,PR_FALSE,mStringValue.mLength,anOffset);
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
PRInt32 nsString::RFindCharInSet(const PRUnichar* aString,PRInt32 anOffset) const{
  nsStringValueImpl<PRUnichar> theStringValue(const_cast<PRUnichar*>(aString));
  return SVRFindCharInSet<PRUnichar,PRUnichar>(mStringValue,theStringValue,PR_FALSE,mStringValue.mLength,anOffset);
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
PRInt32 nsString::RFindCharInSet(const char* aString,PRInt32 anOffset) const{
  nsStringValueImpl<char> theStringValue(const_cast<char*>(aString));
  return SVRFindCharInSet<PRUnichar,char>(mStringValue,theStringValue,PR_FALSE,mStringValue.mLength,anOffset);
}

/***************************************
  These convert to a different type
 ***************************************/

void nsString::Recycle(nsString* aString) {
  //NOT IMPLEMENTED
}

nsString* nsString::CreateString(void) {
  return 0; //NOT IMPLEMENTED
}

/**
 * This method constructs a new nsString is a clone of this string.
 */
nsString* nsString::ToNewString() const {
  return 0; //NOT IMPLEMENTED
}


char* nsString::ToNewCString() const {
  return 0; //NOT IMPLEMENTED
}

char* nsString::ToNewUTF8String() const {
  return 0; //NOT IMPLEMENTED
}

PRUnichar* nsString::ToNewUnicode() const {
  return 0; //NOT IMPLEMENTED
}

char* nsString::ToCString(char* aBuf,PRUint32 aBufLength,PRUint32 anOffset) const {
  int len=(mStringValue.mLength<aBufLength) ? mStringValue.mLength : aBufLength;
  for(int i=0;i<len;i++){
    aBuf[i]=(char)mStringValue.mBuffer[i];
  }
  aBuf[len]=0;
  return aBuf;
}


PRInt32 nsString::CountChar(PRUnichar aChar) {  
  return SVCountChar<PRUnichar>(mStringValue,aChar);
}

  //******************************************
  // Utility methods...
  //******************************************

  
  //This will not work correctly for any unicode set other than ascii
void nsString::DebugDump(void) const { 
  SVDebugDump<PRUnichar>(mStringValue);
}

/**
 * Perform string to float conversion.
 * @param   aErrorCode will contain error if one occurs
 * @return  float rep of string value
 */
float nsString::ToFloat(PRInt32* aErrorCode) const {
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
PRInt32 nsString::ToInteger(PRInt32* anErrorCode,PRUint32 aRadix) const{
  return SVToInteger(mStringValue,anErrorCode,aRadix);
}

nsresult nsString::Append(const nsStringValueImpl<char> &aString,PRInt32 aLength,PRUint32 aSrcOffset) {
  aLength = (aLength<0) ? aString.mLength : MinInt(aLength,aString.mLength);
  return SVAppend< PRUnichar, char> (mStringValue,aString,aLength,aSrcOffset);
}

