/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef NS_PARSERNODE__
#define NS_PARSERNODE__

#include "nscore.h"
#include "nsIParserNode.h"
#include "nsHTMLTags.h"

class nsParserNode : public nsIParserNode
{
  public:
    /**
     * Constructor
     */
    nsParserNode(eHTMLTags aTag);

    /**
     * Destructor
     */
    virtual ~nsParserNode();

    /**
     * Retrieve the type of the parser node.
     * @return  node type.
     */
    virtual int32_t GetNodeType()  const;

    /**
     * Retrieve token type of parser node
     * @return  token type
     */
    virtual int32_t GetTokenType()  const;


    //***************************************
    //methods for accessing key/value pairs
    //***************************************

    /**
     * Retrieve the number of attributes in this node.
     * @return  0
     */
    virtual int32_t GetAttributeCount() const;

    /**
     * Retrieve the key (of key/value pair) at given index
     * @param   anIndex is the index of the key you want
     * @return  string containing key.
     */
    virtual const nsAString& GetKeyAt(uint32_t anIndex) const;

    /**
     * Retrieve the value (of key/value pair) at given index
     * @update	gess5/11/98
     * @param   anIndex is the index of the value you want
     * @return  string containing value.
     */
    virtual const nsAString& GetValueAt(uint32_t anIndex) const;

  private:
    eHTMLTags mTag;
};

#endif
