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

  2) We have a serious problem if all the columns aren't created by
     the time that we start inserting rows into the table. Need to fix
     this so that columns can be dynamically added and removed (we
     need to do this anyway to handle column re-ordering or
     manipulation via DOM calls).

  3) There's lots of stuff screwed up with the ordering of walking the
     tree, creating children, getting assertions, etc. A lot of this
     has to do with the "are children already generated" state
     variable that lives in the RDFResourceElementImpl. I need to sit
     down and really figure out what the heck this needs to do.

 */

#include "nsCOMPtr.h"
#include "nsCRT.h"
#include "nsIAtom.h"
#include "nsIDocument.h"
#include "nsIRDFContent.h"
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
#include "nsLayoutCID.h"
#include "nsRDFCID.h"
#include "nsRDFContentUtils.h"
#include "nsString.h"
#include "rdf.h"
#include "rdfutil.h"

////////////////////////////////////////////////////////////////////////

DEFINE_RDF_VOCAB(NC_NAMESPACE_URI, NC, child);
DEFINE_RDF_VOCAB(NC_NAMESPACE_URI, NC, Columns);
DEFINE_RDF_VOCAB(NC_NAMESPACE_URI, NC, Column);
DEFINE_RDF_VOCAB(NC_NAMESPACE_URI, NC, Folder);
DEFINE_RDF_VOCAB(NC_NAMESPACE_URI, NC, Title);

DEFINE_RDF_VOCAB(RDF_NAMESPACE_URI, RDF, child);

////////////////////////////////////////////////////////////////////////

static NS_DEFINE_IID(kIDocumentIID,               NS_IDOCUMENT_IID);
static NS_DEFINE_IID(kINameSpaceManagerIID,       NS_INAMESPACEMANAGER_IID);
static NS_DEFINE_IID(kIRDFResourceIID,            NS_IRDFRESOURCE_IID);
static NS_DEFINE_IID(kIRDFLiteralIID,             NS_IRDFLITERAL_IID);
static NS_DEFINE_IID(kIRDFContentIID,             NS_IRDFCONTENT_IID);
static NS_DEFINE_IID(kIRDFContentModelBuilderIID, NS_IRDFCONTENTMODELBUILDER_IID);
static NS_DEFINE_IID(kIRDFObserverIID,            NS_IRDFOBSERVER_IID);
static NS_DEFINE_IID(kIRDFServiceIID,             NS_IRDFSERVICE_IID);
static NS_DEFINE_IID(kISupportsIID,               NS_ISUPPORTS_IID);

static NS_DEFINE_CID(kNameSpaceManagerCID,        NS_NAMESPACEMANAGER_CID);
static NS_DEFINE_CID(kRDFServiceCID,              NS_RDFSERVICE_CID);

////////////////////////////////////////////////////////////////////////

