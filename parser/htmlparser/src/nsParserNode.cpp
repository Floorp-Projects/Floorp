/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */


#include "nsParserNode.h"
#include "nsToken.h"

/**
 *  Constructor
 *  
 *  @update  gess 3/25/98
 *  @param   aToken -- token to init internal token
 *  @return  
 */
nsParserNode::nsParserNode(eHTMLTags aTag)
  : mTag(aTag)
{
}

/**
 *  destructor
 *  NOTE: We intentionally DON'T recycle mToken here.
 *        It may get cached for use elsewhere
 *  @update  gess 3/25/98
 *  @param   
 *  @return  
 */
nsParserNode::~nsParserNode() {
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
nsParserNode::GetNodeType(void) const
{
   return mTag;
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
nsParserNode::GetTokenType(void) const
{
  return eToken_start;
}


/**
 *  Retrieve the number of attributes on this node
 *  
 *  @update  gess 3/25/98
 *  @param   
 *  @return  int -- representing attribute count
 */
int32_t 
nsParserNode::GetAttributeCount() const
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
nsParserNode::GetKeyAt(uint32_t anIndex) const
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
nsParserNode::GetValueAt(uint32_t anIndex) const
{
  return EmptyString();
}
