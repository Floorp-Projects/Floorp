/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/*
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

/******************************************************************************************
  MODULE NOTES:

  This file contains the workhorse copy and shift functions used in nsStrStruct.
  Ultimately, I plan to make the function pointers in this system available for 
  use by external modules. They'll be able to install their own "handlers".
  Not so, today though.

*******************************************************************************************/

#ifndef _BUFFERROUTINES_H
#define _BUFFERROUTINES_H

#include "nsCRT.h"

#ifndef RICKG_TESTBED
#include "nsUnicharUtilCIID.h"
#include "nsIServiceManager.h"
#include "nsICaseConversion.h"
#endif


inline PRUnichar GetUnicharAt(const char* aString,PRUint32 anIndex) {
  return ((PRUnichar*)aString)[anIndex];
}

inline PRUnichar GetCharAt(const char* aString,PRUint32 anIndex) {
  return (PRUnichar)aString[anIndex];
}

//----------------------------------------------------------------------------------------
//
//  This set of methods is used to shift the contents of a char buffer.
//  The functions are differentiated by shift direction and the underlying charsize.
//

PRInt32 TrimChars2(char* aString,PRUint32 aLength,const char* aSet,PRBool aEliminateLeading,PRBool aEliminateTrailing);
PRInt32 CompressChars1(char* aString,PRUint32 aLength,const char* aSet,PRUint32 aChar,PRBool aEliminateLeading,PRBool aEliminateTrailing);
PRInt32 CompressChars2(char* aString,PRUint32 aLength,const char* aSet,PRUint32 aChar,PRBool aEliminateLeading,PRBool aEliminateTrailing);


/**
 * This method shifts single byte characters left by a given amount from an given offset.
 * @update	gess 01/04/99
 * @param   aDest is a ptr to a cstring where left-shift is to be performed
 * @param   aLength is the known length of aDest
 * @param   anOffset is the index into aDest where shifting shall begin
 * @param   aCount is the number of chars to be "cut"
 */
void ShiftCharsLeft(char* aDest,PRUint32 aLength,PRUint32 anOffset,PRUint32 aCount) { 
  PRUint32 theMax=aLength-anOffset;
  PRUint32 theLength=(theMax<aCount) ? theMax : aCount;

  char* first= aDest+anOffset+aCount;
  char* last = aDest+aLength;
  char* to   = aDest+anOffset;

  //now loop over characters, shifting them left...
  while(first<=last) {
    *to=*first;  
    to++;
    first++;
  }
}

/**
 * This method shifts single byte characters right by a given amount from an given offset.
 * @update	gess 01/04/99
 * @param   aDest is a ptr to a cstring where the shift is to be performed
 * @param   aLength is the known length of aDest
 * @param   anOffset is the index into aDest where shifting shall begin
 * @param   aCount is the number of chars to be "inserted"
 */
void ShiftCharsRight(char* aDest,PRUint32 aLength,PRUint32 anOffset,PRUint32 aCount) { 
  char* last = aDest+aLength;
  char* first= aDest+anOffset-1;
  char* to = aDest+aLength+aCount;

  //Copy rightmost chars, up to offset+theDelta...
  while(first<=last) {
    *to=*last;  
    to--;
    last--;
  }
}

/**
 * This method shifts unicode characters by a given amount from an given offset.
 * @update	gess 01/04/99
 * @param   aDest is a ptr to a cstring where the shift is to be performed
 * @param   aLength is the known length of aDest
 * @param   anOffset is the index into aDest where shifting shall begin
 * @param   aCount is the number of chars to be "cut"
 */
void ShiftDoubleCharsLeft(char* aDest,PRUint32 aLength,PRUint32 anOffset,PRUint32 aCount) { 
  PRUint32 theMax=aLength-anOffset;
  PRUint32 theLength=(theMax<aCount) ? theMax : aCount;

  PRUnichar* theBuf=(PRUnichar*)aDest;
  PRUnichar* first= theBuf+anOffset+aCount;
  PRUnichar* last = theBuf+aLength;
  PRUnichar* to   = theBuf+anOffset;

  //now loop over characters, shifting them left...
  while(first<=last) {
    *to=*first;  
    to++;
    first++;
  }
}


