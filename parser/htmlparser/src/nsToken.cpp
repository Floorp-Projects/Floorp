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

#include "nsToken.h"
#include "nsScanner.h"

static int TokenCount=0;
static int DelTokenCount=0;

int CToken::GetTokenCount(){return TokenCount-DelTokenCount;}


/**************************************************************
  And now for the CToken...
 **************************************************************/

/**
 *  Default constructor
 *  
 *  @update gess 7/21/98
 */
CToken::CToken(PRInt32 aTag) : mTextValue() {
  mTypeID=aTag;
  mAttrCount=0;
  TokenCount++;
}

/**
 *  Constructor with string for tagname assignment
 *  
 *  @update gess 3/25/98
 *  @param  nsString--name of token
 */
CToken::CToken(const nsString& aName) : mTextValue(aName) {
  mTypeID=0;
  mAttrCount=0;
  TokenCount++;
}

/**
 *  constructor from char*
 *  
 *  @update gess 3/25/98
 *  @param  aName--char* containing name of token
 */
CToken::CToken(const char* aName) : mTextValue(aName) {
  mTypeID=0;
  mAttrCount=0;
  TokenCount++;
}
 
/**
 *  Decstructor
 *  
 *  @update gess 3/25/98
 */
CToken::~CToken() {
  DelTokenCount++;
}


/**
 * This method gets called when a token is about to be reused
 * for some other purpose. The token should initialize itself 
 * to some reasonable default values.
 * @update	gess7/25/98
 * @param   aTag
 * @param   aString
 */
void CToken::Reinitialize(PRInt32 aTag, const nsString& aString){
  if(0==aString.Length()) 
    mTextValue.Truncate(0);
  else mTextValue=aString;
  mAttrCount=0;
  mTypeID=aTag;
  mAttrCount=0;
}
 
/**
 *  Virtual method used to tell this toke to consume his
 *  valid chars.
 *  
 *  @update gess 3/25/98
 *  @param  aChar -- first char in sequence
 *  @param  aScanner -- object to retrieve data from
 *  @return int error code
 */
nsresult CToken::Consume(PRUnichar aChar,nsScanner& aScanner) {
  nsresult result=NS_OK;
  return result;
}


/**
 *  This debug method causes the token to dump its content
 *  to the given stream (formated for debugging).
 *  
 *  @update gess 3/25/98
 *  @param  ostream -- output stream to accept output data
 */
void CToken::DebugDumpToken(ostream& anOutputStream) {
  anOutputStream << "[" << GetClassName() << "] ";
  int i=0;
  for(i=0;i<mTextValue.Length();i++){
    anOutputStream << char(mTextValue.CharAt(i));
  }
  anOutputStream << " TypeID: " << mTypeID << " AttrCount: " << mAttrCount << endl;
}

/**
 *  This debug method causes the token to dump its content
 *  to the given stream, formated as text.
 *  
 *  @update gess 3/25/98
 *  @param  ostream -- output stream to accept output data
 */
void CToken::DebugDumpSource(ostream& anOutputStream) {
  char buf[256];
  anOutputStream << mTextValue.ToCString(buf,256);
}

/**
 * Setter method that changes the string value of this token
 * @update	gess5/11/98
 * @param   name is a char* value containing new string value
 */
void CToken::SetStringValue(const char* name){
  mTextValue=name;
}

/**
 *  This method retrieves the value of this internal string. 
 *  
 *  @update gess 3/25/98
 *  @return nsString reference to internal string value
 */
nsString& CToken::GetStringValueXXX(void) {
  return mTextValue;
}

/**
 *  This method retrieves the value of this internal string
 *  as a cstring.
 *  
 *  @update gess 3/25/98
 *  @return char* rep of internal string value
 */
char* CToken::GetCStringValue(char* aBuffer, PRInt32 aMaxLen) {
  strcpy(aBuffer,"string");
  return aBuffer;
}

/**
 *  Sets the internal ordinal value for this token.
 *  This method is deprecated, and will soon be going away.
 *  
 *  @update gess 3/25/98
 *  @param  value -- new ordinal value for this token
 */
void CToken::SetTypeID(PRInt32 aTypeID) {
  mTypeID=aTypeID;
}

/**
 *  Retrieves copy of internal ordinal value.
 *  This method is deprecated, and will soon be going away.
 *  
 *  @update gess 3/25/98
 *  @return int containing ordinal value
 */
PRInt32 CToken::GetTypeID(void) {
  return mTypeID;
}

/**
 *  Sets the attribute count for this token
 *  
 *  @update gess 3/25/98
 *  @param  value -- new attr count
 */
void CToken::SetAttributeCount(PRInt16 value) {
  mAttrCount=value;
}

/**
 *  Retrieves copy of attr count for this token
 *  
 *  @update gess 3/25/98
 *  @return int containing attribute count
 */
PRInt16  CToken::GetAttributeCount(void) {
  return mAttrCount;
}


/**
 *  Retrieve type of token. This class returns -1, but 
 *  subclasses return something more meaningful.
 *  
 *  @update gess 3/25/98
 *  @return int value containing token type.
 */
PRInt32  CToken::GetTokenType(void) {
  return -1;
}

/**
 *  retrieve this tokens classname.  
 *  
 *  @update gess 3/25/98
 *  @return char* containing name of class
 */
const char*  CToken::GetClassName(void) {
  return "token";
}


/**
 *  
 *  @update gess 3/25/98
 */
void CToken::SelfTest(void) {
#ifdef _DEBUG
#endif
}


