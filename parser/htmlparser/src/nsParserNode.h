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
 * This class is defines the basic interface between the
 * parser and the content sink. The parser will iterate
 * over the collection of tokens that it sees from the
 * tokenizer, coverting each related "group" into one of
 * these. This object gets passed to the sink, and is
 * then immediately reused.
 *
 * If you want to hang onto one of these, you should
 * make your own copy.
 *
 */

#ifndef NS_PARSERNODE__
#define NS_PARSERNODE__

#include "nsIParserNode.h"
#include "nsHTMLTokens.h"
#include "nsString.h"

class nsHTMLParser;

class nsCParserNode :  public nsIParserNode {
            
  public:

                                nsCParserNode(CHTMLToken* aToken);
                                ~nsCParserNode();
    virtual const nsString&     GetName() const;  //to get name of tag
    virtual const nsString&     GetText() const;  //get plain text if available
    virtual const nsString&     GetSkippedContent() const;

              //methods for determining the type of parser node...            
    virtual PRInt32             GetNodeType() const;
    virtual PRInt32             GetTokenType() const;

              //methods for accessing key/value pairs
    virtual PRInt32             GetAttributeCount(void) const;
    virtual const nsString&     GetKeyAt(PRInt32 anIndex) const;
    virtual const nsString&     GetValueAt(PRInt32 anIndex) const;

    virtual void                AddAttribute(CHTMLToken* aToken);
    virtual void                SetSkippedContent(CHTMLToken* aToken);

              // misc
    virtual PRInt32             TranslateToUnicodeStr(nsString& aString) const;

  protected:
                  PRInt32       mAttributeCount;    
                  CHTMLToken*   mToken;
                  CHTMLToken*   mAttributes[20]; // XXX Ack! This needs to be dynamic! 
                  nsString      mName;
                  nsString      mEmptyString;

};

#endif


