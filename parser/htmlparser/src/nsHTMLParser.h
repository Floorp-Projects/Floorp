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
 *  This class does two primary jobs:
 *    1) It iterates the tokens provided during the 
 *       tokenization process, identifing where elements
 *       begin and end (doing validation and normalization).
 *    2) It controls and coordinates with an instance of
 *       the IContentSink interface, to coordinate the
 *       the production of the content model.
 *
 *  The basic operation of this class assumes that an HTML
 *  document is non-normalized. Therefore, we don't process
 *  the document in a normalized way. Don't bother to look
 *  for methods like: doHead() or doBody().
 *
 *  Instead, in order to be backward compatible, we must
 *  scan the set of tokens and perform this basic set of
 *  operations:
 *    1)  Determine the token type (easy, since the tokens know)
 *    2)  Determine the appropriate section of the HTML document
 *        each token belongs in (HTML,HEAD,BODY,FRAMESET).
 *    3)  Insert content into our document (via the sink) into
 *        the correct section.
 *    4)  In the case of tags that belong in the BODY, we must
 *        ensure that our underlying document state reflects
 *        the appropriate context for our tag. 
 *
 *        For example,if we see a <TR>, we must ensure our 
 *        document contains a table into which the row can
 *        be placed. This may result in "implicit containers" 
 *        created to ensure a well-formed document.
 *         
 */

#ifndef NS_HTMLPARSER__
#define NS_HTMLPARSER__

#include "nsIParser.h"
#include "nsDeque.h"
#include "nsHTMLTokens.h"
#include "nsParserNode.h"
#include "nsTokenHandler.h"
#include "nsParserTypes.h"


#define NS_IHTML_PARSER_IID      \
  {0x2ce606b0, 0xbee6,  0x11d1,  \
  {0xaa, 0xd9, 0x00,    0x80, 0x5f, 0x8a, 0x3e, 0x14}}


class CTokenizer;
class IContentSink;
class nsIHTMLContentSink;
class nsIURL;
class nsHTMLDTD;


class nsHTMLParser : public nsIParser {
            
	public:
friend class CTokenHandler;

    NS_DECL_ISUPPORTS
						                    nsHTMLParser();
                                ~nsHTMLParser();
    virtual nsIContentSink*     SetContentSink(nsIContentSink* aSink);
    virtual PRBool              Parse(nsIURL* aURL);
    virtual PRBool              Parse(nsIURL* aURL,eParseMode aMode);
    virtual PRBool              ResumeParse();


            PRBool              HandleStartToken(CToken* aToken);
            PRBool              HandleEndToken(CToken* aToken);
            PRBool              HandleEntityToken(CToken* aToken);
            PRBool              HandleNewlineToken(CToken* aToken);
            PRBool              HandleCommentToken(CToken* aToken);
            PRBool              HandleWhitespaceToken(CToken* aToken);
            PRBool              HandleTextToken(CToken* aToken);
            PRBool              HandleSkippedContentToken(CToken* aToken);
            PRBool              HandleAttributeToken(CToken* aToken);
            PRBool              HandleScriptToken(CToken* aToken);
            PRBool              HandleStyleToken(CToken* aToken);

            PRBool              IsWithinBody(void) const;

  protected:
            PRBool              IterateTokens();
            eHTMLTags           NodeAt(PRInt32 aPos) const;
            eHTMLTags           GetTopNode() const;
            PRInt32             GetStackPos() const;

            PRInt32             CollectAttributes(nsCParserNode& aNode);
            PRInt32             CollectSkippedContent(nsCParserNode& aNode);
            void                InitializeDefaultTokenHandlers();
            CTokenHandler*      GetTokenHandler(const nsString& aString) const;
            CTokenHandler*      GetTokenHandler(eHTMLTokenTypes aType) const;
            CTokenHandler*      AddTokenHandler(CTokenHandler* aHandler);
            nsHTMLParser&       DeleteTokenHandlers(void);

  protected:

                    //these cover methods mimic the sink, and are used
                    //by the parser to manage its context-stack.

            PRBool              OpenHTML(const nsIParserNode& aNode);
            PRBool              CloseHTML(const nsIParserNode& aNode);
            PRBool              OpenHead(const nsIParserNode& aNode);
            PRBool              CloseHead(const nsIParserNode& aNode);
            PRBool              OpenBody(const nsIParserNode& aNode);
            PRBool              CloseBody(const nsIParserNode& aNode);
            PRBool              OpenForm(const nsIParserNode& aNode);
            PRBool              CloseForm(const nsIParserNode& aNode);
            PRBool              OpenFrameset(const nsIParserNode& aNode);
            PRBool              CloseFrameset(const nsIParserNode& aNode);
            PRBool              OpenContainer(const nsIParserNode& aNode);
            PRBool              CloseContainer(const nsIParserNode& aNode);
            PRBool              CloseTopmostContainer();
            PRBool              CloseContainersTo(eHTMLTags aTag);
            PRBool              CloseContainersTo(PRInt32 anIndex);
            PRBool              AddLeaf(const nsIParserNode& aNode);
            PRBool              IsOpen(eHTMLTags aTag) const;
            PRInt32             GetTopmostIndex(eHTMLTags aTag) const;
            PRBool              ReduceContextStackFor(PRInt32 aChildTag);
            PRBool              CreateContextStackFor(PRInt32 aChildTag);

            nsIHTMLContentSink* mSink;
            CTokenizer*         mTokenizer;

            eHTMLTags           mContextStack[50];
            PRInt32             mContextStackPos;

            CTokenHandler*      mTokenHandlers[100];
            PRInt32             mTokenHandlerCount;
            nsDequeIterator*    mCurrentPos;

            nsHTMLDTD*          mDTD;
            eParseMode          mParseMode;
};


#endif 

