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

#include <ctype.h>
#include <string.h> 
#include <stdlib.h>
#include "nsString.h"
#include "nsReadableUtils.h"
#include "nsDebug.h"
#include "nsDeque.h"

#ifndef RICKG_TESTBED
#include "prdtoa.h"
#include "nsISizeOfHandler.h"
#endif 


static const char* kPossibleNull = "Error: possible unintended null in string";
static const char* kNullPointerError = "Error: unexpected null ptr";
static const char* kWhitespace="\b\t\r\n ";

const nsBufferHandle<PRUnichar>*
nsString::GetFlatBufferHandle() const
  {
    return NS_REINTERPRET_CAST(const nsBufferHandle<PRUnichar>*, 1);
  }

/**
 * Default constructor. 
 */
nsString::nsString() {
  Initialize(*this,eTwoByte);
}

nsString::nsString(const PRUnichar* aString) {
  Initialize(*this,eTwoByte);
  Assign(aString);
}

/**
 * This constructor accepts a unicode string
 * @update  gess 1/4/99
 * @param   aString is a ptr to a unichar string
 * @param   aLength tells us how many chars to copy from given aString
 */
nsString::nsString(const PRUnichar* aString,PRInt32 aCount) {  
  Initialize(*this,eTwoByte);
  Assign(aString,aCount);
}

/**
 * This is our copy constructor 
 * @update  gess 1/4/99
 * @param   reference to another nsString
 */
nsString::nsString(const nsString& aString) {
  Initialize(*this,eTwoByte);
  StrAssign(*this,aString,0,aString.mLength);
}

/**
 * Destructor
 * Make sure we call nsStr::Destroy.
 */
nsString::~nsString() {
  nsStr::Destroy(*this);
}

const PRUnichar* nsString::GetReadableFragment( nsReadableFragment<PRUnichar>& aFragment, nsFragmentRequest aRequest, PRUint32 aOffset ) const {
  switch ( aRequest ) {
    case kFirstFragment:
    case kLastFragment:
    case kFragmentAt:
      aFragment.mEnd = (aFragment.mStart = mUStr) + mLength;
      return aFragment.mStart + aOffset;
    
    case kPrevFragment:
    case kNextFragment:
    default:
      return 0;
  }
}

PRUnichar* nsString::GetWritableFragment( nsWritableFragment<PRUnichar>& aFragment, nsFragmentRequest aRequest, PRUint32 aOffset ) {
  switch ( aRequest ) {
    case kFirstFragment:
    case kLastFragment:
    case kFragmentAt:
      aFragment.mEnd = (aFragment.mStart = mUStr) + mLength;
      return aFragment.mStart + aOffset;
    
    case kPrevFragment:
    case kNextFragment:
    default:
      return 0;
  }
}


void
nsString::do_AppendFromElement( PRUnichar inChar )
  {
    PRUnichar buf[2] = { 0, 0 };
    buf[0] = inChar;
    
    nsStr temp;
    nsStr::Initialize(temp, eTwoByte);
    temp.mUStr = buf;
    temp.mLength = 1;
    StrAppend(*this, temp, 0, 1);
  }


nsString::nsString( const nsAString& aReadable ) {
  Initialize(*this,eTwoByte);
  Assign(aReadable);
}

#ifdef DEBUG
void nsString::SizeOf(nsISizeOfHandler* aHandler, PRUint32* aResult) const {
  if (aResult) {
    *aResult = sizeof(*this) + mCapacity * mCharSize;
  }
}
#endif

/**
 * This method truncates this string to given length.
 *
 * @update  gess 01/04/99
 * @param   anIndex -- new length of string
 * @return  nada
 */
void nsString::SetLength(PRUint32 anIndex) {
  if ( anIndex > mCapacity )
    SetCapacity(anIndex);
    // |SetCapacity| normally doesn't guarantee the use we are putting it to here (see its interface comment in nsAWritableString.h),
    //  we can only use it since our local implementation, |nsString::SetCapacity|, is known to do what we want
  nsStr::StrTruncate(*this,anIndex);
}


/**
 * Call this method if you want to force the string to a certain capacity;
 * |SetCapacity(0)| discards associated storage.
 *
 * @param   aNewCapacity -- desired minimum capacity
 */
void
nsString::SetCapacity( PRUint32 aNewCapacity )
  {
    if ( aNewCapacity )
      {
        if( aNewCapacity > mCapacity )
          GrowCapacity(*this, aNewCapacity);
        AddNullTerminator(*this);
      }
    else
      {
        nsStr::Destroy(*this);
        nsStr::Initialize(*this, eTwoByte);
      }
  }

