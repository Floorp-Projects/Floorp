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


#include "nsCOMPtr.h"
#include "nsCRT.h"
#include "nsIAtom.h"
#include "nsIContent.h"
#include "nsIDOMElement.h"
#include "nsIDOMElementObserver.h"
#include "nsIDOMNode.h"
#include "nsIDOMNodeObserver.h"
#include "nsIDocument.h"
#include "nsINameSpaceManager.h"
#include "nsIRDFContentModelBuilder.h"
#include "nsIRDFCursor.h"
#include "nsIRDFCompositeDataSource.h"
#include "nsIRDFDocument.h"
#include "nsIRDFNode.h"
#include "nsIRDFObserver.h"
#include "nsIRDFService.h"
#include "nsIServiceManager.h"
#include "nsINameSpaceManager.h"
#include "nsIServiceManager.h"
#include "nsISupportsArray.h"
#include "nsIURL.h"
#include "nsLayoutCID.h"
#include "nsRDFCID.h"
#include "nsRDFContentUtils.h"
#include "nsString.h"
#include "nsXPIDLString.h"
#include "rdf.h"
#include "rdfutil.h"
#include "nsITimer.h"
#include "nsVoidArray.h"
#include "rdf_qsort.h"

#include "nsRDFGenericBuilder.h"

////////////////////////////////////////////////////////////////////////

DEFINE_RDF_VOCAB(NC_NAMESPACE_URI, NC, child);
DEFINE_RDF_VOCAB(NC_NAMESPACE_URI, NC, Columns);
DEFINE_RDF_VOCAB(NC_NAMESPACE_URI, NC, Column);
DEFINE_RDF_VOCAB(NC_NAMESPACE_URI, NC, Folder);
DEFINE_RDF_VOCAB(NC_NAMESPACE_URI, NC, Title);

DEFINE_RDF_VOCAB(RDF_NAMESPACE_URI, RDF, child);

////////////////////////////////////////////////////////////////////////

static NS_DEFINE_IID(kIContentIID,                NS_ICONTENT_IID);
static NS_DEFINE_IID(kIDocumentIID,               NS_IDOCUMENT_IID);
static NS_DEFINE_IID(kINameSpaceManagerIID,       NS_INAMESPACEMANAGER_IID);
static NS_DEFINE_IID(kIRDFResourceIID,            NS_IRDFRESOURCE_IID);
static NS_DEFINE_IID(kIRDFLiteralIID,             NS_IRDFLITERAL_IID);
static NS_DEFINE_IID(kIRDFContentModelBuilderIID, NS_IRDFCONTENTMODELBUILDER_IID);
static NS_DEFINE_IID(kIRDFObserverIID,            NS_IRDFOBSERVER_IID);
static NS_DEFINE_IID(kIRDFServiceIID,             NS_IRDFSERVICE_IID);
static NS_DEFINE_IID(kISupportsIID,               NS_ISUPPORTS_IID);

static NS_DEFINE_CID(kNameSpaceManagerCID,        NS_NAMESPACEMANAGER_CID);
static NS_DEFINE_CID(kRDFServiceCID,              NS_RDFSERVICE_CID);

////////////////////////////////////////////////////////////////////////

class RDFMenuBuilderImpl : public RDFGenericBuilderImpl
{
public:
    RDFMenuBuilderImpl();
    virtual ~RDFMenuBuilderImpl();

    // Implementation methods
    nsresult
    AddWidgetItem(nsIContent* aMenuItemElement,
                  nsIRDFResource* aProperty,
                  nsIRDFResource* aValue, PRInt32 aNaturalOrderPos);

    nsresult
    RemoveWidgetItem(nsIContent* aMenuItemElement,
                     nsIRDFResource* aProperty,
                     nsIRDFResource* aValue);

    nsresult
    SetWidgetAttribute(nsIContent* aMenuItemElement,
                       nsIRDFResource* aProperty,
                       nsIRDFNode* aValue);

    nsresult
    UnsetWidgetAttribute(nsIContent* aMenuItemElement,
                         nsIRDFResource* aProperty,
                         nsIRDFNode* aValue);

