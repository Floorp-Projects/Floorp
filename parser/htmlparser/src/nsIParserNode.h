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

#include "nshtmlpars.h"
#include "nsISupports.h"
#include "prtypes.h"
#include "nsString.h"
#include "nsDebug.h"

// class CToken;

// 6e59f160-2717-11d2-9246-00805f8a7ab6
#define NS_IPARSER_NODE_IID      \
  {0x6e59f160, 0x2717,  0x11d1,  \
  {0x92, 0x46, 0x00,    0x80, 0x5f, 0x8a, 0x7a, 0xb6}}

/**
 *  Parser nodes are the unit of exchange between the 
 *  parser and the content sink. Nodes offer access to
 *  the current token, its attributes, and its skipped-
 *  content if applicable.
 *  
 *  @update  gess 3/25/98
 */
class nsIParserNode : public nsISupports {
            
  public:


    /**
     * Retrieve the name of the node
     * @update	gess5/11/98
     * @return  string containing node name
     */
    virtual const nsString& GetName() const =0;  //to get name of tag

    /**
     * Retrieve the text from the given node
     * @update	gess5/11/98
     * @return  string containing node text
     */
    virtual const nsString& GetText() const =0;  //get plain text if available

    /**
     * Retrieve skipped context from node
     * @update	gess5/11/98
     * @return  string containing skipped content
     */
    virtual const nsString& GetSkippedContent() const =0;

    /**
     * Retrieve the type of the parser node.
     * @update	gess5/11/98
     * @return  node type.
     */
    virtual PRInt32 GetNodeType()  const =0;

    /**
     * Retrieve token type of parser node
     * @update	gess5/11/98
     * @return  token type
     */
    virtual PRInt32 GetTokenType()  const =0;

    /**
     * Retrieve the number of attributes in this node.
     * @update	gess5/11/98
     * @return  count of attributes (may be 0)
     */
    virtual PRInt32 GetAttributeCount(PRBool askToken=PR_FALSE) const =0;

    /**
     * Retrieve the key (of key/value pair) at given index
     * @update	gess5/11/98
     * @param   anIndex is the index of the key you want
     * @return  string containing key.
     */
    virtual const nsString& GetKeyAt(PRUint32 anIndex) const =0;

    /**
     * Retrieve the value (of key/value pair) at given index
     * @update	gess5/11/98
     * @param   anIndex is the index of the value you want
     * @return  string containing value.
     */
    virtual const nsString& GetValueAt(PRUint32 anIndex) const =0;

    /**
     * NOTE: When the node is an entity, this will translate the entity
     *       to it's unicode value, and store it in aString.
     * @update	gess5/11/98
     * @param   aString will contain the resulting unicode string value
     * @return  int (unicode char or unicode index from table)
     */
    virtual PRInt32 TranslateToUnicodeStr(nsString& aString) const = 0;

    /**
     * This getter retrieves the line number from the input source where
     * the token occured. Lines are interpreted as occuring between \n characters.
     * @update	gess7/24/98
     * @return  int containing the line number the token was found on
     */
    virtual PRInt32 GetSourceLineNumber(void) const =0;

};

#endif


