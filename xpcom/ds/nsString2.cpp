
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
#include "nsString2.h"
#include "nsDebug.h"
#include "nsCRT.h"
#include "nsIAtom.h"
#include "nsISizeOfHandler.h"
#include "prprf.h"
#include "prdtoa.h"

#include "nsUnicharUtilCIID.h"
#include "nsIServiceManager.h"
#include "nsICaseConversion.h"

static const char* kNullPointerError = "Error: unexpected null ptr";
static const char* kWhitespace="\b\t\r\n ";
static const PRInt32  kNotFound=-1;

#ifdef  NS_DEBUG
PRBool nsString2::mSelfTested = PR_FALSE;   
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

class HandleCaseConversionShutdown3 : public nsIShutdownListener {
public :
   NS_IMETHOD OnShutdown(const nsCID& cid, nsISupports* service);
   HandleCaseConversionShutdown3(void) { NS_INIT_REFCNT(); }
   virtual ~HandleCaseConversionShutdown3(void) {}
   NS_DECL_ISUPPORTS
};
static NS_DEFINE_CID(kUnicharUtilCID, NS_UNICHARUTIL_CID);
static NS_DEFINE_IID(kICaseConversionIID, NS_ICASECONVERSION_IID);

static nsICaseConversion * gCaseConv = NULL; 

static NS_DEFINE_IID(kIShutdownListenerIID, NS_ISHUTDOWNLISTENER_IID);
NS_IMPL_ISUPPORTS(HandleCaseConversionShutdown3, kIShutdownListenerIID);

nsresult
HandleCaseConversionShutdown3::OnShutdown(const nsCID& cid, nsISupports* service)
{
    if (cid.Equals(kUnicharUtilCID)) {
        NS_ASSERTION(service == gCaseConv, "wrong service!");
        gCaseConv->Release();
        gCaseConv = NULL;
    }
    return NS_OK;
}

static HandleCaseConversionShutdown3* gListener = NULL;