/**
 * This method shifts unicode characters by a given amount from an given offset.
 * @update	gess 01/04/99
 * @param   aDest is a ptr to a cstring where the shift is to be performed
 * @param   aLength is the known length of aDest
 * @param   anOffset is the index into aDest where shifting shall begin
 * @param   aCount is the number of chars to be "inserted"
 */
void ShiftDoubleCharsRight(char* aDest,PRUint32 aLength,PRUint32 anOffset,PRUint32 aCount) { 
  PRUnichar* theBuf=(PRUnichar*)aDest;
  PRUnichar* last = theBuf+aLength;
  PRUnichar* first= theBuf+anOffset-1;
  PRUnichar* to = theBuf+aLength+aCount;

  //Copy rightmost chars, up to offset+theDelta...
  while(first<=last) {
    *to=*last;  
    to--;
    last--;
  }
}


typedef void (*ShiftChars)(char* aDest,PRUint32 aLength,PRUint32 anOffset,PRUint32 aCount);
ShiftChars gShiftChars[2][2]= {
  {&ShiftCharsLeft,&ShiftCharsRight},
  {&ShiftDoubleCharsLeft,&ShiftDoubleCharsRight}
};


//----------------------------------------------------------------------------------------
//
//  This set of methods is used to copy one buffer onto another.
//  The functions are differentiated by the size of source and dest character sizes.
//  WARNING: Your destination buffer MUST be big enough to hold all the source bytes.
//           We don't validate these ranges here (this should be done in higher level routines).
//


/**
 * Going 1 to 1 is easy, since we assume ascii. No conversions are necessary.
 * @update	gess 01/04/99
 * @param aDest is the destination buffer
 * @param aDestOffset is the pos to start copy to in the dest buffer
 * @param aSource is the source buffer
 * @param anOffset is the offset to start copying from in the source buffer
 * @param aCount is the (max) number of chars to copy
 */
void CopyChars1To1(char* aDest,PRInt32 anDestOffset,const char* aSource,PRUint32 anOffset,PRUint32 aCount) { 

  char*       to   = aDest+anDestOffset;
  const char* first= aSource+anOffset;
  const char* last = first+aCount;

  //now loop over characters, shifting them left...
  while(first<last) {
    *to=*first;  
    to++;
    first++;
  }
  *to=0;
}

/**
 * Going 1 to 2 requires a conversion from ascii to unicode. This can be expensive.
 * @param aDest is the destination buffer
 * @param aDestOffset is the pos to start copy to in the dest buffer
 * @param aSource is the source buffer
 * @param anOffset is the offset to start copying from in the source buffer
 * @param aCount is the (max) number of chars to copy
 */
void CopyChars1To2(char* aDest,PRInt32 anDestOffset,const char* aSource,PRUint32 anOffset,PRUint32 aCount) { 

  PRUnichar* theDest=(PRUnichar*)aDest;
  PRUnichar* to   = theDest+anDestOffset;
  const char* first= aSource+anOffset;
  const char* last = first+aCount;

  //now loop over characters, shifting them left...
  while(first<last) {
    *to=kIsoLatin1ToUCS2[*first];  
    to++;
    first++;
  }
  *to=0;
}


/**
 * Going 2 to 1 requires a conversion from unicode down to ascii. This can be lossy.
 * @update	gess 01/04/99
 * @param aDest is the destination buffer
 * @param aDestOffset is the pos to start copy to in the dest buffer
 * @param aSource is the source buffer
 * @param anOffset is the offset to start copying from in the source buffer
 * @param aCount is the (max) number of chars to copy
 */
