/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */


#include "nsIAtom.h"
#include "nsParserNode.h" 
#include <string.h>
#include "nsHTMLTokens.h"
#include "nsITokenizer.h"
#include "nsDTDUtils.h"


/**
 *  Default Constructor
 */
nsCParserNode::nsCParserNode()
  : mRefCnt(0), mGenericState(false), mUseCount(0), mToken(nullptr),
    mTokenAllocator(nullptr)
{
  MOZ_COUNT_CTOR(nsCParserNode);
#ifdef HEAP_ALLOCATED_NODES
  mNodeAllocator = nullptr;
#endif
}

/**
 *  Constructor
 *  
 *  @update  gess 3/25/98
 *  @param   aToken -- token to init internal token
 *  @return  
 */
nsCParserNode::nsCParserNode(CToken* aToken,
                             nsTokenAllocator* aTokenAllocator,
                             nsNodeAllocator* aNodeAllocator)
  : mRefCnt(0), mGenericState(false), mUseCount(0), mToken(aToken),
    mTokenAllocator(aTokenAllocator)
{
  MOZ_COUNT_CTOR(nsCParserNode);

  static int theNodeCount = 0;
  ++theNodeCount;
  if (mTokenAllocator) {
    IF_HOLD(mToken);
  } // Else a stack-based token

#ifdef HEAP_ALLOCATED_NODES
  mNodeAllocator = aNodeAllocator;
#endif
}

/**
 *  destructor
 *  NOTE: We intentionally DON'T recycle mToken here.
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
  mNodeAllocator = nullptr;
#endif
  mTokenAllocator = 0;
}


/**
 *  Init
 *  
 *  @update  gess 3/25/98
 *  @param   
 *  @return  
 */

nsresult
nsCParserNode::Init(CToken* aToken,
                    nsTokenAllocator* aTokenAllocator,
                    nsNodeAllocator* aNodeAllocator) 
{
  mTokenAllocator = aTokenAllocator;
  mToken = aToken;
  if (mTokenAllocator) {
    IF_HOLD(mToken);
  } // Else a stack-based token
  mGenericState = false;
  mUseCount=0;
#ifdef HEAP_ALLOCATED_NODES
  mNodeAllocator = aNodeAllocator;
#endif
  return NS_OK;
}

void
nsCParserNode::AddAttribute(CToken* aToken) 
{
}


/**
 *  Gets the name of this node. Currently unused.
 *  
 *  @update  gess 3/25/98
 *  @param   
 *  @return  string ref containing node name
 */
const nsAString&
nsCParserNode::GetTagName() const {
  return EmptyString();
}


/**
 *  Get text value of this node, which translates into 
 *  getting the text value of the underlying token
 *  
 *  @update  gess 3/25/98
 *  @param   
 *  @return  string ref of text from internal token
 */
const nsAString& 
nsCParserNode::GetText() const 
{
  if (mToken) {
    return mToken->GetStringValue();
  }
  return EmptyString();
}

/**
 *  Get node type, meaning, get the tag type of the 
 *  underlying token
 *  
 *  @update  gess 3/25/98
 *  @param   
 *  @return  int value that represents tag type
 */
