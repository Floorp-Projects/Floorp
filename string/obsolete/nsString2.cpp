
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
#include "nsString.h"
#include "nsDebug.h"
#include "nsDeque.h"


#ifndef RICKG_TESTBED
#include "prdtoa.h"
#include "nsISizeOfHandler.h"
#endif 


static const char* kNullPointerError = "Error: unexpected null ptr";
static const char* kWhitespace="\b\t\r\n ";



static void Subsume(nsStr& aDest,nsStr& aSource){
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
 * Default constructor. Note that we actually allocate a small buffer
 * to begin with. This is because the "philosophy" of the string class
 * was to allow developers direct access to the underlying buffer for
 * performance reasons. 
 */
nsString::nsString(eCharSize aCharSize,nsIMemoryAgent* anAgent) : mAgent(anAgent) {
  nsStr::Initialize(*this,aCharSize);
}

/**
 * This constructor accepts an ascii string
 * @update	gess 1/4/99
 * @param   aCString is a ptr to a 1-byte cstr
 */
nsString::nsString(const char* aCString,eCharSize aCharSize,nsIMemoryAgent* anAgent) : mAgent(anAgent) {  
  nsStr::Initialize(*this,aCharSize);
  Assign(aCString);

}

/**
 * This constructor accepts an ascii string
 * @update	gess 1/4/99
 * @param   aCString is a ptr to a 1-byte cstr
 */
nsString::nsString(const PRUnichar* aString,eCharSize aCharSize,nsIMemoryAgent* anAgent) : mAgent(anAgent) {  
  nsStr::Initialize(*this,aCharSize);
  Assign(aString);
}

/**
 * This is our copy constructor 
 * @update	gess 1/4/99
 * @param   reference to another nsString
 */
nsString::nsString(const nsStr &aString,eCharSize aCharSize,nsIMemoryAgent* anAgent) : mAgent(anAgent) {
  nsStr::Initialize(*this,aCharSize);
  nsStr::Assign(*this,aString,0,aString.mLength,mAgent);
}

/**
 * This is our copy constructor 
 * @update	gess 1/4/99
 * @param   reference to another nsString
 */
nsString::nsString(const nsString& aString) :mAgent(aString.mAgent) {
  nsStr::Initialize(*this,aString.mCharSize);
  nsStr::Assign(*this,aString,0,aString.mLength,mAgent);
}

/**
 * construct off a subsumeable string
 * @update	gess 1/4/99
 * @param   reference to a subsumeString
 */
nsString::nsString(nsSubsumeStr& aSubsumeStr) :mAgent(0) {
  Subsume(*this,aSubsumeStr);
}

/**
 * Destructor
 * Make sure we call nsStr::Destroy.
 */
nsString::~nsString() {
  nsStr::Destroy(*this,mAgent);
}

void nsString::SizeOf(nsISizeOfHandler* aHandler) const {
#ifndef RICKG_TESTBED
  aHandler->Add(sizeof(*this));
  aHandler->Add(mCapacity << mCharSize);
#endif
}

/**
 * This method truncates this string to given length.
 *
 * @update	gess 01/04/99
 * @param   anIndex -- new length of string
 * @return  nada
 */
void nsString::Truncate(PRInt32 anIndex) {
  nsStr::Truncate(*this,anIndex,mAgent);
}

/**
 *  Determine whether or not the characters in this
 *  string are in sorted order.
 *  
 *  @update  gess 8/25/98
 *  @return  TRUE if ordered.
 */
PRBool nsString::IsOrdered(void) const {
  PRBool  result=PR_TRUE;
  if(mLength>1) {
    PRUint32 theIndex;
    PRUnichar c1=0;
    PRUnichar c2=GetCharAt(*this,0);
    for(theIndex=1;theIndex<mLength;theIndex++) {
      c1=c2;
      c2=GetCharAt(*this,theIndex);
      if(c1>c2) {
        result=PR_FALSE;
        break;
      }
    }
  }
  return result;
}


/**
 * Call this method if you want to force the string to a certain capacity
 * @update	gess 1/4/99
 * @param   aLength -- contains new length for mStr
 * @return
 */
void nsString::SetCapacity(PRUint32 aLength) {
  if(aLength>mCapacity) {
    GrowCapacity(*this,aLength,mAgent);
  }
  AddNullTerminator(*this);
}

/**********************************************************************
  Accessor methods...
 *********************************************************************/


//static char gChar=0;

/** 
 * 
 * @update	gess1/4/99
 * @return  ptr to internal buffer (if 1-byte), otherwise NULL
 */
const char* nsString::GetBuffer(void) const {
  const char* result=(eOneByte==mCharSize) ? mStr : 0;
  return result;
}


/**
 * This method returns the internal unicode buffer. 
 * Now that we've factored the string class, this should never
 * be able to return a 1 byte string.
 *
 * @update	gess1/4/99
 * @return  ptr to internal (2-byte) buffer;
 */
const PRUnichar* nsString::GetUnicode(void)  const {
  const PRUnichar* result=(eOneByte==mCharSize) ? 0 : mUStr;
  return result;
}

/**
 * Get nth character.
 */
PRUnichar nsString::operator[](PRUint32 anIndex) const {
  return GetCharAt(*this,anIndex);
}

PRUnichar nsString::CharAt(PRUint32 anIndex) const {
  return GetCharAt(*this,anIndex);
}

PRUnichar nsString::First(void) const{
  return GetCharAt(*this,0);
}

PRUnichar nsString::Last(void) const{
  return GetCharAt(*this,mLength-1);
}

PRBool nsString::SetCharAt(PRUnichar aChar,PRUint32 anIndex){
  PRBool result=PR_FALSE;
  if(anIndex<mLength){
    if(eOneByte==mCharSize)
      mStr[anIndex]=char(aChar); 
    else mUStr[anIndex]=aChar;

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
 * @update	gess 01/04/99
 * @param   aString -- 2nd string to be appended
 * @return  new string
 */
nsSubsumeStr nsString::operator+(const nsStr& aString){
  nsString temp(*this); //make a temp string the same size as this...
  nsStr::Append(temp,aString,0,aString.mLength,mAgent);
  return nsSubsumeStr(temp);
}

/**
 * Create a new string by appending given string to this
 * @update	gess 01/04/99
 * @param   aString -- 2nd string to be appended
 * @return  new string
 */
nsSubsumeStr nsString::operator+(const nsString& aString){
  nsString temp(*this); //make a temp string the same size as this...
  nsStr::Append(temp,aString,0,aString.mLength,mAgent);
  return nsSubsumeStr(temp);
}

/**
 * create a new string by adding this to the given buffer.
 * @update	gess 01/04/99
 * @param   aCString is a ptr to cstring to be added to this
 * @return  newly created string
 */
nsSubsumeStr nsString::operator+(const char* aCString) {
  nsString temp(*this);
  temp.Append(aCString); 
  return nsSubsumeStr(temp);
}


/**
 * create a new string by adding this to the given char.
 * @update	gess 01/04/99
 * @param   aChar is a char to be added to this
 * @return  newly created string
 */
nsSubsumeStr nsString::operator+(char aChar) {
  nsString temp(*this);
  temp.Append(char(aChar));
  return nsSubsumeStr(temp);
}

/**
 * create a new string by adding this to the given buffer.
 * @update	gess 01/04/99
 * @param   aString is a ptr to unistring to be added to this
 * @return  newly created string
 */
nsSubsumeStr nsString::operator+(const PRUnichar* aString) {
  nsString temp(*this);
  temp.Append(aString);
  return nsSubsumeStr(temp);
}


/**
 * create a new string by adding this to the given char.
 * @update	gess 01/04/99
 * @param   aChar is a unichar to be added to this
 * @return  newly created string
 */
nsSubsumeStr nsString::operator+(PRUnichar aChar) {
  nsString temp(*this);
  temp.Append(char(aChar));
  return nsSubsumeStr(temp);
}

/**********************************************************************
  Lexomorphic transforms...
 *********************************************************************/

/**
 * Converts all chars in internal string to lower
 * @update	gess 01/04/99
 */
void nsString::ToLowerCase() {
  nsStr::ChangeCase(*this,PR_FALSE);
}

/**
 * Converts all chars in internal string to upper
 * @update	gess 01/04/99
 */
void nsString::ToUpperCase() {
  nsStr::ChangeCase(*this,PR_TRUE);
}

/**
 * Converts chars in this to lowercase, and
 * stores them in aString
 * @update	gess 01/04/99
 * @param   aOut is a string to contain result
 */
void nsString::ToLowerCase(nsString& aString) const {
  aString=*this;
  nsStr::ChangeCase(aString,PR_FALSE);
}

/**
 * Converts chars in this to lowercase, and
 * stores them in a given output string
 * @update	gess 01/04/99
 * @param   aOut is a string to contain result
 */
void nsString::ToUpperCase(nsString& aString) const {
  aString=*this;
  nsStr::ChangeCase(aString,PR_TRUE);
}

/**
 *  This method is used to remove all occurances of the
 *  characters found in aSet from this string.
 *  
 *  @update gess 01/04/99
 *  @param  aSet -- characters to be cut from this
 *  @return *this 
 */
nsString& nsString::StripChar(PRUnichar aChar){

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
nsString& nsString::StripChars(const char* aSet){

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
nsString& nsString::StripWhitespace() {
  StripChars(kWhitespace);
  return *this;
}

/**
 *  This method is used to replace all occurances of the
 *  given source char with the given dest char
 *  
 *  @param  
 *  @return *this 
 */
nsString& nsString::ReplaceChar(PRUnichar aSourceChar, PRUnichar aDestChar) {
  PRUint32 theIndex=0;
  if(eTwoByte==mCharSize){
    for(theIndex=0;theIndex<mLength;theIndex++){
      if(mUStr[theIndex]==aSourceChar) {
        mUStr[theIndex]=aDestChar;
      }//if
    }
  }
  else{
    for(theIndex=0;theIndex<mLength;theIndex++){
      if(mStr[theIndex]==(char)aSourceChar) {
        mStr[theIndex]=(char)aDestChar;
      }//if
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
nsString& nsString::ReplaceChar(const char* aSet, PRUnichar aNewChar){
  if(aSet){
    PRInt32 theIndex=FindCharInSet(aSet,0);
    while(kNotFound<theIndex) {
      if(eTwoByte==mCharSize) 
        mUStr[theIndex]=aNewChar;
      else mStr[theIndex]=(char)aNewChar;
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
PRInt32 nsString::CountChar(PRUnichar aChar) {
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
nsString& nsString::Trim(const char* aTrimSet, PRBool aEliminateLeading,PRBool aEliminateTrailing){
  if(aTrimSet){
    nsStr::Trim(*this,aTrimSet,aEliminateLeading,aEliminateTrailing);
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
nsString& nsString::CompressSet(const char* aSet, PRUnichar aChar,PRBool aEliminateLeading,PRBool aEliminateTrailing){
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
nsString& nsString::CompressWhitespace( PRBool aEliminateLeading,PRBool aEliminateTrailing){
  CompressSet(kWhitespace,' ',aEliminateLeading,aEliminateTrailing);

  return *this;
}

/**********************************************************************
  string conversion methods...
 *********************************************************************/

/**
 * Creates a duplicate clone (ptr) of this string.
 * @update	gess 01/04/99
 * @return  ptr to clone of this string
 */
nsString* nsString::ToNewString() const {
  return new nsString(*this);
}

/**
 * Creates an ascii clone of this string
 * @update	gess 01/04/99
 * @return  ptr to new ascii string
 */
char* nsString::ToNewCString() const {
  nsCString temp(*this);
  temp.SetCapacity(8); //ensure that we get an allocated buffer instead of the common empty one.
  char* result=temp.mStr;
  temp.mStr=0;
  temp.mOwnsBuffer=PR_FALSE;

#ifdef RICKG_DEBUG
//  fstream& theStream=GetLogStream();
//  theStream << "tonewcString() " << result << endl;
#endif
  
  return result;
}

/**
 * Creates an ascii clone of this string
 * @update	gess 01/04/99
 * @return  ptr to new ascii string
 */
PRUnichar* nsString::ToNewUnicode() const {
  PRUnichar* result=0;
  if(eOneByte==mCharSize) {
    nsString temp(mStr);
    temp.SetCapacity(8);
    result=temp.mUStr;
    temp.mStr=0;
    temp.mOwnsBuffer=PR_FALSE;
  }
  else{
    nsString temp(mUStr);
    temp.SetCapacity(8);
    result=temp.mUStr;
    temp.mStr=0;
    temp.mOwnsBuffer=PR_FALSE;
  }
  return result;
}

/**
 * Copies contents of this string into he given buffer
 * @update	gess 01/04/99
 * @param 
 * @return
 */
char* nsString::ToCString(char* aBuf, PRUint32 aBufLength,PRUint32 anOffset) const{
  if(aBuf) {
    CBufDescriptor theSB(aBuf,PR_TRUE,aBufLength,0);
    nsCAutoString temp(theSB);
    temp.Assign(*this);
    temp.mStr=0;
  }
  return aBuf;
}

/**
 * Perform string to float conversion.
 * @update	gess 01/04/99
 * @param   aErrorCode will contain error if one occurs
 * @return  float rep of string value
 */
float nsString::ToFloat(PRInt32* aErrorCode) const {
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

 * @update	gess 10/01/98
 * @param   aErrorCode will contain error if one occurs
 * @param   aRadix tells us what base to expect the string in.
 * @return  int rep of string value
 */
PRInt32 _ToInteger(nsString& aString,PRInt32* anErrorCode,PRUint32 aRadix) {

  //copy chars to local buffer -- step down from 2 bytes to 1 if necessary...
  PRInt32 result=0;

  char*   cp = aString.mStr + aString.mLength;
  PRInt32 theMult=1;

  *anErrorCode = NS_OK;

    //now iterate the numeric chars and build our result
  char theDigit=0;
  while(--cp>=aString.mStr){
    char theChar=*cp;
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
 * @update	gess 10/01/98
 * @param   anInputString contains orig string
 * @param   anOutString contains numeric portion copy of input string
 * @param   aRadix (an out parm) tells the caller what base we think the string is in.
 * @return  non-zero error code if this string is non-numeric
 */
PRInt32 GetNumericSubstring(nsString& aString,PRUint32& aRadix) {

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
 * @update	gess 10/01/98
 * @return  10,16,or 0 (meaning I don't know)
 */
PRUint32 nsString::DetermineRadix(void) {
  PRUint32 result=kRadixUnknown;
  if(0<mLength) {
    nsAutoString theString(*this,eOneByte);
    if(NS_OK!=GetNumericSubstring(theString,result))
      result=kRadixUnknown;
  }
  return result;
}


/**
 * Perform decimal numeric string to int conversion.
 * NOTE: In this version, we use the radix you give, even if it's wrong.
 * @update	gess 10/01/98
 * @param   aErrorCode will contain error if one occurs
 * @param   aRadix tells us what base to expect the given string in.
 * @return  int rep of string value
 */
PRInt32 nsString::ToInteger(PRInt32* anErrorCode,PRUint32 aRadix) const {

  //copy chars to local buffer -- step down from 2 bytes to 1 if necessary...
  nsAutoString theString(*this,eOneByte);
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
 * assign given string to this one
 * @update	gess 01/04/99
 * @param   aString: string to be added to this
 * @return  this
 */
nsString& nsString::Assign(const nsStr& aString,PRInt32 aCount) {
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
 * @update	gess 01/04/99
 * @param   aCString: buffer to be assigned to this 
 * @return  this
 */
nsString& nsString::Assign(const char* aCString,PRInt32 aCount) {
  nsStr::Truncate(*this,0,0);
  if(aCString){
    Append(aCString,aCount);
  }
  return *this;
}

/**
 * assign given unichar* to this string
 * @update	gess 01/04/99
 * @param   aString: buffer to be assigned to this 
 * @return  this
 */
nsString& nsString::Assign(const PRUnichar* aString,PRInt32 aCount) {
  nsStr::Truncate(*this,0,0);
  if(aString){
    Append(aString,aCount);
  }
  return *this;
}

/**
 * assign given char to this string
 * @update	gess 01/04/99
 * @param   aChar: char to be assignd to this
 * @return  this
 */
nsString& nsString::Assign(char aChar) {
  nsStr::Truncate(*this,0,0);
  return Append(aChar);
}

/**
 * assign given char to this string
 * @update	gess 01/04/99
 * @param   aChar: char to be assignd to this
 * @return  this
 */
nsString& nsString::Assign(PRUnichar aChar) {
  nsStr::Truncate(*this,0,0);
  return Append(aChar);
}

/**
 * WARNING! THIS IS A VERY SPECIAL METHOD. 
 * This method "steals" the contents of aSource and hands it to aDest.
 * Ordinarily a copy is made, but not in this version.
 * @update	gess10/30/98
 * @param 
 * @return
 */
#ifdef AIX
nsString& nsString::operator=(const nsSubsumeStr& aSubsumeString) {
  nsSubsumeStr temp(aSubsumeString);  // a temp is needed for the AIX compiler
  Subsume(*this,temp);
#else
  nsString& nsString::operator=(nsSubsumeStr& aSubsumeString) {
  Subsume(*this,aSubsumeString);
#endif // AIX
  return *this;
}


/**
 * append given string to this string
 * @update	gess 01/04/99
 * @param   aString : string to be appended to this
 * @return  this
 */
nsString& nsString::Append(const nsStr& aString,PRInt32 aCount) {

  if(aCount<0)
    aCount=aString.mLength;
  else aCount=MinInt(aCount,aString.mLength);

  if(0<aCount)
    nsStr::Append(*this,aString,0,aCount,mAgent);
  return *this;
}


/**
 * append given string to this string
 * @update	gess 01/04/99
 * @param   aString : string to be appended to this
 * @return  this
 */
nsString& nsString::Append(const nsString& aString,PRInt32 aCount) {
  if(aCount<0)
    aCount=aString.mLength;
  else aCount=MinInt(aCount,aString.mLength);
  if(0<aCount)
    nsStr::Append(*this,aString,0,aCount,mAgent);
  return *this;
}

/**
 * append given string to this string
 * @update	gess 01/04/99
 * @param   aString : string to be appended to this
 * @param   aCount -- number of chars to copy; -1 tells us to compute the strlen for you
 * @return  this
 */
nsString& nsString::Append(const char* aCString,PRInt32 aCount) {
  if(aCString && aCount){  //if astring is null or count==0 there's nothing to do
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
 * append given string to this string
 * @update	gess 01/04/99
 * @param   aString : string to be appended to this
 * @param   aCount -- number of chars to copy; -1 tells us to compute the strlen for you
 * @return  this
 */
nsString& nsString::Append(const PRUnichar* aString,PRInt32 aCount) {
  if(aString && aCount){  //if astring is null or count==0 there's nothing to do
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
 * append given string to this string
 * @update	gess 01/04/99
 * @param   aString : string to be appended to this
 * @return  this
 */
nsString& nsString::Append(char aChar) {
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
 * append given string to this string
 * @update	gess 01/04/99
 * @param   aString : string to be appended to this
 * @return  this
 */
nsString& nsString::Append(PRUnichar aChar) {
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
 * 
 * @update	gess 01/04/99
 * @param 
 * @return
 */
nsString& nsString::Append(PRInt32 aInteger,PRInt32 aRadix) {
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
 * 
 * @update	gess 01/04/99
 * @param 
 * @return
 */
nsString& nsString::Append(float aFloat){
  char buf[40];
  // *** XX UNCOMMENT THIS LINE
  //PR_snprintf(buf, sizeof(buf), "%g", aFloat);
  sprintf(buf,"%g",aFloat);
  Append(buf);
  return *this;
}


/*
 *  Copies n characters from this string to given string,
 *  starting at the leftmost offset.
 *  
 *  
 *  @update  gess 4/1/98
 *  @param   aCopy -- Receiving string
 *  @param   aCount -- number of chars to copy
 *  @return  number of chars copied
 */
PRUint32 nsString::Left(nsString& aDest,PRInt32 aCount) const{

  if(aCount<0)
    aCount=mLength;
  else aCount=MinInt(aCount,mLength);
  nsStr::Assign(aDest,*this,0,aCount,mAgent);

  return aDest.mLength;
}

/*
 *  Copies n characters from this string to given string,
 *  starting at the given offset.
 *  
 *  
 *  @update  gess 4/1/98
 *  @param   aDest -- Receiving string
 *  @param   aCount -- number of chars to copy
 *  @param   anOffset -- position where copying begins
 *  @return  number of chars copied
 */
PRUint32 nsString::Mid(nsString& aDest,PRUint32 anOffset,PRInt32 aCount) const{
  if(aCount<0)
    aCount=mLength;
  else aCount=MinInt(aCount,mLength);
  nsStr::Assign(aDest,*this,anOffset,aCount,mAgent);

  return aDest.mLength;
}

/*
 *  Copies n characters from this string to given string,
 *  starting at rightmost char.
 *  
 *  
 *  @update gess 4/1/98
 *  @param  aCopy -- Receiving string
 *  @param  aCount -- number of chars to copy
 *  @return number of chars copied
 */
PRUint32 nsString::Right(nsString& aCopy,PRInt32 aCount) const{
  PRInt32 offset=MaxInt(mLength-aCount,0);
  return Mid(aCopy,offset,aCount);
}


/*
 *  This method inserts n chars from given string into this
 *  string at str[anOffset].
 *  
 *  @update gess 4/1/98
 *  @param  aCopy -- String to be inserted into this
 *  @param  anOffset -- insertion position within this str
 *  @param  aCount -- number of chars to be copied from aCopy
 *  @return number of chars inserted into this.
 */
nsString& nsString::Insert(const nsString& aCopy,PRUint32 anOffset,PRInt32 aCount) {
  nsStr::Insert(*this,anOffset,aCopy,0,aCount,mAgent);
  return *this;
}

/**
 * Insert a single unicode char into this string at
 * a specified offset.
 *
 * @update	gess4/22/98
 * @param   aChar char to be inserted into this string
 * @param   anOffset is insert pos in str 
 * @param   aCounttells us how many chars to insert
 * @return  the number of chars inserted into this string
 */
nsString& nsString::Insert(const char* aCString,PRUint32 anOffset,PRInt32 aCount){
  if(aCString && aCount){
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

    if(0<aCount){
      nsStr::Insert(*this,anOffset,temp,0,aCount,0);
    }
  }
  return *this;  
}


/**
 * Insert a unicode* into this string at
 * a specified offset.
 *
 * @update	gess4/22/98
 * @param   aChar char to be inserted into this string
 * @param   anOffset is insert pos in str 
 * @return  the number of chars inserted into this string
 */
nsString& nsString::Insert(const PRUnichar* aString,PRUint32 anOffset,PRInt32 aCount){
  if(aString && aCount){
    nsStr temp;
    nsStr::Initialize(temp,eTwoByte);
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

    if(0<aCount){
      nsStr::Insert(*this,anOffset,temp,0,aCount,0);
    }
  }
  return *this;  
}


/**
 * Insert a single uni-char into this string at
 * a specified offset.
 *
 * @update	gess4/22/98
 * @param   aChar char to be inserted into this string
 * @param   anOffset is insert pos in str 
 * @return  the number of chars inserted into this string
 */
nsString& nsString::Insert(PRUnichar aChar,PRUint32 anOffset){
  PRUnichar theBuffer[2]={0,0};
  theBuffer[0]=aChar;
  nsStr temp;
  nsStr::Initialize(temp,eTwoByte);
  temp.mUStr=theBuffer;
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
nsString& nsString::Cut(PRUint32 anOffset, PRInt32 aCount) {
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
PRInt32 nsString::BinarySearch(PRUnichar aChar) const{
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
 *  Search for given buffer within this string
 *  
 *  @update  gess 3/25/98
 *  @param   aCStringBuf - charstr to be found
 *  @return  offset in string, or -1 (kNotFound)
 */
PRInt32 nsString::Find(const char* aCString,PRBool aIgnoreCase,PRInt32 anOffset) const{
  NS_ASSERTION(0!=aCString,kNullPointerError);

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
 *  @param   aCStringBuf - charstr to be found
 *  @return  offset in string, or -1 (kNotFound)
 */
PRInt32 nsString::Find(const PRUnichar* aString,PRBool aIgnoreCase,PRInt32 anOffset) const{
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
 *  Search for given buffer within this string
 *  
 *  @update  gess 3/25/98
 *  @param   nsString -- buffer to be found
 *  @return  offset in string, or -1 (kNotFound)
 */
PRInt32 nsString::Find(const nsStr& aString,PRBool aIgnoreCase,PRInt32 anOffset) const{
  PRInt32 result=nsStr::FindSubstr(*this,aString,aIgnoreCase,anOffset);
  return result;
}

/**
 *  Search for given buffer within this string
 *  
 *  @update  gess 3/25/98
 *  @param   nsString -- buffer to be found
 *  @return  offset in string, or -1 (kNotFound)
 */
PRInt32 nsString::Find(const nsString& aString,PRBool aIgnoreCase,PRInt32 anOffset) const{
  PRInt32 result=nsStr::FindSubstr(*this,aString,aIgnoreCase,anOffset);
  return result;
}

/**
 *  Search for a given char, starting at given offset
 *  
 *  @update  gess 3/25/98
 *  @param   
 *  @return  offset of found char, or -1 (kNotFound)
 */
PRInt32 nsString::Find(PRUnichar aChar,PRInt32 anOffset,PRBool aIgnoreCase) const{
  PRInt32 result=nsStr::FindChar(*this,aChar,aIgnoreCase,anOffset);
  return result;
}

/**
 *  Search for a given char, starting at given offset
 *  
 *  @update  gess 3/25/98
 *  @param   
 *  @return  offset of found char, or -1 (kNotFound)
 */
PRInt32 nsString::FindChar(PRUnichar aChar,PRBool aIgnoreCase,PRInt32 anOffset) const{
  PRInt32 result=nsStr::FindChar(*this,aChar,aIgnoreCase,anOffset);
  return result;
}

/**
 *  
 *  
 *  @update  gess 3/25/98
 *  @param   
 *  @return  
 */
PRInt32 nsString::FindCharInSet(const char* aCStringSet,PRInt32 anOffset) const{
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
 *  
 *  
 *  @update  gess 3/25/98
 *  @param   
 *  @return  
 */
PRInt32 nsString::FindCharInSet(const PRUnichar* aStringSet,PRInt32 anOffset) const{
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
 *  
 *  
 *  @update  gess 3/25/98
 *  @param   
 *  @return  
 */
PRInt32 nsString::FindCharInSet(const nsStr& aSet,PRInt32 anOffset) const{
  PRInt32 result=nsStr::FindCharInSet(*this,aSet,PR_FALSE,anOffset);
  return result;
}


/**
 *  
 *  
 *  @update  gess 3/25/98
 *  @param   
 *  @return  
 */
PRInt32 nsString::RFind(const nsStr& aString,PRBool aIgnoreCase,PRInt32 anOffset) const{
  PRInt32 result=nsStr::RFindSubstr(*this,aString,aIgnoreCase,anOffset);
  return result;
}

/**
 *  
 *  
 *  @update  gess 3/25/98
 *  @param   
 *  @return  
 */
PRInt32 nsString::RFind(const nsString& aString,PRBool aIgnoreCase,PRInt32 anOffset) const{
  PRInt32 result=nsStr::RFindSubstr(*this,aString,aIgnoreCase,anOffset);
  return result;
}

/**
 *  
 *  
 *  @update  gess 3/25/98
 *  @param   
 *  @return  
 */
PRInt32 nsString::RFind(const char* aString,PRBool aIgnoreCase,PRInt32 anOffset) const{
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
 *  Search for a given char, starting at given offset
 *  
 *  @update  gess 3/25/98
 *  @param   
 *  @return  offset of found char, or -1 (kNotFound)
 */
PRInt32 nsString::RFind(PRUnichar aChar,PRInt32 anOffset,PRBool aIgnoreCase) const{
  PRInt32 result=nsStr::RFindChar(*this,aChar,aIgnoreCase,anOffset);
  return result;
}
 
/**
 *  Search for a given char, starting at given offset
 *  
 *  @update  gess 3/25/98
 *  @param   
 *  @return  offset of found char, or -1 (kNotFound)
 */
PRInt32 nsString::RFindChar(PRUnichar aChar,PRBool aIgnoreCase,PRInt32 anOffset) const{
  PRInt32 result=nsStr::RFindChar(*this,aChar,aIgnoreCase,anOffset);
  return result;
}

/**
 *  
 *  
 *  @update  gess 3/25/98
 *  @param   
 *  @return   
 */
PRInt32 nsString::RFindCharInSet(const char* aCStringSet,PRInt32 anOffset) const{
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
 *  
 *  
 *  @update  gess 3/25/98
 *  @param   
 *  @return   
 */
PRInt32 nsString::RFindCharInSet(const nsStr& aSet,PRInt32 anOffset) const{
  PRInt32 result=nsStr::RFindCharInSet(*this,aSet,PR_FALSE,anOffset);
  return result;
}

/**
 *  
 *  
 *  @update  gess 3/25/98
 *  @param   
 *  @return   
 */
PRInt32 nsString::RFindCharInSet(const PRUnichar* aStringSet,PRInt32 anOffset) const{
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


/**************************************************************
  COMPARISON METHODS...
 **************************************************************/

/**
 * Compares given cstring to this string. 
 * @update	gess 01/04/99
 * @param   aCString pts to a cstring
 * @param   aIgnoreCase tells us how to treat case
 * @param   aCount tells us how many chars to test; -1 implies full length
 * @return  -1,0,1
 */
PRInt32 nsString::Compare(const nsString& aString,PRBool aIgnoreCase,PRInt32 aCount) const {
  PRInt32 result=nsStr::Compare(*this,aString,aCount,aIgnoreCase);
  return result;
}

/**
 * Compares given cstring to this string. 
 * @update	gess 01/04/99
 * @param   aCString pts to a cstring
 * @param   aIgnoreCase tells us how to treat case
 * @param   aCount tells us how many chars to test; -1 implies full length
 * @return  -1,0,1
 */
PRInt32 nsString::Compare(const char *aCString,PRBool aIgnoreCase,PRInt32 aCount) const {
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
 * @update	gess 01/04/99
 * @param   aString pts to a uni-string
 * @param   aIgnoreCase tells us how to treat case
 * @param   aCount tells us how many chars to test; -1 implies full length
 * @return  -1,0,1
 */
PRInt32 nsString::Compare(const PRUnichar* aString,PRBool aIgnoreCase,PRInt32 aCount) const {
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
PRInt32 nsString::Compare(const nsStr& aString,PRBool aIgnoreCase,PRInt32 aCount) const {
  return nsStr::Compare(*this,aString,aCount,aIgnoreCase);
}


PRBool nsString::operator==(const nsString& S) const {return Equals(S);}      
PRBool nsString::operator==(const nsStr& S) const {return Equals(S);}      
PRBool nsString::operator==(const char* s) const {return Equals(s);}
PRBool nsString::operator==(const PRUnichar* s) const {return Equals(s);}

PRBool nsString::operator!=(const nsString& S) const {return PRBool(Compare(S)!=0);}
PRBool nsString::operator!=(const nsStr& S) const {return PRBool(Compare(S)!=0);}
PRBool nsString::operator!=(const char* s) const {return PRBool(Compare(s)!=0);}
PRBool nsString::operator!=(const PRUnichar* s) const {return PRBool(Compare(s)!=0);}

PRBool nsString::operator<(const nsString& S) const {return PRBool(Compare(S)<0);}
PRBool nsString::operator<(const nsStr& S) const {return PRBool(Compare(S)<0);}
PRBool nsString::operator<(const char* s) const {return PRBool(Compare(s)<0);}
PRBool nsString::operator<(const PRUnichar* s) const {return PRBool(Compare(s)<0);}

PRBool nsString::operator>(const nsString& S) const {return PRBool(Compare(S)>0);}
PRBool nsString::operator>(const nsStr& S) const {return PRBool(Compare(S)>0);}
PRBool nsString::operator>(const char* s) const {return PRBool(Compare(s)>0);}
PRBool nsString::operator>(const PRUnichar* s) const {return PRBool(Compare(s)>0);}

PRBool nsString::operator<=(const nsString& S) const {return PRBool(Compare(S)<=0);}
PRBool nsString::operator<=(const nsStr& S) const {return PRBool(Compare(S)<=0);}
PRBool nsString::operator<=(const char* s) const {return PRBool(Compare(s)<=0);}
PRBool nsString::operator<=(const PRUnichar* s) const {return PRBool(Compare(s)<=0);}

PRBool nsString::operator>=(const nsString& S) const {return PRBool(Compare(S)>=0);}
PRBool nsString::operator>=(const nsStr& S) const {return PRBool(Compare(S)>=0);}
PRBool nsString::operator>=(const char* s) const {return PRBool(Compare(s)>=0);}
PRBool nsString::operator>=(const PRUnichar* s) const {return PRBool(Compare(s)>=0);}


PRBool nsString::EqualsIgnoreCase(const nsString& aString) const {
  return Equals(aString,PR_TRUE);
}

PRBool nsString::EqualsIgnoreCase(const char* aString,PRInt32 aLength) const {
  return Equals(aString,PR_TRUE,aLength);
}

PRBool nsString::EqualsIgnoreCase(const nsIAtom *aAtom) const {
  return Equals(aAtom,PR_TRUE);
}

PRBool nsString::EqualsIgnoreCase(const PRUnichar* s1, const PRUnichar* s2) const {
  return Equals(s1,s2,PR_TRUE);
}

/**
 * Compare this to given string; note that we compare full strings here.
 * 
 * @update gess 01/04/99
 * @param  aString is the other nsString to be compared to
 * @param   aCount tells us how many chars to test; -1 implies full length
 * @return TRUE if equal
 */
PRBool nsString::Equals(const nsString& aString,PRBool aIgnoreCase,PRInt32 aCount) const {
  PRInt32 theAnswer=nsStr::Compare(*this,aString,aCount,aIgnoreCase);
  PRBool  result=PRBool(0==theAnswer);
  return result;

}

/**
 * Compare this to given string; note that we compare full strings here.
 * 
 * @update gess 01/04/99
 * @param  aString is the other nsString to be compared to
 * @param   aCount tells us how many chars to test; -1 implies full length
 * @return TRUE if equal
 */
PRBool nsString::Equals(const nsStr& aString,PRBool aIgnoreCase,PRInt32 aCount) const {
  PRInt32 theAnswer=nsStr::Compare(*this,aString,aCount,aIgnoreCase);
  PRBool  result=PRBool(0==theAnswer);
  return result;
}

/**
 * Compare this to given c-string; note that we compare full strings here.
 *
 * @param  aString is the CString to be compared 
 * @param  aIgnorecase tells us whether to be case sensitive
 * @param   aCount tells us how many chars to test; -1 implies full length
 * @return TRUE if equal
 */
PRBool nsString::Equals(const char* aString,PRBool aIgnoreCase,PRInt32 aCount) const {
  PRInt32 theAnswer=Compare(aString,aIgnoreCase,aCount);
  PRBool  result=PRBool(0==theAnswer);
  return result;
}

/**
 * Compare this to given unicode string; note that we compare full strings here.
 *
 * @param  aString is the U-String to be compared 
 * @param  aIgnorecase tells us whether to be case sensitive
 * @param   aCount tells us how many chars to test; -1 implies full length
 * @return TRUE if equal
 */
PRBool nsString::Equals(const PRUnichar* aString,PRBool aIgnoreCase,PRInt32 aCount) const {
  PRInt32 theAnswer=Compare(aString,aIgnoreCase,aCount);
  PRBool  result=PRBool(0==theAnswer);
  return result;
}

/**
 * Compare this to given atom; note that we compare full strings here.
 * The optional length argument just lets us know how long the given string is.
 * If you provide a length, it is compared to length of this string as an
 * optimization.
 * 
 * @update gess 01/04/99
 * @param  aString -- unistring to compare to this
 * @param  aLength -- length of given string.
 * @return TRUE if equal
 */
PRBool nsString::Equals(const nsIAtom* aAtom,PRBool aIgnoreCase) const{
  NS_ASSERTION(0!=aAtom,kNullPointerError);
  PRBool result=PR_FALSE;
  if(aAtom){
    PRInt32 cmp=0;
    if (aIgnoreCase)
      cmp=nsCRT::strcasecmp(mUStr,aAtom->GetUnicode());
    else
      cmp=nsCRT::strcmp(mUStr,aAtom->GetUnicode());
    result=PRBool(0==cmp);
  }

   return result;
}

/**
 * Compare given strings
 * @update gess 7/27/98
 * @param  s1 -- first unichar string to be compared
 * @param  s2 -- second unichar string to be compared
 * @return TRUE if equal
 */
PRBool nsString::Equals(const PRUnichar* s1, const PRUnichar* s2,PRBool aIgnoreCase) const {
  NS_ASSERTION(0!=s1,kNullPointerError);
  NS_ASSERTION(0!=s2,kNullPointerError);
  PRBool  result=PR_FALSE;
  if((s1) && (s2)){
    PRInt32 cmp=(aIgnoreCase) ? nsCRT::strcasecmp(s1,s2) : nsCRT::strcmp(s1,s2);
    result=PRBool(0==cmp);
  }

   return result;
}

/**
 *  Determine if given char in valid alpha range
 *  
 *  @update  gess 3/31/98
 *  @param   aChar is character to be tested
 *  @return  TRUE if in alpha range
 */
PRBool nsString::IsAlpha(PRUnichar aChar) {
  // XXX i18n
  if (((aChar >= 'A') && (aChar <= 'Z')) || ((aChar >= 'a') && (aChar <= 'z'))) {
    return PR_TRUE;
  }
  return PR_FALSE;
}

/**
 *  Determine if given char is a valid space character
 *  
 *  @update  gess 3/31/98
 *  @param   aChar is character to be tested
 *  @return  TRUE if is valid space char
 */
PRBool nsString::IsSpace(PRUnichar aChar) {
  // XXX i18n
  if ((aChar == ' ') || (aChar == '\r') || (aChar == '\n') || (aChar == '\t')) {
    return PR_TRUE;
  }
  return PR_FALSE;
}

/**
 *  Determine if given buffer contains plain ascii
 *  
 *  @param   aBuffer -- if null, then we test *this, otherwise we test given buffer
 *  @return  TRUE if is all ascii chars, or if strlen==0
 */
PRBool nsString::IsASCII(const PRUnichar* aBuffer) {

  if(!aBuffer) {
    aBuffer=mUStr;
    if(eOneByte==mCharSize)
      return PR_TRUE;
  }
  if(aBuffer) {
    while(*aBuffer) {
      if(*aBuffer>255){
        return PR_FALSE;        
      }
      aBuffer++;
    }
  }
  return PR_TRUE;
}


/**
 *  Determine if given char is valid digit
 *  
 *  @update  gess 3/31/98
 *  @param   aChar is character to be tested
 *  @return  TRUE if char is a valid digit
 */
PRBool nsString::IsDigit(PRUnichar aChar) {
  // XXX i18n
  return PRBool((aChar >= '0') && (aChar <= '9'));
}

#ifndef RICKG_TESTBED
/**************************************************************
  Define the string deallocator class...
 **************************************************************/
class nsStringDeallocator: public nsDequeFunctor{
public:
  virtual void* operator()(void* anObject) {
    static nsMemoryAgent theAgent;
    nsString* aString= (nsString*)anObject;
    if(aString){
      aString->mAgent=&theAgent;
      delete aString;
    }
    return 0;
  }
};

/****************************************************************************
 * This class, appropriately enough, creates and recycles nsString objects..
 ****************************************************************************/

class nsStringRecycler {
public:
  nsStringRecycler() : mDeque(0) {
  }

  ~nsStringRecycler() {
    nsStringDeallocator theDeallocator;
    mDeque.ForEach(theDeallocator); //now delete the strings
  }

  void Recycle(nsString* aString) {
    mDeque.Push(aString);
  }

  nsString* CreateString(eCharSize aCharSize){
    nsString* result=(nsString*)mDeque.Pop();
    if(!result)
      result=new nsString(aCharSize);
    return result;
  }
  nsDeque mDeque;
};
static nsStringRecycler& GetRecycler(void);

/**
 * 
 * @update	gess 01/04/99
 * @param 
 * @return
 */
nsStringRecycler& GetRecycler(void){
  static nsStringRecycler gRecycler;
  return gRecycler;
}
#endif


/**
 * Call this mehod when you're done 
 * @update	gess 01/04/99
 * @param 
 * @return
 */
nsString* nsString::CreateString(eCharSize aCharSize){
  nsString* result=0;
#ifndef RICKG_TESTBED
  GetRecycler().CreateString(aCharSize);
#endif
  return result;
}

/**
 * Call this mehod when you're done 
 * @update	gess 01/04/99
 * @param 
 * @return
 */
void nsString::Recycle(nsString* aString){
#ifndef RICKG_TESTBED
  GetRecycler().Recycle(aString);
#else
  delete aString;
#endif
}

#if 0
/**
 * 
 * @update	gess 01/04/99
 * @param 
 * @return
 */
void nsString::DebugDump(ostream& aStream) const {
  for(PRUint32 i=0;i<mLength;i++) {
    aStream <<mStr[i];
  }
  aStream << endl;
}

/**
 * 
 * @update	gess8/8/98
 * @param 
 * @return
 */
ostream& operator<<(ostream& aStream,const nsString& aString){
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
 * @update	gess 01/04/99
 * @param 
 * @return
 */
NS_COM int fputs(const nsString& aString, FILE* out)
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
 * Special case constructor, that allows the consumer to provide
 * an underlying buffer for performance reasons.
 * @param   aBuffer points to your buffer
 * @param   aBufSize defines the size of your buffer
 * @param   aCurrentLength tells us the current length of the buffer
 */
nsAutoString::nsAutoString(eCharSize aCharSize) : nsString(aCharSize){
  nsStr::Initialize(*this,mBuffer,(sizeof(mBuffer)>>aCharSize)-1,0,aCharSize,PR_FALSE);
  mAgent=0;
  AddNullTerminator(*this);

}

/**
 * Copy construct from ascii c-string
 * @param   aCString is a ptr to a 1-byte cstr
 */
nsAutoString::nsAutoString(const char* aCString,eCharSize aCharSize,PRInt32 aLength) : nsString(aCharSize) {
  nsStr::Initialize(*this,mBuffer,(sizeof(mBuffer)>>aCharSize)-1,0,aCharSize,PR_FALSE);
  mAgent=0;
  AddNullTerminator(*this);
  Append(aCString,aLength);
  

}

/**
 * Copy construct from uni-string
 * @param   aString is a ptr to a unistr
 */
nsAutoString::nsAutoString(const PRUnichar* aString,eCharSize aCharSize,PRInt32 aLength) : nsString(aCharSize) {
  mAgent=0;
  nsStr::Initialize(*this,mBuffer,(sizeof(mBuffer)>>aCharSize)-1,0,aCharSize,PR_FALSE);
  AddNullTerminator(*this);
  Append(aString,aLength);
  
}

/**
 * Copy construct from uni-string
 * @param   aString is a ptr to a unistr
 */
nsAutoString::nsAutoString(CBufDescriptor& aBuffer) : nsString(eTwoByte) {
  mAgent=0;
  if(!aBuffer.mBuffer) {
    nsStr::Initialize(*this,mBuffer,(sizeof(mBuffer)>>eTwoByte)-1,0,eTwoByte,PR_FALSE);
  }
  else {
    nsStr::Initialize(*this,aBuffer.mBuffer,aBuffer.mCapacity,aBuffer.mLength,aBuffer.mCharSize,!aBuffer.mStackBased);
  }
  AddNullTerminator(*this);
}


/**
 * Copy construct from an nsString
 * @param   
 */
nsAutoString::nsAutoString(const nsStr& aString,eCharSize aCharSize) : nsString(aCharSize) {
  mAgent=0;
  nsStr::Initialize(*this,mBuffer,(sizeof(mBuffer)>>aCharSize)-1,0,aCharSize,PR_FALSE);
  AddNullTerminator(*this);
  Append(aString);
  
}

/**
 * Copy construct from an nsString
 * @param   
 */
nsAutoString::nsAutoString(const nsAutoString& aString,eCharSize aCharSize) : nsString(aCharSize) {
  mAgent=0;
  nsStr::Initialize(*this,mBuffer,(sizeof(mBuffer)>>aCharSize)-1,0,aCharSize,PR_FALSE);
  AddNullTerminator(*this);
  Append(aString);
  
}


/**
 * Copy construct from an nsString
 * @param   
 */
nsAutoString::nsAutoString(PRUnichar aChar,eCharSize aCharSize) : nsString(aCharSize){
  mAgent=0;
  nsStr::Initialize(*this,mBuffer,(sizeof(mBuffer)>>aCharSize)-1,0,aCharSize,PR_FALSE);
  AddNullTerminator(*this);
  Append(aChar);
  
}

/**
 * construct from a subsumeable string
 * @update	gess 1/4/99
 * @param   reference to a subsumeString
 */
#ifdef AIX
nsAutoString::nsAutoString(const nsSubsumeStr& aSubsumeStr) :nsString(aSubsumeStr.mCharSize) {
  mAgent=0;
  nsSubsumeStr temp(aSubsumeStr);  // a temp is needed for the AIX compiler
  Subsume(*this,temp);
#else
nsAutoString::nsAutoString( nsSubsumeStr& aSubsumeStr) :nsString(aSubsumeStr.mCharSize) {
  mAgent=0;
  Subsume(*this,aSubsumeStr);
#endif // AIX
}

/**
 * deconstruct the autstring
 * @param   
 */
nsAutoString::~nsAutoString(){
}

void nsAutoString::SizeOf(nsISizeOfHandler* aHandler) const {
#ifndef RICKG_TESTBED
  aHandler->Add(sizeof(*this));
  aHandler->Add(mCapacity << mCharSize);
#endif
}

nsSubsumeStr::nsSubsumeStr(nsStr& aString) : nsString(aString.mCharSize) {
  Subsume(*this,aString);
}

nsSubsumeStr::nsSubsumeStr(PRUnichar* aString,PRBool assumeOwnership,PRInt32 aLength) : nsString(eTwoByte) {
  mUStr=aString;
  mCapacity=mLength=(-1==aLength) ? nsCRT::strlen(aString) : aLength-1;
  mOwnsBuffer=assumeOwnership;
}
 
nsSubsumeStr::nsSubsumeStr(char* aString,PRBool assumeOwnership,PRInt32 aLength) : nsString(eOneByte) {
  mStr=aString;
  mCapacity=mLength=(-1==aLength) ? strlen(aString) : aLength-1;
  mOwnsBuffer=assumeOwnership;
}

