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

#include "prtypes.h"
#include "nsString.h"
#include <iostream.h>

class CScanner;

/**
 *  Token objects represent sequences of characters as they
 *  are consumed from the input stream (URL). While they're
 *  pretty general in nature, we use subclasses (found in
 *  nsHTMLTokens.h) to define <start>, </end>, <text>,
 *  <comment>, <&entity>, <newline>, and <whitespace> tokens.
 *  
 *  @update  gess 3/25/98
 */
class CToken {
  public:

    /**
     * Default constructor
     * @update	gess5/11/98
     * @param   aName is the given name of the token 
     */
    CToken(const nsString& aName);

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
    ~CToken();
    
    /**
     * Retrieve string value of the token
     * @update	gess5/11/98
     * @return  reference to string containing string value
     */
    virtual nsString& GetStringValue(void);

    /**
     * Get text of this token
     * @update	gess5/11/98
     * @return  string ref containing text value of this token
     */
    virtual nsString& GetText(void);

    /**
     * Setter method that changes the string value of this token
     * @update	gess5/11/98
     * @param   name is a char* value containing new string value
     */
    virtual void SetStringValue(const char* name);
    
    /**
     * Sets the ordinal value of this token (not currently used)
     * @update	gess5/11/98
     * @param   value is the new ord value for this token
     */
    virtual void SetOrdinal(PRInt16 value);

    /**
     * Getter which retrieves the current ordinal value for this token
     * @update	gess5/11/98
     * @return  current ordinal value 
     */
    virtual PRInt16 GetOrdinal(void);

    /**
     * Sets the # of attributes found for this token.
     * @update	gess5/11/98
     * @param   value is the attr count
     */
    virtual void SetAttributeCount(PRInt16 aValue);

    /**
     * Getter which retrieves the current attribute count for this token
     * @update	gess5/11/98
     * @return  current attribute count 
     */
    virtual PRInt16 GetAttributeCount(void);

    /**
     * Causes token to consume data from given scanner.
     * Note that behavior varies wildly between CToken subclasses.
     * @update	gess5/11/98
     * @param   aChar -- most recent char consumed
     * @param   aScanner -- input source where token should get data
     * @return  error code (0 means ok)
     */
    virtual PRInt32 Consume(PRUnichar aChar,CScanner& aScanner);

    /**
     * Causes token to dump itself in debug form to given output stream
     * @update	gess5/11/98
     * @param   out is the output stream where token should write itself
     */
    virtual void DebugDumpToken(ostream& out);

    /**
     * Causes token to dump itself in source form to given output stream
     * @update	gess5/11/98
     * @param   out is the output stream where token should write itself
     */
    virtual void DebugDumpSource(ostream& out);

    /**
     * Getter which retrieves type of token
     * @update	gess5/11/98
     * @return  int containing token type
     */
    virtual PRInt32 GetTokenType(void);

    /**
     * Getter which retrieves the class name for this token 
     * This method is only used for debug purposes.
     * @update	gess5/11/98
     * @return  const char* containing class name
     */
    virtual const char* GetClassName(void);

    /**
     * perform self test.
     * @update	gess5/11/98
     */
    virtual void SelfTest(void);

protected:
            PRInt16       mOrdinalValue;
            PRInt16       mAttrCount;
            nsString      mTextValue;
};

#endif


