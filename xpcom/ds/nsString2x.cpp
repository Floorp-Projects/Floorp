
#include "nsString2.h"
#include "nsString.h"
#include "nsBufferManager.h"
#include <stdio.h>  
#include <stdlib.h>  
#include "nsIAllocator.h"

static const char* kNullPointerError = "Unexpected null pointer";
static const char* kWhitespace="\b\t\r\n ";

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

  //call this version for a single char of type char...
nsString::nsString(const nsSubsumeStr &aSubsumeString) : mStringValue(aSubsumeString.mLHS) {
  SVAppend(mStringValue,aSubsumeString.mRHS,aSubsumeString.mRHS.mLength,0);
}

  //call this version for a single char of type char...
nsString::nsString(nsSubsumeStr &aSubsumeString) : mStringValue(aSubsumeString.mLHS) {
  SVAppend(mStringValue,aSubsumeString.mRHS,aSubsumeString.mRHS.mLength,0);
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
  nsStringValueImpl<char> theString(CONST_CAST(char*,aString),aLength);
  SVAssign(mStringValue,theString,theString.mLength,0);
}

  //call this version for all other ABT versions of readable strings
nsString::nsString(const nsAReadableString &aString) : mStringValue() {
  Assign(aString);
}

  //call this version for stack-based string buffers...
nsString::nsString(const nsUStackBuffer &aBuffer) : mStringValue(aBuffer) {
}

nsString::~nsString() { }

void nsString::Reinitialize(PRUnichar* aBuffer,PRUint32 aCapacity,PRInt32 aLength) {
  mStringValue.mBuffer=aBuffer;
  mStringValue.mCapacity=aCapacity;
  mStringValue.mLength=aLength;
  mStringValue.mRefCount=1;
}

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

nsString& nsString::operator=(const nsSubsumeStr &aSubsumeString) {
  //XXX NOT IMPLEMENTED
  return *this;
}


nsString& nsString::operator=(const PRUnichar *aString) {
  if(mStringValue.mBuffer!=aString) {
    nsStringValueImpl<PRUnichar> theStringValue(CONST_CAST(PRUnichar*,aString));
    Assign(aString);
  }
  return *this;
} 

nsString& nsString::operator=(const char *aString) {
  nsStringValueImpl<char> theStringValue(CONST_CAST(char*,aString));
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
    SVRealloc(mStringValue,aCapacity);
  }
  return NS_OK;
}


PRBool nsString::SetCharAt(PRUnichar aChar,PRUint32 anIndex) {
  PRBool result=PR_FALSE;
  if(anIndex<mStringValue.mLength){
    mStringValue.mBuffer[anIndex]=(char)aChar;
    result=PR_TRUE;
  }
  return result;
}


  //******************************************
  // Here are the Assignment methods, 
  // and other mutators...
  //******************************************

nsresult nsString::Truncate(PRUint32 anOffset) {
  SVTruncate(mStringValue,anOffset);
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
  return SVAppendReadable(mStringValue,aString,aLength,aSrcOffset);
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
  aLength = (aLength<0) ? aString.mStringValue.mLength : (aLength<(PRInt32)aString.mStringValue.mLength) ? aLength: aString.mStringValue.mLength;
  return SVAppend (mStringValue,aString.mStringValue,aLength,aSrcOffset);
}

  //append from a type compatible string pointer
nsresult nsString::Append(const char* aString,PRInt32 aLength,PRUint32 aSrcOffset) {
  nsresult result=NS_OK;
  if(aString) {
    if(aLength<0) aLength=stringlen(aString);
    nsStringValueImpl<char> theStringValue(CONST_CAST(char*,aString),aLength);
    result=SVAppend(mStringValue,theStringValue,aLength,aSrcOffset);
  }
  return result;
}

  //append from alternative type string pointer