class RDFTreeBuilderImpl : public nsIRDFContentModelBuilder,
                           public nsIRDFObserver
{
private:
    nsIRDFDocument*            mDocument;
    nsIRDFCompositeDataSource* mDB;
    nsIContent*                mRoot;

    // pseudo-constants
    static nsrefcnt gRefCnt;
    static nsIRDFService* gRDFService;

    static nsIAtom* kContentsGeneratedAtom;
    static nsIAtom* kIdAtom;
    static nsIAtom* kOpenAtom;
    static nsIAtom* kResourceAtom;
    static nsIAtom* kTreeAtom;
    static nsIAtom* kTreeBodyAtom;
    static nsIAtom* kTreeCellAtom;
    static nsIAtom* kTreeChildrenAtom;
    static nsIAtom* kTreeColAtom;
    static nsIAtom* kTreeHeadAtom;
    static nsIAtom* kTreeIndentationAtom;
    static nsIAtom* kTreeItemAtom;

    static PRInt32  kNameSpaceID_RDF;
    static PRInt32  kNameSpaceID_XUL;

    static nsIRDFResource* kNC_Title;
    static nsIRDFResource* kNC_child;
    static nsIRDFResource* kNC_Column;
    static nsIRDFResource* kNC_Folder;
    static nsIRDFResource* kRDF_child;

public:
    RDFTreeBuilderImpl();
    virtual ~RDFTreeBuilderImpl();

    // nsISupports interface
    NS_DECL_ISUPPORTS

    // nsIRDFContentModelBuilder interface
    NS_IMETHOD SetDocument(nsIRDFDocument* aDocument);
    NS_IMETHOD SetDataBase(nsIRDFCompositeDataSource* aDataBase);
    NS_IMETHOD GetDataBase(nsIRDFCompositeDataSource** aDataBase);
    NS_IMETHOD CreateRootContent(nsIRDFResource* aResource);
    NS_IMETHOD SetRootContent(nsIContent* aElement);
    NS_IMETHOD CreateContents(nsIContent* aElement);

    // nsIRDFObserver interface
    NS_IMETHOD OnAssert(nsIRDFResource* aSubject, nsIRDFResource* aPredicate, nsIRDFNode* aObject);
    NS_IMETHOD OnUnassert(nsIRDFResource* aSubject, nsIRDFResource* aPredicate, nsIRDFNode* aObjetct);

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
    EnsureCell(nsIContent* aTreeItemElement, PRInt32 aIndex, nsIContent** aCellElement);

    nsresult
    CreateTreeItemCells(nsIContent* aTreeItemElement);

    nsresult
    FindTreeCellForProperty(nsIContent* aTreeRowElement,
                            nsIRDFResource* aProperty,
                            nsIRDFContent** aTreeCell);

    nsresult
    GetColumnForProperty(nsIContent* aTreeElement,
                         nsIRDFResource* aProperty,
                         PRInt32* aIndex);

    nsresult
    SetCellValue(nsIContent* aTreeRowElement,
                 nsIRDFResource* aProperty,
                 nsIRDFNode* aValue);

	PRBool
	IsTreeElement(nsIContent* aElement);

    PRBool
    IsTreeItemElement(nsIContent* aElement);

    PRBool
    IsTreeProperty(nsIRDFResource* aProperty);
};

////////////////////////////////////////////////////////////////////////

nsrefcnt RDFTreeBuilderImpl::gRefCnt = 0;

nsIAtom* RDFTreeBuilderImpl::kContentsGeneratedAtom;
nsIAtom* RDFTreeBuilderImpl::kIdAtom;
nsIAtom* RDFTreeBuilderImpl::kOpenAtom;
nsIAtom* RDFTreeBuilderImpl::kResourceAtom;
nsIAtom* RDFTreeBuilderImpl::kTreeAtom;
nsIAtom* RDFTreeBuilderImpl::kTreeBodyAtom;
nsIAtom* RDFTreeBuilderImpl::kTreeCellAtom;
nsIAtom* RDFTreeBuilderImpl::kTreeChildrenAtom;
nsIAtom* RDFTreeBuilderImpl::kTreeColAtom;
nsIAtom* RDFTreeBuilderImpl::kTreeHeadAtom;
nsIAtom* RDFTreeBuilderImpl::kTreeIndentationAtom;
nsIAtom* RDFTreeBuilderImpl::kTreeItemAtom;

PRInt32  RDFTreeBuilderImpl::kNameSpaceID_RDF;
PRInt32  RDFTreeBuilderImpl::kNameSpaceID_XUL;

nsIRDFService*  RDFTreeBuilderImpl::gRDFService = nsnull;
nsIRDFResource* RDFTreeBuilderImpl::kNC_Title;
nsIRDFResource* RDFTreeBuilderImpl::kNC_child;
nsIRDFResource* RDFTreeBuilderImpl::kNC_Column;
nsIRDFResource* RDFTreeBuilderImpl::kNC_Folder;
nsIRDFResource* RDFTreeBuilderImpl::kRDF_child;

