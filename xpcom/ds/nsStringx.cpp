
#include "nsString.h"
#include "nsBufferManager.h"
#include <stdio.h>  
#include <stdlib.h>  
#include "nsIAllocator.h"

static const char* kWhitespace="\b\t\r\n ";


void Subsume(nsStringValueImpl<char> &aDest,nsStringValueImpl<char> &aSource){
  if(aSource.mBuffer && aSource.mLength) {
#if 0
    if(aSource.mOwnsBuffer){
      nsStr::Destroy(aDest);
      aDest.mStr=aSource.mStr;
      aDest.mLength=aSource.mLength;
      aDest.mCapacity=aSource.mCapacity;
      aDest.mOwnsBuffer=aSource.mOwnsBuffer;
      aSource.mOwnsBuffer=PR_FALSE;
      aSource.mStr=0;
    }
    else{ 
      nsStr::StrAssign(aDest,aSource,0,aSource.mLength);
    }
#endif
  } 
  else SVTruncate(aDest,0);
}


  //******************************************
  // Ctor's
  //******************************************

nsCString::nsCString() : mStringValue() {
}
  
  //call this to copy construct nsCString
nsCString::nsCString(const nsCString& aString,PRInt32 aLength) : mStringValue() {
  SVAppend(mStringValue,aString.mStringValue,aLength,0);
}

  //call this to construct from nsString
nsCString::nsCString(const nsString& aString,PRInt32 aLength) : mStringValue() {
  SVAppend(mStringValue,aString.mStringValue,aLength,0);
}

  //call this version for char*'s....
nsCString::nsCString(const char* aString,PRInt32 aLength) : mStringValue() {
  Append(aString,aLength);
}

  //call this version for PRUnichar*'s....
nsCString::nsCString(const PRUnichar* aString,PRInt32 aLength) : mStringValue() {
  Append(aString,aLength,0);
}

  //call this version for a single char of type char...
nsCString::nsCString(nsSubsumeCStr &aSubsumeString) : mStringValue() {
  mStringValue=aSubsumeString.mStringValue;
  memset(&aSubsumeString.mStringValue,0,sizeof(mStringValue));
}


  //call this version for a single char of type char...
nsCString::nsCString(const char aChar) : mStringValue() {
  Append(aChar);
}

  //call this version for all other ABT versions of readable strings
nsCString::nsCString(const nsAReadableString &aString) : mStringValue() {
  Assign(aString);
}

nsCString::~nsCString() { 
  SVFree(mStringValue);
}


void nsCString::Reinitialize(char* aBuffer,PRUint32 aCapacity,PRInt32 aLength) {
  mStringValue.mBuffer=aBuffer;
  mStringValue.mCapacity=aCapacity;
  mStringValue.mLength=aLength;
  mStringValue.mRefCount=1;
}


  //******************************************
  // Concatenation operators
  //******************************************

nsSubsumeCStr nsCString::operator+(const nsCString &aCString) {
  nsCString theString(*this);
  SVAppend(theString.mStringValue,aCString.mStringValue);
  return nsSubsumeCStr(theString); 
}

nsSubsumeCStr nsCString::operator+(const char* aString) {
  nsCString theTempString(*this);
  nsStringValueImpl<char> theTempStrValue(NS_CONST_CAST(char*,aString));
  SVAppend(theTempString.mStringValue,theTempStrValue);
  return nsSubsumeCStr(theTempString); 
}

nsSubsumeCStr nsCString::operator+(char aChar) {
  nsCString theTempString(*this);
  theTempString.Append(aChar);
  return nsSubsumeCStr(theTempString); 
}

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
    nsStringValueImpl<char> theStringValue(NS_CONST_CAST(char*,aString));
    SVAssign(mStringValue,theStringValue,theStringValue.mLength,0);
  }
  return *this;
} 

nsCString& nsCString::operator=(const PRUnichar *aString) {
  nsStringValueImpl<PRUnichar> theStringValue(NS_CONST_CAST(PRUnichar*,aString));
  Assign(aString);
  return *this;
}

