/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/*
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
 * Contributor(s): 
 */

#include "nsToken.h"
#include "nsScanner.h"


#ifdef MATCH_CTOR_DTOR    
MOZ_DECL_CTOR_COUNTER(CToken);
#endif

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
CToken::CToken(PRInt32 aTag) {
  // Tokens are allocated through the arena ( not heap allocated..yay ).
  // We, therefore, don't need this macro anymore..
#ifdef MATCH_CTOR_DTOR 
  MOZ_COUNT_CTOR(CToken);
#endif 
  mAttrCount=0;
  mNewlineCount=0;
  mTypeID=aTag;
  mOrigin=eSource;
  // Note that the use count starts with 1 instead of 0. This
  // is because of the assumption that any token created is in
  // use and therefore does not require an explicit addref, or
  // rather IF_HOLD. This, also, will make sure that tokens created 
  // on the stack do not accidently hit the arena recycler.
  mUseCount=1;

#ifdef NS_DEBUG
  TokenCount++;
#endif
}

/**
 *  Decstructor
 *  
 *  @update gess 3/25/98
 */
CToken::~CToken() {
  // Tokens are allocated through the arena ( not heap allocated..yay ).
  // We, therefore, don't need this macro anymore..
#ifdef MATCH_CTOR_DTOR 
  MOZ_COUNT_DTOR(CToken);
#endif
  DelTokenCount++;
  mUseCount=0;
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
nsresult CToken::Consume(PRUnichar aChar,nsScanner& aScanner,PRInt32 aMode) {
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
void CToken::DebugDumpToken(nsOutputStream& anOutputStream) {
  anOutputStream << "[" << GetClassName() << "] ";
  PRUint32 i=0;
  PRUint32 theLen=GetStringValue().Length();
  for(i=0;i<theLen;i++){
    anOutputStream << (const char*)NS_ConvertUCS2toUTF8(GetStringValue());
  }
  anOutputStream << " TypeID: " << mTypeID << " AttrCount: " << mAttrCount << nsEndl;
}

/**
 *  This debug method causes the token to dump its content
 *  to the given stream, formated as text.
 *  
 *  @update gess 3/25/98
 *  @param  ostream -- output stream to accept output data
 */
void CToken::DebugDumpSource(nsOutputStream& anOutputStream) {
  anOutputStream << (const char*)NS_ConvertUCS2toUTF8(GetStringValue());
}


/**
 * Get string of full contents, suitable for debug dump.
 * It should look exactly like the input source.
 * @update	gess5/11/98
 * @return  reference to string containing string value
 */
void CToken::GetSource(nsString& anOutputString){
  anOutputString.Assign(GetStringValue());
}

/**
 * @update	harishd 3/23/00
 * @return  reference to string containing string value
 */
void CToken::AppendSource(nsString& anOutputString){
  anOutputString.Append(GetStringValue());
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


