/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: NPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.1 (the "License"); you may not use this file except in
 * compliance with the License. You may obtain a copy of the License at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is 
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the NPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the NPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */


#include "nsParserNode.h" 
#include "string.h"
#include "nsHTMLTokens.h"
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
 *  Default Constructor
 */
nsCParserNode::nsCParserNode()
  : mLineNumber(1),
    mToken(nsnull),
    mAttributes(nsnull),
    mSkippedContent(nsnull),
    mUseCount(0),
    mGenericState(PR_FALSE),
    mTokenAllocator(nsnull)
{
  MOZ_COUNT_CTOR(nsCParserNode);
#ifdef HEAP_ALLOCATED_NODES
  mNodeAllocator=nsnull;
#endif
}

/**
 *  Constructor
 *  
 *  @update  gess 3/25/98
 *  @param   aToken -- token to init internal token
 *  @return  
 */
nsCParserNode::nsCParserNode(CToken* aToken,PRInt32 aLineNumber,nsTokenAllocator* aTokenAllocator,nsNodeAllocator* aNodeAllocator): 
    nsIParserNode() {
  mRefCnt = 0;
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
#ifdef HEAP_ALLOCATED_NODES
  mNodeAllocator=aNodeAllocator;
#endif
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
#ifdef HEAP_ALLOCATED_NODES
  if(mNodeAllocator) {
    mNodeAllocator->Recycle(this);
  }
  mNodeAllocator=nsnull;
#endif
  mTokenAllocator=0;
  mLineNumber=0;
}


/**
 *  Init
 *  
 *  @update  gess 3/25/98
 *  @param   
 *  @return  
 */

nsresult nsCParserNode::Init(CToken* aToken,PRInt32 aLineNumber,nsTokenAllocator* aTokenAllocator,nsNodeAllocator* aNodeAllocator) {
  if(mAttributes && (mAttributes->GetSize())) {
    NS_ASSERTION(0!=mTokenAllocator, "Error: Attribute tokens on node without token allocator");
    if(mTokenAllocator) {
      CToken* theAttrToken=0;
      while((theAttrToken=NS_STATIC_CAST(CToken*,mAttributes->Pop()))) {
        IF_FREE(theAttrToken, mTokenAllocator);
      }
    }
  }
  mLineNumber=aLineNumber;
  mTokenAllocator=aTokenAllocator;
  mToken=aToken;
  IF_HOLD(mToken);
  mGenericState=PR_FALSE;
  mUseCount=0;
  if(mSkippedContent) {
    mSkippedContent->Truncate();
  }
#ifdef HEAP_ALLOCATED_NODES
  mNodeAllocator=aNodeAllocator;
#endif
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
  NS_PRECONDITION(0!=mTokenAllocator, "Error: Can't add attribute without token allocator");

  if(mTokenAllocator) {
    if(!mAttributes)
      mAttributes=new nsDeque(0);
    if(mAttributes) {
      mAttributes->Push(aToken);
    }
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
const nsAReadableString& nsCParserNode::GetText() const {
  return (mToken) ? mToken->GetStringValue() : NS_STATIC_CAST(const nsAReadableString&,GetEmptyString());
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
  return (mToken) ? mToken->GetTypeID() : 0;
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
  return (mToken) ? mToken->GetTokenType() : 0;
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
const nsAReadableString& nsCParserNode::GetKeyAt(PRUint32 anIndex) const {
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
    return tkn->GetValue();
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
    NS_ASSERTION(0!=mTokenAllocator, "Error: no token allocator");
    if(mTokenAllocator) {
      CToken* theAttrToken=0;
      while((theAttrToken=NS_STATIC_CAST(CToken*,mAttributes->Pop()))) {
        IF_FREE(theAttrToken, mTokenAllocator);
      }
    }
    delete mAttributes;
    mAttributes=0;
  }
  if(mSkippedContent) {
    delete mSkippedContent;
    mSkippedContent=0;
  }

  if(mTokenAllocator) {
    // It was heap allocated, so free it!
    IF_FREE(mToken,mTokenAllocator);
  }

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


