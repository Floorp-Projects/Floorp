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
#include "nsISizeOfHandler.h"


#include "nsUnicharUtilCIID.h"
#include "nsIServiceManager.h"
#include "nsICaseConversion.h"

const PRInt32 kGrowthDelta = 8;
const PRInt32 kNotFound = -1;
PRUnichar gBadChar = 0;
const char* kOutOfBoundsError = "Error: out of bounds";
const char* kNullPointerError = "Error: unexpected null ptr";
const char* kFoolMsg = "Error: Some fool overwrote the shared buffer.";

PRUnichar kCommonEmptyBuffer[100];   //shared by all strings; NEVER WRITE HERE!!!

#ifdef  RICKG_DEBUG
PRBool nsString1::mSelfTested = PR_FALSE;   
#endif


#define NOT_USED 0xfffd

static PRUint16 PA_HackTable[] = {
	NOT_USED,
	NOT_USED,
	0x201a,  /* SINGLE LOW-9 QUOTATION MARK */
	0x0192,  /* LATIN SMALL LETTER F WITH HOOK */
	0x201e,  /* DOUBLE LOW-9 QUOTATION MARK */
	0x2026,  /* HORIZONTAL ELLIPSIS */
	0x2020,  /* DAGGER */
	0x2021,  /* DOUBLE DAGGER */
	0x02c6,  /* MODIFIER LETTER CIRCUMFLEX ACCENT */
	0x2030,  /* PER MILLE SIGN */
	0x0160,  /* LATIN CAPITAL LETTER S WITH CARON */
	0x2039,  /* SINGLE LEFT-POINTING ANGLE QUOTATION MARK */
	0x0152,  /* LATIN CAPITAL LIGATURE OE */
	NOT_USED,
	NOT_USED,
	NOT_USED,

	NOT_USED,
	0x2018,  /* LEFT SINGLE QUOTATION MARK */
	0x2019,  /* RIGHT SINGLE QUOTATION MARK */
	0x201c,  /* LEFT DOUBLE QUOTATION MARK */
	0x201d,  /* RIGHT DOUBLE QUOTATION MARK */
	0x2022,  /* BULLET */
	0x2013,  /* EN DASH */
	0x2014,  /* EM DASH */
	0x02dc,  /* SMALL TILDE */
	0x2122,  /* TRADE MARK SIGN */
	0x0161,  /* LATIN SMALL LETTER S WITH CARON */
	0x203a,  /* SINGLE RIGHT-POINTING ANGLE QUOTATION MARK */
	0x0153,  /* LATIN SMALL LIGATURE OE */
	NOT_USED,
	NOT_USED,
	0x0178   /* LATIN CAPITAL LETTER Y WITH DIAERESIS */
};

static PRUnichar gToUCS2[256];

class CTableConstructor {
public:
  CTableConstructor(){
    PRUnichar* cp = gToUCS2;
    PRInt32 i;
    for (i = 0; i < 256; i++) {
      *cp++ = PRUnichar(i);
    }
    cp = gToUCS2;
    for (i = 0; i < 32; i++) {
      cp[0x80 + i] = PA_HackTable[i];
    }
  }
};
static CTableConstructor gTableConstructor;

//---- XPCOM code to connect with UnicharUtil

class HandleCaseConversionShutdown2 : public nsIShutdownListener {
public :
   NS_IMETHOD OnShutdown(const nsCID& cid, nsISupports* service);
   HandleCaseConversionShutdown2(void) { NS_INIT_REFCNT(); }
   virtual ~HandleCaseConversionShutdown2(void) {}
   NS_DECL_ISUPPORTS
};
static NS_DEFINE_CID(kUnicharUtilCID, NS_UNICHARUTIL_CID);
static NS_DEFINE_IID(kICaseConversionIID, NS_ICASECONVERSION_IID);

static nsICaseConversion * gCaseConv = NULL; 

static NS_DEFINE_IID(kIShutdownListenerIID, NS_ISHUTDOWNLISTENER_IID);
NS_IMPL_ISUPPORTS(HandleCaseConversionShutdown2, kIShutdownListenerIID);

nsresult
HandleCaseConversionShutdown2::OnShutdown(const nsCID& cid, nsISupports* service)
{
    if (cid.Equals(kUnicharUtilCID)) {
        NS_ASSERTION(service == gCaseConv, "wrong service!");
        gCaseConv->Release();
        gCaseConv = NULL;
    }
    return NS_OK;
}

static HandleCaseConversionShutdown2* gListener = NULL;

static void StartUpCaseConversion()
{
    nsresult err;

    if ( NULL == gListener )
    {
      gListener = new HandleCaseConversionShutdown2();
      gListener->AddRef();
    }
    err = nsServiceManager::GetService(kUnicharUtilCID, kICaseConversionIID,
                                        (nsISupports**) &gCaseConv, gListener);
}
static void CheckCaseConversion()
{
    if(NULL == gCaseConv )
      StartUpCaseConversion();

    // NS_ASSERTION( gCaseConv != NULL , "cannot obtain UnicharUtil");
   
}

/***********************************************************************
  IMPLEMENTATION NOTES:

  Man I hate writing string classes. 
  You'd think after about a qintrillion lines of code have been written,
  that no poor soul would ever have to do this again. Sigh.
 ***********************************************************************/


/**
 * Default constructor. Note that we actually allocate a small buffer
 * to begin with. This is because the "philosophy" of the string class
 * was to allow developers direct access to the underlying buffer for
 * performance reasons. 
 */
nsString::nsString() {
  NS_ASSERTION(kCommonEmptyBuffer[0]==0,kFoolMsg);
  mLength = mCapacity = 0;
  mStr = kCommonEmptyBuffer;
#ifdef RICKG_DEBUG
  if(!mSelfTested) {
    mSelfTested=PR_TRUE;
		SelfTest();
  }
#endif
}



/**
 * This constructor accepts an isolatin string
 * @update	gess7/30/98
 * @param   aCString is a ptr to a 1-byte cstr
 */
nsString::nsString(const char* aCString) {  
  mLength=mCapacity=0;
  mStr = kCommonEmptyBuffer;
  if(aCString) {
    PRInt32 len=strlen(aCString);
    EnsureCapacityFor(len);
    this->SetString(aCString,len);
  }
}


/**
 * This is our copy constructor 
 * @update	gess7/30/98
 * @param   reference to another nsString
 */
nsString::nsString(const nsString &aString) {
  mLength=mCapacity=0;
  mStr = kCommonEmptyBuffer;
  if(aString.mLength) {
    EnsureCapacityFor(aString.mLength);
    this->SetString(aString.mStr,aString.mLength);
  }
}


/**
 * Constructor from a unicode string
 * @update	gess7/30/98
 * @param   anicodestr pts to a unicode string
 */