void CopyChars2To1(char* aDest,PRInt32 anDestOffset,const char* aSource,PRUint32 anOffset,PRUint32 aCount) { 
  char*             to   = aDest+anDestOffset;
  PRUnichar*        theSource=(PRUnichar*)aSource;
  const PRUnichar*  first= theSource+anOffset;
  const PRUnichar*  last = first+aCount;

  //now loop over characters, shifting them left...
  while(first<last) {
    if(*first<255)
      *to=(char)*first;  
    else *to='.';
    to++;
    first++;
  }
  *to=0;
}

/**
 * Going 2 to 2 is fast and efficient.
 * @update	gess 01/04/99
 * @param aDest is the destination buffer
 * @param aDestOffset is the pos to start copy to in the dest buffer
 * @param aSource is the source buffer
 * @param anOffset is the offset to start copying from in the source buffer
 * @param aCount is the (max) number of chars to copy
 */
void CopyChars2To2(char* aDest,PRInt32 anDestOffset,const char* aSource,PRUint32 anOffset,PRUint32 aCount) { 
  PRUnichar* theDest=(PRUnichar*)aDest;
  PRUnichar* to   = theDest+anDestOffset;
  PRUnichar* theSource=(PRUnichar*)aSource;
  PRUnichar* from= theSource+anOffset;

  memcpy((void*)to,(void*)from,aCount*2);
}


//--------------------------------------------------------------------------------------


typedef void (*CopyChars)(char* aDest,PRInt32 anDestOffset,const char* aSource,PRUint32 anOffset,PRUint32 aCount);

CopyChars gCopyChars[2][2]={
  {&CopyChars1To1,&CopyChars1To2},
  {&CopyChars2To1,&CopyChars2To2}
};


//----------------------------------------------------------------------------------------
//
//  This set of methods is used to search a buffer looking for a char.
//


/**
 *  This methods cans the given buffer for the given char
 *  
 *  @update  gess 3/25/98
 *  @param   aDest is the buffer to be searched
 *  @param   aLength is the size (in char-units, not bytes) of the buffer
 *  @param   anOffset is the start pos to begin searching
 *  @param   aChar is the target character we're looking for
 *  @param   aIgnorecase tells us whether to use a case sensitive search
 *  @return  index of pos if found, else -1 (kNotFound)
 */
inline PRInt32 FindChar1(const char* aDest,PRUint32 aLength,PRUint32 anOffset,const PRUnichar aChar,PRBool aIgnoreCase) {
  PRUnichar theCmpChar=(aIgnoreCase ? nsCRT::ToUpper(aChar) : aChar);
  PRUint32  theIndex=0;
  for(theIndex=0;theIndex<aLength;theIndex++){
    PRUnichar theChar=GetCharAt(aDest,theIndex);
    if(aIgnoreCase)
      theChar=nsCRT::ToUpper(theChar);
    if(theChar==theCmpChar)
      return theIndex;
  }
  return kNotFound;
}

/**
 *  This methods cans the given buffer for the given char
 *  
 *  @update  gess 3/25/98
 *  @param   aDest is the buffer to be searched
 *  @param   aLength is the size (in char-units, not bytes) of the buffer
 *  @param   anOffset is the start pos to begin searching
 *  @param   aChar is the target character we're looking for
 *  @param   aIgnorecase tells us whether to use a case sensitive search
 *  @return  index of pos if found, else -1 (kNotFound)
 */
inline PRInt32 FindChar2(const char* aDest,PRUint32 aLength,PRUint32 anOffset,const PRUnichar aChar,PRBool aIgnoreCase) {
  PRUnichar theCmpChar=(aIgnoreCase ? nsCRT::ToUpper(aChar) : aChar);
  PRUint32  theIndex=0;
  for(theIndex=0;theIndex<aLength;theIndex++){
    PRUnichar theChar=GetUnicharAt(aDest,theIndex);
    if(aIgnoreCase)
      theChar=nsCRT::ToUpper(theChar);
    if(theChar==theCmpChar)
      return theIndex;
  }
  return kNotFound;
}