    nsresult 
    GetRootWidgetAtom(nsIAtom** aResult) {
        NS_ADDREF(kMenuAtom);
        *aResult = kMenuAtom;
        return NS_OK;
    }

    nsresult
    GetWidgetItemAtom(nsIAtom** aResult) {
        NS_ADDREF(kMenuItemAtom);
        *aResult = kMenuItemAtom;
        return NS_OK;
    }

    nsresult
    GetWidgetFolderAtom(nsIAtom** aResult) {
        NS_ADDREF(kMenuAtom);
        *aResult = kMenuAtom;
        return NS_OK;
    }

    nsresult
    GetInsertionRootAtom(nsIAtom** aResult) {
        NS_ADDREF(kMenuBarAtom);
        *aResult = kMenuBarAtom;
        return NS_OK;
    }

    nsresult
    GetItemAtomThatContainsTheChildren(nsIAtom** aResult) {
        NS_ADDREF(kMenuAtom);
        *aResult = kMenuAtom;
        return NS_OK;
    }

    void
    Notify(nsITimer *timer);

    // pseudo-constants
    static nsrefcnt gRefCnt;
 
    static nsIAtom* kMenuBarAtom;
    static nsIAtom* kMenuAtom;
    static nsIAtom* kMenuItemAtom;
    static nsIAtom* kNameAtom;
};

////////////////////////////////////////////////////////////////////////

nsrefcnt RDFMenuBuilderImpl::gRefCnt = 0;

nsIAtom* RDFMenuBuilderImpl::kMenuAtom;
nsIAtom* RDFMenuBuilderImpl::kMenuItemAtom;
nsIAtom* RDFMenuBuilderImpl::kMenuBarAtom;
nsIAtom* RDFMenuBuilderImpl::kNameAtom;

////////////////////////////////////////////////////////////////////////

nsresult
NS_NewRDFMenuBuilder(nsIRDFContentModelBuilder** result)
{
    NS_PRECONDITION(result != nsnull, "null ptr");
    if (! result)
        return NS_ERROR_NULL_POINTER;

    RDFMenuBuilderImpl* builder = new RDFMenuBuilderImpl();
    if (! builder)
        return NS_ERROR_OUT_OF_MEMORY;

    NS_ADDREF(builder);
    *result = builder;
    return NS_OK;
}



RDFMenuBuilderImpl::RDFMenuBuilderImpl(void)
    : RDFGenericBuilderImpl()
{
    if (gRefCnt == 0) {
        kMenuAtom            = NS_NewAtom("menu");
        kMenuItemAtom        = NS_NewAtom("menuitem");
        kMenuBarAtom         = NS_NewAtom("menubar");
        kNameAtom            = NS_NewAtom("name");
    }

    ++gRefCnt;
}

RDFMenuBuilderImpl::~RDFMenuBuilderImpl(void)
{
    --gRefCnt;
    if (gRefCnt == 0) {
        NS_RELEASE(kMenuAtom);
        NS_RELEASE(kMenuItemAtom);
        NS_RELEASE(kMenuBarAtom);
        NS_RELEASE(kNameAtom);
    }
}

void
RDFMenuBuilderImpl::Notify(nsITimer *timer)
{
}

////////////////////////////////////////////////////////////////////////
// Implementation methods

