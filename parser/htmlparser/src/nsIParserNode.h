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

#ifndef NS_IPARSERNODE__
#define NS_IPARSERNODE__

#include "prtypes.h"
#include "nsString.h"
#include "nsDebug.h"

/**-------------------------------------------------------
 *  Parser nodes are the unit of exchange between the 
 *  parser and the content sink. Nodes offer access to
 *  the current token, its attributes, and its skipped-
 *  content if applicable.
 *  
 *  @update  gess 3/25/98
 *------------------------------------------------------*/
class nsIParserNode {
            
	public:

    virtual const nsString&     GetName() const =0;  //to get name of tag
    virtual const nsString&     GetText() const =0;  //get plain text if available
    virtual const nsString&     GetSkippedContent() const =0;

              //methods for determining the type of parser node...            
    virtual       PRInt32       GetNodeType()  const =0;
    virtual       PRInt32       GetTokenType()  const =0;

              //methods for accessing key/value pairs
    virtual       PRInt32       GetAttributeCount(void) const =0;
    virtual const nsString&     GetKeyAt(PRInt32 anIndex) const =0;
    virtual const nsString&     GetValueAt(PRInt32 anIndex) const =0;

  // When the node is an entity, this will translate the entity to
  // it's unicode value.
  virtual PRInt32 TranslateToUnicode() const = 0;
};

#endif