/**
 *  This methods cans the given buffer (in reverse) for the given char
 *  
 *  @update  gess 3/25/98
 *  @param   aDest is the buffer to be searched
 *  @param   aLength is the size (in char-units, not bytes) of the buffer
 *  @param   anOffset is the start pos to begin searching
 *  @param   aChar is the target character we're looking for
 *  @param   aIgnorecase tells us whether to use a case sensitive search
 *  @return  index of pos if found, else -1 (kNotFound)
 */
inline PRInt32 RFindChar1(const char* aDest,PRUint32 aLength,PRUint32 anOffset,const PRUnichar aChar,PRBool aIgnoreCase) {
  PRUnichar theCmpChar=(aIgnoreCase ? nsCRT::ToUpper(aChar) : aChar);
  PRUint32 theIndex=0;
  for(theIndex=aLength-1;theIndex>=0;theIndex--){
    PRUnichar theChar=GetCharAt(aDest,theIndex);
    if(aIgnoreCase)
      theChar=nsCRT::ToUpper(theChar);
    if(theChar==theCmpChar)
      return theIndex;
  }
  return kNotFound;
}



/**
 *  This methods cans the given buffer for the given char
 *  
 *  @update  gess 3/25/98
 *  @param   aDest is the buffer to be searched
 *  @param   aLength is the size (in char-units, not bytes) of the buffer
 *  @param   anOffset is the start pos to begin searching
 *  @param   aChar is the target character we're looking for
 *  @param   aIgnorecase tells us whether to use a case sensitive search
 *  @return  index of pos if found, else -1 (kNotFound)
 */
inline PRInt32 RFindChar2(const char* aDest,PRUint32 aLength,PRUint32 anOffset,const PRUnichar aChar,PRBool aIgnoreCase) {
  PRUnichar theCmpChar=(aIgnoreCase ? nsCRT::ToUpper(aChar) : aChar);
  PRUint32 theIndex=0;
  for(theIndex=aLength-1;theIndex>=0;theIndex--){
    PRUnichar theChar=GetUnicharAt(aDest,theIndex);
    if(aIgnoreCase)
      theChar=nsCRT::ToUpper(theChar);
    if(theChar==theCmpChar)
      return theIndex;
  }
  return kNotFound;
}

typedef PRInt32 (*FindChars)(const char* aDest,PRUint32 aLength,PRUint32 anOffset,const PRUnichar aChar,PRBool aIgnoreCase);
FindChars gFindChars[]={&FindChar1,&FindChar2};
FindChars gRFindChars[]={&RFindChar1,&RFindChar2};

//----------------------------------------------------------------------------------------
//
//  This set of methods is used to compare one buffer onto another.
//  The functions are differentiated by the size of source and dest character sizes.
//  WARNING: Your destination buffer MUST be big enough to hold all the source bytes.
//           We don't validate these ranges here (this should be done in higher level routines).
//


/**
 * This method compares the data in one buffer with another
 * @update	gess 01/04/99
 * @param   aStr1 is the first buffer to be compared
 * @param   aStr2 is the 2nd buffer to be compared
 * @param   aCount is the number of chars to compare
 * @param   aIgnorecase tells us whether to use a case-sensitive comparison
 * @return  -1,0,1 depending on <,==,>
 */
PRInt32 Compare1To1(const char* aStr1,const char* aStr2,PRUint32 aCount,PRBool aIgnoreCase){ 
  PRInt32 result=0;
  if(aIgnoreCase)
    result=nsCRT::strncasecmp(aStr1,aStr2,aCount);
  else result=strncmp(aStr1,aStr2,aCount);
  return result;
}

/**
 * This method compares the data in one buffer with another
 * @update	gess 01/04/99
 * @param   aStr1 is the first buffer to be compared
 * @param   aStr2 is the 2nd buffer to be compared
 * @param   aCount is the number of chars to compare
 * @param   aIgnorecase tells us whether to use a case-sensitive comparison
 * @return  -1,0,1 depending on <,==,>
 */
