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

  4) Can we do sorting as an insertion sort? This is especially
     important in the case where content streams in; e.g., gets added
     in the OnAssert() method as opposed to the CreateContents()
     method.

  5) There is a fair amount of shared code (~5%) between
     nsRDFTreeBuilder and nsRDFXULBuilder. It might make sense to make
     a base class nsGenericBuilder that holds common routines.

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
#include "nsLayoutCID.h"
#include "nsRDFCID.h"
#include "nsRDFContentUtils.h"
#include "nsString.h"
#include "rdf.h"
#include "rdfutil.h"

#include "nsVoidArray.h"
#include "rdf_qsort.h"

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

class RDFTreeBuilderImpl : public nsIRDFContentModelBuilder,
                           public nsIRDFObserver,
                           public nsIDOMNodeObserver,
                           public nsIDOMElementObserver
{
private:
    nsIRDFDocument*            mDocument;
    nsIRDFCompositeDataSource* mDB;
    nsIContent*                mRoot;

    // pseudo-constants
    static nsrefcnt gRefCnt;
    static nsIRDFService*       gRDFService;
    static nsINameSpaceManager* gNameSpaceManager;

    static nsIAtom* kContainerAtom;
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
    static nsIAtom* kTreeIconAtom;
    static nsIAtom* kTreeIndentationAtom;
    static nsIAtom* kTreeItemAtom;
    static nsIAtom* kContainmentAtom;

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

    // nsIDOMNodeObserver interface
    NS_DECL_IDOMNODEOBSERVER

    // nsIDOMElementObserver interface
    NS_DECL_IDOMELEMENTOBSERVER

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
                            nsIContent** aTreeCell);

    nsresult
    GetColumnForProperty(nsIContent* aTreeElement,
                         nsIRDFResource* aProperty,
                         PRInt32* aIndex);

    nsresult
    SetCellValue(nsIContent* aTreeRowElement,
                 nsIRDFResource* aProperty,
                 nsIRDFNode* aValue);

	PRBool
	IsTreeBodyElement(nsIContent* aElement);

    PRBool
    IsTreeItemElement(nsIContent* aElement);

    PRBool
    IsTreeProperty(nsIContent* aElement, nsIRDFResource* aProperty);

    PRBool
    IsOpen(nsIContent* aElement);

    PRBool
    IsElementInTree(nsIContent* aElement);

    nsresult
    GetDOMNodeResource(nsIDOMNode* aNode, nsIRDFResource** aResource);

    nsresult
    CreateResourceElement(PRInt32 aNameSpaceID,
                          nsIAtom* aTag,
                          nsIRDFResource* aResource,
                          nsIContent** aResult);

    nsresult
    GetResource(PRInt32 aNameSpaceID,
                nsIAtom* aNameAtom,
                nsIRDFResource** aResource);
};

////////////////////////////////////////////////////////////////////////

nsrefcnt RDFTreeBuilderImpl::gRefCnt = 0;

nsIAtom* RDFTreeBuilderImpl::kContainerAtom;
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
nsIAtom* RDFTreeBuilderImpl::kTreeIconAtom;
nsIAtom* RDFTreeBuilderImpl::kTreeIndentationAtom;
nsIAtom* RDFTreeBuilderImpl::kTreeItemAtom;
nsIAtom* RDFTreeBuilderImpl::kContainmentAtom;

PRInt32  RDFTreeBuilderImpl::kNameSpaceID_RDF;
PRInt32  RDFTreeBuilderImpl::kNameSpaceID_XUL;

