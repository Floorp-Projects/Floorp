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
 *   Rick Gessner <rickg@netscape.com>
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
#include <stdio.h>
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

const nsBufferHandle<char>*
nsCString::GetFlatBufferHandle() const
  {
    return NS_REINTERPRET_CAST(const nsBufferHandle<char>*, 1);
  }

/**
 * Default constructor. 
 */
nsCString::nsCString()  {
  nsStrPrivate::Initialize(*this,eOneByte);
}

nsCString::nsCString(const char* aCString) {  
  nsStrPrivate::Initialize(*this,eOneByte);
  Assign(aCString);
}

/**
 * This constructor accepts an ascii string
 * @update  gess 1/4/99
 * @param   aCString is a ptr to a 1-byte cstr
 * @param   aLength tells us how many chars to copy from given CString
 */
nsCString::nsCString(const char* aCString,PRInt32 aLength) {  
  nsStrPrivate::Initialize(*this,eOneByte);
  Assign(aCString,aLength);
}

/**
 * This is our copy constructor 
 * @update  gess 1/4/99
 * @param   reference to another nsCString
 */
nsCString::nsCString(const nsCString& aString)  {
  nsStrPrivate::Initialize(*this,aString.GetCharSize());
  nsStrPrivate::StrAssign(*this,aString,0,aString.mLength);
}

/**
 * Destructor
 */
nsCString::~nsCString() {
  nsStrPrivate::Destroy(*this);
}

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

nsCString::nsCString( const nsACString& aReadable ) {
  nsStrPrivate::Initialize(*this,eOneByte);
  Assign(aReadable);
}

#ifdef DEBUG
void nsCString::SizeOf(nsISizeOfHandler* aHandler, PRUint32* aResult) const {
  if (aResult) {
    *aResult = sizeof(*this) + GetCapacity() * GetCharSize();
  }
}
#endif

/**
 * This method truncates this string to given length.
 *
 * @update  rickg 03.23.2000
 * @param   anIndex -- new length of string
 * @return  nada
 */
void nsCString::SetLength(PRUint32 anIndex) {
  if ( anIndex > GetCapacity() )
    SetCapacity(anIndex);
    // |SetCapacity| normally doesn't guarantee the use we are putting it to here (see its interface comment in nsAString.h),
    //  we can only use it since our local implementation, |nsCString::SetCapacity|, is known to do what we want

  nsStrPrivate::StrTruncate(*this,anIndex);
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
        if( aNewCapacity > GetCapacity() )
          nsStrPrivate::GrowCapacity(*this,aNewCapacity);
        AddNullTerminator(*this);
      }
    else
      {
        nsStrPrivate::Destroy(*this);
        nsStrPrivate::Initialize(*this, eOneByte);
      }
  }

/**********************************************************************
  Accessor methods...
 *********************************************************************/

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

/**********************************************************************
  Lexomorphic transforms...
 *********************************************************************/

/**
 *  This method is used to remove all occurances of the
 *  characters found in aSet from this string.
 *  
 *  @update rickg 03.27.2000
 *  @param  aChar -- char to be stripped
 *  @return *this 
 */