PRInt32 Compare2To2(const char* aStr1,const char* aStr2,PRUint32 aCount,PRBool aIgnoreCase){ 
  PRInt32 result=0;
  if(aIgnoreCase)
    result=nsCRT::strncasecmp((PRUnichar*)aStr1,(PRUnichar*)aStr2,aCount);
  else result=nsCRT::strncmp((PRUnichar*)aStr1,(PRUnichar*)aStr2,aCount);
  return result;
}


/**
 * This method compares the data in one buffer with another
 * @update	gess 01/04/99
 * @param   aStr1 is the first buffer to be compared
 * @param   aStr2 is the 2nd buffer to be compared
 * @param   aCount is the number of chars to compare
 * @param   aIgnorecase tells us whether to use a case-sensitive comparison
 * @return  -1,0,1 depending on <,==,>
 */
PRInt32 Compare2To1(const char* aStr1,const char* aStr2,PRUint32 aCount,PRBool aIgnoreCase){ 
  PRInt32 result;
  if(aIgnoreCase)
    result=nsCRT::strncasecmp((PRUnichar*)aStr1,aStr2,aCount);
  else result=nsCRT::strncmp((PRUnichar*)aStr1,aStr2,aCount);
  return result;
}


/**
 * This method compares the data in one buffer with another
 * @update	gess 01/04/99
 * @param   aStr1 is the first buffer to be compared
 * @param   aStr2 is the 2nd buffer to be compared
 * @param   aCount is the number of chars to compare
 * @param   aIgnorecase tells us whether to use a case-sensitive comparison
 * @return  -1,0,1 depending on <,==,>
 */
PRInt32 Compare1To2(const char* aStr1,const char* aStr2,PRUint32 aCount,PRBool aIgnoreCase){ 
  PRInt32 result;
  if(aIgnoreCase)
    result=nsCRT::strncasecmp((PRUnichar*)aStr2,aStr1,aCount)*-1;
  else result=nsCRT::strncasecmp((PRUnichar*)aStr2,aStr1,aCount)*-1;
  return result;
}


typedef PRInt32 (*CompareChars)(const char* aStr1,const char* aStr2,PRUint32 aCount,PRBool aIgnoreCase);
CompareChars gCompare[2][2]={
  {&Compare1To1,&Compare1To2},
  {&Compare2To1,&Compare2To2},
};

//----------------------------------------------------------------------------------------
//
//  This set of methods is used to convert the case of strings...
//


/**
 * This method performs a case conversion the data in the given buffer 
 *
 * @update	gess 01/04/99
 * @param   aString is the buffer to be case shifted
 * @param   aCount is the number of chars to compare
 * @param   aToUpper tells us whether to convert to upper or lower
 * @return  0
 */
PRInt32 ConvertCase1(char* aString,PRUint32 aCount,PRBool aToUpper){ 
  PRInt32 result=0;

  typedef char  chartype;
  chartype* cp = (chartype*)aString;
  chartype* end = cp + aCount-1;
  while (cp <= end) {
    chartype ch = *cp;
    if(aToUpper) {
      if ((ch >= 'a') && (ch <= 'z')) {
        *cp = 'A' + (ch - 'a');
      }
    }
    else {
      if ((ch >= 'A') && (ch <= 'Z')) {
        *cp = 'a' + (ch - 'A');
      }
    }
    cp++;
  }
  return result;
}

//----------------------------------------------------------------------------------------

#ifndef RICKG_TESTBED
class HandleCaseConversionShutdown3 : public nsIShutdownListener {
public :
   NS_IMETHOD OnShutdown(const nsCID& cid, nsISupports* service);
   HandleCaseConversionShutdown3(void) { NS_INIT_REFCNT(); }
   virtual ~HandleCaseConversionShutdown3(void) {}
   NS_DECL_ISUPPORTS
};

static NS_DEFINE_CID(kUnicharUtilCID, NS_UNICHARUTIL_CID);
static NS_DEFINE_IID(kICaseConversionIID, NS_ICASECONVERSION_IID);
static NS_DEFINE_IID(kIShutdownListenerIID, NS_ISHUTDOWNLISTENER_IID);
static nsICaseConversion * gCaseConv = 0; 

