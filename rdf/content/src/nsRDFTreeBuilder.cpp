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

  An nsIRDFDocument implementation that builds a tree widget XUL
  content model that is to be used with a tree control.

  TO DO

  1) Get a real namespace for XUL. This should go in a 
     header file somewhere.

  2) Get the columns stuff _out_ of here and into XUL. The columns
     code is what makes this thing as scary as it is right now.

  3) We have a serious problem if all the columns aren't created by
     the time that we start inserting rows into the table. Need to fix
     this so that columns can be dynamically added and removed (we
     need to do this anyway to handle column re-ordering or
     manipulation via DOM calls).

  4) There's lots of stuff screwed up with the ordering of walking the
     tree, creating children, getting assertions, etc. A lot of this
     has to do with the "are children already generated" state
     variable that lives in the RDFResourceElementImpl. I need to sit
     down and really figure out what the heck this needs to do.

 */

#include "nsCOMPtr.h"
#include "nsIAtom.h"
#include "nsIDocument.h"
#include "nsIRDFContent.h"
#include "nsIRDFContentModelBuilder.h"
#include "nsIRDFCursor.h"
#include "nsIRDFCompositeDataSource.h"
#include "nsIRDFDocument.h"
#include "nsIRDFNode.h"
#include "nsIRDFService.h"
#include "nsIServiceManager.h"
#include "nsINameSpaceManager.h"
#include "nsIServiceManager.h"
#include "nsISupportsArray.h"
#include "nsLayoutCID.h"
#include "nsRDFCID.h"
#include "nsRDFContentUtils.h"
#include "nsString.h"
#include "rdf.h"
#include "rdfutil.h"

////////////////////////////////////////////////////////////////////////

DEFINE_RDF_VOCAB(NC_NAMESPACE_URI, NC, Columns);
DEFINE_RDF_VOCAB(NC_NAMESPACE_URI, NC, Column);
DEFINE_RDF_VOCAB(NC_NAMESPACE_URI, NC, Title);

////////////////////////////////////////////////////////////////////////

static NS_DEFINE_IID(kIDocumentIID,               NS_IDOCUMENT_IID);
static NS_DEFINE_IID(kINameSpaceManagerIID,       NS_INAMESPACEMANAGER_IID);
static NS_DEFINE_IID(kIRDFResourceIID,            NS_IRDFRESOURCE_IID);
static NS_DEFINE_IID(kIRDFLiteralIID,             NS_IRDFLITERAL_IID);
static NS_DEFINE_IID(kIRDFContentIID,             NS_IRDFCONTENT_IID);
static NS_DEFINE_IID(kIRDFContentModelBuilderIID, NS_IRDFCONTENTMODELBUILDER_IID);
static NS_DEFINE_IID(kIRDFServiceIID,             NS_IRDFSERVICE_IID);

static NS_DEFINE_CID(kRDFServiceCID,              NS_RDFSERVICE_CID);
static NS_DEFINE_CID(kNameSpaceManagerCID,        NS_NAMESPACEMANAGER_CID);

////////////////////////////////////////////////////////////////////////

class RDFTreeBuilderImpl : public nsIRDFContentModelBuilder
{
private:
    nsIRDFDocument* mDocument;
    nsIRDFService*  mRDFService;
    nsIRDFCompositeDataSource* mDB;

public:
    RDFTreeBuilderImpl();
    virtual ~RDFTreeBuilderImpl();

    // nsISupports interface
    NS_DECL_ISUPPORTS

    // nsIRDFContentModelBuilder interface
    NS_IMETHOD SetDocument(nsIRDFDocument* aDocument);
    NS_IMETHOD CreateRoot(nsIRDFResource* aResource);
    NS_IMETHOD CreateContents(nsIRDFContent* aElement);
    NS_IMETHOD OnAssert(nsIRDFContent* aElement, nsIRDFResource* aProperty, nsIRDFNode* aValue);
    NS_IMETHOD OnUnassert(nsIRDFContent* aElement, nsIRDFResource* aProperty, nsIRDFNode* aValue);


    // Implementation methods
    nsresult
    FindChildByTag(nsIContent* aElement,
                   PRInt32 aNameSpaceID,
                   nsIAtom* aTag,
                   nsIContent** aChild);

    nsresult
    FindChildByTagAndResource(nsIContent* aElement,
                              PRInt32 aNameSpaceID,
                              nsIAtom* aTag,
                              nsIRDFResource* aResource,
                              nsIContent** aChild);

    nsresult
    EnsureElementHasGenericChild(nsIContent* aParent,
                                 PRInt32 aNameSpaceID,
                                 nsIAtom* aTag,
                                 nsIContent** aResult);

    nsresult
    FindTreeElement(nsIContent* aElement,
                    nsIContent** aTreeElement);

    nsresult
    AddTreeRow(nsIContent* aTreeItemElement,
               nsIRDFResource* aProperty,
               nsIRDFResource* aValue);

    nsresult
    CreateTreeItemRowAndCells(nsIContent* aTreeItemElement,
                              nsIContent** aTreeRowElement);

    nsresult
    FindTreeCellForProperty(nsIContent* aTreeRowElement,
                            nsIRDFResource* aProperty,
                            nsIRDFContent** aTreeCell);

    nsresult
    SetCellValue(nsIContent* aTreeRowElement,
                 nsIRDFResource* aProperty,
                 nsIRDFNode* aValue);

    // Most of this stuff is just noise that we need to implement
    // column headers from RDF. Hopefully this'll go away once the XUL
    // content sink gets written and we can specify it from there.
    nsresult
    EnsureTreeHasHeadAndRow(nsIContent* aParent,
                            nsIRDFNode* aColumnsContainer);
    