void
nsCString::StripChar(char aChar,PRInt32 anOffset){
  if(mLength && (anOffset<PRInt32(mLength))) {
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
  nsStrPrivate::StripChars1(*this,aSet);
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
nsCString::ReplaceChar(char aOldChar, char aNewChar) {
  PRUint32 theIndex=0;
  
  //only execute this if oldchar and newchar are within legal ascii range
  for(theIndex=0; theIndex < mLength; theIndex++){
    
    if(mStr[theIndex] == aOldChar) {
      mStr[theIndex] = aNewChar;
    }//if
    
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
nsCString::ReplaceChar(const char* aSet, char aNewChar){
  if(aSet) {
      //only execute this if newchar is valid ascii, and aset isn't null.

    PRInt32 theIndex=FindCharInSet(aSet,0);
    
    while(kNotFound < theIndex) {
      mStr[theIndex] = aNewChar;
      theIndex = FindCharInSet(aSet,theIndex+1);
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

    PRInt32 len=nsCharTraits<char>::length(aTarget);
    if(0<len) {
      CBufDescriptor theDesc1(aTarget,PR_TRUE, len+1,len);
      nsCAutoString theTarget(theDesc1);

      len=nsCharTraits<char>::length(aNewValue);
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
      while(kNotFound!=(theIndex=nsStrPrivate::FindSubstr1in1(*this,aTarget,PR_FALSE,theIndex,mLength))) {
        if(aNewValue.mLength<aTarget.mLength) {
          //Since target is longer than newValue, we should delete a few chars first, then overwrite.
          PRInt32 theDelLen=aTarget.mLength-aNewValue.mLength;
          nsStrPrivate::Delete1(*this,theIndex,theDelLen);
          nsStrPrivate::Overwrite(*this,aNewValue,theIndex);
        }
        else {
          //this is the worst case: the newvalue is larger than the substr it's replacing
          //so we have to insert some characters...
          PRInt32 theInsLen=aNewValue.mLength-aTarget.mLength;
          nsStrPrivate::StrInsert1into1(*this,theIndex,aNewValue,0,theInsLen);
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
nsCString::Trim(const char* aTrimSet, PRBool aEliminateLeading,PRBool aEliminateTrailing,PRBool aIgnoreQuotes){

  if(aTrimSet){
    
    char theFirstChar = 0;
    char theLastChar = 0;
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
    
    nsStrPrivate::Trim(*this, aTrimSet, aEliminateLeading, aEliminateTrailing);

    if(aIgnoreQuotes && theQuotesAreNeeded) {
      Insert(theFirstChar, 0);
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
static void
CompressSet(nsCString& aStr, const char* aSet, char aChar,
            PRBool aEliminateLeading, PRBool aEliminateTrailing){
  if(aSet){
    aStr.ReplaceChar(aSet, aChar);
    nsStrPrivate::CompressSet1(aStr, aSet,
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
nsCString::CompressWhitespace( PRBool aEliminateLeading,PRBool aEliminateTrailing){
  CompressSet(*this, kWhitespace,' ',aEliminateLeading,aEliminateTrailing);
}

/**********************************************************************
  string conversion methods...
 *********************************************************************/

/**
 * Perform string to float conversion.
 * @update  gess 01/04/99
 * @param   aErrorCode will contain error if one occurs
 * @return  float rep of string value
 */
float nsCString::ToFloat(PRInt32* aErrorCode) const {
  float res = 0.0f;
  if (mLength > 0) {
    char *conv_stopped;
    const char *str = get();
    // Use PR_strtod, not strtod, since we don't want locale involved.
    res = (float)PR_strtod(str, &conv_stopped);
    if (conv_stopped == str+mLength) {
      *aErrorCode = (PRInt32) NS_OK;
    }
    else {
      /* Not all the string was scanned */
      *aErrorCode = (PRInt32) NS_ERROR_ILLEGAL_VALUE;
    }
  }
  else {
    /* The string was too short (0 characters) */
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
 * assign given unichar* to this string
 * @update  gess 01/04/99
 * @param   aCString: buffer to be assigned to this 
 * @param   aCount -- length of given buffer or -1 if you want me to compute length.
 * NOTE:    IFF you pass -1 as aCount, then your buffer must be null terminated.
 *
 * @return  this
 */
void nsCString::AssignWithConversion(const PRUnichar* aString,PRInt32 aCount) {
  nsStrPrivate::StrTruncate(*this,0);

  if(aString && aCount){
    nsStr temp;
    nsStrPrivate::Initialize(temp,eTwoByte);
    temp.mUStr=(PRUnichar*)aString;

    if(0<aCount) {
      temp.mLength=aCount;

      // If this assertion fires, the caller is probably lying about the length of
      //   the passed-in string.  File a bug on the caller.
#ifdef NS_DEBUG
      PRInt32 len=nsStrPrivate::FindChar2(temp,0,PR_FALSE,temp.mLength);
      if(kNotFound<len) {
        NS_WARNING(kPossibleNull);
      }
#endif

    }
    else aCount=temp.mLength=nsCharTraits<PRUnichar>::length(aString);

    if(0<aCount)
      nsStrPrivate::StrAppend(*this,temp,0,aCount);
  }
}

void nsCString::AssignWithConversion( const nsString& aString ) {
  AssignWithConversion(aString.get(), aString.Length());
}

void nsCString::AssignWithConversion( const nsAString& aString ) {
  nsStrPrivate::StrTruncate(*this,0);
  PRInt32 count = aString.Length();

  if(count){   
    nsAString::const_iterator start; aString.BeginReading(start);
    nsAString::const_iterator end;   aString.EndReading(end);
    
    while (start != end) {
      PRUint32 fraglen = start.size_forward();

      nsStr temp;
      nsStrPrivate::Initialize(temp,eTwoByte);
      temp.mUStr=(PRUnichar*)start.get();

      temp.mLength=fraglen;

      nsStrPrivate::StrAppend(*this,temp,0,fraglen);

      start.advance(fraglen);
    }
  }
}

void nsCString::AppendWithConversion( const nsAString& aString ) {
  PRInt32 count = aString.Length();

  if(count){   
    nsAString::const_iterator start; aString.BeginReading(start);
    nsAString::const_iterator end;   aString.EndReading(end);
    
    while (start != end) {
      PRUint32 fraglen = start.size_forward();

      nsStr temp;
      nsStrPrivate::Initialize(temp,eTwoByte);
      temp.mUStr=(PRUnichar*)start.get();

      temp.mLength=fraglen;

      nsStrPrivate::StrAppend(*this,temp,0,fraglen);
      
      start.advance(fraglen);
    }
  }
}

void nsCString::AppendWithConversion( const PRUnichar* aBuffer, PRInt32 aLength )
  {
    nsStr temp;
    nsStrPrivate::Initialize(temp, eTwoByte);
    temp.mUStr = NS_CONST_CAST(PRUnichar*, aBuffer);

    if ( aLength < 0 )
      aLength = nsCharTraits<PRUnichar>::length(aBuffer);

    if ( aLength > 0 )
      {
        temp.mLength = aLength;
        nsStrPrivate::StrAppend(*this, temp, 0, aLength);
      }
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
    nsStrPrivate::StrAppend(*this,aString,0,aCount);
}

void nsCString::Adopt(char* aPtr, PRInt32 aLength) {
  NS_ASSERTION(aPtr, "Can't adopt |0|");
  nsStrPrivate::Destroy(*this);
  if (aLength == -1)
    aLength = nsCharTraits<char>::length(aPtr);
  // We don't know the capacity, so we'll just have to assume
  // capacity = length.
  nsStrPrivate::Initialize(*this, aPtr, aLength, aLength, eOneByte, PR_TRUE);
}

nsCString::size_type
nsCString::Mid( self_type& aResult, index_type aStartPos, size_type aLengthToCopy ) const
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
    nsStrPrivate::Initialize(temp,eOneByte);
    temp.mLength = nsCharTraits<char>::length(aCString);
    temp.mStr=(char*)aCString;
    result=nsStrPrivate::FindSubstr1in1(*this,temp,aIgnoreCase,anOffset,aCount);
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
PRInt32 nsCString::Find(const nsCString& aString,PRBool aIgnoreCase,PRInt32 anOffset,PRInt32 aCount) const{
  PRInt32 result=nsStrPrivate::FindSubstr1in1(*this,aString,aIgnoreCase,anOffset,aCount);
  return result;
}

/**
 *  This searches this string for a given character 
 *  
 *  @update  gess 2/04/00
 *  @param   char is the character you're trying to find.
 *  @param   anOffset tells us where to start the search; -1 means start at 0.
 *  @param   aCount tell us how many chars to search from offset; -1 means use full length.
 *  @return  index in aDest where member of aSet occurs, or -1 if not found
 */
PRInt32 nsCString::FindChar(PRUnichar aChar,PRInt32 anOffset,PRInt32 aCount) const{
  if (anOffset < 0)
    anOffset=0;
  
  if (aCount < 0)
    aCount = (PRInt32)mLength;
  
  if ((aChar < 256) && (0 < mLength) &&
      ((PRUint32)anOffset < mLength) && (0 < aCount)) {
    // We'll only search if the given aChar is 8-bit,
    // since this string is 8-bit.

    PRUint32 last = anOffset + aCount;
    PRUint32 end = (last < mLength) ? last : mLength;
    PRUint32 searchLen = end - anOffset; // Will be > 0 by the conditions above
    const char* leftp = mStr + anOffset;
    unsigned char theChar = (unsigned char) aChar;
    
    const char* result = (const char*)memchr(leftp, (int)theChar, searchLen);
      
    if (result)
      return result - mStr;
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
PRInt32 nsCString::FindCharInSet(const char* aCStringSet,PRInt32 anOffset) const{
  if (anOffset < 0)
    anOffset = 0;

  if(*aCStringSet && (PRUint32)anOffset < mLength) {
    // Build filter that will be used to filter out characters with
    // bits that none of the terminal chars have. This works very well
    // because searches are often done for chars with only the last
    // 4-6 bits set and normal ascii letters have bit 7 set. Other
    // letters have even higher bits set.

    // Calculate filter
    char filter = nsStrPrivate::GetFindInSetFilter(aCStringSet);
    
    const char* endChar = mStr + mLength;
    for(char *charp = mStr + anOffset; charp < endChar; ++charp) {
      char currentChar = *charp;
      // Check if all bits are in the required area
      if (currentChar & filter) {
        // They were not. Go on with the next char.
        continue;
      }
      
      // Test all chars
      const char *charInSet = aCStringSet;
      char setChar = *charInSet;
      while (setChar) {
        if (setChar == currentChar) {
          // Found it!
          return charp - mStr; // The index of the found char
        }
        setChar = *(++charInSet);
      }
    } // end for all chars in the string
  }
  return kNotFound;
}

PRInt32 nsCString::FindCharInSet(const nsCString& aSet,PRInt32 anOffset) const{
  // This assumes that the set doesn't contain the null char.
  PRInt32 result = FindCharInSet(aSet.get(), anOffset);
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
PRInt32 nsCString::RFind(const nsCString& aString,PRBool aIgnoreCase,PRInt32 anOffset,PRInt32 aCount) const{
  PRInt32 result=nsStrPrivate::RFindSubstr1in1(*this,aString,aIgnoreCase,anOffset,aCount);
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
    nsStrPrivate::Initialize(temp,eOneByte);
    temp.mLength=nsCharTraits<char>::length(aString);
    temp.mStr=(char*)aString;
    result=nsStrPrivate::RFindSubstr1in1(*this,temp,aIgnoreCase,anOffset,aCount);
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
PRInt32 nsCString::RFindChar(PRUnichar aChar,PRInt32 anOffset,PRInt32 aCount) const{
  PRInt32 result=nsStrPrivate::RFindChar1(*this,aChar,anOffset,aCount);
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

  if (anOffset < 0 || (PRUint32)anOffset > mLength-1)
    anOffset = mLength-1;

  if(*aCStringSet) {
    // Build filter that will be used to filter out characters with
    // bits that none of the terminal chars have. This works very well
    // because searches are often done for chars with only the last
    // 4-6 bits set and normal ascii letters have bit 7 set. Other
    // letters have even higher bits set.
    
    // Calculate filter
    char filter = nsStrPrivate::GetFindInSetFilter(aCStringSet);
    
    const char* end = mStr;
    for(char *charp = mStr + anOffset; charp > end; --charp) {
      char currentChar = *charp;
      // Check if all bits are in the required area
      if (currentChar & filter) {
        // They were not. Go on with the next char.
        continue;
      }
        
      // Test all chars
      const char *setp = aCStringSet;
      char setChar = *setp;
      while (setChar) {
        if (setChar == currentChar) {
          // Found it!
          return charp - mStr;
        }
        setChar = *(++setp);
      }
    } // end for all chars in the string
  }
  return kNotFound;
}

PRInt32 nsCString::RFindCharInSet(const nsCString& aSet,PRInt32 anOffset) const{
  // Assumes that the set doesn't contain any nulls.
  PRInt32 result = RFindCharInSet(aSet.get(), anOffset);
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
    nsStrPrivate::Initialize(temp,eOneByte);
    temp.mLength=nsCharTraits<char>::length(aCString);
    temp.mStr=(char*)aCString;
    return nsStrPrivate::StrCompare1To1(*this,temp,aCount,aIgnoreCase);
  }
  return 0;
}

PRBool nsCString::EqualsIgnoreCase(const char* aString,PRInt32 aLength) const {
  return EqualsWithConversion(aString,PR_TRUE,aLength);
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
PRBool nsCString::EqualsWithConversion(const char* aCString,PRBool aIgnoreCase,PRInt32 aCount) const{
  PRInt32 theAnswer=Compare(aCString,aIgnoreCase,aCount);
  PRBool  result=PRBool(0==theAnswer);  
  return result;
}

//----------------------------------------------------------------------

NS_ConvertUCS2toUTF8::NS_ConvertUCS2toUTF8( const nsAString& aString )
  {
    nsAString::const_iterator start; aString.BeginReading(start);
    nsAString::const_iterator end;   aString.EndReading(end);
    
    while (start != end) {
      nsReadableFragment<PRUnichar> frag(start.fragment());
      Append(frag.mStart, frag.mEnd - frag.mStart);
      start.advance(start.size_forward());
    }
  }

void
NS_ConvertUCS2toUTF8::Append( const PRUnichar* aString, PRUint32 aLength )
  {
    // Handle null string by just leaving us as a brand-new
    // uninitialized nsCAutoString.
    if (! aString)
      return;

    // Calculate how many bytes we need
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
    if(mLength+PRUint32(utf8len+1) > sizeof(mBuffer))
      SetCapacity(mLength+utf8len+1);
    // |SetCapacity| normally doesn't guarantee the use we are putting it to here (see its interface comment in nsAString.h),
    //  we can only use it since our local implementation, |nsCString::SetCapacity|, is known to do what we want

    char* out = mStr+mLength;
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
    mLength += utf8len;
  }

NS_LossyConvertUCS2toASCII::NS_LossyConvertUCS2toASCII( const nsAString& aString )
  {
    SetCapacity(aString.Length());

    nsAString::const_iterator start; aString.BeginReading(start);
    nsAString::const_iterator end;   aString.EndReading(end);
    
    while (start != end) {
      nsReadableFragment<PRUnichar> frag(start.fragment());
      AppendWithConversion(frag.mStart, frag.mEnd - frag.mStart);
      start.advance(start.size_forward());
    }
  }


/***********************************************************************
  IMPLEMENTATION NOTES: AUTOSTRING...
 ***********************************************************************/


/**
 * Default constructor
 *
 */
nsCAutoString::nsCAutoString() : nsCString(){
  nsStrPrivate::Initialize(*this,mBuffer,sizeof(mBuffer)-1,0,eOneByte,PR_FALSE);
  AddNullTerminator(*this);

}

nsCAutoString::nsCAutoString( const nsCString& aString ) : nsCString(){
  nsStrPrivate::Initialize(*this,mBuffer,sizeof(mBuffer)-1,0,eOneByte,PR_FALSE);
  AddNullTerminator(*this);
  Append(aString);
}

nsCAutoString::nsCAutoString( const nsACString& aString ) : nsCString(){
  nsStrPrivate::Initialize(*this,mBuffer,sizeof(mBuffer)-1,0,eOneByte,PR_FALSE);
  AddNullTerminator(*this);
  Append(aString);
}

nsCAutoString::nsCAutoString(const char* aCString) : nsCString() {
  nsStrPrivate::Initialize(*this,mBuffer,sizeof(mBuffer)-1,0,eOneByte,PR_FALSE);
  AddNullTerminator(*this);
  Append(aCString);
}

/**
 * Copy construct from ascii c-string
 * @param   aCString is a ptr to a 1-byte cstr
 */
nsCAutoString::nsCAutoString(const char* aCString,PRInt32 aLength) : nsCString() {
  nsStrPrivate::Initialize(*this,mBuffer,sizeof(mBuffer)-1,0,eOneByte,PR_FALSE);
  AddNullTerminator(*this);
  Append(aCString,aLength);
}

/**
 * Copy construct using an external buffer descriptor
 * @param   aBuffer -- descibes external buffer
 */
nsCAutoString::nsCAutoString(const CBufDescriptor& aBuffer) : nsCString() {
  if(!aBuffer.mBuffer) {
    nsStrPrivate::Initialize(*this,mBuffer,sizeof(mBuffer)-1,0,eOneByte,PR_FALSE);
  }
  else {
    nsStrPrivate::Initialize(*this,aBuffer.mBuffer,aBuffer.mCapacity,aBuffer.mLength,aBuffer.mCharSize,!aBuffer.mStackBased);
  }
  if(!aBuffer.mIsConst)
    AddNullTerminator(*this); //this isn't really needed, but it guarantees that folks don't pass string constants.
}


#ifdef DEBUG
void nsCAutoString::SizeOf(nsISizeOfHandler* aHandler, PRUint32* aResult) const {
  if (aResult) {
    *aResult = sizeof(*this) + GetCapacity() * GetCharSize();
  }
}
#endif