NS_IMPL_ISUPPORTS(HandleCaseConversionShutdown3, kIShutdownListenerIID);

nsresult HandleCaseConversionShutdown3::OnShutdown(const nsCID& cid, nsISupports* service) {
    if (cid.Equals(kUnicharUtilCID)) {
        NS_ASSERTION(service == gCaseConv, "wrong service!");
        if(gCaseConv){
          gCaseConv->Release();
          gCaseConv = 0;
        }
    }
    return NS_OK;
}


class CCaseConversionServiceInitializer {
public:
  CCaseConversionServiceInitializer(){
    mListener = new HandleCaseConversionShutdown3();
    if(mListener){
      mListener->AddRef();
      nsresult result=nsServiceManager::GetService(kUnicharUtilCID, kICaseConversionIID,(nsISupports**) &gCaseConv, mListener);
    }
  }
protected:
  HandleCaseConversionShutdown3* mListener;
};

#endif

//----------------------------------------------------------------------------------------

/**
 * This method performs a case conversion the data in the given buffer 
 *
 * @update	gess 01/04/99
 * @param   aString is the buffer to be case shifted
 * @param   aCount is the number of chars to compare
 * @param   aToUpper tells us whether to convert to upper or lower
 * @return  0
 */
PRInt32 ConvertCase2(char* aString,PRUint32 aCount,PRBool aToUpper){ 
  PRUnichar* cp = (PRUnichar*)aString;
  PRUnichar* end = cp + aCount-1;
  PRInt32 result=0;

#ifndef RICKG_TESTBED
  static CCaseConversionServiceInitializer  gCaseConversionServiceInitializer;

  // I18N code begin
  if(gCaseConv) {
    nsresult err=(aToUpper) ? gCaseConv->ToUpper(cp, cp, aCount) : gCaseConv->ToLower(cp, cp, aCount);
    if(NS_SUCCEEDED(err))
      return 0;
  }
  // I18N code end
#endif


  while (cp <= end) {
    PRUnichar ch = *cp;
    if(aToUpper) {
      if ((ch >= 'a') && (ch <= 'z')) {
        *cp = 'A' + (ch - 'a');
      }
    }
    else {
      if ((ch >= 'A') && (ch <= 'Z')) {
        *cp = 'a' + (ch - 'A');
      }
    }
    cp++;
  }

  return result;
}

typedef PRInt32 (*CaseConverters)(char*,PRUint32,PRBool);
CaseConverters gCaseConverters[]={&ConvertCase1,&ConvertCase2};

//----------------------------------------------------------------------------------------
//
//  This set of methods is used strip chars from a given buffer...
//

/**
 * This method removes chars (given in aSet) from the given buffer 
 *
 * @update	gess 01/04/99
 * @param   aString is the buffer to be manipulated
 * @param   anOffset is starting pos in buffer for manipulation
 * @param   aCount is the number of chars to compare
 * @param   aSet tells us which chars to remove from given buffer
 * @return  the new length of the given buffer
 */
PRInt32 StripChars1(char* aString,PRUint32 anOffset,PRUint32 aCount,const char* aSet){ 
  PRInt32 result=0;

  typedef char  chartype;
  chartype*  from = (chartype*)&aString[anOffset];
  chartype*  end  = (chartype*)from + aCount-1;
  chartype*  to   = from;

  if(aSet){
    PRUint32 aSetLen=strlen(aSet);
    while (from <= end) {
      chartype ch = *from;
      if(kNotFound==FindChar1(aSet,aSetLen,0,ch,PR_FALSE)){
        *to++=*from;
      }
      from++;
    }
    *to = 0;
  }
  return to - (chartype*)aString;
}

/**
 * This method removes chars (given in aSet) from the given buffer 
 *
 * @update	gess 01/04/99
 * @param   aString is the buffer to be manipulated
 * @param   anOffset is starting pos in buffer for manipulation
 * @param   aCount is the number of chars to compare
 * @param   aSet tells us which chars to remove from given buffer
 * @return  the new length of the given buffer
 */