////////////////////////////////////////////////////////////////////////

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
    : mDocument(nsnull),
      mDB(nsnull),
      mRoot(nsnull)
{
    NS_INIT_REFCNT();

    if (gRefCnt == 0) {
        kContentsGeneratedAtom = NS_NewAtom("contentsGenerated");

        kIdAtom              = NS_NewAtom("id");
        kOpenAtom            = NS_NewAtom("open");
        kResourceAtom        = NS_NewAtom("resource");

        kTreeAtom            = NS_NewAtom("tree");
        kTreeBodyAtom        = NS_NewAtom("treebody");
        kTreeCellAtom        = NS_NewAtom("treecell");
        kTreeChildrenAtom    = NS_NewAtom("treechildren");
        kTreeColAtom         = NS_NewAtom("treecol");
        kTreeHeadAtom        = NS_NewAtom("treehead");
        kTreeIndentationAtom = NS_NewAtom("treeindentation");
        kTreeItemAtom        = NS_NewAtom("treeitem");

        nsresult rv;

        // Register the XUL and RDF namespaces: these'll just retrieve
        // the IDs if they've already been registered by someone else.
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


        // Initialize the global shared reference to the service
        // manager and get some shared resource objects.
        rv = nsServiceManager::GetService(kRDFServiceCID,
                                          kIRDFServiceIID,
                                          (nsISupports**) &gRDFService);

        if (NS_FAILED(rv)) {
            NS_ERROR("couldnt' get RDF service");
        }

        NS_VERIFY(NS_SUCCEEDED(gRDFService->GetResource(kURINC_Title,  &kNC_Title)),
                  "couldn't get resource");

        NS_VERIFY(NS_SUCCEEDED(gRDFService->GetResource(kURINC_child,  &kNC_child)),
                  "couldn't get resource");

        NS_VERIFY(NS_SUCCEEDED(gRDFService->GetResource(kURINC_Column, &kNC_Column)),
                  "couldn't get resource");

        NS_VERIFY(NS_SUCCEEDED(gRDFService->GetResource(kURINC_Folder,  &kNC_Folder)),
                  "couldn't get resource");

        NS_VERIFY(NS_SUCCEEDED(gRDFService->GetResource(kURIRDF_child,  &kRDF_child)),
                  "couldn't get resource");
    }

    ++gRefCnt;
}

RDFTreeBuilderImpl::~RDFTreeBuilderImpl(void)
{
    NS_IF_RELEASE(mRoot);
    NS_IF_RELEASE(mDB);
    // NS_IF_RELEASE(mDocument) not refcounted

    --gRefCnt;
    if (gRefCnt == 0) {
        NS_RELEASE(kContentsGeneratedAtom);

        NS_RELEASE(kIdAtom);
        NS_RELEASE(kOpenAtom);
        NS_RELEASE(kResourceAtom);

        NS_RELEASE(kTreeAtom);
        NS_RELEASE(kTreeBodyAtom);
        NS_RELEASE(kTreeCellAtom);
        NS_RELEASE(kTreeChildrenAtom);
        NS_RELEASE(kTreeColAtom);
        NS_RELEASE(kTreeHeadAtom);
        NS_RELEASE(kTreeIndentationAtom);
        NS_RELEASE(kTreeItemAtom);

        NS_RELEASE(kNC_Title);
        NS_RELEASE(kNC_Column);

        nsServiceManager::ReleaseService(kRDFServiceCID, gRDFService);
    }
}

////////////////////////////////////////////////////////////////////////

NS_IMPL_ADDREF(RDFTreeBuilderImpl);
NS_IMPL_RELEASE(RDFTreeBuilderImpl);

NS_IMETHODIMP
RDFTreeBuilderImpl::QueryInterface(REFNSIID iid, void** aResult)
{
    NS_PRECONDITION(aResult != nsnull, "null ptr");
    if (! aResult)
        return NS_ERROR_NULL_POINTER;

    if (iid.Equals(kIRDFContentModelBuilderIID) ||
        iid.Equals(kISupportsIID)) {
        *aResult = NS_STATIC_CAST(nsIRDFContentModelBuilder*, this);
        NS_ADDREF(this);
        return NS_OK;
    }
    else if (iid.Equals(kIRDFObserverIID)) {
        *aResult = NS_STATIC_CAST(nsIRDFObserver*, this);
        NS_ADDREF(this);
        return NS_OK;
    }
    else {
        *aResult = nsnull;
        return NS_NOINTERFACE;
    }
}

////////////////////////////////////////////////////////////////////////
// nsIRDFContentModelBuilder methods

NS_IMETHODIMP
RDFTreeBuilderImpl::SetDocument(nsIRDFDocument* aDocument)
{
    NS_PRECONDITION(aDocument != nsnull, "null ptr");
    if (! aDocument)
        return NS_ERROR_NULL_POINTER;

    NS_PRECONDITION(mDocument == nsnull, "already initialized");
    if (mDocument)
        return NS_ERROR_ALREADY_INITIALIZED;

    mDocument = aDocument; // not refcounted
    return NS_OK;
}


