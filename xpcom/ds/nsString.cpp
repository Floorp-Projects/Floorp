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
#include <iostream.h>
#include <ctype.h>
#include <string.h>
#include <stdio.h>
#include "nsString.h"
#include "nsCRT.h"
#include "nsDebug.h"
#include "prprf.h"
#include "prdtoa.h"


const PRInt32 kGrowthDelta = 8;
const PRInt32 kNotFound = -1;
PRUnichar gBadChar = 0;
const char* kOutOfBoundsError = "Error: out of bounds";
const char* kNullPointerError = "Error: unexpected null ptr";

//**********************************************
//NOTE: Our buffer always hold capacity+1 bytes.
//**********************************************


/**
 * Default constructor. We assume that this string will have
 * a bunch of Append's or SetString's done to it, therefore
 * we start with a mid sized buffer.
 */
nsString::nsString() {
  mLength = mCapacity = 0;
  mStr = 0;
  // Size to 4*kGrowthDelta (EnsureCapacityFor adds in kGrowthDelta so
  // subtract it before calling)
  EnsureCapacityFor(4*kGrowthDelta - kGrowthDelta);
}

nsString::nsString(const char* anISOLatin1) {  
  mLength=mCapacity=0;
  mStr=0;
  PRInt32 len=strlen(anISOLatin1);
  EnsureCapacityFor(len);
  this->SetString(anISOLatin1,len);
}

/*-------------------------------------------------------
 * constructor from string
 * @update	gess 3/27/98
 * @param 
 * @return
 *------------------------------------------------------*/
nsString::nsString(const nsString &aString) {
  mLength=mCapacity=0;
  mStr=0;
  EnsureCapacityFor(aString.mLength);
  this->SetString(aString.mStr,aString.mLength);
}

/*-------------------------------------------------------
 * constructor from unicode string
 * @update	gess 3/27/98
 * @param 
 * @return
 *------------------------------------------------------*/
nsString::nsString(const PRUnichar* aUnicodeStr){
  mLength=mCapacity=0;
  mStr=0;
  PRInt32 len=(aUnicodeStr) ? nsCRT::strlen(aUnicodeStr) : 0;
  EnsureCapacityFor(len);
  this->SetString(aUnicodeStr,len);
}

/*-------------------------------------------------------
 * special subclas constructor
 * this will optionally not allocate a buffer
 * but the subclass must
 * @update	psl 4/16/98
 * @param 
 * @return
 *------------------------------------------------------*/
nsString::nsString(PRBool aSubclassBuffer)
{
  mLength=mCapacity=0;
  mStr=0;
  if (PR_FALSE == aSubclassBuffer) {
    EnsureCapacityFor(1);
    this->SetString("");
  }
}

/*-------------------------------------------------------
 * standard destructor 
 * @update	gess 3/27/98
 * @param 
 * @return
 *------------------------------------------------------*/
nsString::~nsString()
{
  delete [] mStr;
  mStr=0;
  mCapacity=mLength=0;
}

/*-------------------------------------------------------
 * This method gets called when the internal buffer needs
 * to grow to a given size.
 * @update  gess 3/30/98
 * @param   aNewLength -- new capacity of string  
 * @return  void
 *------------------------------------------------------*/
void nsString::EnsureCapacityFor(PRInt32 aNewLength)
{
  PRInt32 newCapacity;

  if (mCapacity > 64) {
    // When the string starts getting large, double the capacity as we
    // grow.
    newCapacity = mCapacity * 2;
    if (newCapacity < aNewLength) {
      newCapacity = mCapacity + aNewLength;
    }
  } else {
    // When the string is small, keep it's capacity a multiple of
    // kGrowthDelta
    PRInt32 size =aNewLength+kGrowthDelta;
    newCapacity=size-(size % kGrowthDelta);
  }

  if(mCapacity<newCapacity) {
    mCapacity=newCapacity;
    chartype* temp = new chartype[newCapacity+1];
    if (mLength > 0) {
      nsCRT::memcpy(temp, mStr, mLength * sizeof(chartype));
    }
    if(mStr)
      delete [] mStr;
    mStr = temp;
    mStr[mLength]=0;
  }
}

/*-------------------------------------------------------
 * 
 * @update	gess 3/27/98
 * @param 
 * @return
 *------------------------------------------------------*/
void nsString::SetLength(PRInt32 aLength) {
//  NS_WARNING("Depricated method -- not longer required with dynamic strings. Use Truncate() instead.");
  EnsureCapacityFor(aLength);
  if (aLength > mLength) {
    nsCRT::zero(mStr + mLength, (aLength - mLength) * sizeof(chartype));
  }
  mLength=aLength;
}

/*-------------------------------------------------------
 * This method truncates this string to given length.
 *
 * @update	gess 3/27/98
 * @param   anIndex -- new length of string
 * @return  nada
 *------------------------------------------------------*/
void nsString::Truncate(PRInt32 anIndex) {
  if((anIndex>-1) && (anIndex<mLength)) {
    mLength=anIndex;
    mStr[mLength]=0;
  }
}



                //accessor methods

/*-------------------------------------------------------
 * Retrieve pointer to internal string value
 * @update	gess 3/27/98
 * @param 
 * @return  PRUnichar* to internal string
 *------------------------------------------------------*/
PRUnichar* nsString::GetUnicode(void) const{
  return mStr;
}


/*-------------------------------------------------------
 * Retrieve pointer to internal string value
 * @update	gess 3/27/98
 * @param 
 * @return  PRUnichar* to internal string
 *------------------------------------------------------*/
nsString::operator PRUnichar*() const{
  return mStr;
}


/*-------------------------------------------------------
 * Retrieve pointer to internal string value
 * @update	gess 3/27/98
 * @param 
 * @return  new char* copy of internal string
nsString::operator char*() const{
  return ToNewCString();
}
 *------------------------------------------------------*/


/*-------------------------------------------------------
 * Retrieve pointer to internal string value
 * @update	gess 3/27/98
 * @param 
 * @return  PRUnichar* to internal string
 *------------------------------------------------------*/
PRUnichar* nsString::operator()() const{
  return mStr;
}


/*-------------------------------------------------------
 * Retrieve pointer to internal string value
 * @update	gess 3/27/98
 * @param 
 * @return  PRUnichar* to internal string
 *------------------------------------------------------*/
PRUnichar nsString::operator()(int anIndex) const{
  NS_ASSERTION(anIndex<mLength,kOutOfBoundsError);
  if((anIndex<mLength) && (mStr))
    return mStr[anIndex];
  else return gBadChar;
}

/*-------------------------------------------------------
 * 
 * @update	gess 3/27/98
 * @param 
 * @return
 *------------------------------------------------------*/
PRUnichar& nsString::operator[](PRInt32 anIndex) const{
//  NS_ASSERTION(anIndex<mLength,kOutOfBoundsError);
  if((anIndex<mLength) && (mStr))
    return mStr[anIndex];
  else return gBadChar;
}

/*-------------------------------------------------------
 * 
 * @update	gess 3/27/98
 * @param 
 * @return
 *------------------------------------------------------*/
PRUnichar& nsString::CharAt(PRInt32 anIndex) const{
  NS_ASSERTION(anIndex<mLength,kOutOfBoundsError);
  if((anIndex<mLength) && (mStr))
    return mStr[anIndex];
  else return gBadChar;
}

/*-------------------------------------------------------
 * 
 * @update	gess 3/27/98
 * @param 
 * @return
 *------------------------------------------------------*/
PRUnichar& nsString::First() const{
  if((mLength) && (mStr))
    return mStr[0];
  else return gBadChar;
}

/*-------------------------------------------------------
 * 
 * @update	gess 3/27/98
 * @param 
 * @return
 *------------------------------------------------------*/
