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

  1) Need to implement nsRDFXULBuilder::RemoveAttribute(), and
     figure out how to do nsRDFXULBuilder::Remove() (vanilla) when the
     child isn't a resource element itself.

  2) Implement the remainder of the DOM methods.

  */

#include "nsCOMPtr.h"
#include "nsDebug.h"
#include "nsIAtom.h"
#include "nsIContent.h"
#include "nsIDocument.h"
#include "nsIXULParentDocument.h"
#include "nsIXULChildDocument.h"
#include "nsIDOMElement.h"
#include "nsIDOMElementObserver.h"
#include "nsIDOMNode.h"
#include "nsIDOMNodeObserver.h"
#include "nsINameSpaceManager.h"
#include "nsIRDFContentModelBuilder.h"
#include "nsIRDFCursor.h"
#include "nsIRDFCompositeDataSource.h"
#include "nsIRDFDocument.h"
#include "nsIRDFNode.h"
#include "nsIRDFObserver.h"
#include "nsIRDFService.h"
#include "nsIServiceManager.h"
#include "nsIStreamListener.h"
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
#include "nsIDOMXULElement.h"
#include "nsIDOMXULDocument.h"
#include "nsIXULDocumentInfo.h"
#include "nsIContentViewerContainer.h"
#include "nsIDocumentLoader.h"
#include "nsIWebShell.h"

// XXX These are needed as scaffolding until we get to a more
// DOM-based solution.
#include "nsHTMLParts.h"
#include "nsIHTMLContent.h"

////////////////////////////////////////////////////////////////////////

static NS_DEFINE_IID(kIContentIID,                NS_ICONTENT_IID);
static NS_DEFINE_IID(kIDocumentIID,               NS_IDOCUMENT_IID);
static NS_DEFINE_IID(kINameSpaceManagerIID,       NS_INAMESPACEMANAGER_IID);
static NS_DEFINE_IID(kIRDFContentModelBuilderIID, NS_IRDFCONTENTMODELBUILDER_IID);
static NS_DEFINE_IID(kIRDFCompositeDataSourceIID, NS_IRDFCOMPOSITEDATASOURCE_IID);
static NS_DEFINE_IID(kIRDFLiteralIID,             NS_IRDFLITERAL_IID);
static NS_DEFINE_IID(kIRDFObserverIID,            NS_IRDFOBSERVER_IID);
static NS_DEFINE_IID(kIRDFResourceIID,            NS_IRDFRESOURCE_IID);
static NS_DEFINE_IID(kIRDFServiceIID,             NS_IRDFSERVICE_IID);
static NS_DEFINE_IID(kISupportsIID,               NS_ISUPPORTS_IID);
static NS_DEFINE_IID(kIXULDocumentInfoIID,        NS_IXULDOCUMENTINFO_IID);

static NS_DEFINE_CID(kNameSpaceManagerCID,        NS_NAMESPACEMANAGER_CID);
static NS_DEFINE_CID(kRDFCompositeDataSourceCID,  NS_RDFCOMPOSITEDATASOURCE_CID);
static NS_DEFINE_CID(kRDFServiceCID,              NS_RDFSERVICE_CID);
static NS_DEFINE_CID(kRDFTreeBuilderCID,          NS_RDFTREEBUILDER_CID);
static NS_DEFINE_CID(kRDFMenuBuilderCID,          NS_RDFMENUBUILDER_CID);
static NS_DEFINE_CID(kRDFToolbarBuilderCID,       NS_RDFTOOLBARBUILDER_CID);
static NS_DEFINE_CID(kXULDocumentCID,             NS_XULDOCUMENT_CID);
static NS_DEFINE_CID(kXULDocumentInfoCID,         NS_XULDOCUMENTINFO_CID);

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
                          public nsIDOMNodeObserver,
                          public nsIDOMElementObserver
{
private:
    nsIRDFCompositeDataSource* mDB;
    nsIRDFDocument*            mDocument;
    nsIContent*                mRoot;

    // pseudo-constants
    static PRInt32 gRefCnt;
    static nsIRDFService*       gRDFService;
    static nsINameSpaceManager* gNameSpaceManager;

    static PRInt32  kNameSpaceID_RDF;
    static PRInt32  kNameSpaceID_XUL;

    static nsIAtom* kContainerAtom;
    static nsIAtom* kXULContentsGeneratedAtom;
    static nsIAtom* kXULIncludeSrcAtom;
    static nsIAtom* kDataSourcesAtom;
    static nsIAtom* kIdAtom;
    static nsIAtom* kTreeAtom;
    static nsIAtom* kMenuAtom;
    static nsIAtom* kMenuBarAtom;
    static nsIAtom* kToolbarAtom;

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

    // nsIDOMElementObserver interface
    NS_DECL_IDOMELEMENTOBSERVER

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

    nsresult CreateBuilder(const nsCID& aBuilderCID, nsIContent* aElement,
                           const nsString& aDataSources);

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

    nsresult
    GetElementResource(nsIContent* aElement, nsIRDFResource** aResult);
};