nsIRDFService*  RDFTreeBuilderImpl::gRDFService;
nsINameSpaceManager* RDFTreeBuilderImpl::gNameSpaceManager;
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
        kContainerAtom         = NS_NewAtom("container");
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
        kTreeIconAtom        = NS_NewAtom("treeicon");
        kTreeIndentationAtom = NS_NewAtom("treeindentation");
        kTreeItemAtom        = NS_NewAtom("treeitem");
        kContainmentAtom     = NS_NewAtom("containment");

        nsresult rv;

        // Register the XUL and RDF namespaces: these'll just retrieve
        // the IDs if they've already been registered by someone else.
        if (NS_SUCCEEDED(rv = nsRepository::CreateInstance(kNameSpaceManagerCID,
                                                           nsnull,
                                                           kINameSpaceManagerIID,
                                                           (void**) &gNameSpaceManager))) {

// XXX This is sure to change. Copied from mozilla/layout/xul/content/src/nsXULAtoms.cpp
static const char kXULNameSpaceURI[]
    = "http://www.mozilla.org/keymaster/gatekeeper/there.is.only.xul";

static const char kRDFNameSpaceURI[]
    = RDF_NAMESPACE_URI;

            rv = gNameSpaceManager->RegisterNameSpace(kXULNameSpaceURI, kNameSpaceID_XUL);
            NS_ASSERTION(NS_SUCCEEDED(rv), "unable to register XUL namespace");

            rv = gNameSpaceManager->RegisterNameSpace(kRDFNameSpaceURI, kNameSpaceID_RDF);
            NS_ASSERTION(NS_SUCCEEDED(rv), "unable to register RDF namespace");
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
        NS_RELEASE(kContainerAtom);
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
        NS_RELEASE(kTreeIconAtom);
        NS_RELEASE(kTreeIndentationAtom);
        NS_RELEASE(kTreeItemAtom);
        NS_RELEASE(kContainmentAtom);

        NS_RELEASE(kNC_Title);
        NS_RELEASE(kNC_Column);

        nsServiceManager::ReleaseService(kRDFServiceCID, gRDFService);
        NS_RELEASE(gNameSpaceManager);
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
    }
    else if (iid.Equals(kIRDFObserverIID)) {
        *aResult = NS_STATIC_CAST(nsIRDFObserver*, this);
    }
    else if (iid.Equals(nsIDOMNodeObserver::IID())) {
        *aResult = NS_STATIC_CAST(nsIDOMNodeObserver*, this);
    }
    else if (iid.Equals(nsIDOMElementObserver::IID())) {
        *aResult = NS_STATIC_CAST(nsIDOMElementObserver*, this);
    }
    else {
        *aResult = nsnull;
        return NS_NOINTERFACE;
    }
    NS_ADDREF(this);
    return NS_OK;
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
    if (NS_FAILED(rv = NS_NewRDFElement(kNameSpaceID_HTML, /* XXX */
                                        tag,
                                        getter_AddRefs(root))))
        return rv;

    doc->SetRootContent(NS_STATIC_CAST(nsIContent*, root));

    // Create the BODY element
    nsCOMPtr<nsIContent> body;
    if ((tag = dont_AddRef(NS_NewAtom("body"))) == nsnull)
        return NS_ERROR_OUT_OF_MEMORY;

    if (NS_FAILED(rv = NS_NewRDFElement(kNameSpaceID_HTML, /* XXX */
                                        tag,
                                        getter_AddRefs(body))))
        return rv;


    // Attach the BODY to the DOCUMENT
    if (NS_FAILED(rv = root->AppendChildTo(body, PR_FALSE)))
        return rv;

    // Create the xul:tree element, and indicate that children should
    // be recursively generated on demand
    nsCOMPtr<nsIContent> tree;
    if (NS_FAILED(rv = CreateResourceElement(kNameSpaceID_XUL,
                                             kTreeAtom,
                                             aResource,
                                             getter_AddRefs(tree))))
        return rv;

    if (NS_FAILED(rv = tree->SetAttribute(kNameSpaceID_RDF, kContainerAtom, "true", PR_FALSE)))
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


typedef	struct	_sortStruct	{
    nsIRDFCompositeDataSource	*db;
    nsIRDFResource		*sortProperty;
    PRBool			descendingSort;
} sortStruct, *sortPtr;


int rdfSortCallback(const void *data1, const void *data2, void *data);


