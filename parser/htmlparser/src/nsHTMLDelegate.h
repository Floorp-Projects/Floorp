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

#ifndef	TOKENIZER_DELEGATE
#define	TOKENIZER_DELEGATE

#include "nsHTMLTokens.h"
#include "nsITokenizerDelegate.h"
#include "nsDeque.h"

#define	MAXCOMMANDLEN (18)


class CHTMLTokenizerDelegate : public ITokenizerDelegate {
	public:
    							      CHTMLTokenizerDelegate();

      virtual	CToken*	  GetToken(CScanner* aScanner,PRInt32& anErrorCode);
      virtual PRBool    WillAddToken(CToken& aToken);

		  virtual	PRBool    WillTokenize();
		  virtual	PRBool    DidTokenize();
      static void       SelfTest();


   protected:

					    CToken* 	ConsumeTag(PRUnichar aChar,CScanner& aScanner,PRInt32& anErrorCode);
					    CToken* 	ConsumeStartTag(PRUnichar aChar,CScanner& aScanner,PRInt32& anErrorCode);
					    void		  ConsumeAttributes(PRUnichar aChar,CScanner& aScanner,PRInt32& anErrorCode);
					    CToken* 	ConsumeText(const nsString& aString,CScanner& aScanner,PRInt32& anErrorCode);
					    CToken* 	ConsumeEntity(PRUnichar aChar,CScanner& aScanner,PRInt32& anErrorCode);
#ifdef TOKENIZE_WHITESPACE
					    CToken* 	ConsumeWhitespace(PRUnichar aChar,CScanner& aScanner,PRInt32& anErrorCode);
#endif
					    CToken* 	ConsumeComment(PRUnichar aChar,CScanner& aScanner,PRInt32& anErrorCode);
#ifdef TOKENIZE_CRLF
					    CToken* 	ConsumeNewline(PRUnichar aChar,CScanner& aScanner,PRInt32& anErrorCode);
#endif

                    //the only special case method...
              CToken*   ConsumeContentToEndTag(const nsString& aString,PRUnichar aChar,CScanner& aScanner,PRInt32& anErrorCode);

              CDeque    mTokenDeque;

};

#endif

