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
#include "CScanner.h"


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
    CToken(const nsAFlatCString& aName);

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
    virtual void reinitialize(int aTag, const nsAFlatCString& aString);
    
    /**
     * Retrieve string value of the token
     * @update	gess5/11/98
     * @return  reference to string containing string value
     */
    virtual nsAFlatCString& getStringValue(void);

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
    virtual int consume(char aChar,CScanner& aScanner);

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
    int           mTypeID;
    int           mAttrCount;
    nsCAutoString mTextValue;
};



#endif


