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
#include "string.h"
#include "nsHTMLTokens.h"
#include "nshtmlpars.h"
#include "nsITokenizer.h"


static NS_DEFINE_IID(kISupportsIID, NS_ISUPPORTS_IID);                 
static NS_DEFINE_IID(kClassIID, NS_PARSER_NODE_IID); 
static NS_DEFINE_IID(kIParserNodeIID, NS_IPARSER_NODE_IID); 

nsAutoString& GetEmptyString() {
  static nsAutoString theEmptyString("");
  return theEmptyString;
}

/**
 *  Default constructor
 *  
 *  @update  gess 3/25/98
 *  @param   aToken -- token to init internal token
 *  @return  
 */
nsCParserNode::nsCParserNode(CToken* aToken,PRInt32 aLineNumber,nsITokenRecycler* aRecycler): nsIParserNode() {
  NS_INIT_REFCNT();
  mAttributeCount=0;
  mLineNumber=aLineNumber;
  mToken=aToken;
  memset(mAttributes,0,sizeof(mAttributes));
  mSkippedContent=0;
  mRecycler=aRecycler;
}


/**
 *  default destructor
 *  NOTE: We intentionally DONT recycle mToken here.
 *        It may get cached for use elsewhere
 *  @update  gess 3/25/98
 *  @param   
 *  @return  
 */
nsCParserNode::~nsCParserNode() {
  if(mRecycler) {
    PRUint32 index=0;
    for(index=0;index<mAttributeCount;index++){
      mRecycler->RecycleToken(mAttributes[index]);
    }
    if(mSkippedContent)
      mRecycler->RecycleToken(mSkippedContent);
  }
}

NS_IMPL_ADDREF(nsCParserNode)
NS_IMPL_RELEASE(nsCParserNode)

/**
 *  Init
 *  
 *  @update  gess 3/25/98
 *  @param   
 *  @return  
 */

nsresult nsCParserNode::Init(CToken* aToken,PRInt32 aLineNumber)  
{
  mLineNumber=aLineNumber;
  mToken=aToken;
  return NS_OK;
}

/**
 *  This method gets called as part of our COM-like interfaces.
 *  Its purpose is to create an interface to parser object
 *  of some type.
 *  
 *  @update   gess 3/25/98
 *  @param    nsIID  id of object to discover
 *  @param    aInstancePtr ptr to newly discovered interface
 *  @return   NS_xxx result code
 */
nsresult nsCParserNode::QueryInterface(const nsIID& aIID, void** aInstancePtr)  
{                                                                        
  if (NULL == aInstancePtr) {                                            
    return NS_ERROR_NULL_POINTER;                                        
  }                                                                      

  if(aIID.Equals(kISupportsIID))    {  //do IUnknown...
    *aInstancePtr = (nsIParserNode*)(this);                                        
  }
  else if(aIID.Equals(kIParserNodeIID)) {  //do IParser base class...
    *aInstancePtr = (nsIParserNode*)(this);                                        
  }
  else if(aIID.Equals(kClassIID)) {  //do this class...
    *aInstancePtr = (nsCParserNode*)(this);                                        
  }                 
  else {
    *aInstancePtr=0;
    return NS_NOINTERFACE;
  }
  NS_ADDREF_THIS();
  return NS_OK;                                                        
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
  NS_PRECONDITION(mAttributeCount<PRInt32(sizeof(mAttributes)), "Buffer overrun!");
  NS_PRECONDITION(0!=aToken, "Error: Token shouldn't be null!");

  if(mAttributeCount<eMaxAttr) {
    if(aToken) {
      mAttributes[mAttributeCount++]=aToken;
    }
  }
}

/**
 *  This method gets called when the parser encounters 
 *  skipped content after a start token.
 *  
 *  @update  gess 3/26/98
 *  @param   aToken -- really a skippedcontent token
 *  @return  nada
 */
void nsCParserNode::SetSkippedContent(CToken* aToken){
  if(aToken) {
    NS_ASSERTION(eToken_skippedcontent == aToken->GetTokenType(), "not a skipped content token");
    mSkippedContent = aToken;
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
  return GetEmptyString();
  // return mName;
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
  return mToken->GetStringValueXXX();
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
  if (nsnull != mSkippedContent) {
    return ((CSkippedContentToken*)mSkippedContent)->GetKey();
  }
  return GetEmptyString();
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
  return mToken->GetTypeID(); 
}


/**
 *  Gets the token type, which corresponds to a value from
 *  eHTMLTokens_xxx.
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
PRInt32 nsCParserNode::GetAttributeCount(PRBool askToken) const{
  if(PR_TRUE==askToken)
    return mToken->GetAttributeCount();
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
const nsString& nsCParserNode::GetKeyAt(PRUint32 anIndex) const {
  if(anIndex<mAttributeCount) {
    CAttributeToken* tkn=(CAttributeToken*)(mAttributes[anIndex]);
    return tkn->GetKey();
  }
  return GetEmptyString();
}


/**
 *  Retrieve the string rep of the attribute at given offset
 *  
 *  @update  gess 3/25/98
 *  @param   anIndex-- offset of attribute to retrieve
 *  @return  string rep of given attribute text value
 */
const nsString& nsCParserNode::GetValueAt(PRUint32 anIndex) const {
  NS_PRECONDITION(anIndex<mAttributeCount, "Bad attr index");
  if(anIndex<mAttributeCount){
    return (mAttributes[anIndex])->GetStringValueXXX();
  }
  return GetEmptyString();
}


PRInt32 nsCParserNode::TranslateToUnicodeStr(nsString& aString) const
{
  if (eToken_entity == mToken->GetTokenType()) {
    return ((CEntityToken*)mToken)->TranslateToUnicodeStr(aString);
  }
  return -1;
}

/**
 * This getter retrieves the line number from the input source where
 * the token occured. Lines are interpreted as occuring between \n characters.
 * @update	gess7/24/98
 * @return  int containing the line number the token was found on
 */
PRInt32 nsCParserNode::GetSourceLineNumber(void) const {
  return mLineNumber;
}

