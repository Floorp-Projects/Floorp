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

  An RDF content model builder implementation that builds a XUL
  content model from an RDF graph.
  
  TO DO

  1) I need to make sure that the XUL builder only listens to DOM
     modifications that it should care about. Right now, it'll take
     any old DOM update and try to whack it into RDF.

  2) I need to implement nsRDFXULBuilder::RemoveAttribute(), and
     figure out how to do nsRDFXULBuilder::Remove() (vanilla) when the
     child isn't a resource element itself.

  3) There's an assertion that's firing when it tries to ask for
     RDF:ID on an HTML attribute from nsRDFXULBuilder::Remove().

  4) Figure out how to do natural ordering. I was thinking that maybe
     some kind of partial order relationship between nodes might be
     simple to implement and easy to work out. For example,

        [foo]--RDF:greaterThan-->[bar]

     I remember talking to RJC about this months ago, and we came to
     the conclusing that there could get to be nasty cycles; however,
     it seems like a really simple way to start.

  */

#include "nsCOMPtr.h"
#include "nsDebug.h"
#include "nsIAtom.h"
#include "nsIDocument.h"
#include "nsIDOMNode.h"
#include "nsIDOMNodeObserver.h"
#include "nsINameSpaceManager.h"
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

// XXX These are needed as scaffolding until we get to a more
// DOM-based solution.
#include "nsHTMLParts.h"
#include "nsIHTMLContent.h"

////////////////////////////////////////////////////////////////////////

static NS_DEFINE_IID(kIContentIID,                NS_ICONTENT_IID);
static NS_DEFINE_IID(kIDocumentIID,               NS_IDOCUMENT_IID);
static NS_DEFINE_IID(kINameSpaceManagerIID,       NS_INAMESPACEMANAGER_IID);
static NS_DEFINE_IID(kIRDFContentIID,             NS_IRDFCONTENT_IID);
static NS_DEFINE_IID(kIRDFContentModelBuilderIID, NS_IRDFCONTENTMODELBUILDER_IID);
static NS_DEFINE_IID(kIRDFCompositeDataSourceIID, NS_IRDFCOMPOSITEDATASOURCE_IID);
static NS_DEFINE_IID(kIRDFLiteralIID,             NS_IRDFLITERAL_IID);
static NS_DEFINE_IID(kIRDFObserverIID,            NS_IRDFOBSERVER_IID);
static NS_DEFINE_IID(kIRDFResourceIID,            NS_IRDFRESOURCE_IID);
static NS_DEFINE_IID(kIRDFServiceIID,             NS_IRDFSERVICE_IID);
static NS_DEFINE_IID(kISupportsIID,               NS_ISUPPORTS_IID);

static NS_DEFINE_CID(kNameSpaceManagerCID,        NS_NAMESPACEMANAGER_CID);
static NS_DEFINE_CID(kRDFCompositeDataSourceCID,  NS_RDFCOMPOSITEDATASOURCE_CID);
static NS_DEFINE_CID(kRDFServiceCID,              NS_RDFSERVICE_CID);
static NS_DEFINE_CID(kRDFTreeBuilderCID,          NS_RDFTREEBUILDER_CID);

////////////////////////////////////////////////////////////////////////
// standard vocabulary items

static const char kRDFNameSpaceURI[] = RDF_NAMESPACE_URI;
DEFINE_RDF_VOCAB(RDF_NAMESPACE_URI, RDF, instanceOf);
DEFINE_RDF_VOCAB(RDF_NAMESPACE_URI, RDF, nextVal);
DEFINE_RDF_VOCAB(RDF_NAMESPACE_URI, RDF, type);
DEFINE_RDF_VOCAB(RDF_NAMESPACE_URI, RDF, child); // XXX bogus: needs to be NC:child


// XXX This is sure to change. Copied from mozilla/layout/xul/content/src/nsXULAtoms.cpp
#define XUL_NAMESPACE_URI "http://www.mozilla.org/keymaster/gatekeeper/there.is.only.xul"
static const char kXULNameSpaceURI[] = XUL_NAMESPACE_URI;

#define XUL_NAMESPACE_URI_PREFIX XUL_NAMESPACE_URI "#"
DEFINE_RDF_VOCAB(XUL_NAMESPACE_URI_PREFIX, XUL, element);

////////////////////////////////////////////////////////////////////////

