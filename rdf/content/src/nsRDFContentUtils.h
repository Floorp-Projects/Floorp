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

  Utility routines used throughout the content model library.

 */

#ifndef nsRDFContentUtils_h__
#define nsRDFContentUtils_h__

#include "nsError.h"

class nsIContent;
class nsIDOMNodeList;
class nsIRDFNode;
class nsString;

nsresult
rdf_GetQuotedAttributeValue(const nsString& aSource, 
                            const nsString& aAttribute,
                            nsString& aValue);

nsresult
rdf_AttachTextNode(nsIContent* parent, nsIRDFNode* value);

// In nsRDFDOMNodeList.cpp
extern nsresult
NS_NewRDFDOMNodeList(nsIDOMNodeList** aChildNodes, nsIContent* aElement);

#endif // nsRDFContentUtils_h__

