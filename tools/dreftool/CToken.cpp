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
 * The Original Code is Dreftool.
 *
 * The Initial Developer of the Original Code is
 * Rick Gessner <rick@gessner.com>.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Kenneth Herron <kjh-5727@comcast.net>
 *   Bernd Mielke <bmlk@gmx.de>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
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

#include "CToken.h"
#include "CScanner.h"
#include "nsCRT.h"
#include "nsReadableUtils.h"

static int TokenCount=0;


/**************************************************************
  And now for the CToken...
 **************************************************************/

/**
 *  Default constructor
 *  
 *  @update gess 7/21/98
 */
CToken::CToken(int aTag) : mTextValue() {
  mTypeID=aTag;
  mAttrCount=0;
  TokenCount++;
}

/**
 *  Constructor with string for tagname assignment
 *  
 *  @update gess 3/25/98
 *  @param  nsCString--name of token
 */
CToken::CToken(const nsAFlatCString& aName) : mTextValue(aName) {
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
}


/**
 * This method gets called when a token is about to be reused
 * for some other purpose. The token should initialize itself 
 * to some reasonable default values.
 * @update	gess7/25/98
 * @param   aTag
 * @param   aString
 */
void CToken::reinitialize(int aTag, const nsAFlatCString& aString){
  if(aString.IsEmpty()) 
    mTextValue.Truncate();
  else mTextValue.Assign(aString);
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
int CToken::consume(char aChar,CScanner& aScanner) {
  int result=kNoError;
  return result;
}


/**
 *  This debug method causes the token to dump its content
 *  to the given stream (formated for debugging).
 *  
 *  @update gess 3/25/98
 *  @param  ostream -- output stream to accept output data
 */
void CToken::debugDumpToken(ostream& anOutputStream) {
  anOutputStream << "[" << getClassName() << "] ";
  int i=0;
  for(i=0;i<(int) mTextValue.Length();i++){
    anOutputStream << char(mTextValue.CharAt(i));
  }
  anOutputStream << ": " << mTypeID << endl;
}

/**
 *  This debug method causes the token to dump its content
 *  to the given stream, formated as text.
 *  
 *  @update gess 3/25/98
 *  @param  ostream -- output stream to accept output data
 */
void CToken::debugDumpSource(ostream& anOutputStream) {
  anOutputStream << PromiseFlatCString(mTextValue).get() << endl;
}

/**
 * setter method that changes the string value of this token
 * @update	gess5/11/98
 * @param   name is a char* value containing new string value
 */
void CToken::setStringValue(const char* name){
  mTextValue=name;
}

/**
 *  This method retrieves the value of this internal string. 
 *  
 *  @update gess 3/25/98
 *  @return nsCString reference to internal string value
 */
nsAFlatCString& CToken::getStringValue(void) {
  return mTextValue;
}

/**
 * Retrieve string value of the token
 * @update	gess5/11/98
 * @return  current int value for this token
 */
int CToken::getIntValue(int& anErrorCode){
  return mTextValue.ToInteger(&anErrorCode,10);
}

/**
 * Retrieve string value of the token
 * @update	gess5/11/98
 * @return  reference to string containing string value
 */
float CToken::getFloatValue(int& anErrorCode){
  return mTextValue.ToFloat(&anErrorCode);
}

/**
 *  This method retrieves the value of this internal string
 *  as a nsCString.
 *  
 *  @update gess 3/25/98
 *  @return char* rep of internal string value
 */
char* CToken::getStringValue(char* aBuffer, int aMaxLen) {
  strcpy(aBuffer,"string");
  return aBuffer;
}

/**
 *  sets the internal ordinal value for this token.
 *  This method is deprecated, and will soon be going away.
 *  
 *  @update gess 3/25/98
 *  @param  value -- new ordinal value for this token
 */
void CToken::setTypeID(int aTypeID) {
  mTypeID=aTypeID;
}

/**
 *  Retrieves copy of internal ordinal value.
 *  This method is deprecated, and will soon be going away.
 *  
 *  @update gess 3/25/98
 *  @return int containing ordinal value
 */
int CToken::getTypeID(void) {
  return mTypeID;
}

/**
 *  sets the attribute count for this token
 *  
 *  @update gess 3/25/98
 *  @param  value -- new attr count
 */
void CToken::setAttributeCount(int value) {
  mAttrCount=value;
}

/**
 *  Retrieves copy of attr count for this token
 *  
 *  @update gess 3/25/98
 *  @return int containing attribute count
 */
int  CToken::getAttributeCount(void) {
  return mAttrCount;
}


/**
 *  Retrieve type of token. This class returns -1, but 
 *  subclasses return something more meaningful.
 *  
 *  @update gess 3/25/98
 *  @return int value containing token type.
 */
int  CToken::getTokenType(void) {
  return -1;
}

/**
 *  retrieve this tokens classname.  
 *  
 *  @update gess 3/25/98
 *  @return char* containing name of class
 */
const char*  CToken::getClassName(void) {
  return "token";
}


/**
 *  
 *  @update gess 3/25/98
 */
void CToken::selfTest(void) {
#ifdef _debug
#endif
}


