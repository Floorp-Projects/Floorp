
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
 * This file contains the declarations for all the 
 * HTML specific token types that our HTML tokenizer
 * delegate understands. 
 *
 * If you want to add a new kind of token, this is 
 * the place to do it. You should also add a bit of glue
 * code to the HTML tokenizer delegate class.
 */

#ifndef HTMLTOKENS_H
#define HTMLTOKENS_H

#include "nsToken.h"
#include "nsHTMLTags.h"
#include <iostream.h>

class CScanner;

enum eHTMLTokenTypes {
  eToken_unknown=0,
  eToken_start=1,     eToken_end,     eToken_comment,         eToken_entity,
  eToken_whitespace,  eToken_newline, eToken_text,            eToken_attribute,
  eToken_script,      eToken_style,   eToken_skippedcontent,  
  eToken_last //make sure this stays the last token...
};

#define eHTMLTags nsHTMLTag

PRInt32         ConsumeQuotedString(PRUnichar aChar,nsString& aString,CScanner& aScanner);
PRInt32         ConsumeAttributeText(PRUnichar aChar,nsString& aString,CScanner& aScanner);
const char*     GetTagName(PRInt32 aTag);
//PRInt32         FindEntityIndex(nsString& aString,PRInt32 aCount=-1);


/**
 *  This declares the basic token type used in the html-
 *  parser.
 *  
 *  @update  gess 3/25/98
 */
class CHTMLToken : public CToken {
public:
                        CHTMLToken(eHTMLTags aTag);
                        CHTMLToken(const nsString& aString);
protected:
};


/**
 *  This declares start tokens, which always take the 
 *  form <xxxx>. This class also knows how to consume
 *  related attributes.
 *  
 *  @update  gess 3/25/98
 */
class CStartToken: public CHTMLToken {
  public:
                        CStartToken(eHTMLTags aTag);
                        CStartToken(const nsString& aString);
    virtual PRInt32     Consume(PRUnichar aChar,CScanner& aScanner);
    virtual PRInt32     GetTypeID(void);
    virtual const char* GetClassName(void);
    virtual PRInt32     GetTokenType(void);
            PRBool      IsAttributed(void);
            void        SetAttributed(PRBool aValue);
            PRBool      IsEmpty(void);
            void        SetEmpty(PRBool aValue);
    virtual void        DebugDumpSource(ostream& out);
  
  protected:
            PRBool      mAttributed;      
            PRBool      mEmpty;      
};


/**
 *  This declares end tokens, which always take the 
 *  form </xxxx>. This class also knows how to consume
 *  related attributes.
 *  
 *  @update  gess 3/25/98
 */
class CEndToken: public CHTMLToken {
  public:
                        CEndToken(const nsString& aString);
    virtual PRInt32     Consume(PRUnichar aChar,CScanner& aScanner);
    virtual PRInt32     GetTypeID(void);
    virtual const char* GetClassName(void);
    virtual PRInt32     GetTokenType(void);
    virtual void        DebugDumpSource(ostream& out);
};


/**
 *  This declares comment tokens. Comments are usually 
 *  thought of as tokens, but we treat them that way 
 *  here so that the parser can have a consistent view
 *  of all tokens.
 *  
 *  @update  gess 3/25/98
 */
class CCommentToken: public CHTMLToken {
  public:
                        CCommentToken(const nsString& aString);
    virtual PRInt32     Consume(PRUnichar aChar,CScanner& aScanner);
    virtual const char* GetClassName(void);
    virtual PRInt32     GetTokenType(void);
            char        mLeadingChar;
};


/**
 *  This class declares entity tokens, which always take
 *  the form &xxxx;. This class also offers a few utility
 *  methods that allow you to easily reduce entities.
 *  
 *  @update  gess 3/25/98
 */
class CEntityToken : public CHTMLToken {
  public:
                        CEntityToken(const nsString& aString);
    virtual const char* GetClassName(void);
    virtual PRInt32     GetTokenType(void);
            PRInt32     TranslateToUnicodeStr(nsString& aString);
    virtual PRInt32     Consume(PRUnichar aChar,CScanner& aScanner);
    static  PRInt32     ConsumeEntity(PRUnichar aChar,nsString& aString,CScanner& aScanner);
    static  PRInt32     TranslateToUnicodeStr(PRInt32 aValue,nsString& aString);
    static  PRInt32     FindEntityIndex(nsString& aString);
    static  PRInt32     FindEntityIndexMax(const char* aBuffer,PRInt32 aCount=-1);
    static  PRBool      VerifyEntityTable(void);
    static  PRInt32     ReduceEntities(nsString& aString);
    virtual  void       DebugDumpSource(ostream& out);