    nsresult
    SetColumnTitle(nsIContent* aColumnElement,
                   nsIRDFNode* aTitle);

    nsresult
    SetColumnResource(nsIContent* aColumnElement,
                      nsIRDFNode* aColumnIdentifier);

    nsresult

    AddTreeColumn(nsIContent* aElement,
                  nsIRDFNode* aColumn);


    PRBool
    IsRootColumnsProperty(nsIContent* aElement,
                          nsIRDFResource* aProperty);

	PRBool
	IsTreeElement(nsIContent* aElement);

    PRBool
    IsTreeItemElement(nsIContent* aElement);

    PRBool
    IsColumnSetElement(nsIContent* aElement);

    PRBool
    IsColumnProperty(nsIContent* aElement, nsIRDFResource* aProperty);

    PRBool
    IsColumnElement(nsIContent* node);
};

////////////////////////////////////////////////////////////////////////

static nsIAtom* kIdAtom;
static nsIAtom* kOpenAtom;
static nsIAtom* kResourceAtom;
static nsIAtom* kTreeAtom;
static nsIAtom* kTreeBodyAtom;
static nsIAtom* kTreeCellAtom;
static nsIAtom* kTreeChildrenAtom;
static nsIAtom* kTreeHeadAtom;
static nsIAtom* kTreeItemAtom;
static nsIAtom* kTreeRowAtom;

static PRInt32  kNameSpaceID_RDF;
static PRInt32  kNameSpaceID_XUL;

static nsIRDFResource* kNC_Title;
static nsIRDFResource* kNC_Column;

nsresult
NS_NewRDFTreeBuilder(nsIRDFContentModelBuilder** result)
{
    NS_PRECONDITION(result != nsnull, "null ptr");
    if (! result)
        return NS_ERROR_NULL_POINTER;

    RDFTreeBuilderImpl* builder = new RDFTreeBuilderImpl();
    if (! builder)
        return NS_ERROR_OUT_OF_MEMORY;

    NS_ADDREF(builder);
    *result = builder;
    return NS_OK;
}



RDFTreeBuilderImpl::RDFTreeBuilderImpl(void)
{
    NS_INIT_REFCNT();

    if (nsnull == kIdAtom) {
        kIdAtom           = NS_NewAtom("ID");
        kOpenAtom         = NS_NewAtom("OPEN");
        kResourceAtom     = NS_NewAtom("resource");

        kTreeAtom         = NS_NewAtom("tree");
        kTreeBodyAtom     = NS_NewAtom("treebody");
        kTreeCellAtom     = NS_NewAtom("treecell");
        kTreeChildrenAtom = NS_NewAtom("treechildren");
        kTreeHeadAtom     = NS_NewAtom("treehead");
        kTreeItemAtom     = NS_NewAtom("treeitem");
        kTreeRowAtom      = NS_NewAtom("treerow");

        nsresult rv;
        nsINameSpaceManager* mgr;
        if (NS_SUCCEEDED(rv = nsRepository::CreateInstance(kNameSpaceManagerCID,
                                                           nsnull,
                                                           kINameSpaceManagerIID,
                                                           (void**) &mgr))) {

// XXX This is sure to change. Copied from mozilla/layout/xul/content/src/nsXULAtoms.cpp
static const char kXULNameSpaceURI[]
    = "http://www.mozilla.org/keymaster/gatekeeper/there.is.only.xul";

static const char kRDFNameSpaceURI[]
    = RDF_NAMESPACE_URI;

            rv = mgr->RegisterNameSpace(kXULNameSpaceURI, kNameSpaceID_XUL);
            NS_ASSERTION(NS_SUCCEEDED(rv), "unable to register XUL namespace");

            rv = mgr->RegisterNameSpace(kRDFNameSpaceURI, kNameSpaceID_RDF);
            NS_ASSERTION(NS_SUCCEEDED(rv), "unable to register RDF namespace");

            NS_RELEASE(mgr);
        }
        else {
            NS_ERROR("couldn't create namepsace manager");
        }
    }
    else {
        NS_ADDREF(kIdAtom);
        NS_ADDREF(kOpenAtom);
        NS_ADDREF(kResourceAtom);

        NS_ADDREF(kTreeAtom);
        NS_ADDREF(kTreeBodyAtom);
        NS_ADDREF(kTreeCellAtom);
        NS_ADDREF(kTreeChildrenAtom);
        NS_ADDREF(kTreeHeadAtom);
        NS_ADDREF(kTreeItemAtom);
        NS_ADDREF(kTreeRowAtom);
    }
}

RDFTreeBuilderImpl::~RDFTreeBuilderImpl(void)
{
    NS_IF_RELEASE(mDB);

    if (mRDFService)
        nsServiceManager::ReleaseService(kRDFServiceCID, mRDFService);

    nsrefcnt refcnt;

    NS_RELEASE2(kIdAtom,           refcnt);
    NS_RELEASE2(kOpenAtom,         refcnt);
    NS_RELEASE2(kResourceAtom,     refcnt);

    NS_RELEASE2(kTreeAtom,         refcnt);
    NS_RELEASE2(kTreeBodyAtom,     refcnt);
    NS_RELEASE2(kTreeCellAtom,     refcnt);
    NS_RELEASE2(kTreeChildrenAtom, refcnt);
    NS_RELEASE2(kTreeHeadAtom,     refcnt);
    NS_RELEASE2(kTreeItemAtom,     refcnt);
    NS_RELEASE2(kTreeRowAtom,      refcnt);

    NS_RELEASE2(kNC_Title,         refcnt);
    NS_RELEASE2(kNC_Column,        refcnt);
}