nsString::nsString(const PRUnichar* aUnicodeStr){
  mLength=mCapacity=0;
  mStr = kCommonEmptyBuffer;

  PRInt32 len=(aUnicodeStr) ? nsCRT::strlen(aUnicodeStr) : 0;
  if(len>0) {
    EnsureCapacityFor(len);
    this->SetString(aUnicodeStr,len);
  }
}


/**
 * Destructor
 * @update	gess7/30/98
 */
nsString::~nsString()
{
  if(mStr && (mStr!=kCommonEmptyBuffer))
    delete [] mStr;
  mStr=0;
  mCapacity=mLength=0;
}


/**
 * This method truncates this string to given length.
 *
 * @update	gess 7/27/98
 * @param   anIndex -- new length of string
 * @return  nada
 */
void nsString::Truncate(PRInt32 anIndex) {
  if((anIndex>-1) && (anIndex<mLength)) {
    mLength=anIndex;
    mStr[mLength]=0;
  }
}

void nsString::SizeOf(nsISizeOfHandler* aHandler) const
{
  aHandler->Add(sizeof(*this));
  aHandler->Add(mCapacity * sizeof(chartype));
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
    PRInt32 theIndex;
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
 * This method gets called when the internal buffer needs
 * to grow to a given size.
 * @update  gess 3/30/98
 * @param   aNewLength -- new capacity of string  
 * @return  void
 */
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
      nsCRT::memcpy(temp, mStr, mLength * sizeof(chartype) + sizeof(chartype));
    }
    if(mStr && (mStr!=kCommonEmptyBuffer))
      delete [] mStr;
    mStr = temp;
    mStr[mLength]=0;
  }
}


/**
 * Call this method if you want string to report a shorter length.
 * @update	gess7/30/98
 * @param   aLength -- contains new length for mStr
 * @return
 */
void nsString::SetLength(PRInt32 aLength) {
  if(aLength>mLength) {
    EnsureCapacityFor(aLength);
    nsCRT::zero(mStr + mLength, (aLength - mLength) * sizeof(chartype));
  }
  if((aLength>0) && (aLength<mLength))
    mStr[aLength]=0;
  mLength=aLength;
}


/*********************************************************
  ACCESSOR METHODS....
 *********************************************************/

/**
 * Retrieve pointer to internal string value
 * @update	gess 7/27/98
 * @return  PRUnichar* to internal string
 */
const PRUnichar* nsString::GetUnicode(void) const{
  return mStr;
}

nsString::operator const PRUnichar*() const{
  return mStr;
}


/**
 * Retrieve unicode char at given index
 * @update	gess 7/27/98
 * @param   offset into string
 * @return  PRUnichar* to internal string
 */
PRUnichar nsString::operator()(int anIndex) const{
  NS_ASSERTION(anIndex<mLength,kOutOfBoundsError);
  if((anIndex<mLength) && (mStr))
    return mStr[anIndex];
  else return gBadChar;
}

/**
 * Retrieve reference to unicode char at given index
 * @update	gess 7/27/98
 * @param   offset into string
 * @return  PRUnichar& from internal string
 */
PRUnichar& nsString::operator[](PRInt32 anIndex) const{
//  NS_ASSERTION(anIndex<mLength,kOutOfBoundsError);
  if((anIndex<mLength) && (mStr))
    return mStr[anIndex];
  else return gBadChar;
}

/**
 * Retrieve reference to unicode char at given index
 * @update	gess 7/27/98
 * @param   offset into string
 * @return  PRUnichar& from internal string
 */
PRUnichar& nsString::CharAt(PRInt32 anIndex) const{
  NS_ASSERTION(anIndex<mLength,kOutOfBoundsError);
  if((anIndex<mLength) && (mStr))
    return mStr[anIndex];
  else return gBadChar;
}

/**
 * Retrieve reference to first unicode char in string
 * @update	gess 7/27/98
 * @return  PRUnichar from internal string
 */
PRUnichar& nsString::First() const{
  if((mLength) && (mStr))
    return mStr[0];
  else return gBadChar;
}

/**
 * Retrieve reference to last unicode char in string
 * @update	gess 7/27/98
 * @return  PRUnichar from internal string
 */
PRUnichar& nsString::Last() const{
  if((mLength) && (mStr))
    return mStr[mLength-1];
  else return gBadChar;
}

PRBool nsString::SetCharAt(PRUnichar aChar,PRInt32 anIndex){
  PRBool result=PR_FALSE;
  if(anIndex<mLength){
    PRUnichar* theStr=(PRUnichar*)mStr;
    theStr[anIndex]=aChar;
    result=PR_TRUE;
  }
  return result;
}

/**
 * Create a new string by appending given string to this
 * @update	gess 7/27/98
 * @param   aString -- 2nd string to be appended
 * @return  new string
 */
nsString nsString::operator+(const nsString& aString){
  nsString temp(*this);
  temp.Append(aString.mStr,aString.mLength);
  return temp;
}


/**
 * create a new string by adding this to the given buffer.
 * @update	gess 7/27/98
 * @param   aCString is a ptr to cstring to be added to this
 * @return  newly created string
 */
nsString nsString::operator+(const char* aCString) {
  nsString temp(*this);
  temp.Append(aCString);
  return temp;
}


/**
 * create a new string by adding this to the given char.
 * @update	gess 7/27/98
 * @param   aChar is a char to be added to this
 * @return  newly created string
 */
nsString nsString::operator+(char aChar) {
  nsString temp(*this);
  temp.Append(chartype(aChar));
  return temp;
}


/**
 * create a new string by adding this to the given buffer.
 * @update	gess 7/27/98
 * @param   aStr unichar buffer to be added to this
 * @return  newly created string
 */
nsString nsString::operator+(const PRUnichar* aStr) {
  nsString temp(*this);
  temp.Append(aStr);
  return temp;
}


/**
 * create a new string by adding this to the given char.
 * @update	gess 7/27/98
 * @param   aChar is a unichar to be added to this
  * @return  newly created string
 */
nsString nsString::operator+(PRUnichar aChar) {
  nsString temp(*this);
  temp.Append(aChar);
  return temp;
}


/**
 * Converts all chars in internal string to lower
 * @update	gess 7/27/98
 */