  private:
    static  PRInt32     mEntityTokenCount;
};


/**
 *  Whitespace tokens are used where whitespace can be 
 *  detected as distinct from text. This allows us to 
 *  easily skip leading/trailing whitespace when desired.
 *  
 *  @update  gess 3/25/98
 */
class CWhitespaceToken: public CHTMLToken {
  public:
                        CWhitespaceToken(const nsString& aString);
    virtual PRInt32     Consume(PRUnichar aChar,CScanner& aScanner);
    virtual const char* GetClassName(void);
    virtual PRInt32     GetTokenType(void);
};

/**
 *  Text tokens contain the normalized form of html text.
 *  These tokens are guaranteed not to contain entities,
 *  start or end tags, or newlines.
 *  
 *  @update  gess 3/25/98
 */
class CTextToken: public CHTMLToken {
  public:
                        CTextToken(const nsString& aString);
    virtual PRInt32     Consume(PRUnichar aChar,CScanner& aScanner);
    virtual const char* GetClassName(void);
    virtual PRInt32     GetTokenType(void);
};


/**
 *  Attribute tokens are used to contain attribute key/value
 *  pairs whereever they may occur. Typically, they should
 *  occur only in start tokens. However, we may expand that
 *  ability when XML tokens become commonplace.
 *  
 *  @update  gess 3/25/98
 */
class CAttributeToken: public CHTMLToken {
  public:
                          CAttributeToken(const nsString& aString);
    virtual PRInt32       Consume(PRUnichar aChar,CScanner& aScanner);
    virtual const char*   GetClassName(void);
    virtual PRInt32       GetTokenType(void);
    virtual nsString&     GetKey(void) {return mTextKey;}
    virtual void          DebugDumpToken(ostream& out);
    virtual void          DebugDumpSource(ostream& out);
            PRBool        mLastAttribute;

  protected:
             nsString mTextKey;
};


/**
 *  Newline tokens contain, you guessed it, newlines. 
 *  They consume newline (CR/LF) either alone or in pairs.
 *  
 *  @update  gess 3/25/98
 */
class CNewlineToken: public CHTMLToken { 
  public:
                        CNewlineToken(const nsString& aString);
    virtual PRInt32     Consume(PRUnichar aChar,CScanner& aScanner);
    virtual const char* GetClassName(void);
    virtual PRInt32     GetTokenType(void);
    virtual nsString&   GetText(void);
};


/**
 *  Script tokens contain sequences of javascript (or, gulp,
 *  any other script you care to send). We don't tokenize
 *  it here, nor validate it. We just wrap it up, and pass
 *  it along to the html parser, who sends it (later on) 
 *  to the scripting engine.
 *  
 *  @update  gess 3/25/98
 */
class CScriptToken: public CHTMLToken {
  public:

                        CScriptToken(const nsString& aString);
    virtual const char* GetClassName(void);
    virtual PRInt32     GetTokenType(void);
  protected:
};


/**
 *  Style tokens contain sequences of css style. We don't 
 *  tokenize it here, nor validate it. We just wrap it up, 
 *  and pass it along to the html parser, who sends it 
 *  (later on) to the style engine.
 *  
 *  @update  gess 3/25/98
 */
class CStyleToken: public CHTMLToken {
  public:
                         CStyleToken(const nsString& aString);
    virtual const char*  GetClassName(void);
    virtual PRInt32      GetTokenType(void);
  protected:
};


/**
 *  This is a placeholder token, which is being deprecated.
 *  Don't bother paying attention to this.
 *  
 *  @update  gess 3/25/98
 */
class CSkippedContentToken: public CAttributeToken {
  public:
                        CSkippedContentToken(const nsString& aString);
    virtual PRInt32     Consume(PRUnichar aChar,CScanner& aScanner);
    virtual const char* GetClassName(void);
    virtual PRInt32     GetTokenType(void);
  protected:
};


#endif




