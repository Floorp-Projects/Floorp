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
#include "nsStrPrivate.h"
#include "nsString.h"
#include "nsReadableUtils.h"
#include "nsDebug.h"

#ifndef nsCharTraits_h___
#include "nsCharTraits.h"
#endif

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
  nsStrPrivate::Initialize(*this,eTwoByte);
}

nsString::nsString(const PRUnichar* aString) {
  nsStrPrivate::Initialize(*this,eTwoByte);
  Assign(aString);
}

/**
 * This constructor accepts a unicode string
 * @update  gess 1/4/99
 * @param   aString is a ptr to a unichar string
 * @param   aLength tells us how many chars to copy from given aString
 */
nsString::nsString(const PRUnichar* aString,PRInt32 aCount) {  
  nsStrPrivate::Initialize(*this,eTwoByte);
  Assign(aString,aCount);
}

/**
 * This is our copy constructor 
 * @update  gess 1/4/99
 * @param   reference to another nsString
 */
nsString::nsString(const nsString& aString) {
  nsStrPrivate::Initialize(*this,eTwoByte);
  nsStrPrivate::StrAssign(*this,aString,0,aString.mLength);
}

/**
 * Destructor
 * Make sure we call nsStrPrivate::Destroy.
 */
nsString::~nsString() {
  nsStrPrivate::Destroy(*this);
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
    nsStrPrivate::Initialize(temp, eTwoByte);
    temp.mUStr = buf;
    temp.mLength = 1;
    nsStrPrivate::StrAppend(*this, temp, 0, 1);
  }


nsString::nsString( const nsAString& aReadable ) {
  nsStrPrivate::Initialize(*this,eTwoByte);
  Assign(aReadable);
}