static void StartUpCaseConversion()
{
    nsresult err;

    if ( NULL == gListener )
    {
      gListener = new HandleCaseConversionShutdown3();
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
nsString2::nsString2(eCharSize aCharSize,nsIMemoryAgent* anAgent) : mAgent(anAgent) {
  nsStr::Initialize(*this,aCharSize);

#ifdef NS_DEBUG
  if(!mSelfTested) {
    mSelfTested=PR_TRUE;
		SelfTest();
  }
#endif
}

/**
 * This constructor accepts an ascii string
 * @update	gess 1/4/99
 * @param   aCString is a ptr to a 1-byte cstr
 */
nsString2::nsString2(const char* aCString,eCharSize aCharSize,nsIMemoryAgent* anAgent) : mAgent(anAgent) {  
  nsStr::Initialize(*this,aCharSize);
  Assign(aCString);
}

/**
 * This constructor accepts an ascii string
 * @update	gess 1/4/99
 * @param   aCString is a ptr to a 1-byte cstr
 */
nsString2::nsString2(const PRUnichar* aString,eCharSize aCharSize,nsIMemoryAgent* anAgent) : mAgent(anAgent) {  
  nsStr::Initialize(*this,aCharSize);
  Assign(aString);
}

/**
 * This is our copy constructor 
 * @update	gess 1/4/99
 * @param   reference to another nsString2
 */
nsString2::nsString2(const nsStr &aString,eCharSize aCharSize,nsIMemoryAgent* anAgent) : mAgent(anAgent) {
  nsStr::Initialize(*this,aCharSize);
  nsStr::Assign(*this,aString,0,aString.mLength,mAgent);
}

/**
 * This is our copy constructor 
 * @update	gess 1/4/99
 * @param   reference to another nsString2
 */
nsString2::nsString2(const nsString2& aString) :mAgent(aString.mAgent) {
  nsStr::Initialize(*this,(eCharSize)aString.mMultibyte);
  nsStr::Assign(*this,aString,0,aString.mLength,mAgent);
}

/**
 * Destructor
 * Make sure we call nsStr::Destroy.
 */
nsString2::~nsString2() {
  nsStr::Destroy(*this,mAgent);
}

void nsString2::SizeOf(nsISizeOfHandler* aHandler) const {
  aHandler->Add(sizeof(*this));
  aHandler->Add(mCapacity << mMultibyte);
}

/**
 * This method truncates this string to given length.
 *
 * @update	gess 01/04/99
 * @param   anIndex -- new length of string
 * @return  nada
 */
void nsString2::Truncate(PRInt32 anIndex) {
  nsStr::Truncate(*this,anIndex,mAgent);
}

/**
 *  Determine whether or not the characters in this
 *  string are in sorted order.
 *  
 *  @update  gess 8/25/98
 *  @return  TRUE if ordered.
 */
PRBool nsString2::IsOrdered(void) const {
  PRBool  result=PR_TRUE;
  if(mLength>1) {
    PRUint32 theIndex;
    PRUnichar c1=0;
    PRUnichar c2=nsStr::GetCharAt(*this,0);
    for(theIndex=1;theIndex<mLength;theIndex++) {
      c1=c2;
      c2=nsStr::GetCharAt(*this,theIndex);
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
void nsString2::SetCapacity(PRUint32 aLength) {
  if(aLength>mLength) {
    GrowCapacity(*this,aLength,mAgent);
  }
  mLength=aLength;
  AddNullTerminator(*this);
}

/**********************************************************************
  Accessor methods...
 *********************************************************************/


//static char gChar=0;

/**
 * 
 * @update	gess1/4/99
 * @param 
 * @return
 */
char* nsString2::GetBuffer(void) const {
  if(!mMultibyte)
    return mStr.mCharBuf;
  return 0;
}

/**
 * 
 * @update	gess1/4/99
 * @param 
 * @return
 */
PRUnichar* nsString2::GetUnicode(void) const {
  if(mMultibyte)
    return (PRUnichar*)mStr.mUnicharBuf;
  return 0;
}

/**
 * Get nth character.
 */
PRUnichar nsString2::operator[](int anIndex) const {
  return nsStr::GetCharAt(*this,anIndex);
}

PRUnichar nsString2::CharAt(int anIndex) const {
  return nsStr::GetCharAt(*this,anIndex);
}

PRUnichar nsString2::First(void) const{
  return nsStr::GetCharAt(*this,0);
}

PRUnichar nsString2::Last(void) const{
  return nsStr::GetCharAt(*this,mLength-1);
}

PRBool nsString2::SetCharAt(PRUnichar aChar,PRUint32 anIndex){
  PRBool result=PR_FALSE;
  if(anIndex<mLength){
    if(!mMultibyte)
      mStr.mCharBuf[anIndex]=char(aChar);
    else mStr.mUnicharBuf[anIndex]=aChar;
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
nsString2 nsString2::operator+(const nsStr& aString){
  nsString2 temp(*this); //make a temp string the same size as this...
  nsStr::Append(temp,aString,0,aString.mLength,mAgent);
  return temp;
}

/**
 * Create a new string by appending given string to this
 * @update	gess 01/04/99
 * @param   aString -- 2nd string to be appended
 * @return  new string
 */
nsString2 nsString2::operator+(const nsString2& aString){
  nsString2 temp(*this); //make a temp string the same size as this...
  nsStr::Append(temp,aString,0,aString.mLength,mAgent);
  return temp;
}


/**
 * create a new string by adding this to the given buffer.
 * @update	gess 01/04/99
 * @param   aCString is a ptr to cstring to be added to this
 * @return  newly created string
 */
nsString2 nsString2::operator+(const char* aCString) {
  nsString2 temp(*this);
  temp.Append(aCString);
  return temp;
}


/**
 * create a new string by adding this to the given char.
 * @update	gess 01/04/99
 * @param   aChar is a char to be added to this
 * @return  newly created string
 */
nsString2 nsString2::operator+(char aChar) {
  nsString2 temp(*this);
  temp.Append(char(aChar));
  return temp;
}

/**
 * create a new string by adding this to the given buffer.
 * @update	gess 01/04/99
 * @param   aString is a ptr to unistring to be added to this
 * @return  newly created string
 */
nsString2 nsString2::operator+(const PRUnichar* aString) {
  nsString2 temp(*this);
  temp.Append(aString);
  return temp;
}


/**
 * create a new string by adding this to the given char.
 * @update	gess 01/04/99
 * @param   aChar is a unichar to be added to this
 * @return  newly created string
 */
nsString2 nsString2::operator+(PRUnichar aChar) {
  nsString2 temp(*this);
  temp.Append(char(aChar));
  return temp;
}

/**********************************************************************
  Lexomorphic transforms...
 *********************************************************************/

/**
 * Converts all chars in given string to UCS2
 */
void nsString2::ToUCS2(PRUint32 aStartOffset){
  if(aStartOffset<mLength){
    if(mMultibyte) {
      PRUint32 theIndex=0;
      for(theIndex=aStartOffset;theIndex<mLength;theIndex++){
        unsigned char ch = (unsigned char)mStr.mUnicharBuf[theIndex];
        if( 0x0080 == (0xFFE0 & (mStr.mUnicharBuf[theIndex])) ) // limit to only 0x0080 to 0x009F
          mStr.mUnicharBuf[theIndex]=gToUCS2[ch];
      }
    }
  }
}

/**
 * Converts all chars in internal string to lower
 * @update	gess 01/04/99
 */
void nsString2::ToLowerCase() {
  nsStr::ChangeCase(*this,PR_FALSE);
}

/**
 * Converts all chars in internal string to upper
 * @update	gess 01/04/99
 */
void nsString2::ToUpperCase() {
  nsStr::ChangeCase(*this,PR_TRUE);
}

/**
 * Converts chars in this to lowercase, and
 * stores them in aString
 * @update	gess 01/04/99
 * @param   aOut is a string to contain result
 */
void nsString2::ToLowerCase(nsString2& aString) const {
  aString=*this;
  nsStr::ChangeCase(aString,PR_FALSE);
}

/**
 * Converts chars in this to lowercase, and
 * stores them in a given output string
 * @update	gess 01/04/99
 * @param   aOut is a string to contain result
 */
void nsString2::ToUpperCase(nsString2& aString) const {
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
nsString2& nsString2::StripChars(const char* aSet){
  if(aSet){
    nsStr::StripChars(*this,0,mLength,aSet);
  }
  return *this;
}


/**
 *  This method strips whitespace throughout the string
 *  
 *  @update  gess 01/04/99
 *  @return  this
 */
nsString2& nsString2::StripWhitespace() {
  nsStr::StripChars(*this,0,mLength,kWhitespace);
  return *this;
}

/**
 *  This method is used to replace all occurances of the
 *  given source char with the given dest char
 *  
 *  @param  
 *  @return *this 
 */
nsString2& nsString2::ReplaceChar(PRUnichar aSourceChar, PRUnichar aDestChar) {
  PRUint32 theIndex=0;
  for(theIndex=0;theIndex<mLength;theIndex++){
    if(mMultibyte) {
      if(mStr.mUnicharBuf[theIndex]==aSourceChar)
        mStr.mUnicharBuf[theIndex]=aDestChar;
    }
    else {
      if(mStr.mCharBuf[theIndex]==aSourceChar)
        mStr.mCharBuf[theIndex]=(char)aDestChar;
    }
  }
  return *this;
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
nsString2& nsString2::Trim(const char* aTrimSet, PRBool aEliminateLeading,PRBool aEliminateTrailing){
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
nsString2& nsString2::CompressSet(const char* aSet, char aChar, PRBool aEliminateLeading,PRBool aEliminateTrailing){
  if(aSet){
    nsStr::CompressSet(*this,aSet,aChar,aEliminateLeading,aEliminateTrailing);
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
nsString2& nsString2::CompressWhitespace( PRBool aEliminateLeading,PRBool aEliminateTrailing){
  nsStr::CompressSet(*this,kWhitespace,' ',aEliminateLeading,aEliminateTrailing);
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
nsString2* nsString2::ToNewString() const {
  return new nsString2(*this);
}

/**
 * Creates an ascii clone of this string
 * @update	gess 01/04/99
 * @return  ptr to new ascii string
 */
char* nsString2::ToNewCString() const {
  nsString2 temp(*this,eOneByte);
  char* result=temp.mStr.mCharBuf;
  temp.mStr.mCharBuf=0;
  return result;
}

/**
 * Creates an ascii clone of this string
 * @update	gess 01/04/99
 * @return  ptr to new ascii string
 */
PRUnichar* nsString2::ToNewUnicode() const {
  nsString2 temp(*this,eTwoByte);
  PRUnichar* result=temp.mStr.mUnicharBuf;
  temp.mStr.mUnicharBuf=0; 
  return result;
}

/**
 * Copies contents of this string into he given buffer
 * @update	gess 01/04/99
 * @param 
 * @return
 */
char* nsString2::ToCString(char* aBuf, PRUint32 aBufLength) const{
  if(aBuf) {
    nsStr theTempStr;
    nsStr::Initialize(theTempStr,eOneByte);
    theTempStr.mStr.mCharBuf=aBuf;
    theTempStr.mCapacity=aBufLength;
    nsStr::Assign(theTempStr,*this,0,mLength,mAgent);
  }
  return aBuf;
}

/**
 * Perform string to float conversion.
 * @update	gess 01/04/99
 * @param   aErrorCode will contain error if one occurs
 * @return  float rep of string value
 */
float nsString2::ToFloat(PRInt32* aErrorCode) const {
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
 * @update	gess 10/01/98
 * @param   aErrorCode will contain error if one occurs
 * @param   aRadix tells us what base to expect the string in.
 * @return  int rep of string value
 */
PRInt32 nsString2::ToInteger(PRInt32* anErrorCode,PRUint32 aRadix) const {

  //copy chars to local buffer -- step down from 2 bytes to 1 if necessary...
  PRInt32 result=0;

  nsAutoString2 theString(*this,eOneByte);

  PRInt32   decPt=theString.FindChar(theString,'.',PR_TRUE,0);
  char*     cp = (kNotFound==decPt) ? theString.mStr.mCharBuf + theString.mLength-1 : theString.mStr.mCharBuf+decPt-1;
  char      digit=0;
  char      theChar;
//  PRInt32   theShift=0;
  PRInt32   theMult=1;

  *anErrorCode = (0<theString.mLength) ? NS_OK : NS_ERROR_ILLEGAL_VALUE;

  // Skip trailing non-numeric...
  while (cp >= theString.mStr.mCharBuf) {
    theChar = toupper(*cp);
    if((theChar>='0') && (theChar<='9')){
      break;
    }
    else if((theChar>='A') && (theChar<='F')) {
      break;
    }
    cp--;
  }

    //now iterate the numeric chars and build our result
  while(cp>=theString.mStr.mCharBuf) {
    theChar=toupper(*cp--);
    if((theChar>='0') && (theChar<='9')){
      digit=theChar-'0';
    }
    else if((theChar>='A') && (theChar<='F')) {
      digit=(theChar-'A')+10;
    }
    else if('-'==theChar) {
      result=-result;
      break;
    }
    else if(('+'==theChar) || (' '==theChar)) { //stop in a good state if you see this...
      break;
    }
    else if(('X'==theChar) && (16==aRadix)) {  
      //stop in a good state.
      break;
    }
    else{
      *anErrorCode=NS_ERROR_ILLEGAL_VALUE;
      result=0;
      break;
    }
    result+=digit*theMult;
    theMult*=aRadix;
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
nsString2& nsString2::Assign(const nsStr& aString,PRInt32 aCount) {
  if(-1==aCount) aCount=aString.mLength;
  nsStr::Assign(*this,aString,0,aCount,mAgent);
  return *this;
}

/**
 * assign given string to this one
 * @update	gess 01/04/99
 * @param   aString: string to be added to this
 * @return  this
 */
nsString2& nsString2::Assign(const nsString2& aString,PRInt32 aCount) {
  if(-1==aCount) aCount=aString.mLength;
  nsStr::Assign(*this,aString,0,aCount,mAgent);
  return *this;
}

/**
 * assign given char* to this string
 * @update	gess 01/04/99
 * @param   aCString: buffer to be assigned to this 
 * @return  this
 */
nsString2& nsString2::Assign(const char* aCString,PRInt32 aCount) {
  if(aCString){
    if(-1==aCount) aCount=nsCRT::strlen(aCString);
    nsStr::Truncate(*this,0,0);
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
nsString2& nsString2::Assign(const PRUnichar* aString,PRInt32 aCount) {
  if(aString){
    if(-1==aCount) aCount=nsCRT::strlen(aString);
    nsStr::Truncate(*this,0,0);
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
nsString2& nsString2::Assign(char aChar) {
  nsStr::Truncate(*this,0,0);
  return Append(aChar);
}

/**
 * assign given char to this string
 * @update	gess 01/04/99
 * @param   aChar: char to be assignd to this
 * @return  this
 */
nsString2& nsString2::Assign(PRUnichar aChar) {
  nsStr::Truncate(*this,0,0);
  return Append(aChar);
}

/**
 * Copies contents of this onto given string.
 * @update	gess 7/27/98
 * @param   aString to hold copy of this
 * @return  nada.
 */
void nsString2::Copy(nsString2& aString) const{
  aString.SetString(*this);
}

/**
 * append given string to this string
 * @update	gess 01/04/99
 * @param   aString : string to be appended to this
 * @return  this
 */
nsString2& nsString2::Append(const nsStr& aString,PRInt32 aCount) {
  if(-1==aCount) aCount=aString.mLength;
  nsStr::Append(*this,aString,0,aCount,mAgent);
  return *this;
}


/**
 * append given string to this string
 * @update	gess 01/04/99
 * @param   aString : string to be appended to this
 * @return  this
 */
nsString2& nsString2::Append(const nsString2& aString,PRInt32 aCount) {
  if(-1==aCount) aCount=aString.mLength;
  nsStr::Append(*this,aString,0,aCount,mAgent);
  return *this;
}

/**
 * append given string to this string
 * @update	gess 01/04/99
 * @param   aString : string to be appended to this
 * @param   aCount: #of chars to be copied
 * @return  this
 */
nsString2& nsString2::Append(const char* aCString,PRInt32 aCount) {
  if(aCString){
    nsStr theTemp;
    Initialize(theTemp,eOneByte);
    theTemp.mStr.mCharBuf=(char*)aCString;
    theTemp.mLength=nsCRT::strlen(aCString);
    if(-1==aCount) aCount=theTemp.mLength;
    nsStr::Append(*this,theTemp,0,aCount,mAgent);
  }
  return *this;
}

/**
 * append given uni-string to this string
 * @update	gess 01/04/99
 * @param   aString : string to be appended to this
 * @param   aCount: #of chars to be copied
 * @return  this
 */
nsString2& nsString2::Append(const PRUnichar* aString,PRInt32 aCount) {
  if(aString){
    nsStr theTemp;
    Initialize(theTemp,eTwoByte);
    theTemp.mStr.mUnicharBuf=(PRUnichar*)aString;
    theTemp.mLength=nsCRT::strlen(aString);
    if(-1==aCount) aCount=theTemp.mLength;
    nsStr::Append(*this,theTemp,0,aCount,mAgent);
  }
  return *this;
}

/**
 * append given string to this string
 * @update	gess 01/04/99
 * @param   aString : string to be appended to this
 * @return  this
 */
nsString2& nsString2::Append(char aChar) {
  char buf[2]={0,0};
  buf[0]=aChar;

  nsStr theTemp;
  Initialize(theTemp,eOneByte);
  theTemp.mStr.mCharBuf=buf;
  theTemp.mLength=1;
  nsStr::Append(*this,theTemp,0,1,mAgent);
  return *this;
}

/**
 * append given string to this string
 * @update	gess 01/04/99
 * @param   aString : string to be appended to this
 * @return  this
 */
nsString2& nsString2::Append(PRUnichar aChar) {
  PRUnichar buf[2]={0,0};
  buf[0]=aChar;

  nsStr theTemp;
  Initialize(theTemp,eTwoByte);
  theTemp.mStr.mUnicharBuf=buf;
  theTemp.mLength=1;
  nsStr::Append(*this,theTemp,0,1,mAgent);
  return *this;
}

/**
 * 
 * @update	gess 01/04/99
 * @param 
 * @return
 */
nsString2& nsString2::Append(PRInt32 aInteger,PRInt32 aRadix) {
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
nsString2& nsString2::Append(float aFloat){
  char buf[40];
  // *** XX UNCOMMENT THIS LINE
  //PR_snprintf(buf, sizeof(buf), "%g", aFloat);
  sprintf(buf,"%g",aFloat);
  return Append(buf);
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
PRUint32 nsString2::Left(nsString2& aDest,PRInt32 aCount) const{
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
PRUint32 nsString2::Mid(nsString2& aDest,PRUint32 anOffset,PRInt32 aCount) const{
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
PRUint32 nsString2::Right(nsString2& aCopy,PRInt32 aCount) const{
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
nsString2& nsString2::Insert(nsString2& aCopy,PRUint32 anOffset,PRInt32 aCount) {
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
 * @return  the number of chars inserted into this string
 */
nsString2& nsString2::Insert(const char* aCString,PRUint32 anOffset,PRInt32 aCount){
  if(aCString){
    if(0<aCount) {
      nsStr theTemp;
      nsStr::Initialize(theTemp,eOneByte);
      theTemp.mStr.mCharBuf=(char*)aCString;
      theTemp.mLength=nsCRT::strlen(aCString);
      if(theTemp.mLength){
        nsStr::Insert(*this,anOffset,theTemp,0,aCount,0);
      }
    }
  }
  return *this;  
}


/**
 * Insert a single char into this string at
 * a specified offset.
 *
 * @update	gess4/22/98
 * @param   aChar char to be inserted into this string
 * @param   anOffset is insert pos in str 
 * @return  the number of chars inserted into this string
 */
/*
nsString2& nsString2::Insert(char aChar,PRUint32 anOffset){
  char theBuffer[2]={0,0};
  theBuffer[0]=aChar;
  nsStr theTempStr;
  nsStr::Initialize(theTempStr,eOneByte);
  theTempStr.mStr=(char*)theBuffer;
  theTempStr.mLength=1;
  nsStr::Insert(*this,anOffset,theTempStr,0,1,0);
  return *this;
}
*/

/**
 * Insert a unicode* into this string at
 * a specified offset.
 *
 * @update	gess4/22/98
 * @param   aChar char to be inserted into this string
 * @param   anOffset is insert pos in str 
 * @return  the number of chars inserted into this string
 */
nsString2& nsString2::Insert(const PRUnichar* aString,PRUint32 anOffset,PRInt32 aCount){
  if(aString){
    if(0<aCount) {
      nsStr theTemp;
      nsStr::Initialize(theTemp,eTwoByte);
      theTemp.mStr.mUnicharBuf=(PRUnichar*)aString;
      theTemp.mLength=nsCRT::strlen(aString);
      if(theTemp.mLength){
        nsStr::Insert(*this,anOffset,theTemp,0,aCount,0);
      }
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
nsString2& nsString2::Insert(PRUnichar aChar,PRUint32 anOffset){
  PRUnichar theBuffer[2]={0,0};
  theBuffer[0]=aChar;
  nsStr theTempStr;
  nsStr::Initialize(theTempStr,eTwoByte);
  theTempStr.mStr.mUnicharBuf=theBuffer;
  theTempStr.mLength=1;
  nsStr::Insert(*this,anOffset,theTempStr,0,1,0);
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
nsString2& nsString2::Cut(PRUint32 anOffset, PRInt32 aCount) {
  nsStr::Delete(*this,anOffset,aCount,mAgent);
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
PRInt32 nsString2::BinarySearch(PRUnichar aChar) const{
  PRInt32 low=0;
  PRInt32 high=mLength-1;

  while (low <= high) {
    int middle = (low + high) >> 1;
    PRUnichar theChar=nsStr::GetCharAt(*this,middle);
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
PRInt32 nsString2::Find(const char* aCString,PRBool aIgnoreCase) const{
  NS_ASSERTION(0!=aCString,kNullPointerError);

  PRInt32 result=kNotFound;
  if(aCString) {
    nsStr theTempStr;
    nsStr::Initialize(theTempStr,eOneByte);
    theTempStr.mLength=nsCRT::strlen(aCString);
    theTempStr.mStr.mCharBuf=(char*)aCString;
    result=nsStr::FindSubstr(*this,theTempStr,aIgnoreCase,0);
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
PRInt32 nsString2::Find(const PRUnichar* aString,PRBool aIgnoreCase) const{
  NS_ASSERTION(0!=aString,kNullPointerError);

  PRInt32 result=kNotFound;
  if(aString) {
    nsStr theTempStr;
    nsStr::Initialize(theTempStr,eTwoByte);
    theTempStr.mLength=nsCRT::strlen(aString);
    theTempStr.mStr.mUnicharBuf=(PRUnichar*)aString;
    result=nsStr::FindSubstr(*this,theTempStr,aIgnoreCase,0);
  }
  return result;
}

/**
 *  Search for given buffer within this string
 *  
 *  @update  gess 3/25/98
 *  @param   nsString2 -- buffer to be found
 *  @return  offset in string, or -1 (kNotFound)
 */
PRInt32 nsString2::Find(const nsStr& aString,PRBool aIgnoreCase) const{
  PRInt32 result=nsStr::FindSubstr(*this,aString,aIgnoreCase,0);
  return result;
}

/**
 *  Search for a given char, starting at given offset
 *  
 *  @update  gess 3/25/98
 *  @param   
 *  @return  offset of found char, or -1 (kNotFound)
 */
PRInt32 nsString2::Find(PRUnichar aChar,PRBool aIgnoreCase, PRUint32 anOffset) const{
  PRInt32 result=nsStr::FindChar(*this,aChar,aIgnoreCase,0);
  return result;
}

/**
 *  
 *  
 *  @update  gess 3/25/98
 *  @param   
 *  @return  
 */
PRInt32 nsString2::FindCharInSet(const char* aCStringSet,PRUint32 anOffset) const{
  NS_ASSERTION(0!=aCStringSet,kNullPointerError);

  PRInt32 result=kNotFound;
  if(aCStringSet) {
    nsStr theTempStr;
    nsStr::Initialize(theTempStr,eOneByte);
    theTempStr.mLength=nsCRT::strlen(aCStringSet);
    theTempStr.mStr.mCharBuf=(char*)aCStringSet;
    result=nsStr::FindCharInSet(*this,theTempStr,PR_FALSE,anOffset);
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
PRInt32 nsString2::FindCharInSet(const nsString2& aSet,PRUint32 anOffset) const{
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
PRInt32 nsString2::RFindCharInSet(const char* aCStringSet,PRUint32 anOffset) const{
  NS_ASSERTION(0!=aCStringSet,kNullPointerError);

  PRInt32 result=kNotFound;
  if(aCStringSet) {
    nsStr theTempStr;
    nsStr::Initialize(theTempStr,eOneByte);
    theTempStr.mLength=nsCRT::strlen(aCStringSet);
    theTempStr.mStr.mCharBuf=(char*)aCStringSet;
    result=nsStr::RFindCharInSet(*this,theTempStr,PR_FALSE,anOffset);
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
PRInt32 nsString2::RFindCharInSet(const nsString2& aSet,PRUint32 anOffset) const{
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
PRInt32 nsString2::RFind(const nsStr& aString,PRBool aIgnoreCase) const{
  PRInt32 result=nsStr::RFindSubstr(*this,aString,aIgnoreCase,0);
  return result;
}

/**
 *  
 *  
 *  @update  gess 3/25/98
 *  @param   
 *  @return  
 */
PRInt32 nsString2::RFind(const char* aString,PRBool aIgnoreCase) const{
  NS_ASSERTION(0!=aString,kNullPointerError);

  PRInt32 result=kNotFound;
  if(aString) {
    nsStr theTempStr;
    nsStr::Initialize(theTempStr,eOneByte);
    theTempStr.mLength=nsCRT::strlen(aString);
    theTempStr.mStr.mCharBuf=(char*)aString;
    result=nsStr::RFindSubstr(*this,theTempStr,aIgnoreCase,0);
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
PRInt32 nsString2::RFind(PRUnichar aChar,PRBool aIgnoreCase, PRUint32 anOffset) const{
  PRInt32 result=nsStr::RFindChar(*this,aChar,aIgnoreCase,anOffset);
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
 * @return  -1,0,1
 */
PRInt32 nsString2::Compare(const char *aCString,PRBool aIgnoreCase,PRInt32 aLength) const {
  NS_ASSERTION(0!=aCString,kNullPointerError);

  if(aCString) {
    nsStr theTempStr;
    nsStr::Initialize(theTempStr,eOneByte);
    theTempStr.mLength=nsCRT::strlen(aCString);
    theTempStr.mStr.mCharBuf=(char*)aCString;
    return nsStr::Compare(*this,theTempStr,aLength,aIgnoreCase);
  }
  return 0;
}

/**
 * Compares given unistring to this string. 
 * @update	gess 01/04/99
 * @param   aString pts to a uni-string
 * @param   aIgnoreCase tells us how to treat case
 * @return  -1,0,1
 */
PRInt32 nsString2::Compare(const PRUnichar* aString,PRBool aIgnoreCase,PRInt32 aLength) const {
  NS_ASSERTION(0!=aString,kNullPointerError);

  if(aString) {
    nsStr theTempStr;
    nsStr::Initialize(theTempStr,eTwoByte);
    theTempStr.mLength=nsCRT::strlen(aString);
    theTempStr.mStr.mUnicharBuf=(PRUnichar*)aString;
    return nsStr::Compare(*this,theTempStr,aLength,aIgnoreCase);
  }
  return 0;
}

/**
 * LAST MODS:	gess
 * 
 * @param  
 * @return 
 */
PRInt32 nsString2::Compare(const nsStr& aString,PRBool aIgnoreCase,PRInt32 aLength) const {
  return nsStr::Compare(*this,aString,aLength,aIgnoreCase);
}


PRBool nsString2::operator==(const nsStr &S) const {return Equals(S);}      
PRBool nsString2::operator==(const char* s) const {return Equals(s);}
PRBool nsString2::operator==(const PRUnichar* s) const {return Equals(s);}

PRBool nsString2::operator!=(const nsStr &S) const {return PRBool(Compare(S)!=0);}
PRBool nsString2::operator!=(const char* s) const {return PRBool(Compare(s)!=0);}
PRBool nsString2::operator!=(const PRUnichar* s) const {return PRBool(Compare(s)!=0);}

PRBool nsString2::operator<(const nsStr &S) const {return PRBool(Compare(S)<0);}
PRBool nsString2::operator<(const char* s) const {return PRBool(Compare(s)<0);}
PRBool nsString2::operator<(const PRUnichar* s) const {return PRBool(Compare(s)<0);}

PRBool nsString2::operator>(const nsStr &S) const {return PRBool(Compare(S)>0);}
PRBool nsString2::operator>(const char* s) const {return PRBool(Compare(s)>0);}
PRBool nsString2::operator>(const PRUnichar* s) const {return PRBool(Compare(s)>0);}

PRBool nsString2::operator<=(const nsStr &S) const {return PRBool(Compare(S)<=0);}
PRBool nsString2::operator<=(const char* s) const {return PRBool(Compare(s)<=0);}
PRBool nsString2::operator<=(const PRUnichar* s) const {return PRBool(Compare(s)<=0);}

PRBool nsString2::operator>=(const nsStr &S) const {return PRBool(Compare(S)>=0);}
PRBool nsString2::operator>=(const char* s) const {return PRBool(Compare(s)>=0);}
PRBool nsString2::operator>=(const PRUnichar* s) const {return PRBool(Compare(s)>=0);}


PRBool nsString2::EqualsIgnoreCase(const nsString2& aString) const {
  return Equals(aString,PR_TRUE);
}

PRBool nsString2::EqualsIgnoreCase(const char* aString,PRInt32 aLength) const {
  return Equals(aString,aLength,PR_TRUE);
}

PRBool nsString2::EqualsIgnoreCase(const nsIAtom *aAtom) const {
  return Equals(aAtom,PR_TRUE);
}

PRBool nsString2::EqualsIgnoreCase(const PRUnichar* s1, const PRUnichar* s2) const {
  return Equals(s1,s2,PR_TRUE);
}

/**
 * Compare this to given string; note that we compare full strings here.
 * 
 * @update gess 01/04/99
 * @param  aString is the other nsString2 to be compared to
 * @return TRUE if equal
 */
PRBool nsString2::Equals(const nsStr& aString,PRBool aIgnoreCase) const {
  PRInt32 result=nsStr::Compare(*this,aString,MinInt(mLength,aString.mLength),aIgnoreCase);
  return PRBool(0==result);
}

PRBool nsString2::Equals(const char* aString,PRBool aIgnoreCase) const {
  if(aString) {
    return Equals(aString,nsCRT::strlen(aString),aIgnoreCase);
  } 
  return 0;
}

/**
 * Compare this to given string; note that we compare full strings here.
 * The optional length argument just lets us know how long the given string is.
 * If you provide a length, it is compared to length of this string as an
 * optimization.
 * 
 * @update gess 01/04/99
 * @param  aCString -- Cstr to compare to this
 * @param  aLength -- length of given string.
 * @return TRUE if equal
 */
PRBool nsString2::Equals(const char* aCString,PRUint32 aCount,PRBool aIgnoreCase) const{
  NS_ASSERTION(0!=aCString,kNullPointerError);
  PRBool result=PR_FALSE;
  if(aCString) {
    PRInt32 theAnswer=Compare(aCString,aIgnoreCase,aCount);
    result=PRBool(0==theAnswer);
  }
  return result;
}

PRBool nsString2::Equals(const PRUnichar* aString,PRBool aIgnoreCase) const {
  NS_ASSERTION(0!=aString,kNullPointerError);
  PRBool result=PR_FALSE;
  if(aString) {
    result=Equals(aString,nsCRT::strlen(aString),aIgnoreCase);
  } 
  return result;
}


/**
 * Compare this to given string; note that we compare full strings here.
 * The optional length argument just lets us know how long the given string is.
 * If you provide a length, it is compared to length of this string as an
 * optimization.
 * 
 * @update gess 01/04/99
 * @param  aString -- unistring to compare to this
 * @param  aLength -- length of given string.
 * @return TRUE if equal
 */
PRBool nsString2::Equals(const PRUnichar* aString,PRUint32 aCount,PRBool aIgnoreCase) const{
  NS_ASSERTION(0!=aString,kNullPointerError);
  PRBool result=PR_FALSE;
  if(aString){
    PRInt32 theAnswer=Compare(aString,aIgnoreCase,aCount);
    result=PRBool(0==theAnswer);
  }
  return result;
}

/**
 * Compare this to given string; note that we compare full strings here.
 * The optional length argument just lets us know how long the given string is.
 * If you provide a length, it is compared to length of this string as an
 * optimization.
 * 
 * @update gess 01/04/99
 * @param  aString -- unistring to compare to this
 * @param  aLength -- length of given string.
 * @return TRUE if equal
 */
PRBool nsString2::Equals(const nsIAtom* aAtom,PRBool aIgnoreCase) const{
  NS_ASSERTION(0!=aAtom,kNullPointerError);
  PRBool result=PR_FALSE;
  if(aAtom){
    PRInt32 cmp=nsCRT::strcasecmp(mStr.mUnicharBuf,aAtom->GetUnicode());
    result=PRBool(0==cmp);
  }
  return result;
}

/**
 * Compare given strings
 * @update gess 7/27/98
 * @param  s1 -- first string to be compared
 * @param  s2 -- second string to be compared
 * @return TRUE if equal
 */
PRBool nsString2::Equals(const PRUnichar* s1, const PRUnichar* s2,PRBool aIgnoreCase) const {
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
PRBool nsString2::IsAlpha(PRUnichar aChar) {
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
PRBool nsString2::IsSpace(PRUnichar aChar) {
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
 */
PRBool nsString2::IsDigit(PRUnichar aChar) {
  // XXX i18n
  return PRBool((aChar >= '0') && (aChar <= '9'));
}


/**
 * 
 * @update	gess 01/04/99
 * @param 
 * @return
 */
void nsString2::DebugDump(ostream& aStream) const {
  for(PRUint32 i=0;i<mLength;i++) {
    aStream <<mStr.mCharBuf[i];
  }
  aStream << endl;
}

/**
 * 
 * @update	gess8/8/98
 * @param 
 * @return
 */
ostream& operator<<(ostream& os,nsString2& aString){
  if(PR_FALSE==aString.mMultibyte) {
    os<<aString.mStr.mCharBuf;
  }
  else{
    char* theStr=aString.ToNewCString();
    os<<theStr;
    delete [] theStr;
  }    
	return os;
}


/**
 * 
 * @update	gess 01/04/99
 * @param 
 * @return
 */
NS_BASE int fputs(const nsString2& aString, FILE* out)
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
    delete cp;
  }
  return (int) len;
}
       

/**
 * 
 * @update	gess 01/04/99
 * @param 
 * @return
 */
void nsString2::SelfTest(void) {

#if 0
  static const char* kConstructorError = kConstructorError;
  static const char* kComparisonError  = "Comparision error!";
  static const char* kEqualsError = "Equals error!";

#ifdef  NS_DEBUG
	mSelfTested=PR_TRUE;
#endif
  
  eCharSize theSize=eOneByte;

    //begin by testing the constructors...
  {
    {
      nsString2 theString0("foo",theSize);  //watch it construct and destruct

    }
    nsString2 theString1(theSize);
    nsString2 theString("hello",theSize);
    nsString2 theString3(theString,theSize);
    nsStr& si=theString;
    nsString2 theString4(si,theSize);
    PRUint32 theLen=theString3.Length();

    //and hey, why not do a few lexo-morphic tests...
    theString.ToUpperCase();
    theString.ToLowerCase();

    //while we're here, let's try truncation and setting the length.
    theString3.Truncate(3);
    theLen=theString3.Length();
    theString.SetCapacity(3);
    const char* theBuffer=theString.GetBuffer();
    const char* theOther=theBuffer;

    {
      nsString2 temp("  hello   there   rick   ",theSize);
      temp.CompressWhitespace();
    }

    nsString2 theString5("  hello there rick  ",theSize);
    theString5.StripChars("reo");
    theString5.Trim(" ",PR_TRUE,PR_FALSE);
    theString5.StripWhitespace();

    nsString2* theString6=theString5.ToNewString();
    char* str=theString5.ToNewCString();
    char buffer[100];
    theString5.ToCString(buffer,sizeof(buffer)-1);
    theOther=theString5.GetBuffer();
  }
    //try a few numeric conversion routines...
  {
    nsString2 str1("10000",theSize);
    PRInt32 err;
    PRInt32 theInt=str1.ToInteger(&err);
    str1="100.100";
    float theFloat=str1.ToFloat(&err);
  }
    //Now test the character accessor methods...
  {
    nsString2 theString("hello",theSize);
    PRUint32 len=theString.Length();
    for(PRUint32 i=0;i<len;i++) {
      PRUnichar ch3=theString.CharAt(i);
    }
    PRUnichar ch4=theString.First();
    PRUnichar ch5=theString.Last();
  }

  //**********************************************
  //Now let's test the CONCATENATION operators...
  //**********************************************
  {
    static const char* s1="hello";
    static const char* s2="world";
    
    nsString2 a(s1,theSize);
    nsString2 b(s2,theSize);
    nsString2 temp1(theSize);
    temp1=a+b;
    nsString2 temp2(theSize);
    temp2=a+"world!";
    nsString2 temp3(theSize);
    temp3=temp2+'!';
    temp3+='?';
    temp3+="rick!";
    temp3+=a;
    temp3.Append((float)100.100);
    temp3.Append(10000);
  }

  static const char* temp="hello";

  //**********************************************
  //Now let's test the ASSIGNMENT operators...
  //**********************************************
  {
    nsString2 str1(theSize);
    str1.SetString("abcdefg");
    str1.Assign("hello");
    str1.Assign('z');
    str1='a';
    str1="hello again";
    nsString2 str2("whats up doc?",theSize);
    str1=str2;
  }

  //**********************************************
  //Now let's test the APPEND methods...
  //**********************************************
  {
    {
      nsString2 temp("hello",theSize);
      temp.Append((float)1.5);
    }

    nsString2 temp1("hello",theSize);
    temp1.Append((float)1.5);
    nsString2 temp2("there",theSize);
    temp1.Append(temp2);
    temp1.Append(" xxx ");
    temp1.Append('4');
  }

  //**********************************************
  //Now let's test the SUBSTRING methods...
  //**********************************************
  {
    nsString2 temp1("hello there rick",theSize);
    nsString2 temp2(theSize);
    temp1.Left(temp2,10);
    temp1.Mid(temp2,6,5);
    temp1.Right(temp2,4);
  }


  //**********************************************
  //Now let's test the INSERTION methods...
  //**********************************************
  {
    nsString2 temp1("hello rick",theSize);
    temp1.Insert("there ",6); //char* insertion
    temp1.Insert("?",10); //char insertion
    temp1.Insert('*',10); //char insertion
    temp1.Insert("xxx",100,100); //this should append.
    nsString2 temp2("abcdefghijklmnopqrstuvwxyz",theSize);
    temp2.Insert(temp1,10);
    temp2.Cut(20,5);
    temp2.Cut(100,100); //this should fail.
  }


  //**********************************************
  //Now let's test a few string COMPARISION ops...
  //**********************************************

  nsString2 temp8("aaaa",theSize);
  nsString2 temp8a("AAAA",theSize);
  nsString2 temp9("bbbb",theSize);

  const char* aaaa="aaaa";
  const char* bbbb="bbbb";

    //First test the string compare routines...

  NS_ASSERTION(0>temp8.Compare(bbbb),kComparisonError);
  NS_ASSERTION(0>temp8.Compare(temp9),kComparisonError);
  NS_ASSERTION(0<temp9.Compare(temp8),kComparisonError);
  NS_ASSERTION(0==temp8.Compare(temp8a,PR_TRUE),kComparisonError);
  NS_ASSERTION(0==temp8.Compare(aaaa),kComparisonError);

    //Now test the boolean operators...
  NS_ASSERTION(temp8==temp8,kComparisonError);
  NS_ASSERTION(temp8==aaaa,kComparisonError);

  NS_ASSERTION(temp8!=temp9,kComparisonError);
  NS_ASSERTION(temp8!=bbbb,kComparisonError);

  NS_ASSERTION(((temp8<temp9) && (temp9>=temp8)),kComparisonError);

  NS_ASSERTION(((temp9>temp8) && (temp8<=temp9)),kComparisonError);
  NS_ASSERTION(temp9>aaaa,kComparisonError);

  NS_ASSERTION(temp8<=temp8,kComparisonError);
  NS_ASSERTION(temp8<=temp9,kComparisonError);
  NS_ASSERTION(temp8<=bbbb,kComparisonError);

  NS_ASSERTION(((temp9>=temp8) && (temp8<temp9)),kComparisonError);
  NS_ASSERTION(temp9>=temp8,kComparisonError);
  NS_ASSERTION(temp9>=aaaa,kComparisonError);

  NS_ASSERTION(temp8.Equals(temp8),kEqualsError);
  NS_ASSERTION(temp8.Equals(aaaa),kEqualsError);
  
  nsString2 temp10(temp8);
  temp10.ToUpperCase();
  NS_ASSERTION(temp8.Equals(temp10,PR_TRUE),kEqualsError);
  NS_ASSERTION(temp8.Equals("AAAA",PR_TRUE),kEqualsError);


  //**********************************************
  //Now let's test a few string MANIPULATORS...
  //**********************************************

  {
    nsAutoString2 ab("ab",theSize);
    nsString2 abcde("cde",theSize);
    nsString2 cut("abcdef",theSize);
    cut.Cut(7,10); //this is out of bounds, so ignore...
    cut.DebugDump(cout);
    cut.Cut(5,2); //cut last chars
    cut.DebugDump(cout);
    cut.Cut(1,1); //cut first char
    cut.DebugDump(cout);
    cut.Cut(2,1); //cut one from the middle
    cut.DebugDump(cout);
    cut="Hello there Rick";
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
  }


  //**********************************************
  //Now let's test the SEARCHING operations...
  //**********************************************

  {
    nsString2 find1("abcdefghijk",theSize);
    nsString2 find2("ijk",theSize);

    PRInt32 pos=find1.Find("efg");
    NS_ASSERTION(pos==4,"Error: Find routine");

    pos=find1.Find('d');
    NS_ASSERTION(pos==3,"Error: Find char routine");

    pos=find1.Find(find2);
    NS_ASSERTION(pos==8,"Error: Find char routine");

    pos=find1.FindCharInSet("12k");
    NS_ASSERTION(pos==10,"Error: FindFirstInChar routine");

    pos=find1.RFindCharInSet("12k");
    NS_ASSERTION(pos==10,"Error: FindLastInChar routine");

    pos=find1.RFind("efg");
    NS_ASSERTION(pos==4,"Error: RFind routine");

    pos=find1.RFind("xxx");
    NS_ASSERTION(pos==-1,"Error: RFind routine"); //this should fail

    pos=find1.RFind("");
    NS_ASSERTION(pos==-1,"Error: RFind routine"); //this too should fail.

    pos=find1.RFind('a');
    NS_ASSERTION(pos==0,"Error: RFind routine");

    pos=find1.BinarySearch('a');
    pos=find1.BinarySearch('b');
    pos=find1.BinarySearch('c');
    pos=find1.BinarySearch('d');
    pos=find1.BinarySearch('e');
    pos=find1.BinarySearch('f');
    pos=find1.BinarySearch('g');
    pos=find1.BinarySearch('h');
    pos=find1.BinarySearch('i');
    pos=find1.BinarySearch('z');
  }
#endif
}


/***********************************************************************
  IMPLEMENTATION NOTES: AUTOSTRING...
 ***********************************************************************/

void InitAutoStr(nsAutoString2& aDest,nsBufDescriptor& aBufDescriptor){
  aDest.mAgent=0;
  aDest.mStr=aBufDescriptor.mStr;
  aDest.mMultibyte=aBufDescriptor.mMultibyte;
  aDest.mCapacity=(sizeof(aDest.mBuffer)>>aDest.mMultibyte)-1;
  aDest.mOwnsBuffer=aBufDescriptor.mOwnsBuffer;
  AddNullTerminator(aDest);
}

/**
 * Special case constructor, that allows the consumer to provide
 * an underlying buffer for performance reasons.
 * @param   aBuffer points to your buffer
 * @param   aBufSize defines the size of your buffer
 * @param   aCurrentLength tells us the current length of the buffer
 */
nsAutoString2::nsAutoString2(eCharSize aCharSize) : nsString2(aCharSize){
  nsBufDescriptor theDescriptor(mBuffer,sizeof(mBuffer),aCharSize,PR_FALSE);
  InitAutoStr(*this,theDescriptor);
}

/**
 * construct from external buffer and given string
 * @param   anExtBuffer describes an external buffer
 * @param   aCString is a ptr to a 1-byte cstr
 */
nsAutoString2::nsAutoString2(nsBufDescriptor& aBufDescriptor,const char* aCString) : nsString2(aBufDescriptor.mMultibyte) {
  InitAutoStr(*this,aBufDescriptor);
  Assign(aCString);
}

/**
 * Copy construct from ascii c-string
 * @param   aCString is a ptr to a 1-byte cstr
 */
nsAutoString2::nsAutoString2(const char* aCString,eCharSize aCharSize) : nsString2(aCharSize) {
  nsBufDescriptor theDescriptor(mBuffer,sizeof(mBuffer),aCharSize,PR_FALSE);
  InitAutoStr(*this,theDescriptor);
  Assign(aCString);
}

/**
 * Copy construct from ascii c-string
 * @param   aCString is a ptr to a 1-byte cstr
 */
nsAutoString2::nsAutoString2(char* aCString,PRUint32 aLen,eCharSize aCharSize,PRBool assumeOwnership) : nsString2(aCharSize) {
  if(assumeOwnership) {
    nsBufDescriptor theDescriptor(aCString,aLen,eOneByte,PR_TRUE);
    InitAutoStr(*this,theDescriptor);
  }
  else {
    nsBufDescriptor theDescriptor(mBuffer,aLen,aCharSize,PR_FALSE);
    InitAutoStr(*this,theDescriptor);
  }
  Assign(aCString);
}

/**
 * Copy construct from uni-string
 * @param   aString is a ptr to a unistr
 */
nsAutoString2::nsAutoString2(const PRUnichar* aString,eCharSize aCharSize) : nsString2(aCharSize) {
  nsBufDescriptor theDescriptor(mBuffer,sizeof(mBuffer),aCharSize,PR_FALSE);
  InitAutoStr(*this,theDescriptor);
  Assign(aString);
}

/**
 * Copy construct from uni-string
 * @param   aString is a ptr to a unistr
 */
nsAutoString2::nsAutoString2(PRUnichar* aString,PRUint32 aLength,eCharSize aCharSize,PRBool assumeOwnership) : nsString2(aCharSize) {
  if(assumeOwnership) {
    nsBufDescriptor theDescriptor((char*)aString,aLength,eTwoByte,PR_TRUE);
    InitAutoStr(*this,theDescriptor);
  }
  else {
    nsBufDescriptor theDescriptor(mBuffer,sizeof(mBuffer),aCharSize,PR_FALSE);
    InitAutoStr(*this,theDescriptor);
  }
  Assign(aString);
}


/**
 * Copy construct from an nsString2
 * @param   
 */
nsAutoString2::nsAutoString2(const nsStr& aString,eCharSize aCharSize) : nsString2(aCharSize) {
  nsBufDescriptor theDescriptor(mBuffer,sizeof(mBuffer),aCharSize,PR_FALSE);
  InitAutoStr(*this,theDescriptor);
  Assign(aString);
}

/**
 * Copy construct from an nsString2
 * @param   
 */
nsAutoString2::nsAutoString2(const nsAutoString2& aString,eCharSize aCharSize) : nsString2(aCharSize) {
  nsBufDescriptor theDescriptor(mBuffer,sizeof(mBuffer),aCharSize,PR_FALSE);
  InitAutoStr(*this,theDescriptor);
  Assign(aString);
}

/**
 * Copy construct from an nsString2
 * @param   
 */
nsAutoString2::nsAutoString2(const nsString2& aString,eCharSize aCharSize) : nsString2(aCharSize){
  nsBufDescriptor theDescriptor(mBuffer,sizeof(mBuffer),aCharSize,PR_FALSE);
  InitAutoStr(*this,theDescriptor);
  Assign(aString);
}


/**
 * Copy construct from an nsString2
 * @param   
 */
nsAutoString2::nsAutoString2(PRUnichar aChar,eCharSize aCharSize) : nsString2(aCharSize){
  nsBufDescriptor theDescriptor(mBuffer,sizeof(mBuffer),aCharSize,PR_FALSE);
  InitAutoStr(*this,theDescriptor);
  Assign(aChar);
}

/**
 * deconstruct the autstring
 * @param   
 */
nsAutoString2::~nsAutoString2(){
//  bool b=true;
//  mStr=0;
}

void nsAutoString2::SizeOf(nsISizeOfHandler* aHandler) const {
  aHandler->Add(sizeof(*this));
  aHandler->Add(mCapacity << mMultibyte);
}