PRUnichar& nsString::Last() const{
  if((mLength) && (mStr))
    return mStr[mLength-1];
  else return gBadChar;
}

/*-------------------------------------------------------
 * Create a new string by appending given string to this
 * @update	gess 3/27/98
 * @param   aString -- 2nd string to be appended
 * @return
 *------------------------------------------------------*/
nsString nsString::operator+(const nsString& aString){
  nsString temp(*this);
  temp.Append(aString.mStr,aString.mLength);
  return temp;
}


/*-------------------------------------------------------
 * create a new string by adding this to the given buffer.
 * @update	gess 3/27/98
 * @param   aBuffer: buffer to be added to this
 * @return  newly created string
 *------------------------------------------------------*/
nsString nsString::operator+(const char* anISOLatin1) {
  nsString temp(*this);
  temp.Append(anISOLatin1);
  return temp;
}


/*-------------------------------------------------------
 * create a new string by adding this to the given char.
 * @update	gess 3/27/98
 * @param   aChar: char to be added to this
 * @return  newly created string
 *------------------------------------------------------*/
nsString nsString::operator+(char aChar) {
  nsString temp(*this);
  temp.Append(chartype(aChar));
  return temp;
}


/*-------------------------------------------------------
 * create a new string by adding this to the given buffer.
 * @update	gess 3/27/98
 * @param   aBuffer: buffer to be added to this
 * @return  newly created string
 *------------------------------------------------------*/
nsString nsString::operator+(const PRUnichar* aStr) {
  nsString temp(*this);
  temp.Append(aStr);
  return temp;
}


/*-------------------------------------------------------
 * create a new string by adding this to the given char.
 * @update	gess 3/27/98
 * @param   aChar: char to be added to this
 * @return  newly created string
 *------------------------------------------------------*/
nsString nsString::operator+(PRUnichar aChar) {
  nsString temp(*this);
  temp.Append(aChar);
  return temp;
}


/*-------------------------------------------------------
 * 
 * @update	gess 3/27/98
 * @param 
 * @return
 *------------------------------------------------------*/
void nsString::ToLowerCase()
{
  chartype* cp = mStr;
  chartype* end = cp + mLength;
  while (cp < end) {
    chartype ch = *cp;
    if ((ch >= 'A') && (ch <= 'Z')) {
      *cp = 'a' + (ch - 'A');
    }
    cp++;
  }
}

/*-------------------------------------------------------
 * 
 * @update	gess 3/27/98
 * @param 
 * @return
 *------------------------------------------------------*/
void nsString::ToUpperCase()
{
  chartype* cp = mStr;
  chartype* end = cp + mLength;
  while (cp < end) {
    chartype ch = *cp;
    if ((ch >= 'a') && (ch <= 'z')) {
      *cp = 'A' + (ch - 'a');
    }
    cp++;
  }
}

/*-------------------------------------------------------
 * 
 * @update	gess 3/27/98
 * @param 
 * @return
 *------------------------------------------------------*/
void nsString::ToLowerCase(nsString& aOut) const
{
  aOut.EnsureCapacityFor(mLength);
  aOut.mLength = mLength;
  chartype* to = aOut.mStr;
  chartype* from = mStr;
  chartype* end = from + mLength;
  while (from < end) {
    chartype ch = *from++;
    if ((ch >= 'A') && (ch <= 'Z')) {
      ch = 'a' + (ch - 'A');
    }
    *to++ = ch;
  }
  *to = 0;
}

/*-------------------------------------------------------
 * 
 * @update	gess 3/27/98
 * @param 
 * @return
 *------------------------------------------------------*/
void nsString::ToUpperCase(nsString& aOut) const
{
  aOut.EnsureCapacityFor(mLength);
  aOut.mLength = mLength;
  chartype* to = aOut.mStr;
  chartype* from = mStr;
  chartype* end = from + mLength;
  while (from < end) {
    chartype ch = *from++;
    if ((ch >= 'a') && (ch <= 'z')) {
      ch = 'A' + (ch - 'a');
    }
    *to++ = ch;
  }
  *to = 0;
}

/*-------------------------------------------------------
 * 
 * @update	gess 3/27/98
 * @param 
 * @return
 *------------------------------------------------------*/
nsString* nsString::ToNewString() const {
  return new nsString(mStr);
}

/*-------------------------------------------------------
 * 
 * @update	gess 3/27/98
 * @param 
 * @return
 *------------------------------------------------------*/
char* nsString::ToNewCString() const
{
  char* rv = new char[mLength + 1];
  return ToCString(rv,mLength+1);
}

/*-------------------------------------------------------
 * 
 * @update	gess 3/27/98
 * @param 
 * @return
 *------------------------------------------------------*/
PRUnichar* nsString::ToNewUnicode() const
{
  PRInt32 len = mLength;
  chartype* rv = new chartype[len + 1];
  chartype* to = rv;
  chartype* from = mStr;
  while (--len >= 0) {
    *to++ = *from++;
  }
  *to++ = 0;
  return rv;
}

/*-------------------------------------------------------
 * 
 * @update	gess 3/27/98
 * @param 
 * @return
 *------------------------------------------------------*/
void nsString::ToString(nsString& aString) const
{
  aString.mLength = 0;
  aString.Append(mStr, mLength);
}

/*-------------------------------------------------------
 * 
 * @update	gess 3/27/98
 * @param 
 * @return
 *------------------------------------------------------*/
char* nsString::ToCString(char* aBuf, PRInt32 aBufLength) const
{
  aBufLength--;                 // leave room for the \0
  PRInt32 len = mLength;
  if (len > aBufLength) {
    len = aBufLength;
  }
  char* to = aBuf;
  chartype* from = mStr;
  while (--len >= 0) {
    *to++ = char(*from++);
  }
  *to++ = '\0';
  return aBuf;
}

/*-------------------------------------------------------
 * 
 * @update	gess 3/27/98
 * @param 
 * @return
 *------------------------------------------------------*/
