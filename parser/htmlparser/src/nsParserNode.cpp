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


#include "nsParserNode.h" 
#include "nsHTMLParser.h"
#include "string.h"


/**
 *  Default constructor
 *  
 *  @update  gess 3/25/98
 *  @param   aToken -- token to init internal token
 *  @return  
 */
nsCParserNode::nsCParserNode(CToken* aToken): nsIParserNode(), 
  mName(), mEmptyString() {
  NS_PRECONDITION(0!=aToken, "Null Token");
  mAttributeCount=0;
  mToken=(CHTMLToken*)aToken;
  memset(mAttributes,0,sizeof(mAttributes));
}


/**
 *  default destructor
 *  
 *  @update  gess 3/25/98
 *  @param   
 *  @return  
 */
nsCParserNode::~nsCParserNode() {
}


/**
 *  Causes the given attribute to be added to internal 
 *  mAttributes list, and mAttributeCount to be incremented.
 *  
 *  @update  gess 3/25/98
 *  @param   aToken -- token to be added to attr list
 *  @return  
 */
void nsCParserNode::AddAttribute(CToken* aToken) {
  NS_PRECONDITION(mAttributeCount<sizeof(mAttributes), "Buffer overrun!");
  NS_PRECONDITION(0!=aToken, "Error: Token shouldn't be null!");
  if(aToken) {
    mAttributes[mAttributeCount++]=(CHTMLToken*)aToken;
  }
}


/**
 *  This method gets called when the parser encounters 
 *  skipped content after a start token.
 *  NOTE: To determine if we have skipped content, simply
 *        check mAttributes[mAttributeCount].
 *  
 *  @update  gess 3/26/98
 *  @param   aToken -- really a skippedcontent token
 *  @return  nada
 */
void nsCParserNode::SetSkippedContent(CToken* aToken){
  NS_PRECONDITION(mAttributeCount<sizeof(mAttributes)-1, "Buffer overrun!");
  NS_PRECONDITION(0!=aToken, "Error: Token shouldn't be null!");
  if(aToken) {
    mAttributes[mAttributeCount++]=(CHTMLToken*)aToken;
  }
}


/**
 *  Gets the name of this node. Currently unused.
 *  
 *  @update  gess 3/25/98
 *  @param   
 *  @return  string ref containing node name
 */
const nsString& nsCParserNode::GetName() const {
  return mName;
}


/**
 *  Get text value of this node, which translates into 
 *  getting the text value of the underlying token
 *  
 *  @update  gess 3/25/98
 *  @param   
 *  @return  string ref of text from internal token
 */
const nsString& nsCParserNode::GetText() const {
  return mToken->GetText();
}

/**
 *  Get text value of this node, which translates into 
 *  getting the text value of the underlying token
 *  
 *  @update  gess 3/25/98
 *  @param   
 *  @return  string ref of text from internal token
 */
const nsString& nsCParserNode::GetSkippedContent() const {
  if (0 < mAttributeCount) {
    if(mAttributes[mAttributeCount-1]) {
      CSkippedContentToken* sc=(CSkippedContentToken*)(mAttributes[mAttributeCount-1]);
      if(sc) {
        return sc->GetText();
      }
    }
  }
  return mEmptyString;
}

/**
 *  Get node type, meaning, get the tag type of the 
 *  underlying token
 *  
 *  @update  gess 3/25/98
 *  @param   
 *  @return  int value that represents tag type
 */
PRInt32 nsCParserNode::GetNodeType(void) const{
  return mToken->GetHTMLTag(); 
}


/**
 *  Gets the token type, which corresponds to a value from
 *  eHTMLTags_xxx.
 *  
 *  @update  gess 3/25/98
 *  @param   
 *  @return  
 */
PRInt32 nsCParserNode::GetTokenType(void) const{
  return mToken->GetTokenType();
}


/**
 *  Retrieve the number of attributes on this node
 *  
 *  @update  gess 3/25/98
 *  @param   
 *  @return  int -- representing attribute count
 */
PRInt32 nsCParserNode::GetAttributeCount(void) const{
  return mAttributeCount;
}

/**
 *  Retrieve the string rep of the attribute key at the
 *  given index.
 *  
 *  @update  gess 3/25/98
 *  @param   anIndex-- offset of attribute to retrieve
 *  @return  string rep of given attribute text key
 */
const nsString& nsCParserNode::GetKeyAt(PRInt32 anIndex) const {
  NS_PRECONDITION(anIndex<mAttributeCount, "Bad attr index");
  CAttributeToken* tkn=(CAttributeToken*)(mAttributes[anIndex]);
  return tkn->GetKey();
}


/**
 *  Retrieve the string rep of the attribute at given offset
 *  
 *  @update  gess 3/25/98
 *  @param   anIndex-- offset of attribute to retrieve
 *  @return  string rep of given attribute text value
 */
const nsString& nsCParserNode::GetValueAt(PRInt32 anIndex) const {
  NS_PRECONDITION(anIndex<mAttributeCount, "Bad attr index");
  return (mAttributes[anIndex])->GetText();
}


PRInt32 nsCParserNode::TranslateToUnicodeStr(nsString& aString) const
{
  if (eToken_entity == mToken->GetTokenType()) {
    return ((CEntityToken*)mToken)->TranslateToUnicodeStr(aString);
  }
  return -1;
}