NS_IMETHODIMP
RDFTreeBuilderImpl::SetDataBase(nsIRDFCompositeDataSource* aDataBase)
{
    NS_PRECONDITION(aDataBase != nsnull, "null ptr");
    if (! aDataBase)
        return NS_ERROR_NULL_POINTER;

    NS_PRECONDITION(mDB == nsnull, "already initialized");
    if (mDB)
        return NS_ERROR_ALREADY_INITIALIZED;

    mDB = aDataBase;
    NS_ADDREF(mDB);
    return NS_OK;
}


NS_IMETHODIMP
RDFTreeBuilderImpl::GetDataBase(nsIRDFCompositeDataSource** aDataBase)
{
    NS_PRECONDITION(aDataBase != nsnull, "null ptr");
    if (! aDataBase)
        return NS_ERROR_NULL_POINTER;

    *aDataBase = mDB;
    NS_ADDREF(mDB);
    return NS_OK;
}


NS_IMETHODIMP
RDFTreeBuilderImpl::CreateRootContent(nsIRDFResource* aResource)
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
    if ((tag = dont_AddRef(NS_NewAtom("document"))) == nsnull)
        return NS_ERROR_OUT_OF_MEMORY;

    nsCOMPtr<nsIContent> root;
    if (NS_FAILED(rv = NS_NewRDFGenericElement(getter_AddRefs(root),
                                               kNameSpaceID_HTML, /* XXX */
                                               tag)))
        return rv;

    doc->SetRootContent(NS_STATIC_CAST(nsIContent*, root));

    // Create the BODY element
    nsCOMPtr<nsIContent> body;
    if ((tag = dont_AddRef(NS_NewAtom("body"))) == nsnull)
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
RDFTreeBuilderImpl::SetRootContent(nsIContent* aElement)
{
    NS_IF_RELEASE(mRoot);
    mRoot = aElement;
    NS_IF_ADDREF(mRoot);
    return NS_OK;
}


NS_IMETHODIMP
RDFTreeBuilderImpl::CreateContents(nsIContent* aElement)
{
    nsresult rv;

    {
        // Make sure that we're actually creating content for the tree
        // content model that we've been assigned to deal with.
        nsCOMPtr<nsIContent> treeElement;
        if (NS_FAILED(FindTreeElement(aElement, getter_AddRefs(treeElement))))
            return NS_OK;

        if (treeElement != mRoot)
            return NS_OK;
    }

    // Get the treeitem's resource so that we can generate cell
    // values. We could QI for the nsIRDFResource here, but doing this
    // via the nsIContent interface allows us to support generic nodes
    // that might get added in by DOM calls.
    nsAutoString uri;
    if (NS_FAILED(rv = aElement->GetAttribute(kNameSpaceID_RDF,
                                              kIdAtom,
                                              uri))) {
        NS_ERROR("severe error retrieving attribute");
        return rv;
    }

    if (rv != NS_CONTENT_ATTR_HAS_VALUE) {
        NS_ERROR("tree element has no RDF:ID");
        return NS_ERROR_UNEXPECTED;
    }

    nsCOMPtr<nsIRDFResource> resource;
    if (NS_FAILED(rv = gRDFService->GetUnicodeResource(uri, getter_AddRefs(resource)))) {
        NS_ERROR("unable to create resource");
        return rv;
    }

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
        if (! IsTreeProperty(property))
            continue;

        // Create a second cursor that'll enumerate all of the values
        // for all of the arcs.
        nsCOMPtr<nsIRDFAssertionCursor> assertions;
        if (NS_FAILED(rv = mDB->GetTargets(resource, property, PR_TRUE, getter_AddRefs(assertions)))) {
            NS_ERROR("unable to get targets for property");
            return rv;
        }

        while (NS_SUCCEEDED(rv = assertions->Advance())) {
            nsCOMPtr<nsIRDFNode> value;
            if (NS_FAILED(rv = assertions->GetValue(getter_AddRefs(value)))) {
                NS_ERROR("unable to get cursor value");
                return rv;
            }

            nsCOMPtr<nsIRDFResource> valueResource;
            if (NS_SUCCEEDED(value->QueryInterface(kIRDFResourceIID, (void**) getter_AddRefs(valueResource)))
                && IsTreeProperty(property)) {
                if (NS_FAILED(rv = AddTreeRow(aElement, property, valueResource))) {
                    NS_ERROR("unable to create tree row");
                    return rv;
                }
            }
            else {
                if (NS_FAILED(rv = SetCellValue(aElement, property, value))) {
                    NS_ERROR("unable to set cell value");
                    return rv;
                }
            }
        }
    }

    if (rv == NS_ERROR_RDF_CURSOR_EMPTY)
        // This is a normal return code from nsIRDFCursor::Advance()
        rv = NS_OK;

    return rv;
}