#ifdef DEBUG
void nsString::SizeOf(nsISizeOfHandler* aHandler, PRUint32* aResult) const {
  if (aResult) {
    *aResult = sizeof(*this) + GetCapacity() * GetCharSize();
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
  if ( anIndex > GetCapacity() )
    SetCapacity(anIndex);
    // |SetCapacity| normally doesn't guarantee the use we are putting it to here (see its interface comment in nsAString.h),
    //  we can only use it since our local implementation, |nsString::SetCapacity|, is known to do what we want
  nsStrPrivate::StrTruncate(*this,anIndex);
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
        if( aNewCapacity > GetCapacity() )
          nsStrPrivate::GrowCapacity(*this, aNewCapacity);
        AddNullTerminator(*this);
      }
    else
      {
        nsStrPrivate::Destroy(*this);
        nsStrPrivate::Initialize(*this, eTwoByte);
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
  const PRUnichar* result=(eOneByte==GetCharSize()) ? 0 : mUStr;
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
    if(eOneByte==GetCharSize())
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
    if(eOneByte==GetCharSize()) {
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
  nsStrPrivate::StripChars2(*this,aSet);
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
  if(eTwoByte==GetCharSize()){
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
      if(eTwoByte==GetCharSize()) 
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

    PRInt32 len=nsCharTraits<PRUnichar>::length(aTarget);
    if(0<len) {
      CBufDescriptor theDesc1(aTarget,PR_TRUE, len+1,len);
      nsAutoString theTarget(theDesc1);

      len=nsCharTraits<PRUnichar>::length(aNewValue);
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
      while(kNotFound!=(theIndex=nsStrPrivate::FindSubstr2in2(*this,aTarget,theIndex,mLength))) {
        if(aNewValue.mLength<aTarget.mLength) {
          //Since target is longer than newValue, we should delete a few chars first, then overwrite.
          PRInt32 theDelLen=aTarget.mLength-aNewValue.mLength;
          nsStrPrivate::Delete2(*this,theIndex,theDelLen);
          nsStrPrivate::Overwrite(*this,aNewValue,theIndex);
        }
        else {
          //this is the worst case: the newvalue is larger than the substr it's replacing
          //so we have to insert some characters...
          PRInt32 theInsLen=aNewValue.mLength-aTarget.mLength;
          nsStrPrivate::StrInsert2into2(*this,theIndex,aNewValue,0,theInsLen);
          nsStrPrivate::Overwrite(*this,aNewValue,theIndex);
          theIndex += aNewValue.mLength;
        }
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
    
    nsStrPrivate::Trim(*this,aTrimSet,aEliminateLeading,aEliminateTrailing);

    if(aIgnoreQuotes && theQuotesAreNeeded) {
      Insert(theFirstChar,0);
      Append(theLastChar);
    }

  }
}

/**
 * This method strips chars in given set from string.
 */
static void
CompressSet(nsString& aStr, const char* aSet, PRUnichar aChar,
            PRBool aEliminateLeading, PRBool aEliminateTrailing) {
  if (aSet) {
    aStr.ReplaceChar(aSet, aChar);
    nsStrPrivate::CompressSet2(aStr, aSet,
                               aEliminateLeading, aEliminateTrailing);
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
  CompressSet(*this, kWhitespace,' ', aEliminateLeading, aEliminateTrailing);
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
    nsStrPrivate::StrAssign(temp, *this, anOffset, PR_MIN(mLength, aBufLength-1));
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
  float res = 0.0f;
  char buf[100];
  if (mLength > 0 && mLength < sizeof(buf)) {
    char *conv_stopped;
    const char *str = ToCString(buf, sizeof(buf));
    // Use PR_strtod, not strtod, since we don't want locale involved.
    res = (float)PR_strtod(str, &conv_stopped);
    if (*conv_stopped == '\0') {
      *aErrorCode = (PRInt32) NS_OK;
    }
    else {
      /* Not all the string was scanned */
      *aErrorCode = (PRInt32) NS_ERROR_ILLEGAL_VALUE;
    }
  }
  else {
    /* The string was too short (0 characters) or too long (sizeof(buf)) */
    *aErrorCode = (PRInt32) NS_ERROR_ILLEGAL_VALUE;
  }
  return res;
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
      PRBool haveValue = PR_FALSE;

      while(cp<endcp){
        theChar=*cp++;
        if(('0'<=theChar) && (theChar<='9')){
          result = (theRadix * result) + (theChar-'0');
          haveValue = PR_TRUE;
        }
        else if((theChar>='A') && (theChar<='F')) {
          if(10==theRadix) {
            if(kAutoDetect==aRadix){
              theRadix=16;
              cp=first; //backup
              result=0;
              haveValue = PR_FALSE;
            }
            else {
              *anErrorCode=NS_ERROR_ILLEGAL_VALUE;
              result=0;
              break;
            }
          }
          else {
            result = (theRadix * result) + ((theChar-'A')+10);
            haveValue = PR_TRUE;
          }
        }
        else if((theChar>='a') && (theChar<='f')) {
          if(10==theRadix) {
            if(kAutoDetect==aRadix){
              theRadix=16;
              cp=first; //backup
              result=0;
              haveValue = PR_FALSE;
            }
            else {
              *anErrorCode=NS_ERROR_ILLEGAL_VALUE;
              result=0;
              break;
            }
          }
          else {
            result = (theRadix * result) + ((theChar-'a')+10);
            haveValue = PR_TRUE;
          }
        }
        else if((('X'==theChar) || ('x'==theChar)) && (!haveValue || result == 0)) {
          continue;
        }
        else if((('#'==theChar) || ('+'==theChar)) && !haveValue) {
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
  nsStrPrivate::StrTruncate(*this,0);
  if(aCString){
    AppendWithConversion(aCString,aCount);
  }
}

void nsString::AssignWithConversion(const char* aCString) {
  nsStrPrivate::StrTruncate(*this,0);
  if(aCString){
    AppendWithConversion(aCString);
  }
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
    nsStrPrivate::Initialize(temp,eOneByte);
    temp.mStr = NS_CONST_CAST(char*, aCString);

    if(0<aCount) {
      temp.mLength=aCount;

      // If this assertion fires, the caller is probably lying about the length of
      //   the passed-in string.  File a bug on the caller.

#ifdef NS_DEBUG
      PRInt32 len=nsStrPrivate::FindChar1(temp,0,0,temp.mLength);
      if(kNotFound<len) {
        NS_WARNING(kPossibleNull);
      }
#endif

    }
    else aCount=temp.mLength=nsCharTraits<char>::length(aCString);
      
    if(0<aCount)
      nsStrPrivate::StrAppend(*this,temp,0,aCount);
  }
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
    nsStrPrivate::Initialize(temp,eOneByte);
    temp.mStr = NS_CONST_CAST(char*, aCString);

    if(0<aCount) {
      temp.mLength=aCount;

      // If this assertion fires, the caller is probably lying about the length of
      //   the passed-in string.  File a bug on the caller.
#ifdef NS_DEBUG
      PRInt32 len=nsStrPrivate::FindChar1(temp,0,0,temp.mLength);
      if(kNotFound<len) {
        NS_WARNING(kPossibleNull);
      }
#endif

    }
    else aCount=temp.mLength=nsCharTraits<char>::length(aCString);

    if(0<aCount){
      nsStrPrivate::StrInsert1into2(*this,anOffset,temp,0,aCount);
    }
  }
}

void nsString::Adopt(PRUnichar* aPtr, PRInt32 aLength) {
  NS_ASSERTION(aPtr, "Can't adopt |0|");
  nsStrPrivate::Destroy(*this);
  if (aLength == -1)
    aLength = nsCharTraits<PRUnichar>::length(aPtr);
  // We don't know the capacity, so we'll just have to assume
  // capacity = length.
  nsStrPrivate::Initialize(*this, (char*)aPtr, aLength, aLength, eTwoByte, PR_TRUE);
}

nsAString::size_type
nsString::Mid( self_type& aResult, index_type aStartPos, size_type aLengthToCopy ) const
  {
      // If we're just assigning our entire self, give |aResult| the opportunity to share
    if ( aStartPos == 0 && aLengthToCopy >= Length() )
      aResult = *this;
    else
      aResult = Substring(*this, aStartPos, aLengthToCopy);

    return aResult.Length();
  }


/**********************************************************************
  Searching methods...                
 *********************************************************************/
 
  /**
   *  Search for given character within this string
   *  
   *  @param   aChar is the character to search for
   *  @param   anOffset tells us where in this string to start searching
               (optional parameter)
   *  @param   aCount tells us how far from the offset we are to search. Use
               -1 to search the whole string. (optional parameter)
   *  @return  offset in string, or -1 (kNotFound)
   */
PRInt32 nsString::FindChar(PRUnichar aChar, PRInt32 anOffset/*=0*/, PRInt32 aCount/*=-1*/) const{
  if (anOffset < 0)
    anOffset=0;
  
  if (aCount < 0)
    aCount = (PRInt32)mLength;
  
  if ((0 < mLength) && ((PRUint32)anOffset < mLength) && (0 < aCount)) {
    PRUint32 last = anOffset + aCount;
    PRUint32 end = (last < mLength) ? last : mLength;
    const PRUnichar* charp = mUStr + anOffset;
    const PRUnichar* endp = mUStr + end;

    while (charp < endp && *charp != aChar) {
      ++charp;
    }

    if (charp < endp)
      return charp - mUStr;
  }
  return kNotFound;
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
PRInt32 nsString::Find(const char* aCString,PRBool aIgnoreCase,PRInt32 anOffset,PRInt32 aCount) const{
  NS_ASSERTION(0!=aCString,kNullPointerError);

  PRInt32 result=kNotFound;
  if(aCString) {
    nsStr temp;
    nsStrPrivate::Initialize(temp,eOneByte);
    temp.mLength=nsCharTraits<char>::length(aCString);
    temp.mStr = NS_CONST_CAST(char*, aCString);
    result=nsStrPrivate::FindSubstr1in2(*this,temp,aIgnoreCase,anOffset,aCount);
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
PRInt32 nsString::Find(const PRUnichar* aString,PRInt32 anOffset,PRInt32 aCount) const{
  NS_ASSERTION(0!=aString,kNullPointerError);

  PRInt32 result=kNotFound;
  if(aString) {
    nsStr temp;
    nsStrPrivate::Initialize(temp,eTwoByte);
    temp.mLength = nsCharTraits<PRUnichar>::length(aString);
    temp.mUStr = NS_CONST_CAST(PRUnichar*, aString);
    result=nsStrPrivate::FindSubstr2in2(*this,temp,anOffset,aCount);
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
PRInt32 nsString::Find(const nsAFlatString& aString,PRInt32 anOffset,PRInt32 aCount) const{

  nsStr temp;
  nsStrPrivate::Initialize(temp, eTwoByte);
  temp.mLength = aString.Length();
  temp.mUStr = NS_CONST_CAST(PRUnichar*, aString.get());
  
  PRInt32 result=nsStrPrivate::FindSubstr2in2(*this,temp,anOffset,aCount);
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

  if (anOffset < 0)
    anOffset = 0;

  if(*aCStringSet && (PRUint32)anOffset < mLength) {
    // Build filter that will be used to filter out characters with
    // bits that none of the terminal chars have. This works very well
    // because searches are often done for chars with only the last
    // 4-6 bits set and normal ascii letters have bit 7 set. Other
    // letters have even higher bits set.

    // Calculate filter
    PRUnichar filter = (~PRUnichar(0)^~char(0)) |
                       nsStrPrivate::GetFindInSetFilter(aCStringSet);

    const PRUnichar* endChar = mUStr + mLength;
    for(PRUnichar *charp = mUStr + anOffset; charp < endChar; ++charp) {
      PRUnichar currentChar = *charp;
      // Check if all bits are in the required area
      if (currentChar & filter) {
        // They were not. Go on with the next char.
        continue;
      }
        
      // Test all chars
      const char *setp = aCStringSet;
      PRUnichar setChar = PRUnichar(*setp);
        while (setChar) {
          if (setChar == currentChar) {
            // Found it!
            return charp - mUStr; // The index of the found char
          }
          setChar = PRUnichar(*(++setp));
        }
    } // end for all chars in the string
  }
  return kNotFound;
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

  if (anOffset < 0)
    anOffset = 0;

  if (*aStringSet && (PRUint32)anOffset < mLength) {
    // Build filter that will be used to filter out characters with
    // bits that none of the terminal chars have. This works very well
    // because searches are often done for chars with only the last
    // 4-6 bits set and normal ascii letters have bit 7 set. Other
    // letters have even higher bits set.

    // Calculate filter
    PRUnichar filter = nsStrPrivate::GetFindInSetFilter(aStringSet);

    const PRUnichar* endChar = mUStr + mLength;
    for(PRUnichar *charp = mUStr + anOffset;charp < endChar; ++charp) {
      PRUnichar currentChar = *charp;
      // Check if all bits are in the required area
      if (currentChar & filter) {
        // They were not. Go on with the next char.
        continue;
      }

      // Test all chars
      const PRUnichar *setp = aStringSet;
      PRUnichar setChar = *setp;
      while (setChar) {
        if (setChar == currentChar) {
          // Found it!
          return charp - mUStr; // The index of the found char
        }
        setChar = *(++setp);
      }
    } // end for all chars in the string
  }
  return kNotFound;
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
PRInt32 nsString::RFind(const nsAFlatString& aString,PRInt32 anOffset,PRInt32 aCount) const
{
  nsStr temp;
  nsStrPrivate::Initialize(temp, eTwoByte);
  temp.mLength = aString.Length();
  temp.mUStr = NS_CONST_CAST(PRUnichar*, aString.get());
  PRInt32 result=nsStrPrivate::RFindSubstr2in2(*this,temp,anOffset,aCount);
  return result;
}

PRInt32 nsString::RFind(const PRUnichar* aString, PRInt32 anOffset, PRInt32 aCount) const
{
  PRInt32 result=kNotFound;
  if (aString) {
    nsStr temp;
    nsStrPrivate::Initialize(temp, eTwoByte);
    temp.mLength = nsCharTraits<PRUnichar>::length(aString);
    temp.mUStr = NS_CONST_CAST(PRUnichar*, aString);
    result=nsStrPrivate::RFindSubstr2in2(*this,temp,anOffset,aCount);
  }
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
    nsStrPrivate::Initialize(temp,eOneByte);
    temp.mLength=nsCharTraits<char>::length(aString);
    temp.mStr = NS_CONST_CAST(char*, aString);
    result=nsStrPrivate::RFindSubstr1in2(*this,temp,aIgnoreCase,anOffset,aCount);
  }
  return result;
}

/**
 *  Reverse search for a given char, starting at given offset
 *  
 *  @update  gess 3/25/98
 *  @param   aChar
 *  @param   aIgnoreCase
 *  @param   anOffset
 *  @return  offset of found char, or -1 (kNotFound)
 */
PRInt32 nsString::RFindChar(PRUnichar aChar,PRInt32 anOffset,PRInt32 aCount) const{
  PRInt32 result=nsStrPrivate::RFindChar2(*this,aChar,anOffset,aCount);
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

  if (anOffset < 0 || (PRUint32)anOffset >= mLength)
    anOffset = mLength - 1;

  if(*aStringSet) {
    // Build filter that will be used to filter out characters with
    // bits that none of the terminal chars have. This works very well
    // because searches are often done for chars with only the last
    // 4-6 bits set and normal ascii letters have bit 7 set. Other
    // letters have even higher bits set.

    // Calculate filter
    PRUnichar filter = nsStrPrivate::GetFindInSetFilter(aStringSet);
    
    const PRUnichar* endp = mUStr - 1;
    for(PRUnichar *charp = mUStr + anOffset; charp > endp; --charp) {
      PRUnichar currentChar = *charp;
      // Check if all bits are in the required area
      if (currentChar & filter) {
        // They were not. Go on with the next char.
        continue;
      }
      
      // Test all chars
      const PRUnichar* setp = aStringSet;
      PRUnichar setChar = *setp;
      while (setChar) {
        if (setChar == currentChar) {
          // Found it!
          return charp - mUStr;
        }
        setChar = *(++setp);
      }
    } // end for all chars in the string
  }
  return kNotFound;
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
    nsStrPrivate::Initialize(temp,eOneByte);

    temp.mLength= (0<aCount) ? aCount : nsCharTraits<char>::length(aCString);

    temp.mStr = NS_CONST_CAST(char*, aCString);
    return nsStrPrivate::StrCompare2To1(*this,temp,aCount,aIgnoreCase);
  }

  return 0;
}

PRBool nsString::EqualsIgnoreCase(const char* aString,PRInt32 aLength) const {
  return EqualsWithConversion(aString,PR_TRUE,aLength);
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

PRInt32 Compare2To2(const PRUnichar* aStr1,const PRUnichar* aStr2,PRUint32 aCount);

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
    if(eOneByte==GetCharSize()) {
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


/***********************************************************************
  IMPLEMENTATION NOTES: AUTOSTRING...
 ***********************************************************************/


/**
 * Default constructor
 *
 */
nsAutoString::nsAutoString() : nsString() {
  nsStrPrivate::Initialize(*this, (char*)mBuffer, (sizeof(mBuffer)/sizeof(mBuffer[0]))-1, 0, eTwoByte, PR_FALSE);
  AddNullTerminator(*this);
}

nsAutoString::nsAutoString(const PRUnichar* aString) : nsString() {
  nsStrPrivate::Initialize(*this, (char*)mBuffer, (sizeof(mBuffer)/sizeof(mBuffer[0]))-1, 0, eTwoByte, PR_FALSE);
  AddNullTerminator(*this);
  Append(aString);
}

/**
 * Copy construct from uni-string
 * @param   aString is a ptr to a unistr
 * @param   aLength tells us how many chars to copy from aString
 */
nsAutoString::nsAutoString(const PRUnichar* aString,PRInt32 aLength) : nsString() {
  nsStrPrivate::Initialize(*this, (char*)mBuffer, (sizeof(mBuffer)/sizeof(mBuffer[0]))-1, 0, eTwoByte, PR_FALSE);
  AddNullTerminator(*this);
  Append(aString,aLength);
}

nsAutoString::nsAutoString( const nsString& aString )
  : nsString()
{
  nsStrPrivate::Initialize(*this, (char*)mBuffer, (sizeof(mBuffer)/sizeof(mBuffer[0]))-1, 0, eTwoByte, PR_FALSE);
  AddNullTerminator(*this);
  Append(aString);
}

nsAutoString::nsAutoString( const nsAString& aString )
  : nsString()
{
  nsStrPrivate::Initialize(*this, (char*)mBuffer, (sizeof(mBuffer)/sizeof(mBuffer[0]))-1, 0, eTwoByte, PR_FALSE);
  AddNullTerminator(*this);
  Append(aString);
}



/**
 * constructor that uses external buffer
 * @param   aBuffer describes the external buffer
 */
nsAutoString::nsAutoString(const CBufDescriptor& aBuffer) : nsString() {
  if(!aBuffer.mBuffer) {
    nsStrPrivate::Initialize(*this, (char*)mBuffer, (sizeof(mBuffer)/sizeof(mBuffer[0]))-1, 0, eTwoByte, PR_FALSE);
  }
  else {
    nsStrPrivate::Initialize(*this, aBuffer.mBuffer, aBuffer.mCapacity, aBuffer.mLength, aBuffer.mCharSize, !aBuffer.mStackBased);
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
            else if ( UTF8traits::is4byte(*p) ) {
                p += 4;
                ++mLength;
            }
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
    SetLength(count);

    // All ready? Time to convert

    ConvertUTF8toUCS2 converter(mUStr);
    copy_string(aCString.BeginReading(start), aCString.EndReading(end), converter);
    mLength = converter.Length();
    if (mLength != count) {
      NS_ERROR("Input wasn't UTF8 or incorrect length was calculated");
      Truncate();
    }
  }

}

/**
 * Default copy constructor
 */
nsAutoString::nsAutoString(const nsAutoString& aString) : nsString() {
  nsStrPrivate::Initialize(*this,(char*)mBuffer,(sizeof(mBuffer)/sizeof(mBuffer[0]))-1,0,eTwoByte,PR_FALSE);
  AddNullTerminator(*this);
  Append(aString);
}


/**
 * Copy construct from a unichar
 * @param   
 */
nsAutoString::nsAutoString(PRUnichar aChar) : nsString(){
  nsStrPrivate::Initialize(*this,(char*)mBuffer,(sizeof(mBuffer)/sizeof(mBuffer[0]))-1,0,eTwoByte,PR_FALSE);
  AddNullTerminator(*this);
  Append(aChar);
}

#ifdef DEBUG
void nsAutoString::SizeOf(nsISizeOfHandler* aHandler, PRUint32* aResult) const {
  if (aResult) {
    *aResult = sizeof(*this) + GetCapacity() * GetCharSize();
  }
}
#endif

