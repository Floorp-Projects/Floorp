/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.0 (the "License"); you may not use this file except in
 * compliance with the License.  You may obtain a copy of the License at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS IS"
 * basis, WITHOUT WARRANTY OF ANY KIND, either express or implied.  See
 * the License for the specific language governing rights and limitations
 * under the License.
 *
 * The Original Code is Mozilla Communicator client code.
 *
 * The Initial Developer of the Original Code is Netscape Communications
 * Corporation.  Portions created by Netscape are Copyright (C) 1998
 * Netscape Communications Corporation.  All Rights Reserved.
 */

/*

  A package of routines shared by the RDF content code.

 */

#ifndef nsRDFContentUtils_h__
#define nsRDFContentUtils_h__

#include "nsError.h"

class nsIAtom;
class nsIContent;
class nsIDOMNodeList;
class nsIRDFNode;
class nsString;


class nsRDFContentUtils
{
public:

    static nsresult
    AttachTextNode(nsIContent* parent, nsIRDFNode* value);
    
    static nsresult
    FindTreeBodyElement(nsIContent *tree, nsIContent **treeBody);

    static nsresult
    GetTextForNode(nsIRDFNode* aNode, nsString& aResult);
};


// in nsRDFElement.cpp
extern nsresult
NS_NewRDFElement(PRInt32 aNameSpaceID, nsIAtom* aTag, nsIContent** aResult);

#endif // nsRDFContentUtils_h__