NS_IMETHODIMP
RDFTreeBuilderImpl::OnAssert(nsIRDFResource* aSubject,
                             nsIRDFResource* aPredicate,
                             nsIRDFNode* aObject)
{
    NS_PRECONDITION(mDocument != nsnull, "not initialized");
    if (! mDocument)
        return NS_ERROR_NOT_INITIALIZED;

    nsresult rv;

    nsCOMPtr<nsISupportsArray> elements;
    if (NS_FAILED(rv = NS_NewISupportsArray(getter_AddRefs(elements)))) {
        NS_ERROR("unable to create new ISupportsArray");
        return rv;
    }

    // Find all the elements in the content model that correspond to
    // aSubject: for each, we'll try to build XUL children if
    // appropriate.
    if (NS_FAILED(rv = mDocument->GetElementsForResource(aSubject, elements))) {
        NS_ERROR("unable to retrieve elements from resource");
        return rv;
    }

    for (PRInt32 i = elements->Count() - 1; i >= 0; --i) {
        nsCOMPtr<nsIContent> element(elements->ElementAt(i));
        
        // XXX somehow figure out if building XUL kids on this
        // particular element makes any sense whatsoever.

        // We'll start by making sure that the element at least has
        // the same parent has the content model builder's root
        NS_NOTYETIMPLEMENTED("write me!");
        
        nsCOMPtr<nsIRDFResource> resource;
        if (NS_SUCCEEDED(aObject->QueryInterface(kIRDFResourceIID,
                                                 (void**) getter_AddRefs(resource)))
            && IsTreeProperty(aPredicate)) {
            // Okay, the object _is_ a resource, and the predicate is
            // a tree property. So this'll be a new row in the tree
            // control.

            // But if the contents of aElement _haven't_ yet been
            // generated, then just ignore the assertion. We do this
            // because we know that _eventually_ the contents will be
            // generated (via CreateContents()) when somebody asks for
            // them later.
            nsAutoString contentsGenerated;
            if (NS_FAILED(rv = element->GetAttribute(kNameSpaceID_XUL,
                                                     kContentsGeneratedAtom,
                                                     contentsGenerated))) {
                NS_ERROR("severe problem trying to get attribute");
                return rv;
            }

            if ((rv == NS_CONTENT_ATTR_HAS_VALUE) &&
                contentsGenerated.EqualsIgnoreCase("true")) {
                // Okay, it's a "live" element, so go ahead and append the new
                // child to this node.
                if (NS_FAILED(rv = AddTreeRow(element, aPredicate, resource))) {
                    NS_ERROR("unable to create new tree row");
                    return rv;
                }
            }
        }
        else {
            // Either the object of the assertion is not a resource,
            // or the object is a resource and the predicate is not a
            // tree property. So this won't be a new row in the
            // table. See if we can use it to set a cell value on the
            // current element.
            if (NS_FAILED(rv = SetCellValue(element, aPredicate, aObject))) {
                NS_ERROR("unable to set cell value");
                return rv;
            }
        }
    }
    return NS_OK;
}


NS_IMETHODIMP
RDFTreeBuilderImpl::OnUnassert(nsIRDFResource* aSubject,
                               nsIRDFResource* aPredicate,
                               nsIRDFNode* aObject)
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

        if (NS_FAILED(rv = kid->GetAttribute(kNameSpaceID_RDF, kIdAtom, uri))) {
            NS_ERROR("severe error retrieving attribute");
            return rv; // XXX fatal
        }

        if (rv != NS_CONTENT_ATTR_HAS_VALUE)
            continue;

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
    if (NS_FAILED(rv = CreateTreeItemCells(treeItem)))
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

    //NS_ERROR("must not've started from within a xul:tree");
    return NS_ERROR_FAILURE;
}


