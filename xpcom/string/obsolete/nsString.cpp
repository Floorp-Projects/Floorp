
/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.0 (the "NPL"); you may not use this file except in
 * compliance with the NPL.  You may obtain a copy of the NPL at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the NPL is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the NPL
 * for the specific language governing rights and limitations under the
 * NPL.
 *
 * The Initial Developer of this code under the NPL is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation.  All Rights
 * Reserved.
 */ 

#include <ctype.h>
#include <string.h> 
#include <stdio.h>
#include "nsString.h"
#include "nsDebug.h"
#include "nsCRT.h"
#include "nsDeque.h"

#ifndef RICKG_TESTBED
#include "prdtoa.h"
#include "nsISizeOfHandler.h"
#endif

static const char* kNullPointerError = "Error: unexpected null ptr";
static const char* kWhitespace="\b\t\r\n ";




static void CSubsume(nsStr& aDest,nsStr& aSource){
  if(aSource.mStr && aSource.mLength) {
    if(aSource.mOwnsBuffer){
      nsStr::Destroy(aDest,0);
      aDest.mStr=aSource.mStr;
      aDest.mLength=aSource.mLength;
      aDest.mCharSize=aSource.mCharSize;
      aDest.mCapacity=aSource.mCapacity;
      aDest.mOwnsBuffer=aSource.mOwnsBuffer;
      aSource.mOwnsBuffer=PR_FALSE;
      aSource.mStr=0;
    }
    else{ 
      nsStr::Assign(aDest,aSource,0,aSource.mLength,0);
    }
  } 
  else nsStr::Truncate(aDest,0,0);
}


/**
 * Default constructor. 
 */
nsCString::nsCString(nsIMemoryAgent* anAgent) : mAgent(anAgent) {
  nsStr::Initialize(*this,eOneByte);
}

/**
 * This constructor accepts an ascii string
 * @update  gess 1/4/99
 * @param   aCString is a ptr to a 1-byte cstr
 * @param   aLength tells us how many chars to copy from given CString
 */
nsCString::nsCString(const char* aCString,PRInt32 aLength,nsIMemoryAgent* anAgent) : mAgent(anAgent) {  
  nsStr::Initialize(*this,eOneByte);
  Assign(aCString,aLength);
}

/**
 * This constructor accepts a unicode string
 * @update  gess 1/4/99
 * @param   aString is a ptr to a unichar string
 * @param   aLength tells us how many chars to copy from given aString
 */
nsCString::nsCString(const PRUnichar* aString,PRInt32 aLength,nsIMemoryAgent* anAgent) : mAgent(anAgent) {  
  nsStr::Initialize(*this,eOneByte);

  if(aString && aLength){
    nsStr temp;
    Initialize(temp,eTwoByte);
    temp.mUStr=(PRUnichar*)aString;

    if(0<aLength) {
        //this has to be done to make sure someone doesn't tell us
        //aCount=n but offer a string whose len<aCount
      temp.mLength=aLength;
      PRInt32 pos=nsStr::FindChar(temp,0,PR_FALSE,0);
      if((0<=pos) && (pos<(PRInt32)mLength)) {
        aLength=temp.mLength=pos;
      }
    }
    else aLength=temp.mLength=nsCRT::strlen(aString);

    if(0<aLength)
      nsStr::Append(*this,temp,0,aLength,mAgent);
  }
}

/**
 * This constructor works for all other nsSTr derivatives
 * @update  gess 1/4/99
 * @param   reference to another nsCString
 */
nsCString::nsCString(const nsStr &aString,nsIMemoryAgent* anAgent) : mAgent(anAgent) {
  nsStr::Initialize(*this,eOneByte);
  nsStr::Assign(*this,aString,0,aString.mLength,mAgent);
}

/**
 * This is our copy constructor 
 * @update  gess 1/4/99
 * @param   reference to another nsCString
 */
nsCString::nsCString(const nsCString& aString) :mAgent(aString.mAgent) {
  nsStr::Initialize(*this,aString.mCharSize);
  nsStr::Assign(*this,aString,0,aString.mLength,mAgent);
}

/**
 * construct from subsumeable string
 * @update  gess 1/4/99
 * @param   reference to a subsumeString
 */
nsCString::nsCString(nsSubsumeCStr& aSubsumeStr) :mAgent(0) {
  CSubsume(*this,aSubsumeStr);
}

/**
 * Destructor
 */
nsCString::~nsCString() {
  nsStr::Destroy(*this,mAgent);
}

void nsCString::SizeOf(nsISizeOfHandler* aHandler, PRUint32* aResult) const {
  if (aResult) {
    *aResult = sizeof(*this) + mCapacity * mCharSize;
  }
}

/**
 * This method truncates this string to given length.
 *
 * @update  gess 01/04/99
 * @param   anIndex -- new length of string
 * @return  nada
 */
void nsCString::Truncate(PRInt32 anIndex) {
  nsStr::Truncate(*this,anIndex,mAgent);
}

/**
 *  Determine whether or not the characters in this
 *  string are in sorted order.
 *  
 *  @update  gess 8/25/98
 *  @return  TRUE if ordered.
 */
PRBool nsCString::IsOrdered(void) const {
  PRBool  result=PR_TRUE;
  if(mLength>1) {
    PRUint32 theIndex;
    for(theIndex=1;theIndex<mLength;theIndex++) {
      if(mStr[theIndex-1]>mStr[theIndex]) {
        result=PR_FALSE;
        break;
      }
    }
  }
  return result;
}


/**
 * Call this method if you want to force the string to a certain capacity
 * @update  gess 1/4/99
 * @param   aLength -- contains new length for mStr
 */
void nsCString::SetCapacity(PRUint32 aLength) {
  if(aLength>mCapacity) {
    GrowCapacity(*this,aLength,mAgent);
  }
  AddNullTerminator(*this);
}

/**********************************************************************
  Accessor methods...
 *********************************************************************/


/**
 * Retrieves internal (1-byte) buffer ptr;
 * @update  gess1/4/99
 * @return  ptr to internal buffer
 */
const char* nsCString::GetBuffer(void) const {
  return mStr;
}

/**
 * Get nth character.
 */
PRUnichar nsCString::operator[](PRUint32 anIndex) const {
  return GetCharAt(*this,anIndex);
}

/**
 * Get nth character.
 */
PRUnichar nsCString::CharAt(PRUint32 anIndex) const {
  return GetCharAt(*this,anIndex);
}

/**
 * Get 1st character.
 */
PRUnichar nsCString::First(void) const{
  return GetCharAt(*this,0);
}

/**
 * Get last character.
 */
PRUnichar nsCString::Last(void) const{
  return (char)GetCharAt(*this,mLength-1);
}

/**
 * set a char inside this string at given index
 * @param aChar is the char you want to write into this string
 * @param anIndex is the ofs where you want to write the given char
 * @return TRUE if successful
 */
PRBool nsCString::SetCharAt(PRUnichar aChar,PRUint32 anIndex){
  PRBool result=PR_FALSE;
  if(anIndex<mLength){
    mStr[anIndex]=(char)aChar;
// SOON!   if(0==aChar) mLength=anIndex;
    result=PR_TRUE;
  }
  return result;
}

