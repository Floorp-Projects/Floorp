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

  1) Implement the remainder of the DOM methods.

*/

// Note the ALPHABETICAL ORDER. Heed it. Or die.
#include "nsCOMPtr.h"
#include "nsDebug.h"
#include "nsIAtom.h"
#include "nsIContent.h"
#include "nsIDOMNSDocument.h"
#include "nsIContentViewerContainer.h"
#include "nsIDOMElement.h"
#include "nsIDOMNode.h"
#include "nsIDOMText.h"
#include "nsIDOMXULDocument.h"
#include "nsIDOMXULElement.h"
#include "nsIFormControl.h"
#include "nsIDOMHTMLFormElement.h"
#include "nsIDocument.h"
#include "nsIDocumentLoader.h"
#include "nsINameSpace.h"
#include "nsINameSpaceManager.h"
#include "nsINameSpaceManager.h"
#include "nsIRDFCompositeDataSource.h"
#include "nsIRDFContainer.h"
#include "nsIRDFContainerUtils.h"
#include "nsIRDFContentModelBuilder.h"
#include "nsIRDFDocument.h"
#include "nsIRDFNode.h"
#include "nsIRDFObserver.h"
#include "nsIRDFService.h"
#include "nsIServiceManager.h"
#include "nsIServiceManager.h"
#include "nsIStreamListener.h"
#include "nsISupportsArray.h"
#include "nsIURL.h"
#include "nsIWebShell.h"
#include "nsIXMLContent.h"
#include "nsIXULChildDocument.h"
#include "nsIXULDocumentInfo.h"
#include "nsIXULParentDocument.h"
#include "nsLayoutCID.h"
#include "nsIHTMLElementFactory.h"
#include "nsRDFCID.h"
#include "nsIXULContentUtils.h"
#include "nsString.h"
#include "nsXPIDLString.h"
#include "prlog.h"
#include "rdf.h"
#include "rdfutil.h"
#include "nsIXULKeyListener.h"
#include "nsIDOMEventListener.h"
#include "nsIEventListenerManager.h"
#include "nsIDOMEventReceiver.h"
#include "nsIXULPopupListener.h"

// XXX These are needed as scaffolding until we get to a more
// DOM-based solution.
#include "nsIHTMLContent.h"

////////////////////////////////////////////////////////////////////////

static NS_DEFINE_IID(kIRDFContentModelBuilderIID, NS_IRDFCONTENTMODELBUILDER_IID);
static NS_DEFINE_IID(kIRDFCompositeDataSourceIID, NS_IRDFCOMPOSITEDATASOURCE_IID);
static NS_DEFINE_IID(kIRDFLiteralIID,             NS_IRDFLITERAL_IID);
static NS_DEFINE_IID(kIRDFObserverIID,            NS_IRDFOBSERVER_IID);
static NS_DEFINE_IID(kIRDFResourceIID,            NS_IRDFRESOURCE_IID);
static NS_DEFINE_IID(kIRDFServiceIID,             NS_IRDFSERVICE_IID);
static NS_DEFINE_IID(kISupportsIID,               NS_ISUPPORTS_IID);
static NS_DEFINE_IID(kIXULDocumentInfoIID,        NS_IXULDOCUMENTINFO_IID);
static NS_DEFINE_IID(kIXULKeyListenerIID,         NS_IXULKEYLISTENER_IID);
static NS_DEFINE_IID(kIDOMElementIID,             NS_IDOMELEMENT_IID);
static NS_DEFINE_IID(kIDOMEventReceiverIID,       NS_IDOMEVENTRECEIVER_IID);
static NS_DEFINE_IID(kIXULPopupListenerIID,       NS_IXULPOPUPLISTENER_IID);

static NS_DEFINE_CID(kHTMLElementFactoryCID, NS_HTML_ELEMENT_FACTORY_CID);
static NS_DEFINE_CID(kNameSpaceManagerCID,        NS_NAMESPACEMANAGER_CID);
static NS_DEFINE_CID(kRDFCompositeDataSourceCID,  NS_RDFCOMPOSITEDATASOURCE_CID);
static NS_DEFINE_CID(kRDFServiceCID,              NS_RDFSERVICE_CID);
static NS_DEFINE_CID(kRDFContainerUtilsCID,       NS_RDFCONTAINERUTILS_CID);
static NS_DEFINE_CID(kXULTemplateBuilderCID,      NS_XULTEMPLATEBUILDER_CID);
static NS_DEFINE_CID(kXULDocumentCID,             NS_XULDOCUMENT_CID);
static NS_DEFINE_CID(kXULDocumentInfoCID,         NS_XULDOCUMENTINFO_CID);
static NS_DEFINE_CID(kXULKeyListenerCID,          NS_XULKEYLISTENER_CID);
static NS_DEFINE_CID(kXULPopupListenerCID,          NS_XULPOPUPLISTENER_CID);
static NS_DEFINE_CID(kXULContentUtilsCID,      NS_XULCONTENTUTILS_CID);

////////////////////////////////////////////////////////////////////////

static const char kRDFNameSpaceURI[] = RDF_NAMESPACE_URI;

// XXX This is sure to change. Copied from mozilla/layout/xul/content/src/nsXULAtoms.cpp
#define XUL_NAMESPACE_URI "http://www.mozilla.org/keymaster/gatekeeper/there.is.only.xul"
static const char kXULNameSpaceURI[] = XUL_NAMESPACE_URI;

#ifdef PR_LOGGING
static PRLogModuleInfo* gLog;
#endif

////////////////////////////////////////////////////////////////////////

class RDFXULBuilderImpl : public nsIRDFContentModelBuilder
{
private:
    nsIRDFDocument*                     mDocument; // [WEAK]

    // We are an observer of the composite datasource. The cycle is
    // broken by out-of-band SetDataBase(nsnull) call when document is
    // destroyed.
    nsCOMPtr<nsIRDFCompositeDataSource> mDB;

    nsCOMPtr<nsIContent>                mRoot;
    nsCOMPtr<nsIHTMLElementFactory>     mHTMLElementFactory;

    // pseudo-constants and shared services
    static PRInt32 gRefCnt;
    static nsIRDFService*        gRDFService;
    static nsIRDFContainerUtils* gRDFContainerUtils;
    static nsINameSpaceManager*  gNameSpaceManager;
    static nsIXULContentUtils*   gXULUtils;

    static PRInt32  kNameSpaceID_RDF;
    static PRInt32  kNameSpaceID_XUL;

    static nsIAtom* kDataSourcesAtom;
    static nsIAtom* kIdAtom;
    static nsIAtom* kKeysetAtom;
    static nsIAtom* kRefAtom;
    static nsIAtom* kTemplateAtom;
    static nsIAtom* kXMLNSAtom;
    
    static nsIRDFResource* kRDF_instanceOf;
    static nsIRDFResource* kRDF_nextVal;
    static nsIRDFResource* kRDF_child; // XXX needs to become kNC_child
    static nsIRDFResource* kXUL_element;
    static nsIRDFResource* kXUL_tag;

    RDFXULBuilderImpl();
    nsresult Init();

    virtual ~RDFXULBuilderImpl();

    friend nsresult
    NS_NewRDFXULBuilder(nsIRDFContentModelBuilder** aResult);

public:
    // nsISupports interface
    NS_DECL_ISUPPORTS