nsresult
RDFTreeBuilderImpl::EnsureCell(nsIContent* aTreeItemElement,
                               PRInt32 aIndex,
                               nsIContent** aCellElement)
{
    // This method returns that the aIndex-th <xul:treecell> element
    // if it is already present, and if not, will create up to aIndex
    // nodes to create it.
    NS_PRECONDITION(aIndex >= 0, "invalid arg");
    if (aIndex < 0)
        return NS_ERROR_INVALID_ARG;

    nsresult rv;

    // XXX at this point, we should probably ensure that aElement is
    // actually a <xul:treeitem>...


    // Iterate through the children of the <xul:treeitem>, counting
    // <xul:treecell> tags until we get to the aIndex-th one.
    PRInt32 count;
    if (NS_FAILED(rv = aTreeItemElement->ChildCount(count))) {
        NS_ERROR("unable to get xul:treeitem's child count");
        return rv;
    }

    for (PRInt32 i = 0; i < count; ++i) {
        nsCOMPtr<nsIContent> kid;
        if (NS_FAILED(rv = aTreeItemElement->ChildAt(i, *getter_AddRefs(kid)))) {
            NS_ERROR("unable to retrieve xul:treeitem's child");
            return rv;
        }

        PRInt32 nameSpaceID;
        if (NS_FAILED(rv = kid->GetNameSpaceID(nameSpaceID))) {
            NS_ERROR("unable to get child namespace");
            return rv;
        }

        if (nameSpaceID != kNameSpaceID_XUL)
            continue; // not <xul:*>

        nsCOMPtr<nsIAtom> tag;
        if (NS_FAILED(rv = kid->GetTag(*getter_AddRefs(tag)))) {
            NS_ERROR("unable to get child tag");
            return rv;
        }

        if (tag != kTreeCellAtom)
            continue; // not <xul:treecell>

        // Okay, it's a xul:treecell; see if it's the right one...
        if (aIndex == 0) {
            *aCellElement = kid;
            NS_ADDREF(*aCellElement);
            return NS_OK;
        }

        // Nope, decrement the counter and move on...
        --aIndex;
    }

    // Create all of the xul:treecell elements up to and including the
    // index of the cell that was asked for.
    NS_ASSERTION(aIndex >= 0, "uh oh, I thought aIndex was s'posed t' be >= 0...");

    nsCOMPtr<nsIContent> cellElement;
    while (aIndex-- >= 0) {
        if (NS_FAILED(rv = NS_NewRDFGenericElement(getter_AddRefs(cellElement),
                                                   kNameSpaceID_XUL,
                                                   kTreeCellAtom))) {
            NS_ERROR("unable to create new xul:treecell");
            return rv;
        }

        if (NS_FAILED(rv = aTreeItemElement->AppendChildTo(cellElement, PR_FALSE))) {
            NS_ERROR("unable to append xul:treecell to treeitem");
            return rv;
        }
    }

    *aCellElement = cellElement;
    NS_ADDREF(*aCellElement);
    return NS_OK;
}

