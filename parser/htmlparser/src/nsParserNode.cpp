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


#include "nsParserNode.h" 
#include "string.h"
#include "nsHTMLTokens.h"
#include "nshtmlpars.h"
#include "nsITokenizer.h"
#include "nsDTDUtils.h"


static NS_DEFINE_IID(kISupportsIID, NS_ISUPPORTS_IID);                 
static NS_DEFINE_IID(kClassIID, NS_PARSER_NODE_IID); 
static NS_DEFINE_IID(kIParserNodeIID, NS_IPARSER_NODE_IID); 

static
const nsString& GetEmptyString() {
  static nsString gEmptyStr;
  return gEmptyStr;
}

/**
 *  Default constructor
 *  
 *  @update  gess 3/25/98
 *  @param   aToken -- token to init internal token
 *  @return  
 */
nsCParserNode::nsCParserNode(CToken* aToken,PRInt32 aLineNumber,nsTokenAllocator* aTokenAllocator): 
    nsIParserNode() {
  NS_INIT_REFCNT();
  MOZ_COUNT_CTOR(nsCParserNode);

  static int theNodeCount=0;
  theNodeCount++;
  mAttributes=0;
  mLineNumber=aLineNumber;
  mToken=aToken;
  IF_HOLD(mToken);
  mTokenAllocator=aTokenAllocator;
  mUseCount=0;
  mSkippedContent=0;
  mGenericState=PR_FALSE;
}

static void RecycleTokens(nsTokenAllocator* aTokenAllocator,nsDeque& aDeque) {
  CToken* theToken=0;
  if(aTokenAllocator) {
    while((theToken=(CToken*)aDeque.Pop())){
      IF_FREE(theToken);
    }
  }
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
  MOZ_COUNT_DTOR(nsCParserNode);
  ReleaseAll();
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

nsresult nsCParserNode::Init(CToken* aToken,PRInt32 aLineNumber,nsTokenAllocator* aTokenAllocator) {
  mLineNumber=aLineNumber;
  mTokenAllocator=aTokenAllocator;
  if(mAttributes && (mAttributes->GetSize()))
    RecycleTokens(mTokenAllocator,*mAttributes);
  mToken=aToken;
  IF_HOLD(mToken);
  mGenericState=PR_FALSE;
  mUseCount=0;
  if(mSkippedContent) {
    mSkippedContent->Truncate();
  }
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
  NS_PRECONDITION(0!=aToken, "Error: Token shouldn't be null!");

  if(!mAttributes)
    mAttributes=new nsDeque(0);
  if(mAttributes) {
    mAttributes->Push(aToken);
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
  if(mSkippedContent)
    return *mSkippedContent;
  return GetEmptyString();
}


/**
 *  Get text value of this node, which translates into 
 *  getting the text value of the underlying token
 *  
 *  @update  gess 3/25/98
 *  @param   
 *  @return  string ref of text from internal token
 */
void nsCParserNode::SetSkippedContent(nsString& aString) {
  if(!mSkippedContent) {
    mSkippedContent=new nsString(aString);
  }
  else *mSkippedContent=aString;
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
  PRInt32 result=0;

  if(PR_FALSE==askToken) {
    if(mAttributes)
      result=mAttributes->GetSize();
  }
  else result=mToken->GetAttributeCount();
  return result;
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
  PRInt32 theCount = (mAttributes) ? mAttributes->GetSize() : 0;
  if((PRInt32)anIndex<theCount) {
    CAttributeToken* tkn=(CAttributeToken*)mAttributes->ObjectAt(anIndex);
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
  PRInt32 theCount = (mAttributes) ? mAttributes->GetSize() : 0;

  NS_PRECONDITION(PRInt32(anIndex)<theCount, "Bad attr index");

  if(PRInt32(anIndex)<theCount) {
    CAttributeToken* tkn=(CAttributeToken*)mAttributes->ObjectAt(anIndex);
    return tkn->GetStringValueXXX();
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

/**
 * This method pop the attribute token
 * @update	harishd 03/25/99
 * @return  token at anIndex
 */

CToken* nsCParserNode::PopAttributeToken() {

  CToken* result=0;
  if(mAttributes) {
    result =(CToken*)mAttributes->Pop();
  }
  return result;
}

/** Retrieve a string containing the tag and its attributes in "source" form
 * @update	rickg 06June2000
 * @return  void
 */
void nsCParserNode::GetSource(nsString& aString) {
  aString.Truncate();

  eHTMLTags theTag=(eHTMLTags)mToken->GetTypeID();
  aString.AppendWithConversion("<");
  const char* theTagName=nsHTMLTags::GetCStringValue(theTag);
  if(theTagName) {
    aString.AppendWithConversion(theTagName);
  }
  if(mAttributes) {
    nsAutoString  theAttrStr;
    int           index=0;
    for(index=0;index<mAttributes->GetSize();index++) {
      CAttributeToken *theToken=(CAttributeToken*)mAttributes->ObjectAt(index);
      if(theToken) {
        theToken->AppendSource(theAttrStr);
        aString.AppendWithConversion(" "); //this will get removed...
      }
    }
    aString.Append(theAttrStr);
  }
  aString.AppendWithConversion(">");
}

/** Release all the objects you're holding to.
 * @update	harishd 08/02/00
 * @return  void
 */
nsresult nsCParserNode::ReleaseAll() {
  if(mAttributes) {

    //fixed a bug that patrick found, where the attributes deque existed
    //but was empty. In that case, the attributes deque itself was leaked.
    //THANKS PATRICK!

    if(mTokenAllocator) {
      RecycleTokens(mTokenAllocator,*mAttributes);
    } 
    else {
      CToken* theToken=(CToken*)mAttributes->Pop();
      while(theToken){
        IF_FREE(theToken);
        theToken=(CToken*)mAttributes->Pop();
      }
    }
    delete mAttributes;
    mAttributes=0;
  }
  if(mSkippedContent) {
    delete mSkippedContent;
  }
  IF_FREE(mToken);
  mSkippedContent=0;
  return NS_OK;
}

nsresult
nsCParserNode::GetIDAttributeAtom(nsIAtom** aResult) const
{
  NS_ENSURE_ARG_POINTER(aResult);
  *aResult = mIDAttributeAtom;
  NS_IF_ADDREF(*aResult);

  return NS_OK;
}

nsresult
nsCParserNode::SetIDAttributeAtom(nsIAtom* aID)
{
  NS_ENSURE_ARG(aID);
  mIDAttributeAtom = aID;

  return NS_OK;
}

