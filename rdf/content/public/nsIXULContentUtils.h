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

  A package of routines shared by the XUL content code.

 */

#ifndef nsIXULContentUtils_h__
#define nsIXULContentUtils_h__

#include "nsISupports.h"

class nsIAtom;
class nsIContent;
class nsIDocument;
class nsIDOMNodeList;
class nsIRDFNode;
class nsCString;
class nsString;
class nsIRDFResource;

// {3E727CE0-6499-11d3-BE38-00104BDE6048}
#define NS_IXULCONTENTUTILS_IID \
{ 0x3e727ce0, 0x6499, 0x11d3, { 0xbe, 0x38, 0x0, 0x10, 0x4b, 0xde, 0x60, 0x48 } }


class nsIXULContentUtils : public nsISupports
{
public:
    static const nsIID& GetIID() { static nsIID iid = NS_IXULCONTENTUTILS_IID; return iid; }

    NS_IMETHOD
    AttachTextNode(nsIContent* parent, nsIRDFNode* value) = 0;
    
    NS_IMETHOD
    FindChildByTag(nsIContent *aElement,
                   PRInt32 aNameSpaceID,
                   nsIAtom* aTag,
                   nsIContent **aResult) = 0;

    NS_IMETHOD
    FindChildByResource(nsIContent* aElement,
                        nsIRDFResource* aResource,
                        nsIContent** aResult) = 0;

    NS_IMETHOD
    GetElementResource(nsIContent* aElement, nsIRDFResource** aResult) = 0;

    NS_IMETHOD
    GetElementRefResource(nsIContent* aElement, nsIRDFResource** aResult) = 0;

    NS_IMETHOD
    GetTextForNode(nsIRDFNode* aNode, nsAWritableString& aResult) = 0;

    NS_IMETHOD
    GetElementLogString(nsIContent* aElement, nsAWritableString& aResult) = 0;

    NS_IMETHOD
    GetAttributeLogString(nsIContent* aElement, PRInt32 aNameSpaceID, nsIAtom* aTag, nsAWritableString& aResult) = 0;

    NS_IMETHOD
    MakeElementURI(nsIDocument* aDocument, const nsAReadableString& aElementID, nsCString& aURI) = 0;

    NS_IMETHOD
    MakeElementResource(nsIDocument* aDocument, const nsAReadableString& aElementID, nsIRDFResource** aResult) = 0;

    NS_IMETHOD
    MakeElementID(nsIDocument* aDocument, const nsAReadableString& aURI, nsAWritableString& aElementID) = 0;

    NS_IMETHOD_(PRBool)
    IsContainedBy(nsIContent* aElement, nsIContent* aContainer) = 0;

    NS_IMETHOD
    GetResource(PRInt32 aNameSpaceID, nsIAtom* aAttribute, nsIRDFResource** aResult) = 0;

    NS_IMETHOD
    GetResource(PRInt32 aNameSpaceID, const nsAReadableString& aAttribute, nsIRDFResource** aResult) = 0;

    NS_IMETHOD
    SetCommandUpdater(nsIDocument* aDocument, nsIContent* aElement) = 0;

    NS_IMETHOD
    GetEventHandlerIID(nsIAtom* aName, nsIID* aIID, PRBool* aFound) = 0;

    /**
     * Returns <code>PR_TRUE</code> if the XUL cache should be used
     */
    NS_IMETHOD_(PRBool)
    UseXULCache() = 0;
};


// {3E727CE1-6499-11d3-BE38-00104BDE6048}
#define NS_XULCONTENTUTILS_CID \
{ 0x3e727ce1, 0x6499, 0x11d3, { 0xbe, 0x38, 0x0, 0x10, 0x4b, 0xde, 0x60, 0x48 } }

NS_IMETHODIMP
NS_NewXULContentUtils(nsISupports* aOuter, const nsIID& aIID, void** aResult);

#endif // nsIRDFContentUtils_h__