nsresult
RDFTreeBuilderImpl::CreateTreeItemCells(nsIContent* aTreeItemElement)
{
    // <xul:treeitem>
    //   <xul:treecell RDF:ID="property">value</xul:treecell>
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

    if (rv != NS_CONTENT_ATTR_HAS_VALUE) {
        NS_ERROR("xul:treeitem has no RDF:ID");
        return NS_ERROR_UNEXPECTED;
    }

    nsCOMPtr<nsIRDFResource> treeItemResource;
    if (NS_FAILED(rv = gRDFService->GetUnicodeResource(treeItemResourceURI,
                                                       getter_AddRefs(treeItemResource))))
        return rv;

    // Now walk up to the primordial <xul:tree> tag so that we can
    // iterate through all of the tree's columns.
    nsCOMPtr<nsIContent> treeElement;
    if (NS_FAILED(rv = FindTreeElement(aTreeItemElement,
                                       getter_AddRefs(treeElement)))) {
        NS_ERROR("unable to find xul:tree element");
        return rv;
    }

    PRInt32 count;
    if (NS_FAILED(rv = treeElement->ChildCount(count))) {
        NS_ERROR("unable to count xul:tree element's kids");
        return rv;
    }

    // Iterate through all the columns that have been specified,
    // constructing a cell in the content model for each one.
    PRInt32 cellIndex = 0;
    for (PRInt32 i = 0; i < count; ++i) {
        nsCOMPtr<nsIContent> kid;
        if (NS_FAILED(rv = treeElement->ChildAt(i, *getter_AddRefs(kid)))) {
            NS_ERROR("unable to get xul:tree's child");
            return rv;
        }

        PRInt32 nameSpaceID;
        if (NS_FAILED(rv = kid->GetNameSpaceID(nameSpaceID))) {
            NS_ERROR("unable to get child's namespace");
            return rv;
        }

        if (nameSpaceID != kNameSpaceID_XUL)
            continue; // not <xul:*>

        nsCOMPtr<nsIAtom> tag;
        if (NS_FAILED(rv = kid->GetTag(*getter_AddRefs(tag)))) {
            NS_ERROR("unable to get child's tag");
            return rv;
        }

        if (tag != kTreeColAtom)
            continue; // not <xul:treecol>

        // Okay, we've found a column. Ensure that we've got a real
        // tree cell that lives beneath _this_ tree item for its
        // value.
        nsCOMPtr<nsIContent> cellElement;
        if (NS_FAILED(rv = EnsureCell(aTreeItemElement, cellIndex, getter_AddRefs(cellElement)))) {
            NS_ERROR("unable to find/create cell element");
            return rv;
        }

        // The first cell gets a <xul:treeindentation> element.
        //
        // XXX This is bogus: dogfood ready crap. We need to figure
        // out a better way to specify this.
        if (cellIndex == 0) {
            nsCOMPtr<nsIContent> indentationElement;
            if (NS_FAILED(rv = NS_NewRDFGenericElement(getter_AddRefs(indentationElement),
                                                       kNameSpaceID_XUL,
                                                       kTreeIndentationAtom))) {
                NS_ERROR("unable to create indentation node");
                return rv;
            }

            if (NS_FAILED(rv = cellElement->AppendChildTo(indentationElement, PR_FALSE))) {
                NS_ERROR("unable to append indentation element");
                return rv;
            }
        }

        // The column property is stored in the RDF:resource attribute
        // of the tag.
        nsAutoString uri;
        if (NS_FAILED(rv = kid->GetAttribute(kNameSpaceID_RDF, kResourceAtom, uri))) {
            NS_ERROR("severe error occured retrieving attribute");
            return rv;
        }

        // Set its value, if we know it.
        if (rv == NS_CONTENT_ATTR_HAS_VALUE) {

            // First construct a property resource from the URI...
            nsCOMPtr<nsIRDFResource> property;
            if (NS_FAILED(gRDFService->GetUnicodeResource(uri, getter_AddRefs(property)))) {
                NS_ERROR("unable to construct resource for xul:treecell");
                return rv; // XXX fatal
            }

            // ...then query the RDF back-end
            nsCOMPtr<nsIRDFNode> value;
            if (NS_SUCCEEDED(rv = mDB->GetTarget(treeItemResource,
                                                 property,
                                                 PR_TRUE,
                                                 getter_AddRefs(value)))) {

                // Attach a plain old text node: nothing fancy. Here's
                // where we'd do wacky stuff like pull in an icon or
                // whatever.
                if (NS_FAILED(rv = nsRDFContentUtils::AttachTextNode(cellElement, value))) {
                    NS_ERROR("unable to attach text node to xul:treecell");
                    return rv;
                }
            }
            else if (rv == NS_ERROR_RDF_NO_VALUE) {
                // otherwise, there was no value for this. It'll have to
                // get set later, when an OnAssert() comes in (if it even
                // has a value at all...)
            }
            else {
                NS_ERROR("error getting cell's value");
                return rv; // XXX something serious happened
            }
        }

        ++cellIndex;
    }

    return NS_OK;
}