nsresult
RDFMenuBuilderImpl::AddWidgetItem(nsIContent* aElement,
                                  nsIRDFResource* aProperty,
                                  nsIRDFResource* aValue,
                                  PRInt32 naturalOrderPos)
{
    nsresult rv;

    nsCOMPtr<nsIContent> menuParent;
    menuParent = dont_QueryInterface(aElement);
    if (!IsItemOrFolder(aElement) && !IsWidgetInsertionRootElement(aElement))
    {
        NS_ERROR("Can't add something here!");
        return NS_ERROR_UNEXPECTED;
    }

    // Find out if we're a container or not.
    PRBool markAsContainer = IsContainer(aElement, aValue);
    nsCOMPtr<nsIAtom> itemAtom;
    
    // Figure out what atom to use based on whether or not we're a container
    // or leaf
    if (markAsContainer)
    {
        // We're a menu element
        GetWidgetFolderAtom(getter_AddRefs(itemAtom));
    }
    else
    {
        // We're a menuitem element
        GetWidgetItemAtom(getter_AddRefs(itemAtom));
    }

    // Create the <xul:menuitem> element
    nsCOMPtr<nsIContent> menuItem;
    if (NS_FAILED(rv = CreateResourceElement(kNameSpaceID_XUL,
                                             itemAtom,
                                             aValue,
                                             getter_AddRefs(menuItem))))
        return rv;

    // Add the new menu item to the <xul:menu> element.
    menuParent->AppendChildTo(menuItem, PR_TRUE);

    // Add miscellaneous attributes by iterating _all_ of the
    // properties out of the resource.
    nsCOMPtr<nsIRDFArcsOutCursor> arcs;
    if (NS_FAILED(rv = mDB->ArcLabelsOut(aValue, getter_AddRefs(arcs)))) {
        NS_ERROR("unable to get arcs out");
        return rv;
    }

    while (1) {
        rv = arcs->Advance();
        if (NS_FAILED(rv))
            return rv;

        if (rv == NS_RDF_CURSOR_EMPTY)
            break;

        nsCOMPtr<nsIRDFResource> property;
        if (NS_FAILED(rv = arcs->GetLabel(getter_AddRefs(property)))) {
            NS_ERROR("unable to get cursor value");
            return rv;
        }

        // Ignore properties that are used to indicate containment
        if (IsContainmentProperty(aElement, property))
            continue;

        PRInt32 nameSpaceID;
        nsCOMPtr<nsIAtom> tag;
        if (NS_FAILED(rv = mDocument->SplitProperty(property, &nameSpaceID, getter_AddRefs(tag)))) {
            NS_ERROR("unable to split property");
            return rv;
        }

        nsCOMPtr<nsIRDFNode> value;
        if (NS_FAILED(rv = mDB->GetTarget(aValue, property, PR_TRUE, getter_AddRefs(value)))) {
            NS_ERROR("unable to get target");
            return rv;
        }

        NS_ASSERTION(rv != NS_RDF_NO_VALUE, "arc-out with no target: fix your arcs-out cursor!");
        if (rv == NS_RDF_NO_VALUE)
            continue;

        nsAutoString s;
        rv = nsRDFContentUtils::GetTextForNode(value, s);
        if (NS_FAILED(rv)) return rv;

        rv = menuItem->SetAttribute(nameSpaceID, tag, s, PR_FALSE);
        if (NS_FAILED(rv)) return rv;

        // XXX This should go away and just be determined dynamically;
        // e.g., by setting an attribute on the "xul:menu" tag
        nsAutoString tagStr;
        tag->ToString(tagStr);
        if (tagStr.EqualsIgnoreCase("name")) {
            // Hack to ensure that we add in a lowercase name attribute also.
            menuItem->SetAttribute(kNameSpaceID_None, kNameAtom, s, PR_FALSE);
        }
    }

    // XXX: This is a hack until the menu folks get their act together.
    menuItem->SetAttribute(kNameSpaceID_None, kOpenAtom, "true", PR_FALSE);

    // Finally, mark this as a "container" so that we know to
    // recursively generate kids if they're asked for.
    if (markAsContainer == PR_TRUE)
    {
        // Finally, mark this as a "container" so that we know to
        // recursively generate kids if they're asked for.
        if (NS_FAILED(rv = menuItem->SetAttribute(kNameSpaceID_RDF, kContainerAtom, "true", PR_FALSE)))
            return rv;
    }

    return NS_OK;
}