nsresult nsString::Append(const char aChar) {

  if(mStringValue.mLength==mStringValue.mCapacity) {
    SVRealloc(mStringValue,mStringValue.mCapacity+5);
  }

  if(mStringValue.mLength<mStringValue.mCapacity) {
    //an optimized code path when our buffer can easily contain the given char...
    mStringValue.mBuffer[mStringValue.mLength++]=aChar;
    mStringValue.mBuffer[mStringValue.mLength]=0;
  }

  return NS_OK;
}

  //append from alternative type string pointer
nsresult nsString::Append(const PRUnichar aChar) {

  if(mStringValue.mLength==mStringValue.mCapacity) {
    SVRealloc(mStringValue,mStringValue.mCapacity+5);
  }

  if(mStringValue.mLength<mStringValue.mCapacity) {
    //an optimized code path when our buffer can easily contain the given char...
    mStringValue.mBuffer[mStringValue.mLength++]=aChar;
    mStringValue.mBuffer[mStringValue.mLength]=0;
  }

  return NS_OK;
}

  //append from alternative type string pointer
nsresult nsString::Append(const PRUnichar* aString,PRInt32 aLength,PRUint32 aSrcOffset) {
  nsresult result=NS_OK;
  if(aString) {
    nsStringValueImpl<PRUnichar> theStringValue(CONST_CAST(PRUnichar*,aString),aLength);
    result=SVAppend (mStringValue,theStringValue,aLength,aSrcOffset);
  }
  return result;
}

  //append from an nsCString
nsresult nsString::Append(const nsCString &aString,PRInt32 aLength,PRUint32 aSrcOffset) {
  aLength = (aLength<0) ? aString.mStringValue.mLength : (aLength<(PRInt32)aString.mStringValue.mLength) ? aLength: aString.mStringValue.mLength;
  return SVAppend (mStringValue,aString.mStringValue,aLength,aSrcOffset);
}

  //append from an nsAReadableString (the ABT)
nsresult nsString::Append(const nsAReadableString &aString,PRInt32 aLength,PRUint32 aSrcOffset) {
  aLength = (aLength<0) ? aString.Length() : MinInt(aLength,aString.Length());
  return SVAppendReadable(mStringValue,aString,aLength,aSrcOffset);
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
  return (0<aCount) ? SVDelete(mStringValue,anOffset,aCount) : NS_OK;
}