PRInt32 StripChars2(char* aString,PRUint32 anOffset,PRUint32 aCount,const char* aSet){ 
  PRInt32 result=0;

  typedef PRUnichar  chartype;
  chartype*  from = (chartype*)&aString[anOffset];
  chartype*  end  = (chartype*)from + aCount-1;
  chartype*  to   = from;

  if(aSet){
    PRUint32 aSetLen=strlen(aSet);
    while (from <= end) {
      chartype ch = *from;
      if(kNotFound==FindChar1(aSet,aSetLen,0,ch,PR_FALSE)){
        *to++=*from;
      }
      from++;
    }
    *to = 0;
  }
  return to - (chartype*)aString;
}

typedef PRInt32 (*StripChars)(char* aString,PRUint32 aDestOffset,PRUint32 aCount,const char* aSet);
StripChars gStripChars[]={&StripChars1,&StripChars2};

//----------------------------------------------------------------------------------------
//
//  This set of methods is used trim chars from the edges of a buffer...
//

/**
 * This method trims chars (given in aSet) from the edges of given buffer 
 *
 * @update	gess 01/04/99
 * @param   aString is the buffer to be manipulated
 * @param   aLength is the length of the buffer
 * @param   aSet tells us which chars to remove from given buffer
 * @param   aEliminateLeading tells us whether to strip chars from the start of the buffer
 * @param   aEliminateTrailing tells us whether to strip chars from the start of the buffer
 * @return  the new length of the given buffer
 */
