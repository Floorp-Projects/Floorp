/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Original Author:
 *   Rick Gessner <rickg@netscape.com>
 *
 * Contributor(s): 
 *   Scott Collins <scc@mozilla.org>
 */ 

#include <ctype.h>
#include <string.h> 
#include <stdio.h>
#include <stdlib.h>
#include "nsString.h"
#include "nsDebug.h"
#include "nsCRT.h"
#include "nsDeque.h"

#ifndef RICKG_TESTBED
#include "prdtoa.h"
#include "nsISizeOfHandler.h"
#endif

static const char* kPossibleNull = "Error: possible unintended null in string";
static const char* kNullPointerError = "Error: unexpected null ptr";
static const char* kWhitespace="\b\t\r\n ";


static void CSubsume(nsStr& aDest,nsStr& aSource){
  if(aSource.mStr && aSource.mLength) {
    if(aSource.mOwnsBuffer){
      nsStr::Destroy(aDest);
      aDest.mStr=aSource.mStr;
      aDest.mLength=aSource.mLength;
      aDest.mCharSize=aSource.mCharSize;
      aDest.mCapacity=aSource.mCapacity;
      aDest.mOwnsBuffer=aSource.mOwnsBuffer;
      aSource.mOwnsBuffer=PR_FALSE;
      aSource.mStr=0;
    }
    else{ 
      nsStr::StrAssign(aDest,aSource,0,aSource.mLength);
    }
  } 
  else nsStr::Truncate(aDest,0);
}


/**
 * Default constructor. 
 */
nsCString::nsCString()  {
  Initialize(*this,eOneByte);
}

#ifdef NEW_STRING_APIS
nsCString::nsCString(const char* aCString) {  
  Initialize(*this,eOneByte);
  Assign(aCString);
}
#endif

/**
 * This constructor accepts an ascii string
 * @update  gess 1/4/99
 * @param   aCString is a ptr to a 1-byte cstr
 * @param   aLength tells us how many chars to copy from given CString
 */
nsCString::nsCString(const char* aCString,PRInt32 aLength) {  
  Initialize(*this,eOneByte);
  Assign(aCString,aLength);
}

#if 0
/**
 * This constructor accepts a unicode string
 * @update  gess 1/4/99
 * @param   aString is a ptr to a unichar string
 * @param   aLength tells us how many chars to copy from given aString
 */
nsCString::nsCString(const PRUnichar* aString,PRInt32 aLength)  {  
  Initialize(*this,eOneByte);
  AssignWithConversion(aString,aLength);
}

/**
 * This constructor works for all other nsSTr derivatives
 * @update  gess 1/4/99
 * @param   reference to another nsCString
 */
nsCString::nsCString(const nsStr &aString)  {
  Initialize(*this,eOneByte);
  StrAssign(*this,aString,0,aString.mLength);
}
#endif

/**
 * This is our copy constructor 
 * @update  gess 1/4/99
 * @param   reference to another nsCString
 */
nsCString::nsCString(const nsCString& aString)  {
  Initialize(*this,aString.mCharSize);
  StrAssign(*this,aString,0,aString.mLength);
}

/**
 * construct from subsumeable string
 * @update  gess 1/4/99
 * @param   reference to a subsumeString
 */
nsCString::nsCString(nsSubsumeCStr& aSubsumeStr)  {
  CSubsume(*this,aSubsumeStr);
}

/**
 * Destructor
 */
nsCString::~nsCString() {
  Destroy(*this);
}

#ifdef NEW_STRING_APIS
const char* nsCString::GetReadableFragment( nsReadableFragment<char>& aFragment, nsFragmentRequest aRequest, PRUint32 aOffset ) const {
  switch ( aRequest ) {
    case kFirstFragment:
    case kLastFragment:
    case kFragmentAt:
      aFragment.mEnd = (aFragment.mStart = mStr) + mLength;
      return aFragment.mStart + aOffset;
    
    case kPrevFragment:
    case kNextFragment:
    default:
      return 0;
  }
}

char* nsCString::GetWritableFragment( nsWritableFragment<char>& aFragment, nsFragmentRequest aRequest, PRUint32 aOffset ) {
  switch ( aRequest ) {
    case kFirstFragment:
    case kLastFragment:
    case kFragmentAt:
      aFragment.mEnd = (aFragment.mStart = mStr) + mLength;
      return aFragment.mStart + aOffset;
    
    case kPrevFragment:
    case kNextFragment:
    default:
      return 0;
  }
}

nsCString::nsCString( const nsAReadableCString& aReadable ) {
  Initialize(*this,eOneByte);
  Assign(aReadable);
}
#endif

void nsCString::SizeOf(nsISizeOfHandler* aHandler, PRUint32* aResult) const {
  if (aResult) {
    *aResult = sizeof(*this) + mCapacity * mCharSize;
  }
}

/**
 * This method truncates this string to given length.
 *
 * @update  rickg 03.23.2000
 * @param   anIndex -- new length of string
 * @return  nada
 */
void nsCString::SetLength(PRUint32 anIndex) {
  if ( anIndex > mCapacity )
    SetCapacity(anIndex);
    // |SetCapacity| normally doesn't guarantee the use we are putting it to here (see its interface comment in nsAWritableString.h),
    //  we can only use it since our local implementation, |nsCString::SetCapacity|, is known to do what we want

  nsStr::Truncate(*this,anIndex);
}


/**
 * Call this method if you want to force the string to a certain capacity;
 * |SetCapacity(0)| discards associated storage.
 *
 * @param   aNewCapacity -- desired minimum capacity
 */
void
nsCString::SetCapacity( PRUint32 aNewCapacity )
  {
    if ( aNewCapacity )
      {
        if( aNewCapacity > mCapacity )
          GrowCapacity(*this,aNewCapacity);
        AddNullTerminator(*this);
      }
    else
      {
        nsStr::Destroy(*this);
        nsStr::Initialize(*this, eOneByte);
      }
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

#ifndef NEW_STRING_APIS
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
#endif // !defined(NEW_STRING_APIS)

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
    result=PR_TRUE;
  }
  return result;
}