////////////////////////////////////////////////////////////////////////

NS_IMPL_ISUPPORTS(RDFTreeBuilderImpl, kIRDFContentModelBuilderIID);

////////////////////////////////////////////////////////////////////////
// nsIRDFContentModelBuilder methods

NS_IMETHODIMP
RDFTreeBuilderImpl::SetDocument(nsIRDFDocument* aDocument)
{
    NS_PRECONDITION(aDocument != nsnull, "null ptr");
    if (! aDocument)
        return NS_ERROR_NULL_POINTER;

    mDocument = aDocument; // not refcounted

    nsresult rv;
    if (NS_FAILED(rv = mDocument->GetDataBase(mDB)))
        return rv;

    if (NS_FAILED(rv = nsServiceManager::GetService(kRDFServiceCID,
                                                    kIRDFServiceIID,
                                                    (nsISupports**) &mRDFService)))
        return rv;

    // Get shared resources here
    if (kNC_Title == nsnull) {
        mRDFService->GetResource(kURINC_Title,  &kNC_Title);
        mRDFService->GetResource(kURINC_Column, &kNC_Column);
    }

    return NS_OK;
}

NS_IMETHODIMP
RDFTreeBuilderImpl::CreateRoot(nsIRDFResource* aResource)
{
    // Create a structure that looks like this:
    //
    // <html:document>
    //    <html:body>
    //       <xul:tree>
    //       </xul:tree>
    //    </html:body>
    // </html:document>
    //
    // Eventually, we should be able to do away with the HTML tags and
    // just have it create XUL in some other context.

    nsresult rv;
    nsCOMPtr<nsIAtom>       tag;
    nsCOMPtr<nsIDocument>   doc;

    if (NS_FAILED(rv = mDocument->QueryInterface(kIDocumentIID,
                                                 (void**) getter_AddRefs(doc))))
        return rv;

    // Create the DOCUMENT element
    if ((tag = dont_AddRef(NS_NewAtom("DOCUMENT"))) == nsnull)
        return NS_ERROR_OUT_OF_MEMORY;

    nsCOMPtr<nsIContent> root;
    if (NS_FAILED(rv = NS_NewRDFGenericElement(getter_AddRefs(root),
                                               kNameSpaceID_HTML, /* XXX */
                                               tag)))
        return rv;

    doc->SetRootContent(NS_STATIC_CAST(nsIContent*, root));

    // Create the BODY element
    nsCOMPtr<nsIContent> body;
    if ((tag = dont_AddRef(NS_NewAtom("BODY"))) == nsnull)
        return NS_ERROR_OUT_OF_MEMORY;

    if (NS_FAILED(rv = NS_NewRDFGenericElement(getter_AddRefs(body),
                                               kNameSpaceID_HTML, /* XXX */
                                               tag)))
        return rv;


    // Attach the BODY to the DOCUMENT
    if (NS_FAILED(rv = root->AppendChildTo(body, PR_FALSE)))
        return rv;

    // Create the xul:tree element, and indicate that children should
    // be recursively generated on demand
    nsCOMPtr<nsIRDFContent> tree;
    if (NS_FAILED(rv = NS_NewRDFResourceElement(getter_AddRefs(tree),
                                                aResource,
                                                kNameSpaceID_XUL,
                                                kTreeAtom)))
        return rv;

    if (NS_FAILED(rv = tree->SetContainer(PR_TRUE)))
        return rv;

    // Attach the TREE to the BODY
    if (NS_FAILED(rv = body->AppendChildTo(tree, PR_FALSE)))
        return rv;

    return NS_OK;
}


NS_IMETHODIMP
RDFTreeBuilderImpl::CreateContents(nsIRDFContent* aElement)
{
    nsresult rv;

    nsCOMPtr<nsIRDFResource> resource;
    if (NS_FAILED(rv = aElement->GetResource(*getter_AddRefs(resource))))
        return rv;

    // XXX Eventually, we may want to factor this into one method that
    // handles RDF containers (RDF:Bag, et al.) and another that
    // handles multi-attributes. For performance...

    // Create a cursor that'll enumerate all of the outbound arcs
    nsCOMPtr<nsIRDFArcsOutCursor> properties;

    if (NS_FAILED(rv = mDB->ArcLabelsOut(resource, getter_AddRefs(properties))))
        return rv;

    while (NS_SUCCEEDED(rv = properties->Advance())) {
        nsCOMPtr<nsIRDFResource> property;

        if (NS_FAILED(rv = properties->GetPredicate(getter_AddRefs(property))))
            break;

        // If it's not a tree property, then it doesn't specify an
        // object that is member of the current container element;
        // rather, it specifies a "simple" property of the container
        // element. Skip it.
        PRBool isTreeProperty;
        if (NS_FAILED(rv = mDocument->IsTreeProperty(property, &isTreeProperty)) || !isTreeProperty) {
            continue;
        }
#ifdef DEBUG
        {
            const char* s;
            property->GetValue(&s);
        }
#endif

        // Create a second cursor that'll enumerate all of the values
        // for all of the arcs.
        nsCOMPtr<nsIRDFAssertionCursor> assertions;
        if (NS_FAILED(rv = mDB->GetTargets(resource, property, PR_TRUE, getter_AddRefs(assertions))))
            break;

        while (NS_SUCCEEDED(rv = assertions->Advance())) {
            nsCOMPtr<nsIRDFNode> value;
            if (NS_FAILED(rv = assertions->GetValue(getter_AddRefs(value))))
                return rv; // XXX fatal

            // XXX Eventually, we should make this call the
            // appropriate thing directly to avoid the overhead of
            // figuring out what the heck is going on in OnAssert():
            // this'll be much easier once XUL specifies all the
            // column info.
            if (NS_FAILED(rv = OnAssert(aElement, property, value)))
                return rv; // XXX fatal
        }
    }

    if (rv == NS_ERROR_RDF_CURSOR_EMPTY)
        // This is a normal return code from nsIRDFCursor::Advance()
        rv = NS_OK;

    return rv;
}