PRInt32 TrimChars1(char* aString,PRUint32 aLength,const char* aSet,PRBool aEliminateLeading,PRBool aEliminateTrailing){ 
  PRInt32 result=0;

  typedef char  chartype;
  chartype*  from = (chartype*)aString;
  chartype*  end =  from + aLength -1;
  chartype*  to = from;

  if(aSet) {
    PRUint32 aSetLen=strlen(aSet);
      //begin by find the first char not in aTrimSet
    if(aEliminateLeading) {
      while (from <= end) {
        chartype ch = *from;
        if(kNotFound==FindChar1(aSet,aSetLen,0,ch,PR_FALSE)){
          break;
        }
        from++;
      }
    }
      //Now, find last char not in aTrimSet
    if(aEliminateTrailing) {
      while(from<=end) {
        chartype ch = *end;
        if(kNotFound==FindChar1(aSet,aSetLen,0,ch,PR_FALSE)){
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

    *to = 0;
  }
  return to - (chartype*)aString;
}

/**
 * This method trims chars (given in aSet) from the edges of given buffer 
 *
 * @update	gess 01/04/99
 * @param   aString is the buffer to be manipulated
 * @param   aLength is the length of the buffer
 * @param   aSet tells us which chars to remove from given buffer
 * @param   aEliminateLeading tells us whether to strip chars from the start of the buffer
 * @param   aEliminateTrailing tells us whether to strip chars from the start of the buffer
 * @return  the new length of the given buffer
 */
PRInt32 TrimChars2(char* aString,PRUint32 aLength,const char* aSet,PRBool aEliminateLeading,PRBool aEliminateTrailing){ 
  PRInt32 result=0;

  typedef PRUnichar  chartype;
  chartype*  from = (chartype*)aString;
  chartype*  end =  from + aLength -1;
  chartype*  to = from;

  if(aSet) {
    PRUint32 aSetLen=strlen(aSet);
      //begin by find the first char not in aTrimSet
    if(aEliminateLeading) {
      while (from <= end) {
        chartype ch = *from;
        if(kNotFound==FindChar1(aSet,aSetLen,0,ch,PR_FALSE)){
          break;
        }
        from++;
      }
    }
      //Now, find last char not in aTrimSet
    if(aEliminateTrailing) {
      while(from<=end) {
        chartype ch = *end;
        if(kNotFound==FindChar1(aSet,aSetLen,0,ch,PR_FALSE)){
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

    *to = 0;
  }
  return to - (chartype*)aString;
}

typedef PRInt32 (*TrimChars)(char* aString,PRUint32 aCount,const char* aSet,PRBool aEliminateLeading,PRBool aEliminateTrailing);
TrimChars gTrimChars[]={&TrimChars1,&TrimChars2};

//----------------------------------------------------------------------------------------
//
//  This set of methods is used compress char sequences in a buffer...
//


/**
 * This method compresses duplicate runs of a given char from the given buffer 
 *
 * @update	gess 01/04/99
 * @param   aString is the buffer to be manipulated
 * @param   aLength is the length of the buffer
 * @param   aSet tells us which chars to compress from given buffer
 * @param   aEliminateLeading tells us whether to strip chars from the start of the buffer
 * @param   aEliminateTrailing tells us whether to strip chars from the start of the buffer
 * @return  the new length of the given buffer
 */
PRInt32 CompressChars1(char* aString,PRUint32 aLength,const char* aSet,PRUint32 aChar,PRBool aEliminateLeading,PRBool aEliminateTrailing){ 
  PRInt32 result=0;

  TrimChars1(aString,aLength,aSet,aEliminateLeading,aEliminateTrailing);

  typedef char  chartype;
  chartype*  from = aString;
  chartype*  end =  aString + aLength-1;
  chartype*  to = from;

    //this code converts /n, /t, /r into normal space ' ';
    //it also compresses runs of whitespace down to a single char...
  if(aSet){
    PRUint32 aSetLen=strlen(aSet);
    while (from <= end) {
      chartype ch = *from++;
      if(kNotFound!=FindChar1(aSet,aSetLen,0,ch,PR_FALSE)){
        *to++ = ' ';
        while (from <= end) {
          ch = *from++;
          if(kNotFound==FindChar1(aSet,aSetLen,0,ch,PR_FALSE)){
            *to++ = ch;
            break;
          }
        }
      } else {
        *to++ = ch;
      }
    }
    *to = 0;
  }
  return to - (chartype*)aString;
}


/**
 * This method compresses duplicate runs of a given char from the given buffer 
 *
 * @update	gess 01/04/99
 * @param   aString is the buffer to be manipulated
 * @param   aLength is the length of the buffer
 * @param   aSet tells us which chars to compress from given buffer
 * @param   aEliminateLeading tells us whether to strip chars from the start of the buffer
 * @param   aEliminateTrailing tells us whether to strip chars from the start of the buffer
 * @return  the new length of the given buffer
 */
PRInt32 CompressChars2(char* aString,PRUint32 aLength,const char* aSet,PRUint32 aChar,PRBool aEliminateLeading,PRBool aEliminateTrailing){ 
  PRInt32 result=0;

  TrimChars2(aString,aLength,aSet,aEliminateLeading,aEliminateTrailing);

  typedef PRUnichar  chartype;
  chartype*  from = (chartype*)aString;
  chartype*  end =  from + aLength-1;
  chartype*  to = from;

    //this code converts /n, /t, /r into normal space ' ';
    //it also compresses runs of whitespace down to a single char...
  if(aSet){
    PRUint32 aSetLen=strlen(aSet);
    while (from <= end) {
      chartype ch = *from++;
      if(kNotFound!=FindChar1(aSet,aSetLen,0,ch,PR_FALSE)){
        *to++ = ' ';
        while (from <= end) {
          ch = *from++;
          if(kNotFound==FindChar1(aSet,aSetLen,0,ch,PR_FALSE)){
            *to++ = ch;
            break;
          }
        }
      } else {
        *to++ = ch;
      }
    }
    *to = 0;
  }
  return to - (chartype*)aString;
}

typedef PRInt32 (*CompressChars)(char* aString,PRUint32 aCount,const char* aSet,PRUint32 aChar,PRBool aEliminateLeading,PRBool aEliminateTrailing);
CompressChars gCompressChars[]={&CompressChars1,&CompressChars2};


#endif
