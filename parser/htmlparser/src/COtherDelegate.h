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
 * This class is used as the HTML tokenizer delegate.
 *
 * The tokenzier class has the smarts to open an source,
 * and iterate over its characters to produce a list of
 * tokens. The tokenizer doesn't know HTML, which is 
 * where this delegate comes into play.
 *
 * The tokenizer calls methods on this class to help
 * with the creation of HTML-specific tokens from a source 
 * stream.
 *
 * The interface here is very simple, mainly the call
 * to GetToken(), which Consumes bytes from the underlying
 * scanner.stream, and produces an HTML specific CToken.
 */

#ifndef  _OTHER_DELEGATE
#define  _OTHER_DELEGATE

#include "nsHTMLTokens.h"
#include "nsITokenizerDelegate.h"
#include "nsDeque.h"
#include "nsIDTD.h"

class COtherDelegate : public ITokenizerDelegate {
public:


    /**
     * Default constructor
     * @update	gess5/11/98
     */
    COtherDelegate();


    /**
     * Copy constructor
     * @update	gess 5/11/98
     */
    COtherDelegate(COtherDelegate& aDelegate);

    /**
     * Consume next token from given scanner.
     * @update	gess 5/11/98
     * @param   aScanner is the input source
     * @param   aToken is the next token (or null)
     * @return  error code
     */
    virtual PRInt32 GetToken(CScanner& aScanner,CToken*& aToken);

    /**
     * Ask if its ok to add this token
     * @update	gess 5/11/98
     * @param   aToken is the token to be added
     * @return  True if ok to add the given token
     */
    virtual PRBool WillAddToken(CToken& aToken);

    /**
     * Called as a preprocess -- tells delegate that tokenization will begin
     * @update	gess 5/11/98
     * @param   aIncremental tells us if tokenization is incremental
     * @return  TRUE if ok to continue -- FALSE if process should stop
     */
    virtual PRBool WillTokenize(PRBool aIncremental);

    /**
     * Postprocess -- called to say that tokenization has concluded
     * @update	gess 5/11/98
     * @param   aIncremental tells us if tokenization was incremental
     * @return  TRUE if all went well--FALSE if you encountered an error
     */
    virtual PRBool DidTokenize(PRBool aIncremental);

    /**
     * DEPRECATED. Tells us what mode the delegate is operating in.
     * @update	gess 5/11/98
     * @return  parse mode
     */
    virtual eParseMode  GetParseMode(void) const;

    /**
     * Retrieve the DTD required by this delegate
     * (The parser will call this prior to tokenization)
     * @update	gess 5/11/98
     * @return  ptr to DTD -- should NOT be null.
     */
    virtual nsIDTD*     GetDTD(void) const;

    /**
     * This function deletes the actual delegate and cleans up
     * any referenced memory
     * @update jevering 06/15/98
     * @param
     * @return
     */

    virtual void        Destroy(void);

    /**
     * Conduct self test.
     * @update	gess 5/11/98
     */
    static  void        SelfTest();

protected:

    /**
     * Called to cause delegate to create a token of given type.
     * @update	gess 5/11/98
     * @param   aType represents the kind of token you want to create.
     * @return  new token or NULL
     */
    virtual CToken*     CreateTokenOfType(eHTMLTokenTypes aType);

    /**
     * Retrieve the next TAG from the given scanner.
     * @update	gess 5/11/98
     * @param   aScanner is the input source
     * @param   aToken is the next token (or null)
     * @return  error code
     */
    PRInt32     ConsumeTag(PRUnichar aChar,CScanner& aScanner,CToken*& aToken);
    
    /**
     * Retrieve next START tag from given scanner.
     * @update	gess 5/11/98
     * @param   aScanner is the input source
     * @param   aToken is the next token (or null)
     * @return  error code
     */
    PRInt32     ConsumeStartTag(PRUnichar aChar,CScanner& aScanner,CToken*& aToken);
    
    /**
     * Retrieve collection of HTML/XML attributes from given scanner
     * @update	gess 5/11/98
     * @param   aScanner is the input source
     * @param   aToken is the next token (or null)
     * @return  error code
     */
    PRInt32     ConsumeAttributes(PRUnichar aChar,CScanner& aScanner,CToken*& aToken);
    
    /**
     * Retrieve a sequence of text from given scanner.
     * @update	gess 5/11/98
     * @param   aString will contain retrieved text.
     * @param   aScanner is the input source
     * @param   aToken is the next token (or null)
     * @return  error code
     */
    PRInt32     ConsumeText(const nsString& aString,CScanner& aScanner,CToken*& aToken);
    
    /**
     * Retrieve an entity from given scanner
     * @update	gess 5/11/98
     * @param   aChar last char read from scanner
     * @param   aScanner is the input source
     * @param   aToken is the next token (or null)
     * @return  error code
     */
    PRInt32     ConsumeEntity(PRUnichar aChar,CScanner& aScanner,CToken*& aToken);
    
    /**
     * Retrieve a whitespace sequence from the given scanner
     * @update	gess 5/11/98
     * @param   aChar last char read from scanner
     * @param   aScanner is the input source
     * @param   aToken is the next token (or null)
     * @return  error code
     */
    PRInt32     ConsumeWhitespace(PRUnichar aChar,CScanner& aScanner,CToken*& aToken);
    
    /**
     * Retrieve a comment from the given scanner
     * @update	gess 5/11/98
     * @param   aChar last char read from scanner
     * @param   aScanner is the input source
     * @param   aToken is the next token (or null)
     * @return  error code
     */
    PRInt32     ConsumeComment(PRUnichar aChar,CScanner& aScanner,CToken*& aToken);

    /**
     * Retrieve newlines from given scanner
     * @update	gess 5/11/98
     * @param   aChar last char read from scanner
     * @param   aScanner is the input source
     * @param   aToken is the next token (or null)
     * @return  error code
     */
    PRInt32     ConsumeNewline(PRUnichar aChar,CScanner& aScanner,CToken*& aToken);

    /**
     * Causes content to be skipped up to sequence contained in aString.
     * @update	gess 5/11/98
     * @param   aString ????
     * @param   aChar last char read from scanner
     * @param   aScanner is the input source
     * @param   aToken is the next token (or null)
     * @return  error code
     */
    virtual PRInt32     ConsumeContentToEndTag(const nsString& aString,PRUnichar aChar,CScanner& aScanner,CToken*& aToken);

    nsDeque     mTokenDeque;

};

#endif


