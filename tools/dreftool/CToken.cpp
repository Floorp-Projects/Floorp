/*================================================================*
	Copyright © 1998 Rick Gessner. All Rights Reserved.
 *================================================================*/


#include "CToken.h"
#include "CScanner.h"
#include "nsCRT.h"

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
CToken::CToken(const nsCString& aName) : mTextValue(aName) {
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
void CToken::reinitialize(int aTag, const nsCString& aString){
  if(0==aString.Length()) 
    mTextValue.Truncate(0);
  else mTextValue.SetString(aString);
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
int CToken::consume(PRUnichar aChar,CScanner& aScanner) {
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
  for(i=0;i<mTextValue.Length();i++){
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
  char buf[256];
  mTextValue.ToCString(buf,sizeof(buf)-1);
  anOutputStream << buf << endl;
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
nsCString& CToken::getStringValue(void) {
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


