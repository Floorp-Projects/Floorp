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

#ifndef nsXULTreeElement_h__
#define nsXULTreeElement_h__

#include "nsXULElement.h"
#include "nsIDOMXULTreeElement.h"
#include "nsIXULTreeContent.h"
#include "nsRDFDOMNodeList.h"
#include "nsITimer.h"

class nsXULTreeElement : public nsXULAggregateElement,
                         public nsIDOMXULTreeElement,
                         public nsIXULTreeContent
{
public:
    nsXULTreeElement(nsIDOMXULElement* aOuter);
    ~nsXULTreeElement();

    NS_DECL_ISUPPORTS_INHERITED

    // nsIDOMNode interface
    NS_FORWARD_IDOMNODE(mOuter->);

    // nsIDOMElement interface
    NS_FORWARD_IDOMELEMENT(mOuter->);

    // nsIDOMXULElement interface
    NS_FORWARD_IDOMXULELEMENT(mOuter->);

    // nsIDOMXULTreeElement interface
    NS_DECL_IDOMXULTREEELEMENT
   
    // nsIXULTreeContent interface
    NS_IMETHOD FireOnSelectHandler();

    static nsIAtom*             kSelectedAtom;
    static nsIAtom*             kOpenAtom;
    static nsIAtom*             kTreeRowAtom;
    static nsIAtom*             kTreeItemAtom;
    static nsIAtom*             kTreeChildrenAtom;
    static nsIAtom*             kCurrentAtom;

    static int gRefCnt;

protected:
    // Helpers
    static nsresult IndexOfContent(nsIContent *aRoot, nsIContent *aContent,
                                   PRBool aDescendIntoRows,
                                   PRBool aParentIsOpen,
                                   PRInt32* aResult);

    static void SelectCallback(nsITimer *aTimer, void *aClosure);

protected:
    nsRDFDOMNodeList* mSelectedItems;
    nsIDOMXULElement* mCurrentItem;
    nsIDOMXULElement* mSelectionStart;
    PRBool mSuppressOnSelect;

    nsCOMPtr<nsITimer> mSelectTimer;
};


#endif // nsXULTreeElement_h__