////////////////////////////////////////////////////////////////////////
// Pseudo-constants

PRInt32              RDFXULBuilderImpl::gRefCnt;
nsIRDFService*       RDFXULBuilderImpl::gRDFService;
nsINameSpaceManager* RDFXULBuilderImpl::gNameSpaceManager;

PRInt32         RDFXULBuilderImpl::kNameSpaceID_RDF = kNameSpaceID_Unknown;
PRInt32         RDFXULBuilderImpl::kNameSpaceID_XUL = kNameSpaceID_Unknown;

nsIAtom*        RDFXULBuilderImpl::kContainerAtom;
nsIAtom*        RDFXULBuilderImpl::kXULContentsGeneratedAtom;
nsIAtom*        RDFXULBuilderImpl::kXULIncludeSrcAtom;
nsIAtom*        RDFXULBuilderImpl::kIdAtom;
nsIAtom*        RDFXULBuilderImpl::kDataSourcesAtom;
nsIAtom*        RDFXULBuilderImpl::kTreeAtom;
nsIAtom*        RDFXULBuilderImpl::kMenuAtom;
nsIAtom*        RDFXULBuilderImpl::kMenuBarAtom;
nsIAtom*        RDFXULBuilderImpl::kToolbarAtom;

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
        if (NS_SUCCEEDED(rv = nsComponentManager::CreateInstance(kNameSpaceManagerCID,
                                                           nsnull,
                                                           kINameSpaceManagerIID,
                                                           (void**) &gNameSpaceManager))) {

            rv = gNameSpaceManager->RegisterNameSpace(kXULNameSpaceURI, kNameSpaceID_XUL);
            NS_ASSERTION(NS_SUCCEEDED(rv), "unable to register XUL namespace");

            rv = gNameSpaceManager->RegisterNameSpace(kRDFNameSpaceURI, kNameSpaceID_RDF);
            NS_ASSERTION(NS_SUCCEEDED(rv), "unable to register RDF namespace");
        }
        else {
            NS_ERROR("couldn't create namepsace manager");
        }

        kContainerAtom            = NS_NewAtom("container");
        kXULContentsGeneratedAtom = NS_NewAtom("xulcontentsgenerated");
        kXULIncludeSrcAtom        = NS_NewAtom("include");
        kIdAtom                   = NS_NewAtom("id");
        kDataSourcesAtom          = NS_NewAtom("datasources");
        kTreeAtom                 = NS_NewAtom("tree");
        kMenuAtom                 = NS_NewAtom("menu");
        kMenuBarAtom              = NS_NewAtom("menubar");
        kToolbarAtom              = NS_NewAtom("toolbar");

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

        NS_IF_RELEASE(gNameSpaceManager);

        NS_IF_RELEASE(kRDF_instanceOf);
        NS_IF_RELEASE(kRDF_nextVal);
        NS_IF_RELEASE(kRDF_type);
        NS_IF_RELEASE(kRDF_child);
        NS_IF_RELEASE(kXUL_element);

        NS_IF_RELEASE(kContainerAtom);
        NS_IF_RELEASE(kXULContentsGeneratedAtom);
        NS_IF_RELEASE(kXULIncludeSrcAtom);
        NS_IF_RELEASE(kIdAtom);
        NS_IF_RELEASE(kDataSourcesAtom);
        NS_IF_RELEASE(kTreeAtom);
        NS_IF_RELEASE(kMenuAtom);
        NS_IF_RELEASE(kMenuBarAtom);
        NS_IF_RELEASE(kToolbarAtom);
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
    }
    else if (iid.Equals(kIRDFObserverIID)) {
        *aResult = NS_STATIC_CAST(nsIRDFObserver*, this);
    }
    else if (iid.Equals(nsIDOMNodeObserver::GetIID())) {
        *aResult = NS_STATIC_CAST(nsIDOMNodeObserver*, this);
    }
    else if (iid.Equals(nsIDOMElementObserver::GetIID())) {
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

    // See if someone has marked the element's contents as being
    // generated: this prevents a re-entrant call from triggering
    // another generation.
    nsAutoString attrValue;
    if (NS_FAILED(rv = aElement->GetAttribute(kNameSpaceID_None,
                                              kXULContentsGeneratedAtom,
                                              attrValue))) {
        NS_ERROR("unable to test contents-generated attribute");
        return rv;
    }

    if ((rv == NS_CONTENT_ATTR_HAS_VALUE) && (attrValue.EqualsIgnoreCase("true")))
        return NS_OK;

    // Get the treeitem's resource so that we can generate cell
    // values. We could QI for the nsIRDFResource here, but doing this
    // via the nsIContent interface allows us to support generic nodes
    // that might get added in by DOM calls.
    nsCOMPtr<nsIRDFResource> resource;
    if (NS_FAILED(rv = GetElementResource(aElement, getter_AddRefs(resource)))) {
        NS_ERROR("unable to get element resource");
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

    // Now mark the element's contents as being generated so that
    // any re-entrant calls don't trigger an infinite recursion.
    if (NS_FAILED(rv = aElement->SetAttribute(kNameSpaceID_None,
                                              kXULContentsGeneratedAtom,
                                              "true",
                                              PR_FALSE))) {
        NS_ERROR("unable to set contents-generated attribute");
        return rv;
    }

    // Iterate through all of the element's children, and construct
    // appropriate children for each arc.
    nsCOMPtr<nsIRDFAssertionCursor> children;
    if (NS_FAILED(rv = NS_NewContainerCursor(mDB, resource, getter_AddRefs(children)))) {
        NS_ERROR("unable to create cursor for children");
        return rv;
    }

    while (1) {
        rv = children->Advance();
        if (NS_FAILED(rv))
            return rv;

        if (rv == NS_RDF_CURSOR_EMPTY)
            break;

        nsCOMPtr<nsIRDFNode> child;
        if (NS_FAILED(rv = children->GetTarget(getter_AddRefs(child)))) {
            NS_ERROR("error reading cursor");
            return rv;
        }

        NS_ASSERTION(rv != NS_RDF_NO_VALUE, "null value in cursor");
        if (rv == NS_RDF_NO_VALUE)
            continue;

        if (NS_FAILED(AppendChild(aElement, child))) {
            NS_ERROR("problem appending child to content model");
            return rv;
        }
    }

    // Now that we've built the children, check to see if the includesrc attribute
    // exists on the node.
    nsString includeSrc;
    if (NS_FAILED(rv = aElement->GetAttribute(kNameSpaceID_None,
                                              kXULIncludeSrcAtom,
                                              includeSrc))) {
        NS_ERROR("unable to retrieve includeSrc attribute");
        return rv;
    }

    if (rv == NS_CONTENT_ATTR_HAS_VALUE) {
        // Build a URL object from the attribute's value.
        
        nsCOMPtr<nsIDocument> parentDoc;
        aElement->GetDocument(*getter_AddRefs(parentDoc));

        if (parentDoc == nsnull) {
            NS_ERROR("Unable to retrieve parent document for a subdocument.");
            return rv;
        }
    
        nsCOMPtr<nsIContentViewerContainer> container;
        nsCOMPtr<nsIXULParentDocument> xulParentDocument;
        xulParentDocument = do_QueryInterface(parentDoc);
        if (xulParentDocument == nsnull) {
            NS_ERROR("Unable to turn document into a XUL parent document.");
            return rv;
        }

        if (NS_FAILED(rv = xulParentDocument->GetContentViewerContainer(getter_AddRefs(container)))) {
            NS_ERROR("Unable to retrieve content viewer container from parent document.");
            return rv;
        }

        nsAutoString command;
        if (NS_FAILED(rv = xulParentDocument->GetCommand(command))) {
            NS_ERROR("Unable to retrieve the command from parent document.");
            return rv;
        }
        
        nsCOMPtr<nsIDOMXULElement> xulElement;
        xulElement = do_QueryInterface(aElement);
        if (!xulElement) {
            NS_ERROR("The fragment root is not a XUL element.");
            return rv;
        }

        nsCOMPtr<nsIRDFResource> rdfResource;
        xulElement->GetResource(getter_AddRefs(rdfResource));
        if (!rdfResource) {
            NS_ERROR("The fragment root doesn't have an RDF resource behind it.");
            return rv;
        }
        
        nsCOMPtr<nsIXULDocumentInfo> docInfo;
        if (NS_FAILED(rv = nsComponentManager::CreateInstance(kXULDocumentInfoCID,
                                                              nsnull,
                                                              kIXULDocumentInfoIID,
                                                              (void**) getter_AddRefs(docInfo)))) {
            NS_ERROR("unable to create document info object");
            return rv;
        }
        
        if (NS_FAILED(rv = docInfo->Init(parentDoc, rdfResource))) {
            NS_ERROR("unable to initialize doc info object.");
            return rv;
        }
        
        // Turn the content viewer into a webshell
        nsCOMPtr<nsIWebShell> webshell;
        webshell = do_QueryInterface(container);
        if (webshell == nsnull) {
            NS_ERROR("this isn't a webshell. we're in trouble.");
            return rv;
        }

        nsCOMPtr<nsIDocumentLoader> docLoader;
        if (NS_FAILED(rv = webshell->GetDocumentLoader(*getter_AddRefs(docLoader)))) {
            NS_ERROR("unable to obtain the document loader to kick off the load.");
            return rv;
        }

        docLoader->LoadSubDocument(includeSrc,
                                   docInfo.get());
    }

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
        nsCOMPtr<nsIContent> element( do_QueryInterface(elements->ElementAt(i)) );
        
        // XXX somehow figure out if building XUL kids on this
        // particular element makes any sense whatsoever.

        if (rdf_IsOrdinalProperty(aPredicate)) {
            // It's a child node. If the contents of aElement _haven't_
            // yet been generated, then just ignore the assertion. We do
            // this because we know that _eventually_ the contents will be
            // generated (via CreateContents()) when somebody asks for
            // them later.
            nsAutoString contentsGenerated;
            if (NS_FAILED(rv = element->GetAttribute(kNameSpaceID_None,
                                                     kXULContentsGeneratedAtom,
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
        nsCOMPtr<nsIContent> element( do_QueryInterface(elements->ElementAt(i)) );
        
        // XXX somehow figure out if removing XUL kids from this
        // particular element makes any sense whatsoever.

        if (rdf_IsOrdinalProperty(aPredicate)) {
            // It's a child node. If the contents of aElement _haven't_
            // yet been generated, then just ignore the unassertion. We do
            // this because we know that _eventually_ the contents will be
            // generated (via CreateContents()) when somebody asks for
            // them later.
            nsAutoString contentsGenerated;
            if (NS_FAILED(rv = element->GetAttribute(kNameSpaceID_None,
                                                     kXULContentsGeneratedAtom,
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
    NS_PRECONDITION(aParent != nsnull, "null ptr");
    if (!aParent)
        return NS_ERROR_NULL_POINTER;

    NS_PRECONDITION(aNewChild != nsnull, "null ptr");
    if (!aNewChild)
        return NS_ERROR_NULL_POINTER;

    NS_PRECONDITION(aRefChild != nsnull, "null ptr");
    if (!aRefChild)
        return NS_ERROR_NULL_POINTER;

    nsresult rv;

    // Translate each of the DOM nodes into the RDF resource for which
    // they are acting as a proxy. This implementation will only
    // update nodes that are an RDF:instanceOf a XUL:element.

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
    NS_PRECONDITION(aParent != nsnull, "null ptr");
    if (!aParent)
        return NS_ERROR_NULL_POINTER;

    NS_PRECONDITION(aNewChild != nsnull, "null ptr");
    if (!aNewChild)
        return NS_ERROR_NULL_POINTER;

    NS_PRECONDITION(aOldChild != nsnull, "null ptr");
    if (!aOldChild)
        return NS_ERROR_NULL_POINTER;

    nsresult rv;

    // Translate each of the DOM nodes into the RDF resource for which
    // they are acting as a proxy. This implementation will only
    // update nodes that are an RDF:instanceOf a XUL:element.

    // XXX If aNewChild doesn't have a resource, then somebody is
    // inserting a non-RDF element into aParent. Panic for now.
    nsCOMPtr<nsIRDFResource> newChild;
    if (NS_FAILED(rv = GetDOMNodeResource(aNewChild, getter_AddRefs(newChild)))) {
        NS_ERROR("new child doesn't have a resource");
        return rv;
    }

    // XXX ibid
    nsCOMPtr<nsIRDFResource> oldChild;
    if (NS_FAILED(rv = GetDOMNodeResource(aOldChild, getter_AddRefs(oldChild)))) {
        NS_ERROR("old child doesn't have a resource");
        return rv;
    }

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

            // Remember the old child's index...
            PRInt32 index;
            if (NS_FAILED(rv = rdf_ContainerIndexOf(mDB, parent, oldChild, &index))) {
                NS_ERROR("unable to get index of old child in container");
                return rv;
            }

            // ...then remove the old child from the old collection...
            if (NS_FAILED(rv = rdf_ContainerRemoveElement(mDB, parent, oldChild))) {
                NS_ERROR("unable to remove old child from container");
                return rv;
            }

            // ...and add the new child to the collection at the old child's index
            if (NS_FAILED(rv = rdf_ContainerInsertElementAt(mDB, parent, newChild, index))) {
                NS_ERROR("unable to add new child to container");
                return rv;
            }
        }
    }

    return NS_OK;
}



NS_IMETHODIMP
RDFXULBuilderImpl::OnRemoveChild(nsIDOMNode* aParent, nsIDOMNode* aOldChild)
{
    NS_PRECONDITION(aParent != nsnull, "null ptr");
    if (!aParent)
        return NS_ERROR_NULL_POINTER;

    NS_PRECONDITION(aOldChild != nsnull, "null ptr");
    if (!aOldChild)
        return NS_ERROR_NULL_POINTER;

    nsresult rv;

    // XXX may want to check _here_ to make sure that aOldChild is
    // actually a child of aParent...

    // XXX If aOldChild doesn't have a resource, then somebody is
    // removing a non-RDF element from aParent. This probably isn't a
    // big deal, but panic for now.
    nsCOMPtr<nsIRDFResource> oldChild;
    if (NS_FAILED(rv = GetDOMNodeResource(aOldChild, getter_AddRefs(oldChild)))) {
        NS_ERROR("new child doesn't have a resource");
        return rv;
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

            // ...then remove it from the container
            if (NS_FAILED(rv = rdf_ContainerRemoveElement(mDB, parent, oldChild))) {
                NS_ERROR("unable to insert new element into container");
                return rv;
            }
        }
    }

    return NS_OK;
}



NS_IMETHODIMP
RDFXULBuilderImpl::OnAppendChild(nsIDOMNode* aParent, nsIDOMNode* aNewChild)
{
    NS_PRECONDITION(aParent != nsnull, "null ptr");
    if (!aParent)
        return NS_ERROR_NULL_POINTER;

    NS_PRECONDITION(aNewChild != nsnull, "null ptr");
    if (!aNewChild)
        return NS_ERROR_NULL_POINTER;

    nsresult rv;

    // XXX If aNewChild doesn't have a resource, then somebody is
    // appending a non-RDF element into aParent. Panic for now.
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

            // ...then append it to the container
            if (NS_FAILED(rv = rdf_ContainerAppendElement(mDB, parent, newChild))) {
                NS_ERROR("unable to insert new element into container");
                return rv;
            }
        }
    }

    return NS_OK;
}


////////////////////////////////////////////////////////////////////////
// nsIDOMElementObserver interface


NS_IMETHODIMP
RDFXULBuilderImpl::OnSetAttribute(nsIDOMElement* aElement, const nsString& aName, const nsString& aValue)
{
    nsresult rv;

    nsCOMPtr<nsIRDFResource> resource;
    if (NS_FAILED(rv = GetDOMNodeResource(aElement, getter_AddRefs(resource)))) {
        // XXX it's not a resource element, so there's no assertions
        // we need to make on the back-end. Should we just do the
        // update?
        return NS_OK;
    }

    // Make sure it's a XUL element; otherwise, we really don't have
    // any business with the attribute.
    PRBool isXULElement;
    if (NS_FAILED(rv = mDB->HasAssertion(resource,
                                         kRDF_instanceOf,
                                         kXUL_element,
                                         PR_TRUE,
                                         &isXULElement))) {
        NS_ERROR("unable to determine if element is a XUL element");
        return rv;
    }

    if (! isXULElement)
        return NS_OK;

    // Okay, so it _is_ a XUL element. Deal with it.

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

    // Check for "special" properties that may wreak havoc on the
    // content model.
    if ((nameSpaceID == kNameSpaceID_None) && (nameAtom.get() == kIdAtom)) {
        // They're changing the ID of the element.

        // XXX Punt for now.
        NS_WARNING("ignoring id change on XUL element");
        return NS_OK;
    }
    else if (nameSpaceID == kNameSpaceID_RDF) {
        if (nameAtom.get() == kDataSourcesAtom) {
            NS_NOTYETIMPLEMENTED("can't change the data sources yet");
            return NS_ERROR_NOT_IMPLEMENTED;
        }

        // XXX we should probably just ignore any changes to rdf: attribute
        NS_WARNING("changing an rdf: attribute; this is probably not a good thing");
        return NS_OK;
    }


    // If we get here, it's a vanilla property that we need to go into
    // the RDF graph to update. So, build an RDF resource from the
    // property name...
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
RDFXULBuilderImpl::OnRemoveAttribute(nsIDOMElement* aElement, const nsString& aName)
{
    nsresult rv;

    nsCOMPtr<nsIRDFResource> resource;
    if (NS_FAILED(rv = GetDOMNodeResource(aElement, getter_AddRefs(resource)))) {
        // XXX it's not a resource element, so there's no assertions
        // we need to make on the back-end. Should we just do the
        // update?
        return NS_OK;
    }

    // Make sure it's a XUL element; otherwise, we really don't have
    // any business with the attribute.
    PRBool isXULElement;
    if (NS_SUCCEEDED(rv = mDB->HasAssertion(resource,
                                            kRDF_instanceOf,
                                            kXUL_element,
                                            PR_TRUE,
                                            &isXULElement))
        && isXULElement) {
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
    }
    return NS_OK;
}

NS_IMETHODIMP
RDFXULBuilderImpl::OnSetAttributeNode(nsIDOMElement* aElement, nsIDOMAttr* aNewAttr)
{
    NS_NOTYETIMPLEMENTED("write me!");
    return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
RDFXULBuilderImpl::OnRemoveAttributeNode(nsIDOMElement* aElement, nsIDOMAttr* aOldAttr)
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

        PRInt32 count;
        aElement->ChildCount(count);
        while (--count >= 0) {
            nsCOMPtr<nsIContent> child;
            aElement->ChildAt(count, *getter_AddRefs(child));

            // XXX can't identify HTML elements right now.
            if (IsHTMLElement(child))
                continue;

            nsCOMPtr<nsIRDFResource> elementResource;
            rv = GetElementResource(child, getter_AddRefs(elementResource));

            if (resource != elementResource)
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

    NS_ASSERTION(rv != NS_RDF_NO_VALUE, "no node type");
    if (rv == NS_RDF_NO_VALUE)
        return NS_ERROR_UNEXPECTED;
        

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

    // Force the document to be set _here_. Many of the
    // AppendChildTo() implementations do not recursively ensure
    // that the child's doc is the same as the parent's.
    nsCOMPtr<nsIDocument> doc;
    if (NS_FAILED(rv = mDocument->QueryInterface(kIDocumentIID, getter_AddRefs(doc)))) {
        NS_ERROR("uh, this isn't a document!");
        return rv;
    }

    // Make sure our ID is set. Unlike XUL elements, we want to make sure
    // that our ID is relative if possible.
    //
    // XXX Why? Is this for supporting inline style or something?
    nsXPIDLCString uri;
    if (NS_FAILED(rv = aResource->GetValue( getter_Copies(uri) )))
        return rv;

    // XXX Won't somebody just cram this back into a fully qualified
    // URI somewhere?
    nsString fullURI(uri);
    nsIURL* docURL = nsnull;
    doc->GetBaseURL(docURL);
    if (docURL) {
        const char* url;
        docURL->GetSpec(&url);
        rdf_PossiblyMakeRelative(url, fullURI);
        NS_RELEASE(docURL);
    }
       
    if (NS_FAILED(rv = element->SetAttribute(kNameSpaceID_None, kIdAtom, fullURI, PR_FALSE))) {
        NS_ERROR("unable to set element's ID");
        return rv;
    }

    // Set the document. N.B. that we do this _after_ setting up the
    // ID so that we get the element hashed correctly in the
    // document's resource-to-element map.
    if (NS_FAILED(rv = element->SetDocument(doc, PR_FALSE))) {
        NS_ERROR("couldn't set document on the element");
        return rv;
    }

    // XXX Do we add ourselves to the map here? If so, this is non-dynamic! 
    // If the HTML element's ID changes, we won't adjust
    // the map.  XUL elements do this in the SetAttribute call, but we don't have
    // access to that for HTML elements.  Should they even be in the map?
    // For now the answer is NO.

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
    while (1) {
        rv = properties->Advance();
        if (NS_FAILED(rv))
            return rv;

        if (rv == NS_RDF_CURSOR_EMPTY)
            break;

        nsCOMPtr<nsIRDFResource> property;

        if (NS_FAILED(rv = properties->GetLabel(getter_AddRefs(property)))) {
            NS_ERROR("unable to get property from cursor");
            return rv;
        }

        // These are special beacuse they're used to specify the tree
        // structure of the XUL: ignore them b/c they're not attributes
        if ((property.get() == kRDF_instanceOf) ||
            (property.get() == kRDF_nextVal) ||
            (property.get() == kRDF_type) ||
            (rdf_IsOrdinalProperty(property)))
            continue;

        // For each property, get its value: this will be the value of
        // the new attribute.
        nsCOMPtr<nsIRDFNode> value;
        if (NS_FAILED(rv = mDB->GetTarget(aResource, property, PR_TRUE, getter_AddRefs(value)))) {
            NS_ERROR("unable to get value for property");
            return rv;
        }

        NS_ASSERTION(rv != NS_RDF_NO_VALUE, "null value in cursor");
        if (rv == NS_RDF_NO_VALUE)
            continue;

        // Add the attribute to the newly constructed element
        if (NS_FAILED(rv = AddAttribute(element, property, value))) {
            NS_ERROR("unable to add attribute to element");
            return rv;
        }
    }

    // Create the children NOW, because we can't do it lazily.
    if (NS_FAILED(rv = CreateHTMLContents(element, aResource))) {
        NS_ERROR("error creating child contents");
        return rv;
    }

    if (NS_FAILED(rv = element->QueryInterface(kIContentIID, (void**) aResult))) {
        NS_ERROR("unable to get nsIContent interface");
        return rv;
    }

    // The observes relationship has to be hooked up here, since the children
    // were already built.  We'll miss out on it if we don't plug in here.
    // Now that the contents have been created, perform broadcaster
    // hookups if any of the children are observes nodes.
    // XXX: Initial sync-up doesn't work, since no document observer exists
    // yet.
    PRInt32 childCount;
    element->ChildCount(childCount);
    for (PRInt32 j = 0; j < childCount; j++)
    {
        nsIContent* childContent = nsnull;
        element->ChildAt(j, childContent);
      
        if (!childContent)
          break;

        nsIAtom* tag = nsnull;
        childContent->GetTag(tag);

        if (!tag)
          break;

        nsString tagName;
        tag->ToString(tagName);

        if (tagName == "observes")
        {
            // Find the node that we're supposed to be
            // observing and perform the hookup.
            nsString elementValue;
            nsString attributeValue;
            nsCOMPtr<nsIDOMElement> domContent;
            domContent = do_QueryInterface(childContent);

            domContent->GetAttribute("element",
                                     elementValue);
            
            domContent->GetAttribute("attribute",
                                     attributeValue);

            nsIDOMElement* domElement = nsnull;
            nsCOMPtr<nsIDOMXULDocument> xulDoc;
            xulDoc = do_QueryInterface(doc);
            
            if (xulDoc)
              xulDoc->GetElementById(elementValue, &domElement);
            
            if (!domElement)
              break;

            // We have a DOM element to bind to.  Add a broadcast
            // listener to that element, but only if it's a XUL element.
            // XXX: Handle context nodes.
            nsCOMPtr<nsIDOMElement> listener( do_QueryInterface(element) );
            nsCOMPtr<nsIDOMXULElement> broadcaster( do_QueryInterface(domElement) );
            if (listener)
            {
                broadcaster->AddBroadcastListener(attributeValue,
                                                  listener);
            }

            NS_RELEASE(domElement);
        }

        NS_RELEASE(childContent);
        NS_RELEASE(tag);
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

    while (1) {
        rv = children->Advance();
        if (NS_FAILED(rv))
            return rv;

        if (rv == NS_RDF_CURSOR_EMPTY)
            break;

        nsCOMPtr<nsIRDFNode> child;
        if (NS_FAILED(rv = children->GetTarget(getter_AddRefs(child)))) {
            NS_ERROR("error reading cursor");
            return rv;
        }

        if (NS_FAILED(AppendChild(aElement, child))) {
            NS_ERROR("problem appending child to content model");
            return rv;
        }
    }

    return NS_OK;
}



nsresult
RDFXULBuilderImpl::CreateXULElement(nsIRDFResource* aResource,
                                    PRInt32 aNameSpaceID,
                                    nsIAtom* aTag,
                                    nsIContent** aResult)
{
    nsresult rv;

    nsCOMPtr<nsIContent> element;
    if (NS_FAILED(rv = CreateResourceElement(aNameSpaceID,
                                             aTag,
                                             aResource,
                                             getter_AddRefs(element)))) {
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
    while (1) {
        rv = properties->Advance();
        if (NS_FAILED(rv))
            return rv;

        if (rv == NS_RDF_CURSOR_EMPTY)
            break;

        nsCOMPtr<nsIRDFResource> property;

        if (NS_FAILED(rv = properties->GetLabel(getter_AddRefs(property)))) {
            NS_ERROR("unable to get property from cursor");
            return rv;
        }

        // These are special beacuse they're used to specify the tree
        // structure of the XUL: ignore them b/c they're not attributes
        if ((property.get() == kRDF_instanceOf) ||
            (property.get() == kRDF_nextVal) ||
            (property.get() == kRDF_type) ||
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

    // Make it a container so that its contents get recursively
    // generated on-demand.
    if (NS_FAILED(rv = element->SetAttribute(kNameSpaceID_RDF, kContainerAtom, "true", PR_FALSE))) {
        NS_ERROR("unable to make element a container");
        return rv;
    }

    // There are some tags that we need to pay extra-special attention to...
    if (aTag == kTreeAtom || aTag == kMenuAtom || aTag == kMenuBarAtom || 
        aTag == kToolbarAtom) {
        nsAutoString dataSources;
        if (NS_CONTENT_ATTR_HAS_VALUE ==
            element->GetAttribute(kNameSpaceID_None,
                                  kDataSourcesAtom,
                                  dataSources)) {

            nsCID builderCID;
            if (aTag == kTreeAtom)
                builderCID = kRDFTreeBuilderCID;
            else if (aTag == kMenuAtom || aTag == kMenuBarAtom)
                builderCID = kRDFMenuBuilderCID;
            else if (aTag == kToolbarAtom)
                builderCID = kRDFToolbarBuilderCID;

            rv = CreateBuilder(builderCID, element, dataSources);
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
        nsXPIDLCString uri;
        resource->GetValue( getter_Copies(uri) );
        rv = aElement->SetAttribute(nameSpaceID, tag, (const char*) uri, PR_TRUE);
    }
    else if (NS_SUCCEEDED(rv = aValue->QueryInterface(kIRDFLiteralIID,
                                                      (void**) getter_AddRefs(literal)))) {
        nsXPIDLString s;
        literal->GetValue( getter_Copies(s) );
        rv = aElement->SetAttribute(nameSpaceID, tag, (const PRUnichar*) s, PR_TRUE);
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
        // certain namespaces. We'll just assume that, if the
        // attribute _isn't_ in one of these namespaces, it never got
        // added, so removing it is a no-op.
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

    // XXX At this point, we may want to be extra clever, and see if
    // the Unassert() actually "exposed" some other multiattribute...
    rv = aElement->UnsetAttribute(nameSpaceID, tag, PR_TRUE);
    NS_ASSERTION(NS_SUCCEEDED(rv), "problem unsetting attribute");
    return rv;
}

nsresult
RDFXULBuilderImpl::CreateBuilder(const nsCID& aBuilderCID, nsIContent* aElement,
                                 const nsString& aDataSources)
{
    nsresult rv;

    // construct a new builder
    nsCOMPtr<nsIRDFContentModelBuilder> builder;
    if (NS_FAILED(rv = nsComponentManager::CreateInstance(aBuilderCID,
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
    if (NS_FAILED(rv = nsComponentManager::CreateInstance(kRDFCompositeDataSourceCID,
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

    return GetElementResource(element, aResource);
}

nsresult
RDFXULBuilderImpl::CreateResourceElement(PRInt32 aNameSpaceID,
                                         nsIAtom* aTag,
                                         nsIRDFResource* aResource,
                                         nsIContent** aResult)
{
    nsresult rv;

    nsCOMPtr<nsIContent> result;
    if (NS_FAILED(rv = NS_NewRDFElement(aNameSpaceID, aTag, getter_AddRefs(result))))
        return rv;

    // Set the element's ID. We do this _before_ we insert the element
    // into the document so that it gets properly hashed into the
    // document's resource-to-element map.
    nsXPIDLCString uri;
    if (NS_FAILED(rv = aResource->GetValue( getter_Copies(uri) )))
        return rv;

    if (NS_FAILED(rv = result->SetAttribute(kNameSpaceID_None, kIdAtom, (const char*) uri, PR_FALSE)))
        return rv;

    // Set the document for this element.
    nsCOMPtr<nsIDocument> document( do_QueryInterface(mDocument) );
    result->SetDocument(document, PR_FALSE);

    *aResult = result;
    NS_ADDREF(*aResult);
    return NS_OK;
}


nsresult
RDFXULBuilderImpl::GetResource(PRInt32 aNameSpaceID,
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


nsresult
RDFXULBuilderImpl::GetElementResource(nsIContent* aElement, nsIRDFResource** aResult)
{
    // Perform a reverse mapping from an element in the content model
    // to an RDF resource.
    nsresult rv;
    nsAutoString uri;
    if (NS_FAILED(rv = aElement->GetAttribute(kNameSpaceID_None,
                                              kIdAtom,
                                              uri))) {
        NS_ERROR("severe error retrieving attribute");
        return rv;
    }

    if (rv != NS_CONTENT_ATTR_HAS_VALUE) {
        NS_ERROR("tree element has no ID");
        return NS_ERROR_UNEXPECTED;
    }

    // Since the element will store its ID attribute as a document-relative value,
    // we may need to qualify it first...
    NS_ASSERTION(mDocument != nsnull, "builder has no document -- can't fully qualify URI");
    if (nsnull != mDocument) {
        nsCOMPtr<nsIDocument> doc( do_QueryInterface(mDocument) );
        NS_ASSERTION(doc, "doesn't support nsIDocument");
        if (doc) {
          nsIURL* docURL = nsnull;
          doc->GetBaseURL(docURL);
          if (docURL) {
              const char* url;
              docURL->GetSpec(&url);
              rdf_PossiblyMakeAbsolute(url, uri);
              NS_RELEASE(docURL);
          }
        }
    }

    if (NS_FAILED(rv = gRDFService->GetUnicodeResource(uri, aResult))) {
        NS_ERROR("unable to create resource");
        return rv;
    }

    return NS_OK;
}