int
rdfSortCallback(const void *data1, const void *data2, void *sortData)
{
	int		sortOrder = 0;
	nsresult	rv;

	nsIRDFNode	*node1, *node2;
	node1 = *(nsIRDFNode **)data1;
	node2 = *(nsIRDFNode **)data2;
	_sortStruct	*sortPtr = (_sortStruct *)sortData;

	nsIRDFResource	*res1;
	nsIRDFResource	*res2;
	const PRUnichar	*uniStr1 = nsnull;
	const PRUnichar	*uniStr2 = nsnull;

	if (NS_SUCCEEDED(node1->QueryInterface(kIRDFResourceIID, (void **) &res1)))
	{
		nsIRDFNode	*nodeVal1;
		if (NS_SUCCEEDED(rv = sortPtr->db->GetTarget(res1, sortPtr->sortProperty, PR_TRUE, &nodeVal1)))
		{
			nsIRDFLiteral *literal1;
			if (NS_SUCCEEDED(nodeVal1->QueryInterface(kIRDFLiteralIID, (void **) &literal1)))
			{
				literal1->GetValue(&uniStr1);
			}
		}
	}
	if (NS_SUCCEEDED(node2->QueryInterface(kIRDFResourceIID, (void **) &res2)))
	{
		nsIRDFNode	*nodeVal2;
		if (NS_SUCCEEDED(rv = sortPtr->db->GetTarget(res2, sortPtr->sortProperty, PR_TRUE, &nodeVal2)))
		{
			nsIRDFLiteral	*literal2;
			if (NS_SUCCEEDED(nodeVal2->QueryInterface(kIRDFLiteralIID, (void **) &literal2)))
			{
				literal2->GetValue(&uniStr2);
			}
		}
	}
	if ((uniStr1 != nsnull) && (uniStr2 != nsnull))
	{
		nsAutoString	str1(uniStr1), str2(uniStr2);
		sortOrder = (int)str1.Compare(str2, PR_TRUE);
		if (sortPtr->descendingSort == PR_TRUE)
		{
			sortOrder = -sortOrder;
		}
	}
	else if ((uniStr1 != nsnull) && (uniStr2 == nsnull))
	{
		sortOrder = -1;
	}
	else
	{
		sortOrder = 1;
	}
	return(sortOrder);
}