/*********************************************************
  append (operator+) METHODS....
 *********************************************************/

/**
 * Create a new string by appending given string to this
 * @update  gess 01/04/99
 * @param   aString -- 2nd string to be appended
 * @return  new subsumeable string (this makes things faster by reducing copying)
 */
nsSubsumeCStr nsCString::operator+(const nsCString& aString){
  nsCString temp(*this); //make a temp string the same size as this...
  nsStr::Append(temp,aString,0,aString.mLength,mAgent);
  return nsSubsumeCStr(temp);
}


/**
 * create a new string by adding this to the given buffer.
 * @update  gess 01/04/99
 * @param   aCString is a ptr to cstring to be added to this
 * @return  newly created subsumable string
 */
nsSubsumeCStr nsCString::operator+(const char* aCString) {
  nsCString temp(*this);
  temp.Append(aCString);
  return nsSubsumeCStr(temp);
}


/**
 * create a new string by adding this to the given char.
 * @update  gess 01/04/99
 * @param   aChar is a char to be added to this
 * @return  newly created subsumable string
 */
nsSubsumeCStr nsCString::operator+(PRUnichar aChar) {
  nsCString temp(*this);
  temp.Append(aChar);
  return nsSubsumeCStr(temp);
}

/**
 * create a new string by adding this to the given char.
 * @update  gess 01/04/99
 * @param   aChar is a char to be added to this
 * @return  newly created string
 */
nsSubsumeCStr nsCString::operator+(char aChar) {
  nsCString temp(*this);
  temp.Append(aChar);
  return nsSubsumeCStr(temp);
}


/**********************************************************************
  Lexomorphic transforms...
 *********************************************************************/

/**
 * Converts all chars in internal string to lower
 * @update  gess 01/04/99
 */
void nsCString::ToLowerCase() {
  nsStr::ChangeCase(*this,PR_FALSE);
}

/**
 * Converts all chars in internal string to upper
 * @update  gess 01/04/99
 */
void nsCString::ToUpperCase() {
  nsStr::ChangeCase(*this,PR_TRUE);
}

/**
 * Converts chars in this to lowercase, and
 * stores them in aString
 * @update  gess 01/04/99
 * @param   aOut is a string to contain result
 */
void nsCString::ToLowerCase(nsCString& aString) const {
  aString=*this;
  nsStr::ChangeCase(aString,PR_FALSE);
}

/**
 * Converts chars in this to uppercase, and
 * stores them in a given output string
 * @update  gess 01/04/99
 * @param   aOut is a string to contain result
 */
void nsCString::ToUpperCase(nsCString& aString) const {
  aString=*this;
  nsStr::ChangeCase(aString,PR_TRUE);
}

/**
 *  This method is used to remove all occurances of the
 *  characters found in aSet from this string.
 *  
 *  @update gess 01/04/99
 *  @param  aChar -- char to be stripped
 *  @return *this 
 */
nsCString& nsCString::StripChar(PRUnichar aChar){

  PRInt32 theIndex=FindChar(aChar,PR_FALSE,0);
  while(kNotFound<theIndex) {
    Cut(theIndex,1);
    theIndex=FindChar(aChar,PR_FALSE,theIndex);
  }
  return *this;
}

/**
 *  This method is used to remove all occurances of the
 *  characters found in aSet from this string.
 *  
 *  @update gess 01/04/99
 *  @param  aSet -- characters to be cut from this
 *  @return *this 
 */
nsCString& nsCString::StripChars(const char* aSet){
  if(aSet){
    PRInt32 theIndex=FindCharInSet(aSet,0);
    while(kNotFound<theIndex) {
      Cut(theIndex,1);
      theIndex=FindCharInSet(aSet,theIndex);
    }
  }
  return *this;
}


/**
 *  This method strips whitespace throughout the string
 *  
 *  @update  gess 01/04/99
 *  @return  this
 */
nsCString& nsCString::StripWhitespace() {
  return StripChars(kWhitespace);
}

/**
 *  This method is used to replace all occurances of the
 *  given source char with the given dest char
 *  
 *  @param  
 *  @return *this 
 */
nsCString& nsCString::ReplaceChar(PRUnichar aOldChar, PRUnichar aNewChar) {
  PRUint32 theIndex=0;
  for(theIndex=0;theIndex<mLength;theIndex++){
    if(mStr[theIndex]==(char)aOldChar) {
      mStr[theIndex]=(char)aNewChar;
    }//if
  }
  return *this;
}

/**
 *  This method is used to replace all occurances of the
 *  given source char with the given dest char
 *  
 *  @param  
 *  @return *this 
 */
nsCString& nsCString::ReplaceChar(const char* aSet, PRUnichar aNewChar){
  if(aSet){
    PRInt32 theIndex=FindCharInSet(aSet,0);
    while(kNotFound<theIndex) {
      mStr[theIndex]=(char)aNewChar;
      theIndex=FindCharInSet(aSet,theIndex+1);
    }
  }
  return *this;
}

/**
 *  This method is used to replace all occurances of the
 *  given source char with the given dest char
 *  
 *  @param  
 *  @return *this 
 */
PRInt32 nsCString::CountChar(PRUnichar aChar) {
  PRInt32 theIndex=0;
  PRInt32 theCount=0;
  PRInt32 theLen=(PRInt32)mLength;
  for(theIndex=0;theIndex<theLen;theIndex++){
    PRUnichar theChar=GetCharAt(*this,theIndex);
    if(theChar==aChar)
      theCount++;
  }
  return theCount;
}

/**
 *  This method trims characters found in aTrimSet from
 *  either end of the underlying string.
 *  
 *  @update  gess 3/31/98
 *  @param   aTrimSet -- contains chars to be trimmed from
 *           both ends
 *  @return  this
 */
nsCString& nsCString::Trim(const char* aTrimSet, PRBool aEliminateLeading,PRBool aEliminateTrailing){
  if(aTrimSet){
    nsStr::Trim(*this,aTrimSet,aEliminateLeading,aEliminateTrailing);
  }
  return *this;
}

/**
 *  This method strips chars in given set from string.
 *  You can control whether chars are yanked from
 *  start and end of string as well.
 *  
 *  @update  gess 3/31/98
 *  @param   aEliminateLeading controls stripping of leading ws
 *  @param   aEliminateTrailing controls stripping of trailing ws
 *  @return  this
 */
nsCString& nsCString::CompressSet(const char* aSet, PRUnichar aChar,PRBool aEliminateLeading,PRBool aEliminateTrailing){
  if(aSet){
    ReplaceChar(aSet,aChar);
    nsStr::CompressSet(*this,aSet,aEliminateLeading,aEliminateTrailing);
  }
  return *this;
}

/**
 *  This method strips whitespace from string.
 *  You can control whether whitespace is yanked from
 *  start and end of string as well.
 *  
 *  @update  gess 3/31/98
 *  @param   aEliminateLeading controls stripping of leading ws
 *  @param   aEliminateTrailing controls stripping of trailing ws
 *  @return  this
 */