nsCString& nsCString::operator=(const char aChar) {
  Assign(aChar);
  return *this;
} 

nsCString& nsCString::operator=(const nsSubsumeCStr &aSubsumeString) {
  mStringValue=aSubsumeString.mStringValue;
  memset((void*)&aSubsumeString.mStringValue,0,sizeof(mStringValue));
  return *this;
}


  //******************************************
  // Here are the accessor methods...
  //******************************************


nsresult nsCString::SetCapacity(PRUint32 aCapacity) {
  if(aCapacity>mStringValue.mCapacity) {
    SVRealloc(mStringValue,aCapacity);
  }
  return NS_OK;
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


  //******************************************
  // Here are the Assignment methods, 
  // and other mutators...
  //******************************************

nsresult nsCString::Truncate(PRUint32 anOffset) {
  SVTruncate(mStringValue,anOffset);
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
  nsresult result=SVAssign(mStringValue,aString.mStringValue,aLength,aSrcOffset);
  return result;
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
nsresult nsCString::Assign(const nsString& aString,PRInt32 aLength,PRUint32 aSrcOffset) {
  nsresult result=SVAssign(mStringValue,aString.mStringValue,aLength,aSrcOffset);
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
nsresult nsCString::Assign(const PRUnichar* aString,PRInt32 aLength,PRUint32 aSrcOffset) {
  Truncate();
  return Append(aString,aLength,aSrcOffset);
}

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
  return SVAppendReadable(mStringValue,aString,aLength,aSrcOffset);
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
  return SVAppend (mStringValue,aString.mStringValue,aLength,aSrcOffset);
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
  return SVAppend(mStringValue,aString.mStringValue,aString.mStringValue.mLength,aSrcOffset);
}

  //append from a char*
nsresult nsCString::Append(const char* aString,PRInt32 aLength,PRUint32 aSrcOffset) {
  nsresult result=NS_OK;
  if(aString) {
    nsStringValueImpl<char> theStringValue(NS_CONST_CAST(char*,aString),aLength);
    result=SVAppend(mStringValue,theStringValue,aLength,aSrcOffset);
  }
  return result;
}

  //append from a char
nsresult nsCString::Append(const char aChar) {

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

  //append from a PRUnichar
nsresult nsCString::Append(const PRUnichar aChar) {

  if(mStringValue.mLength<mStringValue.mCapacity) {
    //an optimized code path when our buffer can easily contain the given char...
    if(aChar<256) {
      mStringValue.mBuffer[mStringValue.mLength++]=(unsigned char)aChar;
    }
    else {
      mStringValue.mBuffer[mStringValue.mLength++]='.';  //XXX HACK!
    }
    mStringValue.mBuffer[mStringValue.mLength]=0;
    return NS_OK;
  }

  PRUnichar theBuffer[]={aChar,0};
  nsStringValueImpl<PRUnichar> theStringValue(theBuffer,1);
  return SVAppend(mStringValue,theStringValue,1,0);
}

  //append from a PRUnichar*
nsresult nsCString::Append(const PRUnichar* aString,PRInt32 aLength,PRUint32 aSrcOffset) {
  nsresult result=NS_OK;
  if(aString) {
    nsStringValueImpl<PRUnichar> theStringValue(NS_CONST_CAST(PRUnichar*,aString),aLength);
    result=SVAppend(mStringValue,theStringValue,aLength,aSrcOffset);
  }
  return result;
}

  //append from an nsAReadableString (the ABT)
nsresult nsCString::Append(const nsAReadableString &aString,PRInt32 aLength,PRUint32 aSrcOffset) {
  return SVAppendReadable(mStringValue,aString,aLength,aSrcOffset);
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

  Append(buf);

  return result;
}


  //***************************************
  //  Here come a few deletion methods...
  //***************************************

nsresult nsCString::Cut(PRUint32 anOffset,PRInt32 aCount) {
  return (0<aCount) ? SVDelete(mStringValue,anOffset,aCount) : NS_OK;
}

nsresult nsCString::Trim(const char* aTrimSet,PRBool aEliminateLeading,PRBool aEliminateTrailing,PRBool aIgnoreQuotes) {
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
nsresult nsCString::CompressSet(const char* aSet, char aChar,PRBool aEliminateLeading,PRBool aEliminateTrailing){
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
nsresult nsCString::CompressWhitespace( PRBool aEliminateLeading,PRBool aEliminateTrailing){
  return CompressSet(kWhitespace,' ',aEliminateLeading,aEliminateTrailing);
}


  //***************************************
  //  Here come a wad of insert methods...
  //***************************************

nsresult nsCString::Insert(const nsCString& aString,PRUint32 anOffset,PRInt32 aCount) {
  return SVInsert(mStringValue,anOffset,aString.mStringValue,aCount,anOffset);
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

    nsStringValueImpl<char> theStringValue(NS_CONST_CAST(char*,aString),aLength);
    
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
nsresult nsCString::Insert(char aChar,PRUint32 anOffset){
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
nsresult nsCString::ReplaceChar(char anOldChar,char aNewChar){
  char *root_cp=mStringValue.mBuffer;
  char *null_cp=root_cp+mStringValue.mLength;

  while(root_cp<null_cp) {
    if(*root_cp==anOldChar) {
      *root_cp=aNewChar;
    }
    root_cp++;
  }
  return NS_OK;
}

nsresult nsCString::ReplaceChar(const char* aSet,char aNewChar){
  return SVReplaceCharsInSet(mStringValue,aSet,aNewChar,mStringValue.mLength,0);
}

nsresult nsCString::ReplaceSubstring( const nsCString& aTarget,const nsCString& aNewValue){
  return SVReplace(mStringValue,aTarget.mStringValue,aNewValue.mStringValue,mStringValue.mLength,0);
}


nsresult nsCString::ReplaceSubstring(const char* aTarget,const char* aNewValue) {
  nsresult result=NS_OK;

  const nsStringValueImpl<char> theTarget(NS_CONST_CAST(char*,aTarget));
  const nsStringValueImpl<char> theNewValue(NS_CONST_CAST(char*,aNewValue));

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
nsresult nsCString::StripChar(char aChar,PRUint32 anOffset){
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
nsresult nsCString::StripChar(PRInt32 anInt,PRUint32 anOffset){
  return SVStripChar(mStringValue,(char)anInt,mStringValue.mLength,anOffset);
}

/**
 *  This method is used to remove all occurances of the
 *  characters found in aSet from this string.
 *  
 *  @update  rickg 03.01.2000
 *  @param  aSet -- characters to be cut from this
 *  @return *this 
 */
nsresult nsCString::StripChars(const char* aSet,PRInt32 aLength){
  nsStringValueImpl<char> theSet(NS_CONST_CAST(char*,aSet),aLength);
  return SVStripCharsInSet(mStringValue,aSet,mStringValue.mLength,0);
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
  aCount = (aCount<0) ? mStringValue.mLength : (aCount<(PRInt32)mStringValue.mLength) ? aCount : mStringValue.mLength;
  return aCopy.Assign(*this,aCount,0);
}

nsresult nsCString::Mid(nsCString& aCopy,PRUint32 anOffset,PRInt32 aCount) const {
  aCount = (aCount<0) ? mStringValue.mLength : (aCount<(PRInt32)mStringValue.mLength) ? aCount : mStringValue.mLength;
  return aCopy.Assign(*this,aCount,anOffset);
}

nsresult nsCString::Right(nsCString& aCopy,PRInt32 aCount) const {
  PRUint32  theLen=mStringValue.mLength-aCount;
  PRInt32   offset=(theLen<0) ? 0 : theLen;
  return Mid(aCopy,offset,aCount);
}


  //*******************************************
  //  Here come the operator+=() methods...
  //*******************************************


nsCString& nsCString::operator+=(const char* aString) {
  Append(aString);
  return *this;
}

nsCString& nsCString::operator+=(const PRUnichar* aString) {
  Append(aString);
  return *this;
}

nsCString& nsCString::operator+=(const nsCString& aCString) {
  Append(aCString);
  return *this;
}

nsCString& nsCString::operator+=(const char aChar) {
  char theBuffer[]={aChar,0};
  Append(theBuffer,1);
  return *this;
}

nsCString& nsCString::operator+=(const PRUnichar aChar) {
  PRUnichar theBuffer[]={aChar,0};
  Append(theBuffer,1);
  return *this;
}

nsCString& nsCString::operator+=(const int anInt){
  Append(anInt,10);
  return *this;
}

void nsCString::ToLowerCase() {
  SVToLowerCase(mStringValue);
}

void nsCString::ToLowerCase(nsCString &aString) const {
  aString=*this;
  aString.ToLowerCase();
}

void nsCString::ToUpperCase() {
  SVToUpperCase(mStringValue);
}

void nsCString::ToUpperCase(nsCString &aString) const {
  aString=*this;
  aString.ToUpperCase();
}


PRInt32  nsCString::Compare(const nsCString &aString,PRBool aIgnoreCase,PRInt32 aCount) const {
  return SVCompareChars(mStringValue,aString.mStringValue,aIgnoreCase,aCount);
}

PRInt32  nsCString::Compare(const char* aString,PRBool aIgnoreCase,PRInt32 aCount) const {
  nsStringValueImpl<char> theStringValue(NS_CONST_CAST(char*,aString),aCount);
  return SVCompareChars(mStringValue,theStringValue,aIgnoreCase,aCount);
}

PRInt32  nsCString::Compare(const PRUnichar* aString,PRBool aIgnoreCase,PRInt32 aCount) const {
  nsStringValueImpl<PRUnichar> theStringValue(NS_CONST_CAST(PRUnichar*,aString),aCount);
  return SVCompare(mStringValue,theStringValue,aIgnoreCase,aCount);
}

PRBool  nsCString::Equals(const nsString &aString,PRBool aIgnoreCase,PRInt32 aCount) const {
  return PRBool(0==SVCompare(mStringValue,aString.mStringValue,aIgnoreCase,aCount));
}

PRBool  nsCString::Equals(const nsCString &aString,PRBool aIgnoreCase,PRInt32 aCount) const {
  PRInt32 result=SVCompareChars(mStringValue,aString.mStringValue,aIgnoreCase,aCount);
  return PRBool(0==result);
}

PRBool  nsCString::Equals(const char* aString,PRBool aIgnoreCase,PRInt32 aCount) const {
  nsStringValueImpl<char> theStringValue(NS_CONST_CAST(char*,aString),aCount);
  return PRBool(0==SVCompareChars(mStringValue,theStringValue,aIgnoreCase,aCount));
}


PRBool  nsCString::Equals(const PRUnichar* aString,PRBool aIgnoreCase,PRInt32 aCount) const {
  nsStringValueImpl<PRUnichar> theStringValue(NS_CONST_CAST(PRUnichar*,aString),aCount);
  return PRBool(0==SVCompare(mStringValue,theStringValue,aIgnoreCase,aCount));
}


PRBool nsCString::Equals(/*FIX: const */nsIAtom* anAtom,PRBool aIgnoreCase) const {
  return PR_TRUE;
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
PRInt32 nsCString::Find(const nsString& aTarget,PRBool aIgnoreCase,PRUint32 anOffset,PRInt32 aRepCount) const {
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
PRInt32 nsCString::Find(const nsCString &aString,PRBool aIgnoreCase,PRUint32 anOffset,PRInt32 aRepCount) const {
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
PRInt32 nsCString::Find(const char* aString,PRBool aIgnoreCase,PRUint32 anOffset,PRInt32 aRepCount) const {
  nsStringValueImpl<char> theString(NS_CONST_CAST(char*,aString));
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
PRInt32 nsCString::Find(const PRUnichar* aString,PRBool aIgnoreCase,PRUint32 anOffset,PRInt32 aRepCount) const {

  nsStringValueImpl<PRUnichar> theString(NS_CONST_CAST(PRUnichar*,aString));
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
PRInt32 nsCString::FindCharInSet(const nsCString& aString,PRUint32 anOffset) const{
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
PRInt32 nsCString::FindCharInSet(const char *aString,PRUint32 anOffset) const{

  nsStringValueImpl<char> theStringValue(NS_CONST_CAST(char*,aString));
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
PRInt32 nsCString::FindChar(char aChar,PRBool aIgnoreCase,PRInt32 anOffset,PRInt32 aCount) const {
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
PRInt32 nsCString::RFindChar(char aChar,PRBool aIgnoreCase,PRInt32 anOffset,PRInt32 aRepCount) const{
  return SVRFindChar(mStringValue,aChar,aIgnoreCase,aRepCount,anOffset);
}
 

PRInt32 nsCString::RFind(const char* aString,PRBool aIgnoreCase,PRInt32 anOffset,PRInt32 aRepCount) const{
  nsStringValueImpl<char> theStringValue(NS_CONST_CAST(char*,aString));
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
PRInt32 nsCString::RFindCharInSet(const nsCString& aString,PRInt32 anOffset) const {
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
PRInt32 nsCString::RFindCharInSet(const char* aString,PRInt32 anOffset) const{
  nsStringValueImpl<char> theStringValue(NS_CONST_CAST(char*,aString));
  return SVRFindCharInSet(mStringValue,theStringValue,PR_FALSE,mStringValue.mLength,anOffset);
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
  SVCopyBuffer_(aBuf,mStringValue.mBuffer,len);
  aBuf[len]=0;
  return aBuf;
}


PRInt32 nsCString::CountChar(char aChar) {  
  return SVCountChar(mStringValue,aChar);
}

  //******************************************
  // Utility methods...
  //******************************************

  
  //This will not work correctly for any unicode set other than ascii
void nsCString::DebugDump(void) const { 
  SVDebugDump(mStringValue);
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
PRInt32 nsCString::ToInteger(PRInt32* anErrorCode,PRUint32 aRadix) const{
  return SVToInteger(mStringValue,anErrorCode,aRadix);
}



/*****************************************************************
  Now we define the nsSubsumeCStr class
 *****************************************************************/

nsSubsumeCStr::nsSubsumeCStr(nsCString& aString) : nsCString() {
  mStringValue=aString.mStringValue;
  mOwnsBuffer=PR_TRUE;
  aString.mStringValue.mBuffer=0;
  aString.mStringValue.mRefCount=0;
}

nsSubsumeCStr::nsSubsumeCStr(char* aString,PRBool assumeOwnership,PRInt32 aLength) : nsCString() {
  mStringValue.mBuffer=aString;
  mStringValue.mCapacity=mStringValue.mLength=(-1==aLength) ? stringlen(aString) : aLength;
  mOwnsBuffer=assumeOwnership;
}


/**
 * 
 * @update  gess 01/04/99
 * @param 
 * @return
 */
NS_COM int fputs(const nsCString& aString, FILE* out)
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

PRUint32 HashCode(const nsCString& aDest) {
  PRUint32 result=0;
//  return (PRUint32)PL_HashString((const void*) aDest.GetBuffer());
  return result;
}

void Recycle( char* aBuffer) { 
  nsAllocator::Free(aBuffer); 
}

/*****************************************************************
  Now we declare the nsCAutoString class
 *****************************************************************/


nsCAutoString::nsCAutoString() : nsCString() {
  mInternalBuffer[0]=0;
  mStringValue.mCapacity=kDefaultStringSize;
  mStringValue.mBuffer=mInternalBuffer;
  mStringValue.mRefCount=2;
}

  //call this version nsAutoString derivatives...
nsCAutoString::nsCAutoString(const nsCAutoString& aString,PRInt32 aLength) : nsCString() {
  mInternalBuffer[0]=0;
  mStringValue.mCapacity=kDefaultStringSize;
  mStringValue.mBuffer=mInternalBuffer;
  mStringValue.mRefCount=2;
  nsCString::Assign(aString,aLength,0);
}

//call this version for nsCString,nsCString and the autostrings
nsCAutoString::nsCAutoString(const nsCString& aString,PRInt32 aLength) : nsCString() {
  mInternalBuffer[0]=0;
  mStringValue.mCapacity=kDefaultStringSize;
  mStringValue.mBuffer=mInternalBuffer;
  mStringValue.mRefCount=2;
  nsCString::Assign(aString,aLength,0);
}

  //call this version for char*'s....
nsCAutoString::nsCAutoString(const char* aString,PRInt32 aLength) : nsCString() {
  mInternalBuffer[0]=0;
  mStringValue.mCapacity=kDefaultStringSize;
  mStringValue.mBuffer=mInternalBuffer;
  mStringValue.mRefCount=2;
  nsCString::Assign(aString,aLength,0);
}

  //call this version for a single char of type char...
nsCAutoString::nsCAutoString(const char aChar) : nsCString() {
  mInternalBuffer[0]=0;
  mStringValue.mCapacity=kDefaultStringSize;
  mStringValue.mBuffer=mInternalBuffer;
  mStringValue.mRefCount=2;
  nsCString::Assign(aChar);
}


  //call this version for PRUnichar*'s....
nsCAutoString::nsCAutoString(const PRUnichar* aString,PRInt32 aLength) : nsCString() {
  mInternalBuffer[0]=0;
  mStringValue.mCapacity=kDefaultStringSize;
  mStringValue.mBuffer=mInternalBuffer;
  mStringValue.mRefCount=2;
  nsCString::Assign(aString,aLength);
}


  //call this version for all other ABT versions of readable strings
nsCAutoString::nsCAutoString(const nsAReadableString &aString) : nsCString() {
  mInternalBuffer[0]=0;
  mStringValue.mCapacity=kDefaultStringSize;
  mStringValue.mBuffer=mInternalBuffer;
  mStringValue.mRefCount=2;
  nsCString::Assign(aString);
}

nsCAutoString::nsCAutoString(const CBufDescriptor &aBuffer) : nsCString() {
  mInternalBuffer[0]=0;
  mStringValue.mBuffer=aBuffer.mBuffer;
  mStringValue.mCapacity=aBuffer.mCapacity;
  mStringValue.mLength=aBuffer.mLength;
  mStringValue.mRefCount=(aBuffer.mOwnsBuffer) ? 1 : 2;
}

nsCAutoString::nsCAutoString(const nsSubsumeCStr& aSubsumeString) : nsCString() {
  mInternalBuffer[0]=0;
  mStringValue=aSubsumeString.mStringValue;
  memset((void*)&aSubsumeString.mStringValue,0,sizeof(mStringValue));
}

nsCAutoString::~nsCAutoString() { 
}


nsCAutoString& nsCAutoString::operator=(const nsCAutoString& aString) {
  nsCString::operator=(aString);
  return *this;
}

nsCAutoString& nsCAutoString::operator=(const nsCString& aString) {
  nsCString::operator=(aString);
  return *this;
}

nsCAutoString& nsCAutoString::operator=(const char* aString) {
  nsCString::operator=(aString);
  return *this;
} 

nsCAutoString& nsCAutoString::operator=(const PRUnichar* aString) {
  nsCString::operator=(aString);
  return *this;
}

nsCAutoString& nsCAutoString::operator=(const char aChar) {
  nsCString::operator=(aChar);
  return *this;
} 


nsCAutoString& nsCAutoString::operator=(const nsSubsumeCStr &aSubsumeString) {
  nsCString::operator=(aSubsumeString);
  return *this;
}