NS_IMETHODIMP
RDFTreeBuilderImpl::OnAssert(nsIRDFContent* aElement,
                             nsIRDFResource* aProperty,
                             nsIRDFNode* aValue)
{
    // So here's where a new assertion comes in. Figure out what that
    // means to the content model.

    nsresult rv;
    nsCOMPtr<nsIRDFResource> resource;

    if (NS_SUCCEEDED(rv = aValue->QueryInterface(kIRDFResourceIID,
                                                 (void**) getter_AddRefs(resource)))) {
        if (IsRootColumnsProperty(aElement, aProperty)) {
            // XXX this should go away once XUL specifies columns...
            return EnsureTreeHasHeadAndRow(aElement, aValue);
        }

        if (IsColumnProperty(aElement, aProperty)) {
            // XXX this should go away once XUL specifies columns...
            return AddTreeColumn(aElement, aValue);
        }

        PRBool isTreeProperty;
        if (NS_SUCCEEDED(mDocument->IsTreeProperty(aProperty, &isTreeProperty)) && isTreeProperty) {
            return AddTreeRow(aElement, aProperty, resource);
        }

#if 0
        if (rdf_IsContainer(mDB, resource)) {
            return AddTreeRow(aElement, aProperty, resource);
        }
#endif

        // fall through and add URI as a cell value
    }

    // XXX this should go away once XUL specifies columns...
    if (IsColumnElement(aElement)) {
        if (aProperty == kNC_Title) {
            return SetColumnTitle(aElement, aValue);
        }
        else if (aProperty == kNC_Column) {
            return SetColumnResource(aElement, aValue);
        }
        else {
            // don't add any other crap to the column element
            return NS_OK;
        }
    }

    // XXX don't add random properties to the root or the column
    // set. Properties that hang off of the root end up in the root's
    // TREEHEAD tag, which makes them appear as dummy columns
    // sometimes.
    // XXX this should go away once XUL specifies columns...
	if (IsTreeElement(aElement) || IsColumnSetElement(aElement)) {
        return NS_OK;
    }

    return SetCellValue(aElement, aProperty, aValue);
}


NS_IMETHODIMP
RDFTreeBuilderImpl::OnUnassert(nsIRDFContent* aElement,
                               nsIRDFResource* aProperty,
                               nsIRDFNode* aValue)
{
    return NS_OK;
}

////////////////////////////////////////////////////////////////////////
// Implementation methods

nsresult
RDFTreeBuilderImpl::FindChildByTag(nsIContent* aElement,
                                   PRInt32 aNameSpaceID,
                                   nsIAtom* aTag,
                                   nsIContent** aChild)
{
    nsresult rv;

    PRInt32 count;
    if (NS_FAILED(rv = aElement->ChildCount(count)))
        return rv;

    for (PRInt32 i = 0; i < count; ++i) {
        nsCOMPtr<nsIContent> kid;
        if (NS_FAILED(rv = aElement->ChildAt(i, *getter_AddRefs(kid))))
            return rv; // XXX fatal

        PRInt32 nameSpaceID;
        if (NS_FAILED(rv = kid->GetNameSpaceID(nameSpaceID)))
            return rv; // XXX fatal

        if (nameSpaceID != aNameSpaceID)
            continue; // wrong namespace

        nsCOMPtr<nsIAtom> kidTag;
        if (NS_FAILED(rv = kid->GetTag(*getter_AddRefs(kidTag))))
            return rv; // XXX fatal

        if (kidTag != aTag)
            continue;

        *aChild = kid;
        NS_ADDREF(*aChild);
        return NS_OK;
    }

    return NS_ERROR_RDF_NO_VALUE; // not found
}


nsresult
RDFTreeBuilderImpl::FindChildByTagAndResource(nsIContent* aElement,
                                              PRInt32 aNameSpaceID,
                                              nsIAtom* aTag,
                                              nsIRDFResource* aResource,
                                              nsIContent** aChild)
{
    nsresult rv;

    const char* resourceURI;
    if (NS_FAILED(rv = aResource->GetValue(&resourceURI)))
        return rv;

    PRInt32 count;
    if (NS_FAILED(rv = aElement->ChildCount(count)))
        return rv;

    for (PRInt32 i = 0; i < count; ++i) {
        nsCOMPtr<nsIContent> kid;
        if (NS_FAILED(rv = aElement->ChildAt(i, *getter_AddRefs(kid))))
            return rv; // XXX fatal

        // Make sure it's a <xul:treecell>
        PRInt32 nameSpaceID;
        if (NS_FAILED(rv = kid->GetNameSpaceID(nameSpaceID)))
            return rv; // XXX fatal

        if (nameSpaceID != aNameSpaceID)
            continue; // wrong namespace

        nsCOMPtr<nsIAtom> tag;
        if (NS_FAILED(rv = kid->GetTag(*getter_AddRefs(tag))))
            return rv; // XXX fatal

        if (tag != aTag)
            continue; // wrong tag

        // Now get the resource ID from the RDF:ID attribute. We do it
        // via the content model, because you're never sure who
        // might've added this stuff in...
        nsAutoString uri;

        if (NS_FAILED(kid->GetAttribute(kNameSpaceID_RDF, kIdAtom, uri)))
            continue; // not specified

        if (! uri.Equals(resourceURI))
            continue; // not the resource we want

        // Fount it!
        *aChild = kid;
        NS_ADDREF(*aChild);
        return NS_OK;
    }

    return NS_ERROR_RDF_NO_VALUE; // not found
}