float nsString::ToFloat(PRInt32* aErrorCode) const
{
  char buf[40];
  if (mLength > sizeof(buf)-1) {
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

/*-------------------------------------------------------
 * 
 * @update	gess 3/27/98
 * @param 
 * @return
 *------------------------------------------------------*/
PRInt32 nsString::ToInteger(PRInt32* aErrorCode) const {
  PRInt32 rv = 0;
  PRUnichar* cp = mStr;
  PRUnichar* end = mStr + mLength;

  // Skip leading whitespace
  while (cp < end) {
    PRUnichar ch = *cp;
    if (!IsSpace(ch)) {
      break;
    }
    cp++;
  }
  if (cp == end) {
    *aErrorCode = (PRInt32) NS_ERROR_ILLEGAL_VALUE;
    return rv;
  }

  // Check for sign
  PRUnichar sign = '+';
  if ((*cp == '+') || (*cp == '-')) {
    sign = *cp++;
  }
  if (cp == end) {
    *aErrorCode = (PRInt32) NS_ERROR_ILLEGAL_VALUE;
    return rv;
  }

  // Convert the number
  while (cp < end) {
    PRUnichar ch = *cp++;
    if ((ch < '0') || (ch > '9')) {
      *aErrorCode = (PRInt32) NS_ERROR_ILLEGAL_VALUE;
      break;
    }
    rv = rv * 10 + (ch - '0');
  }

  if (sign == '-') {
    rv = -rv;
  }
  *aErrorCode = NS_OK;
  return rv;
}


/*-------------------------------------------------------
 * assign given string to this one
 * @update	gess 3/27/98
 * @param   aString: string to be added to this
 * @return  this
 *------------------------------------------------------*/
nsString& nsString::operator=(const nsString& aString) {
  return this->SetString(aString.mStr,aString.mLength);
}


/*-------------------------------------------------------
 * assign given char* to this string
 * @update	gess 3/27/98
 * @param   anISOLatin1: buffer to be assigned to this 
 * @return  this
 *------------------------------------------------------*/
nsString& nsString::operator=(const char* anISOLatin1) {
  return SetString(anISOLatin1);
}


/*-------------------------------------------------------
 * assign given char to this string
 * @update	gess 3/27/98
 * @param   aChar: char to be assignd to this
 * @return  this
 *------------------------------------------------------*/
nsString& nsString::operator=(char aChar) {
  return Append(PRUnichar(aChar));
}

/*-------------------------------------------------------
 * assign given PRUnichar* to this string
 * @update	gess 3/27/98
 * @param   anISOLatin1: buffer to be assigned to this 
 * @return  this
 *------------------------------------------------------*/
nsString& nsString::SetString(const PRUnichar* aStr,PRInt32 aLength) {
  if(aStr!=0) {
    PRInt32 len=(kNotFound==aLength) ? nsCRT::strlen(aStr) : aLength;
    if(mCapacity< len ) 
      EnsureCapacityFor(len);
    nsCRT::memcpy(mStr,aStr,len*sizeof(chartype));
    mLength=len;
    mStr[mLength]=0;
  }
  else {                          
    mLength=0;  //This little bit of code handles the case                  
    mStr[0]=0;  //where some blockhead hands us a null string
  }
  return *this;
}


/*-------------------------------------------------------
 * assign given char* to this string
 * @update	gess 3/27/98
 * @param   anISOLatin1: buffer to be assigned to this 
 * @return  this
 *------------------------------------------------------*/
nsString& nsString::SetString(const char* anISOLatin1,PRInt32 aLength) {
  if(anISOLatin1!=0) {
    PRInt32 len=(kNotFound==aLength) ? nsCRT::strlen(anISOLatin1) : aLength; 
    if(mCapacity<len) 
      EnsureCapacityFor(len);
    for(int i=0;i<len;i++) {
      mStr[i]=chartype(anISOLatin1[i]);
    }
    mLength=len;
    mStr[mLength]=0;
  }
  else {                          
    mLength=0;  //This little bit of code handles the case                  
    mStr[0]=0;  //where some blockhead hands us a null string
  }
  return *this;
}

/*-------------------------------------------------------
 * assign given char* to this string
 * @update	gess 3/27/98
 * @param   anISOLatin1: buffer to be assigned to this 
 * @return  this
 *------------------------------------------------------*/
nsString& nsString::operator=(const PRUnichar* aStr) {
  return this->SetString(aStr);
}


/*-------------------------------------------------------
 * assign given char to this string
 * @update	gess 3/27/98
 * @param   aChar: char to be assignd to this
 * @return  this
 *------------------------------------------------------*/
nsString& nsString::operator=(PRUnichar aChar) {
  mLength=1;
  if(mCapacity<1) 
    EnsureCapacityFor(kGrowthDelta);
  mStr[0]=aChar;
  mStr[1]=0;
  return *this;
}

/*-------------------------------------------------------
 * append given string to this string
 * @update	gess 3/27/98
 * @param   aString : string to be appended to this
 * @return  this
 *------------------------------------------------------*/
nsString& nsString::Append(const nsString& aString,PRInt32 aLength) {
  return Append(aString.mStr,aString.mLength);
}

/*-------------------------------------------------------
 * append given string to this string
 * @update	gess 3/27/98
 * @param   aString : string to be appended to this
 * @return  this
 *------------------------------------------------------*/
nsString& nsString::Append(const char* anISOLatin1,PRInt32 aLength) {
  if(anISOLatin1!=0) {
    PRInt32 len=(kNotFound==aLength) ? strlen(anISOLatin1) : aLength;
    if(mLength+len > mCapacity) {
      EnsureCapacityFor(mLength+len);
    }
    for(int i=0;i<len;i++) {
      mStr[mLength+i]=chartype(anISOLatin1[i]);
    }
    mLength+=len;
    mStr[mLength]=0;
  }
  return *this;
}

/*-------------------------------------------------------
 * append given string to this string
 * @update	gess 3/27/98
 * @param   aString : string to be appended to this
 * @return  this
 *------------------------------------------------------*/
nsString& nsString::Append(char aChar) {
  return Append(PRUnichar(aChar));
}

/*-------------------------------------------------------
 * append given string to this string
 * @update	gess 3/27/98
 * @param   aString : string to be appended to this
 * @return  this
 *------------------------------------------------------*/
nsString& nsString::Append(const PRUnichar* aString,PRInt32 aLength) {
  if(aString!=0) {
    PRInt32 len=(kNotFound==aLength) ? nsCRT::strlen(aString) : aLength;
    if(mLength+len > mCapacity) {
      EnsureCapacityFor(mLength+len);
    }
    if(len>0)
      nsCRT::memcpy(&mStr[mLength],aString,len*sizeof(chartype));
    mLength+=len;
    mStr[mLength]=0;
  }
  return *this;
}

/*-------------------------------------------------------
 * append given string to this string
 * @update	gess 3/27/98
 * @param   aString : string to be appended to this
 * @return  this
 *------------------------------------------------------*/
nsString& nsString::Append(PRUnichar aChar) {
  if(mLength < mCapacity) {
    mStr[mLength++]=aChar;             // the new string len < capacity, so just copy
    mStr[mLength]=0;
  }
  else {                            // The new string exceeds our capacity
    EnsureCapacityFor(mLength+1);
    mStr[mLength++]=aChar;
    mStr[mLength]=0;
  }
  return *this;
}


/*-------------------------------------------------------
 * append given string to this string
 * @update	gess 3/27/98
 * @param   aString : string to be appended to this
 * @return  this
 *------------------------------------------------------*/
nsString& nsString::operator+=(const nsString &aString) {  
  return this->Append(aString.mStr,aString.mLength);
}


/*-------------------------------------------------------
 * append given buffer to this string
 * @update	gess 3/27/98
 * @param   anISOLatin1: buffer to be appended to this
 * @return  this
 *------------------------------------------------------*/
nsString& nsString::operator+=(const char* anISOLatin1) {
  return Append(anISOLatin1);
}


/*-------------------------------------------------------
 * append given buffer to this string
 * @update	gess 3/27/98
 * @param   aBuffer: buffer to be appended to this
 * @return  this
 *------------------------------------------------------*/
nsString& nsString::operator+=(const PRUnichar* aBuffer) {
  return Append(aBuffer);
}


/*-------------------------------------------------------
 * append given char to this string
 * @update	gess 3/27/98
 * @param   aChar: char to be appended to this
 * @return  this
 *------------------------------------------------------*/
nsString& nsString::operator+=(PRUnichar aChar) {
  return Append(aChar);
}

/*-------------------------------------------------------
 * 
 * @update	gess 3/27/98
 * @param 
 * @return
 *------------------------------------------------------*/
nsString& nsString::Append(PRInt32 aInteger,PRInt32 aRadix) {
  char* fmt = "%d";
  if (8 == aRadix) {
    fmt = "%o";
  } else if (16 == aRadix) {
    fmt = "%x";
  }
  char buf[40];
  PR_snprintf(buf, sizeof(buf), fmt, aInteger);
  Append(buf);
  return *this;
}


/*-------------------------------------------------------
 * 
 * @update	gess 3/27/98
 * @param 
 * @return
 *------------------------------------------------------*/
nsString& nsString::Append(float aFloat){
  char buf[40];
  PR_snprintf(buf, sizeof(buf), "%g", aFloat);
  Append(buf);
  return *this;
}



/**-------------------------------------------------------
 *  Copies n characters from this string to given string,
 *  starting at the leftmost offset.
 *  
 *  
 *  @update  gess 4/1/98
 *  @param   aCopy -- Receiving string
 *  @param   aCount -- number of chars to copy
 *  @return  number of chars copied
 *------------------------------------------------------*/
PRInt32 nsString::Left(nsString& aCopy,PRInt32 aCount) {
  return Mid(aCopy,0,aCount);
}

/**-------------------------------------------------------
 *  Copies n characters from this string to given string,
 *  starting at the given offset.
 *  
 *  
 *  @update  gess 4/1/98
 *  @param   aCopy -- Receiving string
 *  @param   aCount -- number of chars to copy
 *  @param   anOffset -- position where copying begins
 *  @return  number of chars copied
 *------------------------------------------------------*/
PRInt32 nsString::Mid(nsString& aCopy,PRInt32 anOffset,PRInt32 aCount) {
  if(anOffset<mLength) {
    aCount=(anOffset+aCount<=mLength) ? aCount : mLength-anOffset;

    PRUnichar* from = mStr + anOffset;
    PRUnichar* end = mStr + anOffset + aCount;

    while (from < end) {
      PRUnichar ch = *from;
      aCopy.Append(ch);
      from++;
    }
  }
  else aCount=0;
  return aCount;
}

/**-------------------------------------------------------
 *  Copies n characters from this string to given string,
 *  starting at rightmost char.
 *  
 *  
 *  @update gess 4/1/98
 *  @param  aCopy -- Receiving string
 *  @param  aCount -- number of chars to copy
 *  @return number of chars copied
 *------------------------------------------------------*/
PRInt32 nsString::Right(nsString& aCopy,PRInt32 aCount) {
  PRInt32 offset=(mLength-aCount<0) ? 0 : mLength-aCount;
  return Mid(aCopy,offset,aCount);
}

/**-------------------------------------------------------
 *  This method inserts n chars from given string into this
 *  string at str[anOffset].
 *  
 *  @update gess 4/1/98
 *  @param  aCopy -- String to be inserted into this
 *  @param  anOffset -- insertion position within this str
 *  @param  aCount -- number of chars to be copied from aCopy
 *  @return number of chars inserted into this.
 *------------------------------------------------------*/
PRInt32 nsString::Insert(nsString& aCopy,PRInt32 anOffset,PRInt32 aCount) {
  aCount=(aCount>aCopy.mLength) ? aCopy.mLength : aCount; //don't try to copy more than you are given
  if(aCount>0) {

    //1st optimization: If you're inserting at end, then simply append!
    if(anOffset>=mLength){
      Append(aCopy,aCopy.mLength);
      return aCopy.mLength;
    }

    if(mLength+aCount > mCapacity) {
      EnsureCapacityFor(mLength+aCount);
    }

    PRUnichar* last = mStr + mLength;
    PRUnichar* first = mStr + anOffset-1;
    PRUnichar* next = mStr + mLength + aCount;
  
    //Copy rightmost chars, up to offset+aCount...
    while(first<last) {
      char ch1=char(*last);
      char ch2=char(*next);
      *next=*last;  
      next--;
      last--;
    }

    //now insert new chars, starting at offset
    next = last;
    first = aCopy.mStr - 1;
    last = aCopy.mStr + aCount;

    while (++first<last) {
      *(++next)=*first;
    }
    mLength+=aCount;
  }
  return aCount;
}


/**
 * Insert a single unicode char into this string at
 * a specified offset.
 *
 * @update	gess4/22/98
 * @param   aChar char to be inserted into this string
 * @param   anOffset is insert pos in str 
 * @return  the number of chars inserted into this string
 */
PRInt32 nsString::Insert(PRUnichar aChar,PRInt32 anOffset){

  //1st optimization: If you're inserting at end, then simply append!
  if(anOffset<mLength){

    if(mLength+1> mCapacity) {
      EnsureCapacityFor(mLength+1);
    }

    PRUnichar* last = mStr + mLength;
    PRUnichar* first = mStr + anOffset-1;
    PRUnichar* next = mStr + mLength + 1;

    //Copy rightmost chars, up to offset+aCount...
    while(first<last) {
      char ch1=char(*last);
      char ch2=char(*next);
      *next=*last;  
      next--;
      last--;
    }

    //now insert new chars, starting at offset
    mStr[anOffset]=aChar;
    mLength+=1;
  }
  else Append(aChar);

  return 1;
}

/**-------------------------------------------------------
 *  This method is used to cut characters in this string
 *  starting at anOffset, continuing for aCount chars.
 *  
 *  @update gess 3/26/98
 *  @param  anOffset -- start pos for cut operation
 *  @param  aCount -- number of chars to be cut
 *  @return *this
 *------------------------------------------------------*/
nsString&
nsString::Cut(PRInt32 anOffset, PRInt32 aCount)
{
  if (PRUint32(anOffset) < PRUint32(mLength)) {
    PRInt32 spos=anOffset+aCount;
    PRInt32 delcnt=(spos<mLength) ? aCount : mLength-anOffset;
    if (spos < mLength) {
      nsCRT::memmove(&mStr[anOffset], &mStr[spos],
                     sizeof(chartype) * (mLength - spos));
    }
    mLength -= delcnt;
    mStr[mLength] = 0;          // restore zero terminator
  }
  return *this;
}

/**-------------------------------------------------------
 *  This method is used to remove all occurances of the
 *  characters found in aSet from this string.
 *  
 *  @update gess 3/26/98
 *  @param  aSet -- characters to be cut from this
 *  @return *this 
 *------------------------------------------------------*/
nsString& nsString::StripChars(const char* aSet){
  PRUnichar*  from = mStr;
  PRUnichar*  end = mStr + mLength;
  PRUnichar*  to = mStr;

  while (from < end) {
    PRUnichar ch = *from;
    if(0==strchr(aSet,char(ch))) {
      *to++=*from;
    }
    from++;
  }
  *to = '\0';
  mLength = to - mStr;
  return *this;
}

/**-------------------------------------------------------
 *  
 *  
 *  @update  gess 3/31/98
 *  @param   
 *  @return  
 *------------------------------------------------------*/
PRBool nsString::IsAlpha(PRUnichar ch) {
  // XXX i18n
  if (((ch >= 'A') && (ch <= 'Z')) || ((ch >= 'a') && (ch <= 'z'))) {
    return PR_TRUE;
  }
  return PR_FALSE;
}

/**-------------------------------------------------------
 *  
 *  
 *  @update  gess 3/31/98
 *  @param   
 *  @return  
 *------------------------------------------------------*/
PRBool nsString::IsSpace(PRUnichar ch) {
  // XXX i18n
  if ((ch == ' ') || (ch == '\r') || (ch == '\n') || (ch == '\t')) {
    return PR_TRUE;
  }
  return PR_FALSE;
}

/**-------------------------------------------------------
 *  
 *  
 *  @update  gess 3/31/98
 *  @param   
 *  @return  isalpha
 *------------------------------------------------------*/
PRBool nsString::IsDigit(PRUnichar ch) {
  // XXX i18n
  return PRBool((ch >= '0') && (ch <= '9'));
}

/**-------------------------------------------------------
 *  This method trims characters found in aTrimSet from
 *  either end of the underlying string.
 *  
 *  @update  gess 3/31/98
 *  @param   aTrimSet -- contains chars to be trimmed from
 *           both ends
 *  @return  this
 *------------------------------------------------------*/
nsString& nsString::Trim(const char* aTrimSet,
                               PRBool aEliminateLeading,
                               PRBool aEliminateTrailing)
{
  PRUnichar* from = mStr;
  PRUnichar* end = mStr + mLength-1;
  PRUnichar* to = mStr;

    //begin by find the first char not in aTrimSet
  if(aEliminateLeading) {
    while (from < end) {
      PRUnichar ch = *from;
      if(!strchr(aTrimSet,char(ch))) {
        break;
      }
      from++;
    }
  }

    //Now, find last char not in aTrimSet
  if(aEliminateTrailing) {
    while(end> from) {
      PRUnichar ch = *end;
      if(!strchr(aTrimSet,char(ch))) {
        break;
      }
      end--;
    }
  }

    //now rewrite your string without unwanted 
    //leading or trailing characters.
  while (from <= end) {
    *to++ = *from++;
  }

  *to = '\0';
  mLength = to - mStr;
  return *this;
}

/**-------------------------------------------------------
 *  
 *  
 *  @update  gess 3/31/98
 *  @param   
 *  @return  
 *------------------------------------------------------*/
nsString& nsString::CompressWhitespace( PRBool aEliminateLeading,
                                              PRBool aEliminateTrailing)
{
  PRUnichar* from = mStr;
  PRUnichar* end = mStr + mLength;
  PRUnichar* to = from;

  Trim(" \r\n\t",aEliminateLeading,aEliminateTrailing);

    //this code converts /n, /t, /r into normal space ' ';
    //it also eliminates runs of whitespace...
  while (from < end) {
    PRUnichar ch = *from++;
    if (IsSpace(ch)) {
      *to++ = ' ';
      while (from < end) {
        ch = *from++;
        if (!IsSpace(ch)) {
          *to++ = ch;
          break;
        }
      }
    } else {
      *to++ = ch;
    }
  }

  *to = '\0';
  mLength = to - mStr;
  return *this;
}

/**-------------------------------------------------------
 *  XXX This is used by bina all over the place; not sure
 *  it belongs here though
 *  
 *  @update  gess 3/31/98
 *  @param   
 *  @return  
 *------------------------------------------------------*/
nsString& nsString::StripWhitespace()
{
  Trim(" \r\n\t");
  return StripChars("\r\t\n");
}


/**-------------------------------------------------------
 *  Search for given buffer within this string
 *  
 *  @update  gess 3/25/98
 *  @param   anISOLatin1Buf - charstr to be found
 *  @return  offset in string, or -1 (kNotFound)
 *------------------------------------------------------*/
PRInt32 nsString::Find(const char* anISOLatin1Buf) const{
  NS_ASSERTION(0!=anISOLatin1Buf,kNullPointerError);
  PRInt32 result=-1;
  if(anISOLatin1Buf) {
    PRInt32 len=strlen(anISOLatin1Buf);
    if(len<=mLength) { //only enter if abuffer length is <= mStr length.
      for(PRInt32 offset=0;offset<mLength-len;offset++) 
        if(0==nsCRT::strncmp(&mStr[offset],anISOLatin1Buf,len)) 
          return offset;  //in this case, 0 means they match
    }
  }
  return result;
}

/**-------------------------------------------------------
 *  Search for given buffer within this string
 *  
 *  @update  gess 3/25/98
 *  @param   aString - PUnichar* to be found
 *  @return  offset in string, or -1 (kNotFound)
 *------------------------------------------------------*/
PRInt32 nsString::Find(const PRUnichar* aString) const{
  NS_ASSERTION(0!=aString,kNullPointerError);
  PRInt32 result=-1;
  if(aString) {
    PRInt32 len=nsCRT::strlen(aString);
    if(len<=mLength) { //only enter if abuffer length is <= mStr length.
      for(PRInt32 offset=0;offset<mLength-len;offset++) 
        if(0==nsCRT::strncmp(&mStr[offset],aString,len)) 
          return offset;  //in this case, 0 means they match
    }
  }
  return result;
}


/**-------------------------------------------------------
 *  Search for given buffer within this string
 *  
 *  @update  gess 3/25/98
 *  @param   nsString -- buffer to be found
 *  @return  offset in string, or -1 (kNotFound)
 *------------------------------------------------------*/
PRInt32 nsString::Find(const nsString& aString) const{
  PRInt32 result=-1;

  PRInt32 len=aString.mLength;
  if(len<=mLength) { //only enter if abuffer length is <= mStr length.
    for(PRInt32 offset=0;offset<mLength-len;offset++) {
      if(0==nsCRT::strncmp(&mStr[offset],aString.mStr,len)) {
        return offset;  //in this case, 0 means they match
      }
    }
  }
  return result;
}


/**-------------------------------------------------------
 *  Search for a given char, starting at given offset
 *  
 *  @update  gess 3/25/98
 *  @param   
 *  @return  offset of found char, or -1 (kNotFound)
 *------------------------------------------------------*/
PRInt32 nsString::Find(PRUnichar aChar, PRInt32 anOffset) const{
  PRInt32 result=0;

  for(PRInt32 i=anOffset;i<mLength;i++)
    if(aChar==mStr[i])
      return i;
  return kNotFound;
}


/**-------------------------------------------------------
 *  
 *  
 *  @update  gess 3/25/98
 *  @param   
 *  @return  
 *------------------------------------------------------*/
PRInt32 nsString::FindFirstCharInSet(const char* anISOLatin1Set,PRInt32 anOffset) const{
  NS_ASSERTION(0!=anISOLatin1Set,kNullPointerError);
  if(anISOLatin1Set && (strlen(anISOLatin1Set))) {
    for(PRInt32 i=anOffset;i<mLength;i++){
      char* pos=strchr(anISOLatin1Set,char(mStr[i]));
      if(pos)
        return i;
    }
  }
  return kNotFound;
}

/**-------------------------------------------------------
 *  
 *  
 *  @update  gess 3/25/98
 *  @param   
 *  @return  
 *------------------------------------------------------*/
PRInt32 nsString::FindFirstCharInSet(nsString& aSet,PRInt32 anOffset) const{
  if(aSet.Length()) {
    for(PRInt32 i=anOffset;i<mLength;i++){
      PRInt32 pos=aSet.Find(mStr[i]);
      if(kNotFound!=pos)
        return i;
    }
  }
  return kNotFound;
}

/**-------------------------------------------------------
 *  
 *  
 *  @update  gess 3/25/98
 *  @param   
 *  @return   
 *------------------------------------------------------*/
PRInt32 nsString::FindLastCharInSet(const char* anISOLatin1Set,PRInt32 anOffset) const{
  NS_ASSERTION(0!=anISOLatin1Set,kNullPointerError);
  if(anISOLatin1Set && strlen(anISOLatin1Set)) {
    for(PRInt32 i=mLength-1;i>0;i--){
      char* pos=strchr(anISOLatin1Set,char(mStr[i]));
      if(pos) 
        return i;
    }
  }
  return kNotFound;
}

/**-------------------------------------------------------
 *  
 *  
 *  @update  gess 3/25/98
 *  @param   
 *  @return   
 *------------------------------------------------------*/
PRInt32 nsString::FindLastCharInSet(nsString& aSet,PRInt32 anOffset) const{
  if(aSet.Length()) {
    for(PRInt32 i=mLength-1;i>0;i--){
      PRInt32 pos=aSet.Find(mStr[i]);
      if(kNotFound!=pos) 
        return i;
    }
  }
  return kNotFound;
}

/**-------------------------------------------------------
 *  
 *  
 *  @update  gess 3/25/98
 *  @param   
 *  @return  
 *------------------------------------------------------*/
PRInt32 nsString::RFind(const PRUnichar* aString,PRBool aIgnoreCase) const{
  NS_ASSERTION(0!=aString,kNullPointerError);

  if(aString) {
    PRInt32 len=nsCRT::strlen(aString);
    if((len) && (len<=mLength)) { //only enter if abuffer length is <= mStr length.
      for(PRInt32 offset=mLength-len;offset>=0;offset--) {
        PRInt32 result=0;
        if(aIgnoreCase) 
          result=nsCRT::strncasecmp(&mStr[offset],aString,len);
        else result=nsCRT::strncmp(&mStr[offset],aString,len);
        if(0==result)
          return offset;  //in this case, 0 means they match
      }
    }
  }
  return kNotFound;
}

/**-------------------------------------------------------
 *  
 *  
 *  @update  gess 3/25/98
 *  @param   
 *  @return  
 *------------------------------------------------------*/
PRInt32 nsString::RFind(const nsString& aString,PRBool aIgnoreCase) const{
  PRInt32 len=aString.mLength;
  if((len) && (len<=mLength)) { //only enter if abuffer length is <= mStr length.
    for(PRInt32 offset=mLength-len;offset>=0;offset--) {
      PRInt32 result=0;
      if(aIgnoreCase) 
        result=nsCRT::strncasecmp(&mStr[offset],aString.mStr,len);
      else result=nsCRT::strncmp(&mStr[offset],aString.mStr,len);
      if(0==result)
        return offset;  //in this case, 0 means they match
    }
  }
  return kNotFound;
}

/**-------------------------------------------------------
 *  
 *  
 *  @update  gess 3/25/98
 *  @param   
 *  @return  
 *------------------------------------------------------*/
PRInt32 nsString::RFind(const char* anISOLatin1Set,PRBool aIgnoreCase) const{
  NS_ASSERTION(0!=anISOLatin1Set,kNullPointerError);

  if(anISOLatin1Set) {
    PRInt32 len=strlen(anISOLatin1Set);
    if((len) && (len<=mLength)) { //only enter if abuffer length is <= mStr length.
      for(PRInt32 offset=mLength-len;offset>=0;offset--) {
        PRInt32 result=0;
        if(aIgnoreCase) 
          result=nsCRT::strncasecmp(&mStr[offset],anISOLatin1Set,len);
        else result=nsCRT::strncmp(&mStr[offset],anISOLatin1Set,len);
        if(0==result)
          return offset;  //in this case, 0 means they match
      }
    }
  }
  return kNotFound;
}


/**-------------------------------------------------------
 *  Scans this string backwards for first occurance of
 *  the given char.
 *  
 *  @update  gess 3/25/98
 *  @param   
 *  @return  offset of char in string, or -1 (kNotFound)
 *------------------------------------------------------*/
PRInt32 nsString::RFind(PRUnichar aChar,PRBool aIgnoreCase) const{
  chartype uc=nsCRT::ToUpper(aChar);
  for(PRInt32 offset=mLength-1;offset>0;offset--) 
    if(aIgnoreCase) {
      if(nsCRT::ToUpper(mStr[offset])==uc)
        return offset;
    }
    else if(mStr[offset]==aChar)
      return offset;  //in this case, 0 means they match
  return kNotFound;

}

 //****** comparision methods... *******

/*-------------------------------------------------------
 * 
 * @update	gess 3/27/98
 * @param 
 * @return
 *------------------------------------------------------*/
PRInt32 nsString::Compare(const char *anISOLatin1,PRBool aIgnoreCase) const {
  if (aIgnoreCase) {
    return nsCRT::strcasecmp(mStr,anISOLatin1);
  }
  return nsCRT::strcmp(mStr,anISOLatin1);
}

/*-------------------------------------------------------
 * LAST MODS:	gess
 * 
 * @param  
 * @return 
 *------------------------------------------------------*/
PRInt32 nsString::Compare(const nsString &S,PRBool aIgnoreCase) const {
  if (aIgnoreCase) {
    return nsCRT::strcasecmp(mStr,S.mStr);
  }
  return nsCRT::strcmp(mStr,S.mStr);
}

/*-------------------------------------------------------
 * 
 * @update	gess 3/27/98
 * @param 
 * @return
 *------------------------------------------------------*/
PRInt32 nsString::Compare(const PRUnichar* aString,PRBool aIgnoreCase) const {
  if (aIgnoreCase) {
    return nsCRT::strcasecmp(mStr,aString);
  }
  return nsCRT::strcmp(mStr,aString);
}


PRInt32 nsString::operator==(const nsString &S) const {return Compare(S)==0;}      
PRInt32 nsString::operator==(const char *s) const {return Compare(s)==0;}
PRInt32 nsString::operator==(const PRUnichar *s) const {return Compare(s)==0;}
PRInt32 nsString::operator!=(const nsString &S) const {return Compare(S)!=0;}
PRInt32 nsString::operator!=(const char *s ) const {return Compare(s)!=0;}
PRInt32 nsString::operator!=(const PRUnichar *s) const {return Compare(s)==0;}
PRInt32 nsString::operator<(const nsString &S) const {return Compare(S)<0;}
PRInt32 nsString::operator<(const char *s) const {return Compare(s)<0;}
PRInt32 nsString::operator<(const PRUnichar *s) const {return Compare(s)==0;}
PRInt32 nsString::operator>(const nsString &S) const {return Compare(S)>0;}
PRInt32 nsString::operator>(const char *s) const {return Compare(s)>0;}
PRInt32 nsString::operator>(const PRUnichar *s) const {return Compare(s)==0;}
PRInt32 nsString::operator<=(const nsString &S) const {return Compare(S)<=0;}
PRInt32 nsString::operator<=(const char *s) const {return Compare(s)<=0;}
PRInt32 nsString::operator<=(const PRUnichar *s) const {return Compare(s)==0;}
PRInt32 nsString::operator>=(const nsString &S) const {return Compare(S)>=0;}
PRInt32 nsString::operator>=(const char *s) const {return Compare(s)>=0;}
PRInt32 nsString::operator>=(const PRUnichar *s) const {return Compare(s)==0;}



/*-------------------------------------------------------
 * Compare this to given string
 * @update gess 3/27/98
 * @param  aString -- string to compare to this
 * @return TRUE if equal
 *------------------------------------------------------*/
PRBool nsString::Equals(const nsString& aString) const {
  PRInt32 result=nsCRT::strcmp(mStr,aString.mStr);
  return PRBool(0==result);
}


/*-------------------------------------------------------
 * Compare this to given string
 * @update gess 3/27/98
 * @param  aCString -- Cstr to compare to this
 * @return TRUE if equal
 *------------------------------------------------------*/
PRBool nsString::Equals(const char* aCString) const{
  NS_ASSERTION(0!=aCString,kNullPointerError);
  PRInt32 result=nsCRT::strcmp(mStr,aCString);
  return PRBool(0==result);
}


/*-------------------------------------------------------
 * Compare this to given atom
 * @update gess 3/27/98
 * @param  aAtom -- atom to compare to this
 * @return TRUE if equal
 *------------------------------------------------------*/
PRBool nsString::Equals(const nsIAtom* aAtom) const
{
  NS_ASSERTION(0!=aAtom,kNullPointerError);
  PRInt32 result=nsCRT::strcmp(mStr,aAtom->GetUnicode());
  return PRBool(0==result);
}


/*-------------------------------------------------------
 * Compare given strings
 * @update gess 3/27/98
 * @param  s1 -- first string to be compared
 * @param  s2 -- second string to be compared
 * @return TRUE if equal
 *------------------------------------------------------*/
PRBool nsString::Equals(const PRUnichar* s1, const PRUnichar* s2) const {
  NS_ASSERTION(0!=s1,kNullPointerError);
  NS_ASSERTION(0!=s2,kNullPointerError);
  PRBool  result=PR_FALSE;
  if((s1) && (s2)){
    PRInt32 cmp=nsCRT::strcmp(s1,s2);
    result=PRBool(0==cmp);
  }
  return result;
}


/*-------------------------------------------------------
 * 
 * @update	gess 3/27/98
 * @param 
 * @return
 *------------------------------------------------------*/
PRBool nsString::EqualsIgnoreCase(const nsString& aString) const{
  PRInt32 result=nsCRT::strcasecmp(mStr,aString.mStr);
  return PRBool(0==result);
}


/*-------------------------------------------------------
 * 
 * @update	gess 3/27/98
 * @param 
 * @return
 *------------------------------------------------------*/
PRBool nsString::EqualsIgnoreCase(const nsIAtom *aAtom) const{
  NS_ASSERTION(0!=aAtom,kNullPointerError);
  PRBool result=PR_FALSE;
  if(aAtom){
    PRInt32 cmp=nsCRT::strcasecmp(mStr,aAtom->GetUnicode());
    result=PRBool(0==cmp);
  }
  return result;
}



/*-------------------------------------------------------
 * 
 * @update	gess 3/27/98
 * @param 
 * @return
 *------------------------------------------------------*/
PRBool nsString::EqualsIgnoreCase(const PRUnichar* s1, const PRUnichar* s2) const {
  NS_ASSERTION(0!=s1,kNullPointerError);
  NS_ASSERTION(0!=s2,kNullPointerError);
  PRBool  result=PR_FALSE;
  if((s1) && (s2)){
    PRInt32 cmp=nsCRT::strcasecmp(s1,s2);
    result=PRBool(0==cmp);
  }
  return result;
}


/*-------------------------------------------------------
 * 
 * @update	gess 3/27/98
 * @param 
 * @return
 *------------------------------------------------------*/
PRBool nsString::EqualsIgnoreCase(const char* aCString) const {
  NS_ASSERTION(0!=aCString,kNullPointerError);
  PRBool  result=PR_FALSE;
  if(aCString){
    PRInt32 cmp=nsCRT::strcasecmp(mStr,aCString);
    result=PRBool(0==cmp);
  }
  return result;
}


/*-------------------------------------------------------
 * 
 * @update	gess 3/27/98
 * @param 
 * @return
 *------------------------------------------------------*/
void nsString::DebugDump(ostream& aStream) const {
  for(int i=0;i<mLength;i++) {
    aStream <<char(mStr[i]);
  }
  aStream << endl;
}

/*-------------------------------------------------------
 * 
 * @update	gess 3/27/98
 * @param 
 * @return
 *------------------------------------------------------*/
void nsString::SelfTest(void) {
  
#if 0

  nsAutoString as("Hello there");
  as.SelfTest();

  static const char* temp="hello";

  //first, let's test the constructors...
  nsString a(temp);
  nsString* a_=new nsString(a);  //test copy constructor
  nsString b("world!");

  //verify destructor...
  delete a_;
  a_=0;

  //Let's verify the Length() method...
  NS_ASSERTION(5==a.Length(),"Error: constructor probably bad!");

  //**********************************************
  //Let's check out the ACCESSORS...
  //**********************************************

  chartype* p1=a.GetUnicode();
  chartype* p2=a; //should invoke the PRUnichar conversion operator...
  for(int i=0;i<a.Length();i++) {
    NS_ASSERTION(a[i]==temp[i],"constructor error!");         //test [] operator
    NS_ASSERTION(a.CharAt(i)==temp[i],"constructor error!");  //test charAt method
    NS_ASSERTION(a(i)==temp[i],"constructor error!");         //test (i) operator
  }
  NS_ASSERTION(a.First()==temp[0],"constructor error!");
  NS_ASSERTION(a.Last()==temp[a.Length()-1],"constructor error!");

  //**********************************************
  //Now let's test the CREATION operators...
  //**********************************************

  static const char* temp1="helloworld!";
  nsString temp2=a+b;
  nsString temp3=a+"world!";
  nsString temp4=temp2+'!';

    //let's quick check the PRUnichar operator+ method...
  PRUnichar* uc=temp4.GetUnicode();
  nsString temp4a("Begin");
  temp4a.DebugDump(cout);
  nsString temp4b=temp4a+uc;
  temp4b.DebugDump(cout);

  temp2.DebugDump(cout);
  temp3.DebugDump(cout);
  temp4.DebugDump(cout);
  for(i=0;i<temp2.Length();i++) {
    NS_ASSERTION(temp1[i]==temp2[i],"constructor error!");         
    NS_ASSERTION(temp1[i]==temp3[i],"constructor error!");           
    NS_ASSERTION(temp1[i]==temp4[i],"constructor error!");           
  }
  NS_ASSERTION(temp4.Last()=='!',"constructor error!");
  NS_ASSERTION(temp4.Length()>temp3.Length(),"constructor error!"); //should be char longer

  nsString* es1=temp2.ToNewString(); //this should make us a new string
  char* es2=temp2.ToNewCString();
  for(i=0;i<temp2.Length();i++) {
    NS_ASSERTION(es2[i]==(*es1)[i],"Creation error!");         
  }

  nsString temp5("123.123");
  PRInt32 error=0;
  float f=temp5.ToFloat(&error);
  nsString temp6("1234");
  error=0;
  PRInt32 theInt=temp6.ToInteger(&error);

  //**********************************************
  //Now let's test a few string COMPARISION ops...
  //**********************************************

  nsString temp8("aaaa");
  nsString temp9("bbbb");

  const char* aaaa="aaaa";
  const char* bbbb="bbbb";

  NS_ASSERTION(temp8==temp8,"Error: Comparision routine");
  NS_ASSERTION(temp8==aaaa,"Error: Comparision routine");

  NS_ASSERTION(temp8!=temp9,"Error: Comparision (!) routine");
  NS_ASSERTION(temp8!=bbbb,"Error: Comparision (!) routine");

  NS_ASSERTION(temp8<temp9,"Error: Comparision (<) routine");
  NS_ASSERTION(temp8<bbbb,"Error: Comparision (<) routine");

  NS_ASSERTION(temp9>temp8,"Error: Comparision (>) routine");
  NS_ASSERTION(temp9>aaaa,"Error: Comparision (>) routine");

  NS_ASSERTION(temp8<=temp8,"Error: Comparision (<=) routine");
  NS_ASSERTION(temp8<=temp9,"Error: Comparision (<=) routine");
  NS_ASSERTION(temp8<=bbbb,"Error: Comparision (<=) routine");

  NS_ASSERTION(temp9>=temp9,"Error: Comparision (<=) routine");
  NS_ASSERTION(temp9>=temp8,"Error: Comparision (<=) routine");
  NS_ASSERTION(temp9>=aaaa,"Error: Comparision (<=) routine");

  NS_ASSERTION(temp8.Equals(temp8),"Equals error");
  NS_ASSERTION(temp8.Equals(aaaa),"Equals error");
  
  nsString temp10(temp8);
  temp10.ToUpperCase();
  NS_ASSERTION(temp8.EqualsIgnoreCase(temp10),"Equals error");
  NS_ASSERTION(temp8.EqualsIgnoreCase("AAAA"),"Equals error");


  //**********************************************
  //Now let's test a few string MANIPULATORS...
  //**********************************************

  nsAutoString ab("ab");
  nsString abcde("cde");
  abcde.Insert(ab,0,2);
  nsAutoString xxx("xxx");
  abcde.Insert(xxx,2,3);

  temp2.ToUpperCase();
  for(i=0;i<temp2.Length();i++) {
    NS_ASSERTION(nsCRT::ToUpper(temp1[i])==temp2[i],"ToUpper error!");         
  }
  temp2.ToLowerCase();
  for(i=0;i<temp2.Length();i++) {
    NS_ASSERTION(temp1[i]==temp2[i],"ToLower error!");         
  }
  nsString temp7(temp2);
  temp2.ToUpperCase(temp7);
  for(i=0;i<temp2.Length();i++) {
    NS_ASSERTION(nsCRT::ToUpper(temp1[i])==temp7[i],"ToLower error!");         
  }

  nsString cut("abcdef");
  cut.Cut(7,10); //this is out of bounds, so ignore...
  cut.DebugDump(cout);
  cut.Cut(5,2); //cut last chars
  cut.DebugDump(cout);
  cut.Cut(1,1); //cut first char
  cut.DebugDump(cout);
  cut.Cut(2,1); //cut one from the middle
  cut.DebugDump(cout);
  cut="Hello there Rick";
  cut.StripChars("Re"); //remove the R and e characters...
  cut.DebugDump(cout);

  cut="'\"abcdef\"'";
  cut.Trim("'");
  cut.DebugDump(cout);
  cut.Trim("\"",PR_TRUE,PR_FALSE);
  cut.DebugDump(cout);
  cut.Trim("\"",PR_FALSE,PR_TRUE);
  cut.DebugDump(cout);

  cut="abc  def\n\n ghi";
  cut.StripWhitespace();
  cut.DebugDump(cout);
  cut="abc  def\n\n ghi";
  cut.CompressWhitespace();
  cut.DebugDump(cout);


  //**********************************************
  //Now let's test the ASSIGNMENT operators...
  //**********************************************

  nsString temp12;
  nsString temp13;
  nsString temp14;

  temp12 = a;  //test assignment from another string ("hello")
  temp13= "Hello world!"; //test assignment from char*
  temp14 = '?'; //test assignment from char

  PRUnichar* uni=temp4.GetUnicode();
  nsString temp14a;
  temp14a=uni; //test PRUnichar assignment operator...
  temp14a.DebugDump(cout);

  //**********************************************
  //Now let's test the APPENDING operators...
  //**********************************************

  temp14a+=uni;  //test PRUnichar append operator (+=)
  temp14a.DebugDump(cout);

  temp12+=temp2;   //append another string
  temp12.DebugDump(cout);
  temp13+="What!"; //append a char*
  temp13.DebugDump(cout);
  temp14+='?';    //append a char
  temp14.DebugDump(cout);

  temp14.Append(1000,10); //append an int
  temp14.DebugDump(cout);
  float f1=123.55f;
  temp14.Append(f1); //append a float
  temp14.DebugDump(cout);

  //**********************************************
  //Now let's test the SEARCHING operations...
  //**********************************************

  nsString temp15("abcdefghijklmnopqrstuvwxyzabc");
  PRInt32 pos=temp15.Find("efg");
  NS_ASSERTION(pos==4,"Error: Find routine");

  pos=temp15.Find(PRUnichar('r'));
  NS_ASSERTION(pos==17,"Error: Find char routine");

  pos=temp15.FindFirstCharInSet("12k");
  NS_ASSERTION(pos==10,"Error: FindFirstInChar routine");

  pos=temp15.FindLastCharInSet("12k");
  NS_ASSERTION(pos==10,"Error: FindLastInChar routine");

  pos=temp15.RFind("abc");
  NS_ASSERTION(pos==26,"Error: RFind routine");

  pos=temp15.RFind("xxx");
  NS_ASSERTION(pos==-1,"Error: RFind routine"); //this should fail

  pos=temp15.RFind("");
  NS_ASSERTION(pos==-1,"Error: RFind routine"); //this too should fail.

  pos=temp15.RFind(PRUnichar('a'));
  NS_ASSERTION(pos==26,"Error: RFind routine");


#endif
}



//----------------------------------------------------------------------

/*-------------------------------------------------------
 * 
 * @update	gess 3/27/98
 * @param 
 * @return
 *------------------------------------------------------*/
nsAutoString::nsAutoString() 
  : nsString(PR_TRUE) 
{
  mStr = mBuf;
  mLength=0;
  mCapacity = (sizeof(mBuf) / sizeof(chartype))-sizeof(chartype);
  mStr[0] = 0;
}

/*-------------------------------------------------------
 * 
 * @update	gess 3/27/98
 * @param 
 * @return
 *------------------------------------------------------*/
nsAutoString::nsAutoString(const char* isolatin1) 
  : nsString(PR_TRUE)
{
  mLength=0;
  mCapacity = (sizeof(mBuf) / sizeof(chartype))-sizeof(chartype);
  mStr=mBuf;
  SetString(isolatin1);
}

/*-------------------------------------------------------
 * 
 * @update	gess 3/27/98
 * @param 
 * @return
 *------------------------------------------------------*/
nsAutoString::nsAutoString(const nsString& other)
  : nsString(PR_TRUE)
{
  mLength=0;
  mCapacity = (sizeof(mBuf) / sizeof(chartype))-sizeof(chartype);
  mStr=mBuf;
  SetString(other.GetUnicode(),other.Length());
}

/*-------------------------------------------------------
 * 
 * @update	gess 3/27/98
 * @param 
 * @return
 *------------------------------------------------------*/
nsAutoString::nsAutoString(PRUnichar aChar) 
  : nsString(PR_TRUE)
{
  mLength=0;
  mCapacity = (sizeof(mBuf) / sizeof(chartype))-sizeof(chartype);
  mStr=mBuf;
  Append(aChar);
}

/*-------------------------------------------------------
 * 
 * @update	gess 3/27/98
 * @param 
 * @return
 *------------------------------------------------------*/
nsAutoString::nsAutoString(const nsAutoString& other) 
  : nsString(PR_TRUE)
{
  mLength=0;
  mCapacity = (sizeof(mBuf) / sizeof(chartype))-sizeof(chartype);
  mStr=mBuf;
  SetString(other.GetUnicode(),other.mLength);
}

/**
 * nsAutoString's buffer growing routine uses a different algorithm
 * than nsString because the lifetime of the auto string is assumed
 * to be shorter. Therefore, we double the size of the buffer each
 * time we grow so that (hopefully) we quickly get to the right
 * size.
 */
void nsAutoString::EnsureCapacityFor(PRInt32 aNewLength) {
  if (aNewLength > mCapacity) {
    PRInt32 size = mCapacity * 2;
    if (size < aNewLength) {
      size = mCapacity + aNewLength;
    }
    mCapacity=size;
    chartype* temp = new chartype[mCapacity+1];
    if (mLength > 0) {
      nsCRT::memcpy(temp, mStr, mLength * sizeof(chartype));
    }
    if ((mStr != mBuf) && (0 != mStr)) {
      delete [] mStr;
    }
    mStr = temp;
  }
}

/*-------------------------------------------------------
 * 
 * @update	gess 3/27/98
 * @param 
 * @return
 *------------------------------------------------------*/
nsAutoString::nsAutoString(const PRUnichar* unicode, PRInt32 uslen) 
  : nsString(PR_TRUE)
{
  mStr = mBuf;
  mCapacity = sizeof(mBuf) / sizeof(chartype);
  if (0 == uslen) {
    uslen = nsCRT::strlen(unicode);
  }
  Append(unicode, uslen);
}

/*-------------------------------------------------------
 * 
 * @update	gess 3/27/98
 * @param 
 * @return
 *------------------------------------------------------*/
nsAutoString::~nsAutoString()
{
  if (mStr == mBuf) {
    // Force to null so that baseclass dtor doesn't do damage
    mStr = nsnull;
  }
}


/**-------------------------------------------------------
 *  
 *  
 *  @update  gess 3/31/98
 *  @param   
 *  @return  
 *------------------------------------------------------*/
void nsAutoString::SelfTest(){
  nsAutoString xas("Hello there");
  xas.Append("this string exceeds the max size");
  xas.DebugDump(cout);
}


/*-------------------------------------------------------
 * 
 * @update	gess 3/27/98
 * @param 
 * @return
 *------------------------------------------------------*/
NS_BASE int fputs(const nsString& aString, FILE* out)
{
  char buf[100];
  char* cp = buf;
  PRInt32 len = aString.Length();
  if (len >= sizeof(buf)) {
    cp = aString.ToNewCString();
  } else {
    aString.ToCString(cp, len + 1);
  }
  ::fwrite(cp, 1, len, out);
  if (cp != buf) {
    delete cp;
  }
  return (int) len;
}
       