    // nsIRDFContentModelBuilder interface
    NS_IMETHOD SetDocument(nsIRDFDocument* aDocument);
    NS_IMETHOD SetDataBase(nsIRDFCompositeDataSource* aDataBase);
    NS_IMETHOD GetDataBase(nsIRDFCompositeDataSource** aDataBase);
    NS_IMETHOD CreateRootContent(nsIRDFResource* aResource);
    NS_IMETHOD SetRootContent(nsIContent* aResource);
    NS_IMETHOD CreateContents(nsIContent* aElement);
    NS_IMETHOD OpenContainer(nsIContent* aElement);
    NS_IMETHOD CloseContainer(nsIContent* aElement);
    NS_IMETHOD RebuildContainer(nsIContent* aElement);
    NS_IMETHOD CreateElement(PRInt32 aNameSpaceID,
                             nsIAtom* aTagName,
                             nsIRDFResource* aResource,
                             nsIContent** aResult);


    // Implementation methods
    nsresult CreateChildrenFromGraph(nsINameSpace* aNameSpace,
                                     nsIRDFResource* aResource,
                                     nsIContent* aElement,
                                     PRBool aForceIDAttr);

    nsresult CreateElement(nsINameSpace* aContainingNameSpace,
                           nsIRDFResource* aResource,
                           PRBool aForceIDAttr,
                           nsIContent** aResult);

    nsresult CreateHTMLElement(nsINameSpace* aContainingNameSpace,
                               nsIRDFResource* aResource,
                               nsIAtom* aTagName,
                               PRBool aForceIDAttr,
                               nsIContent** aResult);

    nsresult CreateXULElement(nsINameSpace* aContainingNameSpace,
                              nsIRDFResource* aResource,
                              PRInt32 aNameSpaceID,
                              nsIAtom* aTagName,
                              PRBool aForceIDAttr,
                              nsIContent** aResult);

    nsresult
    GetContainingNameSpace(nsIContent* aElement, nsINameSpace** aNameSpace);

    PRBool
    IsHTMLElement(nsIContent* aElement);

    PRBool
    IsAttributeProperty(nsIRDFResource* aProperty);

    nsresult AddAttribute(nsIContent* aElement,
                          nsIRDFResource* aProperty,
                          nsIRDFNode* aValue);

    nsresult RemoveAttribute(nsIContent* aElement,
                             nsIRDFResource* aProperty,
                             nsIRDFNode* aValue);

    nsresult CreateTemplateBuilder(nsIContent* aElement,
                                   const nsString& aDataSources,
                                   nsIRDFContentModelBuilder** aResult);

    nsresult
    GetRDFResourceFromXULElement(nsIDOMNode* aNode, nsIRDFResource** aResult);

    nsresult
    GetGraphNodeForXULElement(nsIDOMNode* aNode, nsIRDFNode** aResult);

    nsresult
    GetResource(PRInt32 aNameSpaceID,
                nsIAtom* aNameAtom,
                nsIRDFResource** aResource);

    nsresult
    MakeProperty(PRInt32 aNameSpaceID, nsIAtom* aTagName, nsIRDFResource** aResult);

    nsresult
    GetParentNodeWithHTMLHack(nsIDOMNode* aNode, nsCOMPtr<nsIDOMNode>* aResult);

    PRBool
    IsXULElement(nsIContent* aElement);
};

////////////////////////////////////////////////////////////////////////
// Pseudo-constants

PRInt32              RDFXULBuilderImpl::gRefCnt;
nsIRDFService*       RDFXULBuilderImpl::gRDFService;
nsIRDFContainerUtils* RDFXULBuilderImpl::gRDFContainerUtils;
nsINameSpaceManager* RDFXULBuilderImpl::gNameSpaceManager;
nsIXULContentUtils*  RDFXULBuilderImpl::gXULUtils;

PRInt32         RDFXULBuilderImpl::kNameSpaceID_RDF = kNameSpaceID_Unknown;
PRInt32         RDFXULBuilderImpl::kNameSpaceID_XUL = kNameSpaceID_Unknown;

nsIAtom*        RDFXULBuilderImpl::kDataSourcesAtom;
nsIAtom*        RDFXULBuilderImpl::kIdAtom;
nsIAtom*        RDFXULBuilderImpl::kKeysetAtom;
nsIAtom*        RDFXULBuilderImpl::kRefAtom;
nsIAtom*        RDFXULBuilderImpl::kTemplateAtom;
nsIAtom*        RDFXULBuilderImpl::kXMLNSAtom;

nsIRDFResource* RDFXULBuilderImpl::kRDF_instanceOf;
nsIRDFResource* RDFXULBuilderImpl::kRDF_nextVal;
nsIRDFResource* RDFXULBuilderImpl::kRDF_child;
nsIRDFResource* RDFXULBuilderImpl::kXUL_element;
nsIRDFResource* RDFXULBuilderImpl::kXUL_tag;

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

    nsresult rv;
    rv = builder->Init();
    if (NS_FAILED(rv)) {
        delete builder;
        return rv;
    }

    NS_ADDREF(builder);
    *result = builder;
    return NS_OK;
}


RDFXULBuilderImpl::RDFXULBuilderImpl(void)
    : mDocument(nsnull)
{
    NS_INIT_REFCNT();
}


nsresult
RDFXULBuilderImpl::Init()
{
    nsresult rv;

    if (gRefCnt++ == 0) {
        rv = nsComponentManager::CreateInstance(kNameSpaceManagerCID,
                                                nsnull,
                                                nsCOMTypeInfo<nsINameSpaceManager>::GetIID(),
                                                (void**) &gNameSpaceManager);
        if (NS_FAILED(rv)) return rv;

        rv = gNameSpaceManager->RegisterNameSpace(kXULNameSpaceURI, kNameSpaceID_XUL);
        NS_ASSERTION(NS_SUCCEEDED(rv), "unable to register XUL namespace");
        if (NS_FAILED(rv)) return rv;

        rv = gNameSpaceManager->RegisterNameSpace(kRDFNameSpaceURI, kNameSpaceID_RDF);
        NS_ASSERTION(NS_SUCCEEDED(rv), "unable to register RDF namespace");
        if (NS_FAILED(rv)) return rv;

        kDataSourcesAtom          = NS_NewAtom("datasources");
        kIdAtom                   = NS_NewAtom("id");
        kKeysetAtom               = NS_NewAtom("keyset");
        kRefAtom                  = NS_NewAtom("ref");
        kTemplateAtom             = NS_NewAtom("template");
        kXMLNSAtom                = NS_NewAtom("xmlns");
        
        rv = nsServiceManager::GetService(kRDFServiceCID,
                                          kIRDFServiceIID,
                                          (nsISupports**) &gRDFService);

        NS_ASSERTION(NS_SUCCEEDED(rv), "unable to get RDF service");
        if (NS_FAILED(rv)) return rv;

        gRDFService->GetResource(RDF_NAMESPACE_URI "instanceOf", &kRDF_instanceOf);
        gRDFService->GetResource(RDF_NAMESPACE_URI "nextVal",    &kRDF_nextVal);
        gRDFService->GetResource(RDF_NAMESPACE_URI "child",      &kRDF_child);
        gRDFService->GetResource(XUL_NAMESPACE_URI "#element",   &kXUL_element);
        gRDFService->GetResource(XUL_NAMESPACE_URI "#tag",       &kXUL_tag);

        rv = nsServiceManager::GetService(kRDFContainerUtilsCID,
                                          nsIRDFContainerUtils::GetIID(),
                                          (nsISupports**) &gRDFContainerUtils);

        if (NS_FAILED(rv)) return rv;

        rv = nsServiceManager::GetService(kXULContentUtilsCID,
                                          nsCOMTypeInfo<nsIXULContentUtils>::GetIID(),
                                          (nsISupports**) &gXULUtils);

        if (NS_FAILED(rv)) return rv;
    }

    rv = nsComponentManager::CreateInstance(kHTMLElementFactoryCID,
                                            nsnull,
                                            nsIHTMLElementFactory::GetIID(),
                                            getter_AddRefs(mHTMLElementFactory));
    if (NS_FAILED(rv)) return rv;

#ifdef PR_LOGGING
    if (! gLog)
        gLog = PR_NewLogModule("nsRDFXULBuilder");
#endif

    return NS_OK;
}

