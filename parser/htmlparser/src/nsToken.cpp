/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsToken.h"
#include "nsScanner.h"

static int TokenCount=0;
static int DelTokenCount=0;

int CToken::GetTokenCount() {
  return TokenCount-DelTokenCount;
}


/**************************************************************
  And now for the CToken...
 **************************************************************/

/**
 *  Default constructor
 *  
 *  @update gess 7/21/98
 */
CToken::CToken(PRInt32 aTag) {
  mAttrCount=0;
  mNewlineCount=0;
  mLineNumber = 0;
  mInError = false;
  mTypeID=aTag;
  // Note that the use count starts with 1 instead of 0. This
  // is because of the assumption that any token created is in
  // use and therefore does not require an explicit addref, or
  // rather IF_HOLD. This, also, will make sure that tokens created 
  // on the stack do not accidently hit the arena recycler.
  mUseCount=1;
  NS_LOG_ADDREF(this, 1, "CToken", sizeof(*this));

#ifdef DEBUG
  ++TokenCount;
#endif
}

/**
 *  Decstructor
 *  
 *  @update gess 3/25/98
 */
CToken::~CToken() {
  ++DelTokenCount;
#ifdef NS_BUILD_REFCNT_LOGGING
  if (mUseCount == 1) {
    // Stack token
    NS_LOG_RELEASE(this, 0, "CToken");
  }
#endif
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
 * Get string of full contents, suitable for debug dump.
 * It should look exactly like the input source.
 * @update	gess5/11/98
 * @return  reference to string containing string value
 */
void CToken::GetSource(nsString& anOutputString) {
  anOutputString.Assign(GetStringValue());
}

/**
 * @update	harishd 3/23/00
 * @return  reference to string containing string value
 */
void CToken::AppendSourceTo(nsAString& anOutputString) {
  anOutputString.Append(GetStringValue());
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
 *  Retrieves copy of attr count for this token
 *  
 *  @update gess 3/25/98
 *  @return int containing attribute count
 */
PRInt16 CToken::GetAttributeCount(void) {
  return mAttrCount;
}


/**
 *  Retrieve type of token. This class returns -1, but 
 *  subclasses return something more meaningful.
 *  
 *  @update gess 3/25/98
 *  @return int value containing token type.
 */
PRInt32 CToken::GetTokenType(void) {
  return -1;
}


/**
 *  
 *  @update gess 3/25/98
 */
void CToken::SelfTest(void) {
#ifdef _DEBUG
#endif
}


