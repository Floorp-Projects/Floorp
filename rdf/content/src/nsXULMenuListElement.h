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

  An aggregate object that implements the XUL MenuList widget APIs

*/

#ifndef nsXULMenuListElement_h__
#define nsXULMenuListElement_h__

#include "nsCOMPtr.h"
#include "nsXULElement.h"
#include "nsIDOMXULMenuListElement.h"

class nsXULMenuListElement : public nsXULAggregateElement,
                          public nsIDOMXULMenuListElement
{
public:
    nsXULMenuListElement(nsIDOMXULElement* aOuter);
    ~nsXULMenuListElement();

    NS_DECL_ISUPPORTS_INHERITED

    // nsIDOMNode interface
    NS_FORWARD_IDOMNODE(mOuter->);

    // nsIDOMElement interface
    NS_FORWARD_IDOMELEMENT(mOuter->);

    // nsIDOMXULElement interface
    NS_FORWARD_IDOMXULELEMENT(mOuter->);

    // nsIDOMXULMenuListElement interface
    NS_DECL_IDOMXULMENULISTELEMENT
   
protected:
    nsCOMPtr<nsIDOMElement> mSelectedItem;
};


#endif // nsXULMenuListElement_h__