RDFXULBuilderImpl::~RDFXULBuilderImpl(void)
{
    // NS_IF_RELEASE(mDocument) not refcounted

    if (--gRefCnt == 0) {
        if (gRDFService) {
            nsServiceManager::ReleaseService(kRDFServiceCID, gRDFService);
            gRDFService = nsnull;
        }

        if (gRDFContainerUtils) {
            nsServiceManager::ReleaseService(kRDFContainerUtilsCID, gRDFContainerUtils);
            gRDFContainerUtils = nsnull;
        }

        if (gXULUtils) {
            nsServiceManager::ReleaseService(kXULContentUtilsCID, gXULUtils);
            gXULUtils = nsnull;
        }

        NS_IF_RELEASE(gNameSpaceManager);

        NS_IF_RELEASE(kRDF_instanceOf);
        NS_IF_RELEASE(kRDF_nextVal);
        NS_IF_RELEASE(kRDF_child);
        NS_IF_RELEASE(kXUL_element);
        NS_IF_RELEASE(kXUL_tag);

        NS_IF_RELEASE(kDataSourcesAtom);
        NS_IF_RELEASE(kIdAtom);
        NS_IF_RELEASE(kKeysetAtom);
        NS_IF_RELEASE(kRefAtom);
        NS_IF_RELEASE(kTemplateAtom);
        NS_IF_RELEASE(kXMLNSAtom);
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
	// note: document can now be null to indicate its going away
    mDocument = aDocument; // not refcounted
    return NS_OK;
}

NS_IMETHODIMP
RDFXULBuilderImpl::SetDataBase(nsIRDFCompositeDataSource* aDataBase)
{
    // We don't add ourselves as an observer. The XUL datasource is
    // assumed to be immutable.
    mDB = dont_QueryInterface(aDataBase);

    return NS_OK;
}

NS_IMETHODIMP
RDFXULBuilderImpl::GetDataBase(nsIRDFCompositeDataSource** aDataBase)
{
    NS_PRECONDITION(aDataBase != nsnull, "null ptr");
    if (! aDataBase)
        return NS_ERROR_NULL_POINTER;

    *aDataBase = mDB;
    NS_ADDREF(*aDataBase);
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

    nsCOMPtr<nsINameSpace> rootNameSpace;
    rv = gNameSpaceManager->CreateRootNameSpace(*getter_AddRefs(rootNameSpace));
    if (NS_FAILED(rv)) return rv;

    rv = CreateElement(rootNameSpace, aResource, PR_FALSE, getter_AddRefs(mRoot));
    NS_ASSERTION(NS_SUCCEEDED(rv), "unable to create root element");
    if (NS_FAILED(rv)) return rv;

    // Now set it as the document's root content
    nsCOMPtr<nsIDocument> doc = do_QueryInterface(mDocument);
    NS_ASSERTION(doc, "couldn't get nsIDocument interface");
    if (! doc)
        return NS_ERROR_UNEXPECTED;

    doc->SetRootContent(mRoot);

    // Always insert a hidden form as the very first child of the window.
    // Create a new form element.
    nsCOMPtr<nsIHTMLContent> newElement;
    if (!mHTMLElementFactory) {
        rv = nsComponentManager::CreateInstance(kHTMLElementFactoryCID,
                                                nsnull,
                                                nsIHTMLElementFactory::GetIID(),
                                                (void**) &mHTMLElementFactory);
        if (NS_FAILED(rv)) {
            return rv;
        }
    }
    rv = mHTMLElementFactory->CreateInstanceByTag(nsAutoString("form"),
                                                  getter_AddRefs(newElement));
    NS_ASSERTION(NS_SUCCEEDED(rv), "unable to create HTML element");
    if (NS_FAILED(rv)) return rv;

    nsCOMPtr<nsIDOMHTMLFormElement> htmlFormElement = do_QueryInterface(newElement);
    if (htmlFormElement) {
        nsCOMPtr<nsIContent> content = do_QueryInterface(htmlFormElement);
        if (content) {
            // XXX Would like to make this anonymous, but still need
            // the form's frame to get built. For now make it explicit.
            mRoot->InsertChildAt(content, 0, PR_FALSE); 
        }
        mDocument->SetForm(htmlFormElement);
    }

    // Now build up all of the document's children.
	nsCOMPtr<nsINameSpace> nameSpace;

    nsCOMPtr<nsIXMLContent> xml( do_QueryInterface(mRoot) );
    if (xml) {
        rv = xml->GetContainingNameSpace(*getter_AddRefs(nameSpace));
        NS_ASSERTION(NS_SUCCEEDED(rv), "unable to get containing namespace");
        if (NS_FAILED(rv)) return rv;
    }
    else {
        nameSpace = rootNameSpace;
    }

    rv = CreateChildrenFromGraph(nameSpace, aResource, mRoot, PR_FALSE);
    if (NS_FAILED(rv)) return rv;

    return NS_OK;
}


NS_IMETHODIMP
RDFXULBuilderImpl::SetRootContent(nsIContent* aElement)
{
    mRoot = dont_QueryInterface(aElement);
    return NS_OK;
}


NS_IMETHODIMP
RDFXULBuilderImpl::CreateContents(nsIContent* aElement)
{
    return NS_OK;
}


NS_IMETHODIMP
RDFXULBuilderImpl::OpenContainer(nsIContent* aContainer)
{
    return NS_OK;
}


NS_IMETHODIMP
RDFXULBuilderImpl::CloseContainer(nsIContent* aContainer)
{
    return NS_OK;
}


NS_IMETHODIMP
RDFXULBuilderImpl::RebuildContainer(nsIContent* aContainer)
{
    return NS_OK;
}


NS_IMETHODIMP
RDFXULBuilderImpl::CreateElement(PRInt32 aNameSpaceID,
                                 nsIAtom* aTagName,
                                 nsIRDFResource* aResource,
                                 nsIContent** aResult)
{
    nsresult rv;

    nsCOMPtr<nsIContent> result;
    if (aNameSpaceID == kNameSpaceID_HTML) {
        rv = CreateHTMLElement(nsnull, aResource, aTagName, PR_FALSE, getter_AddRefs(result));
    }
    else {
        rv = CreateXULElement(nsnull, aResource, aNameSpaceID, aTagName, PR_FALSE, getter_AddRefs(result));
    }
    if (NS_FAILED(rv)) return rv;

    *aResult = result;
    NS_ADDREF(*aResult);
    return NS_OK;
}


////////////////////////////////////////////////////////////////////////
// Implementation methods


nsresult
RDFXULBuilderImpl::CreateChildrenFromGraph(nsINameSpace* aContainingNameSpace,
                                           nsIRDFResource* aResource,
                                           nsIContent* aElement,
                                           PRBool aForceIDAttr)
{
    nsresult rv;

    if (! aForceIDAttr) {
        // If we're inside a template tag, then force all of the kids
        // to have their 'id' attribute set.
        PRInt32 nameSpaceID;
        rv = aElement->GetNameSpaceID(nameSpaceID);
        if (NS_FAILED(rv)) return rv;

        if (nameSpaceID == kNameSpaceID_XUL) {
            nsCOMPtr<nsIAtom> tag;
            rv = aElement->GetTag(*getter_AddRefs(tag));
            if (NS_FAILED(rv)) return rv;

            if (tag.get() == kTemplateAtom)
                aForceIDAttr = PR_TRUE;
        }
    }

    nsCOMPtr<nsISimpleEnumerator> children;
    rv = NS_NewContainerEnumerator(mDB, aResource, getter_AddRefs(children));
    NS_ASSERTION(NS_SUCCEEDED(rv), "unable to create cursor for children");
    if (NS_FAILED(rv)) return rv;

    while (1) {
        PRBool hasMore;
        rv = children->HasMoreElements(&hasMore);
        if (NS_FAILED(rv)) return rv;

        if (! hasMore)
            break;

        nsCOMPtr<nsISupports> isupports;
        rv = children->GetNext(getter_AddRefs(isupports));
        NS_ASSERTION(NS_SUCCEEDED(rv), "error reading cursor");
        if (NS_FAILED(rv)) return rv;

        nsCOMPtr<nsIRDFNode> child = do_QueryInterface(isupports);

        // Add the specified node as a child container of this
        // element. What we do will vary slightly depending on whether
        // aValue is a resource or a literal.
        nsCOMPtr<nsIRDFResource> resource;
        nsCOMPtr<nsIRDFLiteral> literal;

        if (resource = do_QueryInterface(child)) {
            // If it's a resource, then add it as a child container.
            nsCOMPtr<nsIContent> content;
            rv = CreateElement(aContainingNameSpace, resource, aForceIDAttr, getter_AddRefs(content));
            NS_ASSERTION(NS_SUCCEEDED(rv), "unable to create new XUL element");
            if (NS_FAILED(rv)) return rv;

#ifdef PR_LOGGING
            if (PR_LOG_TEST(gLog, PR_LOG_DEBUG)) {
                nsAutoString parentStr;
                rv = gXULUtils->GetElementLogString(aElement, parentStr);
                NS_ASSERTION(NS_SUCCEEDED(rv), "unable to get parent element string");

                nsAutoString contentStr;
                rv = gXULUtils->GetElementLogString(content, contentStr);
                NS_ASSERTION(NS_SUCCEEDED(rv), "unable to get child element string");

                char* parentCStr = parentStr.ToNewCString();
                char* contentCStr = contentStr.ToNewCString();

                PR_LOG(gLog, PR_LOG_DEBUG, ("xulbuilder append-child"));
                PR_LOG(gLog, PR_LOG_DEBUG, ("  %s",   parentCStr));
                PR_LOG(gLog, PR_LOG_DEBUG, ("    %s", contentCStr));

                nsCRT::free(contentCStr);
                nsCRT::free(parentCStr);
            }
#endif

            rv = aElement->AppendChildTo(content, PR_FALSE);
            NS_ASSERTION(NS_SUCCEEDED(rv), "unable to add element to content model");
            if (NS_FAILED(rv)) return rv;

            // Recursively build its contents. This'll build up the
            // content model in a depth-first way. First, see if any
            // new namespace information was added.
            nsCOMPtr<nsINameSpace> nameSpace;

            nsCOMPtr<nsIXMLContent> xml( do_QueryInterface(content) );
            if (xml) {
                rv = xml->GetContainingNameSpace(*getter_AddRefs(nameSpace));
                NS_ASSERTION(NS_SUCCEEDED(rv), "unable to get containing namespace");
                if (NS_FAILED(rv)) return rv;
            }
            else {
                nameSpace = aContainingNameSpace;
            }

            rv = CreateChildrenFromGraph(nameSpace, resource, content, aForceIDAttr);
            NS_ASSERTION(NS_SUCCEEDED(rv), "unable to create child nodes");
            if (NS_FAILED(rv)) return rv;
        }
        else if (literal = do_QueryInterface(child)) {
            // If it's a literal, then add it as a simple text node.
            rv = gXULUtils->AttachTextNode(aElement, literal);
            NS_ASSERTION(NS_SUCCEEDED(rv), "unable to add text to content model");
            if (NS_FAILED(rv)) return rv;
        }
        else {
            // This should _never_ happen
            NS_ERROR("node is not a literal or a resource");
            return NS_ERROR_UNEXPECTED;
        }
    }

    // Check for a 'datasources' tag, in which case we'll create a
    // template builder.
    {
        nsAutoString dataSources;
        rv = aElement->GetAttribute(kNameSpaceID_None,
                                    kDataSourcesAtom,
                                    dataSources);

        if (rv == NS_CONTENT_ATTR_HAS_VALUE) {
            nsCOMPtr<nsIRDFContentModelBuilder> builder;
            rv = CreateTemplateBuilder(aElement, dataSources, getter_AddRefs(builder));
            NS_ASSERTION(NS_SUCCEEDED(rv), "unable to add datasources");

            // Force construction of immediate template sub-content _now_.
            rv = builder->CreateContents(aElement);
            if (NS_FAILED(rv)) return rv;
        }
    }
    
    return NS_OK;
}


nsresult
RDFXULBuilderImpl::CreateElement(nsINameSpace* aContainingNameSpace,
                                 nsIRDFResource* aResource,
                                 PRBool aForceIDAttr,
                                 nsIContent** aResult)
{
    nsresult rv;

    // Split the resource into a namespace ID and a tag, and create
    // a content element for it.

    // First, we get the node's type so we can create a tag.
    nsCOMPtr<nsIRDFNode> typeNode;
    rv = mDB->GetTarget(aResource, kXUL_tag, PR_TRUE, getter_AddRefs(typeNode));
    NS_ASSERTION(NS_SUCCEEDED(rv), "unable to get node's type");
    if (NS_FAILED(rv)) return rv;

    NS_ASSERTION(rv != NS_RDF_NO_VALUE, "no node type");
    if (rv == NS_RDF_NO_VALUE)
        return NS_ERROR_UNEXPECTED;


    nsCOMPtr<nsIRDFResource> type = do_QueryInterface(typeNode);
    NS_ASSERTION(type != nsnull, "type wasn't a resource");
    if (! type)
        return NS_ERROR_UNEXPECTED;

    PRInt32 nameSpaceID;
    nsCOMPtr<nsIAtom> tag;
    rv = mDocument->SplitProperty(type, &nameSpaceID, getter_AddRefs(tag));
    NS_ASSERTION(NS_SUCCEEDED(rv), "unable to split resource into namespace/tag pair");
    if (NS_FAILED(rv)) return rv;

    if (nameSpaceID == kNameSpaceID_HTML) {
        rv = CreateHTMLElement(aContainingNameSpace, aResource, tag, aForceIDAttr, aResult);
    }
    else {
        rv = CreateXULElement(aContainingNameSpace, aResource, nameSpaceID, tag, aForceIDAttr, aResult);
    }

    NS_ASSERTION(NS_SUCCEEDED(rv), "unable to create element");
    if (NS_FAILED(rv)) return rv;

#ifdef PR_LOGGING
    if (PR_LOG_TEST(gLog, PR_LOG_DEBUG)) {
        nsAutoString elementStr;
        rv = gXULUtils->GetElementLogString(*aResult, elementStr);
        NS_ASSERTION(NS_SUCCEEDED(rv), "unable to get element string");

        char* elementCStr = elementStr.ToNewCString();

        PR_LOG(gLog, PR_LOG_DEBUG, ("xulbuilder create-or-reuse created new element"));
        PR_LOG(gLog, PR_LOG_DEBUG, ("  %s", elementCStr));

        nsCRT::free(elementCStr);
    }
#endif

    return NS_OK;
}



nsresult
RDFXULBuilderImpl::CreateHTMLElement(nsINameSpace* aContainingNameSpace,
                                     nsIRDFResource* aResource,
                                     nsIAtom* aTagName,
                                     PRBool aForceIDAttr,
                                     nsIContent** aResult)
{
    nsresult rv;

    // XXX This is where we go out and create the HTML content. It's a
    // bit of a hack: a bridge until we get to a more DOM-based
    // solution.
    nsCOMPtr<nsIHTMLContent> element;
    const PRUnichar *unicodeString;
    aTagName->GetUnicode(&unicodeString);
    rv = mHTMLElementFactory->CreateInstanceByTag(unicodeString,
                                                  getter_AddRefs(element));

    if (NS_FAILED(rv)) return rv;

    // XXX This is part of what will be an optimal solution.  To allow
    // elements that expect to be inside a form to work without forms, we
    // will create a secret hidden form up in the XUL document.  Only
    // do this if the new HTML element is a form control element.
    // Eventually we'll really want to be tracking all forms full force.
    nsCOMPtr<nsIFormControl> htmlFormControl = do_QueryInterface(element);
    if (htmlFormControl) {
        // Retrieve the hidden form from the XUL document.
        nsCOMPtr<nsIDOMHTMLFormElement> htmlFormElement;
        mDocument->GetForm(getter_AddRefs(htmlFormElement));
      
        // Set the form
        htmlFormControl->SetForm(htmlFormElement);
    }

    // Force the document to be set _here_. Many of the
    // AppendChildTo() implementations do not recursively ensure
    // that the child's doc is the same as the parent's.
    nsCOMPtr<nsIDocument> doc = do_QueryInterface(mDocument);
    if (! doc)
        return NS_ERROR_UNEXPECTED;

    // Set the document.
    rv = element->SetDocument(doc, PR_FALSE);
    NS_ASSERTION(NS_SUCCEEDED(rv), "couldn't set document on the element");
    if (NS_FAILED(rv)) return rv;

    if (aResource) {
        PRBool isAnonymous;
        rv = gRDFService->IsAnonymousResource(aResource, &isAnonymous);
        if (NS_FAILED(rv)) return rv;

        if (!isAnonymous || aForceIDAttr) {
            // Set the 'id' attribute
            nsXPIDLCString uri;
            rv = aResource->GetValue( getter_Copies(uri) );
            if (NS_FAILED(rv)) return rv;

            nsAutoString id;
            rv = gXULUtils->MakeElementID(doc, nsAutoString(uri), id);
            if (NS_FAILED(rv)) return rv;
       
            rv = element->SetAttribute(kNameSpaceID_None, kIdAtom, id, PR_FALSE);
            NS_ASSERTION(NS_SUCCEEDED(rv), "unable to set element's ID");
            if (NS_FAILED(rv)) return rv;

            // Since we didn't notify when setting the 'id' attribute, we
            // need to explicitly add ourselves to the resource-to-element
            // map. XUL elements don't need to do this, because they've
            // got a hacked SetAttribute() method...
            rv = mDocument->AddElementForResource(aResource, element);
            if (NS_FAILED(rv)) return rv;
        }

        // Now iterate through all the properties and add them as
        // attributes on the element.  First, create a cursor that'll
        // iterate through all the properties that lead out of this
        // resource.
        nsCOMPtr<nsISimpleEnumerator> properties;
        rv = mDB->ArcLabelsOut(aResource, getter_AddRefs(properties));
        NS_ASSERTION(NS_SUCCEEDED(rv), "unable to create arcs-out cursor");
        if (NS_FAILED(rv)) return rv;

        // Advance that cursor 'til it runs outta steam
        while (1) {
            PRBool hasMore;
            rv = properties->HasMoreElements(&hasMore);
            if (NS_FAILED(rv)) return rv;

            if (! hasMore)
                break;

            nsCOMPtr<nsISupports> isupports;
            rv = properties->GetNext(getter_AddRefs(isupports));
            NS_ASSERTION(NS_SUCCEEDED(rv), "unable to get property from cursor");
            if (NS_FAILED(rv)) return rv;

            // Skip any properties that aren't meant to be added to
            // the content model.
            nsCOMPtr<nsIRDFResource> property = do_QueryInterface(isupports);

            if (! IsAttributeProperty(property))
                continue;

            // For each property, get its value: this will be the value of
            // the new attribute.
            nsCOMPtr<nsIRDFNode> value;
            rv = mDB->GetTarget(aResource, property, PR_TRUE, getter_AddRefs(value));
            NS_ASSERTION(NS_SUCCEEDED(rv), "unable to get value for property");
            if (NS_FAILED(rv)) return rv;

            NS_ASSERTION(rv != NS_RDF_NO_VALUE, "null value in cursor");
            if (rv == NS_RDF_NO_VALUE)
                continue;

            // Add the attribute to the newly constructed element
            rv = AddAttribute(element, property, value);
            NS_ASSERTION(NS_SUCCEEDED(rv), "unable to add attribute to element");
            if (NS_FAILED(rv)) return rv;
        }
    }
    
    // ...and finally, return the result.
    rv = element->QueryInterface(nsCOMTypeInfo<nsIContent>::GetIID(), (void**) aResult);
    NS_ASSERTION(NS_SUCCEEDED(rv), "unable to get nsIContent interface");
    if (NS_FAILED(rv)) return rv;

    return NS_OK;
}


nsresult
RDFXULBuilderImpl::CreateXULElement(nsINameSpace* aContainingNameSpace,
                                    nsIRDFResource* aResource,
                                    PRInt32 aNameSpaceID,
                                    nsIAtom* aTagName,
                                    PRBool aForceIDAttr,
                                    nsIContent** aResult)
{
    nsresult rv;

    // Create a new XUL element.
    nsCOMPtr<nsIContent> element;
    rv = NS_NewRDFElement(aNameSpaceID, aTagName, getter_AddRefs(element));
    NS_ASSERTION(NS_SUCCEEDED(rv), "unable to create new content element");
    if (NS_FAILED(rv)) return rv;

    nsCOMPtr<nsIDocument> document = do_QueryInterface(mDocument);
    if (! document)
        return NS_ERROR_UNEXPECTED;

    // Set the document for this element.
    element->SetDocument(document, PR_FALSE);

    // Initialize the element's containing namespace to that of its
    // parent.
    nsCOMPtr<nsIXMLContent> xml( do_QueryInterface(element) );
    NS_ASSERTION(xml != nsnull, "not an XML element");
    if (! xml)
        return NS_ERROR_UNEXPECTED;

    rv = xml->SetContainingNameSpace(aContainingNameSpace);
    if (NS_FAILED(rv)) return rv;

    if (aResource) {
        PRBool isAnonymous;
        rv = gRDFService->IsAnonymousResource(aResource, &isAnonymous);
        if (NS_FAILED(rv)) return rv;

        if (!isAnonymous || aForceIDAttr) {
            // Set the element's ID. The XUL document will be listening
            // for 'id' and 'ref' attribute changes, so we're sure that
            // this will get properly hashed into the document's
            // resource-to-element map.
            nsXPIDLCString uri;
            rv = aResource->GetValue( getter_Copies(uri) );
            if (NS_FAILED(rv)) return rv;

            nsAutoString id;
            rv = gXULUtils->MakeElementID(document, nsAutoString(uri), id);
            if (NS_FAILED(rv)) return rv;

            rv = element->SetAttribute(kNameSpaceID_None, kIdAtom, id, PR_FALSE);
            if (NS_FAILED(rv)) return rv;

            // The XUL element's implementation has a hacked
            // SetAttribute() method that'll be smart enough to add the
            // element to the document's element-to-resource map, so no
            // need to do it ourselves. N.B. that this is _different_ from
            // an HTML element...
        }

        // Now iterate through all the properties and add them as
        // attributes on the element.  First, create a cursor that'll
        // iterate through all the properties that lead out of this
        // resource.
        nsCOMPtr<nsISimpleEnumerator> properties;
        rv = mDB->ArcLabelsOut(aResource, getter_AddRefs(properties));
        NS_ASSERTION(NS_SUCCEEDED(rv), "unable to create arcs-out cursor");
        if (NS_FAILED(rv)) return rv;

        // Advance that cursor 'til it runs outta steam
        while (1) {
            PRBool hasMore;
            rv = properties->HasMoreElements(&hasMore);
            if (NS_FAILED(rv)) return rv;

            if (! hasMore)
                break;

            nsCOMPtr<nsISupports> isupports;
            rv = properties->GetNext(getter_AddRefs(isupports));
            NS_ASSERTION(NS_SUCCEEDED(rv), "unable to get property from cursor");
            if (NS_FAILED(rv)) return rv;

            // Skip any properties that aren't meant to be added to
            // the content model.
            nsCOMPtr<nsIRDFResource> property = do_QueryInterface(isupports);

            if (! IsAttributeProperty(property))
                continue;

            // For each property, set its value.
            nsCOMPtr<nsIRDFNode> value;
            rv = mDB->GetTarget(aResource, property, PR_TRUE, getter_AddRefs(value));
            NS_ASSERTION(NS_SUCCEEDED(rv), "unable to get value for property");
            if (NS_FAILED(rv)) return rv;

            // Add the attribute to the newly constructed element
            rv = AddAttribute(element, property, value);
            NS_ASSERTION(NS_SUCCEEDED(rv), "unable to add attribute to element");
            if (NS_FAILED(rv)) return rv;
        }

        // Set the XML tag info: namespace prefix and ID. We do this
        // _after_ processing all the attributes so that we can extract an
        // appropriate prefix based on whatever namespace decls are
        // active.
        rv = xml->SetNameSpaceID(aNameSpaceID);
        if (NS_FAILED(rv)) return rv;

        if (aNameSpaceID != kNameSpaceID_None) {
            nsCOMPtr<nsINameSpace> nameSpace;
            rv = xml->GetContainingNameSpace(*getter_AddRefs(nameSpace));
            if (NS_FAILED(rv)) return rv;

            nsCOMPtr<nsIAtom> prefix;
            rv = nameSpace->FindNameSpacePrefix(aNameSpaceID, *getter_AddRefs(prefix));
            if (NS_FAILED(rv)) return rv;

            rv = xml->SetNameSpacePrefix(prefix);
            if (NS_FAILED(rv)) return rv;
        }

        // We also need to pay special attention to the keyset tag to set up a listener
        if(aTagName == kKeysetAtom) {
            // Create our nsXULKeyListener and hook it up.
            nsCOMPtr<nsIXULKeyListener> keyListener;
            rv = nsComponentManager::CreateInstance(kXULKeyListenerCID,
                                                    nsnull,
                                                    kIXULKeyListenerIID,
                                                    getter_AddRefs(keyListener));
            NS_ASSERTION(NS_SUCCEEDED(rv), "unable to create a key listener");
            if (NS_FAILED(rv)) return rv;
        
            nsCOMPtr<nsIDOMEventListener> domEventListener = do_QueryInterface(keyListener);
            if (domEventListener) {
                // Take the key listener and add it as an event listener for keypress events.
                nsCOMPtr<nsIDOMDocument> domDocument = do_QueryInterface(mDocument);

                // Init the listener with the keyset node
                nsCOMPtr<nsIDOMElement> domElement = do_QueryInterface(element);
                keyListener->Init(domElement, domDocument);
            
                nsCOMPtr<nsIDOMEventReceiver> receiver = do_QueryInterface(document);
                if(receiver) {
                    receiver->AddEventListener("keypress", domEventListener, PR_TRUE); 
                    receiver->AddEventListener("keydown",  domEventListener, PR_TRUE);  
                    receiver->AddEventListener("keyup",    domEventListener, PR_TRUE);   
                }
            }
        }
    }

    // Finally, assign the newly constructed element to the result
    // pointer and addref it for the trip home.
    *aResult = element;
    NS_ADDREF(*aResult);

    return NS_OK;
}



nsresult
RDFXULBuilderImpl::GetContainingNameSpace(nsIContent* aElement, nsINameSpace** aNameSpace)
{
    // Walk up the content model to find the first XML element
    // (including this one). Once we find it, suck out the namespace
    // decl.
    nsCOMPtr<nsIContent> element( dont_QueryInterface(aElement) );
    while (element) {
        nsCOMPtr<nsIXMLContent> xml( do_QueryInterface(element) );
        if (xml) {
            nsresult rv = xml->GetContainingNameSpace(*aNameSpace);
            return rv;
        }

        nsCOMPtr<nsIContent> parent;
        element->GetParent(*getter_AddRefs(parent));
        element = parent;
    }

    NS_ERROR("no XUL element");
    return NS_ERROR_UNEXPECTED;
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

PRBool
RDFXULBuilderImpl::IsAttributeProperty(nsIRDFResource* aProperty)
{
    // Return 'true' is the property should be added to the content
    // model as an attribute.
    NS_ASSERTION(aProperty != nsnull, "null ptr");
    if (! aProperty)
        return PR_FALSE;

    // These are special beacuse they're used to specify the tree
    // structure of the XUL: ignore them b/c they're not attributes
    if ((aProperty == kRDF_nextVal) ||
        (aProperty == kXUL_tag) ||
        (aProperty == kRDF_instanceOf))
        return PR_FALSE;

    nsresult rv;
    PRBool isOrdinal;
    rv = gRDFContainerUtils->IsOrdinalProperty(aProperty, &isOrdinal);
    if (NS_FAILED(rv))
        return PR_FALSE;

    if (isOrdinal)
        return PR_FALSE;

    return PR_TRUE;
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

#ifdef PR_LOGGING
    if (PR_LOG_TEST(gLog, PR_LOG_DEBUG)) {
        nsAutoString elementStr;
        rv = gXULUtils->GetElementLogString(aElement, elementStr);
        NS_ASSERTION(NS_SUCCEEDED(rv), "unable to get element string");

        nsAutoString attrStr;
        rv = gXULUtils->GetAttributeLogString(aElement, nameSpaceID, tag, attrStr);
        NS_ASSERTION(NS_SUCCEEDED(rv), "unable to get child element string");

        nsAutoString valueStr;
        rv = gXULUtils->GetTextForNode(aValue, valueStr);
        NS_ASSERTION(NS_SUCCEEDED(rv), "unable to get value text");

        char* elementCStr = elementStr.ToNewCString();
        char* attrCStr = attrStr.ToNewCString();
        char* valueCStr = valueStr.ToNewCString();

        PR_LOG(gLog, PR_LOG_DEBUG, ("xulbuilder add-attribute"));
        PR_LOG(gLog, PR_LOG_DEBUG, ("  %s",   elementCStr));
        PR_LOG(gLog, PR_LOG_DEBUG, ("    %s=\"%s\"", attrCStr, valueCStr));

        nsCRT::free(valueCStr);
        nsCRT::free(attrCStr);
        nsCRT::free(elementCStr);
    }
#endif

    if ((nameSpaceID == kNameSpaceID_XMLNS) ||
        ((nameSpaceID == kNameSpaceID_None) && (tag.get() == kXMLNSAtom))) {
        // This is the receiving end of the namespace hack. The XUL
        // content sink will dump "xmlns:" attributes into the graph
        // so we can suck them out _here_ to install the proper
        // namespace hierarchy (which we need to be able to create the
        // illusion of the DOM).
        //
        // We pull the current containing namespace out of the
        // element, use it to construct a new child namespace, then
        // re-set the element's containing namespace to the new child
        // namespace.
        nsCOMPtr<nsIXMLContent> xml( do_QueryInterface(aElement) );
        NS_ASSERTION(xml != nsnull, "not an XML element");
        if (! xml)
            return NS_ERROR_UNEXPECTED;

        nsCOMPtr<nsINameSpace> parentNameSpace;
        rv = xml->GetContainingNameSpace(*getter_AddRefs(parentNameSpace));
        if (NS_FAILED(rv)) return rv;

        NS_ASSERTION(parentNameSpace != nsnull, "no containing namespace");
        if (! parentNameSpace)
            return NS_ERROR_UNEXPECTED;

        // the prefix is null for a default namespace
        nsIAtom* prefix = (tag.get() != kXMLNSAtom) ? tag.get() : nsnull;

        nsCOMPtr<nsIRDFLiteral> uri;
        rv = aValue->QueryInterface(nsIRDFLiteral::GetIID(), getter_AddRefs(uri));
        NS_ASSERTION(NS_SUCCEEDED(rv), "namespace URI not a literal");
        if (NS_FAILED(rv)) return rv;

        nsXPIDLString uriStr;
        rv = uri->GetValue(getter_Copies(uriStr));
        if (NS_FAILED(rv)) return rv;

        nsCOMPtr<nsINameSpace> childNameSpace;
        rv = parentNameSpace->CreateChildNameSpace(prefix, (const PRUnichar*) uriStr,
                                                   *getter_AddRefs(childNameSpace));
        if (NS_FAILED(rv)) return rv;

        rv = xml->SetContainingNameSpace(childNameSpace);
        if (NS_FAILED(rv)) return rv;

        return NS_OK;
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
            PR_LOG(gLog, PR_LOG_ALWAYS,
                   ("ignoring non-HTML attribute on HTML tag"));

            return NS_OK;
        }
    }

    nsAutoString value;
    rv = gXULUtils->GetTextForNode(aValue, value);
    if (NS_FAILED(rv)) return rv;

    rv = aElement->SetAttribute(nameSpaceID, tag, value, PR_TRUE);
    if (NS_FAILED(rv)) return rv;

    return NS_OK;
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

#ifdef PR_LOGGING
    if (PR_LOG_TEST(gLog, PR_LOG_DEBUG)) {
        nsAutoString elementStr;
        rv = gXULUtils->GetElementLogString(aElement, elementStr);
        NS_ASSERTION(NS_SUCCEEDED(rv), "unable to get element string");

        nsAutoString attrStr;
        rv = gXULUtils->GetAttributeLogString(aElement, nameSpaceID, tag, attrStr);
        NS_ASSERTION(NS_SUCCEEDED(rv), "unable to get child element string");

        nsAutoString valueStr;
        rv = gXULUtils->GetTextForNode(aValue, valueStr);
        NS_ASSERTION(NS_SUCCEEDED(rv), "unable to get value text");

        char* elementCStr = elementStr.ToNewCString();
        char* attrCStr = attrStr.ToNewCString();
        char* valueCStr = valueStr.ToNewCString();

        PR_LOG(gLog, PR_LOG_DEBUG, ("xulbuilder remove-attribute"));
        PR_LOG(gLog, PR_LOG_DEBUG, ("  %s",   elementCStr));
        PR_LOG(gLog, PR_LOG_DEBUG, ("    %s=\"%s\"", attrCStr, valueCStr));

        nsCRT::free(valueCStr);
        nsCRT::free(attrCStr);
        nsCRT::free(elementCStr);
    }
#endif

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
RDFXULBuilderImpl::CreateTemplateBuilder(nsIContent* aElement,
                                         const nsString& aDataSources,
                                         nsIRDFContentModelBuilder** aResult)
{
    nsresult rv;

    // construct a new builder
    nsCOMPtr<nsIRDFContentModelBuilder> builder;
    rv = nsComponentManager::CreateInstance(kXULTemplateBuilderCID,
                                            nsnull,
                                            kIRDFContentModelBuilderIID,
                                            (void**) getter_AddRefs(builder));
    NS_ASSERTION(NS_SUCCEEDED(rv), "unable to create tree content model builder");
    if (NS_FAILED(rv)) return rv;

    if (NS_FAILED(rv = builder->SetRootContent(aElement))) {
        NS_ERROR("unable to set builder's root content element");
        return rv;
    }

    // create a database for the builder
    nsCOMPtr<nsIRDFCompositeDataSource> db;
    rv = nsComponentManager::CreateInstance(kRDFCompositeDataSourceCID,
                                            nsnull,
                                            kIRDFCompositeDataSourceIID,
                                            getter_AddRefs(db));

    if (NS_FAILED(rv)) {
        NS_ERROR("unable to construct new composite data source");
        return rv;
    }

    // Add the local store as the first data source in the db.
    {
        nsCOMPtr<nsIRDFDataSource> localstore;
        rv = gRDFService->GetDataSource("rdf:local-store", getter_AddRefs(localstore));
        NS_ASSERTION(NS_SUCCEEDED(rv), "unable to get local store");
        if (NS_FAILED(rv)) return rv;

        rv = db->AddDataSource(localstore);
        NS_ASSERTION(NS_SUCCEEDED(rv), "unable to add local store to db");
        if (NS_FAILED(rv)) return rv;
    }

    // Parse datasources: they are assumed to be a whitespace
    // separated list of URIs; e.g.,
    //
    //     rdf:bookmarks rdf:history http://foo.bar.com/blah.cgi?baz=9
    //
    PRInt32 first = 0;

    nsCOMPtr<nsIDocument> document = do_QueryInterface(mDocument);
    if (! document)
        return NS_ERROR_UNEXPECTED;

    nsCOMPtr<nsIURI> docURL = dont_AddRef( document->GetDocumentURL() );
    if (! docURL)
        return NS_ERROR_UNEXPECTED;

    while(1) {
        while (first < aDataSources.Length() && nsString::IsSpace(aDataSources.CharAt(first)))
            ++first;

        if (first >= aDataSources.Length())
            break;

        PRInt32 last = first;
        while (last < aDataSources.Length() && !nsString::IsSpace(aDataSources.CharAt(last)))
            ++last;

        nsAutoString uri;
        aDataSources.Mid(uri, first, last - first);
        first = last + 1;

        rv = rdf_MakeAbsoluteURI(docURL, uri);
        if (NS_FAILED(rv)) return rv;

        nsCOMPtr<nsIRDFDataSource> ds;

        // Some monkey business to convert the nsAutoString to a
        // C-string safely. Sure'd be nice to have this be automagic.
        {
            char buf[256], *p = buf;
            if (uri.Length() >= PRInt32(sizeof buf))
                p = (char *)nsAllocator::Alloc(uri.Length() + 1);

            uri.ToCString(p, uri.Length() + 1);

            rv = gRDFService->GetDataSource(p, getter_AddRefs(ds));

            if (p != buf)
                nsCRT::free(p);
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

    *aResult = builder;
    NS_ADDREF(*aResult);
    return NS_OK;
}


nsresult
RDFXULBuilderImpl::GetRDFResourceFromXULElement(nsIDOMNode* aNode, nsIRDFResource** aResult)
{
    nsresult rv;

    // Given an nsIDOMNode that presumably has been created as a proxy
    // for an RDF resource, pull the RDF resource information out of
    // it.

    nsCOMPtr<nsIContent> element;
    rv = aNode->QueryInterface(nsCOMTypeInfo<nsIContent>::GetIID(), getter_AddRefs(element) );
    NS_ASSERTION(NS_SUCCEEDED(rv), "DOM element doesn't support nsIContent");
    if (NS_FAILED(rv)) return rv;

    return gXULUtils->GetElementResource(element, aResult);
}


nsresult
RDFXULBuilderImpl::GetGraphNodeForXULElement(nsIDOMNode* aNode, nsIRDFNode** aResult)
{
    nsresult rv;

    nsCOMPtr<nsIDOMText> text( do_QueryInterface(aNode) );
    if (text) {
        nsAutoString data;
        rv = text->GetData(data);
        if (NS_FAILED(rv)) return rv;

        nsCOMPtr<nsIRDFLiteral> literal;
        rv = gRDFService->GetLiteral(data.GetUnicode(), getter_AddRefs(literal));
        if (NS_FAILED(rv)) return rv;

        *aResult = literal;
        NS_ADDREF(*aResult);
    }
    else {
        // XXX If aNode doesn't have a resource, then panic for
        // now. (We may be able to safely just ignore this at some
        // point.)
        nsCOMPtr<nsIRDFResource> resource;
        rv = GetRDFResourceFromXULElement(aNode, getter_AddRefs(resource));
//        NS_ASSERTION(NS_SUCCEEDED(rv), "new child doesn't have a resource");
        if (NS_FAILED(rv)) return rv;

        // If the node isn't marked as a XUL element in the graph,
        // then we'll ignore it.
        PRBool isXULElement;
        rv = mDB->HasAssertion(resource, kRDF_instanceOf, kXUL_element, PR_TRUE, &isXULElement);
        if (NS_FAILED(rv)) return rv;

        if (! isXULElement)
            return NS_RDF_NO_VALUE;

        *aResult = resource;
        NS_ADDREF(*aResult);
    }

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
    const PRUnichar *unicodeString;
    aNameAtom->GetUnicode(&unicodeString);
    nsAutoString tag(unicodeString);
    if (0 < uri.Length() && uri.Last() != '#' && uri.Last() != '/' && tag.First() != '#')
        uri.Append('#');

    uri.Append(tag);

    nsresult rv = gRDFService->GetUnicodeResource(uri.GetUnicode(), aResource);
    NS_ASSERTION(NS_SUCCEEDED(rv), "unable to get resource");
    return rv;
}



nsresult
RDFXULBuilderImpl::MakeProperty(PRInt32 aNameSpaceID, nsIAtom* aTagName, nsIRDFResource** aResult)
{
    // Using the namespace ID and the tag, construct a fully-qualified
    // URI and turn it into an RDF property.

    nsCOMPtr<nsIDocument> document = do_QueryInterface(mDocument);
    if (! document)
        return NS_ERROR_UNEXPECTED;

    nsresult rv;
    nsCOMPtr<nsINameSpaceManager> nsmgr;
    rv = document->GetNameSpaceManager(*getter_AddRefs(nsmgr));
    if (NS_FAILED(rv)) return rv;

    nsAutoString uri;
    rv = nsmgr->GetNameSpaceURI(aNameSpaceID, uri);
    NS_ASSERTION(NS_SUCCEEDED(rv), "unable to get URI for namespace");
    if (NS_FAILED(rv)) return rv;

    if (uri.Last() != PRUnichar('#') && uri.Last() != PRUnichar('/'))
        uri.Append('#');

    const PRUnichar *unicodeString;
    aTagName->GetUnicode(&unicodeString);
    uri.Append(unicodeString);

    rv = gRDFService->GetUnicodeResource(uri.GetUnicode(), aResult);
    return rv;
}




nsresult
RDFXULBuilderImpl::GetParentNodeWithHTMLHack(nsIDOMNode* aNode, nsCOMPtr<nsIDOMNode>* aResult)
{
    // Since HTML nodes will return the document as their parent node
    // when their mParent == nsnull && mDocument != nsnull, we need to
    // deal. See bug 6917 for details.
    nsresult rv;

    nsCOMPtr<nsIDOMNode> result;

    rv = aNode->GetParentNode(getter_AddRefs(result));
    if (NS_FAILED(rv)) return rv;

    if (result) {
        // If we can QI to nsIDocument, then we've hit the case where
        // HTML nodes screw us. Make the parent be null.
        if (nsCOMPtr<nsIDocument>(do_QueryInterface(result))) {
            result = nsnull;
        }
    }

    *aResult = result;
    return NS_OK;
}


PRBool
RDFXULBuilderImpl::IsXULElement(nsIContent* aElement)
{
    // Return PR_TRUE if the element is a XUL element; that is, it is
    // part of the original XUL document, or has been created via the
    // DOM. We can tell because it'll be decorated with an
    // "[RDF:instanceOf]-->[XUL:element]" attribute.
    nsresult rv;

    nsCOMPtr<nsIRDFResource> resource;
    rv = gXULUtils->GetElementResource(aElement, getter_AddRefs(resource));
    if (NS_FAILED(rv)) return PR_FALSE;

    PRBool isXULElement;
    rv = mDB->HasAssertion(resource, kRDF_instanceOf, kXUL_element, PR_TRUE, &isXULElement);
    if (NS_FAILED(rv)) return PR_FALSE;

    return isXULElement;
}