class RDFXULBuilderImpl : public nsIRDFContentModelBuilder,
                          public nsIRDFObserver,
                          public nsIDOMNodeObserver
{
private:
    nsIRDFCompositeDataSource* mDB;
    nsIRDFDocument*            mDocument;
    nsIContent*                mRoot;

    // pseudo-constants
    static PRInt32 gRefCnt;
    static nsIRDFService*  gRDFService;

    static PRInt32  kNameSpaceID_RDF;
    static PRInt32  kNameSpaceID_XUL;

    static nsIAtom* kContentsGeneratedAtom;
    static nsIAtom* kDataSourcesAtom;
    static nsIAtom* kIdAtom;
    static nsIAtom* kTreeAtom;

    static nsIRDFResource* kRDF_instanceOf;
    static nsIRDFResource* kRDF_nextVal;
    static nsIRDFResource* kRDF_type;
    static nsIRDFResource* kRDF_child; // XXX needs to become kNC_child
    static nsIRDFResource* kXUL_element;

public:
    RDFXULBuilderImpl();
    virtual ~RDFXULBuilderImpl();

    // nsISupports interface
    NS_DECL_ISUPPORTS

    // nsIRDFContentModelBuilder interface
    NS_IMETHOD SetDocument(nsIRDFDocument* aDocument);
    NS_IMETHOD SetDataBase(nsIRDFCompositeDataSource* aDataBase);
    NS_IMETHOD GetDataBase(nsIRDFCompositeDataSource** aDataBase);
    NS_IMETHOD CreateRootContent(nsIRDFResource* aResource);
    NS_IMETHOD SetRootContent(nsIContent* aResource);
    NS_IMETHOD CreateContents(nsIContent* aElement);

    // nsIRDFObserver interface
    NS_IMETHOD OnAssert(nsIRDFResource* aSubject, nsIRDFResource* aPredicate, nsIRDFNode* aObject);
    NS_IMETHOD OnUnassert(nsIRDFResource* aSubject, nsIRDFResource* aPredicate, nsIRDFNode* aObjetct);

    // nsIDOMNodeObserver interface
    NS_DECL_IDOMNODEOBSERVER

    // Implementation methods
    nsresult AppendChild(nsIContent* aElement,
                         nsIRDFNode* aValue);

    nsresult RemoveChild(nsIContent* aElement,
                         nsIRDFNode* aValue);

    nsresult CreateElement(nsIRDFResource* aResource,
                           nsIContent** aResult);

    nsresult CreateHTMLElement(nsIRDFResource* aResource,
                               nsIAtom* aTag,
                               nsIContent** aResult);

    nsresult CreateHTMLContents(nsIContent* aElement,
                                nsIRDFResource* aResource);

    nsresult CreateXULElement(nsIRDFResource* aResource,
                              PRInt32 aNameSpaceID,
                              nsIAtom* aTag,
                              nsIContent** aResult);

    PRBool
    IsHTMLElement(nsIContent* aElement);

    nsresult AddAttribute(nsIContent* aElement,
                          nsIRDFResource* aProperty,
                          nsIRDFNode* aValue);

    nsresult RemoveAttribute(nsIContent* aElement,
                             nsIRDFResource* aProperty,
                             nsIRDFNode* aValue);

    nsresult CreateTreeBuilder(nsIContent* aElement,
                               const nsString& aDataSources);

    nsresult
    GetDOMNodeResource(nsIDOMNode* aNode, nsIRDFResource** aResource);
};

////////////////////////////////////////////////////////////////////////
// Pseudo-constants

PRInt32         RDFXULBuilderImpl::gRefCnt = 0;
nsIRDFService*  RDFXULBuilderImpl::gRDFService = nsnull;

PRInt32         RDFXULBuilderImpl::kNameSpaceID_RDF = kNameSpaceID_Unknown;
PRInt32         RDFXULBuilderImpl::kNameSpaceID_XUL = kNameSpaceID_Unknown;

nsIAtom*        RDFXULBuilderImpl::kContentsGeneratedAtom = nsnull;
nsIAtom*        RDFXULBuilderImpl::kIdAtom                = nsnull;
nsIAtom*        RDFXULBuilderImpl::kDataSourcesAtom       = nsnull;
nsIAtom*        RDFXULBuilderImpl::kTreeAtom              = nsnull;

nsIRDFResource* RDFXULBuilderImpl::kRDF_instanceOf;
nsIRDFResource* RDFXULBuilderImpl::kRDF_nextVal;
nsIRDFResource* RDFXULBuilderImpl::kRDF_type;
nsIRDFResource* RDFXULBuilderImpl::kRDF_child;
nsIRDFResource* RDFXULBuilderImpl::kXUL_element;

////////////////////////////////////////////////////////////////////////

nsresult
NS_NewRDFXULBuilder(nsIRDFContentModelBuilder** result)
{
    NS_PRECONDITION(result != nsnull, "null ptr");
    if (! result)
        return NS_ERROR_NULL_POINTER;

    RDFXULBuilderImpl* builder = new RDFXULBuilderImpl();
    if (! builder)
        return NS_ERROR_OUT_OF_MEMORY;

    NS_ADDREF(builder);
    *result = builder;
    return NS_OK;
}


RDFXULBuilderImpl::RDFXULBuilderImpl(void)
    : mDB(nsnull),
      mDocument(nsnull),
      mRoot(nsnull)
{
    NS_INIT_REFCNT();

    if (gRefCnt++ == 0) {
        nsresult rv;
        nsINameSpaceManager* mgr;
        if (NS_SUCCEEDED(rv = nsRepository::CreateInstance(kNameSpaceManagerCID,
                                                           nsnull,
                                                           kINameSpaceManagerIID,
                                                           (void**) &mgr))) {

            rv = mgr->RegisterNameSpace(kXULNameSpaceURI, kNameSpaceID_XUL);
            NS_ASSERTION(NS_SUCCEEDED(rv), "unable to register XUL namespace");

            rv = mgr->RegisterNameSpace(kRDFNameSpaceURI, kNameSpaceID_RDF);
            NS_ASSERTION(NS_SUCCEEDED(rv), "unable to register RDF namespace");

            NS_RELEASE(mgr);
        }
        else {
            NS_ERROR("couldn't create namepsace manager");
        }

        kContentsGeneratedAtom = NS_NewAtom("contentsGenerated");
        kIdAtom                = NS_NewAtom("id");
        kDataSourcesAtom       = NS_NewAtom("datasources");
        kTreeAtom              = NS_NewAtom("tree");


        if (NS_SUCCEEDED(rv = nsServiceManager::GetService(kRDFServiceCID,
                                                           kIRDFServiceIID,
                                                           (nsISupports**) &gRDFService))) {

            NS_VERIFY(NS_SUCCEEDED(gRDFService->GetResource(kURIRDF_instanceOf, &kRDF_instanceOf)),
                      "unable to get resource");

            NS_VERIFY(NS_SUCCEEDED(gRDFService->GetResource(kURIRDF_nextVal, &kRDF_nextVal)),
                      "unable to get resource");

            NS_VERIFY(NS_SUCCEEDED(gRDFService->GetResource(kURIRDF_type, &kRDF_type)),
                      "unable to get resource");

            NS_VERIFY(NS_SUCCEEDED(gRDFService->GetResource(kURIRDF_child, &kRDF_child)),
                      "unable to get resource");

            NS_VERIFY(NS_SUCCEEDED(gRDFService->GetResource(kURIXUL_element, &kXUL_element)),
                      "unable to get resource");
        }
        else {
            NS_ERROR("couldn't get RDF service");
        }
    }
}