nsresult
RDFTreeBuilderImpl::EnsureElementHasGenericChild(nsIContent* parent,
                                                 PRInt32 nameSpaceID,
                                                 nsIAtom* tag,
                                                 nsIContent** result)
{
    nsresult rv;

    if (NS_SUCCEEDED(rv = FindChildByTag(parent, nameSpaceID, tag, result)))
        return NS_OK;

    if (rv != NS_ERROR_RDF_NO_VALUE)
        return rv; // something really bad happened

    // if we get here, we need to construct a new child element.
    nsCOMPtr<nsIContent> element;

    if (NS_FAILED(rv = NS_NewRDFGenericElement(getter_AddRefs(element), nameSpaceID, tag)))
        return rv;

    if (NS_FAILED(rv = parent->AppendChildTo(element, PR_FALSE)))
        return rv;

    *result = element;
    NS_ADDREF(*result);
    return NS_OK;
}

nsresult
RDFTreeBuilderImpl::AddTreeRow(nsIContent* aElement,
                               nsIRDFResource* aProperty,
                               nsIRDFResource* aValue)
{
    // If it's a tree property, then we need to add the new child
    // element to a special "children" element in the parent.  The
    // child element's value will be the value of the
    // property. We'll also attach an "ID=" attribute to the new
    // child; e.g.,
    //
    // <xul:treeitem>
    //   ...
    //   <xul:treechildren>
    //     <xul:treeitem RDF:ID="value">
    //        <xul:treerow>
    //           <xul:treecell RDF:ID="column1">
    //              <!-- value not specified until SetCellValue() -->
    //           </xul:treecell>
    //
    //           </xul:treecell RDF:ID="column2"/>
    //              <!-- value not specified until SetCellValue() -->
    //           </xul:treecell>
    //
    //           ...
    //
    //        </xul:treerow>
    //
    //        <!-- Other content recursively generated -->
    //
    //     </xul:treeitem>
    //   </xul:treechildren>
    //   ...
    // </xul:treeitem>

    nsresult rv;

    nsCOMPtr<nsIContent> treeChildren;
    if (IsTreeElement(aElement)) {
        // Ensure that the <xul:treebody> exists on the root
        if (NS_FAILED(rv = EnsureElementHasGenericChild(aElement,
                                                        kNameSpaceID_XUL,
                                                        kTreeBodyAtom,
                                                        getter_AddRefs(treeChildren))))
            return rv;
    }
    else if (IsTreeItemElement(aElement)) {
        // Ensure that the <xul:treechildren> element exists on the parent.
        if (NS_FAILED(rv = EnsureElementHasGenericChild(aElement,
                                                        kNameSpaceID_XUL,
                                                        kTreeChildrenAtom,
                                                        getter_AddRefs(treeChildren))))
            return rv;
    }
    else {
        NS_ERROR("new tree row doesn't fit here!");
        return NS_ERROR_UNEXPECTED;
    }

    // Create the <xul:treeitem> element
    nsCOMPtr<nsIRDFContent> treeItem;
    if (NS_FAILED(rv = NS_NewRDFResourceElement(getter_AddRefs(treeItem),
                                                aValue,
                                                kNameSpaceID_XUL,
                                                kTreeItemAtom)))
        return rv;

    // Add the <xul:treeitem> to the <xul:treechildren> element.
    treeChildren->AppendChildTo(treeItem, PR_TRUE);

    // Create the row and cell substructure
    nsCOMPtr<nsIContent> rowElement;
    if (NS_FAILED(rv = CreateTreeItemRowAndCells(treeItem,
                                                 getter_AddRefs(rowElement))))
        return rv;

    if (NS_FAILED(rv = treeItem->SetContainer(PR_TRUE)))
        return rv;

#define ALL_NODES_OPEN_HACK
#ifdef ALL_NODES_OPEN_HACK
    // XXX what should the namespace really be?
    if (NS_FAILED(rv = aElement->SetAttribute(kNameSpaceID_None, kOpenAtom, "TRUE", PR_FALSE)))
        return rv;
#endif

    return NS_OK;
}


nsresult
RDFTreeBuilderImpl::EnsureTreeHasHeadAndRow(nsIContent* aTreeElement,
                                            nsIRDFNode* aColumnsContainer)
{
    nsresult rv;
    nsCOMPtr<nsIContent> treeHead;

    // This constructs the tree header beneath the xul:tree tag:
    //
    // <xul:tree>
    //   <xul:treehead>
    //     <xul:treerow RDF:ID="...">
    //     </xul:treerow>
    //   </xul:treehead>
    // </xul:tree>
    //
    // Where the RDF:ID corresponds to the URI of the RDF container
    // that identifies all of the columns

    // See if the <xul:treehead> is already there...
    if (NS_SUCCEEDED(rv = FindChildByTag(aTreeElement,
                                         kNameSpaceID_XUL,
                                         kTreeHeadAtom,
                                         getter_AddRefs(treeHead))))
        return NS_OK;

    if (rv != NS_ERROR_RDF_NO_VALUE)
        return rv; // something serious happened

    // ...nope. So create it.
    if (NS_FAILED(rv = NS_NewRDFGenericElement(getter_AddRefs(treeHead),
                                               kNameSpaceID_XUL,
                                               kTreeHeadAtom)))
        return rv;

    // ...and now create <xul:treerow RDF:ID="..."> beneath <xul:treehead>
    nsCOMPtr<nsIRDFResource> resource;
    if (NS_FAILED(rv = aColumnsContainer->QueryInterface(kIRDFResourceIID,
                                                         (void**) getter_AddRefs(resource))))
        return rv;

    nsCOMPtr<nsIRDFContent> treeRow;
    if (NS_FAILED(rv = NS_NewRDFResourceElement(getter_AddRefs(treeRow),
                                                resource,
                                                kNameSpaceID_XUL,
                                                kTreeRowAtom)))
        return rv;

    if (NS_FAILED(rv = treeRow->SetContainer(PR_TRUE)))
        return rv;

    // Now put 'em both into the content model.
    if (NS_FAILED(rv = aTreeElement->AppendChildTo(treeHead, PR_FALSE)))
        return rv;

    if (NS_FAILED(rv = treeHead->AppendChildTo(treeRow, PR_FALSE)))
        return rv;

    return NS_OK;
}