NS_IMETHODIMP
RDFTreeBuilderImpl::CreateContents(nsIContent* aElement)
{
    // First, make sure that the element is in the right tree -- ours.
    if (! IsElementInTree(aElement))
        return NS_OK;

    // Next, see if the treeitem is even "open". If not, then just
    // pretend it doesn't have _any_ contents.
    if (! IsOpen(aElement))
        return NS_OK;

    // Get the treeitem's resource so that we can generate cell
    // values. We could QI for the nsIRDFResource here, but doing this
    // via the nsIContent interface allows us to support generic nodes
    // that might get added in by DOM calls.
    nsresult rv;
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

// rjc - sort
	nsVoidArray	*tempArray;
        if ((tempArray = new nsVoidArray()) == nsnull)	return (NS_ERROR_OUT_OF_MEMORY);

    while (NS_SUCCEEDED(rv = properties->Advance())) {
        nsCOMPtr<nsIRDFResource> property;

        if (NS_FAILED(rv = properties->GetPredicate(getter_AddRefs(property))))
            break;

        // If it's not a tree property, then it doesn't specify an
        // object that is member of the current container element;
        // rather, it specifies a "simple" property of the container
        // element. Skip it.
        if (! IsTreeProperty(aElement, property))
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
                // return rv;
                break;
            }

            nsCOMPtr<nsIRDFResource> valueResource;
            if (NS_SUCCEEDED(value->QueryInterface(kIRDFResourceIID, (void**) getter_AddRefs(valueResource)))
                && IsTreeProperty(aElement, property)) {
			/* XXX hack: always append value resource 1st!
			       due to sort callback implementation */
                	tempArray->AppendElement(valueResource);
                	tempArray->AppendElement(property);
/*                if (NS_FAILED(rv = AddTreeRow(aElement, property, valueResource))) {
                    NS_ERROR("unable to create tree row");
                    return rv;
                }
*/
            }
            else {
                if (NS_FAILED(rv = SetCellValue(aElement, property, value))) {
                    NS_ERROR("unable to set cell value");
                    // return rv;
                    break;
                }
            }
        }
    }

        unsigned long numElements = tempArray->Count();
        if (numElements > 0)
        {
        	nsIRDFResource ** flatArray = (nsIRDFResource **)malloc(numElements * sizeof(void *));
        	if (flatArray)
        	{
			_sortStruct		sortInfo;

			// get sorting info (property to sort on, direction to sort, etc)

			// XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX
			// XXX Note: currently hardcoded; should get from DOM, or... ?
			// XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX

			sortInfo.db = mDB;
			sortInfo.descendingSort = PR_FALSE;
			if (NS_FAILED(rv = gRDFService->GetResource("http://home.netscape.com/NC-rdf#Name", &sortInfo.sortProperty)))
			{
				NS_ERROR("unable to create gSortProperty resource");
				return rv;
			}

			// flatten array of resources, sort them, then add as tree elements
			unsigned long loop;

        	        for (loop=0; loop<numElements; loop++)
				flatArray[loop] = (nsIRDFResource *)tempArray->ElementAt(loop);
        		rdf_qsort((void *)flatArray, numElements/2, 2 * sizeof(void *), rdfSortCallback, (void *)&sortInfo);
        		for (loop=0; loop<numElements; loop++)
        		{
				if (NS_FAILED(rv = AddTreeRow(aElement,
							(nsIRDFResource *)flatArray[loop+1],
							(nsIRDFResource *)flatArray[loop])))
				{
					NS_ERROR("unable to create tree row");
				}
        		}
        		free(flatArray);
        	}
        }
        delete tempArray;

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
        if (! IsElementInTree(element))
            continue;
        
        nsCOMPtr<nsIRDFResource> resource;
        if (NS_SUCCEEDED(aObject->QueryInterface(kIRDFResourceIID,
                                                 (void**) getter_AddRefs(resource)))
            && IsTreeProperty(element, aPredicate)) {
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
// nsIDOMNodeObserver interface

NS_IMETHODIMP
RDFTreeBuilderImpl::OnSetNodeValue(nsIDOMNode* aNode, const nsString& aValue)
{
    NS_NOTYETIMPLEMENTED("write me!");
    return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
RDFTreeBuilderImpl::OnInsertBefore(nsIDOMNode* aParent, nsIDOMNode* aNewChild, nsIDOMNode* aRefChild)
{
    return NS_OK;
}



NS_IMETHODIMP
RDFTreeBuilderImpl::OnReplaceChild(nsIDOMNode* aParent, nsIDOMNode* aNewChild, nsIDOMNode* aOldChild)
{
    return NS_OK;
}



NS_IMETHODIMP
RDFTreeBuilderImpl::OnRemoveChild(nsIDOMNode* aParent, nsIDOMNode* aOldChild)
{
    return NS_OK;
}



NS_IMETHODIMP
RDFTreeBuilderImpl::OnAppendChild(nsIDOMNode* aParent, nsIDOMNode* aNewChild)
{
    return NS_OK;
}


////////////////////////////////////////////////////////////////////////
// nsIDOMElementObserver interface


NS_IMETHODIMP
RDFTreeBuilderImpl::OnSetAttribute(nsIDOMElement* aElement, const nsString& aName, const nsString& aValue)
{
    nsresult rv;

    nsCOMPtr<nsIRDFResource> resource;
    if (NS_FAILED(rv = GetDOMNodeResource(aElement, getter_AddRefs(resource)))) {
        // XXX it's not a resource element, so there's no assertions
        // we need to make on the back-end. Should we just do the
        // update?
        return NS_OK;
    }

    // Get the nsIContent interface, it's a bit more utilitarian
    nsCOMPtr<nsIContent> element( do_QueryInterface(aElement) );
    if (! element) {
        NS_ERROR("element doesn't support nsIContent");
        return NS_ERROR_UNEXPECTED;
    }

    // Split the property name into its namespace and tag components
    PRInt32  nameSpaceID;
    nsCOMPtr<nsIAtom> nameAtom;
    if (NS_FAILED(rv = element->ParseAttributeString(aName, *getter_AddRefs(nameAtom), nameSpaceID))) {
        NS_ERROR("unable to parse attribute string");
        return rv;
    }

    nsCOMPtr<nsIRDFResource> property;
    if (NS_FAILED(rv = GetResource(nameSpaceID, nameAtom, getter_AddRefs(property)))) {
        NS_ERROR("unable to construct resource");
        return rv;
    }

    // Unassert the old value, if there was one.
    nsAutoString oldValue;
    if (NS_CONTENT_ATTR_HAS_VALUE == element->GetAttribute(nameSpaceID, nameAtom, oldValue)) {
        nsCOMPtr<nsIRDFLiteral> value;
        if (NS_FAILED(rv = gRDFService->GetLiteral(oldValue, getter_AddRefs(value)))) {
            NS_ERROR("unable to construct literal");
            return rv;
        }

        rv = mDB->Unassert(resource, property, value);
        NS_ASSERTION(NS_SUCCEEDED(rv), "unable to unassert old property value");
    }

    // Assert the new value
    {
        nsCOMPtr<nsIRDFLiteral> value;
        if (NS_FAILED(rv = gRDFService->GetLiteral(aValue, getter_AddRefs(value)))) {
            NS_ERROR("unable to construct literal");
            return rv;
        }

        if (NS_FAILED(rv = mDB->Assert(resource, property, value, PR_TRUE))) {
            NS_ERROR("unable to assert new property value");
            return rv;
        }
    }
    

    return NS_OK;
}

NS_IMETHODIMP
RDFTreeBuilderImpl::OnRemoveAttribute(nsIDOMElement* aElement, const nsString& aName)
{
    nsresult rv;

    nsCOMPtr<nsIRDFResource> resource;
    if (NS_FAILED(rv = GetDOMNodeResource(aElement, getter_AddRefs(resource)))) {
        // XXX it's not a resource element, so there's no assertions
        // we need to make on the back-end. Should we just do the
        // update?
        return NS_OK;
    }

    // Get the nsIContent interface, it's a bit more utilitarian
    nsCOMPtr<nsIContent> element( do_QueryInterface(aElement) );
    if (! element) {
        NS_ERROR("element doesn't support nsIContent");
        return NS_ERROR_UNEXPECTED;
    }

    // Split the property name into its namespace and tag components
    PRInt32  nameSpaceID;
    nsCOMPtr<nsIAtom> nameAtom;
    if (NS_FAILED(rv = element->ParseAttributeString(aName, *getter_AddRefs(nameAtom), nameSpaceID))) {
        NS_ERROR("unable to parse attribute string");
        return rv;
    }

    nsCOMPtr<nsIRDFResource> property;
    if (NS_FAILED(rv = GetResource(nameSpaceID, nameAtom, getter_AddRefs(property)))) {
        NS_ERROR("unable to construct resource");
        return rv;
    }

    // Unassert the old value, if there was one.
    nsAutoString oldValue;
    if (NS_CONTENT_ATTR_HAS_VALUE == element->GetAttribute(nameSpaceID, nameAtom, oldValue)) {
        nsCOMPtr<nsIRDFLiteral> value;
        if (NS_FAILED(rv = gRDFService->GetLiteral(oldValue, getter_AddRefs(value)))) {
            NS_ERROR("unable to construct literal");
            return rv;
        }

        rv = mDB->Unassert(resource, property, value);
        NS_ASSERTION(NS_SUCCEEDED(rv), "unable to unassert old property value");
    }

    return NS_OK;
}

NS_IMETHODIMP
RDFTreeBuilderImpl::OnSetAttributeNode(nsIDOMElement* aElement, nsIDOMAttr* aNewAttr)
{
    NS_NOTYETIMPLEMENTED("write me!");
    return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
RDFTreeBuilderImpl::OnRemoveAttributeNode(nsIDOMElement* aElement, nsIDOMAttr* aOldAttr)
{
    NS_NOTYETIMPLEMENTED("write me!");
    return NS_ERROR_NOT_IMPLEMENTED;
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

        if (kidTag.get() != aTag)
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

        if (tag.get() != aTag)
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

    if (NS_FAILED(rv = NS_NewRDFElement(nameSpaceID, tag, getter_AddRefs(element))))
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
    //
    // We can also handle the case where they've specified RDF
    // contents on the <xul:treebody> tag, in which case we'll just
    // dangle the new row directly off the treebody.

    nsresult rv;

    nsCOMPtr<nsIContent> treeChildren;
    if (IsTreeItemElement(aElement)) {
        // Ensure that the <xul:treechildren> element exists on the parent.
        if (NS_FAILED(rv = EnsureElementHasGenericChild(aElement,
                                                        kNameSpaceID_XUL,
                                                        kTreeChildrenAtom,
                                                        getter_AddRefs(treeChildren))))
            return rv;
    }
    else if (IsTreeBodyElement(aElement)) {
        // We'll just use the <xul:treebody> as the element onto which
        // we'll dangle a new row.
        treeChildren = do_QueryInterface(aElement);
        if (! treeChildren) {
            NS_ERROR("aElement is not nsIContent!?!");
            return NS_ERROR_UNEXPECTED;
        }
    }
    else {
        NS_ERROR("new tree row doesn't fit here!");
        return NS_ERROR_UNEXPECTED;
    }

    // Create the <xul:treeitem> element
    nsCOMPtr<nsIContent> treeItem;
    if (NS_FAILED(rv = CreateResourceElement(kNameSpaceID_XUL,
                                             kTreeItemAtom,
                                             aValue,
                                             getter_AddRefs(treeItem))))
        return rv;

    // Add the <xul:treeitem> to the <xul:treechildren> element.
    treeChildren->AppendChildTo(treeItem, PR_TRUE);

    // Create the cell substructure
    if (NS_FAILED(rv = CreateTreeItemCells(treeItem)))
        return rv;

    // Add miscellaneous attributes by iterating _all_ of the
    // properties out of the resource.
    nsCOMPtr<nsIRDFArcsOutCursor> arcs;
    if (NS_FAILED(rv = mDB->ArcLabelsOut(aValue, getter_AddRefs(arcs)))) {
        NS_ERROR("unable to get arcs out");
        return rv;
    }

    while (NS_SUCCEEDED(rv = arcs->Advance())) {
        nsCOMPtr<nsIRDFResource> property;
        if (NS_FAILED(rv = arcs->GetPredicate(getter_AddRefs(property)))) {
            NS_ERROR("unable to get cursor value");
            return rv;
        }

        // Ignore ordinal properties
        if (rdf_IsOrdinalProperty(property))
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

        nsCOMPtr<nsIRDFResource> resource;
        nsCOMPtr<nsIRDFLiteral> literal;

        nsAutoString s;
        if (NS_SUCCEEDED(rv = value->QueryInterface(kIRDFResourceIID, getter_AddRefs(resource)))) {
            const char* uri;
            resource->GetValue(&uri);
            s = uri;
        }
        else if (NS_SUCCEEDED(rv = value->QueryInterface(kIRDFLiteralIID, getter_AddRefs(literal)))) {
            const PRUnichar* p;
            literal->GetValue(&p);
            s = p;
        }
        else {
            NS_ERROR("not a resource or a literal");
            return NS_ERROR_UNEXPECTED;
        }

        treeItem->SetAttribute(nameSpaceID, tag, s, PR_FALSE);
    }

    if (NS_FAILED(rv) && (rv != NS_ERROR_RDF_CURSOR_EMPTY)) {
        NS_ERROR("error advancing cursor");
        return rv;
    }

    // Finally, mark this as a "container" so that we know to
    // recursively generate kids if they're asked for.
    if (NS_FAILED(rv = treeItem->SetAttribute(kNameSpaceID_RDF, kContainerAtom, "true", PR_FALSE)))
        return rv;

    return NS_OK;
}


nsresult
RDFTreeBuilderImpl::FindTreeElement(nsIContent* aElement,
                                    nsIContent** aTreeElement)
{
    nsresult rv;

    // walk up the tree until you find <xul:tree>
    nsCOMPtr<nsIContent> element(do_QueryInterface(aElement));

    while (element) {
        PRInt32 nameSpaceID;
        if (NS_FAILED(rv = element->GetNameSpaceID(nameSpaceID)))
            return rv; // XXX fatal

        if (nameSpaceID == kNameSpaceID_XUL) {
            nsCOMPtr<nsIAtom> tag;
            if (NS_FAILED(rv = element->GetTag(*getter_AddRefs(tag))))
                return rv;

            if (tag.get() == kTreeAtom) {
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

        if (tag.get() != kTreeCellAtom)
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
        if (NS_FAILED(rv = NS_NewRDFElement(kNameSpaceID_XUL,
                                            kTreeCellAtom,
                                            getter_AddRefs(cellElement)))) {
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

        if (tag.get() != kTreeColAtom)
            continue; // not <xul:treecol>

        // Okay, we've found a column. Ensure that we've got a real
        // tree cell that lives beneath _this_ tree item for its
        // value.
        nsCOMPtr<nsIContent> cellElement;
        if (NS_FAILED(rv = EnsureCell(aTreeItemElement, cellIndex, getter_AddRefs(cellElement)))) {
            NS_ERROR("unable to find/create cell element");
            return rv;
        }

        nsIContent* textParent = cellElement;

        // The first cell gets a <xul:treeindentation> element and a
        // <xul:treeicon> element...
        //
        // XXX This is bogus: dogfood ready crap. We need to figure
        // out a better way to specify this.
        if (cellIndex == 0) {
            nsCOMPtr<nsIContent> indentationElement;
            if (NS_FAILED(rv = NS_NewRDFElement(kNameSpaceID_XUL,
                                                kTreeIndentationAtom,
                                                getter_AddRefs(indentationElement)))) {
                NS_ERROR("unable to create indentation node");
                return rv;
            }

            if (NS_FAILED(rv = cellElement->AppendChildTo(indentationElement, PR_FALSE))) {
                NS_ERROR("unable to append indentation element");
                return rv;
            }

            nsCOMPtr<nsIContent> iconElement;
            if (NS_FAILED(rv = NS_NewRDFElement(kNameSpaceID_XUL,
                                                kTreeIconAtom,
                                                getter_AddRefs(iconElement)))) {
                NS_ERROR("unable to create icon node");
                return rv;
            }

            if (NS_FAILED(rv = cellElement->AppendChildTo(iconElement, PR_FALSE))) {
                NS_ERROR("uanble to append icon element");
                return rv;
            }

            textParent = iconElement;
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
                if (NS_FAILED(rv = nsRDFContentUtils::AttachTextNode(textParent, value))) {
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

        if (tag.get() != kTreeColAtom)
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
                *aIndex = index;
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
RDFTreeBuilderImpl::IsTreeBodyElement(nsIContent* element)
{
    // Returns PR_TRUE if the element is a <xul:treebody> element.
    nsresult rv;

    // "element" must be xul:treebody
    PRInt32 nameSpaceID;
    if (NS_FAILED(rv = element->GetNameSpaceID(nameSpaceID))) {
        NS_ERROR("unable to get namespace ID");
        return PR_FALSE;
    }

    if (nameSpaceID != kNameSpaceID_XUL)
        return PR_FALSE; // not a XUL element

    nsCOMPtr<nsIAtom> elementTag;
    if (NS_FAILED(rv = element->GetTag(*getter_AddRefs(elementTag)))) {
        NS_ERROR("unable to get tag");
        return PR_FALSE;
    }

    if (elementTag.get() != kTreeBodyAtom)
        return PR_FALSE; // not a <xul:treebody>

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

    if (tag.get() != kTreeItemAtom)
        return PR_FALSE;

    return PR_TRUE;
}


PRBool
RDFTreeBuilderImpl::IsTreeProperty(nsIContent* aElement, nsIRDFResource* aProperty)
{
    // XXX is this okay to _always_ treat ordinal properties as tree
    // properties? Probably not...
    if (rdf_IsOrdinalProperty(aProperty))
        return PR_TRUE;

    nsresult rv;
    const char* propertyURI;
    if (NS_FAILED(rv = aProperty->GetValue(&propertyURI))) {
        NS_ERROR("unable to get property URI");
        return PR_FALSE;
    }

    nsAutoString uri;

    nsCOMPtr<nsIContent> element( do_QueryInterface(aElement) );
    while (element) {
        // Never ever ask an HTML element about non-HTML attributes.
        PRInt32 nameSpaceID;
        if (NS_FAILED(rv = element->GetNameSpaceID(nameSpaceID))) {
            NS_ERROR("unable to get element namespace");
            PR_FALSE;
        }

        if (nameSpaceID != kNameSpaceID_HTML) {
            // Is this the "rdf:containment? property?
            if (NS_FAILED(rv = element->GetAttribute(kNameSpaceID_RDF, kContainmentAtom, uri))) {
                NS_ERROR("unable to get attribute value");
                return PR_FALSE;
            }

            if (rv == NS_CONTENT_ATTR_HAS_VALUE) {
                // Okay we've found the locally specified tree properties,
                // a whitespace-separated list of property URIs. So we
                // definitively know whether this is a tree property or
                // not.
                if (uri.Find(propertyURI) >= 0)
                    return PR_TRUE;
                else
                    return PR_FALSE;
            }
        }

        nsCOMPtr<nsIContent> parent;
        element->GetParent(*getter_AddRefs(parent));
        element = parent;
    }

    // If we get here, we didn't find any tree property: so now
    // defaults start to kick in.

    //#define TREE_PROPERTY_HACK
#if defined(TREE_PROPERTY_HACK)
    if ((aProperty == kNC_child) ||
        (aProperty == kNC_Folder) ||
        (aProperty == kRDF_child)) {
        return PR_TRUE;
    }
    else
#endif // defined(TREE_PROPERTY_HACK)

        return PR_FALSE;
}


PRBool
RDFTreeBuilderImpl::IsOpen(nsIContent* aElement)
{
    nsresult rv;

    // needs to be a <xul:treeitem> or <xul:treebody> to begin with.
    PRInt32 nameSpaceID;
    if (NS_FAILED(rv = aElement->GetNameSpaceID(nameSpaceID))) {
        NS_ERROR("unable to get namespace ID");
        return PR_FALSE;
    }

    if (nameSpaceID != kNameSpaceID_XUL)
        return PR_FALSE;

    nsCOMPtr<nsIAtom> tag;
    if (NS_FAILED(rv = aElement->GetTag( *getter_AddRefs(tag) ))) {
        NS_ERROR("unable to get element's tag");
        return PR_FALSE;
    }

    // The <xul:treebody> is _always_ open.
    if (tag.get() == kTreeBodyAtom)
        return PR_TRUE;

    // If it's not a <xul:treeitem>, then it's not open.
    if (tag.get() != kTreeItemAtom)
        return PR_FALSE;

    nsAutoString value;
    if (NS_FAILED(rv = aElement->GetAttribute(kNameSpaceID_XUL, kOpenAtom, value))) {
        NS_ERROR("unable to get xul:open attribute");
        return rv;
    }

    if (rv == NS_CONTENT_ATTR_HAS_VALUE) {
        if (value.EqualsIgnoreCase("true"))
            return PR_TRUE;
    }

    return PR_FALSE;
}


PRBool
RDFTreeBuilderImpl::IsElementInTree(nsIContent* aElement)
{
    // Make sure that we're actually creating content for the tree
    // content model that we've been assigned to deal with.
    nsCOMPtr<nsIContent> treeElement;
    if (NS_FAILED(FindTreeElement(aElement, getter_AddRefs(treeElement))))
        return PR_FALSE;

    if (treeElement.get() != mRoot)
        return PR_FALSE;

    return PR_TRUE;
}

nsresult
RDFTreeBuilderImpl::GetDOMNodeResource(nsIDOMNode* aNode, nsIRDFResource** aResource)
{
    nsresult rv;

    // Given an nsIDOMNode that presumably has been created as a proxy
    // for an RDF resource, pull the RDF resource information out of
    // it.

    nsCOMPtr<nsIContent> element;
    if (NS_FAILED(rv = aNode->QueryInterface(kIContentIID, getter_AddRefs(element) ))) {
        NS_ERROR("DOM element doesn't support nsIContent");
        return rv;
    }

    nsAutoString uri;
    if (NS_FAILED(rv = element->GetAttribute(kNameSpaceID_RDF,
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
    if (NS_FAILED(rv = gRDFService->GetUnicodeResource(uri, aResource))) {
        NS_ERROR("unable to create resource");
        return rv;
    }

    return NS_OK;
}

nsresult
RDFTreeBuilderImpl::CreateResourceElement(PRInt32 aNameSpaceID,
                                          nsIAtom* aTag,
                                          nsIRDFResource* aResource,
                                          nsIContent** aResult)
{
    nsresult rv;

    nsCOMPtr<nsIContent> result;
    if (NS_FAILED(rv = NS_NewRDFElement(aNameSpaceID, aTag, getter_AddRefs(result))))
        return rv;

    const char* uri;
    if (NS_FAILED(rv = aResource->GetValue(&uri)))
        return rv;

    if (NS_FAILED(rv = result->SetAttribute(kNameSpaceID_RDF, kIdAtom, uri, PR_FALSE)))
        return rv;

    *aResult = result;
    NS_ADDREF(*aResult);
    return NS_OK;
}



nsresult
RDFTreeBuilderImpl::GetResource(PRInt32 aNameSpaceID,
                                nsIAtom* aNameAtom,
                                nsIRDFResource** aResource)
{
    NS_PRECONDITION(aNameAtom != nsnull, "null ptr");
    if (! aNameAtom)
        return NS_ERROR_NULL_POINTER;

    // XXX should we allow nodes with no namespace???
    NS_PRECONDITION(aNameSpaceID != kNameSpaceID_Unknown, "no namespace");
    if (aNameSpaceID == kNameSpaceID_Unknown)
        return NS_ERROR_UNEXPECTED;

    // construct a fully-qualified URI from the namespace/tag pair.
    nsAutoString uri;
    gNameSpaceManager->GetNameSpaceURI(aNameSpaceID, uri);

    // XXX check to see if we need to insert a '/' or a '#'
    nsAutoString tag(aNameAtom->GetUnicode());
    if (0 < uri.Length() && uri.Last() != '#' && uri.Last() != '/' && tag.First() != '#')
        uri.Append('#');

    uri.Append(tag);

    nsresult rv = gRDFService->GetUnicodeResource(uri, aResource);
    NS_ASSERTION(NS_SUCCEEDED(rv), "unable to get resource");
    return rv;
}