nsCString& nsCString::CompressWhitespace( PRBool aEliminateLeading,PRBool aEliminateTrailing){
  CompressSet(kWhitespace,' ',aEliminateLeading,aEliminateTrailing);
  return *this;
}

/**********************************************************************
  string conversion methods...
 *********************************************************************/

/**
 * Creates a duplicate clone (ptr) of this string.
 * @update  gess 01/04/99
 * @return  ptr to clone of this string
 */
nsCString* nsCString::ToNewString() const {
  return new nsCString(*this);
}

/**
 * Creates an ascii clone of this string
 * Note that calls to this method should be matched with calls to Recycle().
 * @update  gess 01/04/99
 * @return  ptr to new ascii string
 */
char* nsCString::ToNewCString() const {
  nsCString temp(mStr);
  temp.SetCapacity(8);
  char* result=temp.mStr;
  temp.mStr=0;
  return result;
}

/**
 * Creates an unicode clone of this string
 * Note that calls to this method should be matched with calls to Recycle().
 * @update  gess 01/04/99
 * @return  ptr to new ascii string
 */
PRUnichar* nsCString::ToNewUnicode() const {
  nsString temp(mStr);
  temp.SetCapacity(8);
  PRUnichar* result=temp.mUStr;
  temp.mStr=0;
  temp.mOwnsBuffer=PR_FALSE;
  return result;
}

/**
 * Copies contents of this string into he given buffer
 * Note that if you provide me a buffer that is smaller than the length of
 * this string, only the number of bytes that will fit are copied. 
 *
 * @update  gess 01/04/99
 * @param   aBuf
 * @param   aBufLength
 * @param   anOffset
 * @return
 */
char* nsCString::ToCString(char* aBuf, PRUint32 aBufLength,PRUint32 anOffset) const{
  if(aBuf) {

    // NS_ASSERTION(mLength<=aBufLength,"buffer smaller than string");

    CBufDescriptor theDescr(aBuf,PR_TRUE,aBufLength,0);
    nsCAutoString temp(theDescr);
    temp.Assign(*this,aBufLength-1);
    temp.mStr=0;
  }
  return aBuf;
}


/**
 * Perform string to float conversion.
 * @update  gess 01/04/99
 * @param   aErrorCode will contain error if one occurs
 * @return  float rep of string value
 */
