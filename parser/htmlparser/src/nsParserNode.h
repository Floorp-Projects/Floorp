/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/*
 * The contents of this file are subject to the Netscape Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
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
#include "nsToken.h"
#include "nsString.h"
#include "nsParserCIID.h"
#include "nsDeque.h"

class nsTokenAllocator;

class nsCParserNode :  public nsIParserNode {
  
  public:

    NS_DECL_ISUPPORTS

    /**
     * Default constructor
     * @update	gess5/11/98
     * @param   aToken is the token this node "refers" to
     */
    nsCParserNode(CToken* aToken=nsnull,PRInt32 aLineNumber=1,nsTokenAllocator* aTokenAllocator=0);

    /**
     * Destructor
     * @update	gess5/11/98
     */
    virtual ~nsCParserNode();

    /**
     * Init
     * @update	gess5/11/98
     */
    virtual nsresult Init(CToken* aToken=nsnull,PRInt32 aLineNumber=1,nsTokenAllocator* aTokenAllocator=0);

    /**
     * Retrieve the name of the node
     * @update	gess5/11/98
     * @return  string containing node name
     */
    virtual const nsString& GetName() const;

    /**
     * Retrieve the text from the given node
     * @update	gess5/11/98
     * @return  string containing node text
     */
    virtual const nsString& GetText() const;

    /**
     * Retrieve skipped context from node
     * @update	gess5/11/98
     * @return  string containing skipped content
     */
    virtual const nsString& GetSkippedContent() const;

    /**
     * Retrieve skipped context from node
     * @update	gess5/11/98
     * @return  string containing skipped content
     */
    virtual void SetSkippedContent(nsString& aString);

    /**
     * Retrieve the type of the parser node.
     * @update	gess5/11/98
     * @return  node type.
     */
    virtual PRInt32 GetNodeType()  const;

    /**
     * Retrieve token type of parser node
     * @update	gess5/11/98
     * @return  token type
     */
    virtual PRInt32 GetTokenType()  const;


    //***************************************
    //methods for accessing key/value pairs
    //***************************************

    /**
     * Retrieve the number of attributes in this node.
     * @update	gess5/11/98
     * @return  count of attributes (may be 0)
     */
    virtual PRInt32 GetAttributeCount(PRBool askToken=PR_FALSE) const;

    /**
     * Retrieve the key (of key/value pair) at given index
     * @update	gess5/11/98
     * @param   anIndex is the index of the key you want
     * @return  string containing key.
     */
    virtual const nsString& GetKeyAt(PRUint32 anIndex) const;

    /**
     * Retrieve the value (of key/value pair) at given index
     * @update	gess5/11/98
     * @param   anIndex is the index of the value you want
     * @return  string containing value.
     */
    virtual const nsString& GetValueAt(PRUint32 anIndex) const;

    /**
     * NOTE: When the node is an entity, this will translate the entity
     *       to it's unicode value, and store it in aString.
     * @update	gess5/11/98
     * @param   aString will contain the resulting unicode string value
     * @return  int (unicode char or unicode index from table)
     */
    virtual PRInt32 TranslateToUnicodeStr(nsString& aString) const;

    /**
     * 
     * @update	gess5/11/98
     * @param 
     * @return
     */
    virtual void AddAttribute(CToken* aToken);

    /**
     * This getter retrieves the line number from the input source where
     * the token occured. Lines are interpreted as occuring between \n characters.
     * @update	gess7/24/98
     * @return  int containing the line number the token was found on
     */
    virtual PRInt32 GetSourceLineNumber(void) const;

    /** This method pop the attribute token from the given index
     * @update	harishd 03/25/99
     * @return  token at anIndex
     */
    virtual CToken* PopAttributeToken();

    /** Retrieve a string containing the tag and its attributes in "source" form
     * @update	rickg 06June2000
     * @return  void
     */
    virtual void GetSource(nsString& aString);

    /*
     * Get and set the ID attribute atom for this node.
     * See http://www.w3.org/TR/1998/REC-xml-19980210#sec-attribute-types
     * for the definition of an ID attribute.
     *
     */
    virtual nsresult GetIDAttributeAtom(nsIAtom** aResult) const;
    virtual nsresult SetIDAttributeAtom(nsIAtom* aID);

    /**
     * This pair of methods allows us to set a generic bit (for arbitrary use)
     * on each node stored in the context.
     * @update	gess 11May2000
     */
    virtual PRBool  GetGenericState(void) const {return mGenericState;}
    virtual void    SetGenericState(PRBool aState) {mGenericState=aState;}

    /** Release all the objects you're holding
     * @update	harishd 08/02/00
     * @return  void
     */
    virtual nsresult ReleaseAll();
    
    PRInt32   mLineNumber;
    CToken*   mToken;
    nsDeque*  mAttributes;
    nsString* mSkippedContent;
    PRInt32   mUseCount;
    PRBool    mGenericState;
    nsCOMPtr<nsIAtom> mIDAttributeAtom;
    
   nsTokenAllocator* mTokenAllocator;
};

#endif


