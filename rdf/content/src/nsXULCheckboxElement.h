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
 * Contributor(s): 
 */

/*

  An aggregate object that implements the XUL titledbutton widget APIs

*/

#ifndef nsXULCheckboxElement_h__
#define nsXULCheckboxElement_h__

#include "nsXULElement.h"
#include "nsIDOMXULCheckboxElement.h"

class nsXULCheckboxElement : public nsXULAggregateElement,
                          public nsIDOMXULCheckboxElement
{
public:
    nsXULCheckboxElement(nsIDOMXULElement* aOuter);
    ~nsXULCheckboxElement();

    NS_DECL_ISUPPORTS_INHERITED

    // nsIDOMNode interface
    NS_FORWARD_IDOMNODE(mOuter->);

    // nsIDOMElement interface
    NS_FORWARD_IDOMELEMENT(mOuter->);

    // nsIDOMXULElement interface
    NS_FORWARD_IDOMXULELEMENT(mOuter->);

    // nsIDOMXULCheckboxElement interface
    NS_DECL_IDOMXULCHECKBOXELEMENT
};


#endif // nsXULCheckboxElement_h__