/*********************************************************
  append (operator+) METHODS....
 *********************************************************/
#ifndef NEW_STRING_APIS
/**
 * Create a new string by appending given string to this
 * @update  gess 01/04/99
 * @param   aString -- 2nd string to be appended
 * @return  new subsumeable string (this makes things faster by reducing copying)
 */
nsSubsumeCStr nsCString::operator+(const nsCString& aString){
  nsCString temp(*this); //make a temp string the same size as this...
  nsStr::StrAppend(temp,aString,0,aString.mLength);
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
#endif // !defined(NEW_STRING_APIS)

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
 *  @update rickg 03.27.2000
 *  @param  aChar -- char to be stripped
 *  @return *this 
 */
void
nsCString::StripChar(PRUnichar aChar,PRInt32 anOffset){
  if(mLength && (anOffset<PRInt32(mLength))) {
    if(eOneByte==mCharSize) {
      char*  to   = mStr + anOffset;
      char*  from = mStr + anOffset;
      char*  end  = mStr + mLength;

      while (from < end) {
        char theChar = *from++;
        if(aChar!=theChar) {
          *to++ = theChar;
        }
      }
      *to = 0; //add the null
      mLength=to - mStr;
    }
    else {
      PRUnichar*  to   = mUStr + anOffset;
      PRUnichar*  from = mUStr + anOffset;
      PRUnichar*  end  = mUStr + mLength;

      while (from < end) {
        PRUnichar theChar = *from++;
        if(aChar!=theChar) {
          *to++ = theChar;
        }
      }
      *to = 0; //add the null
      mLength=to - mUStr;
    }
  }
}

/**
 *  This method is used to remove all occurances of the
 *  characters found in aSet from this string.
 *  
 *  @update gess 01/04/99
 *  @param  aSet -- characters to be cut from this
 *  @return *this 
 */
void
nsCString::StripChars(const char* aSet){
  nsStr::StripChars(*this,aSet);
}


/**
 *  This method strips whitespace throughout the string
 *  
 *  @update  gess 01/04/99
 *  @return  this
 */
void
nsCString::StripWhitespace() {
  StripChars(kWhitespace);
}

/**
 *  This method is used to replace all occurances of the
 *  given source char with the given dest char
 *  
 *  @param  
 *  @return *this 
 */
void
nsCString::ReplaceChar(PRUnichar aOldChar, PRUnichar aNewChar) {
  PRUint32 theIndex=0;
  
  if((aOldChar<256) && (aNewChar<256)){
      //only execute this if oldchar and newchar are within legal ascii range
    for(theIndex=0;theIndex<mLength;theIndex++){
      if(mStr[theIndex]==(char)aOldChar) {
        mStr[theIndex]=(char)aNewChar;
      }//if
    }
  }
}

/**
 *  This method is used to replace all occurances of the
 *  given source char with the given dest char
 *  
 *  @param  
 *  @return *this 
 */
void
nsCString::ReplaceChar(const char* aSet, PRUnichar aNewChar){
  if(aSet && (aNewChar<256)){
      //only execute this if newchar is valid ascii, and aset isn't null.

    PRInt32 theIndex=FindCharInSet(aSet,0);
    while(kNotFound<theIndex) {
      mStr[theIndex]=(char)aNewChar;
      theIndex=FindCharInSet(aSet,theIndex+1);
    }
  }
}

/**
 *  This method is used to replace all occurances of the
 *  given target with the given replacement
 *  
 *  @param  
 *  @return *this 
 */
void
nsCString::ReplaceSubstring(const char* aTarget,const char* aNewValue){
  if(aTarget && aNewValue) {

    PRInt32 len=strlen(aTarget);
    if(0<len) {
      CBufDescriptor theDesc1(aTarget,PR_TRUE, len+1,len);
      nsCAutoString theTarget(theDesc1);

      len=strlen(aNewValue);
      if(0<len) {
        CBufDescriptor theDesc2(aNewValue,PR_TRUE, len+1,len);
        nsCAutoString theNewValue(theDesc2);

        ReplaceSubstring(theTarget,theNewValue);
      }
    }
  }
}

/**
 *  This method is used to replace all occurances of the
 *  given target substring with the given replacement substring
 *  
 *  @param aTarget
 *  @param aNewValue
 *  @return *this 
 */
void
nsCString::ReplaceSubstring(const nsCString& aTarget,const nsCString& aNewValue){


  //WARNING: This is not working yet!!!!!

  if(aTarget.mLength && aNewValue.mLength) {
    PRBool isSameLen=(aTarget.mLength==aNewValue.mLength);

    if((isSameLen) && (1==aNewValue.mLength)) {
      ReplaceChar(aTarget.CharAt(0),aNewValue.CharAt(0));
    }
    else {
      PRInt32 theIndex=0;
      while(kNotFound!=(theIndex=nsStr::FindSubstr(*this,aTarget,PR_FALSE,theIndex,mLength))) {
        if(aNewValue.mLength<aTarget.mLength) {
          //Since target is longer than newValue, we should delete a few chars first, then overwrite.
          PRInt32 theDelLen=aTarget.mLength-aNewValue.mLength;
          nsStr::Delete(*this,theIndex,theDelLen);
        }
        else {
          //this is the worst case: the newvalue is larger than the substr it's replacing
          //so we have to insert some characters...
          PRInt32 theInsLen=aNewValue.mLength-aTarget.mLength;
          StrInsert(*this,theIndex,aNewValue,0,theInsLen);
        }
        nsStr::Overwrite(*this,aNewValue,theIndex);
      }
    }
  }
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
void
nsCString::Trim(const char* aTrimSet, PRBool aEliminateLeading,PRBool aEliminateTrailing,PRBool aIgnoreQuotes){

  if(aTrimSet){
    
    PRUnichar theFirstChar=0;
    PRUnichar theLastChar=0;
    PRBool    theQuotesAreNeeded=PR_FALSE;

    if(aIgnoreQuotes && (mLength>2)) {
      theFirstChar=First();    
      theLastChar=Last();
      if(theFirstChar==theLastChar) {
        if(('\''==theFirstChar) || ('"'==theFirstChar)) {
          Cut(0,1);
          Truncate(mLength-1);
          theQuotesAreNeeded=PR_TRUE;
        }
        else theFirstChar=0;
      }
    }
    
    nsStr::Trim(*this,aTrimSet,aEliminateLeading,aEliminateTrailing);

    if(aIgnoreQuotes && theQuotesAreNeeded) {
      InsertWithConversion(theFirstChar,0);
      AppendWithConversion(theLastChar);
    }

  }
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
void
nsCString::CompressSet(const char* aSet, PRUnichar aChar,PRBool aEliminateLeading,PRBool aEliminateTrailing){
  if(aSet){
    ReplaceChar(aSet,aChar);
    nsStr::CompressSet(*this,aSet,aEliminateLeading,aEliminateTrailing);
  }
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
void
nsCString::CompressWhitespace( PRBool aEliminateLeading,PRBool aEliminateTrailing){
  CompressSet(kWhitespace,' ',aEliminateLeading,aEliminateTrailing);
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
 * @update  gess 02/24/00
 * @return  ptr to new ascii string
 */
char* nsCString::ToNewCString() const {
  return nsCRT::strdup(mStr);
}

/**
 * Creates an unicode clone of this string
 * Note that calls to this method should be matched with calls to Recycle().
 * @update  gess 01/04/99
 * @return  ptr to new ascii string
 */
PRUnichar* nsCString::ToNewUnicode() const {
  PRUnichar* result = NS_STATIC_CAST(PRUnichar*, nsMemory::Alloc(sizeof(PRUnichar) * (mLength + 1)));
  if (result) {
    CBufDescriptor desc(result, PR_TRUE, mLength + 1, 0);
    nsAutoString temp(desc);
    temp.AssignWithConversion(*this);
  }
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
    temp.Assign(*this, PR_MIN(mLength, aBufLength-1));
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
 * Perform decimal numeric string to int conversion.
 * NOTE: In this version, we use the radix you give, even if it's wrong.
 * @update  gess 02/14/00
 * @param   aErrorCode will contain error if one occurs
 * @param   aRadix tells us what base to expect the given string in. kAutoDetect tells us to determine the radix.
 * @return  int rep of string value
 */
PRInt32 nsCString::ToInteger(PRInt32* anErrorCode,PRUint32 aRadix) const {
  char*       cp=mStr;
  PRInt32     theRadix=10; // base 10 unless base 16 detected, or overriden (aRadix != kAutoDetect)
  PRInt32     result=0;
  PRBool      negate=PR_FALSE;
  char        theChar=0;

    //initial value, override if we find an integer
  *anErrorCode=NS_ERROR_ILLEGAL_VALUE;
  
  if(cp) {
  
    //begin by skipping over leading chars that shouldn't be part of the number...
    
    char*  endcp=cp+mLength;
    PRBool done=PR_FALSE;
    
    while((cp<endcp) && (!done)){
      switch(*cp++) {
        case 'a': case 'b': case 'c': case 'd': case 'e': case 'f':
        case 'A': case 'B': case 'C': case 'D': case 'E': case 'F':
          theRadix=16;
          done=PR_TRUE;
          break;
        case '0': case '1': case '2': case '3': case '4': 
        case '5': case '6': case '7': case '8': case '9':
          done=PR_TRUE;
          break;
        case '-': 
          negate=PR_TRUE; //fall through...
          break;
        case 'X': case 'x': 
          theRadix=16;
          break; 
        default:
          break;
      } //switch
    }

    if (done) {

        //integer found
      *anErrorCode = NS_OK;

      if (aRadix!=kAutoDetect) theRadix = aRadix; // override

        //now iterate the numeric chars and build our result
      char* first=--cp;  //in case we have to back up.

      while(cp<endcp){
        theChar=*cp++;
        if(('0'<=theChar) && (theChar<='9')){
          result = (theRadix * result) + (theChar-'0');
        }
        else if((theChar>='A') && (theChar<='F')) {
          if(10==theRadix) {
            if(kAutoDetect==aRadix){
              theRadix=16;
              cp=first; //backup
              result=0;
            }
            else {
              *anErrorCode=NS_ERROR_ILLEGAL_VALUE;
              result=0;
              break;
            }
          }
          else {
            result = (theRadix * result) + ((theChar-'A')+10);
          }
        }
        else if((theChar>='a') && (theChar<='f')) {
          if(10==theRadix) {
            if(kAutoDetect==aRadix){
              theRadix=16;
              cp=first; //backup
              result=0;
            }
            else {
              *anErrorCode=NS_ERROR_ILLEGAL_VALUE;
              result=0;
              break;
            }
          }
          else {
            result = (theRadix * result) + ((theChar-'a')+10);
          }
        }
        else if(('X'==theChar) || ('x'==theChar) || ('#'==theChar) || ('+'==theChar)) {
          continue;
        }
        else {
          //we've encountered a char that's not a legal number or sign
          break;
        }
      } //while
      if(negate)
        result=-result;
    } //if
  }
  return result;
}

/**********************************************************************
  String manipulation methods...                
 *********************************************************************/

#ifndef NEW_STRING_APIS
/**
 * assign given nsStr (or derivative) to this one
 * @update  gess 01/04/99
 * @param   aString: string to be appended
 * @return  this
 */
nsCString& nsCString::Assign(const nsStr& aString,PRInt32 aCount) {
  if(this!=&aString){
    nsStr::Truncate(*this,0);

    if(aCount<0)
      aCount=aString.mLength;
    else aCount=MinInt(aCount,aString.mLength);

    StrAssign(*this,aString,0,aCount);
  }
  return *this;
}
  
/**
 * assign given char* to this string
 * @update  gess 01/04/99
 * @param   aCString: buffer to be assigned to this 
 * @param   aCount -- length of given buffer or -1 if you want me to compute length.
 * NOTE:    IFF you pass -1 as aCount, then your buffer must be null terminated.
 *
 * @return  this
 */
nsCString& nsCString::Assign(const char* aCString,PRInt32 aCount) {
  nsStr::Truncate(*this,0);
  if(aCString){
    Append(aCString,aCount);
  }
  return *this;
}

/**
 * assign given char to this string
 * @update  gess 01/04/99
 * @param   aChar: char to be assignd to this
 * @return  this
 */
nsCString& nsCString::Assign(char aChar) {
  nsStr::Truncate(*this,0);
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
#if defined(AIX) || defined(XP_OS2_VACPP)
nsCString& nsCString::operator=(const nsSubsumeCStr& aSubsumeString) {
  nsSubsumeCStr temp(aSubsumeString);  // a temp is needed for the AIX and VAC++ compiler
  CSubsume(*this,temp);
#else
nsCString& nsCString::operator=(nsSubsumeCStr& aSubsumeString) {
  CSubsume(*this,aSubsumeString);
#endif // AIX || XP_OS2_VACPP
  return *this;
}
#endif

/**
 * assign given unichar* to this string
 * @update  gess 01/04/99
 * @param   aCString: buffer to be assigned to this 
 * @param   aCount -- length of given buffer or -1 if you want me to compute length.
 * NOTE:    IFF you pass -1 as aCount, then your buffer must be null terminated.
 *
 * @return  this
 */
void nsCString::AssignWithConversion(const PRUnichar* aString,PRInt32 aCount) {
  nsStr::Truncate(*this,0);

  if(aString && aCount){
    nsStr temp;
    nsStr::Initialize(temp,eTwoByte);
    temp.mUStr=(PRUnichar*)aString;

    if(0<aCount) {
      temp.mLength=aCount;

      // If this assertion fires, the caller is probably lying about the length of
      //   the passed-in string.  File a bug on the caller.
#ifdef NS_DEBUG
      PRInt32 len=nsStr::FindChar(temp,0,PR_FALSE,0,temp.mLength);
      if(kNotFound<len) {
        NS_WARNING(kPossibleNull);
      }
#endif

    }
    else aCount=temp.mLength=nsCRT::strlen(aString);

    if(0<aCount)
      StrAppend(*this,temp,0,aCount);
  }
}

void nsCString::AssignWithConversion( const nsString& aString ) {
  AssignWithConversion(aString.GetUnicode(), aString.Length());
}



/**
 * assign given unichar to this string
 * @update  gess 01/04/99
 * @param   aChar: char to be assignd to this
 * @return  this
 */
void nsCString::AssignWithConversion(PRUnichar aChar) {
  nsStr::Truncate(*this,0);
  AppendWithConversion(aChar);
}

#ifdef NEW_STRING_APIS
void nsCString::do_AppendFromReadable( const nsAReadableCString& aReadable )
  {
    if ( SameImplementation( NS_STATIC_CAST(const nsAReadableCString&, *this), aReadable) )
      StrAppend(*this, NS_STATIC_CAST(const nsCString&, aReadable), 0, aReadable.Length());
    else
      nsAWritableCString::do_AppendFromReadable(aReadable);
  }
#endif

#ifndef NEW_STRING_APIS
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
    StrAppend(*this,aString,0,aCount);
  return *this;
}

#if 0
/**
 * append given string to this string; 
 * @update  gess 01/04/99
 * @param   aString : string to be appended to this
 * @return  this
 */
nsCString& nsCString::Append(const nsStr& aString,PRInt32 aCount) {

  if(aCount<0)
    aCount=aString.mLength;
  else aCount=MinInt(aCount,aString.mLength);

  if(0<aCount)
    StrAppend(*this,aString,0,aCount);
  return *this;
}
#endif

/**
 * append given c-string to this string
 * @update  gess 01/04/99
 * @param   aString : string to be appended to this
 * @param   aCount: #of chars to be copied; -1 means to copy the whole thing
 * NOTE:    IFF you pass -1 as aCount, then your buffer must be null terminated.
 *
 * @return  this
 */
nsCString& nsCString::Append(const char* aCString,PRInt32 aCount) {
  if(aCString){
    nsStr temp;
    nsStr::Initialize(temp,eOneByte);
    temp.mStr=(char*)aCString;

    if(0<aCount) {
      temp.mLength=aCount;
    }
    else aCount=temp.mLength=nsCRT::strlen(aCString);

    if(0<aCount)
      StrAppend(*this,temp,0,aCount);
  }
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
  nsStr::Initialize(temp,eOneByte);
  temp.mStr=buf;
  temp.mLength=1;
  StrAppend(*this,temp,0,1);
  return *this;
}
#endif

/**
 * append given char to this string
 * @update  gess 01/04/99
 * @param   aChar: char to be appended to this
 * @return  this
 */
void nsCString::AppendWithConversion(PRUnichar aChar) {
  PRUnichar buf[2]={0,0};
  buf[0]=aChar;

  nsStr temp;
  nsStr::Initialize(temp,eTwoByte);
  temp.mUStr=buf;
  temp.mLength=1;
  StrAppend(*this,temp,0,1);
}

void nsCString::AppendWithConversion( const PRUnichar* aBuffer, PRInt32 aLength )
  {
    nsStr temp;
    nsStr::Initialize(temp, eTwoByte);
    temp.mUStr = NS_CONST_CAST(PRUnichar*, aBuffer);

    if ( aLength < 0 )
      aLength = nsCharTraits<PRUnichar>::length(aBuffer);

    if ( aLength > 0 )
      StrAppend(*this, temp, 0, aLength);
  }

/**
 * Append the given integer to this string 
 * @update  gess 01/04/99
 * @param   aInteger:
 * @param   aRadix:
 * @return
 */
void nsCString::AppendInt(PRInt32 anInteger,PRInt32 aRadix) {

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
  Append(buf);
}


/**
 * Append the given float to this string 
 * @update  gess 01/04/99
 * @param   aFloat:
 * @return
 */
void nsCString::AppendFloat( double aFloat ){
  char buf[40];
  // *** XX UNCOMMENT THIS LINE
  //PR_snprintf(buf, sizeof(buf), "%g", aFloat);
  sprintf(buf,"%g",aFloat);
  Append(buf);
}

/**
 * append given string to this string; 
 * @update  gess 01/04/99
 * @param   aString : string to be appended to this
 * @return  this
 */
void nsCString::AppendWithConversion(const nsString& aString,PRInt32 aCount) {

  if(aCount<0)
    aCount=aString.mLength;
  else aCount=MinInt(aCount,aString.mLength);

  if(0<aCount)
    StrAppend(*this,aString,0,aCount);
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
  StrAssign(aDest,*this,0,aCount);
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
  StrAssign(aDest,*this,anOffset,aCount);
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


/**
 * Insert a single unichar into this string at a specified offset.
 *
 * @update  gess4/22/98
 * @param   aChar unichar to be inserted into this string
 * @param   anOffset is insert pos in str 
 * @return  this
 */
void nsCString::InsertWithConversion(PRUnichar aChar,PRUint32 anOffset){
  PRUnichar theBuffer[2]={0,0};
  theBuffer[0]=aChar;
  nsStr temp;
  nsStr::Initialize(temp,eTwoByte);
  temp.mUStr=theBuffer;
  temp.mLength=1;
  StrInsert(*this,anOffset,temp,0,1);
}

#ifdef NEW_STRING_APIS
void nsCString::do_InsertFromReadable( const nsAReadableCString& aReadable, PRUint32 atPosition )
  {
    if ( SameImplementation( NS_STATIC_CAST(const nsAReadableCString&, *this), aReadable) )
      StrInsert(*this, atPosition, NS_STATIC_CAST(const nsCString&, aReadable), 0, aReadable.Length());
    else
      nsAWritableCString::do_InsertFromReadable(aReadable, atPosition);
  }
#endif

#ifndef NEW_STRING_APIS
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
void nsCString::Insert(const nsCString& aString,PRUint32 anOffset,PRInt32 aCount) {

  StrInsert(*this,anOffset,aString,0,aCount);
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
void nsCString::Insert(const char* aCString,PRUint32 anOffset,PRInt32 aCount){
  if(aCString){
    nsStr temp;
    nsStr::Initialize(temp,eOneByte);
    temp.mStr=(char*)aCString;

    if(0<aCount) {
      temp.mLength=aCount;

      // If this assertion fires, the caller is probably lying about the length of
      //   the passed-in string.  File a bug on the caller.
#ifdef NS_DEBUG
      PRInt32 len=nsStr::FindChar(temp,0,PR_FALSE,0,temp.mLength);
      if(kNotFound<len) {
        NS_WARNING(kPossibleNull);
      }
#endif

    }
    else aCount=temp.mLength=nsCRT::strlen(aCString);

    if(temp.mLength && (0<aCount)){
      StrInsert(*this,anOffset,temp,0,aCount);
    }
  }
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
void nsCString::Insert(char aChar,PRUint32 anOffset){
  char theBuffer[2]={0,0};
  theBuffer[0]=aChar;
  nsStr temp;
  nsStr::Initialize(temp,eOneByte);
  temp.mStr=theBuffer;
  temp.mLength=1;
  StrInsert(*this,anOffset,temp,0,1);
}
#endif


/*
 *  This method is used to cut characters in this string
 *  starting at anOffset, continuing for aCount chars.
 *  
 *  @update gess 01/04/99
 *  @param  anOffset -- start pos for cut operation
 *  @param  aCount -- number of chars to be cut
 *  @return *this
 */
#ifndef NEW_STRING_APIS
void nsCString::Cut(PRUint32 anOffset, PRInt32 aCount) {
  if(0<aCount) {
    nsStr::Delete(*this,anOffset,aCount);
  }
}
#endif

/**********************************************************************
  Searching methods...                
 *********************************************************************/
 

/**
 *  Search for given cstr within this string
 *  
 *  @update  gess 3/25/98
 *  @param   aCString - substr to be found
 *  @param   aIgnoreCase tells us whether or not to do caseless compare
 *  @param   anOffset tells us where in this string to start searching
 *  @param   aCount tells us how many iterations to make starting at the given offset
 *  @return  offset in string, or -1 (kNotFound)
 */
PRInt32 nsCString::Find(const char* aCString,PRBool aIgnoreCase,PRInt32 anOffset,PRInt32 aCount) const{
  PRInt32 result=kNotFound;
  if(aCString) {
    nsStr temp;
    nsStr::Initialize(temp,eOneByte);
    temp.mLength = nsCRT::strlen(aCString);
    temp.mStr=(char*)aCString;
    result=nsStr::FindSubstr(*this,temp,aIgnoreCase,anOffset,aCount);
  }
  return result;
}

/** 
 *  Search for given buffer within this string
 *  
 *  @update  gess 3/25/98
 *  @param   aString - substr to be found
 *  @param   aIgnoreCase tells us whether or not to do caseless compare
 *  @param   anOffset tells us where in this string to start searching
 *  @param   aCount tells us how many iterations to make starting at the given offset
 *  @return  offset in string, or -1 (kNotFound)
 */
PRInt32 nsCString::Find(const PRUnichar* aString,PRBool aIgnoreCase,PRInt32 anOffset,PRInt32 aCount) const{
  NS_ASSERTION(0!=aString,kNullPointerError);

  PRInt32 result=kNotFound;
  if(aString) {
    nsStr temp;
    nsStr::Initialize(temp,eTwoByte);
    temp.mLength = nsCRT::strlen(aString);
    temp.mUStr=(PRUnichar*)aString;
    result=nsStr::FindSubstr(*this,temp,aIgnoreCase,anOffset,aCount);
  }
  return result;
}

/**
 *  Search for given string within this string
 *  
 *  @update  gess 3/25/98
 *  @param   aString - substr to be found
 *  @param   aIgnoreCase tells us whether or not to do caseless compare
 *  @param   anOffset tells us where in this string to start searching
 *  @param   aCount tells us how many iterations to make starting at the given offset
 *  @return  offset in string, or -1 (kNotFound)
 */
PRInt32 nsCString::Find(const nsStr& aString,PRBool aIgnoreCase,PRInt32 anOffset,PRInt32 aCount) const{
  PRInt32 result=nsStr::FindSubstr(*this,aString,aIgnoreCase,anOffset,aCount);
  return result;
}


/**
 *  This searches this string for a given character 
 *  
 *  @update  gess 2/04/00
 *  @param   char is the character you're trying to find.
 *  @param   aIgnorecase indicates case sensitivity of search
 *  @param   anOffset tells us where to start the search; -1 means start at 0.
 *  @param   aCount tell us how many chars to search from offset; -1 means use full length.
 *  @return  index in aDest where member of aSet occurs, or -1 if not found
 */
PRInt32 nsCString::FindChar(PRUnichar aChar,PRBool aIgnoreCase,PRInt32 anOffset,PRInt32 aCount) const{
  PRInt32 result=nsStr::FindChar(*this,aChar,aIgnoreCase,anOffset,aCount);
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
 *  Reverse search for given string within this string
 *  
 *  @update  gess 3/25/98
 *  @param   aString - substr to be found
 *  @param   aIgnoreCase tells us whether or not to do caseless compare
 *  @param   anOffset tells us where in this string to start searching
 *  @param   aCount tells us how many iterations to make starting at the given offset
 *  @return  offset in string, or -1 (kNotFound)
 */
PRInt32 nsCString::RFind(const nsStr& aString,PRBool aIgnoreCase,PRInt32 anOffset,PRInt32 aCount) const{
  PRInt32 result=nsStr::RFindSubstr(*this,aString,aIgnoreCase,anOffset,aCount);
  return result;
}


/**
 *  Reverse search for given string within this string
 *  
 *  @update  gess 3/25/98
 *  @param   aString - substr to be found
 *  @param   aIgnoreCase tells us whether or not to do caseless compare
 *  @param   anOffset tells us where in this string to start searching
 *  @param   aCount tells us how many iterations to make starting at the given offset
 *  @return  offset in string, or -1 (kNotFound)
 */
PRInt32 nsCString::RFind(const char* aString,PRBool aIgnoreCase,PRInt32 anOffset,PRInt32 aCount) const{
  NS_ASSERTION(0!=aString,kNullPointerError);

  PRInt32 result=kNotFound;
  if(aString) {
    nsStr temp;
    nsStr::Initialize(temp,eOneByte);
    temp.mLength=nsCRT::strlen(aString);
    temp.mStr=(char*)aString;
    result=nsStr::RFindSubstr(*this,temp,aIgnoreCase,anOffset,aCount);
  }
  return result;
}

/**
 *  This reverse searches this string for a given character 
 *  
 *  @update  gess 2/04/00
 *  @param   char is the character you're trying to find.
 *  @param   aIgnorecase indicates case sensitivity of search
 *  @param   anOffset tells us where to start the search; -1 means start at 0.
 *  @param   aCount tell us how many chars to search from offset; -1 means use full length.
 *  @return  index in aDest where member of aSet occurs, or -1 if not found
 */
PRInt32 nsCString::RFindChar(PRUnichar aChar,PRBool aIgnoreCase,PRInt32 anOffset,PRInt32 aCount) const{
  PRInt32 result=nsStr::RFindChar(*this,aChar,aIgnoreCase,anOffset,aCount);
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
 * Compares given unistring to this string. 
 * @update  gess 01/04/99
 * @param   aString pts to a uni-string
 * @param   aIgnoreCase tells us how to treat case
 * @param   aCount tells us how many chars to test; -1 implies full length
 * @return  -1,0,1
 */
PRInt32 nsCString::CompareWithConversion(const PRUnichar* aString,PRBool aIgnoreCase,PRInt32 aCount) const {
  NS_ASSERTION(0!=aString,kNullPointerError);

  if(aString) {
    nsStr temp;
    nsStr::Initialize(temp,eTwoByte);
    temp.mLength=nsCRT::strlen(aString);
    temp.mUStr=(PRUnichar*)aString;
    return nsStr::StrCompare(*this,temp,aCount,aIgnoreCase);
  }
  return 0;
}

/**
 * Compares given cstring to this string. 
 * @update  gess 01/04/99
 * @param   aCString points to a cstring
 * @param   aIgnoreCase tells us how to treat case
 * @param   aCount tells us how many chars to test; -1 implies full length
 * @return  -1,0,1
 */
PRInt32 nsCString::CompareWithConversion(const char *aCString,PRBool aIgnoreCase,PRInt32 aCount) const {
  NS_ASSERTION(0!=aCString,kNullPointerError);

  if(aCString) {
    nsStr temp;
    nsStr::Initialize(temp,eOneByte);
    temp.mLength=nsCRT::strlen(aCString);
    temp.mStr=(char*)aCString;
    return nsStr::StrCompare(*this,temp,aCount,aIgnoreCase);
  }
  return 0;
}

#ifndef NEW_STRING_APIS
/**
 * Compare given nsStr with this cstring.
 * 
 * @param   aString is an nsStr instance to be compared
 * @param   aIgnoreCase tells us how to treat case
 * @param   aCount tells us how many chars to test; -1 implies full length
 * @return  -1,0,1
 */
PRInt32 nsCString::Compare(const nsStr& aString,PRBool aIgnoreCase,PRInt32 aCount) const {
  return nsStr::StrCompare(*this,aString,aCount,aIgnoreCase);
}
#endif

#ifndef NEW_STRING_APIS
/**
 *  Here come a whole bunch of operator functions that are self-explanatory...
 */
PRBool nsCString::operator==(const nsStr& S) const {return Equals(S);}      
PRBool nsCString::operator==(const char* s) const {return Equals(s);}
//PRBool nsCString::operator==(const PRUnichar* s) const {return Equals(s);}

PRBool nsCString::operator!=(const nsStr& S) const {return PRBool(Compare(S)!=0);}
PRBool nsCString::operator!=(const char* s) const {return PRBool(Compare(nsCAutoString(s))!=0);}
//PRBool nsCString::operator!=(const PRUnichar* s) const {return PRBool(Compare(s)!=0);}

PRBool nsCString::operator<(const nsStr& S) const {return PRBool(Compare(S)<0);}
PRBool nsCString::operator<(const char* s) const {return PRBool(Compare(nsCAutoString(s))<0);}
//PRBool nsCString::operator<(const PRUnichar* s) const {return PRBool(Compare(s)<0);}

PRBool nsCString::operator>(const nsStr& S) const {return PRBool(Compare(S)>0);}
PRBool nsCString::operator>(const char* s) const {return PRBool(Compare(nsCAutoString(s))>0);}
//PRBool nsCString::operator>(const PRUnichar* s) const {return PRBool(Compare(s)>0);}

PRBool nsCString::operator<=(const nsStr& S) const {return PRBool(Compare(S)<=0);}
PRBool nsCString::operator<=(const char* s) const {return PRBool(Compare(nsCAutoString(s))<=0);}
//PRBool nsCString::operator<=(const PRUnichar* s) const {return PRBool(Compare(s)<=0);}

PRBool nsCString::operator>=(const nsStr& S) const {return PRBool(Compare(S)>=0);}
PRBool nsCString::operator>=(const char* s) const {return PRBool(Compare(nsCAutoString(s))>=0);}
//PRBool nsCString::operator>=(const PRUnichar* s) const {return PRBool(Compare(s)>=0);}
#endif

#ifndef NEW_STRING_APIS
PRBool nsCString::EqualsIgnoreCase(const nsStr& aString) const {
  return Equals(aString,PR_TRUE);
}
#endif

PRBool nsCString::EqualsIgnoreCase(const char* aString,PRInt32 aLength) const {
  return EqualsWithConversion(aString,PR_TRUE,aLength);
}

PRBool nsCString::EqualsIgnoreCase(const PRUnichar* aString,PRInt32 aLength) const {
  return EqualsWithConversion(aString,PR_TRUE,aLength);
}


#ifndef NEW_STRING_APIS
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
  PRInt32 theAnswer=nsStr::StrCompare(*this,aString,aCount,aIgnoreCase);
  PRBool  result=PRBool(0==theAnswer);
  return result;
}
#endif

/**
 * Compare this to given string; note that we compare full strings here.
 *
 * @param  aString is the CString to be compared 
 * @param  aCount tells us how many chars you want to compare starting with start of string
 * @param  aIgnorecase tells us whether to be case sensitive
 * @param  aCount tells us how many chars to test; -1 implies full length
 * @return TRUE if equal
 */
PRBool nsCString::EqualsWithConversion(const char* aCString,PRBool aIgnoreCase,PRInt32 aCount) const{
  PRInt32 theAnswer=CompareWithConversion(aCString,aIgnoreCase,aCount);
  PRBool  result=PRBool(0==theAnswer);  
  return result;
}

PRBool nsCString::EqualsWithConversion(const PRUnichar* aString,PRBool aIgnoreCase,PRInt32 aCount) const {
  PRInt32 theAnswer=CompareWithConversion(aString,aIgnoreCase,aCount);
  PRBool  result=PRBool(0==theAnswer);  
  return result;
}

 PRBool nsCString::EqualsWithConversion(const nsString& aString, PRBool aIgnoreCase,PRInt32 aCount) const {
  PRInt32 theAnswer=CompareWithConversion(aString.GetUnicode(),aIgnoreCase,aCount);
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
    nsCString* aString= (nsCString*)anObject;
    if(aString){
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

/**
 * Dumps the contents of the string to stdout
 * @update  gess 11/15/99
 */
void nsCString::DebugDump(void) const {
  
  if(mStr && (eOneByte==mCharSize)) {
    printf("\n%s",mStr);
  }
}
       
//----------------------------------------------------------------------

void
NS_ConvertUCS2toUTF8::Init( const PRUnichar* aString, PRUint32 aLength )
  {
    // Handle null string by just leaving us as a brand-new
    // uninitialized nsCAutoString.
    if (! aString)
      return;

    // Caculate how many bytes we need
    const PRUnichar* p;
    PRInt32 count, utf8len;
    for (p = aString, utf8len = 0, count = aLength; 0 != count && 0 != (*p); count--, p++)
      {
        if (! ((*p) & 0xFF80))
          utf8len += 1; // 0000 0000 - 0000 007F
        else if (! ((*p) & 0xF800))
          utf8len += 2; // 0000 0080 - 0000 07FF
        else 
          utf8len += 3; // 0000 0800 - 0000 FFFF
        // Note: Surrogate pair needs 4 bytes, but in this calcuation
        // we count it as 6 bytes. It will waste 2 bytes per surrogate pair
      }

    // Make sure our buffer's big enough, so we don't need to do
    // multiple allocations.
    if(PRUint32(utf8len+1) > sizeof(mBuffer))
      SetCapacity(utf8len+1);
    // |SetCapacity| normally doesn't guarantee the use we are putting it to here (see its interface comment in nsAWritableString.h),
    //  we can only use it since our local implementation, |nsCString::SetCapacity|, is known to do what we want

    char* out = mStr;
    PRUint32 ucs4=0;

    for (p = aString, count = aLength; 0 != count && 0 != (*p); count--, p++)
      {
        if (0 == ucs4)
          {
            if (! ((*p) & 0xFF80))
              {
                *out++ = (char)*p;
              } 
            else if (! ((*p) & 0xF800))
              {
                *out++ = 0xC0 | (char)((*p) >> 6);
                *out++ = 0x80 | (char)(0x003F & (*p));
              }
            else
              {
                if (0xD800 == (0xFC00 & (*p))) 
                  {
                    // D800- DBFF - High Surrogate 
                    // N = (H- D800) *400 + 10000 + ...
                    ucs4 = 0x10000 | ((0x03FF & (*p)) << 10);
                  }
                else if (0xDC00 == (0xFC00 & (*p)))
                  { 
                    // DC00- DFFF - Low Surrogate 
                    // error here. We should hit High Surrogate first
                    // Do not output any thing in this case
                  }
                else
                  {
                    *out++ = 0xE0 | (char)((*p) >> 12);
                    *out++ = 0x80 | (char)(0x003F & (*p >> 6));
                    *out++ = 0x80 | (char)(0x003F & (*p) );
                  }
              }
          }
        else
          {
            if (0xDC00 == (0xFC00 & (*p)))
              { 
                // DC00- DFFF - Low Surrogate 
                // N += ( L - DC00 )  
                ucs4 |= (0x03FF & (*p));

                // 0001 0000-001F FFFF
                *out++ = 0xF0 | (char)(ucs4 >> 18);
                *out++ = 0x80 | (char)(0x003F & (ucs4 >> 12));
                *out++ = 0x80 | (char)(0x003F & (ucs4 >> 6));
                *out++ = 0x80 | (char)(0x003F & ucs4) ;
              }
            else
              {
                // Got a High Surrogate but no low surrogate
                // output nothing.
              }
            ucs4 = 0;
          }
      }

    *out = '\0'; // null terminate
    mLength = utf8len;
  }


/***********************************************************************
  IMPLEMENTATION NOTES: AUTOSTRING...
 ***********************************************************************/


/**
 * Default constructor
 *
 */
nsCAutoString::nsCAutoString() : nsCString(){
  Initialize(*this,mBuffer,sizeof(mBuffer)-1,0,eOneByte,PR_FALSE);
  AddNullTerminator(*this);

}

nsCAutoString::nsCAutoString( const nsCString& aString ) : nsCString(){
  Initialize(*this,mBuffer,sizeof(mBuffer)-1,0,eOneByte,PR_FALSE);
  AddNullTerminator(*this);
  Append(aString);
}


nsCAutoString::nsCAutoString(const char* aCString) : nsCString() {
  Initialize(*this,mBuffer,sizeof(mBuffer)-1,0,eOneByte,PR_FALSE);
  AddNullTerminator(*this);
  Append(aCString);
}

/**
 * Copy construct from ascii c-string
 * @param   aCString is a ptr to a 1-byte cstr
 */
nsCAutoString::nsCAutoString(const char* aCString,PRInt32 aLength) : nsCString() {
  Initialize(*this,mBuffer,sizeof(mBuffer)-1,0,eOneByte,PR_FALSE);
  AddNullTerminator(*this);
  Append(aCString,aLength);
}

/**
 * Copy construct using an external buffer descriptor
 * @param   aBuffer -- descibes external buffer
 */
nsCAutoString::nsCAutoString(const CBufDescriptor& aBuffer) : nsCString() {
  if(!aBuffer.mBuffer) {
    Initialize(*this,mBuffer,sizeof(mBuffer)-1,0,eOneByte,PR_FALSE);
  }
  else {
    Initialize(*this,aBuffer.mBuffer,aBuffer.mCapacity,aBuffer.mLength,aBuffer.mCharSize,!aBuffer.mStackBased);
  }
  if(!aBuffer.mIsConst)
    AddNullTerminator(*this); //this isn't really needed, but it guarantees that folks don't pass string constants.
}

#if 0
/**
 * Copy construct from uni-string
 * @param   aString is a ptr to a unistr
 */
nsCAutoString::nsCAutoString(const PRUnichar* aString,PRInt32 aLength) : nsCString() {
  Initialize(*this,mBuffer,sizeof(mBuffer)-1,0,eOneByte,PR_FALSE);
  AddNullTerminator(*this);
  Append(aString,aLength);
}

/**
 * construct from an nsStr
 * @param   
 */
nsCAutoString::nsCAutoString(const nsStr& aString) : nsCString() {
  Initialize(*this,mBuffer,sizeof(mBuffer)-1,0,eOneByte,PR_FALSE);
  AddNullTerminator(*this);
  Append(aString);
}



/**
 * construct from a char
 * @param   
 */
nsCAutoString::nsCAutoString(PRUnichar aChar) : nsCString(){
  Initialize(*this,mBuffer,sizeof(mBuffer)-1,0,eOneByte,PR_FALSE);
  AddNullTerminator(*this);
  Append(aChar);
}
#endif


/**
 * construct from a subsumeable string
 * @update  gess 1/4/99
 * @param   reference to a subsumeString
 */
#if defined(AIX) || defined(XP_OS2_VACPP)
nsCAutoString::nsCAutoString(const nsSubsumeCStr& aSubsumeStr) :nsCString() {
  nsSubsumeCStr temp(aSubsumeStr);  // a temp is needed for the AIX and VAC++ compilers
  CSubsume(*this,temp);
#else
nsCAutoString::nsCAutoString( nsSubsumeCStr& aSubsumeStr) :nsCString() {
  CSubsume(*this,aSubsumeStr);
#endif // AIX || XP_OS2_VACPP
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