nsresult
RDFTreeBuilderImpl::GetColumnForProperty(nsIContent* aTreeElement,
                                         nsIRDFResource* aProperty,
                                         PRInt32* aIndex)
{
    nsresult rv;

    const char* propertyURI;
    if (NS_FAILED(rv = aProperty->GetValue(&propertyURI))) {
        NS_ERROR("unable to get property's URI");
        return rv;
    }

    // XXX should ensure that aTreeElement really is a xul:tree
    
    PRInt32 count;
    if (NS_FAILED(rv = aTreeElement->ChildCount(count))) {
        NS_ERROR("unable to count xul:tree element's kids");
        return rv;
    }

    // Iterate through the columns to find the one that's appropriate
    // for this cell.
    PRInt32 index = 0;
    for (PRInt32 i = 0; i < count; ++i) {
        nsCOMPtr<nsIContent> kid;
        if (NS_FAILED(rv = aTreeElement->ChildAt(i, *getter_AddRefs(kid)))) {
            NS_ERROR("unable to get xul:tree's child");
            return rv;
        }

        PRInt32 nameSpaceID;
        if (NS_FAILED(rv = kid->GetNameSpaceID(nameSpaceID))) {
            NS_ERROR("unable to get child's namespace");
            return rv;
        }

        if (nameSpaceID != kNameSpaceID_XUL)
            continue; // not <xul:*>

        nsCOMPtr<nsIAtom> tag;
        if (NS_FAILED(rv = kid->GetTag(*getter_AddRefs(tag)))) {
            NS_ERROR("unable to get child's tag");
            return rv;
        }

        if (tag != kTreeColAtom)
            continue; // not <xul:treecol>

        // Okay, we've found a column. Is it the right one?  The
        // column property is stored in the RDF:resource attribute of
        // the tag....
        nsAutoString uri;
        if (NS_FAILED(rv = kid->GetAttribute(kNameSpaceID_RDF, kResourceAtom, uri))) {
            NS_ERROR("severe error occured retrieving attribute");
            return rv;
        }

        if (rv == NS_CONTENT_ATTR_HAS_VALUE) {
            if (0 == nsCRT::strcmp(uri, propertyURI)) {
                *aIndex == index;
                return NS_OK;
            }
        }

        ++index;
    }

    // Nope, couldn't find it.
    return NS_ERROR_FAILURE;
}

nsresult
RDFTreeBuilderImpl::SetCellValue(nsIContent* aTreeItemElement,
                                 nsIRDFResource* aProperty,
                                 nsIRDFNode* aValue)
{
    nsresult rv;

    // XXX We assume that aTreeItemElement is actually a
    // <xul:treeitem>, it'd be good to enforce this...

    // First, walk up to the tree's root so we can enumerate the
    // columns & figure out where to put this...

    nsCOMPtr<nsIContent> treeElement;
    if (NS_FAILED(rv = FindTreeElement(aTreeItemElement,
                                       getter_AddRefs(treeElement)))) {
        NS_ERROR("unable to find xul:tree element");
        return rv;
    }

    PRInt32 index;
    if (NS_FAILED(rv = GetColumnForProperty(treeElement, aProperty, &index))) {
        // If we can't find a column for the specified property, that
        // just means there isn't a column in the tree for that
        // property. No big deal. Bye!
        return NS_OK;
    }

    nsCOMPtr<nsIContent> cellElement;
    if (NS_FAILED(rv = EnsureCell(aTreeItemElement, index, getter_AddRefs(cellElement)))) {
        NS_ERROR("unable to find/create cell element");
        return rv;
    }

    // XXX if the cell already has a value, we need to replace it, not
    // just append new text...

    if (NS_FAILED(rv = nsRDFContentUtils::AttachTextNode(cellElement, aValue)))
        return rv;

    return NS_OK;
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
RDFTreeBuilderImpl::IsTreeProperty(nsIRDFResource* aProperty)
{
    // XXX This whole method is a mega-kludge. This should be read off
    // of the element somehow...

#define TREE_PROPERTY_HACK
#if defined(TREE_PROPERTY_HACK)
    if ((aProperty == kNC_child) ||
        (aProperty == kNC_Folder) ||
        (aProperty == kRDF_child)) {
        return PR_TRUE;
    }
    else
#endif // defined(TREE_PROPERTY_HACK)
    if (rdf_IsOrdinalProperty(aProperty)) {
        return PR_TRUE;
    }
    else {
        return PR_FALSE;
    }
}