nsresult
RDFTreeBuilderImpl::SetColumnTitle(nsIContent* aColumnElement,
                                   nsIRDFNode* aTitleNode)
{
    nsresult rv;

    nsCOMPtr<nsIRDFLiteral> title;

    if (NS_FAILED(aTitleNode->QueryInterface(kIRDFLiteralIID,
                                            (void**) getter_AddRefs(title))))
        return rv;

    if (NS_FAILED(rv = rdf_AttachTextNode(aColumnElement, title)))
        return rv;

    return NS_OK;
}


nsresult
RDFTreeBuilderImpl::SetColumnResource(nsIContent* aColumn,
                                      nsIRDFNode* aNode)
{
    nsresult rv;

    // XXX if we were paranoid, we'd make sure that aTreeCellElement
    // was really a <xul:treecell> in the tree's treehead.

    // This sets the RDF:resource property on the <xul:treecell>
    // element to refer to the resource that this column is to
    // represent.

    nsCOMPtr<nsIRDFResource> resource;
    if (NS_FAILED(rv = aNode->QueryInterface(kIRDFResourceIID,
                                             (void**) getter_AddRefs(resource))))
        return rv;

    const char* uri;
    if (NS_FAILED(rv = resource->GetValue(&uri)))
        return rv;

    if (NS_FAILED(rv = aColumn->SetAttribute(kNameSpaceID_RDF,
                                             kResourceAtom,
                                             uri,
                                             PR_FALSE)))
        return rv;

    // XXX At this point, if we've actually changed the column
    // resource to something different, we need to grovel through all
    // the rows in the table and fix up the cells corresponding to
    // this column.

    return NS_OK;
}


nsresult
RDFTreeBuilderImpl::AddTreeColumn(nsIContent* aTreeRowElement,
                                  nsIRDFNode* aColumnNode)
{
    nsresult rv;

    nsCOMPtr<nsIRDFResource> column;
    if (NS_FAILED(rv = aColumnNode->QueryInterface(kIRDFResourceIID,
                                                   (void**) getter_AddRefs(column))))
        return rv;

    // Create a column element as a <xul:treecell>, with an RDF:ID of
    // the column resource.
    nsCOMPtr<nsIRDFContent> columnElement;
    if (NS_FAILED(rv = NS_NewRDFResourceElement(getter_AddRefs(columnElement),
                                                column,
                                                kNameSpaceID_XUL,
                                                kTreeCellAtom)))
        return rv;

    if (NS_FAILED(rv = aTreeRowElement->AppendChildTo(columnElement, PR_FALSE)))
        return rv;

    // Try to set the title now, if we can
    nsCOMPtr<nsIRDFNode> titleNode;
    if (NS_SUCCEEDED(rv = mDB->GetTarget(column, kNC_Title, PR_TRUE, getter_AddRefs(titleNode))))
    {
        rv = SetColumnTitle(columnElement, titleNode);
        NS_VERIFY(NS_SUCCEEDED(rv), "unable to set column title");
    }

    // Try to set the column resource, if we can
    nsCOMPtr<nsIRDFNode> columnNode;
    if (NS_SUCCEEDED(rv = mDB->GetTarget(column, kNC_Column, PR_TRUE, getter_AddRefs(columnNode))))
    {
        rv = SetColumnResource(columnElement, columnNode);
        NS_VERIFY(NS_SUCCEEDED(rv), "unable to set column resource");
    }

    // XXX At this point, we need to grovel through *all* the rows in
    // the table, and add the cell corresponding to this column to
    // each.

    return NS_OK;
}


nsresult
RDFTreeBuilderImpl::FindTreeElement(nsIContent* aElement,
                                    nsIContent** aTreeElement)
{
    nsresult rv;

    // walk up the tree until you find <xul:tree>
    nsCOMPtr<nsIContent> element(aElement);

    while (element) {
        PRInt32 nameSpaceID;
        if (NS_FAILED(rv = element->GetNameSpaceID(nameSpaceID)))
            return rv; // XXX fatal

        if (nameSpaceID == kNameSpaceID_XUL) {
            nsCOMPtr<nsIAtom> tag;
            if (NS_FAILED(rv = element->GetTag(*getter_AddRefs(tag))))
                return rv;

            if (tag == kTreeAtom) {
                *aTreeElement = element;
                NS_ADDREF(*aTreeElement);
                return NS_OK;
            }
        }

        // up to the parent...
        nsCOMPtr<nsIContent> parent;
        element->GetParent(*getter_AddRefs(parent));
        element = parent;
    }

    NS_ERROR("must not've started from within a xul:tree");
    return NS_ERROR_FAILURE;
}