/**********************************************************************
  Accessor methods...
 *********************************************************************/


/**
 * This method returns the internal unicode buffer. 
 * Now that we've factored the string class, this should never
 * be able to return a 1 byte string.
 *
 * @update  gess1/4/99
 * @return  ptr to internal (2-byte) buffer;
 */
const PRUnichar* nsString::get() const {
  const PRUnichar* result=(eOneByte==mCharSize) ? 0 : mUStr;
  return result;
}

/**
 * set a char inside this string at given index
 * @param aChar is the char you want to write into this string
 * @param anIndex is the ofs where you want to write the given char
 * @return TRUE if successful
 */
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


/**********************************************************************
  Lexomorphic transforms...
 *********************************************************************/

/**
 * Converts all chars in internal string to lower
 * @update  gess 01/04/99
 */
void nsString::ToLowerCase() {
  nsStr::ChangeCase(*this,PR_FALSE);
}

/**
 * Converts all chars in internal string to upper
 * @update  gess 01/04/99
 */
void nsString::ToUpperCase() {
  nsStr::ChangeCase(*this,PR_TRUE);
}

/**
 * Converts chars in this to uppercase, and
 * stores them in aString
 * @update  gess 01/04/99
 * @param   aOut is a string to contain result
 */
void nsString::ToLowerCase(nsString& aString) const {
  aString=*this;
  nsStr::ChangeCase(aString,PR_FALSE);
}

/**
 * Converts chars in this to uppercase, and
 * stores them in a given output string
 * @update  gess 01/04/99
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
 *  @update rickg 03.23.2000
 *  @param  aChar -- char to be stripped
 *  @param  anOffset -- where in this string to start stripping chars
 *  @return *this 
 */