int32_t 
nsCParserNode::GetNodeType(void) const
{
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
int32_t 
nsCParserNode::GetTokenType(void) const
{
  return (mToken) ? mToken->GetTokenType() : 0;
}


/**
 *  Retrieve the number of attributes on this node
 *  
 *  @update  gess 3/25/98
 *  @param   
 *  @return  int -- representing attribute count
 */
int32_t 
nsCParserNode::GetAttributeCount(bool askToken) const
{
  return 0;
}

/**
 *  Retrieve the string rep of the attribute key at the
 *  given index.
 *  
 *  @update  gess 3/25/98
 *  @param   anIndex-- offset of attribute to retrieve
 *  @return  string rep of given attribute text key
 */
const nsAString&
nsCParserNode::GetKeyAt(uint32_t anIndex) const 
{
  return EmptyString();
}


/**
 *  Retrieve the string rep of the attribute at given offset
 *  
 *  @update  gess 3/25/98
 *  @param   anIndex-- offset of attribute to retrieve
 *  @return  string rep of given attribute text value
 */
const nsAString&
nsCParserNode::GetValueAt(uint32_t anIndex) const 
{
  return EmptyString();
}

int32_t 
nsCParserNode::TranslateToUnicodeStr(nsString& aString) const
{
  if (eToken_entity == mToken->GetTokenType()) {
    return ((CEntityToken*)mToken)->TranslateToUnicodeStr(aString);
  }
  return -1;
}

/**
 * This getter retrieves the line number from the input source where
 * the token occurred. Lines are interpreted as occurring between \n characters.
 * @update	gess7/24/98
 * @return  int containing the line number the token was found on
 */
int32_t
nsCParserNode::GetSourceLineNumber(void) const {
  return mToken ? mToken->GetLineNumber() : 0;
}

/**
 * This method pop the attribute token
 * @update	harishd 03/25/99
 * @return  token at anIndex
 */

CToken* 
nsCParserNode::PopAttributeToken() {
  return 0;
}

CToken* 
nsCParserNode::PopAttributeTokenFront() {
  return 0;
}

/** Retrieve a string containing the tag and its attributes in "source" form
 * @update	rickg 06June2000
 * @return  void
 */
void 
nsCParserNode::GetSource(nsString& aString) const
{
  eHTMLTags theTag = mToken ? (eHTMLTags)mToken->GetTypeID() : eHTMLTag_unknown;
  aString.Assign(PRUnichar('<'));
  const PRUnichar* theTagName = nsHTMLTags::GetStringValue(theTag);
  if(theTagName) {
    aString.Append(theTagName);
  }
  aString.Append(PRUnichar('>'));
}

/** Release all the objects you're holding to.
 * @update	harishd 08/02/00
 * @return  void
 */
nsresult 
nsCParserNode::ReleaseAll() 
{
  if(mTokenAllocator) {
    IF_FREE(mToken,mTokenAllocator);
  }
  return NS_OK;
}

nsresult 
nsCParserStartNode::Init(CToken* aToken,
                         nsTokenAllocator* aTokenAllocator,
                         nsNodeAllocator* aNodeAllocator) 
{
  NS_ASSERTION(mAttributes.GetSize() == 0, "attributes not recycled!");
  return nsCParserNode::Init(aToken, aTokenAllocator, aNodeAllocator);
}

void nsCParserStartNode::AddAttribute(CToken* aToken) 
{
  NS_ASSERTION(0 != aToken, "Error: Token shouldn't be null!");
  mAttributes.Push(aToken);
}

int32_t 
nsCParserStartNode::GetAttributeCount(bool askToken) const
{
  int32_t result = 0;
  if (askToken) {
    result = mToken ? mToken->GetAttributeCount() : 0;
  }
  else {
    result = mAttributes.GetSize();
  }
  return result;
}

const nsAString&
nsCParserStartNode::GetKeyAt(uint32_t anIndex) const 
{
  if ((int32_t)anIndex < mAttributes.GetSize()) {
    CAttributeToken* attr = 
      static_cast<CAttributeToken*>(mAttributes.ObjectAt(anIndex));
    if (attr) {
      return attr->GetKey();
    }
  }
  return EmptyString();
}

const nsAString&
nsCParserStartNode::GetValueAt(uint32_t anIndex) const 
{
  if (int32_t(anIndex) < mAttributes.GetSize()) {
    CAttributeToken* attr = 
      static_cast<CAttributeToken*>(mAttributes.ObjectAt(anIndex));
    if (attr) {
      return attr->GetValue();
    }
  }
  return EmptyString();
}

CToken* 
nsCParserStartNode::PopAttributeToken() 
{
  return static_cast<CToken*>(mAttributes.Pop());
}

CToken* 
nsCParserStartNode::PopAttributeTokenFront() 
{
  return static_cast<CToken*>(mAttributes.PopFront());
}

void nsCParserStartNode::GetSource(nsString& aString) const
{
  aString.Assign(PRUnichar('<'));
  const PRUnichar* theTagName = 
    nsHTMLTags::GetStringValue(nsHTMLTag(mToken->GetTypeID()));
  if (theTagName) {
    aString.Append(theTagName);
  }
  int32_t index;
  int32_t size = mAttributes.GetSize();
  for (index = 0 ; index < size; ++index) {
    CAttributeToken *theToken = 
      static_cast<CAttributeToken*>(mAttributes.ObjectAt(index));
    if (theToken) {
      theToken->AppendSourceTo(aString);
      aString.Append(PRUnichar(' ')); //this will get removed...
    }
  }
  aString.Append(PRUnichar('>'));
}

nsresult nsCParserStartNode::ReleaseAll() 
{
  NS_ASSERTION(0!=mTokenAllocator, "Error: no token allocator");
  CToken* theAttrToken;
  while ((theAttrToken = static_cast<CToken*>(mAttributes.Pop()))) {
    IF_FREE(theAttrToken, mTokenAllocator);
  }
  nsCParserNode::ReleaseAll();
  return NS_OK; 
}