nsresult
RDFTreeBuilderImpl::CreateTreeItemRowAndCells(nsIContent* aTreeItemElement,
                                              nsIContent** aTreeRowElement)
{
    // <xul:treeitem>
    //   <xul:treerow>
    //     <xul:treecell RDF:ID="property">value</xul:treecell>
    //   </xul:treerow>
    //   ...
    // </xul:treeitem>
    nsresult rv;

    // XXX at this point, we should probably ensure that aElement is
    // actually a <xul:treeitem>...

    // Get the treeitem's resource so that we can generate cell
    // values. We could QI for the nsIRDFResource here, but doing this
    // via the nsIContent interface allows us to support generic nodes
    // that might get added in by DOM calls.
    nsAutoString treeItemResourceURI;
    if (NS_FAILED(rv = aTreeItemElement->GetAttribute(kNameSpaceID_RDF,
                                                      kIdAtom,
                                                      treeItemResourceURI)))
        return rv;

    nsCOMPtr<nsIRDFResource> treeItemResource;
    if (NS_FAILED(rv = mRDFService->GetUnicodeResource(treeItemResourceURI,
                                                       getter_AddRefs(treeItemResource))))
        return rv;

    // Create the <xul:treerow> element
    nsCOMPtr<nsIContent> rowElement;
    if (NS_FAILED(rv = EnsureElementHasGenericChild(aTreeItemElement,
                                                    kNameSpaceID_XUL,
                                                    kTreeRowAtom,
                                                    getter_AddRefs(rowElement))))
        return rv;

    // Now walk up to the primordial <xul:tree> tag, then down into
    // the <xul:treehead> so that we can iterate through all of the
    // tree's columns.
    nsCOMPtr<nsIContent> treeElement;
    if (NS_FAILED(rv = FindTreeElement(aTreeItemElement,
                                       getter_AddRefs(treeElement))))
        return rv;

    nsCOMPtr<nsIContent> treeHead;
    if (NS_FAILED(rv = FindChildByTag(treeElement,
                                      kNameSpaceID_XUL,
                                      kTreeHeadAtom,
                                      getter_AddRefs(treeHead))))
        return rv;

    nsCOMPtr<nsIContent> treeRow;
    if (NS_FAILED(rv = FindChildByTag(treeHead,
                                      kNameSpaceID_XUL,
                                      kTreeRowAtom,
                                      getter_AddRefs(treeRow))))
        return rv;

    PRInt32 count;
    if (NS_FAILED(rv = treeRow->ChildCount(count)))
        return rv;

    // Iterate through all the columns that have been specified,
    // constructing a cell in the content model for each one.
    for (PRInt32 i = 0; i < count; ++i) {

        nsCOMPtr<nsIContent> kid;
        if (NS_FAILED(rv = treeRow->ChildAt(i, *getter_AddRefs(kid))))
            return rv; // XXX fatal

        PRInt32 nameSpaceID;
        if (NS_FAILED(rv = kid->GetNameSpaceID(nameSpaceID)))
            return rv; // XXX fatal

        if (nameSpaceID != kNameSpaceID_XUL)
            continue; // not <xul:treecell>

        nsCOMPtr<nsIAtom> tag;
        if (NS_FAILED(rv = kid->GetTag(*getter_AddRefs(tag))))
            return rv; // XXX fatal

        if (tag != kTreeCellAtom)
            continue; // not <xul:treecell>

        // The column property is stored in the RDF:resource attribute
        // of the tag.
        nsAutoString uri;
        if (NS_FAILED(rv = kid->GetAttribute(kNameSpaceID_RDF, kResourceAtom, uri))) {
            NS_WARNING("column header with no RDF:resource");
            continue;
        }

        // now construct the cell
        nsCOMPtr<nsIRDFResource> property;
        if (NS_FAILED(mRDFService->GetUnicodeResource(uri, getter_AddRefs(property))))
            return rv; // XXX fatal

        nsCOMPtr<nsIRDFContent> cellElement;
        if (NS_FAILED(rv = NS_NewRDFResourceElement(getter_AddRefs(cellElement),
                                                    property,
                                                    kNameSpaceID_XUL,
                                                    kTreeCellAtom)))
            return rv;

        if (NS_FAILED(rv = rowElement->AppendChildTo(cellElement, PR_FALSE)))
            return rv;

        // Set its value, if we know it.
        nsCOMPtr<nsIRDFNode> value;
        if (NS_FAILED(rv = mDB->GetTarget(treeItemResource,
                                          property,
                                          PR_TRUE,
                                          getter_AddRefs(value)))) {
            if (rv != NS_ERROR_RDF_NO_VALUE) {
                NS_ERROR("error getting cell's value");
                return rv; // XXX something serious happened
            }

            // otherwise, there was no value for this. It'll have to
            // get set later, when an OnAssert() comes in (if it even
            // has a value at all...)
            continue;
        }

        if (NS_FAILED(rv = rdf_AttachTextNode(cellElement, value)))
            return rv; // XXX fatal
    }

    *aTreeRowElement = rowElement;
    NS_ADDREF(*aTreeRowElement);
    return NS_OK;
}