void nsString::ToLowerCase()
{
  // I18N code begin
  CheckCaseConversion();
  if(gCaseConv) {
    nsresult err = gCaseConv->ToLower(mStr, mStr, mLength);
    if( NS_SUCCEEDED(err))
      return;
  }
  // I18N code end

  // somehow UnicharUtil return failed, fallback to the old ascii only code
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

/**
 * Converts all chars in internal string to upper
 * @update	gess 7/27/98
 */
void nsString::ToUpperCase()
{
  // I18N code begin
  CheckCaseConversion();
  if(gCaseConv) {
    nsresult err = gCaseConv->ToUpper(mStr, mStr, mLength);
    if( NS_SUCCEEDED(err))
      return;
  }
  // I18N code end

  // somehow UnicharUtil return failed, fallback to the old ascii only code
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

/**
 * Converts all chars in given string to UCS2
 */
void nsString::ToUCS2(PRInt32 aStartOffset){
  if(aStartOffset<mLength){
    chartype* cp = &mStr[aStartOffset];
    chartype* end = cp + mLength;
    while (cp < end) {
      unsigned char ch = (unsigned char)*cp;
      if( 0x0080 == (0xFFE0 & (*cp)) ) // limit to only 0x0080 to 0x009F
        *cp=gToUCS2[ch];
      cp++;
    }
  }
}



/**
 * Converts chars in this to lowercase, and
 * stores them in aOut
 * @update	gess 7/27/98
 * @param   aOut is a string to contain result
 */
void nsString::ToLowerCase(nsString& aOut) const
{
  aOut.EnsureCapacityFor(mLength);
  aOut.mLength = mLength;

  // I18N code begin
  CheckCaseConversion();
  if(gCaseConv) {
    nsresult err = gCaseConv->ToLower(mStr, aOut.mStr, mLength);
    (*(aOut.mStr+mLength)) = 0;
    if( NS_SUCCEEDED(err))
      return;
  }
  // I18N code end

  // somehow UnicharUtil return failed, fallback to the old ascii only code

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

/**
 * Converts chars in this to lowercase, and
 * stores them in a given output string
 * @update	gess 7/27/98
 * @param   aOut is a string to contain result
 */
void nsString::ToUpperCase(nsString& aOut) const
{
  aOut.EnsureCapacityFor(mLength);
  aOut.mLength = mLength;

  // I18N code begin
  CheckCaseConversion();
  if(gCaseConv) {
    nsresult err = gCaseConv->ToUpper(mStr, aOut.mStr, mLength);
    (*(aOut.mStr+mLength)) = 0;
    if( NS_SUCCEEDED(err))
      return;
  }
  // I18N code end

  // somehow UnicharUtil return failed, fallback to the old ascii only code
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

/**
 * Creates a duplicate clone (ptr) of this string.
 * @update	gess 7/27/98
 * @return  ptr to clone of this string
 */
nsString* nsString::ToNewString() const {
  return new nsString(mStr);
}

/**
 * Creates an aCString clone of this string
 * NOTE: Call delete[] when you're done with this copy. NOT free!
 * @update	gess 7/27/98
 * @return  ptr to new aCString string
 */
char* nsString::ToNewCString() const
{
  char* rv = new char[mLength + 1];
  return ToCString(rv,mLength+1);
}

/**
 * Creates an unichar clone of this string
 * @update	gess 7/27/98
 * @return  ptr to new unichar string
 */
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

/**
 * Copies contents of this onto given string.
 * @update	gess 7/27/98
 * @param   aString to hold copy of this
 * @return  nada.
 */
void nsString::Copy(nsString& aString) const
{
  aString.mLength = 0;
  aString.Append(mStr, mLength);
}

/**
 * 
 * @update	gess 7/27/98
 * @param 
 * @return
 */
char* nsString::ToCString(char* aBuf, PRInt32 aBufLength) const
{
  aBufLength--;                 // leave room for the \0
  PRInt32 len = (mLength > aBufLength) ? aBufLength : mLength;
  char* to = aBuf;
  chartype* from = mStr;
  while (--len >= 0) {
    *to++ = char(*from++);
  }
  *to++ = '\0';
  return aBuf;
}

/**
 * Perform string to float conversion.
 * @update	gess 7/27/98
 * @param   aErrorCode will contain error if one occurs
 * @return  float rep of string value
 */
float nsString::ToFloat(PRInt32* aErrorCode) const
{
  char buf[40];
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
 * @update	gess 10/01/98
 * @param   aErrorCode will contain error if one occurs
 * @param   aRadix tells us what base to expect the string in.
 * @return  int rep of string value; aErrorCode gets set too: NS_OK, NS_ERROR_ILLEGAL_VALUE
 */
PRInt32 nsString::ToInteger(PRInt32* aErrorCode,PRInt32 aRadix) const {
  PRInt32     result = 0;
  PRInt32     decPt=Find(PRUnichar('.'),0);
  PRUnichar*  cp = (-1==decPt) ? mStr + mLength-1 : mStr+decPt-1;
  char        digit=0;
  PRUnichar   theChar;
//  PRInt32     theShift=0;
  PRInt32     theMult=1;

  *aErrorCode = (0<mLength) ? NS_OK : NS_ERROR_ILLEGAL_VALUE;


  // Skip trailing non-numeric...
  while (cp >= mStr) {
    theChar = *cp;
    if((theChar>='0') && (theChar<='9')){
      break;
    }
    else if((theChar>='a') && (theChar<='f')) {
      break;
    }
    else if((theChar>='A') && (theChar<='F')) {
      break;
    }
    cp--;
  }

    //now iterate the numeric chars and build our result
  while(cp>=mStr) {
    theChar=*cp--;
    if((theChar>='0') && (theChar<='9')){
      digit=theChar-'0';
    }
    else if((theChar>='a') && (theChar<='f')) {
      digit=(theChar-'a')+10;
    }
    else if((theChar>='A') && (theChar<='F')) {
      digit=(theChar-'A')+10;
    }
    else if('-'==theChar) {
      result=-result;
      break;
    }
    else if(('+'==theChar) || (' '==theChar) || ('#'==theChar)) { //stop in a good state if you see this...
      break;
    }
    else if((('x'==theChar) || ('X'==theChar)) && (16==aRadix)) {  
      //stop in a good state.
      break;
    }
    else{
      *aErrorCode=NS_ERROR_ILLEGAL_VALUE;
      result=0;
      break;
    }
    result+=digit*theMult;
    theMult*=aRadix;
  }
  
  return result;
}

/**
 * assign given PRUnichar* to this string
 * @update	gess 7/27/98
 * @param   PRUnichar: buffer to be assigned to this 
 * @return  this
 */
nsString& nsString::SetString(const PRUnichar* aStr,PRInt32 aLength) {
  if((0 == aLength) || (nsnull == aStr)) {
    mLength=0;
    if (nsnull != mStr) {
      mStr[0]=0;  
    }

  } else {
    PRInt32 len=(aLength<0) ? nsCRT::strlen(aStr) : aLength;
    if(mCapacity<=len) 
      EnsureCapacityFor(len);
    nsCRT::memcpy(mStr,aStr,len*sizeof(chartype));
    mLength=len;
    mStr[mLength]=0;
  }

  return *this;
}


/**
 * assign given char* to this string
 * @update	gess 7/27/98
 * @param   aCString: buffer to be assigned to this 
 * @return  this
 */
nsString& nsString::SetString(const char* aCString,PRInt32 aLength) {
  if(aCString!=0) {
    PRInt32 len=(aLength<0) ? nsCRT::strlen(aCString) : aLength; 
    if(mCapacity<=len) 
      EnsureCapacityFor(len);
    const unsigned char* from = (const unsigned char*) aCString;
    const unsigned char* end = from + len;
    PRUnichar* dst = mStr;
    while (from < end) {
      *dst++ = PRUnichar(*from++);
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

/**
 * assign given char* to this string
 * @update	gess 7/27/98
 * @param   PRUnichar: buffer to be assigned to this 
 * @return  this
 */
nsString& nsString::operator=(const PRUnichar* aStr) {
  return this->SetString(aStr);
}

/**
 * assign given string to this one
 * @update	gess 7/27/98
 * @param   aString: string to be added to this
 * @return  this
 */
nsString& nsString::operator=(const nsString& aString) {
  return this->SetString(aString.mStr,aString.mLength);
}


/**
 * assign given char* to this string
 * @update	gess 7/27/98
 * @param   aCString: buffer to be assigned to this 
 * @return  this
 */
nsString& nsString::operator=(const char* aCString) {
  return SetString(aCString);
}


/**
 * assign given char to this string
 * @update	gess 7/27/98
 * @param   aChar: char to be assignd to this
 * @return  this
 */
nsString& nsString::operator=(char aChar) {
  return this->operator=(PRUnichar(aChar));
}

/**
 * assign given char to this string
 * @update	gess 7/27/98
 * @param   aChar: char to be assignd to this
 * @return  this
 */
nsString& nsString::operator=(PRUnichar aChar) {
  if(mCapacity<1) 
    EnsureCapacityFor(kGrowthDelta);
  mStr[0]=aChar;
  mLength=1;
  mStr[mLength]=0;
  return *this;
}

/**
 * append given string to this string
 * @update	gess 7/27/98
 * @param   aString : string to be appended to this
 * @return  this
 */
nsString& nsString::Append(const nsString& aString,PRInt32 aLength) {
  return Append(aString.mStr,aString.mLength);
}

/**
 * append given string to this string
 * @update	gess 7/27/98
 * @param   aString : string to be appended to this
 * @return  this
 */
nsString& nsString::Append(const char* aCString,PRInt32 aLength) {
  if(aCString!=0) {
    PRInt32 len=(aLength<0) ? strlen(aCString) : aLength;
    if(mLength+len >= mCapacity) {
      EnsureCapacityFor(mLength+len);
    }
    const unsigned char* from = (const unsigned char*) aCString;
    const unsigned char* end = from + len;
    PRUnichar* to = mStr + mLength;
    while (from < end) {
      *to++ = PRUnichar(*from++);
    }
    mLength+=len;
    mStr[mLength]=0;
  }
  return *this;
}

/**
 * append given string to this string
 * @update	gess 7/27/98
 * @param   aString : string to be appended to this
 * @return  this
 */
nsString& nsString::Append(char aChar) {
  return Append(PRUnichar(aChar));
}

/**
 * append given string to this string
 * @update	gess 7/27/98
 * @param   aString : string to be appended to this
 * @return  this
 */
nsString& nsString::Append(const PRUnichar* aString,PRInt32 aLength) {
  if(aString!=0) {
    PRInt32 len=(aLength<0) ? nsCRT::strlen(aString) : aLength;
    if(mLength+len >= mCapacity) {
      EnsureCapacityFor(mLength+len);
    }
    if(len>0)
      nsCRT::memcpy(&mStr[mLength],aString,len*sizeof(chartype));
    mLength+=len;
    mStr[mLength]=0;
  }
  return *this;
}


/**
 * append given string to this string
 * @update	gess 7/27/98
 * @param   aString : string to be appended to this
 * @return  this
 */
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


/**
 * append given string to this string
 * @update	gess 7/27/98
 * @param   aString : string to be appended to this
 * @return  this
 */
nsString& nsString::operator+=(const nsString &aString) {  
  return this->Append(aString.mStr,aString.mLength);
}


/**
 * append given buffer to this string
 * @update	gess 7/27/98
 * @param   aCString: buffer to be appended to this
 * @return  this
 */
nsString& nsString::operator+=(const char* aCString) {
  return Append(aCString);
}


/**
 * append given buffer to this string
 * @update	gess 7/27/98
 * @param   aBuffer: buffer to be appended to this
 * @return  this
 */
nsString& nsString::operator+=(const PRUnichar* aBuffer) {
  return Append(aBuffer);
}


/**
 * append given char to this string
 * @update	gess 7/27/98
 * @param   aChar: char to be appended to this
 * @return  this
 */
nsString& nsString::operator+=(PRUnichar aChar) {
  return Append(aChar);
}

/**
 * 
 * @update	gess 7/27/98
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
  PR_snprintf(buf, sizeof(buf), fmt, aInteger);
  Append(buf);
  return *this;
}


/**
 * 
 * @update	gess 7/27/98
 * @param 
 * @return
 */
nsString& nsString::Append(float aFloat){
  char buf[40];
  PR_snprintf(buf, sizeof(buf), "%g", aFloat);
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
PRInt32 nsString::Left(nsString& aCopy,PRInt32 aCount) const {
  return Mid(aCopy,0,aCount);
}

/*
 *  Copies n characters from this string to given string,
 *  starting at the given offset.
 *  
 *  
 *  @update  gess 4/1/98
 *  @param   aCopy -- Receiving string
 *  @param   aCount -- number of chars to copy
 *  @param   anOffset -- position where copying begins
 *  @return  number of chars copied
 */
PRInt32 nsString::Mid(nsString& aCopy,PRInt32 anOffset,PRInt32 aCount) const {
  aCopy.Truncate();
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
PRInt32 nsString::Right(nsString& aCopy,PRInt32 aCount) const {
  PRInt32 offset=(mLength-aCount<0) ? 0 : mLength-aCount;
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
PRInt32 nsString::Insert(const nsString& aCopy,PRInt32 anOffset,PRInt32 aCount) {
  aCount=(aCount>aCopy.mLength) ? aCopy.mLength : aCount; //don't try to copy more than you are given
  if (aCount < 0) aCount = aCopy.mLength;
  if(0<=anOffset) {
    if(aCount>0) {

      //1st optimization: If you're inserting at end, then simply append!
      if(anOffset>=mLength){
        Append(aCopy,aCopy.mLength);
        return aCopy.mLength;
      }

      if(mLength+aCount >= mCapacity) {
        EnsureCapacityFor(mLength+aCount);
      }

      PRUnichar* last = mStr + mLength;
      PRUnichar* first = mStr + anOffset-1;
      PRUnichar* next = mStr + mLength + aCount;
  
      //Copy rightmost chars, up to offset+aCount...
      while(first<last) {
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
    else aCount=0;
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

    if(mLength+1>=mCapacity) {
      EnsureCapacityFor(mLength+1);
    }

    PRUnichar* last = mStr + mLength;
    PRUnichar* first = mStr + anOffset-1;
    PRUnichar* next = mStr + mLength + 1;

    //Copy rightmost chars, up to offset+aCount...
    while(first<last) {
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

/*
 *  This method is used to cut characters in this string
 *  starting at anOffset, continuing for aCount chars.
 *  
 *  @update gess 3/26/98
 *  @param  anOffset -- start pos for cut operation
 *  @param  aCount -- number of chars to be cut
 *  @return *this
 */
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

/**
 *  This method is used to remove all occurances of the
 *  characters found in aSet from this string.
 *  
 *  @update gess 3/26/98
 *  @param  aSet -- characters to be cut from this
 *  @return *this 
 */
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
 *  Determine if given char is valid digit
 *  
 *  @update  gess 3/31/98
 *  @param   aChar is character to be tested
 *  @return  TRUE if char is a valid digit
 */PRBool nsString::IsDigit(PRUnichar aChar) {
  // XXX i18n
  return PRBool((aChar >= '0') && (aChar <= '9'));
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
  if (from != to) {
    while (from <= end) {
      *to++ = *from++;
    }
  }
  else {
    to = ++end;
  }

  *to = '\0';
  mLength = to - mStr;
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
nsString& nsString::CompressWhitespace( PRBool aEliminateLeading,
                                              PRBool aEliminateTrailing)
{
  
  Trim(" \r\n\t",aEliminateLeading,aEliminateTrailing);

  PRUnichar* from = mStr;
  PRUnichar* end = mStr + mLength;
  PRUnichar* to = from;

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

/**
 *  This method strips whitespace throughout the string
 *  
 *  @update  gess 7/27/98
 *  @return  this
 */
nsString& nsString::StripWhitespace()
{
  Trim(" \r\n\t");
  return StripChars("\r\t\n");
}

/**
 *  This method is used to replace all occurances of the
 *  given source char with the given dest char
 *  
 *  @param  
 *  @return *this 
 */
nsString& nsString::ReplaceChar(PRUnichar aSourceChar, PRUnichar aDestChar) {
  PRUnichar* from = mStr;
  PRUnichar* end = mStr + mLength;

  while (from < end) {
    PRUnichar ch = *from;
    if(ch==aSourceChar) {
      *from = aDestChar;
    }
    from++;
  }

  return *this;
}

/**
 *  Search for given character within this string.
 *  This method does so by using a binary search,
 *  so your string HAD BETTER BE ORDERED!
 *  
 *  @update  gess 3/25/98
 *  @param   aChar is the unicode char to be found
 *  @return  offset in string, or -1 (kNotFound)
 */
PRInt32 nsString::BinarySearch(PRUnichar aChar) const {
  PRInt32 low=0;
  PRInt32 high=mLength-1;

  while (low <= high) {
    int middle = (low + high) >> 1;
    if (mStr[middle]==aChar)
      return middle;
    if (mStr[middle]>aChar)
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
PRInt32 nsString::Find(const char* aCStringBuf) const{
  NS_ASSERTION(0!=aCStringBuf,kNullPointerError);
  PRInt32 result=kNotFound;
  if(aCStringBuf) {
    PRInt32 len=strlen(aCStringBuf);
    if((0<len) && (len<=mLength)) { //only enter if abuffer length is <= mStr length.
      PRInt32 max=mLength-len;
      for(PRInt32 offset=0;offset<=max;offset++) 
        if(0==nsCRT::strncmp(&mStr[offset],aCStringBuf,len)) 
          return offset;  //in this case, 0 means they match
    }
  }
  return result;
}

/**
 *  Search for given buffer within this string
 *  
 *  @update  gess 3/25/98
 *  @param   aString - PUnichar* to be found
 *  @return  offset in string, or -1 (kNotFound)
 */
PRInt32 nsString::Find(const PRUnichar* aString) const{
  NS_ASSERTION(0!=aString,kNullPointerError);
  PRInt32 result=kNotFound;
  if(aString) {
    PRInt32 len=nsCRT::strlen(aString);
    if((0<len) && (len<=mLength)) { //only enter if abuffer length is <= mStr length.
      PRInt32 max=mLength-len;
      for(PRInt32 offset=0;offset<=max;offset++) 
        if(0==nsCRT::strncmp(&mStr[offset],aString,len)) 
          return offset;  //in this case, 0 means they match
    }
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
PRInt32 nsString::Find(const nsString& aString) const{
  PRInt32 result=kNotFound;

  PRInt32 len=aString.mLength;
  PRInt32 offset=0;
  if((0<len) && (len<=mLength)) { //only enter if abuffer length is <= mStr length.
    PRInt32 max=mLength-len;
    for(offset=0;offset<=max;offset++) {
      if(0==nsCRT::strncmp(&mStr[offset],aString.mStr,len)) {
        return offset;  //in this case, 0 means they match
      }
    }
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
PRInt32 nsString::Find(PRUnichar aChar, PRInt32 anOffset) const{

  for(PRInt32 i=anOffset;i<mLength;i++)
    if(aChar==mStr[i])
      return i;
  return kNotFound;
}


/**
 *  
 *  
 *  @update  gess 3/25/98
 *  @param   
 *  @return  
 */
PRInt32 nsString::FindCharInSet(const char* anAsciiSet,PRInt32 anOffset) const{
  NS_ASSERTION(0!=anAsciiSet,kNullPointerError);
  if(anAsciiSet && (strlen(anAsciiSet))) {
    for(PRInt32 i=anOffset;i<mLength;i++){
      char* pos=strchr(anAsciiSet,char(mStr[i]));
      if(pos)
        return i;
    }
  }
  return kNotFound;
}

/**
 *  
 *  
 *  @update  gess 3/25/98
 *  @param   
 *  @return  
 */
PRInt32 nsString::FindCharInSet(nsString& aSet,PRInt32 anOffset) const{
  if(aSet.Length()) {
    for(PRInt32 i=anOffset;i<mLength;i++){
      PRInt32 pos=aSet.Find(mStr[i]);
      if(kNotFound!=pos)
        return i;
    }
  }
  return kNotFound;
}

/**
 *  
 *  
 *  @update  gess 3/25/98
 *  @param   
 *  @return   
 */
PRInt32 nsString::RFindCharInSet(const char* anAsciiSet,PRInt32 anOffset) const{
  NS_ASSERTION(0!=anAsciiSet,kNullPointerError);
  if(anAsciiSet && strlen(anAsciiSet)) {
    for(PRInt32 i=mLength-1;i>0;i--){
      char* pos=strchr(anAsciiSet,char(mStr[i]));
      if(pos) 
        return i;
    }
  }
  return kNotFound;
}

/**
 *  
 *  
 *  @update  gess 3/25/98
 *  @param   
 *  @return   
 */
PRInt32 nsString::RFindCharInSet(nsString& aSet,PRInt32 anOffset) const{
  if(aSet.Length()) {
    for(PRInt32 i=mLength-1;i>0;i--){
      PRInt32 pos=aSet.Find(mStr[i]);
      if(kNotFound!=pos) 
        return i;
    }
  }
  return kNotFound;
}

/**
 *  
 *  
 *  @update  gess 3/25/98
 *  @param   
 *  @return  
 */
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

/**
 *  
 *  
 *  @update  gess 3/25/98
 *  @param   
 *  @return  
 */
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

/**
 *  
 *  
 *  @update  gess 3/25/98
 *  @param   
 *  @return  
 */
PRInt32 nsString::RFind(const char* anAsciiSet,PRBool aIgnoreCase) const{
  NS_ASSERTION(0!=anAsciiSet,kNullPointerError);

  if(anAsciiSet) {
    PRInt32 len=strlen(anAsciiSet);
    if((len) && (len<=mLength)) { //only enter if abuffer length is <= mStr length.
      for(PRInt32 offset=mLength-len;offset>=0;offset--) {
        PRInt32 result=0;
        if(aIgnoreCase) 
          result=nsCRT::strncasecmp(&mStr[offset],anAsciiSet,len);
        else result=nsCRT::strncmp(&mStr[offset],anAsciiSet,len);
        if(0==result)
          return offset;  //in this case, 0 means they match
      }
    }
  }
  return kNotFound;
}


/**
 *  Scans this string backwards for first occurance of
 *  the given char.
 *  
 *  @update  gess 3/25/98
 *  @param   
 *  @return  offset of char in string, or -1 (kNotFound)
 */
PRInt32 nsString::RFind(PRUnichar aChar,PRBool aIgnoreCase) const{
  chartype uc=nsCRT::ToUpper(aChar);
  for(PRInt32 offset=mLength-1;offset>=0;offset--) 
    if(aIgnoreCase) {
      if(nsCRT::ToUpper(mStr[offset])==uc)
        return offset;
    }
    else if(mStr[offset]==aChar)
      return offset;  //in this case, 0 means they match
  return kNotFound;

}

/**************************************************************
  COMPARISON METHODS...
 **************************************************************/

/**
 * Compares given cstring to this string. 
 * @update	gess 7/27/98
 * @param   aCString pts to a cstring
 * @param   aIgnoreCase tells us how to treat case
 * @return  -1,0,1
 */
PRInt32 nsString::Compare(const char *aCString,PRBool aIgnoreCase,PRInt32 aLength) const {
  NS_ASSERTION(0!=aCString,kNullPointerError);

  if(-1!=aLength) {

    //if you're given a length, use it to determine the max # of bytes to compare.
    //In some cases, this can speed up the string comparison.

    int maxlen=(aLength<mLength) ? aLength : mLength;
    if (maxlen == 0) {
      if ((mLength == 0) && (aLength == 0))
        return 0;
      if (mLength == 0)
        return -1;
      return 1;
    }
    if (aIgnoreCase) {
      return nsCRT::strncasecmp(mStr,aCString,maxlen);
    }
    return nsCRT::strncmp(mStr,aCString,maxlen);
  }

  if (aIgnoreCase) {
    return nsCRT::strcasecmp(mStr,aCString);
  }
  return nsCRT::strcmp(mStr,aCString);
}

/**
 * LAST MODS:	gess
 * 
 * @param  
 * @return 
 */
PRInt32 nsString::Compare(const nsString &S,PRBool aIgnoreCase) const {
  int maxlen=(S.mLength<mLength) ? S.mLength : mLength;
  if (maxlen == 0) {
    if ((mLength == 0) && (S.mLength == 0))
      return 0;
    if (mLength == 0)
      return -1;
    return 1;
  }
  if (aIgnoreCase) {
    return nsCRT::strcasecmp(mStr,S.mStr);
  }
  return nsCRT::strcmp(mStr,S.mStr);
}

/**
 * 
 * @update	gess 7/27/98
 * @param 
 * @return
 */
PRInt32 nsString::Compare(const PRUnichar* aString,PRBool aIgnoreCase,PRInt32 aLength) const {
  NS_ASSERTION(0!=aString,kNullPointerError);
  if(-1!=aLength) {

    //if you're given a length, use it to determine the max # of bytes to compare.
    //In some cases, this can speed up the string comparison.

    int maxlen=(aLength<mLength) ? aLength : mLength;
    if (maxlen == 0) {
      if ((mLength == 0) && (aLength == 0))
        return 0;
      if (mLength == 0)
        return -1;
      return 1;
    }
    if (aIgnoreCase) {
      return nsCRT::strncasecmp(mStr,aString,maxlen);
    }
    return nsCRT::strncmp(mStr,aString,maxlen);
  }
  
  if (aIgnoreCase) {
    return nsCRT::strcasecmp(mStr,aString);
  }
  return nsCRT::strcmp(mStr,aString);
}

PRBool nsString::operator==(const nsString &S) const {return Equals(S);}      
PRBool nsString::operator==(const char *s) const {return Equals(s);}
PRBool nsString::operator==(const PRUnichar *s) const {return Equals(s);}
PRBool nsString::operator==(PRUnichar *s) const {return Equals(s);}
PRBool nsString::operator!=(const nsString &S) const {return PRBool(Compare(S)!=0);}
PRBool nsString::operator!=(const char *s) const {return PRBool(Compare(s)!=0);}
PRBool nsString::operator!=(const PRUnichar *s) const {return PRBool(Compare(s)!=0);}
PRBool nsString::operator<(const nsString &S) const {return PRBool(Compare(S)<0);}
PRBool nsString::operator<(const char *s) const {return PRBool(Compare(s)<0);}
PRBool nsString::operator<(const PRUnichar *s) const {return PRBool(Compare(s)<0);}
PRBool nsString::operator>(const nsString &S) const {return PRBool(Compare(S)>0);}
PRBool nsString::operator>(const char *s) const {return PRBool(Compare(s)>0);}
PRBool nsString::operator>(const PRUnichar *s) const {return PRBool(Compare(s)>0);}
PRBool nsString::operator<=(const nsString &S) const {return PRBool(Compare(S)<=0);}
PRBool nsString::operator<=(const char *s) const {return PRBool(Compare(s)<=0);}
PRBool nsString::operator<=(const PRUnichar *s) const {return PRBool(Compare(s)<=0);}
PRBool nsString::operator>=(const nsString &S) const {return PRBool(Compare(S)>=0);}
PRBool nsString::operator>=(const char *s) const {return PRBool(Compare(s)>=0);}
PRBool nsString::operator>=(const PRUnichar *s) const {return PRBool(Compare(s)>=0);}


/**
 * Compare this to given string; note that we compare full strings here.
 * 
 * @update gess 7/27/98
 * @param  aString is the other nsString to be compared to
 * @return TRUE if equal
 */
PRBool nsString::Equals(const nsString& aString) const {
  if(aString.mLength==mLength) {
    PRInt32 result=nsCRT::strcmp(mStr,aString.mStr);
    return PRBool(0==result);
  }
  return PR_FALSE;
}


/**
 * Compare this to given string; note that we compare full strings here.
 * The optional length argument just lets us know how long the given string is.
 * If you provide a length, it is compared to length of this string as an
 * optimization.
 * 
 * @update gess 7/27/98
 * @param  aCString -- Cstr to compare to this
 * @param  aLength -- length of given string.
 * @return TRUE if equal
 */
PRBool nsString::Equals(const char* aCString,PRInt32 aLength) const{
  NS_ASSERTION(0!=aCString,kNullPointerError);

  if((aLength>0) && (aLength!=mLength))
    return PR_FALSE;
  
  PRInt32 result=nsCRT::strcmp(mStr,aCString);
  return PRBool(0==result);
}


/**
 * Compare this to given atom
 * @update gess 7/27/98
 * @param  aAtom -- atom to compare to this
 * @return TRUE if equal
 */
PRBool nsString::Equals(const nsIAtom* aAtom) const
{
  NS_ASSERTION(0!=aAtom,kNullPointerError);
  PRInt32 result=nsCRT::strcmp(mStr,aAtom->GetUnicode());
  return PRBool(0==result);
}


/**
 * Compare given strings
 * @update gess 7/27/98
 * @param  s1 -- first string to be compared
 * @param  s2 -- second string to be compared
 * @return TRUE if equal
 */
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


/**
 * Compares all chars in both strings w/o regard to case
 * @update	gess 7/27/98
 * @param 
 * @return
 */
PRBool nsString::EqualsIgnoreCase(const nsString& aString) const{
  if(aString.mLength==mLength) {
    PRInt32 result=nsCRT::strcasecmp(mStr,aString.mStr);
    return PRBool(0==result);
  }
  return PR_FALSE;
}


/**
 * 
 * @update	gess 7/27/98
 * @param 
 * @return
 */
PRBool nsString::EqualsIgnoreCase(const nsIAtom *aAtom) const{
  NS_ASSERTION(0!=aAtom,kNullPointerError);
  PRBool result=PR_FALSE;
  if(aAtom){
    PRInt32 cmp=nsCRT::strcasecmp(mStr,aAtom->GetUnicode());
    result=PRBool(0==cmp);
  }
  return result;
}



/**
 * Compares given unicode string to this w/o regard to case
 * @update	gess 7/27/98
 * @param   s1 is the unicode string to be compared with this
 * @param   aLength is the length of s1, not # of bytes to compare
 * @return  true if full length of both strings are equal (modulo case)
 */
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


/**
 * Compare this to given string w/o regard to case; note that we compare full strings here.
 * The optional length argument just lets us know how long the given string is.
 * If you provide a length, it is compared to length of this string as an optimization.
 * 
 * @update gess 7/27/98
 * @param  aCString -- Cstr to compare to this
 * @param  aLength -- length of given string.
 * @return TRUE if equal
 */
PRBool nsString::EqualsIgnoreCase(const char* aCString,PRInt32 aLength) const {
  NS_ASSERTION(0!=aCString,kNullPointerError);

  if((aLength>0) && (aLength!=mLength))
    return PR_FALSE;

  PRInt32 cmp=nsCRT::strcasecmp(mStr,aCString);
  return PRBool(0==cmp);
}


/**
 * 
 * @update	gess 7/27/98
 * @param 
 * @return
 */
void nsString::DebugDump(ostream& aStream) const {
  for(int i=0;i<mLength;i++) {
    aStream <<char(mStr[i]);
  }
  aStream << endl;
}

/**
 * 
 * @update	gess8/8/98
 * @param 
 * @return
 */
ostream& operator<<(ostream& os,nsString& aString){
	const PRUnichar* uc=aString.GetUnicode();
	int len=aString.Length();
	for(int i=0;i<len;i++)
		os<<(char)uc[i];
	return os;
}


//----------------------------------------------------------------------

#define INIT_AUTO_STRING()                                        \
  mLength = 0;                                                    \
  mCapacity = (sizeof(mBuf) / sizeof(chartype))-sizeof(chartype); \
  mStr = mBuf

/**
 * 
 * @update	gess 7/27/98
 * @param 
 * @return
 */
nsAutoString::nsAutoString() 
  : nsString() 
{
  INIT_AUTO_STRING();
  mStr[0] = 0;
}

/**
 * 
 * @update	gess 7/27/98
 * @param 
 * @return
 */
nsAutoString::nsAutoString(const char* aCString) 
  : nsString()
{
  INIT_AUTO_STRING();
  SetString(aCString);
}

/**
 * 
 * @update	gess 7/27/98
 * @param 
 * @return
 */
nsAutoString::nsAutoString(const nsString& other)
  : nsString()
{
  INIT_AUTO_STRING();
  SetString(other.GetUnicode(),other.Length());
}

/**
 * 
 * @update	gess 7/27/98
 * @param 
 * @return
 */
nsAutoString::nsAutoString(PRUnichar aChar) 
  : nsString()
{
  INIT_AUTO_STRING();
  Append(aChar);
}

/**
 * 
 * @update	gess 7/27/98
 * @param 
 * @return
 */
nsAutoString::nsAutoString(const nsAutoString& other) 
  : nsString()
{
  INIT_AUTO_STRING();
  SetString(other.GetUnicode(),other.mLength);
}

/**
 * 
 * @update	gess 7/27/98
 * @param 
 * @return
 */
nsAutoString::nsAutoString(const PRUnichar* unicode, PRInt32 uslen) 
  : nsString()
{
  INIT_AUTO_STRING();
  Append(unicode, uslen ? uslen : nsCRT::strlen(unicode));
}

/**
 * 
 * @update	gess 7/27/98
 * @param 
 * @return
 */
nsAutoString::~nsAutoString()
{
  if (mStr == mBuf) {
    // Force to null so that baseclass dtor doesn't do damage
    mStr = nsnull;
  }
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
      nsCRT::memcpy(temp, mStr, mLength * sizeof(chartype) + sizeof(chartype));
    }
    if ((mStr != mBuf) && (0 != mStr)) {
      delete [] mStr;
    }
    mStr = temp;
  }
}

void
nsAutoString::SizeOf(nsISizeOfHandler* aHandler) const
{
  aHandler->Add(sizeof(*this));
  if (mStr != mBuf) {
    aHandler->Add(mCapacity * sizeof(chartype));
  }
}


/**
 *  
 *  
 *  @update  gess 3/31/98
 *  @param   
 *  @return  
 */
void nsAutoString::SelfTest(){
  nsAutoString xas("Hello there");
  xas.Append("this string exceeds the max size");
  xas.DebugDump(cout);
}

/**
 * 
 * @update	gess8/8/98
 * @param 
 * @return
 */
ostream& operator<<(ostream& os,nsAutoString& aString){
	const PRUnichar* uc=aString.GetUnicode();
	int len=aString.Length();
	for(int i=0;i<len;i++)
		os<<(char)uc[i];
	return os;
}


/**
 * 
 * @update	gess 7/27/98
 * @param 
 * @return
 */
NS_BASE int fputs(const nsString& aString, FILE* out)
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
    delete [] cp;
  }
  return (int) len;
}
       

/**
 * 
 * @update	gess 7/27/98
 * @param 
 * @return
 */
void nsString::SelfTest(void) {

#ifdef  RICKG_DEBUG
	mSelfTested=PR_TRUE;
  nsAutoString a("foobar");
  nsAutoString b("foo");
  nsAutoString c(".5111");
  nsAutoString d(" 5");
  PRInt32 result=a.Compare(b);
  PRInt32 result2=result;
  result=c.ToInteger(&result2);
  result=d.ToInteger(&result2);
  result2=result;

  static const char* kConstructorError = kConstructorError;
  static const char* kComparisonError  = "Comparision error!";
  static const char* kEqualsError = "Equals error!";
  
  nsAutoString as("Hello there");
  as.SelfTest();

  static const char* temp="hello";

  //first, let's test the constructors...
  nsString empty;
  empty="";
  empty="xxx";
  empty="";

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

  const chartype* p1=a.GetUnicode();
  const chartype* p2=a; //should invoke the PRUnichar conversion operator...
  for(int i=0;i<a.Length();i++) {
    NS_ASSERTION(a[i]==temp[i],kConstructorError);         //test [] operator
    NS_ASSERTION(a.CharAt(i)==temp[i],kConstructorError);  //test charAt method
    NS_ASSERTION(a(i)==temp[i],kConstructorError);         //test (i) operator
  }
  NS_ASSERTION(a.First()==temp[0],kConstructorError);
  NS_ASSERTION(a.Last()==temp[a.Length()-1],kConstructorError);

  //**********************************************
  //Now let's test the CREATION operators...
  //**********************************************

  static const char* temp1="helloworld!";
  nsString temp2=a+b;
  nsString temp3=a+"world!";
  nsString temp4=temp2+'!';

    //let's quick check the PRUnichar operator+ method...
  const PRUnichar* uc=temp4.GetUnicode();
  nsString temp4a("Begin");
  temp4a.DebugDump(cout);
  nsString temp4b=temp4a+uc;
  temp4b.DebugDump(cout);

  temp2.DebugDump(cout);
  temp3.DebugDump(cout);
  temp4.DebugDump(cout);
  for(i=0;i<temp2.Length();i++) {
    NS_ASSERTION(temp1[i]==temp2[i],kConstructorError);         
    NS_ASSERTION(temp1[i]==temp3[i],kConstructorError);           
    NS_ASSERTION(temp1[i]==temp4[i],kConstructorError);           
  }
  NS_ASSERTION(temp4.Last()=='!',kConstructorError);
  NS_ASSERTION(temp4.Length()>temp3.Length(),kConstructorError); //should be char longer

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

  nsString  sourceString("Hello how are you"); 
  nsString  subString("you"); 
  nsString  replacementStr("xxx"); 

  PRInt32 offset = sourceString.Find(subString);  
  sourceString.Cut(offset, subString.Length());    
  sourceString.Insert(replacementStr, offset, replacementStr.Length()); // Offset isn't checked in Insert either 

  //**********************************************
  //Now let's test a few string COMPARISION ops...
  //**********************************************

  nsString temp8("aaaa");
  nsString temp8a("AAAA");
  nsString temp9("bbbb");

  const char* aaaa="aaaa";
  const char* bbbb="bbbb";

    //First test the string compare routines...

  NS_ASSERTION(0>temp8.Compare(temp9),kComparisonError);
  NS_ASSERTION(0<temp9.Compare(temp8),kComparisonError);
  NS_ASSERTION(0==temp8.Compare(temp8a,PR_TRUE),kComparisonError);
  NS_ASSERTION(0==temp8.Compare(aaaa),kComparisonError);

    //Now test the boolean operators...
  NS_ASSERTION(temp8==temp8,kComparisonError);
  NS_ASSERTION(temp8==aaaa,kComparisonError);

  NS_ASSERTION(temp8!=temp9,kComparisonError);
  NS_ASSERTION(temp8!=bbbb,kComparisonError);

  NS_ASSERTION(temp8<temp9,kComparisonError);
  NS_ASSERTION(temp8<bbbb,kComparisonError);

  NS_ASSERTION(temp9>temp8,kComparisonError);
  NS_ASSERTION(temp9>aaaa,kComparisonError);

  NS_ASSERTION(temp8<=temp8,kComparisonError);
  NS_ASSERTION(temp8<=temp9,kComparisonError);
  NS_ASSERTION(temp8<=bbbb,kComparisonError);

  NS_ASSERTION(temp9>=temp9,kComparisonError);
  NS_ASSERTION(temp9>=temp8,kComparisonError);
  NS_ASSERTION(temp9>=aaaa,kComparisonError);

  NS_ASSERTION(temp8.Equals(temp8),kEqualsError);
  NS_ASSERTION(temp8.Equals(aaaa),kEqualsError);
  
  nsString temp10(temp8);
  temp10.ToUpperCase();
  NS_ASSERTION(temp8.EqualsIgnoreCase(temp10),kEqualsError);
  NS_ASSERTION(temp8.EqualsIgnoreCase("AAAA"),kEqualsError);


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

  const PRUnichar* uni=temp4.GetUnicode();
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

  pos=temp15.FindCharInSet("12k");
  NS_ASSERTION(pos==10,"Error: FindFirstInChar routine");

  pos=temp15.RFindCharInSet("12k");
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