nsresult nsString::Trim(const char* aTrimSet,PRBool aEliminateLeading,PRBool aEliminateTrailing,PRBool aIgnoreQuotes) {
  return SVTrim(mStringValue,aTrimSet,aEliminateLeading,aEliminateTrailing);
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
  return SVCompressSet(mStringValue,aSet,aChar,aEliminateLeading,aEliminateTrailing);
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
  return SVInsert(mStringValue,anOffset,aCopy.mStringValue,aCount,anOffset);
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

    nsStringValueImpl<char> theStringValue(CONST_CAST(char*,aString),aLength);
    
    if(0<theStringValue.mLength){
      result=SVInsert(mStringValue,anOffset,theStringValue,theStringValue.mLength,0);
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

    nsStringValueImpl<PRUnichar> theStringValue(CONST_CAST(PRUnichar*,aString),aLength);
    
    if(0<theStringValue.mLength){
      result=SVInsert(mStringValue,anOffset,theStringValue,theStringValue.mLength,0);
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

  return SVInsert(mStringValue,anOffset,theStringValue,1,0);
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
  PRUnichar *root_cp=mStringValue.mBuffer;
  PRUnichar *null_cp=root_cp+mStringValue.mLength;

  while(root_cp<null_cp) {
    if(*root_cp==anOldChar) {
      *root_cp=aNewChar;
    }
    root_cp++;
  }
  return NS_OK;
}

nsresult nsString::ReplaceChar(const char* aSet,char aNewChar){
  return SVReplaceCharsInSet(mStringValue,aSet,aNewChar,mStringValue.mLength,0);
}

nsresult nsString::ReplaceSubstring( const nsString& aTarget,const nsString& aNewValue){
  return SVReplace(mStringValue,aTarget.mStringValue,aNewValue.mStringValue,mStringValue.mLength,0);
}


nsresult nsString::ReplaceSubstring(const char* aTarget,const char* aNewValue) {
  nsresult result=NS_OK;

  const nsStringValueImpl<char> theTarget(CONST_CAST(char*,aTarget));
  const nsStringValueImpl<char> theNewValue(CONST_CAST(char*,aNewValue));

  return SVReplace(mStringValue,theTarget,theNewValue,mStringValue.mLength,0);
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
nsresult nsString::StripChar(PRUnichar aChar,PRUint32 anOffset){
  return SVStripChar(mStringValue,aChar,mStringValue.mLength,anOffset);
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
  return SVStripChar(mStringValue,(PRUnichar)anInt,mStringValue.mLength,anOffset);
}

/**
 *  This method is used to remove all occurances of the
 *  characters found in aSet from this string.
 *  
 *  @update  rickg 03.01.2000
 *  @param  aSet -- characters to be cut from this
 *  @return *this 
 */
nsresult nsString::StripChars(const char* aSet,PRInt32 aLength){
  nsStringValueImpl<char> theSet(CONST_CAST(char*,aSet),aLength);
  return SVStripCharsInSet(mStringValue,aSet,mStringValue.mLength,0);
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
  aCount = (aCount<0) ? mStringValue.mLength : (aCount<(PRInt32)mStringValue.mLength) ? aCount : mStringValue.mLength;
  return aCopy.Assign(*this,aCount,0);
}

nsresult nsString::Mid(nsString& aCopy,PRUint32 anOffset,PRInt32 aCount) const {
  aCount = (aCount<0) ? mStringValue.mLength : (aCount<(PRInt32)mStringValue.mLength) ? aCount : mStringValue.mLength;
  return aCopy.Assign(*this,aCount,anOffset);
}

nsresult nsString::Right(nsString& aCopy,PRInt32 aCount) const {
  PRUint32 theLen=mStringValue.mLength-aCount;
  PRInt32 offset=(theLen<0) ? 0 : theLen;
  return Mid(aCopy,offset,aCount);
}

  //******************************************
  // Concatenation operators
  //******************************************

nsSubsumeStr nsString::operator+(const nsString &aString) {
  nsSubsumeStr result(mStringValue,aString.mStringValue);
  return result;
}

nsSubsumeStr nsString::operator+(const PRUnichar* aString) {
  nsSubsumeStr result;  //NOT IMPLEMENTED
  return result;
}

nsSubsumeStr nsString::operator+(const char* aCString) {
  nsSubsumeStr result;  //NOT IMPLEMENTED
  return result;
}

nsSubsumeStr nsString::operator+(PRUnichar aChar) {
  nsSubsumeStr result;  //NOT IMPLEMENTED
  return result;
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
  SVToLowerCase(mStringValue);
}

void nsString::ToLowerCase(nsString &aString) const {
  aString=*this;
  aString.ToLowerCase();
}

void nsString::ToUpperCase() {
  SVToUpperCase(mStringValue);
}

void nsString::ToUpperCase(nsString &aString) const {
  aString=*this;
  aString.ToUpperCase();
}


PRInt32  nsString::Compare(const nsString &aString,PRBool aIgnoreCase,PRInt32 aCount) const {
  return SVCompareChars(mStringValue,aString.mStringValue,aIgnoreCase,aCount);
}

PRInt32  nsString::Compare(const nsCString &aString,PRBool aIgnoreCase,PRInt32 aCount) const {
  return SVCompare(mStringValue,aString.mStringValue,aIgnoreCase,aCount);
}

PRInt32  nsString::Compare(const char* aString,PRBool aIgnoreCase,PRInt32 aCount) const {
  nsStringValueImpl<char> theStringValue(CONST_CAST(char*,aString),aCount);
  return SVCompare(mStringValue,theStringValue,aIgnoreCase,aCount);
}

PRInt32  nsString::Compare(const PRUnichar* aString,PRBool aIgnoreCase,PRInt32 aCount) const {
  nsStringValueImpl<PRUnichar> theStringValue(CONST_CAST(PRUnichar*,aString),aCount);
  return SVCompareChars(mStringValue,theStringValue,aIgnoreCase,aCount);
}

PRBool  nsString::Equals(const nsString &aString,PRBool aIgnoreCase,PRInt32 aCount) const {
  return PRBool(0==SVCompare(mStringValue,aString.mStringValue,aIgnoreCase,aCount));
}

PRBool  nsString::Equals(const nsCString &aString,PRBool aIgnoreCase,PRInt32 aCount) const {
  return PRBool(0==SVCompare(mStringValue,aString.mStringValue,aIgnoreCase,aCount));
}

PRBool  nsString::Equals(const char* aString,PRBool aIgnoreCase,PRInt32 aCount) const {
  nsStringValueImpl<char> theStringValue(CONST_CAST(char*,aString),aCount);
  return PRBool(0==SVCompare(mStringValue,theStringValue,aIgnoreCase,aCount));
}


PRBool  nsString::Equals(const PRUnichar* aString,PRBool aIgnoreCase,PRInt32 aCount) const {
  nsStringValueImpl<PRUnichar> theStringValue(CONST_CAST(PRUnichar*,aString),aCount);
  return PRBool(0==SVCompareChars(mStringValue,theStringValue,aIgnoreCase,aCount));
}

PRBool nsString::Equals(const PRUnichar* aLHS,const PRUnichar* aRHS,PRBool aIgnoreCase) const {
  NS_ASSERTION(0!=aLHS,kNullPointerError);
  NS_ASSERTION(0!=aRHS,kNullPointerError);
  PRBool  result=PR_FALSE;
  if((aLHS) && (aRHS)){


//    PRInt32 cmp=(aIgnoreCase) ? nsCRT::strcasecmp(aLHS,aRHS) : nsCRT::strcmp(aLHS,aRHS);
//    result=PRBool(0==cmp);


  }

  return result;
}

PRBool nsString::Equals(/*FIX: const */nsIAtom* aAtom,PRBool aIgnoreCase) const{
  NS_ASSERTION(0!=aAtom,kNullPointerError);
  PRBool result=PR_FALSE;

#if 0

  if(aAtom){
    PRInt32 cmp=0;
    const PRUnichar* unicode;
    if (aAtom->GetUnicode(&unicode) != NS_OK || unicode == nsnull)
        return PR_FALSE;
    if (aIgnoreCase)
      cmp=nsCRT::strcasecmp(mUStr,unicode);
    else
      cmp=nsCRT::strcmp(mUStr,unicode);
    result=PRBool(0==cmp);
  }
#endif
  return result;
}


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
  return SVFind(mStringValue,aTarget.mStringValue,aIgnoreCase,aRepCount,anOffset);
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
  return SVFind(mStringValue,aString.mStringValue,aIgnoreCase,aRepCount,anOffset);
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
  nsStringValueImpl<char> theString(CONST_CAST(char*,aString));
  return SVFind(mStringValue,theString,aIgnoreCase,aRepCount,anOffset);
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

  nsStringValueImpl<PRUnichar> theString(CONST_CAST(PRUnichar*,aString));
  return SVFind(mStringValue,theString,aIgnoreCase,aRepCount,anOffset);
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
  return SVFindCharInSet(mStringValue,aString.mStringValue,PR_FALSE,mStringValue.mLength,anOffset);
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

  nsStringValueImpl<char> theStringValue(CONST_CAST(char*,aString));
  return SVFindCharInSet(mStringValue,theStringValue,PR_FALSE,mStringValue.mLength,anOffset);
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

  nsStringValueImpl<PRUnichar> theStringValue(CONST_CAST(PRUnichar*,aString));
  return SVFindCharInSet(mStringValue,theStringValue,PR_FALSE,mStringValue.mLength,anOffset);
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
  return SVFindChar(mStringValue,aChar,aIgnoreCase,aCount,anOffset);
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
  return SVRFindChar(mStringValue,aChar,aIgnoreCase,aRepCount,anOffset);
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
    result=SVRFind(mStringValue,aString.mStringValue,aIgnoreCase,aRepCount,anOffset);
  }
  return result;
}

PRInt32 nsString::RFind(const char* aString,PRBool aIgnoreCase,PRInt32 anOffset,PRInt32 aRepCount) const{
  nsStringValueImpl<char> theStringValue(CONST_CAST(char*,aString));
  return SVRFind(mStringValue,theStringValue,aIgnoreCase,aRepCount,anOffset);
}
  

PRInt32 nsString::RFind(const PRUnichar* aString,PRBool aIgnoreCase,PRInt32 anOffset,PRInt32 aRepCount) const{
  nsStringValueImpl<PRUnichar> theStringValue(CONST_CAST(PRUnichar*,aString));
  return SVRFind(mStringValue,theStringValue,aIgnoreCase,aRepCount,anOffset);
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
  return SVRFindCharInSet(mStringValue,aString.mStringValue,PR_FALSE,mStringValue.mLength,anOffset);
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
  nsStringValueImpl<PRUnichar> theStringValue(CONST_CAST(PRUnichar*,aString));
  return SVRFindCharInSet(mStringValue,theStringValue,PR_FALSE,mStringValue.mLength,anOffset);
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
  nsStringValueImpl<char> theStringValue(CONST_CAST(char*,aString));
  return SVRFindCharInSet(mStringValue,theStringValue,PR_FALSE,mStringValue.mLength,anOffset);
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
  return SVCountChar(mStringValue,aChar);
}

  //******************************************
  // Utility methods...
  //******************************************

  
  //This will not work correctly for any unicode set other than ascii
void nsString::DebugDump(void) const { 
  SVDebugDump(mStringValue);
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
  float f = (float) strtod(cp, &cp);
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
  aLength = (aLength<0) ? aString.mLength : (aLength<(PRInt32)aString.mLength) ? aLength: aString.mLength;
  return SVAppend(mStringValue,aString,aLength,aSrcOffset);
}

/*****************************************************************
  Now we declare the nsCSubsumeString class
 *****************************************************************/


nsSubsumeStr::nsSubsumeStr() {
}


nsSubsumeStr::nsSubsumeStr(const nsString& aString) : mLHS(aString.mStringValue), mRHS() {
}


nsSubsumeStr::nsSubsumeStr(const nsSubsumeStr& aSubsumeString) : 
  mLHS(aSubsumeString.mLHS), 
  mRHS(aSubsumeString.mRHS) {
}


nsSubsumeStr::nsSubsumeStr(const nsStringValueImpl<PRUnichar> &aLHS,const nsSubsumeStr& aSubsumeString) : 
  mLHS(aLHS), 
  mRHS(aSubsumeString.mLHS) {
}


nsSubsumeStr::nsSubsumeStr(const nsStringValueImpl<PRUnichar> &aLHS,const nsStringValueImpl<PRUnichar> &aRHS) : 
  mLHS(aLHS), 
  mRHS(aRHS) {
}

nsSubsumeStr::nsSubsumeStr(PRUnichar* aString,PRBool assumeOwnership,PRInt32 aLength) {
}

nsSubsumeStr nsSubsumeStr::operator+(const nsSubsumeStr &aSubsumeString) {
  nsSubsumeStr result(*this);
  return result;
}


nsSubsumeStr nsSubsumeStr::operator+(const nsString &aString) {
  SVAppend(mLHS,mRHS,mRHS.mLength,0);
  memcpy(&mRHS,&aString.mStringValue,sizeof(mRHS));
  return *this;
}

int nsSubsumeStr::Subsume(PRUnichar* aString,PRBool assumeOwnership,PRInt32 aLength) {
  //XXX NOT IMPLEMENTED
  int result=0;
  return result;
}

/**
 * 
 * @update  gess 01/04/99
 * @param 
 * @return
 */
NS_COM int fputs(const nsString& aString, FILE* out)
{
  char buf[100];
  char* cp = buf;
  PRInt32 len = aString.Length();
  if (len >= PRInt32(sizeof(buf))) {
    cp = aString.ToNewCString();
  } else {
    aString.ToCString(cp, len + 1);
  }
  if(len>0)
    ::fwrite(cp, 1, len, out);
  if (cp != buf) {
    delete[] cp;
  }
  return (int) len;
}

PRUint32 HashCode(const nsString& aDest) {
  PRUint32 h = 0;
  PRUint32 n = aDest.Length();
  PRUint32 m;
  const PRUnichar* c=aDest.GetUnicode();

  if (n < 16) {	/* Hash every char in a short string. */
    for(; n; c++, n--)
      h = (h >> 28) ^ (h << 4) ^ *c;
  }
  else {	/* Sample a la java.lang.String.hash(). */
    for(m = n / 8; n >= m; c += m, n -= m)
      h = (h >> 28) ^ (h << 4) ^ *c;
  }
  return h; 
}

void Recycle( PRUnichar* aBuffer) { 
  nsAllocator::Free(aBuffer); 
}


/*****************************************************************
  Now we declare the nsAutoString class
 *****************************************************************/


nsAutoString::nsAutoString() : nsString() {
  memset(mInternalBuffer,0,sizeof(mInternalBuffer));
  mStringValue.mCapacity=kDefaultStringSize;
  mStringValue.mBuffer=mInternalBuffer;
  mStringValue.mRefCount++; //so we don't try to delete our internal buffer
}

  //call this version nsAutoString derivatives...
nsAutoString::nsAutoString(const nsAutoString& aString,PRInt32 aLength) : nsString() {
  memset(mInternalBuffer,0,sizeof(mInternalBuffer));
  mStringValue.mCapacity=kDefaultStringSize;
  mStringValue.mBuffer=mInternalBuffer;
  mStringValue.mRefCount++; //so we don't try to delete our internal buffer
  Assign(aString,aLength);
}

//call this version for nsString and the autostrings
nsAutoString::nsAutoString(const nsString& aString,PRInt32 aLength) : nsString() {
  memset(mInternalBuffer,0,sizeof(mInternalBuffer));
  mStringValue.mCapacity=kDefaultStringSize;
  mStringValue.mBuffer=mInternalBuffer;
  mStringValue.mRefCount++; //so we don't try to delete our internal buffer
  Assign(aString,aLength);
}

  //call this version with nsCString
nsAutoString::nsAutoString(const nsCString& aString) : nsString(aString) {
  mStringValue.mRefCount++; //so we don't try to delete our internal buffer
}

  //call this version for char*'s....
nsAutoString::nsAutoString(const char* aString,PRInt32 aLength) : nsString() {
  memset(mInternalBuffer,0,sizeof(mInternalBuffer));

  nsStringValueImpl<char> theString(CONST_CAST(char*,aString),aLength);
  mStringValue.mCapacity=kDefaultStringSize;
  mStringValue.mBuffer=mInternalBuffer;
  mStringValue.mRefCount++; //so we don't try to delete our internal buffer
  SVAssign(mStringValue,theString,aLength,0);
}

  //call this version for a single char of type char...
nsAutoString::nsAutoString(char aChar) : nsString() {

  memset(mInternalBuffer,0,sizeof(mInternalBuffer));
  mStringValue.mCapacity=kDefaultStringSize;
  mStringValue.mBuffer=mInternalBuffer;
  mStringValue.mRefCount++; //so we don't try to delete our internal buffer

  PRUnichar theBuffer[]={aChar,0};

  Assign(theBuffer,1,0);
}

  //call this version for a single char of type char...
nsAutoString::nsAutoString(PRUnichar aChar) : nsString() {

  memset(mInternalBuffer,0,sizeof(mInternalBuffer));
  mStringValue.mCapacity=kDefaultStringSize;
  mStringValue.mBuffer=mInternalBuffer;
  mStringValue.mRefCount++; //so we don't try to delete our internal buffer

  PRUnichar theBuffer[]={aChar,0};

  Assign(theBuffer,1,0);
}


  //call this version for PRUnichar*'s....
nsAutoString::nsAutoString(const PRUnichar* aString,PRInt32 aLength) : nsString() {
  memset(mInternalBuffer,0,sizeof(mInternalBuffer));
  mStringValue.mCapacity=kDefaultStringSize;
  mStringValue.mBuffer=mInternalBuffer;
  nsStringValueImpl<PRUnichar> theString(CONST_CAST(PRUnichar*,aString),aLength);
  mStringValue.mRefCount++; //so we don't try to delete our internal buffer
  SVAssign(mStringValue,theString,theString.mLength,0);
}


  //call this version for all other ABT versions of readable strings
nsAutoString::nsAutoString(const nsAReadableString &aString) : nsString() {
  memset(mInternalBuffer,0,sizeof(mInternalBuffer));
  mStringValue.mCapacity=kDefaultStringSize;
  mStringValue.mBuffer=mInternalBuffer;
  mStringValue.mRefCount++; //so we don't try to delete our internal buffer
  Assign(aString);
}

nsAutoString::nsAutoString(const nsUStackBuffer &aBuffer) : nsString() {

  memset(mInternalBuffer,0,sizeof(mInternalBuffer));

  mStringValue.mLength=aBuffer.mLength;
  mStringValue.mCapacity=aBuffer.mCapacity;
  mStringValue.mBuffer=aBuffer.mBuffer;
  mStringValue.mRefCount++; //so we don't try to delete our internal buffer
  
}

nsAutoString::nsAutoString(const CBufDescriptor& aBuffer) : nsString() {
  mStringValue.mBuffer=(PRUnichar*)aBuffer.mBuffer;
  mStringValue.mCapacity=aBuffer.mCapacity;
  mStringValue.mLength=aBuffer.mLength;
  mStringValue.mRefCount=(aBuffer.mStackBased) ? 2 : 1;
}

nsAutoString::nsAutoString(const nsSubsumeStr& aSubsumeStringX) : nsString() {
}


nsAutoString::~nsAutoString() { }


nsAutoString& nsAutoString::operator=(const nsAutoString& aCopy) {
  if(aCopy.mStringValue.mBuffer!=mStringValue.mBuffer) {
    Assign(aCopy);
  }
  return *this;
}

nsAutoString& nsAutoString::operator=(const nsString& aString) {
  Assign(aString);
  return *this;
}

nsAutoString& nsAutoString::operator=(const nsCString& aString) {
  Assign(aString);
  return *this;
}

nsAutoString& nsAutoString::operator=(const PRUnichar* aString) {
  if(mStringValue.mBuffer!=aString) {
    nsStringValueImpl<PRUnichar> theStringValue(CONST_CAST(PRUnichar*,aString));
    Assign(aString);
  }
  return *this;
} 

nsAutoString& nsAutoString::operator=(const char* aString) {
  nsStringValueImpl<char> theStringValue(CONST_CAST(char*,aString));
  SVAssign(mStringValue,theStringValue,theStringValue.mLength,0);
  return *this;
}

nsAutoString& nsAutoString::operator=(const PRUnichar aChar) {
  Assign(aChar);
  return *this;
} 


nsAutoString& nsAutoString::operator=(const nsSubsumeStr &aSubsumeString) {
  nsString::operator=(aSubsumeString);
  return *this;
}

