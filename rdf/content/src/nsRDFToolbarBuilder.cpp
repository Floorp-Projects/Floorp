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

/* XXX: Teach this how to make toolboxes as well as toolbars. */

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

class RDFToolbarBuilderImpl : public RDFGenericBuilderImpl
{
public:
    RDFToolbarBuilderImpl();
    virtual ~RDFToolbarBuilderImpl();

    // Implementation methods
    nsresult
    AddWidgetItem(nsIContent* aToolbarItemElement,
                  nsIRDFResource* aProperty,
                  nsIRDFResource* aValue, PRInt32 aNaturalOrderPos);

    nsresult
    RemoveWidgetItem(nsIContent* aTreeItemElement,
                     nsIRDFResource* aProperty,
                     nsIRDFResource* aValue);

    nsresult 
    GetRootWidgetAtom(nsIAtom** aResult) {
        NS_ADDREF(kToolbarAtom);
        *aResult = kToolbarAtom;
        return NS_OK;
    }

    nsresult
    GetWidgetItemAtom(nsIAtom** aResult) {
        NS_ADDREF(kTitledButtonAtom);
        *aResult = kTitledButtonAtom;
        return NS_OK;
    }

    nsresult
    GetWidgetFolderAtom(nsIAtom** aResult) {
        NS_ADDREF(kTitledButtonAtom);
        *aResult = kTitledButtonAtom;
        return NS_OK;
    }

    nsresult
    GetInsertionRootAtom(nsIAtom** aResult) {
        NS_ADDREF(kToolbarAtom);
        *aResult = kToolbarAtom;
        return NS_OK;
    }

    nsresult
    GetItemAtomThatContainsTheChildren(nsIAtom** aResult) {
        NS_ADDREF(kTitledButtonAtom);
        *aResult = kTitledButtonAtom;
        return NS_OK;
    }

    // pseudo-constants
    static nsrefcnt gRefCnt;
 
    static nsIAtom* kToolbarAtom;
    static nsIAtom* kTitledButtonAtom;
};

////////////////////////////////////////////////////////////////////////

nsrefcnt RDFToolbarBuilderImpl::gRefCnt = 0;

nsIAtom* RDFToolbarBuilderImpl::kToolbarAtom;
nsIAtom* RDFToolbarBuilderImpl::kTitledButtonAtom;

////////////////////////////////////////////////////////////////////////

nsresult
NS_NewRDFToolbarBuilder(nsIRDFContentModelBuilder** result)
{
    NS_PRECONDITION(result != nsnull, "null ptr");
    if (! result)
        return NS_ERROR_NULL_POINTER;

    RDFToolbarBuilderImpl* builder = new RDFToolbarBuilderImpl();
    if (! builder)
        return NS_ERROR_OUT_OF_MEMORY;

    NS_ADDREF(builder);
    *result = builder;
    return NS_OK;
}



RDFToolbarBuilderImpl::RDFToolbarBuilderImpl(void)
    : RDFGenericBuilderImpl()
{
    if (gRefCnt == 0) {
        kToolbarAtom             = NS_NewAtom("toolbar");
        kTitledButtonAtom        = NS_NewAtom("titledbutton");
    }

    ++gRefCnt;
}

RDFToolbarBuilderImpl::~RDFToolbarBuilderImpl(void)
{
    --gRefCnt;
    if (gRefCnt == 0) {
        
        NS_RELEASE(kToolbarAtom);
        NS_RELEASE(kTitledButtonAtom);
    }
}

////////////////////////////////////////////////////////////////////////
// Implementation methods

nsresult
RDFToolbarBuilderImpl::AddWidgetItem(nsIContent* aElement,
                                     nsIRDFResource* aProperty,
                                     nsIRDFResource* aValue,
                                     PRInt32 naturalOrderPos)
{
    nsresult rv;

    nsCOMPtr<nsIContent> toolbarParent;
    toolbarParent = dont_QueryInterface(aElement);
    if (!IsItemOrFolder(aElement) && !IsWidgetInsertionRootElement(aElement))
    {
        NS_ERROR("Can't add something here!");
        return NS_ERROR_UNEXPECTED;
    }

    PRBool markAsContainer = IsContainer(aElement, aValue);

    // Create the <xul:titledbutton> element
    nsCOMPtr<nsIContent> toolbarItem;
    if (NS_FAILED(rv = CreateResourceElement(kNameSpaceID_XUL,
                                             kTitledButtonAtom,
                                             aValue,
                                             getter_AddRefs(toolbarItem))))
        return rv;

    // Add the <xul:titledbutton> to the <xul:toolbar> element.
    toolbarParent->AppendChildTo(toolbarItem, PR_TRUE);

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

        // Ignore properties that are used to indicate "tree-ness"
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

        NS_ASSERTION(rv != NS_RDF_NO_VALUE, "arc-out with no target: fix your arcs out cursor");
        if (rv == NS_RDF_NO_VALUE)
            continue;

        nsCOMPtr<nsIRDFResource> resource;
        nsCOMPtr<nsIRDFLiteral> literal;

        nsAutoString s;
        if (NS_SUCCEEDED(rv = value->QueryInterface(kIRDFResourceIID, getter_AddRefs(resource)))) {
            nsXPIDLCString uri;
            resource->GetValue( getter_Copies(uri) );
            s = uri;
        }
        else if (NS_SUCCEEDED(rv = value->QueryInterface(kIRDFLiteralIID, getter_AddRefs(literal)))) {
            nsXPIDLString p;
            literal->GetValue( getter_Copies(p) );
            s = p;
        }
        else {
            NS_ERROR("not a resource or a literal");
            return NS_ERROR_UNEXPECTED;
        }

        toolbarItem->SetAttribute(nameSpaceID, tag, s, PR_FALSE);

        nsString nameAtom;
        tag->ToString(nameAtom);
        if (nameAtom == "Name")
        {
            nsIAtom* lowerName = NS_NewAtom("value");
            // Hack to ensure that we add in a lowercase value attribute also.
            toolbarItem->SetAttribute(kNameSpaceID_None, lowerName, s, PR_FALSE);
            NS_RELEASE(lowerName);
        }

        // XXX (Dave) I want these to go away. Style sheets should be used for these.
        nsIAtom* alignment = NS_NewAtom("align");
        nsIAtom* imagesrc = NS_NewAtom("src");
        toolbarItem->SetAttribute(kNameSpaceID_None, alignment, "right", PR_FALSE);
        toolbarItem->SetAttribute(kNameSpaceID_None, imagesrc, "resource:/res/toolbar/TB_Location.gif", PR_FALSE);
    }

    // Finally, mark this as a "container" so that we know to
    // recursively generate kids if they're asked for.
    if (markAsContainer == PR_TRUE)
    {
        // Finally, mark this as a "container" so that we know to
        // recursively generate kids if they're asked for.
        if (NS_FAILED(rv = toolbarItem->SetAttribute(kNameSpaceID_RDF, kContainerAtom, "true", PR_FALSE)))
            return rv;
    }

    return NS_OK;
}



nsresult
RDFToolbarBuilderImpl::RemoveWidgetItem(nsIContent* aToolbarItemElement,
                                        nsIRDFResource* aProperty,
                                        nsIRDFResource* aValue)
{
    NS_NOTYETIMPLEMENTED("write me");
    return NS_ERROR_NOT_IMPLEMENTED;
}