nsresult
RDFMenuBuilderImpl::RemoveWidgetItem(nsIContent* aMenuItemElement,
                                     nsIRDFResource* aProperty,
                                     nsIRDFResource* aValue)
{
    nsresult rv;

    // Find the doomed kid and blow it away.
    PRInt32 count;
    if (NS_FAILED(rv = aMenuItemElement->ChildCount(count)))
        return rv;

    for (PRInt32 i = 0; i < count; ++i) {
        nsCOMPtr<nsIContent> kid;
        rv = aMenuItemElement->ChildAt(i, *getter_AddRefs(kid));
        if (NS_FAILED(rv)) return rv;

        // Make sure it's a <xul:menu> or <xul:menuitem>
        PRInt32 nameSpaceID;
        rv = kid->GetNameSpaceID(nameSpaceID);
        if (NS_FAILED(rv)) return rv;

        if (nameSpaceID != kNameSpaceID_XUL)
            continue; // wrong namespace

        nsCOMPtr<nsIAtom> tag;
        rv = kid->GetTag(*getter_AddRefs(tag));
        if (NS_FAILED(rv)) return rv;

        if ((tag.get() != kMenuItemAtom) &&
            (tag.get() != kMenuAtom))
            continue; // wrong tag

        // Now get the resource ID from the RDF:ID attribute. We do it
        // via the content model, because you're never sure who
        // might've added this stuff in...
        nsCOMPtr<nsIRDFResource> resource;
        rv = GetElementResource(kid, getter_AddRefs(resource));
        NS_ASSERTION(NS_SUCCEEDED(rv), "severe error retrieving resource");
        if(NS_FAILED(rv)) return rv;

        if (resource.get() != aValue)
            continue; // not the resource we want

        // Fount it! Now kill it.
        rv = aMenuItemElement->RemoveChildAt(i, PR_TRUE);
        NS_ASSERTION(NS_SUCCEEDED(rv), "unable to remove xul:menu[item] from xul:menu");
        if (NS_FAILED(rv)) return rv;

        return NS_OK;
    }

    // XXX make this a warning
    NS_WARNING("unable to find child to remove");
    return NS_OK;
}

nsresult
RDFMenuBuilderImpl::SetWidgetAttribute(nsIContent* aMenuItemElement,
                                       nsIRDFResource* aProperty,
                                       nsIRDFNode* aValue)
{
    nsresult rv;

    PRInt32 nameSpaceID;
    nsCOMPtr<nsIAtom> tag;
    rv = mDocument->SplitProperty(aProperty, &nameSpaceID, getter_AddRefs(tag));
    if (NS_FAILED(rv)) return rv;

    nsAutoString value;
    rv = nsRDFContentUtils::GetTextForNode(aValue, value);
    if (NS_FAILED(rv)) return rv;

    rv = aMenuItemElement->SetAttribute(nameSpaceID, tag, value, PR_TRUE);
    if (NS_FAILED(rv)) return rv;

    // XXX This should go away and just be determined dynamically;
    // e.g., by setting an attribute on the "xul:menu" tag.
    nsAutoString tagStr;
    tag->ToString(tagStr);
    if (tagStr.EqualsIgnoreCase("name")) {
        // Hack to ensure that we add in a lowercase name attribute also.
        aMenuItemElement->SetAttribute(kNameSpaceID_None, kNameAtom, value, PR_TRUE);
    }

    return NS_OK;
}

nsresult
RDFMenuBuilderImpl::UnsetWidgetAttribute(nsIContent* aMenuItemElement,
                                         nsIRDFResource* aProperty,
                                         nsIRDFNode* aValue)
{
    nsresult rv;

    PRInt32 nameSpaceID;
    nsCOMPtr<nsIAtom> tag;
    rv = mDocument->SplitProperty(aProperty, &nameSpaceID, getter_AddRefs(tag));
    if (NS_FAILED(rv)) return rv;

    rv = aMenuItemElement->UnsetAttribute(nameSpaceID, tag, PR_TRUE);
    if (NS_FAILED(rv)) return rv;

    // XXX This should go away and just be determined dynamically;
    // e.g., by setting an attribute on the "xul:menu" tag.
    nsAutoString tagStr;
    tag->ToString(tagStr);
    if (tagStr.EqualsIgnoreCase("name")) {
        // Hack to ensure that we add in a lowercase name attribute also.
        aMenuItemElement->UnsetAttribute(kNameSpaceID_None, kNameAtom, PR_TRUE);
    }

    return NS_OK;
}

