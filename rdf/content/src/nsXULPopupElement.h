/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
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
 * The Original Code is Mozilla Communicator client code.
 *
 * The Initial Developer of the Original Code is Netscape Communications
 * Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Original Author: David W. Hyatt (hyatt@netscape.com)
 *
 * Contributor(s): 
 */

/*

  An aggregate object that implements the XUL tree widget APIs

*/

#ifndef nsXULPopupElement_h__
#define nsXULPopupElement_h__

#include "nsXULElement.h"
#include "nsIDOMXULPopupElement.h"
#include "nsIXULTreeContent.h"
#include "nsRDFDOMNodeList.h"

class nsXULPopupElement : public nsXULAggregateElement,
                          public nsIDOMXULPopupElement
{
public:
    nsXULPopupElement(nsIDOMXULElement* aOuter);
    ~nsXULPopupElement();

    NS_DECL_ISUPPORTS_INHERITED

    // nsIDOMNode interface
    NS_FORWARD_IDOMNODE(mOuter->);

    // nsIDOMElement interface
    NS_FORWARD_IDOMELEMENT(mOuter->);

    // nsIDOMXULElement interface
    NS_FORWARD_IDOMXULELEMENT(mOuter->);

    // nsIDOMXULPopupElement interface
    NS_DECL_IDOMXULPOPUPELEMENT
   
protected:
};


#endif // nsXULPopupElement_h__
