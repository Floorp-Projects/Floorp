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

#include "nsISupports.h"
#include "prtypes.h"
#include "nsStringGlue.h"
#include "nsDebug.h"

//#define HEAP_ALLOCATED_NODES 
//#define DEBUG_TRACK_NODES

class nsIAtom;
class CToken;

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
class nsIParserNode { // XXX Should be nsAParserNode
            
  public:


    /**
     * Retrieve the name of the node
     * @update	gess5/11/98
     * @return  string containing node name
     */
    virtual const nsAString& GetTagName() const = 0;  //to get name of tag

    /**
     * Retrieve the text from the given node
     * @update	gess5/11/98
     * @return  string containing node text
     */
    virtual const nsAString& GetText() const = 0;  //get plain text if available

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
    virtual PRInt32 GetAttributeCount(bool askToken=false) const =0;

    /**
     * Retrieve the key (of key/value pair) at given index
     * @update	gess5/11/98
     * @param   anIndex is the index of the key you want
     * @return  string containing key.
     */
    virtual const nsAString& GetKeyAt(PRUint32 anIndex) const = 0;

    /**
     * Retrieve the value (of key/value pair) at given index
     * @update	gess5/11/98
     * @param   anIndex is the index of the value you want
     * @return  string containing value.
     */
    virtual const nsAString& GetValueAt(PRUint32 anIndex) const = 0;

    /**
     * NOTE: When the node is an entity, this will translate the entity
     *       to it's unicode value, and store it in aString.
     * @update	gess5/11/98
     * @param   aString will contain the resulting unicode string value
     * @return  int (unicode char or unicode index from table)
     */
    virtual PRInt32 TranslateToUnicodeStr(nsString& aString) const = 0;


    virtual void AddAttribute(CToken* aToken)=0;

    /**
     * This getter retrieves the line number from the input source where
     * the token occurred. Lines are interpreted as occurring between \n characters.
     * @update	gess7/24/98
     * @return  int containing the line number the token was found on
     */
    virtual PRInt32 GetSourceLineNumber(void) const =0;

    /**
     * This pair of methods allows us to set a generic bit (for arbitrary use)
     * on each node stored in the context.
     * @update	gess 11May2000
     */
    virtual bool    GetGenericState(void) const =0;
    virtual void    SetGenericState(bool aState) =0;

    /** Retrieve a string containing the tag and its attributes in "source" form
     * @update	rickg 06June2000
     * @return  void
     */
    virtual void GetSource(nsString& aString) const = 0;

    /** Release all the objects you're holding
     * @update	harishd 08/02/00
     * @return  void
     */
    virtual nsresult ReleaseAll()=0;
};

#endif
