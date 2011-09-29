/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
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
 * Alternatively, the contents of this file may be used under the terms of
 * either of the GNU General Public License Version 2 or later (the "GPL"),
 * or the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */


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
  : mRefCnt(0), mGenericState(PR_FALSE), mUseCount(0), mToken(nsnull),
    mTokenAllocator(nsnull)
{
  MOZ_COUNT_CTOR(nsCParserNode);
#ifdef HEAP_ALLOCATED_NODES
  mNodeAllocator = nsnull;
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
  : mRefCnt(0), mGenericState(PR_FALSE), mUseCount(0), mToken(aToken),
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
  mNodeAllocator = nsnull;
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
  mGenericState = PR_FALSE;
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
PRInt32 
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
PRInt32 
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
PRInt32 
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
nsCParserNode::GetKeyAt(PRUint32 anIndex) const 
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
nsCParserNode::GetValueAt(PRUint32 anIndex) const 
{
  return EmptyString();
}

PRInt32 
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
PRInt32
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

PRInt32 
nsCParserStartNode::GetAttributeCount(bool askToken) const
{
  PRInt32 result = 0;
  if (askToken) {
    result = mToken ? mToken->GetAttributeCount() : 0;
  }
  else {
    result = mAttributes.GetSize();
  }
  return result;
}

const nsAString&
nsCParserStartNode::GetKeyAt(PRUint32 anIndex) const 
{
  if ((PRInt32)anIndex < mAttributes.GetSize()) {
    CAttributeToken* attr = 
      static_cast<CAttributeToken*>(mAttributes.ObjectAt(anIndex));
    if (attr) {
      return attr->GetKey();
    }
  }
  return EmptyString();
}

const nsAString&
nsCParserStartNode::GetValueAt(PRUint32 anIndex) const 
{
  if (PRInt32(anIndex) < mAttributes.GetSize()) {
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
  PRInt32 index;
  PRInt32 size = mAttributes.GetSize();
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

