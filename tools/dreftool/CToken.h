/*================================================================*
	Copyright © 1998 Rick Gessner. All Rights Reserved.
 *================================================================*/


/**
 * MODULE NOTES:
 * @update  gess 4/1/98
 * 
 * This class is defines the basic notion of a token
 * within our system. All other tokens are derived from
 * this one. It offers a few basic interfaces, but the
 * most important is consume(). The consume() method gets
 * called during the tokenization process when an instance
 * of that particular token type gets detected in the 
 * input stream.
 *
 */


#ifndef CTOKEN__
#define CTOKEN__


#include "SharedTypes.h"
#include "nsString.h"
#include <iostream.h>
#include "cscanner.h"


/**
 *  Token objects represent sequences of characters as they
 *  are consumed from the input stream (URL). While they're
 *  pretty general in nature, we use subclasses (found in
 *  nsHTMLTokens.h) to define <start>, </end>, <text>,
 *  <comment>, <&entity>, <newline>, and <whitespace> tokens.
 *  
 *  @update  gess 3/25/98
 */
class  CToken {
  public:

    /**
     * Default constructor
     * @update	gess7/21/98
     */
    CToken(int aTag=0);

    /**
     * Constructor with string assignment for tag
     * @update	gess5/11/98
     * @param   aName is the given name of the token 
     */
    CToken(const nsCString& aName);

    /**
     * constructor from char*
     * @update	gess5/11/98
     * @param   aName is the given name of the token 
     */
    CToken(const char* aName);

    /**
     * destructor
     * @update	gess5/11/98
     */
    virtual ~CToken();

    /**
     * This method gets called when a token is about to be reused
     * for some other purpose. The token should reinit itself to
     * some reasonable default values.
     * @update	gess7/25/98
     * @param   aTag
     * @param   aString
     */
    virtual void reinitialize(int aTag, const nsCString& aString);
    
    /**
     * Retrieve string value of the token
     * @update	gess5/11/98
     * @return  reference to string containing string value
     */
    virtual nsCString& getStringValue(void);

    /**
     * Retrieve string value of the token
     * @update	gess5/11/98
     * @return  current int value for this token
     */
    virtual int getIntValue(int& anErrorCode);

    /**
     * Retrieve string value of the token
     * @update	gess5/11/98
     * @return  reference to string containing string value
     */
    virtual float getFloatValue(int& anErrorCode);

    /**
     * setter method that changes the string value of this token
     * @update	gess5/11/98
     * @param   name is a char* value containing new string value
     */
    virtual void setStringValue(const char* name);

    /**
     * Retrieve string value of the token as a c-string
     * @update	gess5/11/98
     * @return  reference to string containing string value
     */
    virtual char* getStringValue(char* aBuffer, int aMaxLen);
    
    /**
     * sets the ordinal value of this token (not currently used)
     * @update	gess5/11/98
     * @param   value is the new ord value for this token
     */
    virtual void setTypeID(int aValue);
    
    /**
     * getter which retrieves the current ordinal value for this token
     * @update	gess5/11/98
     * @return  current ordinal value 
     */
    virtual int getTypeID(void);

    /**
     * sets the # of attributes found for this token.
     * @update	gess5/11/98
     * @param   value is the attr count
     */
    virtual void setAttributeCount(int aValue);

    /**
     * getter which retrieves the current attribute count for this token
     * @update	gess5/11/98
     * @return  current attribute count 
     */
    virtual int getAttributeCount(void);

    /**
     * Causes token to consume data from given scanner.
     * Note that behavior varies wildly between CToken subclasses.
     * @update	gess5/11/98
     * @param   aChar -- most recent char consumed
     * @param   aScanner -- input source where token should get data
     * @return  error code (0 means ok)
     */
    virtual int consume(PRUnichar aChar,CScanner& aScanner);

    /**
     * Causes token to dump itself in debug form to given output stream
     * @update	gess5/11/98
     * @param   out is the output stream where token should write itself
     */
    virtual void debugDumpToken(ostream& out);

    /**
     * Causes token to dump itself in source form to given output stream
     * @update	gess5/11/98
     * @param   out is the output stream where token should write itself
     */
    virtual void debugDumpSource(ostream& out);

    /**
     * getter which retrieves type of token
     * @update	gess5/11/98
     * @return  int containing token type
     */
    virtual int getTokenType(void);

    /**
     * getter which retrieves the class name for this token 
     * This method is only used for debug purposes.
     * @update	gess5/11/98
     * @return  const char* containing class name
     */
    virtual const char* getClassName(void);

    /**
     * perform self test.
     * @update	gess5/11/98
     */
    virtual void selfTest(void);

protected:
    int				    mTypeID;
    int				    mAttrCount;
    nsCAutoString	mTextValue;
};



#endif