nsresult
RDFTreeBuilderImpl::SetCellValue(nsIContent* aTreeItemElement,
                                 nsIRDFResource* aProperty,
                                 nsIRDFNode* aValue)
{
    nsresult rv;

    // XXX We assume that aTreeItemElement is actually a
    // <xul:treeitem>, it'd be good to enforce this...

    // Find the <xul:treerow> child of the <xul:treeitem>
    nsCOMPtr<nsIContent> rowElement;
    if (NS_FAILED(rv = FindChildByTag(aTreeItemElement,
                                      kNameSpaceID_XUL,
                                      kTreeRowAtom,
                                      getter_AddRefs(rowElement)))) {
        NS_ERROR("couldn't find xul:treerow in xul:treeitem");
        return rv;
    }

    // Find the <xul:cell> beneath <xul:treerow> that corresponds to
    // aProperty
    nsCOMPtr<nsIContent> cellElement;
    if (NS_FAILED(rv = FindChildByTagAndResource(rowElement,
                                                 kNameSpaceID_XUL,
                                                 kTreeCellAtom,
                                                 aProperty,
                                                 getter_AddRefs(cellElement)))) {
        if (rv == NS_ERROR_RDF_NO_VALUE) {
            // If we can't find a cell for the specified property,
            // that just means there isn't a column in the tree for
            // that property. No big deal.
            return NS_OK;
        }

        return rv; // XXX somthing worse happened...
    }

    // XXX if the cell already has a value, we need to replace it, not
    // just append new text...

    if (NS_FAILED(rv = rdf_AttachTextNode(cellElement, aValue)))
        return rv;

    return NS_OK;
}


PRBool
RDFTreeBuilderImpl::IsRootColumnsProperty(nsIContent* aElement,
                                          nsIRDFResource* aProperty)
{
    // Returns PR_TRUE if the element is the tree's root, and the
    // property is "NC:Columns", which specifies the column set for
    // the tree.
    if (! IsTreeElement(aElement))
        return PR_FALSE;

    nsresult rv;
    PRBool isColumnsProperty;
    if (NS_FAILED(rv = aProperty->EqualsString(kURINC_Columns, &isColumnsProperty)))
        return PR_FALSE;

    return isColumnsProperty;
}


PRBool
RDFTreeBuilderImpl::IsTreeElement(nsIContent* element)
{
    // Returns PR_TRUE if the element is a <xul:tree> element.
    nsresult rv;

    // "element" must be xul:tree
    nsCOMPtr<nsIAtom> elementTag;
    if (NS_FAILED(rv = element->GetTag(*getter_AddRefs(elementTag))))
        return PR_FALSE;

    if (elementTag != kTreeAtom)
        return PR_FALSE;

    return PR_TRUE;
}

PRBool
RDFTreeBuilderImpl::IsTreeItemElement(nsIContent* aElement)
{
    // Returns PR_TRUE if the element is a <xul:treeitem> element.
    nsresult rv;

    // "element" must be xul:tree
    nsCOMPtr<nsIAtom> tag;
    if (NS_FAILED(rv = aElement->GetTag(*getter_AddRefs(tag))))
        return PR_FALSE;

    if (tag != kTreeItemAtom)
        return PR_FALSE;

    return PR_TRUE;
}


PRBool
RDFTreeBuilderImpl::IsColumnSetElement(nsIContent* aElement)
{
    // Returns PR_TRUE if the specified element is the tree's column
    // set; that is, it is the <xul:treerow> inside the <xul:treehead>
    // that specifies the tree's columns.
    nsresult rv;

    // The element must be xul:treerow
    nsCOMPtr<nsIAtom> elementTag;
    if (NS_FAILED(rv = aElement->GetTag(*getter_AddRefs(elementTag))))
        return PR_FALSE;

    if (elementTag != kTreeRowAtom)
        return PR_FALSE;

    // "parent" must be xul:treehead
    nsCOMPtr<nsIContent> parent;
    if (NS_FAILED(rv = aElement->GetParent(*getter_AddRefs(parent))))
        return PR_FALSE;

    nsCOMPtr<nsIAtom> parentTag;
    if (NS_FAILED(rv = parent->GetTag(*getter_AddRefs(parentTag))))
        return PR_FALSE;

    if (parentTag != kTreeHeadAtom)
        return PR_FALSE;

    // "grand-parent" must be xul:tree
    nsCOMPtr<nsIContent> grandParent;
    if (NS_FAILED(rv = parent->GetParent(*getter_AddRefs(grandParent))))
        return PR_FALSE;

    if (! IsTreeElement(grandParent))
        return PR_FALSE;

    // XXX we could be really anal here and enforce that the parent's
    // resource is actually an RDF container, but what the hell.
    return PR_TRUE;
}

PRBool
RDFTreeBuilderImpl::IsColumnProperty(nsIContent* aElement,
                                     nsIRDFResource* aProperty)
{
    // Returns PR_TRUE if the element is the column set element, and
    // the RDF property specifies a member of the column set. (This
    // assumes that all columns are specified in an RDF container;
    // therefore, the individual columns will be ordinal elements of
    // the container.)
    if (IsColumnSetElement(aElement) && rdf_IsOrdinalProperty(aProperty))
        return PR_TRUE;
    else
        return PR_FALSE;
}


PRBool
RDFTreeBuilderImpl::IsColumnElement(nsIContent* aElement)
{
    // Returns PR_TRUE if the specified element is a column element,
    // that is, a <xul:treecell> in the treehead:
    //
    // <xul:tree>
    //   <xul:treehead>
    //     <xul:treerow>
    //       <!-- Below is a "column element" -->
    //       <xul:treecell>
    //       ...
    //
    nsresult rv;

    // "element" must be xul:treecell
    nsCOMPtr<nsIAtom> elementTag;
    if (NS_FAILED(rv = aElement->GetTag(*getter_AddRefs(elementTag))))
        return PR_FALSE;

    if (elementTag != kTreeCellAtom)
        return PR_FALSE;

    // "parent" must be the column set element.
    nsCOMPtr<nsIContent> parent;
    if (NS_FAILED(rv = aElement->GetParent(*getter_AddRefs(parent))))
        return PR_FALSE;

    return IsColumnSetElement(parent);
}