RDFXULBuilderImpl::~RDFXULBuilderImpl(void)
{
    NS_IF_RELEASE(mRoot);
    if (mDB) {
        mDB->RemoveObserver(this);
        NS_RELEASE(mDB);
    }
    // NS_IF_RELEASE(mDocument) not refcounted

    if (--gRefCnt == 0) {
        if (gRDFService)
            nsServiceManager::ReleaseService(kRDFServiceCID, gRDFService);

        NS_IF_RELEASE(kRDF_instanceOf);
        NS_IF_RELEASE(kRDF_nextVal);
        NS_IF_RELEASE(kRDF_type);
        NS_IF_RELEASE(kRDF_child);
        NS_IF_RELEASE(kXUL_element);

        NS_IF_RELEASE(kContentsGeneratedAtom);
        NS_IF_RELEASE(kIdAtom);
        NS_IF_RELEASE(kDataSourcesAtom);
        NS_IF_RELEASE(kTreeAtom);
    }
}

////////////////////////////////////////////////////////////////////////

NS_IMPL_ADDREF(RDFXULBuilderImpl);
NS_IMPL_RELEASE(RDFXULBuilderImpl);

NS_IMETHODIMP
RDFXULBuilderImpl::QueryInterface(REFNSIID iid, void** aResult)
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
    else if (iid.Equals(nsIDOMNodeObserver::IID())) {
        *aResult = NS_STATIC_CAST(nsIDOMNodeObserver*, this);
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
RDFXULBuilderImpl::SetDocument(nsIRDFDocument* aDocument)
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
RDFXULBuilderImpl::SetDataBase(nsIRDFCompositeDataSource* aDataBase)
{
    NS_PRECONDITION(aDataBase != nsnull, "null ptr");
    if (! aDataBase)
        return NS_ERROR_NULL_POINTER;

    NS_PRECONDITION(mDB == nsnull, "already initialized");
    if (mDB)
        return NS_ERROR_ALREADY_INITIALIZED;

    mDB = aDataBase;
    NS_ADDREF(mDB);

    mDB->AddObserver(this);
    return NS_OK;
}

NS_IMETHODIMP
RDFXULBuilderImpl::GetDataBase(nsIRDFCompositeDataSource** aDataBase)
{
    NS_PRECONDITION(aDataBase != nsnull, "null ptr");
    if (! aDataBase)
        return NS_ERROR_NULL_POINTER;

    *aDataBase = mDB;
    NS_ADDREF(mDB);
    return NS_OK;
}


NS_IMETHODIMP
RDFXULBuilderImpl::CreateRootContent(nsIRDFResource* aResource)
{
    NS_PRECONDITION(mDocument != nsnull, "not initialized");
    if (! mDocument)
        return NS_ERROR_NOT_INITIALIZED;

    NS_PRECONDITION(aResource != nsnull, "null ptr");
    if (! aResource)
        return NS_ERROR_NULL_POINTER;

    nsresult rv;

    nsCOMPtr<nsIContent> root;
    if (NS_FAILED(rv = CreateElement(aResource, getter_AddRefs(root)))) {
        NS_ERROR("unable to create root element");
        return rv;
    }

    // Now set it as the document's root content
    nsCOMPtr<nsIDocument> doc;
    if (NS_FAILED(rv = mDocument->QueryInterface(kIDocumentIID,
                                                 (void**) getter_AddRefs(doc)))) {
        NS_ERROR("couldn't get nsIDocument interface");
        return rv;
    }

    doc->SetRootContent(root);

    mRoot = root;
    NS_ADDREF(mRoot);

    return NS_OK;
}


NS_IMETHODIMP
RDFXULBuilderImpl::SetRootContent(nsIContent* aElement)
{
    NS_IF_RELEASE(mRoot);
    mRoot = aElement;
    NS_IF_ADDREF(mRoot);
    return NS_OK;
}


NS_IMETHODIMP
RDFXULBuilderImpl::CreateContents(nsIContent* aElement)
{
    nsresult rv;

    // First, mark the elements contents as being generated so that
    // any re-entrant calls don't trigger an infinite recursion.
    if (NS_FAILED(rv = aElement->SetAttribute(kNameSpaceID_XUL,
                                              kContentsGeneratedAtom,
                                              "true",
                                              PR_FALSE))) {
        NS_ERROR("unable to set contents-generated attribute");
        return rv;
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

    // Ignore any elements that aren't XUL elements: we can't
    // construct content for them anyway.
    PRBool isXULElement;
    if (NS_FAILED(rv = mDB->HasAssertion(resource, kRDF_instanceOf, kXUL_element, PR_TRUE, &isXULElement))) {
        NS_ERROR("unable to determine if element is a XUL element");
        return rv;
    }

    if (! isXULElement)
        return NS_OK;

    // If it's a XUL element, it'd better be an RDF Sequence...
    NS_ASSERTION(rdf_IsContainer(mDB, resource), "element is a XUL:element, but not an RDF:Seq");
    if (! rdf_IsContainer(mDB, resource))
        return NS_ERROR_UNEXPECTED;

    // Iterate through all of the element's children, and construct
    // appropriate children for each arc.
    nsCOMPtr<nsIRDFAssertionCursor> children;
    if (NS_FAILED(rv = NS_NewContainerCursor(mDB, resource, getter_AddRefs(children)))) {
        NS_ERROR("unable to create cursor for children");
        return rv;
    }

    while (NS_SUCCEEDED(rv = children->Advance())) {
        nsCOMPtr<nsIRDFNode> child;
        if (NS_FAILED(rv = children->GetObject(getter_AddRefs(child)))) {
            NS_ERROR("error reading cursor");
            return rv;
        }

        if (NS_FAILED(AppendChild(aElement, child))) {
            NS_ERROR("problem appending child to content model");
            return rv;
        }
    }

    if (rv == NS_ERROR_RDF_CURSOR_EMPTY)
        rv = NS_OK;

    return rv;
}


NS_IMETHODIMP
RDFXULBuilderImpl::OnAssert(nsIRDFResource* aSubject,
                            nsIRDFResource* aPredicate,
                            nsIRDFNode* aObject)
{
    NS_PRECONDITION(mDocument != nsnull, "not initialized");
    if (! mDocument)
        return NS_ERROR_NOT_INITIALIZED;

    // Stuff that we can ignore outright
    // XXX is this the best place to put it???
    if ((aPredicate == kRDF_nextVal) ||
        (aPredicate == kRDF_instanceOf))
        return NS_OK;

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

        if (rdf_IsOrdinalProperty(aPredicate)) {
            // It's a child node. If the contents of aElement _haven't_
            // yet been generated, then just ignore the assertion. We do
            // this because we know that _eventually_ the contents will be
            // generated (via CreateContents()) when somebody asks for
            // them later.
            nsAutoString contentsGenerated;
            if (NS_FAILED(rv = element->GetAttribute(kNameSpaceID_XUL,
                                                     kContentsGeneratedAtom,
                                                     contentsGenerated))) {
                NS_ERROR("severe problem trying to get attribute");
                return rv;
            }

            if (rv == NS_CONTENT_ATTR_NOT_THERE || rv == NS_CONTENT_ATTR_NO_VALUE)
                continue;

            if (! contentsGenerated.EqualsIgnoreCase("true"))
                continue;

            // Okay, it's a "live" element, so go ahead and append the new
            // child to this node.
            if (NS_FAILED(AppendChild(element, aObject))) {
                NS_ERROR("problem appending child to content model");
                return rv;
            }
        }
        else if (aPredicate == kRDF_type) {
            // We shouldn't ever see this: if we do, there ain't much we
            // can do.
            NS_ERROR("attempt to change tag type after-the-fact");
            return NS_ERROR_UNEXPECTED;
        }
        else {
            // Add the thing as a vanilla attribute to the element.
            if (NS_FAILED(rv = AddAttribute(element, aPredicate, aObject))) {
                NS_ERROR("unable to add attribute to the element");
                return rv;
            }
        }
    }
    return NS_OK;
}


NS_IMETHODIMP
RDFXULBuilderImpl::OnUnassert(nsIRDFResource* aSubject,
                              nsIRDFResource* aPredicate,
                              nsIRDFNode* aObject)
{
    NS_PRECONDITION(mDocument != nsnull, "not initialized");
    if (! mDocument)
        return NS_ERROR_NOT_INITIALIZED;

    // Stuff that we can ignore outright
    // XXX is this the best place to put it???
    if ((aPredicate == kRDF_nextVal) ||
        (aPredicate == kRDF_instanceOf))
        return NS_OK;

    nsresult rv;

    nsCOMPtr<nsISupportsArray> elements;
    if (NS_FAILED(rv = NS_NewISupportsArray(getter_AddRefs(elements)))) {
        NS_ERROR("unable to create new ISupportsArray");
        return rv;
    }

    // Find all the elements in the content model that correspond to
    // aSubject: for each, we'll try to remove XUL children if
    // appropriate.
    if (NS_FAILED(rv = mDocument->GetElementsForResource(aSubject, elements))) {
        NS_ERROR("unable to retrieve elements from resource");
        return rv;
    }

    for (PRInt32 i = elements->Count() - 1; i >= 0; --i) {
        nsCOMPtr<nsIContent> element(elements->ElementAt(i));
        
        // XXX somehow figure out if removing XUL kids from this
        // particular element makes any sense whatsoever.

        if (rdf_IsOrdinalProperty(aPredicate)) {
            // It's a child node. If the contents of aElement _haven't_
            // yet been generated, then just ignore the unassertion. We do
            // this because we know that _eventually_ the contents will be
            // generated (via CreateContents()) when somebody asks for
            // them later.
            nsAutoString contentsGenerated;
            if (NS_FAILED(rv = element->GetAttribute(kNameSpaceID_XUL,
                                                     kContentsGeneratedAtom,
                                                     contentsGenerated))) {
                NS_ERROR("severe problem trying to get attribute");
                return rv;
            }

            if (rv == NS_CONTENT_ATTR_NOT_THERE || rv == NS_CONTENT_ATTR_NO_VALUE)
                continue;

            if (! contentsGenerated.EqualsIgnoreCase("true"))
                continue;

            // Okay, it's a "live" element, so go ahead and remove the
            // child from this node.
            if (NS_FAILED(RemoveChild(element, aObject))) {
                NS_ERROR("problem removing child from content model");
                return rv;
            }
        }
        else if (aPredicate == kRDF_type) {
            // We shouldn't ever see this: if we do, there ain't much we
            // can do.
            NS_ERROR("attempt to remove tag type");
            return NS_ERROR_UNEXPECTED;
        }
        else {
            // Remove this attribute from the element.
            if (NS_FAILED(rv = RemoveAttribute(element, aPredicate, aObject))) {
                NS_ERROR("unable to remove attribute to the element");
                return rv;
            }
        }
    }
    return NS_OK;
}


////////////////////////////////////////////////////////////////////////
// nsIDOMNodeObserver interface

NS_IMETHODIMP
RDFXULBuilderImpl::OnSetNodeValue(nsIDOMNode* aNode, const nsString& aValue)
{
    NS_NOTYETIMPLEMENTED("write me!");
    return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
RDFXULBuilderImpl::OnInsertBefore(nsIDOMNode* aParent, nsIDOMNode* aNewChild, nsIDOMNode* aRefChild)
{
    nsresult rv;

    // Translate each of the DOM nodes into the RDF resource for which
    // they are acting as a proxy.

    // XXX TODO Figure out how to constrain this to only update nodes
    // that are appropriate for the XUL builder to be
    // updating. This'll just whack _every_ node that comes through...


    // XXX If aNewChild doesn't have a resource, then somebody is
    // inserting a non-RDF element into aParent. Panic for now.
    nsCOMPtr<nsIRDFResource> newChild;
    if (NS_FAILED(rv = GetDOMNodeResource(aNewChild, getter_AddRefs(newChild)))) {
        NS_ERROR("new child doesn't have a resource");
        return rv;
    }

    // If there was an old parent for newChild, then make sure to
    // remove that relationship.
    nsCOMPtr<nsIDOMNode> oldParentNode;
    if (NS_FAILED(rv = aNewChild->GetParentNode(getter_AddRefs(oldParentNode)))) {
        NS_ERROR("unable to get new child's parent");
        return rv;
    }

    if (oldParentNode) {
        nsCOMPtr<nsIRDFResource> oldParent;

        // If the old parent has a resource...
        if (NS_SUCCEEDED(rv = GetDOMNodeResource(oldParentNode, getter_AddRefs(oldParent)))) {

            // ...and it's a XUL element...
            PRBool isXULElement;

            if (NS_SUCCEEDED(rv = mDB->HasAssertion(oldParent,
                                                    kRDF_instanceOf,
                                                    kXUL_element,
                                                    PR_TRUE,
                                                    &isXULElement))
                && isXULElement) {

                // remove the child from the old collection
                if (NS_FAILED(rv = rdf_ContainerRemoveElement(mDB, oldParent, newChild))) {
                    NS_ERROR("unable to remove newChild from oldParent");
                    return rv;
                }
            }
        }
    }

    // If the new parent has a resource...
    nsCOMPtr<nsIRDFResource> parent;
    if (NS_SUCCEEDED(rv = GetDOMNodeResource(aParent, getter_AddRefs(parent)))) {

        // ...and it's a XUL element...
        PRBool isXULElement;
        if (NS_SUCCEEDED(rv = mDB->HasAssertion(parent,
                                                kRDF_instanceOf,
                                                kXUL_element,
                                                PR_TRUE,
                                                &isXULElement))
            && isXULElement) {

            // XXX For now, we panic if the refChild doesn't have a resouce
            nsCOMPtr<nsIRDFResource> refChild;
            if (NS_FAILED(rv = GetDOMNodeResource(aRefChild, getter_AddRefs(refChild)))) {
                NS_ERROR("ref child doesn't have a resource");
                return rv;
            }

            // Determine the index of the refChild in the container
            PRInt32 index;
            if (NS_FAILED(rv = rdf_ContainerIndexOf(mDB, parent, refChild, &index))) {
                NS_ERROR("unable to determine index of refChild in container");
                return rv;
            }

            // ...and insert the newChild before it.
            if (NS_FAILED(rv = rdf_ContainerInsertElementAt(mDB, parent, newChild, index))) {
                NS_ERROR("unable to insert new element into container");
                return rv;
            }
        }
    }

    return NS_OK;
}



NS_IMETHODIMP
RDFXULBuilderImpl::OnReplaceChild(nsIDOMNode* aParent, nsIDOMNode* aNewChild, nsIDOMNode* aOldChild)
{
    NS_NOTYETIMPLEMENTED("write me!");
    return NS_ERROR_NOT_IMPLEMENTED;
}



NS_IMETHODIMP
RDFXULBuilderImpl::OnRemoveChild(nsIDOMNode* aParent, nsIDOMNode* aOldChild)
{
    NS_NOTYETIMPLEMENTED("write me!");
    return NS_ERROR_NOT_IMPLEMENTED;
}



NS_IMETHODIMP
RDFXULBuilderImpl::OnAppendChild(nsIDOMNode* aParent, nsIDOMNode* aNewChild)
{
    NS_NOTYETIMPLEMENTED("write me!");
    return NS_ERROR_NOT_IMPLEMENTED;
}


////////////////////////////////////////////////////////////////////////
// Implementation methods

nsresult
RDFXULBuilderImpl::AppendChild(nsIContent* aElement,
                               nsIRDFNode* aValue)
{
    nsresult rv;

    // Add the specified node as a child container of this
    // element. What we do will vary slightly depending on whether
    // aValue is a resource or a literal.
    nsCOMPtr<nsIRDFResource> resource;
    nsCOMPtr<nsIRDFLiteral> literal;

    if (NS_SUCCEEDED(rv = aValue->QueryInterface(kIRDFResourceIID,
                                                 (void**) getter_AddRefs(resource)))) {

        // If it's a resource, then add it as a child container.
        nsCOMPtr<nsIContent> child;
        if (NS_FAILED(rv = CreateElement(resource, getter_AddRefs(child)))) {
            NS_ERROR("unable to create new XUL element");
            return rv;
        }

        if (NS_FAILED(rv = aElement->AppendChildTo(child, PR_TRUE))) {
            NS_ERROR("unable to add element to content model");
            return rv;
        }
    }
    else if (NS_SUCCEEDED(rv = aValue->QueryInterface(kIRDFLiteralIID,
                                                      (void**) getter_AddRefs(literal)))) {
        // If it's a literal, then add it as a simple text node.

        if (NS_FAILED(rv = nsRDFContentUtils::AttachTextNode(aElement, literal))) {
            NS_ERROR("unable to add text to content model");
            return rv;
        }
    }
    else {
        // This should _never_ happen
        NS_ERROR("node is not a value or a resource");
        return NS_ERROR_UNEXPECTED;
    }

    return NS_OK;
}



nsresult
RDFXULBuilderImpl::RemoveChild(nsIContent* aElement, nsIRDFNode* aValue)
{
    nsresult rv;

    // Remove the specified node from the children of this
    // element. What we do will vary slightly depending on whether
    // aValue is a resource or a literal.
    nsCOMPtr<nsIRDFResource> resource;
    nsCOMPtr<nsIRDFLiteral> literal;

    if (NS_SUCCEEDED(rv = aValue->QueryInterface(kIRDFResourceIID,
                                                 (void**) getter_AddRefs(resource)))) {

        // If it's a resource, then look for a child container to remove
        const char* resourceURI;
        resource->GetValue(&resourceURI);

        PRInt32 count;
        aElement->ChildCount(count);
        while (--count >= 0) {
            nsCOMPtr<nsIContent> child;
            aElement->ChildAt(count, *getter_AddRefs(child));

            // XXX can't identify HTML elements right now.
            if (IsHTMLElement(child))
                continue;

            nsAutoString uri;
            rv = child->GetAttribute(kNameSpaceID_RDF, kIdAtom, uri);
            if (rv != NS_CONTENT_ATTR_HAS_VALUE)
                continue;

            if (! uri.Equals(resourceURI))
                continue;

            // okay, found it. now blow it away...
            aElement->RemoveChildAt(count, PR_TRUE);
            return NS_OK;
        }
    }
    else if (NS_SUCCEEDED(rv = aValue->QueryInterface(kIRDFLiteralIID,
                                                      (void**) getter_AddRefs(literal)))) {
        // If it's a literal, then look for a simple text node to remove
        NS_NOTYETIMPLEMENTED("write me!");
    }
    else {
        // This should _never_ happen
        NS_ERROR("node is not a value or a resource");
        return NS_ERROR_UNEXPECTED;
    }

    return NS_OK;
}


nsresult
RDFXULBuilderImpl::CreateElement(nsIRDFResource* aResource,
                                 nsIContent** aResult)
{
    nsresult rv;

    // Split the resource into a namespace ID and a tag, and create
    // a content element for it.
    nsCOMPtr<nsIRDFNode> typeNode;
    if (NS_FAILED(rv = mDB->GetTarget(aResource, kRDF_type, PR_TRUE, getter_AddRefs(typeNode)))) {
        NS_ERROR("unable to get node's type");
        return rv;
    }

    nsCOMPtr<nsIRDFResource> type;
    if (NS_FAILED(rv = typeNode->QueryInterface(kIRDFResourceIID, getter_AddRefs(type)))) {
        NS_ERROR("type wasn't a resource");
        return rv;
    }

    PRInt32 nameSpaceID;
    nsCOMPtr<nsIAtom> tag;
    if (NS_FAILED(rv = mDocument->SplitProperty(type, &nameSpaceID, getter_AddRefs(tag)))) {
        NS_ERROR("unable to split resource into namespace/tag pair");
        return rv;
    }

    if (nameSpaceID == kNameSpaceID_HTML) {
        return CreateHTMLElement(aResource, tag, aResult);
    }
    else {
        return CreateXULElement(aResource, nameSpaceID, tag, aResult);
    }
}

nsresult
RDFXULBuilderImpl::CreateHTMLElement(nsIRDFResource* aResource,
                                     nsIAtom* aTag,
                                     nsIContent** aResult)
{
    nsresult rv;

    // XXX This is where we go out and create the HTML content. It's a
    // bit of a hack: a bridge until we get to a more DOM-based
    // solution.
    nsCOMPtr<nsIHTMLContent> element;
    if (NS_FAILED(rv = NS_CreateHTMLElement(getter_AddRefs(element), aTag->GetUnicode()))) {
        NS_ERROR("unable to create HTML element");
        return rv;
    }

    {
        // Force the document to be set _here_. Many of the
        // AppendChildTo() implementations do not recursively ensure
        // that the child's doc is the same as the parent's.
        nsCOMPtr<nsIDocument> doc;
        if (NS_FAILED(rv = mDocument->QueryInterface(kIDocumentIID, getter_AddRefs(doc)))) {
            NS_ERROR("uh, this isn't a document!");
            return rv;
        }

        if (NS_FAILED(rv = element->SetDocument(doc, PR_FALSE))) {
            NS_ERROR("couldn't set document on the element");
            return rv;
        }
    }

    // Now iterate through all the properties and add them as
    // attributes on the element.  First, create a cursor that'll
    // iterate through all the properties that lead out of this
    // resource.
    nsCOMPtr<nsIRDFArcsOutCursor> properties;
    if (NS_FAILED(rv = mDB->ArcLabelsOut(aResource, getter_AddRefs(properties)))) {
        NS_ERROR("unable to create arcs-out cursor");
        return rv;
    }

    // Advance that cursor 'til it runs outta steam
    while (NS_SUCCEEDED(rv = properties->Advance())) {
        nsCOMPtr<nsIRDFResource> property;

        if (NS_FAILED(rv = properties->GetPredicate(getter_AddRefs(property)))) {
            NS_ERROR("unable to get property from cursor");
            return rv;
        }

        // These are special beacuse they're used to specify the tree
        // structure of the XUL: ignore them b/c they're not attributes
        if ((property == kRDF_instanceOf) ||
            (property == kRDF_nextVal) ||
            (property == kRDF_type))
            continue;

        // XXX TODO Move this out of this loop, and use a cursor
        // coughed up by NS_NewContainerCursor(), so we're sure that
        // these get created in the right order.

        // Recursively generate child nodes NOW: we can't "dummy" up
        // nsIHTMLContent.
        if (rdf_IsOrdinalProperty(property)) {
            CreateHTMLContents(element, aResource);
            continue;
        }

        // For each property, get its value: this will be the value of
        // the new attribute.
        nsCOMPtr<nsIRDFNode> value;
        if (NS_FAILED(rv = mDB->GetTarget(aResource, property, PR_TRUE, getter_AddRefs(value)))) {
            NS_ERROR("unable to get value for property");
            return rv;
        }

        // Add the attribute to the newly constructed element
        if (NS_FAILED(rv = AddAttribute(element, property, value))) {
            NS_ERROR("unable to add attribute to element");
            return rv;
        }
    }

    if (rv == NS_ERROR_RDF_CURSOR_EMPTY) {
        rv = NS_OK;
    }
    else if (NS_FAILED(rv)) {
        // uh oh...
        NS_ERROR("problem iterating properties");
        return rv;
    }

    if (NS_FAILED(rv = element->QueryInterface(kIContentIID, (void**) aResult))) {
        NS_ERROR("unable to get nsIContent interface");
        return rv;
    }

    return NS_OK;
}


nsresult
RDFXULBuilderImpl::CreateHTMLContents(nsIContent* aElement,
                                      nsIRDFResource* aResource)
{
    nsresult rv;

    nsCOMPtr<nsIRDFAssertionCursor> children;
    if (NS_FAILED(rv = NS_NewContainerCursor(mDB, aResource, getter_AddRefs(children)))) {
        NS_ERROR("unable to create cursor for children");
        return rv;
    }

    while (NS_SUCCEEDED(rv = children->Advance())) {
        nsCOMPtr<nsIRDFNode> child;
        if (NS_FAILED(rv = children->GetObject(getter_AddRefs(child)))) {
            NS_ERROR("error reading cursor");
            return rv;
        }

        if (NS_FAILED(AppendChild(aElement, child))) {
            NS_ERROR("problem appending child to content model");
            return rv;
        }
    }

    if (rv == NS_ERROR_RDF_CURSOR_EMPTY)
        rv = NS_OK;

    return rv;
}



nsresult
RDFXULBuilderImpl::CreateXULElement(nsIRDFResource* aResource,
                                    PRInt32 aNameSpaceID,
                                    nsIAtom* aTag,
                                    nsIContent** aResult)
{
    nsresult rv;

    nsCOMPtr<nsIRDFContent> element;
    if (NS_FAILED(rv = NS_NewRDFResourceElement(getter_AddRefs(element),
                                                aResource,
                                                aNameSpaceID,
                                                aTag))) {
        NS_ERROR("unable to create new content element");
        return rv;
    }

    // Now iterate through all the properties and add them as
    // attributes on the element.  First, create a cursor that'll
    // iterate through all the properties that lead out of this
    // resource.
    nsCOMPtr<nsIRDFArcsOutCursor> properties;
    if (NS_FAILED(rv = mDB->ArcLabelsOut(aResource, getter_AddRefs(properties)))) {
        NS_ERROR("unable to create arcs-out cursor");
        return rv;
    }

    // Advance that cursor 'til it runs outta steam
    while (NS_SUCCEEDED(rv = properties->Advance())) {
        nsCOMPtr<nsIRDFResource> property;

        if (NS_FAILED(rv = properties->GetPredicate(getter_AddRefs(property)))) {
            NS_ERROR("unable to get property from cursor");
            return rv;
        }

        // These are special beacuse they're used to specify the tree
        // structure of the XUL: ignore them b/c they're not attributes
        if ((property == kRDF_instanceOf) ||
            (property == kRDF_nextVal) ||
            (property == kRDF_type) ||
            rdf_IsOrdinalProperty(property))
            continue;

        // For each property, set its value.
        nsCOMPtr<nsIRDFNode> value;
        if (NS_FAILED(rv = mDB->GetTarget(aResource, property, PR_TRUE, getter_AddRefs(value)))) {
            NS_ERROR("unable to get value for property");
            return rv;
        }

        // Add the attribute to the newly constructed element
        if (NS_FAILED(rv = AddAttribute(element, property, value))) {
            NS_ERROR("unable to add attribute to element");
            return rv;
        }
    }

    if (rv == NS_ERROR_RDF_CURSOR_EMPTY) {
        rv = NS_OK;
    }
    else if (NS_FAILED(rv)) {
        // uh oh...
        NS_ERROR("problem iterating properties");
        return rv;
    }

    // Make it a container so that its contents get recursively
    // generated on-demand.
    if (NS_FAILED(rv = element->SetContainer(PR_TRUE))) {
        NS_ERROR("unable to make element a container");
        return rv;
    }

    // There are some tags that we need to pay extra-special attention to...
    if (aTag == kTreeAtom) {
        nsAutoString dataSources;
        if (NS_CONTENT_ATTR_HAS_VALUE ==
            element->GetAttribute(kNameSpaceID_XUL,
                                  kDataSourcesAtom,
                                  dataSources)) {
            rv = CreateTreeBuilder(element, dataSources);
            NS_ASSERTION(NS_SUCCEEDED(rv), "unable to add datasources");
        }
    }

    // Finally, assign the newly constructed element to the result
    // pointer and addref it for the trip home.
    *aResult = element;
    NS_ADDREF(*aResult);

    return NS_OK;
}



PRBool
RDFXULBuilderImpl::IsHTMLElement(nsIContent* aElement)
{
    nsresult rv;

    PRInt32 nameSpaceID;
    if (NS_FAILED(rv = aElement->GetNameSpaceID(nameSpaceID))) {
        NS_ERROR("unable to get element's namespace ID");
        return PR_FALSE;
    }

    return (kNameSpaceID_HTML == nameSpaceID);
}

nsresult
RDFXULBuilderImpl::AddAttribute(nsIContent* aElement,
                                nsIRDFResource* aProperty,
                                nsIRDFNode* aValue)
{
    nsresult rv;

    // First, split the property into its namespace and tag components
    PRInt32 nameSpaceID;
    nsCOMPtr<nsIAtom> tag;
    if (NS_FAILED(rv = mDocument->SplitProperty(aProperty, &nameSpaceID, getter_AddRefs(tag)))) {
        NS_ERROR("unable to split resource into namespace/tag pair");
        return rv;
    }

    if (IsHTMLElement(aElement)) {
        // XXX HTML elements are picky and only want attributes from
        // certain namespaces.
        switch (nameSpaceID) {
        case kNameSpaceID_HTML:
        case kNameSpaceID_None:
        case kNameSpaceID_Unknown:
            break;

        default:
            NS_WARNING("ignoring non-HTML attribute on HTML tag");
            return NS_OK;
        }
    }

    nsCOMPtr<nsIRDFResource> resource;
    nsCOMPtr<nsIRDFLiteral> literal;

    // Now we need to figure out the attributes value and actually set
    // it on the element. What we do differs a bit depending on
    // whether we're aValue is a resource or a literal.
    if (NS_SUCCEEDED(rv = aValue->QueryInterface(kIRDFResourceIID,
                                                 (void**) getter_AddRefs(resource)))) {
        const char* uri;
        resource->GetValue(&uri);
        rv = aElement->SetAttribute(nameSpaceID, tag, uri, PR_TRUE);
    }
    else if (NS_SUCCEEDED(rv = aValue->QueryInterface(kIRDFLiteralIID,
                                                      (void**) getter_AddRefs(literal)))) {
        const PRUnichar* s;
        literal->GetValue(&s);
        rv = aElement->SetAttribute(nameSpaceID, tag, s, PR_TRUE);
    }
    else {
        // This should _never_ happen.
        NS_ERROR("uh, this isn't a resource or a literal!");
        rv = NS_ERROR_UNEXPECTED;
    }

    return rv;
}


nsresult
RDFXULBuilderImpl::RemoveAttribute(nsIContent* aElement,
                                   nsIRDFResource* aProperty,
                                   nsIRDFNode* aValue)
{
    NS_NOTYETIMPLEMENTED("write me!");
    return NS_ERROR_NOT_IMPLEMENTED;
}

nsresult
RDFXULBuilderImpl::CreateTreeBuilder(nsIContent* aElement,
                                     const nsString& aDataSources)
{
    nsresult rv;

    // construct a new builder
    nsCOMPtr<nsIRDFContentModelBuilder> builder;
    if (NS_FAILED(rv = nsRepository::CreateInstance(kRDFTreeBuilderCID,
                                                    nsnull,
                                                    kIRDFContentModelBuilderIID,
                                                    (void**) getter_AddRefs(builder)))) {
        NS_ERROR("unable to create tree content model builder");
        return rv;
    }

    if (NS_FAILED(rv = builder->SetRootContent(aElement))) {
        NS_ERROR("unable to set builder's root content element");
        return rv;
    }

    // create a database for the builder
    nsCOMPtr<nsIRDFCompositeDataSource> db;
    if (NS_FAILED(rv = nsRepository::CreateInstance(kRDFCompositeDataSourceCID,
                                                    nsnull,
                                                    kIRDFCompositeDataSourceIID,
                                                    (void**) getter_AddRefs(db)))) {
        NS_ERROR("unable to construct new composite data source");
        return rv;
    }

    // Parse datasources: they are assumed to be a whitespace
    // separated list of URIs; e.g.,
    //
    //     rdf:bookmarks rdf:history http://foo.bar.com/blah.cgi?baz=9
    //
    PRInt32 first = 0;

    while(1) {
        while (first < aDataSources.Length() && nsString::IsSpace(aDataSources[first]))
            ++first;

        if (first >= aDataSources.Length())
            break;

        PRInt32 last = first;
        while (last < aDataSources.Length() && !nsString::IsSpace(aDataSources[last]))
            ++last;

        nsAutoString uri;
        aDataSources.Mid(uri, first, last - first);
        first = last + 1;

        nsCOMPtr<nsIRDFDataSource> ds;

        // Some monkey business to convert the nsAutoString to a
        // C-string safely. Sure'd be nice to have this be automagic.
        {
            char buf[256], *p = buf;
            if (uri.Length() >= sizeof(buf))
                p = new char[uri.Length() + 1];

            uri.ToCString(p, uri.Length() + 1);

            rv = gRDFService->GetDataSource(p, getter_AddRefs(ds));

            if (p != buf)
                delete[] p;
        }

        if (NS_FAILED(rv)) {
            // This is only a warning because the data source may not
            // be accessable for any number of reasons, including
            // security, a bad URL, etc.
            NS_WARNING("unable to load datasource");
            continue;
        }

        if (NS_FAILED(rv = db->AddDataSource(ds))) {
            NS_ERROR("unable to add datasource to composite data source");
            return rv;
        }
    }

    if (NS_FAILED(rv = builder->SetDataBase(db))) {
        NS_ERROR("unable to set builder's database");
        return rv;
    }

    // add it to the set of builders in use by the document
    if (NS_FAILED(rv = mDocument->AddContentModelBuilder(builder))) {
        NS_ERROR("unable to add builder to the document");
        return rv;
    }

    return NS_OK;
}


nsresult
RDFXULBuilderImpl::GetDOMNodeResource(nsIDOMNode* aNode, nsIRDFResource** aResource)
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