float nsCString::ToFloat(PRInt32* aErrorCode) const {
  char buf[100];
  if (mLength > PRInt32(sizeof(buf)-1)) {
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
 * Perform numeric string to int conversion with given radix.
 * NOTE: 1. This method mandates that the string is well formed.
 *       2. This method will return an error if the string you give
            contains chars outside the range for the specified radix.

 * @update  gess 10/01/98
 * @param   aErrorCode will contain error if one occurs
 * @param   aRadix tells us what base to expect the string in.
 * @return  int rep of string value
 */
PRInt32 _ToInteger(nsCString& aString,PRInt32* anErrorCode,PRUint32 aRadix) {

  //copy chars to local buffer -- step down from 2 bytes to 1 if necessary...
  PRInt32 result=0;

  char*   cp = aString.mStr + aString.mLength;
  PRInt32 theMult=1;

  *anErrorCode = NS_OK;

    //now iterate the numeric chars and build our result
  char theChar=0;
  char theDigit=0;
  while(--cp>=aString.mStr){
    theChar=*cp;
    if((theChar>='0') && (theChar<='9')){
      theDigit=theChar-'0';
    }
    else if((theChar>='A') && (theChar<='F')) {
      if(10==aRadix){
        *anErrorCode=NS_ERROR_ILLEGAL_VALUE;
        result=0;
        break;
      }
      theDigit=(theChar-'A')+10;
    }
    else if('-'==theChar) {
      result=-result;
      break;
    }
    else if(('+'==theChar) || (' '==theChar)) { //stop in a good state if you see this...
      break;
    }
    else {
      //we've encountered a char that's not a legal number or sign
      *anErrorCode=NS_ERROR_ILLEGAL_VALUE;
      result=0;
      break;
    }

    result+=theDigit*theMult;
    theMult*=aRadix;
  }
  
  return result;
}

/**
 * Call this method to extract the rightmost numeric value from the given 
 * 1-byte input string, and simultaneously determine the radix.
 * NOTE: This method mandates that the string is well formed.
 *       Leading and trailing gunk should be removed, and the case upper.
 * @update  gess 10/01/98
 * @param   anInputString contains orig string
 * @param   anOutString contains numeric portion copy of input string
 * @param   aRadix (an out parm) tells the caller what base we think the string is in.
 * @return  non-zero error code if this string is non-numeric
 */
PRInt32 GetNumericSubstring(nsCString& aString,PRUint32& aRadix) {

  aString.ToUpperCase();

  PRInt32 decPt=nsStr::FindChar(aString,'.',PR_TRUE,0);
  char*   cp = (kNotFound==decPt) ? aString.mStr + aString.mLength-1 : aString.mStr+decPt-1;
  
  aRadix=kRadixUnknown; //assume for starters...

  // Skip trailing non-numeric...
  while (cp >= aString.mStr) {
    if((*cp>='0') && (*cp<='9')){
      if(kRadixUnknown==aRadix)
        aRadix=kRadix10;
      break;
    }
    else if((*cp>='A') && (*cp<='F')) {
      aRadix=16;
      break;
    }  
    cp--;
  }
  aString.Truncate(cp-aString.mStr+1);

  //ok, now scan through chars until you find the start of this number...
  //we delimit the number by the presence of: +,-,#,X

  // Skip trailing non-numeric...
  cp++;
  while (--cp >= aString.mStr) {
    if((*cp>='0') && (*cp<='9')){
      continue;
    }
    else if((*cp>='A') && (*cp<='F')) {
      continue;
    }
    else if((*cp=='-') || (*cp=='+')){
      break;
    }
    else {
      if(('#'==(*cp)) || ('X'==(*cp))) 
        aRadix=kRadix16;
      cp++; //move back by one
      break;
    }
  }
  if(cp>aString.mStr)
    aString.Cut(0,cp-aString.mStr);
  PRInt32 result=(0==aString.mLength) ? NS_ERROR_ILLEGAL_VALUE : NS_OK;

  return result;
}


/**
 * This method tries to autodetect that radix given a string
 * @update  gess 10/01/98
 * @return  10,16,or 0 (meaning I don't know)
 */
PRUint32 nsCString::DetermineRadix(void) {
  PRUint32 result=kRadixUnknown;
  if(0<mLength) {
    nsCAutoString theString(*this);
    if(NS_OK!=GetNumericSubstring(theString,result))
      result=kRadixUnknown;
  }
  return result;
}


/**
 * Perform decimal numeric string to int conversion.
 * NOTE: In this version, we use the radix you give, even if it's wrong.
 * @update  gess 10/01/98
 * @param   aErrorCode will contain error if one occurs
 * @param   aRadix tells us what base to expect the given string in.
 * @return  int rep of string value
 */
PRInt32 nsCString::ToInteger(PRInt32* anErrorCode,PRUint32 aRadix) const {

  //copy chars to local buffer -- step down from 2 bytes to 1 if necessary...
  nsCAutoString theString(*this);
  PRUint32  theRadix=aRadix;
  PRInt32   result=0;
  
  *anErrorCode=GetNumericSubstring(theString,theRadix); //we actually don't use this radix; use given radix instead
  if(NS_OK==*anErrorCode){
    if(kAutoDetect==aRadix)
      aRadix=theRadix;
    if((kRadix10==aRadix) || (kRadix16==aRadix))
      result=_ToInteger(theString,anErrorCode,aRadix); //note we use the given radix, not the computed one.
    else *anErrorCode=NS_ERROR_ILLEGAL_VALUE;
  }

  return result;
}

/**********************************************************************
  String manipulation methods...                
 *********************************************************************/


/**
 * assign given nsStr (or derivative) to this one
 * @update  gess 01/04/99
 * @param   aString: string to be appended
 * @return  this
 */
nsCString& nsCString::Assign(const nsStr& aString,PRInt32 aCount) {
  if(this!=&aString){
    nsStr::Truncate(*this,0,0);

    if(aCount<0)
      aCount=aString.mLength;
    else aCount=MinInt(aCount,aString.mLength);

    nsStr::Assign(*this,aString,0,aCount,mAgent);
  }
  return *this;
}
  
/**
 * assign given char* to this string
 * @update  gess 01/04/99
 * @param   aCString: buffer to be assigned to this 
 * @return  this
 */
nsCString& nsCString::Assign(const char* aCString,PRInt32 aCount) {
  nsStr::Truncate(*this,0,0);
  if(aCString){
    Append(aCString,aCount);
  }
  return *this;
}

/**
 * assign given unichar* to this string
 * @update  gess 01/04/99
 * @param   aCString: buffer to be assigned to this 
 * @return  this
 */
nsCString& nsCString::Assign(const PRUnichar* aString,PRInt32 aCount) {
  nsStr::Truncate(*this,0,0);

  if(aString){
    nsStr temp;
    Initialize(temp,eTwoByte);
    temp.mUStr=(PRUnichar*)aString;

    if(0<aCount) {
        //this has to be done to make sure someone doesn't tell us
        //aCount=n but offer a string whose len<aCount
      temp.mLength=aCount;
      PRInt32 pos=nsStr::FindChar(temp,0,PR_FALSE,0);
      if((0<=pos) && (pos<aCount)) {
        aCount=temp.mLength=pos;
      }
    }
    else aCount=temp.mLength=nsCRT::strlen(aString);

    if(0<aCount)
      nsStr::Append(*this,temp,0,aCount,mAgent);
  }
  return *this;
}

/**
 * assign given unichar to this string
 * @update  gess 01/04/99
 * @param   aChar: char to be assignd to this
 * @return  this
 */
nsCString& nsCString::Assign(PRUnichar aChar) {
  nsStr::Truncate(*this,0,0);
  return Append(aChar);
}

/**
 * assign given char to this string
 * @update  gess 01/04/99
 * @param   aChar: char to be assignd to this
 * @return  this
 */
nsCString& nsCString::Assign(char aChar) {
  nsStr::Truncate(*this,0,0);
  return Append(aChar);
}

/**
 * WARNING! THIS IS A VERY SPECIAL METHOD. 
 * This method "steals" the contents of aSource and hands it to aDest.
 * Ordinarily a copy is made, but not in this version.
 * @update  gess10/30/98
 * @param 
 * @return
 */
#ifdef AIX
nsCString& nsCString::operator=(const nsSubsumeCStr& aSubsumeString) {
  nsSubsumeCStr temp(aSubsumeString);  // a temp is needed for the AIX compiler
  CSubsume(*this,temp);
#else
nsCString& nsCString::operator=(nsSubsumeCStr& aSubsumeString) {
  CSubsume(*this,aSubsumeString);
#endif // AIX
  return *this;
}


/**
 * append given string to this string
 * @update  gess 01/04/99
 * @param   aString : string to be appended to this
 * @return  this
 */
nsCString& nsCString::Append(const nsCString& aString,PRInt32 aCount) {
  if(aCount<0)
    aCount=aString.mLength;
  else aCount=MinInt(aCount,aString.mLength);
  if(0<aCount)
    nsStr::Append(*this,aString,0,aCount,mAgent);
  return *this;
}

/**
 * append given c-string to this string
 * @update  gess 01/04/99
 * @param   aString : string to be appended to this
 * @param   aCount: #of chars to be copied; -1 means to copy the whole thing
 * @return  this
 */
nsCString& nsCString::Append(const char* aCString,PRInt32 aCount) {
  if(aCString){
    nsStr temp;
    Initialize(temp,eOneByte);
    temp.mStr=(char*)aCString;

    if(0<aCount) {
        //this has to be done to make sure someone doesn't tell us
        //aCount=n but offer a string whose len<aCount
      temp.mLength=aCount;
      PRInt32 pos=nsStr::FindChar(temp,0,PR_FALSE,0);
      if((0<=pos) && (pos<aCount)) {
        aCount=temp.mLength=pos;
      }
    }
    else aCount=temp.mLength=nsCRT::strlen(aCString);

    if(0<aCount)
      nsStr::Append(*this,temp,0,aCount,mAgent);
  }
  return *this;
}


/**
 * append given char to this string
 * @update  gess 01/04/99
 * @param   aChar: char to be appended to this
 * @return  this
 */
nsCString& nsCString::Append(PRUnichar aChar) {
  PRUnichar buf[2]={0,0};
  buf[0]=aChar;

  nsStr temp;
  Initialize(temp,eTwoByte);
  temp.mUStr=buf;
  temp.mLength=1;
  nsStr::Append(*this,temp,0,1,mAgent);
  return *this;
}

/**
 * append given unichar to this string
 * @update  gess 01/04/99
 * @param   aChar: char to be appended to this
 * @return  this
 */
nsCString& nsCString::Append(char aChar) {
  char buf[2]={0,0};
  buf[0]=aChar;

  nsStr temp;
  Initialize(temp,eOneByte);
  temp.mStr=buf;
  temp.mLength=1;
  nsStr::Append(*this,temp,0,1,mAgent);
  return *this;
}

/**
 * Append the given integer to this string 
 * @update  gess 01/04/99
 * @param   aInteger:
 * @param   aRadix:
 * @return
 */
nsCString& nsCString::Append(PRInt32 aInteger,PRInt32 aRadix) {
  char* fmt = "%d";
  if (8 == aRadix) {
    fmt = "%o";
  } else if (16 == aRadix) {
    fmt = "%x";
  }
  char buf[40];
  // *** XX UNCOMMENT THIS LINE
  //PR_snprintf(buf, sizeof(buf), fmt, aInteger);
  sprintf(buf,fmt,aInteger);
  return Append(buf);
}


/**
 * Append the given float to this string 
 * @update  gess 01/04/99
 * @param   aFloat:
 * @return
 */
nsCString& nsCString::Append(float aFloat){
  char buf[40];
  // *** XX UNCOMMENT THIS LINE
  //PR_snprintf(buf, sizeof(buf), "%g", aFloat);
  sprintf(buf,"%g",aFloat);
  return Append(buf);
}


/*
 *  Copies n characters from this left of this string to given string,
 *  
 *  @update  gess 4/1/98
 *  @param   aDest -- Receiving string
 *  @param   aCount -- number of chars to copy
 *  @return  number of chars copied
 */
PRUint32 nsCString::Left(nsCString& aDest,PRInt32 aCount) const{
  if(aCount<0)
    aCount=mLength;
  else aCount=MinInt(aCount,mLength);
  nsStr::Assign(aDest,*this,0,aCount,mAgent);
  return aDest.mLength;
}

/*
 *  Copies n characters from this string to from given offset
 *  
 *  @update  gess 4/1/98
 *  @param   aDest -- Receiving string
 *  @param   anOffset -- where copying should begin
 *  @param   aCount -- number of chars to copy
 *  @return  number of chars copied
 */
PRUint32 nsCString::Mid(nsCString& aDest,PRUint32 anOffset,PRInt32 aCount) const{
  if(aCount<0)
    aCount=mLength;
  else aCount=MinInt(aCount,mLength);
  nsStr::Assign(aDest,*this,anOffset,aCount,mAgent);
  return aDest.mLength;
}

/*
 *  Copies last n characters from this string to given string,
 *  
 *  @update gess 4/1/98
 *  @param  aDest -- Receiving string
 *  @param  aCount -- number of chars to copy
 *  @return number of chars copied
 */
PRUint32 nsCString::Right(nsCString& aDest,PRInt32 aCount) const{
  PRInt32 offset=MaxInt(mLength-aCount,0);
  return Mid(aDest,offset,aCount);
}


/*
 *  This method inserts n chars from given string into this
 *  string at str[anOffset].
 *  
 *  @update gess 4/1/98
 *  @param  aString -- source String to be inserted into this
 *  @param  anOffset -- insertion position within this str
 *  @param  aCount -- number of chars to be copied from aCopy
 *  @return this
 */
nsCString& nsCString::Insert(const nsCString& aString,PRUint32 anOffset,PRInt32 aCount) {

  nsStr::Insert(*this,anOffset,aString,0,aCount,mAgent);
  return *this;
}

/**
 * Insert a char* into this string at a specified offset.
 *
 * @update  gess4/22/98
 * @param   char* aCString to be inserted into this string
 * @param   anOffset is insert pos in str 
 * @param   aCounttells us how many chars to insert
 * @return  this
 */
nsCString& nsCString::Insert(const char* aCString,PRUint32 anOffset,PRInt32 aCount){
  if(aCString){
    nsStr temp;
    nsStr::Initialize(temp,eOneByte);
    temp.mStr=(char*)aCString;

    if(0<aCount) {
        //this has to be done to make sure someone doesn't tell us
        //aCount=n but offer a string whose len<aCount
      temp.mLength=aCount;
      PRInt32 pos=nsStr::FindChar(temp,0,PR_FALSE,0);
      if((0<=pos) && (pos<aCount)) {
        aCount=temp.mLength=pos;
      }
    }
    else aCount=temp.mLength=nsCRT::strlen(aCString);

    if(temp.mLength && (0<aCount)){
      nsStr::Insert(*this,anOffset,temp,0,aCount,0);
    }
  }
  return *this;  
}


/**
 * Insert a single unichar into this string at a specified offset.
 *
 * @update  gess4/22/98
 * @param   aChar unichar to be inserted into this string
 * @param   anOffset is insert pos in str 
 * @return  this
 */
nsCString& nsCString::Insert(PRUnichar aChar,PRUint32 anOffset){
  PRUnichar theBuffer[2]={0,0};
  theBuffer[0]=aChar;
  nsStr temp;
  nsStr::Initialize(temp,eTwoByte);
  temp.mUStr=theBuffer;
  temp.mLength=1;
  nsStr::Insert(*this,anOffset,temp,0,1,0);
  return *this;
}

/**
 * Insert a single uni-char into this string at
 * a specified offset.
 *
 * @update  gess4/22/98
 * @param   aChar char to be inserted into this string
 * @param   anOffset is insert pos in str 
 * @return  this
 */
nsCString& nsCString::Insert(char aChar,PRUint32 anOffset){
  char theBuffer[2]={0,0};
  theBuffer[0]=aChar;
  nsStr temp;
  nsStr::Initialize(temp,eOneByte);
  temp.mStr=theBuffer;
  temp.mLength=1;
  nsStr::Insert(*this,anOffset,temp,0,1,0);
  return *this;
}

/*
 *  This method is used to cut characters in this string
 *  starting at anOffset, continuing for aCount chars.
 *  
 *  @update gess 01/04/99
 *  @param  anOffset -- start pos for cut operation
 *  @param  aCount -- number of chars to be cut
 *  @return *this
 */
nsCString& nsCString::Cut(PRUint32 anOffset, PRInt32 aCount) {
  if(0<aCount) {
    nsStr::Delete(*this,anOffset,aCount,mAgent);
  }
  return *this;
}

/**********************************************************************
  Searching methods...                
 *********************************************************************/
 
/**
 *  Search for given character within this string.
 *  This method does so by using a binary search,
 *  so your string HAD BETTER BE ORDERED!
 *  
 *  @param   aChar is the unicode char to be found
 *  @return  offset in string, or -1 (kNotFound)
 */
PRInt32 nsCString::BinarySearch(PRUnichar aChar) const{
  PRInt32 low=0;
  PRInt32 high=mLength-1;

  while (low <= high) {
    int middle = (low + high) >> 1;
    PRUnichar theChar=GetCharAt(*this,middle);
    if (theChar==aChar)
      return middle;
    if (theChar>aChar)
      high = middle - 1; 
    else
      low = middle + 1; 
  }
  return kNotFound;
}

/**
 *  Search for given cstr within this string
 *  
 *  @update  gess 3/25/98
 *  @param   aCString - substr to be found
 *  @param   aIgnoreCase
 *  @param   anOffset
 *  @return  offset in string, or -1 (kNotFound)
 */
PRInt32 nsCString::Find(const char* aCString,PRBool aIgnoreCase,PRInt32 anOffset) const{
  PRInt32 result=kNotFound;
  if(aCString) {
    nsStr temp;
    nsStr::Initialize(temp,eOneByte);
    temp.mLength=nsCRT::strlen(aCString);
    temp.mStr=(char*)aCString;
    result=nsStr::FindSubstr(*this,temp,aIgnoreCase,anOffset);
  }
  return result;
}

/** 
 *  Search for given buffer within this string
 *  
 *  @update  gess 3/25/98
 *  @param   aString  unichar* to be found
 *  @param   aIgnoreCase
 *  @param   anOffset
 *  @return  offset in string, or -1 (kNotFound)
 */
PRInt32 nsCString::Find(const PRUnichar* aString,PRBool aIgnoreCase,PRInt32 anOffset) const{
  NS_ASSERTION(0!=aString,kNullPointerError);

  PRInt32 result=kNotFound;
  if(aString) {
    nsStr temp;
    nsStr::Initialize(temp,eTwoByte);
    temp.mLength=nsCRT::strlen(aString);
    temp.mUStr=(PRUnichar*)aString;
    result=nsStr::FindSubstr(*this,temp,aIgnoreCase,anOffset);
  }
  return result;
}

/**
 *  Search for given string within this string
 *  
 *  @update  gess 3/25/98
 *  @param   aSTring -- buffer to be found
 *  @param   aIgnoreCase
 *  @param   anOffset
 *  @return  offset in string, or -1 (kNotFound)
 */
PRInt32 nsCString::Find(const nsStr& aString,PRBool aIgnoreCase,PRInt32 anOffset) const{
  PRInt32 result=nsStr::FindSubstr(*this,aString,aIgnoreCase,anOffset);
  return result;
}


/**
 *  Search for a given char, starting at given offset
 *  
 *  @update  gess 3/25/98
 *  @param   aChar the unichar to be sought
 *  @param   aIgnoreCase
 *  @param   anOffset
 *  @return  offset of found char, or -1 (kNotFound)
 */
PRInt32 nsCString::FindChar(PRUnichar aChar,PRBool aIgnoreCase,PRInt32 anOffset) const{
  PRInt32 result=nsStr::FindChar(*this,aChar,aIgnoreCase,anOffset);
  return result;
}

/**
 *  This method finds the offset of the first char in this string that is
 *  a member of the given charset, starting the search at anOffset
 *  
 *  @update  gess 3/25/98
 *  @param   aCStringSet
 *  @param   anOffset -- where in this string to start searching
 *  @return  
 */
PRInt32 nsCString::FindCharInSet(const char* aCStringSet,PRInt32 anOffset) const{
  NS_ASSERTION(0!=aCStringSet,kNullPointerError);

  PRInt32 result=kNotFound;
  if(aCStringSet) {
    nsStr temp;
    nsStr::Initialize(temp,eOneByte);
    temp.mLength=nsCRT::strlen(aCStringSet);
    temp.mStr=(char*)aCStringSet;
    result=nsStr::FindCharInSet(*this,temp,PR_FALSE,anOffset);
  }
  return result;
}

/**
 *  This method finds the offset of the first char in this string that is
 *  a member of the given charset, starting the search at anOffset
 *  
 *  @update  gess 3/25/98
 *  @param   aStringSet
 *  @param   anOffset -- where in this string to start searching
 *  @return  
 */
PRInt32 nsCString::FindCharInSet(const PRUnichar* aStringSet,PRInt32 anOffset) const{
  NS_ASSERTION(0!=aStringSet,kNullPointerError);

  PRInt32 result=kNotFound;
  if(aStringSet) {
    nsStr temp;
    nsStr::Initialize(temp,eTwoByte);
    temp.mLength=nsCRT::strlen(aStringSet);
    temp.mStr=(char*)aStringSet;
    result=nsStr::FindCharInSet(*this,temp,PR_FALSE,anOffset);
  }
  return result;
}

/**
 *  This method finds the offset of the first char in this string that is
 *  a member of the given charset, starting the search at anOffset
 *  
 *  @update  gess 3/25/98
 *  @param   aCStringSet
 *  @param   anOffset -- where in this string to start searching
 *  @return  
 */
PRInt32 nsCString::FindCharInSet(const nsStr& aSet,PRInt32 anOffset) const{
  PRInt32 result=nsStr::FindCharInSet(*this,aSet,PR_FALSE,anOffset);
  return result;
}

/**
 *  Reverse search for substring
 *  
 *  @update  gess 3/25/98
 *  @param   aString
 *  @param   aIgnoreCase
 *  @param   anOffset - tells us where to begin the search
 *  @return  offset of substring or -1
 */
PRInt32 nsCString::RFind(const nsStr& aString,PRBool aIgnoreCase,PRInt32 anOffset) const{
  PRInt32 result=nsStr::RFindSubstr(*this,aString,aIgnoreCase,anOffset);
  return result;
}


/**
 *  Reverse search for substring
 *  
 *  @update  gess 3/25/98
 *  @param   aString
 *  @param   aIgnoreCase
 *  @param   anOffset - tells us where to begin the search
 *  @return  offset of substring or -1
 */
PRInt32 nsCString::RFind(const char* aString,PRBool aIgnoreCase,PRInt32 anOffset) const{
  NS_ASSERTION(0!=aString,kNullPointerError);

  PRInt32 result=kNotFound;
  if(aString) {
    nsStr temp;
    nsStr::Initialize(temp,eOneByte);
    temp.mLength=nsCRT::strlen(aString);
    temp.mStr=(char*)aString;
    result=nsStr::RFindSubstr(*this,temp,aIgnoreCase,anOffset);
  }
  return result;
}

/**
 *  Reverse search for char
 *  
 *  @update  gess 3/25/98
 *  @param   aChar
 *  @param   aIgnoreCase
 *  @param   anOffset - tells us where to begin the search
 *  @return  offset of substring or -1
 */
PRInt32 nsCString::RFindChar(PRUnichar aChar,PRBool aIgnoreCase,PRInt32 anOffset) const{
  PRInt32 result=nsStr::RFindChar(*this,aChar,aIgnoreCase,anOffset);
  return result;
}

/**
 *  Reverse search for char in this string that is also a member of given charset
 *  
 *  @update  gess 3/25/98
 *  @param   aCStringSet
 *  @param   anOffset
 *  @return  offset of found char, or -1 (kNotFound)
 */
PRInt32 nsCString::RFindCharInSet(const char* aCStringSet,PRInt32 anOffset) const{
  NS_ASSERTION(0!=aCStringSet,kNullPointerError);

  PRInt32 result=kNotFound;
  if(aCStringSet) {
    nsStr temp;
    nsStr::Initialize(temp,eOneByte);
    temp.mLength=nsCRT::strlen(aCStringSet);
    temp.mStr=(char*)aCStringSet;
    result=nsStr::RFindCharInSet(*this,temp,PR_FALSE,anOffset);
  }
  return result;
}

/**
 *  Reverse search for char in this string that is also a member of given charset
 *  
 *  @update  gess 3/25/98
 *  @param   aStringSet
 *  @param   anOffset
 *  @return  offset of found char, or -1 (kNotFound)
 */
PRInt32 nsCString::RFindCharInSet(const PRUnichar* aStringSet,PRInt32 anOffset) const{
  NS_ASSERTION(0!=aStringSet,kNullPointerError);

  PRInt32 result=kNotFound;
  if(aStringSet) {
    nsStr temp;
    nsStr::Initialize(temp,eTwoByte);
    temp.mLength=nsCRT::strlen(aStringSet);
    temp.mUStr=(PRUnichar*)aStringSet;
    result=nsStr::RFindCharInSet(*this,temp,PR_FALSE,anOffset);
  }
  return result;
}


/**
 *  Reverse search for char in this string that is also a member of given charset
 *  
 *  @update  gess 3/25/98
 *  @param   aCStringSet
 *  @param   anOffset
 *  @return  offset of found char, or -1 (kNotFound)
 */
PRInt32 nsCString::RFindCharInSet(const nsStr& aSet,PRInt32 anOffset) const{
  PRInt32 result=nsStr::RFindCharInSet(*this,aSet,PR_FALSE,anOffset);
  return result;
}


/**************************************************************
  COMPARISON METHODS...
 **************************************************************/


/**
 * Compares given cstring to this string. 
 * @update  gess 01/04/99
 * @param   aCString points to a cstring
 * @param   aIgnoreCase tells us how to treat case
 * @param   aCount tells us how many chars to test; -1 implies full length
 * @return  -1,0,1
 */
PRInt32 nsCString::Compare(const char *aCString,PRBool aIgnoreCase,PRInt32 aCount) const {
  NS_ASSERTION(0!=aCString,kNullPointerError);

  if(aCString) {
    nsStr temp;
    nsStr::Initialize(temp,eOneByte);
    temp.mLength=nsCRT::strlen(aCString);
    temp.mStr=(char*)aCString;
    return nsStr::Compare(*this,temp,aCount,aIgnoreCase);
  }
  return 0;
}

/**
 * Compares given unistring to this string. 
 * @update  gess 01/04/99
 * @param   aString pts to a uni-string
 * @param   aIgnoreCase tells us how to treat case
 * @param   aCount tells us how many chars to test; -1 implies full length
 * @return  -1,0,1
 */
PRInt32 nsCString::Compare(const PRUnichar* aString,PRBool aIgnoreCase,PRInt32 aCount) const {
  NS_ASSERTION(0!=aString,kNullPointerError);

  if(aString) {
    nsStr temp;
    nsStr::Initialize(temp,eTwoByte);
    temp.mLength=nsCRT::strlen(aString);
    temp.mUStr=(PRUnichar*)aString;
    return nsStr::Compare(*this,temp,aCount,aIgnoreCase);
  }
  return 0;
}

/**
 * Compare given nsStr with this cstring.
 * 
 * @param   aString is an nsStr instance to be compared
 * @param   aIgnoreCase tells us how to treat case
 * @param   aCount tells us how many chars to test; -1 implies full length
 * @return  -1,0,1
 */
PRInt32 nsCString::Compare(const nsStr& aString,PRBool aIgnoreCase,PRInt32 aCount) const {
  return nsStr::Compare(*this,aString,aCount,aIgnoreCase);
}


/**
 *  Here come a whole bunch of operator functions that are self-explanatory...
 */
PRBool nsCString::operator==(const nsStr& S) const {return Equals(S);}      
PRBool nsCString::operator==(const char* s) const {return Equals(s);}
PRBool nsCString::operator==(const PRUnichar* s) const {return Equals(s);}

PRBool nsCString::operator!=(const nsStr& S) const {return PRBool(Compare(S)!=0);}
PRBool nsCString::operator!=(const char* s) const {return PRBool(Compare(s)!=0);}
PRBool nsCString::operator!=(const PRUnichar* s) const {return PRBool(Compare(s)!=0);}

PRBool nsCString::operator<(const nsStr& S) const {return PRBool(Compare(S)<0);}
PRBool nsCString::operator<(const char* s) const {return PRBool(Compare(s)<0);}
PRBool nsCString::operator<(const PRUnichar* s) const {return PRBool(Compare(s)<0);}

PRBool nsCString::operator>(const nsStr& S) const {return PRBool(Compare(S)>0);}
PRBool nsCString::operator>(const char* s) const {return PRBool(Compare(s)>0);}
PRBool nsCString::operator>(const PRUnichar* s) const {return PRBool(Compare(s)>0);}

PRBool nsCString::operator<=(const nsStr& S) const {return PRBool(Compare(S)<=0);}
PRBool nsCString::operator<=(const char* s) const {return PRBool(Compare(s)<=0);}
PRBool nsCString::operator<=(const PRUnichar* s) const {return PRBool(Compare(s)<=0);}

PRBool nsCString::operator>=(const nsStr& S) const {return PRBool(Compare(S)>=0);}
PRBool nsCString::operator>=(const char* s) const {return PRBool(Compare(s)>=0);}
PRBool nsCString::operator>=(const PRUnichar* s) const {return PRBool(Compare(s)>=0);}


PRBool nsCString::EqualsIgnoreCase(const nsStr& aString) const {
  return Equals(aString,PR_TRUE);
}

PRBool nsCString::EqualsIgnoreCase(const char* aString,PRInt32 aLength) const {
  return Equals(aString,PR_TRUE,aLength);
}

PRBool nsCString::EqualsIgnoreCase(const PRUnichar* aString,PRInt32 aLength) const {
  return Equals(aString,PR_TRUE,aLength);
}


/**
 * Compare this to given string; note that we compare full strings here.
 * 
 * @update gess 01/04/99
 * @param  aString is the other nsCString to be compared to
 * @param   aIgnoreCase tells us how to treat case
 * @param   aCount tells us how many chars to test; -1 implies full length
 * @return TRUE if equal
 */
PRBool nsCString::Equals(const nsStr& aString,PRBool aIgnoreCase,PRInt32 aCount) const {
  PRInt32 theAnswer=nsStr::Compare(*this,aString,aCount,aIgnoreCase);
  PRBool  result=PRBool(0==theAnswer);
  return result;
}

/**
 * Compare this to given string; note that we compare full strings here.
 *
 * @param  aString is the CString to be compared 
 * @param  aCount tells us how many chars you want to compare starting with start of string
 * @param  aIgnorecase tells us whether to be case sensitive
 * @param  aCount tells us how many chars to test; -1 implies full length
 * @return TRUE if equal
 */
PRBool nsCString::Equals(const char* aCString,PRBool aIgnoreCase,PRInt32 aCount) const{
  PRInt32 theAnswer=Compare(aCString,aIgnoreCase,aCount);
  PRBool  result=PRBool(0==theAnswer);  
  return result;
}

PRBool nsCString::Equals(const PRUnichar* aString,PRBool aIgnoreCase,PRInt32 aCount) const {
  PRInt32 theAnswer=Compare(aString,aIgnoreCase,aCount);
  PRBool  result=PRBool(0==theAnswer);  
  return result;
}



/**************************************************************
  Define the string deallocator class...
 **************************************************************/
#ifndef RICKG_TESTBED
class nsCStringDeallocator: public nsDequeFunctor{
public:
  virtual void* operator()(void* anObject) {
    static nsMemoryAgent theAgent;
    nsCString* aString= (nsCString*)anObject;
    if(aString){
      aString->mAgent=&theAgent;
      delete aString;
    }
    return 0;
  }
};
#endif

/****************************************************************************
 * This class, appropriately enough, creates and recycles nsCString objects..
 ****************************************************************************/



#ifndef RICKG_TESTBED
class nsCStringRecycler {
public:
  nsCStringRecycler() : mDeque(0) {
  }

  ~nsCStringRecycler() {
    nsCStringDeallocator theDeallocator;
    mDeque.ForEach(theDeallocator); //now delete the strings
  }

  void Recycle(nsCString* aString) {
    mDeque.Push(aString);
  }

  nsCString* CreateString(void){
    nsCString* result=(nsCString*)mDeque.Pop();
    if(!result)
      result=new nsCString();
    return result;
  }
  nsDeque mDeque;
};
static nsCStringRecycler& GetRecycler(void);


/**
 * 
 * @update  gess 01/04/99
 * @param 
 * @return
 */
nsCStringRecycler& GetRecycler(void){
  static nsCStringRecycler gCRecycler;
  return gCRecycler;
}

#endif

/**
 * Call this mehod when you're done 
 * @update  gess 01/04/99
 * @param 
 * @return
 */
nsCString* nsCString::CreateString(void){
  nsCString* result=0;
#ifndef RICKG_TESTBED
  result=GetRecycler().CreateString();
#endif
  return result;
}

/**
 * Call this mehod when you're done 
 * @update  gess 01/04/99
 * @param 
 * @return
 */
void nsCString::Recycle(nsCString* aString){
#ifndef RICKG_TESTBED
  GetRecycler().Recycle(aString);
#endif
}

#if 0
/**
 * 
 * @update  gess 01/04/99
 * @param 
 * @return
 */
void nsCString::DebugDump(ostream& aStream) const {
  for(PRUint32 i=0;i<mLength;i++) {
    aStream <<mStr[i];
  }
  aStream << endl;
}

/**
 * 
 * @update  gess8/8/98
 * @param 
 * @return
 */
ostream& operator<<(ostream& aStream,const nsCString& aString){
  if(eOneByte==aString.mCharSize) {
    aStream<<aString.mStr;
  }
  else{
    PRUint32        theOffset=0;
    const PRUint32  theBufSize=300;
    char            theBuf[theBufSize+1];
    PRUint32        theCount=0;
    PRUint32        theRemains=0;

    while(theOffset<aString.mLength){
      theRemains=aString.mLength-theOffset;
      theCount=(theRemains<theBufSize) ? theRemains : theBufSize;
      aString.ToCString(theBuf,theCount+1,theOffset);
      theBuf[theCount]=0;
      aStream<<theBuf;
      theOffset+=theCount;
    }
  }    
  return aStream;
}
#endif

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
  PRInt32 len = aString.mLength;
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
       

/***********************************************************************
  IMPLEMENTATION NOTES: AUTOSTRING...
 ***********************************************************************/


/**
 * Default constructor
 *
 */
nsCAutoString::nsCAutoString() : nsCString(){
  nsStr::Initialize(*this,mBuffer,sizeof(mBuffer)-1,0,eOneByte,PR_FALSE);
  mAgent=0;
  AddNullTerminator(*this);

}

/**
 * Default constructor
 */
nsCAutoString::nsCAutoString(const nsCAutoString& aString) : nsCString() {
  nsStr::Initialize(*this,mBuffer,sizeof(mBuffer)-1,0,eOneByte,PR_FALSE);
  mAgent=0;
  AddNullTerminator(*this);
  Append(aString);
}


/**
 * Copy construct from ascii c-string
 * @param   aCString is a ptr to a 1-byte cstr
 */
nsCAutoString::nsCAutoString(const char* aCString,PRInt32 aLength) : nsCString() {
  nsStr::Initialize(*this,mBuffer,sizeof(mBuffer)-1,0,eOneByte,PR_FALSE);
  mAgent=0;
  AddNullTerminator(*this);
  Append(aCString,aLength);
}

/**
 * Copy construct using an external buffer descriptor
 * @param   aBuffer -- descibes external buffer
 */
nsCAutoString::nsCAutoString(const CBufDescriptor& aBuffer) : nsCString() {
  mAgent=0;
  if(!aBuffer.mBuffer) {
    nsStr::Initialize(*this,mBuffer,sizeof(mBuffer)-1,0,eOneByte,PR_FALSE);
  }
  else {
    nsStr::Initialize(*this,aBuffer.mBuffer,aBuffer.mCapacity,aBuffer.mLength,aBuffer.mCharSize,!aBuffer.mStackBased);
  }
  AddNullTerminator(*this); //this isn't really needed, but it guarantees that folks don't pass string constants.
}

/**
 * Copy construct from uni-string
 * @param   aString is a ptr to a unistr
 */
nsCAutoString::nsCAutoString(const PRUnichar* aString,PRInt32 aLength) : nsCString() {
  mAgent=0;
  nsStr::Initialize(*this,mBuffer,sizeof(mBuffer)-1,0,eOneByte,PR_FALSE);
  AddNullTerminator(*this);
  Append(aString,aLength);
}

/**
 * construct from an nsStr
 * @param   
 */
nsCAutoString::nsCAutoString(const nsStr& aString) : nsCString() {
  mAgent=0;
  nsStr::Initialize(*this,mBuffer,sizeof(mBuffer)-1,0,eOneByte,PR_FALSE);
  AddNullTerminator(*this);
  Append(aString);
}



/**
 * construct from a char
 * @param   
 */
nsCAutoString::nsCAutoString(PRUnichar aChar) : nsCString(){
  mAgent=0;
  nsStr::Initialize(*this,mBuffer,sizeof(mBuffer)-1,0,eOneByte,PR_FALSE);
  AddNullTerminator(*this);
  Append(aChar);
}

/**
 * construct from a subsumeable string
 * @update  gess 1/4/99
 * @param   reference to a subsumeString
 */
#ifdef AIX
nsCAutoString::nsCAutoString(const nsSubsumeCStr& aSubsumeStr) :nsCString() {
  mAgent=0;
  nsSubsumeCStr temp(aSubsumeStr);  // a temp is needed for the AIX compiler
  CSubsume(*this,temp);
#else
nsCAutoString::nsCAutoString( nsSubsumeCStr& aSubsumeStr) :nsCString() {
  mAgent=0;
  CSubsume(*this,aSubsumeStr);
#endif // AIX
}

/**
 * deconstructor
 * @param   
 */
nsCAutoString::~nsCAutoString(){
}

void nsCAutoString::SizeOf(nsISizeOfHandler* aHandler, PRUint32* aResult) const {
  if (aResult) {
    *aResult = sizeof(*this) + mCapacity * mCharSize;
  }
}

nsSubsumeCStr::nsSubsumeCStr(nsStr& aString) : nsCString() {
  CSubsume(*this,aString);
}

nsSubsumeCStr::nsSubsumeCStr(PRUnichar* aString,PRBool assumeOwnership,PRInt32 aLength) : nsCString() {
  mUStr=aString;
  mCapacity=mLength=(-1==aLength) ? nsCRT::strlen(aString) : aLength;
  mOwnsBuffer=assumeOwnership;
}
 
nsSubsumeCStr::nsSubsumeCStr(char* aString,PRBool assumeOwnership,PRInt32 aLength) : nsCString() {
  mStr=aString;
  mCapacity=mLength=(-1==aLength) ? strlen(aString) : aLength;
  mOwnsBuffer=assumeOwnership;
}