void
nsString::StripChar(PRUnichar aChar,PRInt32 anOffset){
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
nsString::StripChars(const char* aSet){
  nsStr::StripChars(*this,aSet);
}


/**
 *  This method strips whitespace throughout the string
 *  
 *  @update  gess 01/04/99
 *  @return  this
 */
void
nsString::StripWhitespace() {
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
nsString::ReplaceChar(PRUnichar aSourceChar, PRUnichar aDestChar) {
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
}

/**
 *  This method is used to replace all occurances of the
 *  given source char with the given dest char
 *  
 *  @param  
 *  @return *this 
 */
void
nsString::ReplaceChar(const char* aSet, PRUnichar aNewChar){
  if(aSet){
    PRInt32 theIndex=FindCharInSet(aSet,0);
    while(kNotFound<theIndex) {
      if(eTwoByte==mCharSize) 
        mUStr[theIndex]=aNewChar;
      else mStr[theIndex]=(char)aNewChar;
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
nsString::ReplaceSubstring(const PRUnichar* aTarget,const PRUnichar* aNewValue){
  if(aTarget && aNewValue) {

    PRInt32 len=nsCRT::strlen(aTarget);
    if(0<len) {
      CBufDescriptor theDesc1(aTarget,PR_TRUE, len+1,len);
      nsAutoString theTarget(theDesc1);

      len=nsCRT::strlen(aNewValue);
      if(0<len) {
        CBufDescriptor theDesc2(aNewValue,PR_TRUE, len+1,len);
        nsAutoString theNewValue(theDesc2);

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
nsString::ReplaceSubstring(const nsString& aTarget,const nsString& aNewValue){


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
 *  This method trims characters found in aTrimSet from
 *  either end of the underlying string.
 *  
 *  @update  gess 3/31/98
 *  @param   aTrimSet -- contains chars to be trimmed from
 *           both ends
 *  @return  this
 */
void
nsString::Trim(const char* aTrimSet, PRBool aEliminateLeading,PRBool aEliminateTrailing,PRBool aIgnoreQuotes){

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
      Insert(theFirstChar,0);
      Append(theLastChar);
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
nsString::CompressSet(const char* aSet, PRUnichar aChar,PRBool aEliminateLeading,PRBool aEliminateTrailing){
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
nsString::CompressWhitespace( PRBool aEliminateLeading,PRBool aEliminateTrailing){
  CompressSet(kWhitespace,' ',aEliminateLeading,aEliminateTrailing);
}

/**********************************************************************
  string conversion methods...
 *********************************************************************/

/**
 * Copies contents of this string into he given buffer
 * Note that if you provide me a buffer that is smaller than the length of
 * this string, only the number of bytes that will fit are copied. 
 *
 * @update  gess 01/04/99
 * @param   aBuf
 * @param   aBufLength -- size of your external buffer (including null)
 * @param   anOffset -- THIS IS NOT USED AT THIS TIME!
 * @return
 */
char* nsString::ToCString(char* aBuf, PRUint32 aBufLength,PRUint32 anOffset) const{
  if(aBuf) {

    // NS_ASSERTION(mLength<=aBufLength,"buffer smaller than string");

    CBufDescriptor theDescr(aBuf,PR_TRUE,aBufLength,0);
    nsCAutoString temp(theDescr);
    nsStr::StrAssign(temp, *this, anOffset, PR_MIN(mLength, aBufLength-1));
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
 * Perform decimal numeric string to int conversion.
 * NOTE: In this version, we use the radix you give, even if it's wrong.
 * @update  gess 02/14/00
 * @param   aErrorCode will contain error if one occurs
 * @param   aRadix tells us what base to expect the given string in. kAutoDetect tells us to determine the radix.
 * @return  int rep of string value
 */
PRInt32 nsString::ToInteger(PRInt32* anErrorCode,PRUint32 aRadix) const {
  PRUnichar*  cp=mUStr;
  PRInt32     theRadix=10; // base 10 unless base 16 detected, or overriden (aRadix != kAutoDetect)
  PRInt32     result=0;
  PRBool      negate=PR_FALSE;
  PRUnichar   theChar=0;

    //initial value, override if we find an integer
  *anErrorCode=NS_ERROR_ILLEGAL_VALUE;
  
  if(cp) {
  
    //begin by skipping over leading chars that shouldn't be part of the number...
    
    PRUnichar*  endcp=cp+mLength;
    PRBool      done=PR_FALSE;
    
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
      PRUnichar* first=--cp;  //in case we have to back up.

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



/**
 * assign given char* to this string
 * @update  gess 01/04/99
 * @param   aCString: buffer to be assigned to this 
 * @param   aCount -- length of given buffer or -1 if you want me to compute length.
 * NOTE:    IFF you pass -1 as aCount, then your buffer must be null terminated.
 *
 * @return  this
 */
void nsString::AssignWithConversion(const char* aCString,PRInt32 aCount) {
  nsStr::StrTruncate(*this,0);
  if(aCString){
    AppendWithConversion(aCString,aCount);
  }
}

void nsString::AssignWithConversion(const char* aCString) {
  nsStr::StrTruncate(*this,0);
  if(aCString){
    AppendWithConversion(aCString);
  }
}


/**
 * assign given char to this string
 * @update  gess 01/04/99
 * @param   aChar: char to be assignd to this
 * @return  this
 */
void nsString::AssignWithConversion(char aChar) {
  nsStr::StrTruncate(*this,0);
  AppendWithConversion(aChar);
}


/**
 * append given c-string to this string
 * @update  gess 01/04/99
 * @param   aString : string to be appended to this
 * @param   aCount -- length of given buffer or -1 if you want me to compute length.
 * NOTE:    IFF you pass -1 as aCount, then your buffer must be null terminated.
 *
 * @return  this
 */
void nsString::AppendWithConversion(const char* aCString,PRInt32 aCount) {
  if(aCString && aCount){  //if astring is null or count==0 there's nothing to do
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
      
    if(0<aCount)
      StrAppend(*this,temp,0,aCount);
  }
}

/**
 * append given char to this string
 * @update  gess 01/04/99
 * @param   aChar: char to be appended 
 * @return  this
 */
void nsString::AppendWithConversion(char aChar) {
  char buf[2]={0,0};
  buf[0]=aChar;

  nsStr temp;
  nsStr::Initialize(temp,eOneByte);
  temp.mStr=buf;
  temp.mLength=1;
  StrAppend(*this,temp,0,1);
}

/**
 * Append the given integer to this string 
 * @update  gess 01/04/99
 * @param   aInteger:
 * @param   aRadix:
 * @return
 */
void nsString::AppendInt(PRInt32 anInteger,PRInt32 aRadix) {

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
  AppendWithConversion(buf);
}


/**
 * Append the given float to this string 
 * @update  gess 01/04/99
 * @param   aFloat:
 * @return
 */
void nsString::AppendFloat(double aFloat){
  char buf[40];
  // *** XX UNCOMMENT THIS LINE
  //PR_snprintf(buf, sizeof(buf), "%g", aFloat);
  sprintf(buf,"%g",aFloat);
  AppendWithConversion(buf);
}



/**
 * Insert a char* into this string at a specified offset.
 *
 * @update  gess4/22/98
 * @param   char* aCString to be inserted into this string
 * @param   anOffset is insert pos in str 
 * @param   aCount -- length of given buffer or -1 if you want me to compute length.
 * NOTE:    IFF you pass -1 as aCount, then your buffer must be null terminated.
 *
 * @return  this
 */
void nsString::InsertWithConversion(const char* aCString,PRUint32 anOffset,PRInt32 aCount){
  if(aCString && aCount){
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

    if(0<aCount){
      StrInsert(*this,anOffset,temp,0,aCount);
    }
  }
}



/**********************************************************************
  Searching methods...                
 *********************************************************************/
 
/**
 *  search for given string within this string
 *  
 *  @update  gess 3/25/98
 *  @param   aString - substr to be found
 *  @param   aIgnoreCase tells us whether or not to do caseless compare
 *  @param   anOffset tells us where in this string to start searching
 *  @param   aCount tells us how many iterations to make starting at the given offset
 *  @return  offset in string, or -1 (kNotFound)
 */
PRInt32 nsString::Find(const char* aCString,PRBool aIgnoreCase,PRInt32 anOffset,PRInt32 aCount) const{
  NS_ASSERTION(0!=aCString,kNullPointerError);

  PRInt32 result=kNotFound;
  if(aCString) {
    nsStr temp;
    nsStr::Initialize(temp,eOneByte);
    temp.mLength=nsCRT::strlen(aCString);
    temp.mStr=(char*)aCString;
    result=nsStr::FindSubstr(*this,temp,aIgnoreCase,anOffset,aCount);
  }
  return result;
}

/**
 *  search for given string within this string
 *  
 *  @update  gess 3/25/98
 *  @param   aString - substr to be found
 *  @param   aIgnoreCase tells us whether or not to do caseless compare
 *  @param   anOffset tells us where in this string to start searching
 *  @param   aCount tells us how many iterations to make starting at the given offset
 *  @return  offset in string, or -1 (kNotFound)
 */
PRInt32 nsString::Find(const PRUnichar* aString,PRBool aIgnoreCase,PRInt32 anOffset,PRInt32 aCount) const{
  NS_ASSERTION(0!=aString,kNullPointerError);

  PRInt32 result=kNotFound;
  if(aString) {
    nsStr temp;
    nsStr::Initialize(temp,eTwoByte);
    temp.mLength=nsCRT::strlen(aString);
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
PRInt32 nsString::Find(const nsStr& aString,PRBool aIgnoreCase,PRInt32 anOffset,PRInt32 aCount) const{
  PRInt32 result=nsStr::FindSubstr(*this,aString,aIgnoreCase,anOffset,aCount);
  return result;
}

/**
 *  search for given string within this string
 *  
 *  @update  gess 3/25/98
 *  @param   aString - substr to be found
 *  @param   aIgnoreCase tells us whether or not to do caseless compare
 *  @param   anOffset tells us where in this string to start searching
 *  @param   aCount tells us how many iterations to make starting at the given offset
 *  @return  offset in string, or -1 (kNotFound)
 */
PRInt32 nsString::Find(const nsString& aString,PRBool aIgnoreCase,PRInt32 anOffset,PRInt32 aCount) const{
  PRInt32 result=nsStr::FindSubstr(*this,aString,aIgnoreCase,anOffset,aCount);
  return result;
}

/**
 *  Search for a given char, starting at given offset
 *  
 *  @update  gess 3/25/98
 *  @param   aChar
 *  @return  offset of found char, or -1 (kNotFound)
 */
#if 0
PRInt32 nsString::Find(PRUnichar aChar,PRInt32 anOffset,PRBool aIgnoreCase) const{
  PRInt32 result=nsStr::FindChar(*this,aChar,aIgnoreCase,anOffset);
  return result;
}
#endif

/**
 *  Search for a given char, starting at given offset
 *  
 *  @update  gess 3/25/98
 *  @param   aChar is the unichar to be sought
 *  @param   aIgnoreCase tells us whether or not to do caseless compare
 *  @param   anOffset tells us where in this string to start searching
 *  @param   aCount tells us how many iterations to make starting at the given offset
 *  @return  offset in string, or -1 (kNotFound)
 */
PRInt32 nsString::FindChar(PRUnichar aChar,PRBool aIgnoreCase,PRInt32 anOffset,PRInt32 aCount) const{
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
 *  This method finds the offset of the first char in this string that is
 *  a member of the given charset, starting the search at anOffset
 *  
 *  @update  gess 3/25/98
 *  @param   aCStringSet
 *  @param   anOffset -- where in this string to start searching
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
 *  This method finds the offset of the first char in this string that is
 *  a member of the given charset, starting the search at anOffset
 *  
 *  @update  gess 3/25/98
 *  @param   aCStringSet
 *  @param   anOffset -- where in this string to start searching
 *  @return  
 */
PRInt32 nsString::FindCharInSet(const nsStr& aSet,PRInt32 anOffset) const{
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
PRInt32 nsString::RFind(const nsStr& aString,PRBool aIgnoreCase,PRInt32 anOffset,PRInt32 aCount) const{
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
PRInt32 nsString::RFind(const nsString& aString,PRBool aIgnoreCase,PRInt32 anOffset,PRInt32 aCount) const{
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
PRInt32 nsString::RFind(const char* aString,PRBool aIgnoreCase,PRInt32 anOffset,PRInt32 aCount) const{
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
 *  Reverse search for char
 *  
 *  @update  gess 3/25/98
 *  @param   achar
 *  @param   aIgnoreCase
 *  @param   anOffset - tells us where to begin the search
 *  @return  offset of substring or -1
 */
#if 0
PRInt32 nsString::RFind(PRUnichar aChar,PRInt32 anOffset,PRBool aIgnoreCase) const{
  PRInt32 result=nsStr::RFindChar(*this,aChar,aIgnoreCase,anOffset);
  return result;
}
#endif
 
/**
 *  Reverse search for a given char, starting at given offset
 *  
 *  @update  gess 3/25/98
 *  @param   aChar
 *  @param   aIgnoreCase
 *  @param   anOffset
 *  @return  offset of found char, or -1 (kNotFound)
 */
PRInt32 nsString::RFindChar(PRUnichar aChar,PRBool aIgnoreCase,PRInt32 anOffset,PRInt32 aCount) const{
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
 *  Reverse search for a first char in this string that is a
 *  member of the given char set
 *  
 *  @update  gess 3/25/98
 *  @param   aSet
 *  @param   aIgnoreCase
 *  @param   anOffset
 *  @return  offset of found char, or -1 (kNotFound)
 */
PRInt32 nsString::RFindCharInSet(const nsStr& aSet,PRInt32 anOffset) const{
  PRInt32 result=nsStr::RFindCharInSet(*this,aSet,PR_FALSE,anOffset);
  return result;
}

/**
 *  Reverse search for a first char in this string that is a
 *  member of the given char set
 *  
 *  @update  gess 3/25/98
 *  @param   aSet
 *  @param   aIgnoreCase
 *  @param   anOffset
 *  @return  offset of found char, or -1 (kNotFound)
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
 * @update  gess 01/04/99
 * @param   aCString pts to a cstring
 * @param   aIgnoreCase tells us how to treat case
 * @param   aCount tells us how many chars to test; -1 implies full length
 * @return  -1,0,1
 */
PRInt32 nsString::CompareWithConversion(const char *aCString,PRBool aIgnoreCase,PRInt32 aCount) const {
  NS_ASSERTION(0!=aCString,kNullPointerError);

  if(aCString) {
    nsStr temp;
    nsStr::Initialize(temp,eOneByte);

    temp.mLength= (0<aCount) ? aCount : nsCRT::strlen(aCString);

    temp.mStr=(char*)aCString;
    return nsStr::StrCompare(*this,temp,aCount,aIgnoreCase);
  }

  return 0;
}

/**
 * Compares given cstring to this string. 
 * @update  gess 01/04/99
 * @param   aCString pts to a cstring
 * @param   aIgnoreCase tells us how to treat case
 * @param   aCount tells us how many chars to test; -1 implies full length
 * @return  -1,0,1
 */
PRInt32 nsString::CompareWithConversion(const nsString& aString,PRBool aIgnoreCase,PRInt32 aCount) const {
  PRInt32 result=nsStr::StrCompare(*this,aString,aCount,aIgnoreCase);
  return result;
}

/**
 * Compares given unistring to this string. 
 * @update  gess 01/04/99
 * @param   aString pts to a uni-string
 * @param   aIgnoreCase tells us how to treat case
 * @param   aCount tells us how many chars to test; -1 implies full length
 * @return  -1,0,1
 */
PRInt32 nsString::CompareWithConversion(const PRUnichar* aString,PRBool aIgnoreCase,PRInt32 aCount) const {
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



PRBool nsString::EqualsIgnoreCase(const nsString& aString) const {
  return EqualsWithConversion(aString,PR_TRUE);
}

PRBool nsString::EqualsIgnoreCase(const char* aString,PRInt32 aLength) const {
  return EqualsWithConversion(aString,PR_TRUE,aLength);
}

/**
 * Compare this to given string; note that we compare full strings here.
 * 
 * @update gess 01/04/99
 * @param  aString is the other nsString to be compared to
 * @param   aCount tells us how many chars to test; -1 implies full length
 * @return TRUE if equal
 */
PRBool nsString::EqualsWithConversion(const nsString& aString,PRBool aIgnoreCase,PRInt32 aCount) const {
  PRInt32 theAnswer=nsStr::StrCompare(*this,aString,aCount,aIgnoreCase);
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
PRBool nsString::EqualsWithConversion(const char* aString,PRBool aIgnoreCase,PRInt32 aCount) const {
  PRInt32 theAnswer=CompareWithConversion(aString,aIgnoreCase,aCount);
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
PRBool nsString::EqualsWithConversion(const PRUnichar* aString,PRBool aIgnoreCase,PRInt32 aCount) const {
  PRInt32 theAnswer=CompareWithConversion(aString,aIgnoreCase,aCount);
  PRBool  result=PRBool(0==theAnswer);
  return result;
}

PRInt32 Compare2To2(const char* aStr1,const char* aStr2,PRUint32 aCount,PRBool aIgnoreCase);
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
PRBool nsString::EqualsAtom(/*FIX: const */nsIAtom* aAtom,PRBool aIgnoreCase) const{
  NS_ASSERTION(0!=aAtom,kNullPointerError);
  PRBool result=PR_FALSE;
  if(aAtom){
    PRInt32 cmp=0;
    const PRUnichar* unicode;
    if (aAtom->GetUnicode(&unicode) != NS_OK || unicode == nsnull)
        return PR_FALSE;
    cmp=Compare2To2((const char*)mUStr,(const char*)unicode, nsCRT::strlen(mUStr), aIgnoreCase);
    result=PRBool(0==cmp);
  }

   return result;
}

PRBool nsString::EqualsIgnoreCase(/*FIX: const */nsIAtom *aAtom) const {
  return EqualsAtom(aAtom,PR_TRUE);
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
    if(eOneByte==mCharSize) {
        char* aByte = mStr;
        while(*aByte) {
          if(*aByte & 0x80) { // don't use (*aByte > 0x7F) since char is signed
            return PR_FALSE;        
          }
          aByte++;
        }
      return PR_TRUE;
    } else {
      aBuffer=mUStr; // let the following code handle it
    }
  }
  if(aBuffer) {
    while(*aBuffer) {
      if(*aBuffer>0x007F){
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


/***********************************************************************
  IMPLEMENTATION NOTES: AUTOSTRING...
 ***********************************************************************/


/**
 * Default constructor
 *
 */
nsAutoString::nsAutoString() : nsString() {
  Initialize(*this,mBuffer,(sizeof(mBuffer)>>eTwoByte)-1,0,eTwoByte,PR_FALSE);
  AddNullTerminator(*this);
}

nsAutoString::nsAutoString(const PRUnichar* aString) : nsString() {
  Initialize(*this,mBuffer,(sizeof(mBuffer)>>eTwoByte)-1,0,eTwoByte,PR_FALSE);
  AddNullTerminator(*this);
  Append(aString);
}

/**
 * Copy construct from uni-string
 * @param   aString is a ptr to a unistr
 * @param   aLength tells us how many chars to copy from aString
 */
nsAutoString::nsAutoString(const PRUnichar* aString,PRInt32 aLength) : nsString() {
  Initialize(*this,mBuffer,(sizeof(mBuffer)>>eTwoByte)-1,0,eTwoByte,PR_FALSE);
  AddNullTerminator(*this);
  Append(aString,aLength);
}

nsAutoString::nsAutoString( const nsString& aString )
  : nsString()
{
  Initialize(*this, mBuffer, (sizeof(mBuffer)>>eTwoByte)-1, 0, eTwoByte, PR_FALSE);
  AddNullTerminator(*this);
  Append(aString);
}

nsAutoString::nsAutoString( const nsAString& aString )
  : nsString()
{
  Initialize(*this, mBuffer, (sizeof(mBuffer)>>eTwoByte)-1, 0, eTwoByte, PR_FALSE);
  AddNullTerminator(*this);
  Append(aString);
}



/**
 * constructor that uses external buffer
 * @param   aBuffer describes the external buffer
 */
nsAutoString::nsAutoString(const CBufDescriptor& aBuffer) : nsString() {
  if(!aBuffer.mBuffer) {
    Initialize(*this,mBuffer,(sizeof(mBuffer)>>eTwoByte)-1,0,eTwoByte,PR_FALSE);
  }
  else {
    Initialize(*this,aBuffer.mBuffer,aBuffer.mCapacity,aBuffer.mLength,aBuffer.mCharSize,!aBuffer.mStackBased);
  }
  if(!aBuffer.mIsConst)
    AddNullTerminator(*this);
}

void
NS_ConvertASCIItoUCS2::Init( const char* aCString, PRUint32 aLength )
  {
    AppendWithConversion(aCString,aLength);
  }

NS_ConvertASCIItoUCS2::NS_ConvertASCIItoUCS2( const nsACString& aCString )
  {
    SetCapacity(aCString.Length());

    nsACString::const_iterator start; aCString.BeginReading(start);
    nsACString::const_iterator end;   aCString.EndReading(end);

    while (start != end)
      {
        const nsReadableFragment<char>& frag = start.fragment();
        AppendWithConversion(frag.mStart, frag.mEnd - frag.mStart);
        start.advance(start.size_forward());
      }
  }

class UTF8traits
  {
    public:
      static PRBool isASCII(char c) { return (c & 0x80) == 0x00; }
      static PRBool isInSeq(char c) { return (c & 0xC0) == 0x80; }
      static PRBool is2byte(char c) { return (c & 0xE0) == 0xC0; }
      static PRBool is3byte(char c) { return (c & 0xF0) == 0xE0; }
      static PRBool is4byte(char c) { return (c & 0xF8) == 0xF0; }
      static PRBool is5byte(char c) { return (c & 0xFC) == 0xF8; }
      static PRBool is6byte(char c) { return (c & 0xFE) == 0xFC; }
  };

class CalculateUTF8Length
  {
    public:
      typedef nsACString::char_type value_type;

    CalculateUTF8Length() : mLength(0), mErrorEncountered(PR_FALSE) { }

    size_t Length() const { return mLength; }

    PRUint32 write( const value_type* start, PRUint32 N )
      {
          // ignore any further requests
        if ( mErrorEncountered )
            return N;

        // algorithm assumes utf8 units won't
        // be spread across fragments
        const value_type* p = start;
        const value_type* end = start + N;
        for ( ; p < end /* && *p */; ++mLength )
          {
            if ( UTF8traits::isASCII(*p) )
                p += 1;
            else if ( UTF8traits::is2byte(*p) )
                p += 2;
            else if ( UTF8traits::is3byte(*p) )
                p += 3;
            else if ( UTF8traits::is4byte(*p) )
                p += 4;
            else if ( UTF8traits::is5byte(*p) )
                p += 5;
            else if ( UTF8traits::is6byte(*p) )
                p += 6;
            else
              {
                break;
              }
          }
        if ( p != end )
          {
            NS_ERROR("Not a UTF-8 string. This code should only be used for converting from known UTF-8 strings.");
            mErrorEncountered = PR_TRUE;
            mLength = 0;
            return N;
          }
        return p - start;
      }

    private:
      size_t mLength;
      PRBool mErrorEncountered;
  };

class ConvertUTF8toUCS2
  {
    public:
      typedef nsACString::char_type value_type;
      typedef nsAString::char_type  buffer_type;

    ConvertUTF8toUCS2( buffer_type* aBuffer ) : mStart(aBuffer), mBuffer(aBuffer) {}

    size_t Length() const { return mBuffer - mStart; }

    PRUint32 write( const value_type* start, PRUint32 N )
      {
        // algorithm assumes utf8 units won't
        // be spread across fragments
        const value_type* p = start;
        const value_type* end = start + N;
        for ( ; p != end /* && *p */; )
          {
            char c = *p++;

            if ( UTF8traits::isASCII(c) )
              {
                *mBuffer++ = buffer_type(c);
                continue;
              }

            PRUint32 ucs4;
            PRUint32 minUcs4;
            PRInt32 state = 0;

            if ( UTF8traits::is2byte(c) )
              {
                ucs4 = (PRUint32(c) << 6) & 0x000007C0L;
                state = 1;
                minUcs4 = 0x00000080;
              }
            else if ( UTF8traits::is3byte(c) )
              {
                ucs4 = (PRUint32(c) << 12) & 0x0000F000L;
                state = 2;
                minUcs4 = 0x00000800;
              }
            else if ( UTF8traits::is4byte(c) )
              {
                ucs4 = (PRUint32(c) << 18) & 0x001F0000L;
                state = 3;
                minUcs4 = 0x00010000;
              }
            else if ( UTF8traits::is5byte(c) )
              {
                ucs4 = (PRUint32(c) << 24) & 0x03000000L;
                state = 4;
                minUcs4 = 0x00200000;
              }
            else if ( UTF8traits::is6byte(c) )
              {
                ucs4 = (PRUint32(c) << 30) & 0x40000000L;
                state = 5;
                minUcs4 = 0x04000000;
              }
            else
              {
                NS_ERROR("Not a UTF-8 string. This code should only be used for converting from known UTF-8 strings.");
                break;
              }

            while ( state-- )
              {
                c = *p++;

                if ( UTF8traits::isInSeq(c) )
                  {
                    PRInt32 shift = state * 6;
                    ucs4 |= (PRUint32(c) & 0x3F) << shift;
                  }
                else
                  {
                    NS_ERROR("not a UTF8 string");
                    return p - start;
                  }
              }

            if ( ucs4 < minUcs4 )
              {
                // Overlong sequence
                *mBuffer++ = 0xFFFD;
              }
            else if ( ucs4 <= 0xD7FF )
              {
                *mBuffer++ = ucs4;
              }
            else if ( /* ucs4 >= 0xD800 && */ ucs4 <= 0xDFFF )
              {
                // Surrogates
                *mBuffer++ = 0xFFFD;
              }
            else if ( ucs4 == 0xFFFE || ucs4 == 0xFFFF )
              {
                // Prohibited characters
                *mBuffer++ = 0xFFFD;
              }
            else if ( ucs4 >= 0x00010000 )
              {
                *mBuffer++ = 0xFFFD;
              }
            else
              {
                if ( ucs4 != 0xFEFF ) // ignore BOM
                    *mBuffer++ = ucs4;
              }
          }
        return p - start;
      }

    private:
      buffer_type* mStart;
      buffer_type* mBuffer;
  };

void
NS_ConvertUTF8toUCS2::Init( const nsACString& aCString )
{
  // Compute space required: do this once so we don't incur multiple
  // allocations. This "optimization" is probably of dubious value...

  nsACString::const_iterator start, end;
  CalculateUTF8Length calculator;
  copy_string(aCString.BeginReading(start), aCString.EndReading(end), calculator);

  PRUint32 count = calculator.Length();

  if (count) {

    // Grow the buffer if we need to.
    SetCapacity(count);
      // |SetCapacity| normally doesn't guarantee the use we are putting it to here (see its interface comment in nsAWritableString.h),
      //  we can only use it since our local implementation, |nsString::SetCapacity|, is known to do what we want

    // All ready? Time to convert

    ConvertUTF8toUCS2 converter(mUStr);
    copy_string(aCString.BeginReading(start), aCString.EndReading(end), converter);
    mLength = converter.Length();
    if (mCapacity)
      mUStr[mLength] = '\0'; // null terminate
  }

  NS_ASSERTION(count == mLength, "calculator calculated incorrect length");
}

#if 0
/**
 * Copy construct from ascii c-string
 * @param   aCString is a ptr to a 1-byte cstr
 * @param   aLength tells us how many chars to copy from aCString
 */
nsAutoString::nsAutoString(const char* aCString,PRInt32 aLength) : nsString() {
  Initialize(*this,mBuffer,(sizeof(mBuffer)>>eTwoByte)-1,0,eTwoByte,PR_FALSE);
  AddNullTerminator(*this);
  Append(aCString,aLength);
}

/**
 * Copy construct from an nsStr
 * @param   
 */
nsAutoString::nsAutoString(const nsStr& aString) : nsString() {
  Initialize(*this,mBuffer,(sizeof(mBuffer)>>eTwoByte)-1,0,eTwoByte,PR_FALSE);
  AddNullTerminator(*this);
  Append(aString);
}
#endif

/**
 * Default copy constructor
 */
nsAutoString::nsAutoString(const nsAutoString& aString) : nsString() {
  Initialize(*this,mBuffer,(sizeof(mBuffer)>>eTwoByte)-1,0,eTwoByte,PR_FALSE);
  AddNullTerminator(*this);
  Append(aString);
}


/**
 * Copy construct from a unichar
 * @param   
 */
nsAutoString::nsAutoString(PRUnichar aChar) : nsString(){
  Initialize(*this,mBuffer,(sizeof(mBuffer)>>eTwoByte)-1,0,eTwoByte,PR_FALSE);
  AddNullTerminator(*this);
  Append(aChar);
}

/**
 * deconstruct the autstring
 * @param   
 */
nsAutoString::~nsAutoString(){
}

#ifdef DEBUG
void nsAutoString::SizeOf(nsISizeOfHandler* aHandler, PRUint32* aResult) const {
  if (aResult) {
    *aResult = sizeof(*this) + mCapacity * mCharSize;
  }
}
#endif

