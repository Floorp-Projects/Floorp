/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
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
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either of the GNU General Public License Version 2 or later (the "GPL"),
 * or the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#include "nsToken.h"
#include "nsScanner.h"


#ifdef MATCH_CTOR_DTOR    
MOZ_DECL_CTOR_COUNTER(CToken)
#endif

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
  // Tokens are allocated through the arena ( not heap allocated..yay ).
  // We, therefore, don't need this macro anymore..
#ifdef MATCH_CTOR_DTOR 
  MOZ_COUNT_CTOR(CToken);
#endif 
  mAttrCount=0;
  mNewlineCount=0;
  mLineNumber = 0;
  mInError = PR_FALSE;
  mTypeID=aTag;
  // Note that the use count starts with 1 instead of 0. This
  // is because of the assumption that any token created is in
  // use and therefore does not require an explicit addref, or
  // rather IF_HOLD. This, also, will make sure that tokens created 
  // on the stack do not accidently hit the arena recycler.
  mUseCount=1;

#ifdef NS_DEBUG
  ++TokenCount;
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
  ++DelTokenCount;
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


