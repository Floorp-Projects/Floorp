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

  2) I really _really_ need to figure out how to factor the logic in
     OnAssert, OnUnassert, OnMove, and OnChange. These all mostly
     kinda do the same sort of thing. It atrocious how much code is
     cut-n-pasted.

 */

#include "nsCOMPtr.h"
#include "nsCRT.h"
#include "nsIAtom.h"
#include "nsIContent.h"
#include "nsIDOMElement.h"
#include "nsIDOMNode.h"
#include "nsIDOMXULDocument.h"
#include "nsIDOMXULElement.h"
#include "nsIDocument.h"
#include "nsIHTMLContent.h"
#include "nsIHTMLElementFactory.h"
#include "nsINameSpaceManager.h"
#include "nsIRDFCompositeDataSource.h"
#include "nsIRDFContainerUtils.h" 
#include "nsIRDFContentModelBuilder.h"
#include "nsIRDFDocument.h"
#include "nsIRDFNode.h"
#include "nsIRDFObserver.h"
#include "nsIRDFRemoteDataSource.h"
#include "nsIRDFService.h"
#include "nsIServiceManager.h"
#include "nsISupportsArray.h"
#include "nsITextContent.h"
#include "nsITimer.h"
#include "nsIURL.h"
#include "nsIXULSortService.h"
#include "nsLayoutCID.h"
#include "nsRDFCID.h"
#include "nsRDFContentUtils.h"
#include "nsString.h"
#include "nsVoidArray.h"
#include "nsXPIDLString.h"
#include "prlog.h"
#include "rdf.h"
#include "rdfutil.h"

// Return values for EnsureElementHasGenericChild()
#define NS_RDF_ELEMENT_WAS_CREATED NS_RDF_NO_VALUE
#define NS_RDF_ELEMENT_WAS_THERE   NS_OK

static PRLogModuleInfo* gLog;

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
static NS_DEFINE_CID(kRDFContainerUtilsCID,       NS_RDFCONTAINERUTILS_CID);
static NS_DEFINE_CID(kTextNodeCID,                NS_TEXTNODE_CID);

static NS_DEFINE_IID(kXULSortServiceCID,         NS_XULSORTSERVICE_CID);
static NS_DEFINE_IID(kIXULSortServiceIID,        NS_IXULSORTSERVICE_IID);

static NS_DEFINE_CID(kHTMLElementFactoryCID,  NS_HTML_ELEMENT_FACTORY_CID);
static NS_DEFINE_CID(kIHTMLElementFactoryIID, NS_IHTML_ELEMENT_FACTORY_IID);

////////////////////////////////////////////////////////////////////////

#define XUL_NAMESPACE_URI "http://www.mozilla.org/keymaster/gatekeeper/there.is.only.xul"

////////////////////////////////////////////////////////////////////////

class RDFGenericBuilderImpl : public nsIRDFContentModelBuilder,
                              public nsIRDFObserver
{
public:
    RDFGenericBuilderImpl();
    virtual ~RDFGenericBuilderImpl();

    nsresult Init();

    // nsISupports interface
    NS_DECL_ISUPPORTS

    // nsIRDFContentModelBuilder interface
    NS_IMETHOD SetDocument(nsIRDFDocument* aDocument);
    NS_IMETHOD SetDataBase(nsIRDFCompositeDataSource* aDataBase);
    NS_IMETHOD GetDataBase(nsIRDFCompositeDataSource** aDataBase);
    NS_IMETHOD CreateRootContent(nsIRDFResource* aResource);
    NS_IMETHOD SetRootContent(nsIContent* aElement);
    NS_IMETHOD CreateContents(nsIContent* aElement);
    NS_IMETHOD OpenContainer(nsIContent* aContainer);
    NS_IMETHOD CloseContainer(nsIContent* aContainer);
    NS_IMETHOD RebuildContainer(nsIContent* aContainer);
    NS_IMETHOD CreateElement(PRInt32 aNameSpaceID,
                             nsIAtom* aTag,
                             nsIRDFResource* aResource,
                             nsIContent** aResult);

    // nsIRDFObserver interface
    NS_IMETHOD OnAssert(nsIRDFResource* aSource,
                        nsIRDFResource* aProperty,
                        nsIRDFNode* aTarget);

    NS_IMETHOD OnUnassert(nsIRDFResource* aSource,
                          nsIRDFResource* aProperty,
                          nsIRDFNode* aTarget);

    NS_IMETHOD OnChange(nsIRDFResource* aSource,
                        nsIRDFResource* aProperty,
                        nsIRDFNode* aOldTarget,
                        nsIRDFNode* aNewTarget);

    NS_IMETHOD OnMove(nsIRDFResource* aOldSource,
                      nsIRDFResource* aNewSource,
                      nsIRDFResource* aProperty,
                      nsIRDFNode* aTarget);

    // Implementation methods
    nsresult
    FindTemplate(nsIContent* aElement,
                 nsIRDFResource* aProperty,
                 nsIRDFResource* aChild,
                 nsIContent **theTemplate);

    nsresult
    IsTemplateRuleMatch(nsIContent* aElement,
                        nsIRDFResource* aProperty,
                        nsIRDFResource* aChild,
                        nsIContent *aRule,
                        PRBool *isMatch);

    PRBool
    IsIgnoreableAttribute(PRInt32 aNameSpaceID, nsIAtom* aAtom);

    nsresult
    GetSubstitutionText(nsIRDFResource* aResource,
                        const nsString& aSubstitution,
                        nsString& aResult);

    nsresult
    BuildContentFromTemplate(nsIContent *aTemplateNode,
                             nsIContent *aRealNode,
                             PRBool aIsUnique,
                             nsIRDFResource* aChild,
                             PRInt32 aNaturalOrderPos,
                             PRBool aNotify);

    nsresult
    CreateWidgetItem(nsIContent* aElement,
                     nsIRDFResource* aProperty,
                     nsIRDFResource* aChild,
                     PRInt32 aNaturalOrderPos,
                     PRBool aNotify);

    enum eUpdateAction { eSet, eClear };

#if 0
    PRBool
    IsAttributePersisent(nsIContent *element, PRInt32 aNameSpaceID, nsIAtom* aAtom);

    void
    GetPersistentAttributes(nsIContent *realKid);

    void
    PersistAttribute(nsIContent *element,
    			PRInt32 aNameSpaceID,
    			nsIAtom* aAtom,
    			nsString aValue,
    			eUpdateAction action);

    void
    PersistProperty(nsIContent *element,
    			nsIRDFResource *aProperty,
    			nsIRDFNode *aTarget,
    			eUpdateAction action);
#endif

    nsresult
    SynchronizeUsingTemplate(nsIContent *aTemplateNode,
                             nsIContent* aRealNode,
                             eUpdateAction aAction,
                             nsIRDFResource* aProperty,
                             nsIRDFNode* aValue);

    nsresult
    RemoveWidgetItem(nsIContent* aElement,
                     nsIRDFResource* aProperty,
                     nsIRDFResource* aValue,
                     PRBool aNotify);

    nsresult
    CreateContainerContents(nsIContent* aElement, nsIRDFResource* aResource, PRBool aNotify);

    nsresult
    CreateTemplateContents(nsIContent* aElement, const nsString& aTemplateID);

    nsresult
    EnsureElementHasGenericChild(nsIContent* aParent,
                                 PRInt32 aNameSpaceID,
                                 nsIAtom* aTag,
                                 PRBool aNotify,
                                 nsIContent** aResult);

    PRBool
    IsContainmentProperty(nsIContent* aElement, nsIRDFResource* aProperty);

    PRBool
    IsIgnoredProperty(nsIContent* aElement, nsIRDFResource* aProperty);

    PRBool
    IsContainer(nsIContent* aParentElement, nsIRDFResource* aTargetResource);

    PRBool
    IsEmpty(nsIContent* aParentElement, nsIRDFResource* aContainer);

    PRBool
    IsOpen(nsIContent* aElement);

    PRBool
    IsElementInWidget(nsIContent* aElement);
   
    PRBool
    IsResourceElement(nsIContent* aElement);
 
    nsresult
    GetDOMNodeResource(nsIDOMNode* aNode, nsIRDFResource** aResource);

    nsresult
    GetResource(PRInt32 aNameSpaceID,
                nsIAtom* aNameAtom,
                nsIRDFResource** aResource);

    nsresult FindInsertionPoint(nsIContent* aElement, nsIContent** aResult);
    nsresult RemoveGeneratedContent(nsIContent* aElement);
    nsresult FindFirstGeneratedChild(nsIContent* aElement, PRInt32* aIndex);

    // XXX. Urg. Hack until layout can batch reflows. See bug 10818.
    PRBool
    IsTreeWidgetItem(nsIContent* aElement);

    PRBool
    IsReflowScheduled();

    nsresult
    ScheduleReflow();

    static void
    ForceTreeReflow(nsITimer* aTimer, void* aClosure);

protected:
    nsIRDFDocument*            mDocument; // [WEAK]

    // We are an observer of the composite datasource. The cycle is
    // broken by out-of-band SetDataBase(nsnull) call when document is
    // destroyed.
    nsCOMPtr<nsIRDFCompositeDataSource> mDB;
    nsCOMPtr<nsIContent>                mRoot;

    nsCOMPtr<nsITimer> mTimer;

    static nsIRDFDataSource	*mLocalstore;
    static PRBool		persistLock;

    // pseudo-constants
    static nsrefcnt gRefCnt;
    static nsIRDFService*         gRDFService;
    static nsIRDFContainerUtils*  gRDFContainerUtils;
    static nsINameSpaceManager*   gNameSpaceManager;
    static nsIHTMLElementFactory* gHTMLElementFactory;

    static nsIAtom* kContainerAtom;
    static nsIAtom* kLazyContentAtom;
    static nsIAtom* kIsContainerAtom;
    static nsIAtom* kIsEmptyAtom;
    static nsIAtom* kXULContentsGeneratedAtom;
    static nsIAtom* kTemplateContentsGeneratedAtom;
    static nsIAtom* kContainerContentsGeneratedAtom;
    static nsIAtom* kNaturalOrderPosAtom;
    static nsIAtom* kIdAtom;
    static nsIAtom* kPersistAtom;
    static nsIAtom* kOpenAtom;
    static nsIAtom* kEmptyAtom;
    static nsIAtom* kResourceAtom;
    static nsIAtom* kURIAtom;
    static nsIAtom* kContainmentAtom;
    static nsIAtom* kIgnoreAtom;
    static nsIAtom* kRefAtom;
    static nsIAtom* kValueAtom;

    static nsIAtom* kTemplateAtom;
    static nsIAtom* kRuleAtom;
    static nsIAtom* kTextAtom;
    static nsIAtom* kPropertyAtom;
    static nsIAtom* kInstanceOfAtom;

    static nsIAtom* kTreeAtom;
    static nsIAtom* kTreeChildrenAtom;
    static nsIAtom* kTreeItemAtom;

    static PRInt32  kNameSpaceID_RDF;
    static PRInt32  kNameSpaceID_XUL;

    static nsIRDFResource* kNC_Title;
    static nsIRDFResource* kNC_child;
    static nsIRDFResource* kNC_Column;
    static nsIRDFResource* kNC_Folder;
    static nsIRDFResource* kRDF_child;
    static nsIRDFResource* kRDF_instanceOf;
    static nsIRDFResource* kXUL_element;

    static nsIXULSortService* gXULSortService;
};

////////////////////////////////////////////////////////////////////////

nsrefcnt            RDFGenericBuilderImpl::gRefCnt = 0;
nsIXULSortService*	RDFGenericBuilderImpl::gXULSortService = nsnull;

nsIAtom* RDFGenericBuilderImpl::kContainerAtom;
nsIAtom* RDFGenericBuilderImpl::kLazyContentAtom;
nsIAtom* RDFGenericBuilderImpl::kIsContainerAtom;
nsIAtom* RDFGenericBuilderImpl::kIsEmptyAtom;
nsIAtom* RDFGenericBuilderImpl::kXULContentsGeneratedAtom;
nsIAtom* RDFGenericBuilderImpl::kTemplateContentsGeneratedAtom;
nsIAtom* RDFGenericBuilderImpl::kContainerContentsGeneratedAtom;
nsIAtom* RDFGenericBuilderImpl::kIdAtom;
nsIAtom* RDFGenericBuilderImpl::kPersistAtom;
nsIAtom* RDFGenericBuilderImpl::kOpenAtom;
nsIAtom* RDFGenericBuilderImpl::kEmptyAtom;
nsIAtom* RDFGenericBuilderImpl::kResourceAtom;
nsIAtom* RDFGenericBuilderImpl::kURIAtom;
nsIAtom* RDFGenericBuilderImpl::kContainmentAtom;
nsIAtom* RDFGenericBuilderImpl::kNaturalOrderPosAtom;
nsIAtom* RDFGenericBuilderImpl::kIgnoreAtom;
nsIAtom* RDFGenericBuilderImpl::kRefAtom;
nsIAtom* RDFGenericBuilderImpl::kValueAtom;

nsIAtom* RDFGenericBuilderImpl::kTemplateAtom;
nsIAtom* RDFGenericBuilderImpl::kRuleAtom;
nsIAtom* RDFGenericBuilderImpl::kTextAtom;
nsIAtom* RDFGenericBuilderImpl::kPropertyAtom;
nsIAtom* RDFGenericBuilderImpl::kInstanceOfAtom;

nsIAtom* RDFGenericBuilderImpl::kTreeAtom;
nsIAtom* RDFGenericBuilderImpl::kTreeChildrenAtom;
nsIAtom* RDFGenericBuilderImpl::kTreeItemAtom;

PRInt32  RDFGenericBuilderImpl::kNameSpaceID_RDF;
PRInt32  RDFGenericBuilderImpl::kNameSpaceID_XUL;

nsIRDFService*  RDFGenericBuilderImpl::gRDFService;
nsIRDFDataSource* RDFGenericBuilderImpl::mLocalstore;
PRBool		RDFGenericBuilderImpl::persistLock;
nsIRDFContainerUtils* RDFGenericBuilderImpl::gRDFContainerUtils;
nsINameSpaceManager* RDFGenericBuilderImpl::gNameSpaceManager;
nsIHTMLElementFactory* RDFGenericBuilderImpl::gHTMLElementFactory;

nsIRDFResource* RDFGenericBuilderImpl::kNC_Title;
nsIRDFResource* RDFGenericBuilderImpl::kNC_child;
nsIRDFResource* RDFGenericBuilderImpl::kNC_Column;
nsIRDFResource* RDFGenericBuilderImpl::kNC_Folder;
nsIRDFResource* RDFGenericBuilderImpl::kRDF_child;
nsIRDFResource* RDFGenericBuilderImpl::kRDF_instanceOf;
nsIRDFResource* RDFGenericBuilderImpl::kXUL_element;


////////////////////////////////////////////////////////////////////////


nsresult
NS_NewXULTemplateBuilder(nsIRDFContentModelBuilder** aResult)
{
    nsresult rv;
    RDFGenericBuilderImpl* result = new RDFGenericBuilderImpl();
    if (! result)
        return NS_ERROR_OUT_OF_MEMORY;

    NS_ADDREF(result); // stabilize

    rv = result->Init();
    if (NS_SUCCEEDED(rv)) {
        rv = result->QueryInterface(nsCOMTypeInfo<nsIRDFContentModelBuilder>::GetIID(), (void**) aResult);
    }

    NS_RELEASE(result);
    return rv;
}


RDFGenericBuilderImpl::RDFGenericBuilderImpl(void)
    : mDocument(nsnull),
      mDB(nsnull),
      mRoot(nsnull),
      mTimer(nsnull)
{
    NS_INIT_REFCNT();
}

RDFGenericBuilderImpl::~RDFGenericBuilderImpl(void)
{
    // NS_IF_RELEASE(mDocument) not refcounted

    --gRefCnt;
    if (gRefCnt == 0) {
        NS_RELEASE(kContainerAtom);
        NS_RELEASE(kLazyContentAtom);
        NS_RELEASE(kIsContainerAtom);
        NS_RELEASE(kIsEmptyAtom);
        NS_RELEASE(kXULContentsGeneratedAtom);
        NS_RELEASE(kTemplateContentsGeneratedAtom);
        NS_RELEASE(kContainerContentsGeneratedAtom);

        NS_RELEASE(kIdAtom);
        NS_RELEASE(kPersistAtom);
        NS_RELEASE(kOpenAtom);
        NS_RELEASE(kEmptyAtom);
        NS_RELEASE(kResourceAtom);
        NS_RELEASE(kURIAtom);
        NS_RELEASE(kContainmentAtom);
        NS_RELEASE(kIgnoreAtom);
        NS_RELEASE(kRefAtom);
        NS_RELEASE(kValueAtom);
        NS_RELEASE(kNaturalOrderPosAtom);

        NS_RELEASE(kTemplateAtom);
        NS_RELEASE(kRuleAtom);
        NS_RELEASE(kTextAtom);
        NS_RELEASE(kPropertyAtom);
        NS_RELEASE(kInstanceOfAtom);

        NS_RELEASE(kTreeAtom);
        NS_RELEASE(kTreeChildrenAtom);
        NS_RELEASE(kTreeItemAtom);

        NS_RELEASE(kNC_Title);
        NS_RELEASE(kNC_child);
        NS_RELEASE(kNC_Column);
        NS_RELEASE(kNC_Folder);
        NS_RELEASE(kRDF_child);
        NS_RELEASE(kRDF_instanceOf);
        NS_RELEASE(kXUL_element);

	if (mLocalstore)
	{
		// XXX Currently, need to flush localstore as its being leaked
		// and thus never written out to disk otherwise

		nsCOMPtr<nsIRDFRemoteDataSource>	remoteLocalStore = do_QueryInterface(mLocalstore);
		if (remoteLocalStore)
		{
			remoteLocalStore->Flush();
		}

		NS_RELEASE(mLocalstore);
		mLocalstore = nsnull;
		persistLock = PR_FALSE;
	}

        nsServiceManager::ReleaseService(kRDFServiceCID, gRDFService);
        nsServiceManager::ReleaseService(kRDFContainerUtilsCID, gRDFContainerUtils);
        nsServiceManager::ReleaseService(kXULSortServiceCID, gXULSortService);
        NS_RELEASE(gNameSpaceManager);
        NS_IF_RELEASE(gHTMLElementFactory);
    }
}


nsresult
RDFGenericBuilderImpl::Init()
{
    if (gRefCnt++ == 0) {
        kContainerAtom                  = NS_NewAtom("container");
        kLazyContentAtom                = NS_NewAtom("lazycontent");
        kIsContainerAtom                = NS_NewAtom("iscontainer");
        kIsEmptyAtom                    = NS_NewAtom("isempty");
        kXULContentsGeneratedAtom       = NS_NewAtom("xulcontentsgenerated");
        kTemplateContentsGeneratedAtom  = NS_NewAtom("templatecontentsgenerated");
        kContainerContentsGeneratedAtom = NS_NewAtom("containercontentsgenerated");

        kIdAtom              = NS_NewAtom("id");
        kPersistAtom         = NS_NewAtom("persist");
        kOpenAtom            = NS_NewAtom("open");
        kEmptyAtom           = NS_NewAtom("empty");
        kResourceAtom        = NS_NewAtom("resource");
        kURIAtom             = NS_NewAtom("uri");
        kContainmentAtom     = NS_NewAtom("containment");
        kIgnoreAtom          = NS_NewAtom("ignore");
        kRefAtom             = NS_NewAtom("ref");
        kValueAtom           = NS_NewAtom("value");
        kNaturalOrderPosAtom = NS_NewAtom("pos");

        kTemplateAtom        = NS_NewAtom("template");
        kRuleAtom            = NS_NewAtom("rule");

        kTextAtom            = NS_NewAtom("text");
        kPropertyAtom        = NS_NewAtom("property");
        kInstanceOfAtom      = NS_NewAtom("instanceOf");

        kTreeAtom            = NS_NewAtom("tree");
        kTreeChildrenAtom    = NS_NewAtom("treechildren");
        kTreeItemAtom        = NS_NewAtom("treeitem");

        nsresult rv;

        // Register the XUL and RDF namespaces: these'll just retrieve
        // the IDs if they've already been registered by someone else.
        rv = nsComponentManager::CreateInstance(kNameSpaceManagerCID,
                                                nsnull,
                                                nsCOMTypeInfo<nsINameSpaceManager>::GetIID(),
                                                (void**) &gNameSpaceManager);
        NS_ASSERTION(NS_SUCCEEDED(rv), "unable to create namespace manager");
        if (NS_FAILED(rv)) return rv;

        // XXX This is sure to change. Copied from mozilla/layout/xul/content/src/nsXULAtoms.cpp
        static const char kXULNameSpaceURI[]
            = "http://www.mozilla.org/keymaster/gatekeeper/there.is.only.xul";

        static const char kRDFNameSpaceURI[]
            = RDF_NAMESPACE_URI;

        rv = gNameSpaceManager->RegisterNameSpace(kXULNameSpaceURI, kNameSpaceID_XUL);
        NS_ASSERTION(NS_SUCCEEDED(rv), "unable to register XUL namespace");
        if (NS_FAILED(rv)) return rv;

        rv = gNameSpaceManager->RegisterNameSpace(kRDFNameSpaceURI, kNameSpaceID_RDF);
        NS_ASSERTION(NS_SUCCEEDED(rv), "unable to register RDF namespace");
        if (NS_FAILED(rv)) return rv;


        // Initialize the global shared reference to the service
        // manager and get some shared resource objects.
        rv = nsServiceManager::GetService(kRDFServiceCID,
                                          kIRDFServiceIID,
                                          (nsISupports**) &gRDFService);
        if (NS_FAILED(rv)) return rv;

        gRDFService->GetResource(NC_NAMESPACE_URI "Title",   &kNC_Title);
        gRDFService->GetResource(NC_NAMESPACE_URI "child",   &kNC_child);
        gRDFService->GetResource(NC_NAMESPACE_URI "Column",  &kNC_Column);
        gRDFService->GetResource(NC_NAMESPACE_URI "Folder",  &kNC_Folder);
        gRDFService->GetResource(RDF_NAMESPACE_URI "child",  &kRDF_child);
        gRDFService->GetResource(RDF_NAMESPACE_URI "instanceOf", &kRDF_instanceOf);
        gRDFService->GetResource(XUL_NAMESPACE_URI "element",    &kXUL_element);

	persistLock = PR_FALSE;
        rv = gRDFService->GetDataSource("rdf:local-store", &mLocalstore);
	if (NS_FAILED(rv))
	{
		// failure merely means we can't use persistence
		mLocalstore = nsnull;
	}

        rv = nsServiceManager::GetService(kRDFContainerUtilsCID,
                                          nsIRDFContainerUtils::GetIID(),
                                          (nsISupports**) &gRDFContainerUtils);
        if (NS_FAILED(rv)) return rv;

        rv = nsServiceManager::GetService(kXULSortServiceCID,
                                          kIXULSortServiceIID,
                                          (nsISupports**) &gXULSortService);
        if (NS_FAILED(rv)) return rv;

        rv = nsComponentManager::CreateInstance(kHTMLElementFactoryCID,
                                                nsnull,
                                                kIHTMLElementFactoryIID,
                                                (void**) &gHTMLElementFactory);
        if (NS_FAILED(rv)) return rv;
    }

#ifdef PR_LOGGING
    if (! gLog)
        gLog = PR_NewLogModule("nsRDFGenericBuilder");
#endif

    return NS_OK;
}

////////////////////////////////////////////////////////////////////////

NS_IMPL_ADDREF(RDFGenericBuilderImpl);
NS_IMPL_RELEASE(RDFGenericBuilderImpl);

NS_IMETHODIMP
RDFGenericBuilderImpl::QueryInterface(REFNSIID iid, void** aResult)
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
RDFGenericBuilderImpl::SetDocument(nsIRDFDocument* aDocument)
{
    // note: null now allowed, it indicates document going away

    mDocument = aDocument; // not refcounted
    return NS_OK;
}


NS_IMETHODIMP
RDFGenericBuilderImpl::SetDataBase(nsIRDFCompositeDataSource* aDataBase)
{
    NS_PRECONDITION(mRoot != nsnull, "not initialized");
    if (! mRoot)
        return NS_ERROR_NOT_INITIALIZED;

    
    if (mDB)
        mDB->RemoveObserver(this);

    mDB = dont_QueryInterface(aDataBase);

    if (mDB) {
        mDB->AddObserver(this);

        // Now set the database on the element, so that script writers can
        // access it.
        nsCOMPtr<nsIDOMXULElement> element( do_QueryInterface(mRoot) );
        if (element) {
            nsresult rv;
            rv = element->SetDatabase(aDataBase);
            if (NS_FAILED(rv)) return rv;
        }
        else {
            // Hmm. This must be an HTML element. Maybe try to set it
            // as a JS property "by hand"?
        }
    }

    return NS_OK;
}


NS_IMETHODIMP
RDFGenericBuilderImpl::GetDataBase(nsIRDFCompositeDataSource** aDataBase)
{
    NS_PRECONDITION(aDataBase != nsnull, "null ptr");
    if (! aDataBase)
        return NS_ERROR_NULL_POINTER;

    *aDataBase = mDB;
    NS_ADDREF(*aDataBase);
    return NS_OK;
}


NS_IMETHODIMP
RDFGenericBuilderImpl::CreateRootContent(nsIRDFResource* aResource)
{
    // XXX Remove this method from the interface
    NS_NOTREACHED("whoops");
    return NS_ERROR_UNEXPECTED;
}

NS_IMETHODIMP
RDFGenericBuilderImpl::SetRootContent(nsIContent* aElement)
{
    mRoot = dont_QueryInterface(aElement);
    return NS_OK;
}


NS_IMETHODIMP
RDFGenericBuilderImpl::CreateContents(nsIContent* aElement)
{
    NS_PRECONDITION(aElement != nsnull, "null ptr");
    if (! aElement)
        return NS_ERROR_NULL_POINTER;

    // First, make sure that the element is in the right widget -- ours.
    if (!IsElementInWidget(aElement))
        return NS_OK;

    nsresult rv;

    nsCOMPtr<nsIRDFResource> resource;
    rv = nsRDFContentUtils::GetElementRefResource(aElement, getter_AddRefs(resource));
    if (NS_SUCCEEDED(rv)) {
        // The element has a resource; that means that it corresponds
        // to something in the graph, so we need to go to the graph to
        // create its contents.
        rv = CreateContainerContents(aElement, resource, PR_FALSE);
        if (NS_FAILED(rv)) return rv;
    }

    nsAutoString templateID;
    rv = aElement->GetAttribute(kNameSpaceID_None, kTemplateAtom, templateID);
    if (NS_FAILED(rv)) return rv;

    // Hmm, this isn't a template node after all. Not sure _what_ it is.
    if (rv == NS_CONTENT_ATTR_HAS_VALUE) {
        rv = CreateTemplateContents(aElement, templateID);
        if (NS_FAILED(rv)) return rv;
    }

    return NS_OK;
}


NS_IMETHODIMP
RDFGenericBuilderImpl::OpenContainer(nsIContent* aElement)
{
    nsresult rv;

    // First, make sure that the element is in the right widget -- ours.
    if (!IsElementInWidget(aElement))
        return NS_OK;

    nsCOMPtr<nsIRDFResource> resource;
    rv = nsRDFContentUtils::GetElementRefResource(aElement, getter_AddRefs(resource));

    // If it has no resource, there's nothing that we need to be
    // concerned about here.
    if (NS_FAILED(rv))
        return NS_OK;

    // The element has a resource; that means that it corresponds
    // to something in the graph, so we need to go to the graph to
    // create its contents.
    rv = CreateContainerContents(aElement, resource, PR_TRUE);
    if (NS_FAILED(rv)) return rv;

    if (rv == NS_RDF_ELEMENT_WAS_CREATED) {
        nsCOMPtr<nsIAtom> tag;
        rv = aElement->GetTag(*getter_AddRefs(tag));
        if (NS_FAILED(rv)) return rv;

        if (tag.get() == kTreeItemAtom) {
            nsCOMPtr<nsIContent> insertionpoint;
            rv = FindInsertionPoint(aElement, getter_AddRefs(insertionpoint));
            if (NS_FAILED(rv)) return rv;

            if (! insertionpoint) {
                // No content got built. Bail!
                return NS_OK;
            }

            // Find the first element beneath the insertion point that
            // is generated from a template.
            PRInt32 indx;
            rv = FindFirstGeneratedChild(insertionpoint, &indx);
            if (NS_FAILED(rv)) return rv;

            if (indx != -1) {
                nsCOMPtr<nsIDocument> doc = do_QueryInterface(mDocument);
                if (! doc)
                    return NS_ERROR_UNEXPECTED;

                rv = doc->ContentAppended(insertionpoint, indx);
                if (NS_FAILED(rv)) return rv;
            }
        }
    }

    return NS_OK;
}

NS_IMETHODIMP
RDFGenericBuilderImpl::CloseContainer(nsIContent* aElement)
{
    NS_PRECONDITION(aElement != nsnull, "null ptr");
    if (! aElement)
        return NS_ERROR_NULL_POINTER;

    // First, make sure that the element is in the right widget -- ours.
    if (!IsElementInWidget(aElement))
        return NS_OK;

    nsresult rv;

    nsCOMPtr<nsIAtom> tag;
    rv = aElement->GetTag(*getter_AddRefs(tag));
    if (NS_FAILED(rv)) return rv;

    // If it's not a tree, just bail. Keep the content around until
    // the cows come home.
    if (tag.get() != kTreeItemAtom)
        return NS_OK;

    // Find the tag that contains the children so that we can remove
    // all of the children.
    //
    // XXX We do this as a (premature?) optimization so that nodes
    // which are not being displayed don't hang around taking up
    // space. Unfortunately, the tree widget currently _relies_ on
    // this behavior and will break if we don't do it :-(.
    nsCOMPtr<nsIContent> insertionpoint;
    rv = FindInsertionPoint(aElement, getter_AddRefs(insertionpoint));
    if (NS_FAILED(rv)) return rv;

    if (insertionpoint) {
        PRInt32 count;
        rv = insertionpoint->ChildCount(count);
        if (NS_FAILED(rv)) return rv;

        rv = RemoveGeneratedContent(insertionpoint);
        if (NS_FAILED(rv)) return rv;
    }

    // Clear the contents-generated attribute so that the next time we
    // come back, we'll regenerate the kids we just killed.
    rv = aElement->UnsetAttribute(kNameSpaceID_None,
                                  kContainerContentsGeneratedAtom,
                                  PR_FALSE);
    if (NS_FAILED(rv)) return rv;

    // XXX Hack. Setting this attribute forces the RDF element to
    // remember that it needs to be re-generated next time around.
	rv = aElement->SetAttribute(kNameSpaceID_None,
                                kLazyContentAtom,
                                "true",
                                PR_FALSE);
	if (NS_FAILED(rv)) return rv;

    return NS_OK;
}


NS_IMETHODIMP
RDFGenericBuilderImpl::RebuildContainer(nsIContent* aElement)
{
    nsresult rv;

    // Remove any generated children from this node
    rv = RemoveGeneratedContent(aElement);
    if (NS_FAILED(rv)) return rv;

    // Clear the contents-generated attribute so that the next time we
    // come back, we'll regenerate the kids we just killed.
    rv = aElement->UnsetAttribute(kNameSpaceID_None,
                                  kContainerContentsGeneratedAtom,
                                  PR_FALSE);
    if (NS_FAILED(rv)) return rv;

    // XXX Hack. Setting this attribute forces the RDF element to
    // remember that it needs to be re-generated next time around.
	rv = aElement->SetAttribute(kNameSpaceID_None,
                                kLazyContentAtom,
                                "true",
                                PR_FALSE);
	if (NS_FAILED(rv)) return rv;

    // Since we're changing the 'ref' attribute, we'll need to
    // rebuild content for _this_ resource template, too.
    rv = aElement->UnsetAttribute(kNameSpaceID_None,
                                  kTemplateContentsGeneratedAtom,
                                  PR_FALSE);
    if (NS_FAILED(rv)) return rv;

    // Do it!!!
    rv = CreateContents(aElement);
    if (NS_FAILED(rv)) return rv;

    // Now see where on earth the content was _really_ appended so we
    // can tell the frames to go reflow themselves. Start with _this_
    // element.
    nsCOMPtr<nsIContent> insertionpoint = dont_QueryInterface(aElement);

    PRInt32 indx;
    rv = FindFirstGeneratedChild(insertionpoint, &indx);
    if (NS_FAILED(rv)) return rv;

    if (indx == -1) {
        // Okay, nothing got inserted directly beneath this node; see
        // if the it was inserted somewhere _below_ us...
        rv = FindInsertionPoint(aElement, getter_AddRefs(insertionpoint));
        if (NS_FAILED(rv)) return rv;

        if ((insertionpoint != nsnull) && (insertionpoint.get() != aElement)) {
            rv = FindFirstGeneratedChild(insertionpoint, &indx);
            if (NS_FAILED(rv)) return rv;
        }
    }

    if (indx != -1) {
        nsCOMPtr<nsIDocument> doc = do_QueryInterface(mDocument);

        rv = doc->ContentAppended(insertionpoint, indx);
        if (NS_FAILED(rv)) return rv;
    }

    return NS_OK;
}


NS_IMETHODIMP
RDFGenericBuilderImpl::CreateElement(PRInt32 aNameSpaceID,
                                     nsIAtom* aTag,
                                     nsIRDFResource* aResource,
                                     nsIContent** aResult)
{
    nsresult rv;
    nsCOMPtr<nsIContent> result;

    if (aNameSpaceID == kNameSpaceID_HTML) {
        nsCOMPtr<nsIHTMLContent> element;
        /*const*/ PRUnichar *tagName;
        aTag->GetUnicode(&tagName);

        rv = gHTMLElementFactory->CreateInstanceByTag(tagName, getter_AddRefs(element));
        if (NS_FAILED(rv)) return rv;

        result = do_QueryInterface(element);
        if (! result)
            return NS_ERROR_UNEXPECTED;
    }
    else {
        rv = NS_NewRDFElement(aNameSpaceID, aTag, getter_AddRefs(result));
        if (NS_FAILED(rv)) return rv;
    }

    nsCOMPtr<nsIDocument> doc( do_QueryInterface(mDocument) );

    if (aResource) {
        const char *uri;
        rv = aResource->GetValueConst(&uri);
        NS_ASSERTION(NS_SUCCEEDED(rv), "unable to get resource URI");
        if (NS_FAILED(rv)) return rv;

        nsAutoString id;
        rv = nsRDFContentUtils::MakeElementID(doc, nsAutoString(uri), id);
        if (NS_FAILED(rv)) return rv;

        rv = result->SetAttribute(kNameSpaceID_None, kIdAtom, id, PR_FALSE);
        NS_ASSERTION(NS_SUCCEEDED(rv), "unable to set id attribute");
        if (NS_FAILED(rv)) return rv;
    }

    rv = result->SetDocument(doc, PR_FALSE);
    NS_ASSERTION(NS_SUCCEEDED(rv), "unable to set element's document");
    if (NS_FAILED(rv)) return rv;

    if (aResource && aNameSpaceID == kNameSpaceID_HTML) {
        // If this is an HTML element, then explicitly add it to the
        // map. (XUL elements don't have to do this because their
        // SetDocument() call does the magic.) Don't worry: the
        // document observer methods are on the lookout to update the
        // map for "attribute changed" calls that monkey with the 'id'
        // or 'ref' parameters.
        rv = mDocument->AddElementForResource(aResource, result);
        if (NS_FAILED(rv)) return rv;
    }

    *aResult = result;
    NS_ADDREF(*aResult);
    return NS_OK;
}

////////////////////////////////////////////////////////////////////////
// nsIRDFObserver interface

NS_IMETHODIMP
RDFGenericBuilderImpl::OnAssert(nsIRDFResource* aSource,
                                nsIRDFResource* aProperty,
                                nsIRDFNode* aTarget)
{
	// Just silently fail, because this can happen "normally" as part
	// of tear-down code. (Bug 9098)
    if (! mDocument)
        return NS_OK;

    nsresult rv;

    nsCOMPtr<nsISupportsArray> elements;
    rv = NS_NewISupportsArray(getter_AddRefs(elements));
	NS_ASSERTION(NS_SUCCEEDED(rv), "unable to create new ISupportsArray");
	if (NS_FAILED(rv)) return rv;

    // Find all the elements in the content model that correspond to
    // aSource: for each, we'll try to build XUL children if
    // appropriate.
    rv = mDocument->GetElementsForResource(aSource, elements);
	NS_ASSERTION(NS_SUCCEEDED(rv), "unable to retrieve elements from resource");
	if (NS_FAILED(rv)) return rv;

    PRUint32 cnt;
    rv = elements->Count(&cnt);
    if (NS_FAILED(rv)) return rv;

    for (PRInt32 i = PRInt32(cnt) - 1; i >= 0; --i) {
		nsISupports* isupports = elements->ElementAt(i);
		nsCOMPtr<nsIContent> element( do_QueryInterface(isupports) );
		NS_IF_RELEASE(isupports);

        // XXX somehow figure out if building XUL kids on this
        // particular element makes any sense whatsoever.

        // We'll start by making sure that the element at least has
        // the same parent has the content model builder's root
        if (!IsElementInWidget(element))
            continue;
        
        nsCOMPtr<nsIRDFResource> resource = do_QueryInterface(aTarget);
        if (resource && IsContainmentProperty(element, aProperty)) {
            // Okay, the target  _is_ a resource, and the property is
            // a containment property. So this'll be a new item in the widget
            // control.

            // But if the contents of aElement _haven't_ yet been
            // generated, then just ignore the assertion. We do this
            // because we know that _eventually_ the contents will be
            // generated (via CreateContents()) when somebody asks for
            // them later.
            nsAutoString contentsGenerated;
            rv = element->GetAttribute(kNameSpaceID_None,
                                       kContainerContentsGeneratedAtom,
                                       contentsGenerated);
			if (NS_FAILED(rv)) return rv;

            if ((rv != NS_CONTENT_ATTR_HAS_VALUE) || !contentsGenerated.EqualsIgnoreCase("true"))
                return NS_OK;

            // Okay, it's a "live" element, so go ahead and append the new
            // child to this node.
            rv = CreateWidgetItem(element, aProperty, resource, 0, PR_TRUE);
            NS_ASSERTION(NS_SUCCEEDED(rv), "unable to create widget item");
            if (NS_FAILED(rv)) return rv;

            // Update the "empty" attribute
            nsAutoString empty;
            rv = element->GetAttribute(kNameSpaceID_None, kEmptyAtom, empty);
            if (NS_FAILED(rv)) return rv;

            if ((rv != NS_CONTENT_ATTR_HAS_VALUE) || (! empty.Equals("false"))) {
                rv = element->SetAttribute(kNameSpaceID_None, kEmptyAtom, "false", PR_TRUE);
                if (NS_FAILED(rv)) return rv;
            }
        }
        else {
            // Either the target of the assertion is not a resource,
            // or the object is a resource and the predicate is not a
            // containment property. So this won't be a new item in
            // the widget. See if we can use it to set a some
            // substructure on the current element.
            nsAutoString templateID;
            rv = element->GetAttribute(kNameSpaceID_None,
                                       kTemplateAtom,
                                       templateID);
            if (NS_FAILED(rv)) return rv;

            if (rv == NS_CONTENT_ATTR_HAS_VALUE) {
                nsCOMPtr<nsIDOMXULDocument>	xulDoc;
                xulDoc = do_QueryInterface(mDocument);
                if (! xulDoc)
                    return NS_ERROR_UNEXPECTED;

                nsCOMPtr<nsIDOMElement>	domElement;
                rv = xulDoc->GetElementById(templateID, getter_AddRefs(domElement));
                NS_ASSERTION(NS_SUCCEEDED(rv), "unable to find template node");
                if (NS_FAILED(rv)) return rv;

                nsCOMPtr<nsIContent> templateNode = do_QueryInterface(domElement);
                if (! templateNode)
                    return NS_ERROR_UNEXPECTED;

                // this node was created by a XUL template, so update it accordingly
                rv = SynchronizeUsingTemplate(templateNode, element, eSet, aProperty, aTarget);
                if (NS_FAILED(rv)) return rv;
            }
        }
    }
    return NS_OK;
}

NS_IMETHODIMP
RDFGenericBuilderImpl::OnUnassert(nsIRDFResource* aSource,
                                  nsIRDFResource* aProperty,
                                  nsIRDFNode* aTarget)
{
	// Just silently fail, because this can happen "normally" as part
	// of tear-down code. (Bug 9098)
    if (! mDocument)
        return NS_OK;

    nsresult rv;

    nsCOMPtr<nsISupportsArray> elements;
    rv = NS_NewISupportsArray(getter_AddRefs(elements));
	NS_ASSERTION(NS_SUCCEEDED(rv), "unable to create new ISupportsArray");
	if (NS_FAILED(rv)) return rv;

    // Find all the elements in the content model that correspond to
    // aSource: for each, we'll try to build XUL children if
    // appropriate.
    rv = mDocument->GetElementsForResource(aSource, elements);
	NS_ASSERTION(NS_SUCCEEDED(rv), "unable to retrieve elements from resource");
	if (NS_FAILED(rv)) return rv;

    PRUint32 cnt;
    rv = elements->Count(&cnt);
    if (NS_FAILED(rv)) return rv;

    for (PRInt32 i = cnt - 1; i >= 0; --i) {
		nsISupports* isupports = elements->ElementAt(i);
        nsCOMPtr<nsIContent> element( do_QueryInterface(isupports) );
		NS_IF_RELEASE(isupports);
        
        // XXX somehow figure out if building XUL kids on this
        // particular element makes any sense whatsoever.

        // We'll start by making sure that the element at least has
        // the same parent has the content model builder's root
        if (!IsElementInWidget(element))
            continue;
        
        nsCOMPtr<nsIRDFResource> resource = do_QueryInterface(aTarget);
        if (resource && IsContainmentProperty(element, aProperty)) {
            // Okay, the object _is_ a resource, and the predicate is
            // a containment property. So we'll need to remove this
            // item from the widget.

            // But if the contents of aElement _haven't_ yet been
            // generated, then just ignore the unassertion: nothing is
            // in the content model to remove.
            nsAutoString contentsGenerated;
            rv = element->GetAttribute(kNameSpaceID_None,
                                       kContainerContentsGeneratedAtom,
                                       contentsGenerated);
            if (NS_FAILED(rv)) return rv;

            if ((rv != NS_CONTENT_ATTR_HAS_VALUE) || !contentsGenerated.EqualsIgnoreCase("true"))
                return NS_OK;

            // Okay, it's a "live" element, so go ahead and remove the
            // child from this node.
            rv = RemoveWidgetItem(element, aProperty, resource, PR_TRUE);
            NS_ASSERTION(NS_SUCCEEDED(rv), "unable to remove widget item");
            if (NS_FAILED(rv)) return rv;

            PRInt32	numKids;
            rv = element->ChildCount(numKids);
            if (NS_FAILED(rv)) return rv;

            if (numKids == 0) {
                nsAutoString empty;
                rv = element->GetAttribute(kNameSpaceID_None, kEmptyAtom, empty);
                if (NS_FAILED(rv)) return rv;

                if ((rv != NS_CONTENT_ATTR_HAS_VALUE) && (! empty.Equals("true"))) {
                    rv = element->SetAttribute(kNameSpaceID_None, kEmptyAtom, "true", PR_TRUE);
                    if (NS_FAILED(rv)) return rv;
                }
            }
            
        }
        else {
            // Either the target of the assertion is not a resource,
            // or the target is a resource and the property is not a
            // containment property. So this won't be an item in the
            // widget. See if we need to tear down some substructure
            // on the current element.
            nsAutoString templateID;
            rv = element->GetAttribute(kNameSpaceID_None,
                                       kTemplateAtom,
                                       templateID);
            if (NS_FAILED(rv)) return rv;

            if (rv == NS_CONTENT_ATTR_HAS_VALUE) {
				nsCOMPtr<nsIDOMXULDocument>	xulDoc;
				xulDoc = do_QueryInterface(mDocument);
                if (! xulDoc)
                    return NS_ERROR_UNEXPECTED;

				nsCOMPtr<nsIDOMElement>	domElement;
                rv = xulDoc->GetElementById(templateID, getter_AddRefs(domElement));
                NS_ASSERTION(NS_SUCCEEDED(rv), "unable to find template node");
                if (NS_FAILED(rv)) return rv;

				nsCOMPtr<nsIContent> templateNode = do_QueryInterface(domElement);
                if (! templateNode)
                    return NS_ERROR_UNEXPECTED;

                // this node was created by a XUL template, so update it accordingly
                rv = SynchronizeUsingTemplate(templateNode, element, eClear, aProperty, aTarget);
                if (NS_FAILED(rv)) return rv;
            }
        }
    }
    return NS_OK;
}


NS_IMETHODIMP
RDFGenericBuilderImpl::OnChange(nsIRDFResource* aSource,
								nsIRDFResource* aProperty,
								nsIRDFNode* aOldTarget,
								nsIRDFNode* aNewTarget)
{
	// Just silently fail, because this can happen "normally" as part
	// of tear-down code. (Bug 9098)
    if (! mDocument)
        return NS_OK;

    nsresult rv;

    nsCOMPtr<nsISupportsArray> elements;
    rv = NS_NewISupportsArray(getter_AddRefs(elements));
	NS_ASSERTION(NS_SUCCEEDED(rv), "unable to create new ISupportsArray");
	if (NS_FAILED(rv)) return rv;

    // Find all the elements in the content model that correspond to
    // aSource: for each, we'll try to build XUL children if
    // appropriate.
    rv = mDocument->GetElementsForResource(aSource, elements);
	NS_ASSERTION(NS_SUCCEEDED(rv), "unable to retrieve elements from resource");
	if (NS_FAILED(rv)) return rv;

    PRUint32 cnt;
    rv = elements->Count(&cnt);
    if (NS_FAILED(rv)) return rv;

    for (PRInt32 i = cnt - 1; i >= 0; --i) {
		nsISupports* isupports = elements->ElementAt(i);
        nsCOMPtr<nsIContent> element( do_QueryInterface(isupports) );
		NS_IF_RELEASE(isupports);
        
        // XXX somehow figure out if building XUL kids on this
        // particular element makes any sense whatsoever.

        // We'll start by making sure that the element at least has
        // the same parent has the content model builder's root
        if (!IsElementInWidget(element))
            continue;
        
        nsCOMPtr<nsIRDFResource> oldresource = do_QueryInterface(aOldTarget);
        if (oldresource && IsContainmentProperty(element, aProperty)) {
            // Okay, the object _is_ a resource, and the predicate is
            // a containment property. So we'll need to remove the old
            // item from the widget, and then add the new one.

            // But if the contents of aElement _haven't_ yet been
            // generated, then just ignore: nothing is in the content
            // model to remove.
            nsAutoString contentsGenerated;
            rv = element->GetAttribute(kNameSpaceID_None,
                                       kContainerContentsGeneratedAtom,
                                       contentsGenerated);
            if (NS_FAILED(rv)) return rv;

            if ((rv != NS_CONTENT_ATTR_HAS_VALUE) || !contentsGenerated.EqualsIgnoreCase("true"))
                return NS_OK;

            // Okay, it's a "live" element, so go ahead and remove the
            // child from this node.
            rv = RemoveWidgetItem(element, aProperty, oldresource, PR_TRUE);
            NS_ASSERTION(NS_SUCCEEDED(rv), "unable to remove widget item");
            if (NS_FAILED(rv)) return rv;

            nsCOMPtr<nsIRDFResource> newresource = do_QueryInterface(aNewTarget);

            // XXX Tough. We don't handle the case where they've
            // changed a resource to a literal. But this is
            // bizarre anyway.
            if (! newresource)
                return NS_OK;

            rv = CreateWidgetItem(element, aProperty, newresource, 0, PR_TRUE);
            if (NS_FAILED(rv)) return rv;
        }
        else {
            // Either the target of the assertion is not a resource,
            // or the target is a resource and the property is not a
            // containment property. So this won't be an item in the
            // widget. See if we need to tear down some substructure
            // on the current element.
            nsAutoString templateID;
            rv = element->GetAttribute(kNameSpaceID_None,
                                       kTemplateAtom,
                                       templateID);
            if (NS_FAILED(rv)) return rv;

            if (rv == NS_CONTENT_ATTR_HAS_VALUE) {
				nsCOMPtr<nsIDOMXULDocument>	xulDoc;
				xulDoc = do_QueryInterface(mDocument);
                if (! xulDoc)
                    return NS_ERROR_UNEXPECTED;

				nsCOMPtr<nsIDOMElement>	domElement;
                rv = xulDoc->GetElementById(templateID, getter_AddRefs(domElement));
                NS_ASSERTION(NS_SUCCEEDED(rv), "unable to find template node");
                if (NS_FAILED(rv)) return rv;

				nsCOMPtr<nsIContent> templateNode = do_QueryInterface(domElement);
                if (! templateNode)
                    return NS_ERROR_UNEXPECTED;

                // this node was created by a XUL template, so update it accordingly
                rv = SynchronizeUsingTemplate(templateNode, element, eSet, aProperty, aNewTarget);
                if (NS_FAILED(rv)) return rv;
            }
        }
    }
    return NS_OK;
}


NS_IMETHODIMP
RDFGenericBuilderImpl::OnMove(nsIRDFResource* aOldSource,
							  nsIRDFResource* aNewSource,
							  nsIRDFResource* aProperty,
							  nsIRDFNode* aTarget)
{
	NS_NOTYETIMPLEMENTED("write me");
	return NS_ERROR_NOT_IMPLEMENTED;
}


#if 0
PRBool
RDFGenericBuilderImpl::IsAttributePersisent(nsIContent *element, PRInt32 aNameSpaceID, nsIAtom *aAtom)
{
	PRBool		persistFlag = PR_FALSE;
	nsresult	rv;
	nsAutoString	persistValue;

	if (NS_SUCCEEDED(rv = element->GetAttribute(kNameSpaceID_None, kPersistAtom, persistValue))
		&& (rv == NS_CONTENT_ATTR_HAS_VALUE) )
	{
		nsAutoString	tagName;
		aAtom->ToString(tagName);
		if (tagName.Length() > 0)
		{
			// XXX should really be a bit smarter with matching...
			// i.e. break up items in-between spaces and then compare
			if (persistValue.Find(tagName, PR_TRUE) >= 0)
			{
				persistFlag = PR_TRUE;
			}
		}
	}
	return(persistFlag);
}


void
RDFGenericBuilderImpl::GetPersistentAttributes(nsIContent *element)
{
	if ((!mLocalstore) || (!mDocument))	return;

	nsresult			rv;
	nsCOMPtr<nsIRDFResource>	elementRes;
	if (NS_FAILED(rv = nsRDFContentUtils::GetElementResource(element, getter_AddRefs(elementRes))))	return;

	nsAutoString			persistValue;
	if (NS_FAILED(rv = element->GetAttribute(kNameSpaceID_None, kPersistAtom, persistValue))
		|| (rv != NS_CONTENT_ATTR_HAS_VALUE))	return;

	nsAutoString	attribute;
	while(persistValue.Length() > 0)
	{
		attribute.Truncate();

		// space or comma separated list of attributes
		PRInt32		offset;
		if ((offset = persistValue.FindCharInSet(" ,")) > 0)
		{
			persistValue.Left(attribute, offset);
			persistValue.Cut(0, offset + 1);
		}
		else
		{
			attribute = persistValue;
			persistValue.Truncate();
		}
		attribute.Trim(" ");

		if (attribute.Length() > 0)
		{
			char	*attributeStr = attribute.ToNewCString();
			if (!attributeStr)	break;
			nsCOMPtr<nsIAtom>	attributeAtom;
			attributeAtom = NS_NewAtom(attributeStr);
			delete [] attributeStr;

			nsCOMPtr<nsIRDFResource>	propertyRes;
			if (NS_FAILED(rv = GetResource(kNameSpaceID_None, attributeAtom, getter_AddRefs(propertyRes))))	return;
			
			nsCOMPtr<nsIRDFNode>	target;
			if (NS_SUCCEEDED(rv = mLocalstore->GetTarget(elementRes, propertyRes, PR_TRUE,
				getter_AddRefs(target))) && (rv != NS_RDF_NO_VALUE))
			{
				persistLock = PR_TRUE;

				// XXX Don't OnAssert the persistant attribute;
				// just set it in the content model instead
				// OnAssert(elementRes, propertyRes, target);

				PRInt32			nameSpaceID;
				nsCOMPtr<nsIAtom>	tag;
				if (NS_SUCCEEDED(rv = mDocument->SplitProperty(propertyRes, &nameSpaceID, getter_AddRefs(tag))))
				{
					nsCOMPtr<nsIRDFLiteral>	literal = do_QueryInterface(target);
					if (literal)
					{
						const	PRUnichar	*uniLiteral = nsnull;
						literal->GetValueConst(&uniLiteral);
						if (uniLiteral)
						{
							rv = element->SetAttribute(nameSpaceID, tag, uniLiteral, PR_FALSE);
						}
					}
				}

				persistLock = PR_FALSE;
			}
		}
	}
}


void
RDFGenericBuilderImpl::PersistAttribute(nsIContent *element, PRInt32 aNameSpaceID,
					nsIAtom *aAtom, nsString aValue, eUpdateAction action)
{
	nsresult	rv = NS_OK;
	
	if ((!mLocalstore) || (!mDocument))	return;

	if (persistLock == PR_TRUE)	return;

	if (IsAttributePersisent(element, aNameSpaceID, aAtom) == PR_FALSE)	return;

	nsCOMPtr<nsIRDFResource>	elementRes;
	if (NS_FAILED(rv = nsRDFContentUtils::GetElementResource(element, getter_AddRefs(elementRes))))	return;

	nsCOMPtr<nsIRDFResource>	propertyRes;
	if (NS_FAILED(rv = GetResource(kNameSpaceID_None, aAtom, getter_AddRefs(propertyRes))))	return;

	nsCOMPtr<nsIRDFLiteral>		newLiteral;
	if (NS_FAILED(rv = gRDFService->GetLiteral(aValue.GetUnicode(), getter_AddRefs(newLiteral))))	return;
	nsCOMPtr<nsIRDFNode>		newTarget = do_QueryInterface(newLiteral);
	if (!newTarget)	return;

	if (action == eSet)
	{
		nsCOMPtr<nsIRDFNode>	oldTarget;
		if (NS_SUCCEEDED(rv = mLocalstore->GetTarget(elementRes, propertyRes, PR_TRUE,
			getter_AddRefs(oldTarget))) && (rv != NS_RDF_NO_VALUE))
		{
			rv = mLocalstore->Change(elementRes, propertyRes, oldTarget, newTarget );
		}
		else
		{
			rv = mLocalstore->Assert(elementRes, propertyRes, newTarget, PR_TRUE );
		}
	}
	else if (action == eClear)
	{
		rv = mLocalstore->Unassert(elementRes, propertyRes, newTarget);
	}

	// XXX Currently, need to flush localstore as its being leaked
	// and thus never written out to disk otherwise

	nsCOMPtr<nsIRDFRemoteDataSource>	remoteLocalStore = do_QueryInterface(mLocalstore);
	if (remoteLocalStore)
	{
		remoteLocalStore->Flush();
	}
}


void
RDFGenericBuilderImpl::PersistProperty(nsIContent *element, nsIRDFResource *aProperty,
					nsIRDFNode *aTarget, eUpdateAction action)
{
	nsresult	rv = NS_OK;
	
	if ((mLocalstore) && (mDocument))
	{
		PRInt32			nameSpaceID;
		nsCOMPtr<nsIAtom>	tag;
		if (NS_SUCCEEDED(rv = mDocument->SplitProperty(aProperty, &nameSpaceID, getter_AddRefs(tag))))
		{
			nsAutoString	value;
			if (NS_SUCCEEDED(rv = nsRDFContentUtils::GetTextForNode(aTarget, value)))
			{
				PersistAttribute(element, nameSpaceID, tag, value, action);
			}
		}
	}
}
#endif // 0


////////////////////////////////////////////////////////////////////////
// Implementation methods

nsresult
RDFGenericBuilderImpl::IsTemplateRuleMatch(nsIContent* aElement,
                                           nsIRDFResource *aProperty,
                                           nsIRDFResource* aChild,
                                           nsIContent *aRule,
                                           PRBool *aIsMatch)
{
    nsresult rv;

	PRInt32	count;
	rv = aRule->GetAttributeCount(count);
    if (NS_FAILED(rv)) return(rv);

	for (PRInt32 loop=0; loop<count; loop++) {
		PRInt32 attrNameSpaceID;
		nsCOMPtr<nsIAtom> attr;
		rv = aRule->GetAttributeNameAt(loop, attrNameSpaceID, *getter_AddRefs(attr));
        if (NS_FAILED(rv)) return rv;

		// Note: some attributes must be skipped on XUL template rule subtree

		// never compare against {}:container attribute
		if ((attr.get() == kLazyContentAtom) && (attrNameSpaceID == kNameSpaceID_None))
			continue;
		// never compare against rdf:property attribute
		else if ((attr.get() == kPropertyAtom) && (attrNameSpaceID == kNameSpaceID_RDF))
			continue;
		// never compare against rdf:instanceOf attribute
		else if ((attr.get() == kInstanceOfAtom) && (attrNameSpaceID == kNameSpaceID_RDF))
			continue;
		// never compare against {}:id attribute
		else if ((attr.get() == kIdAtom) && (attrNameSpaceID == kNameSpaceID_None))
			continue;
		// never compare against {}:xulcontentsgenerated attribute
		else if ((attr.get() == kXULContentsGeneratedAtom) && (attrNameSpaceID == kNameSpaceID_None))
			continue;
		// never compare against {}:itemcontentsgenerated attribute (bogus)
		else if ((attr.get() == kTemplateContentsGeneratedAtom) && (attrNameSpaceID == kNameSpaceID_None))
			continue;

        nsAutoString value;
        rv = aRule->GetAttribute(attrNameSpaceID, attr, value);
        if (NS_FAILED(rv)) return rv;

		if ((attrNameSpaceID == kNameSpaceID_None) && (attr.get() == kIsContainerAtom)) {
			// check and see if aChild is a container
			PRBool isContainer = IsContainer(aRule, aChild);
			if (isContainer && (!value.EqualsIgnoreCase("true"))) {
                *aIsMatch = PR_FALSE;
                return NS_OK;
            }
			else if (!isContainer && (!value.EqualsIgnoreCase("false"))) {
                *aIsMatch = PR_FALSE;
                return NS_OK;
            }
		}
        else if ((attrNameSpaceID == kNameSpaceID_None) && (attr.get() == kIsEmptyAtom)) {
            PRBool isEmpty = IsEmpty(aRule, aChild);
            if (isEmpty && (!value.EqualsIgnoreCase("true"))) {
                *aIsMatch = PR_FALSE;
                return NS_OK;
            }
            else if (!isEmpty && (!value.EqualsIgnoreCase("false"))) {
                *aIsMatch = PR_FALSE;
                return NS_OK;
            }
        }
        else {
            nsCOMPtr<nsIRDFResource> property;
            rv = GetResource(attrNameSpaceID, attr, getter_AddRefs(property));
            if (NS_FAILED(rv)) return rv;

            nsCOMPtr<nsIRDFNode> target;
            rv = mDB->GetTarget(aChild, property, PR_TRUE, getter_AddRefs(target));
            if (NS_FAILED(rv)) return rv;

            nsAutoString targetStr;
            rv = nsRDFContentUtils::GetTextForNode(target, targetStr);
            NS_ASSERTION(NS_SUCCEEDED(rv), "unable to get text for target");
            if (NS_FAILED(rv)) return rv;

            if (!value.Equals(targetStr)) {
                *aIsMatch = PR_FALSE;
                return NS_OK;
            }
        }
	}

    *aIsMatch = PR_TRUE;
	return NS_OK;
}


nsresult
RDFGenericBuilderImpl::FindTemplate(nsIContent* aElement,
                                    nsIRDFResource *aProperty,
                                    nsIRDFResource* aChild,
                                    nsIContent **aTemplate)
{
	nsresult		rv;

	*aTemplate = nsnull;

	PRInt32	count;
	rv = mRoot->ChildCount(count);
    if (NS_FAILED(rv)) return rv;

	for (PRInt32 loop=0; loop<count; loop++)
	{
        nsCOMPtr<nsIContent> tmpl;
		rv = mRoot->ChildAt(loop, *getter_AddRefs(tmpl));
        if (NS_FAILED(rv)) return rv;

		PRInt32 nameSpaceID;
		rv = tmpl->GetNameSpaceID(nameSpaceID);
        if (NS_FAILED(rv)) return rv;

		if (nameSpaceID != kNameSpaceID_XUL)
			continue;

		nsCOMPtr<nsIAtom>	tag;
		rv = tmpl->GetTag(*getter_AddRefs(tag));
        if (NS_FAILED(rv)) return rv;

		if (tag.get() != kTemplateAtom)
			continue;

		// found a template; check against any (optional) rules
		PRInt32		numRuleChildren, numRulesFound = 0;
		rv = tmpl->ChildCount(numRuleChildren);
        if (NS_FAILED(rv)) return rv;

		for (PRInt32 ruleLoop=0; ruleLoop<numRuleChildren; ruleLoop++)
		{
			nsCOMPtr<nsIContent>	aRule;
			rv = tmpl->ChildAt(ruleLoop, *getter_AddRefs(aRule));
            if (NS_FAILED(rv)) return rv;

			PRInt32	ruleNameSpaceID;
			rv = aRule->GetNameSpaceID(ruleNameSpaceID);
            if (NS_FAILED(rv)) return rv;

			if (ruleNameSpaceID != kNameSpaceID_XUL)
				continue;

			nsCOMPtr<nsIAtom>	ruleTag;
			rv = aRule->GetTag(*getter_AddRefs(ruleTag));
            if (NS_FAILED(rv)) return rv;

            if (ruleTag.get() == kRuleAtom) {
                ++numRulesFound;
                PRBool		isMatch = PR_FALSE;
                rv = IsTemplateRuleMatch(aElement, aProperty, aChild, aRule, &isMatch);
                if (NS_FAILED(rv)) return rv;

                if (isMatch) {
                    // found a matching rule, use it as the template
                    *aTemplate = aRule;
                    NS_ADDREF(*aTemplate);
                    return NS_OK;
                }
            }
		}

		if (numRulesFound == 0) {
			// if no rules are specified in the template, just use it
			*aTemplate = tmpl;
			NS_ADDREF(*aTemplate);
            return NS_OK;
		}
	}

    return NS_ERROR_FAILURE;
}



PRBool
RDFGenericBuilderImpl::IsIgnoreableAttribute(PRInt32 aNameSpaceID, nsIAtom* aAttribute)
{
    // never copy rdf:container attribute
    if ((aAttribute == kLazyContentAtom) && (aNameSpaceID == kNameSpaceID_None)) {
        return PR_TRUE;
    }
    // never copy rdf:property attribute
    else if ((aAttribute == kPropertyAtom) && (aNameSpaceID == kNameSpaceID_RDF)) {
        return PR_TRUE;
    }
    // never copy rdf:instanceOf attribute
    else if ((aAttribute == kInstanceOfAtom) && (aNameSpaceID == kNameSpaceID_RDF)) {
        return PR_TRUE;
    }
    // never copy {}:ID attribute
    else if ((aAttribute == kIdAtom) && (aNameSpaceID == kNameSpaceID_None)) {
        return PR_TRUE;
    }
    // never copy {}:uri attribute
    else if ((aAttribute == kURIAtom) && (aNameSpaceID == kNameSpaceID_None)) {
        return PR_TRUE;
    }
    // never copy {}:templatecontentsgenerated attribute (bogus)
    else if ((aAttribute == kTemplateContentsGeneratedAtom) && (aNameSpaceID == kNameSpaceID_None)) {
        return PR_TRUE;
    }
    else {
        return PR_FALSE;
    }
}


nsresult
RDFGenericBuilderImpl::GetSubstitutionText(nsIRDFResource* aResource,
                                           const nsString& aSubstitution,
                                           nsString& aResult)
{
    nsresult rv;

    if (aSubstitution.Find("rdf:") == 0) {
        // found an attribute which wants to bind
        // its value to RDF so look it up in the
        // graph
        nsAutoString propertyStr(aSubstitution);
        propertyStr.Cut(0,4);

        nsCOMPtr<nsIRDFResource> property;
        rv = gRDFService->GetUnicodeResource(propertyStr.GetUnicode(), getter_AddRefs(property));
        if (NS_FAILED(rv)) return rv;

        nsCOMPtr<nsIRDFNode> valueNode;
        rv = mDB->GetTarget(aResource, property, PR_TRUE, getter_AddRefs(valueNode));
        if (NS_FAILED(rv)) return rv;

        if (valueNode) {
            rv = nsRDFContentUtils::GetTextForNode(valueNode, aResult);
            if (NS_FAILED(rv)) return rv;
        }
        else {
            aResult.Truncate();
        }
    }
    else if (aSubstitution.EqualsIgnoreCase("...")) {
        const char *uri = nsnull;
        aResource->GetValueConst(&uri);
        aResult = uri;
    }
    else {
        aResult = aSubstitution;
    }

    return NS_OK;
}

nsresult
RDFGenericBuilderImpl::BuildContentFromTemplate(nsIContent *aTemplateNode,
                                                nsIContent *aRealNode,
                                                PRBool aIsUnique,
                                                nsIRDFResource* aChild,
                                                PRInt32 aNaturalOrderPos,
                                                PRBool aNotify)
{
	nsresult rv;

#ifdef PR_LOGGING
    if (PR_LOG_TEST(gLog, PR_LOG_DEBUG)) {
        nsCOMPtr<nsIAtom> tag;
        rv = aTemplateNode->GetTag(*getter_AddRefs(tag));
        if (NS_FAILED(rv)) return rv;

        nsXPIDLCString resourceCStr;
        rv = aChild->GetValue(getter_Copies(resourceCStr));
        if (NS_FAILED(rv)) return rv;
        
        PRUnichar *unicodeString;
        tag->GetUnicode(&unicodeString);
        nsAutoString tagStr(unicodeString);
        char* tagCStr = tagStr.ToNewCString();

        PR_LOG(gLog, PR_LOG_DEBUG,
               ("rdfgeneric build-content-fromt-template %s [%s]",
                tagCStr, (const char*) resourceCStr));

        delete[] tagCStr;
    }
#endif

	PRInt32	count;
	rv = aTemplateNode->ChildCount(count);
    if (NS_FAILED(rv)) return rv;

	for (PRInt32 kid = 0; kid < count; kid++) {
		nsCOMPtr<nsIContent> tmplKid;
		rv = aTemplateNode->ChildAt(kid, *getter_AddRefs(tmplKid));
        if (NS_FAILED(rv)) return rv;

		PRInt32 nameSpaceID;
		rv = tmplKid->GetNameSpaceID(nameSpaceID);
        if (NS_FAILED(rv)) return rv;

		// check whether this item is the resource element 
		PRBool isResourceElement = PR_FALSE;

        nsAutoString idValue;
        rv = tmplKid->GetAttribute(kNameSpaceID_None,
                                   kURIAtom,
                                   idValue);
        if (NS_FAILED(rv)) return rv;

        if ((rv == NS_CONTENT_ATTR_HAS_VALUE) && (idValue.EqualsIgnoreCase("..."))) {
            isResourceElement = PR_TRUE;
            aIsUnique = PR_FALSE;
		}

		nsCOMPtr<nsIAtom> tag;
		rv = tmplKid->GetTag(*getter_AddRefs(tag));
        if (NS_FAILED(rv)) return rv;

        PRBool realKidAlreadyExisted = PR_FALSE;

        nsCOMPtr<nsIContent> realKid;
        if (aIsUnique) {
            rv = EnsureElementHasGenericChild(aRealNode, nameSpaceID, tag, aNotify, getter_AddRefs(realKid));
            if (NS_FAILED(rv)) return rv;

            if (rv == NS_RDF_ELEMENT_WAS_THERE) {
                realKidAlreadyExisted = PR_TRUE;
            }
            else {
                // Mark the element's contents as being generated so
                // that any re-entrant calls don't trigger an infinite
                // recursion.
                rv = realKid->SetAttribute(kNameSpaceID_None,
                                           kTemplateContentsGeneratedAtom,
                                           "true",
                                           PR_FALSE);
                NS_ASSERTION(NS_SUCCEEDED(rv), "unable to set contents-generated attribute");
                if (NS_FAILED(rv)) return rv;
            }

            // Recurse until we get to the resource element.
            rv = BuildContentFromTemplate(tmplKid, realKid, PR_TRUE, aChild, -1, aNotify);
            if (NS_FAILED(rv)) return rv;
        }
        else if (isResourceElement) {
            rv = CreateElement(nameSpaceID, tag, aChild, getter_AddRefs(realKid));
            if (NS_FAILED(rv)) return rv;

            if (IsContainer(realKid, aChild)) {
                rv = realKid->SetAttribute(kNameSpaceID_None, kContainerAtom, "true", PR_FALSE);
                if (NS_FAILED(rv)) return rv;

                // test to see if the container has contents
                char* isEmpty = IsEmpty(realKid, aChild) ? (char *)"true" : (char *)"false";
                rv = realKid->SetAttribute(kNameSpaceID_None, kEmptyAtom, isEmpty, PR_FALSE);
                if (NS_FAILED(rv)) return rv;
            }
            
        }
        else if ((tag.get() == kTextAtom) && (nameSpaceID == kNameSpaceID_XUL)) {
            // <xul:text value="..."> is replaced by text of the
            // actual value of the rdf:resource attribute for the given node
            nsAutoString	attrValue;
            rv = tmplKid->GetAttribute(kNameSpaceID_None, kValueAtom, attrValue);
            if (NS_FAILED(rv)) return rv;

            if ((rv == NS_CONTENT_ATTR_HAS_VALUE) && (attrValue.Length() > 0)) {
                nsAutoString text;
                rv = GetSubstitutionText(aChild, attrValue, text);
                if (NS_FAILED(rv)) return rv;

                nsCOMPtr<nsITextContent> content;
                rv = nsComponentManager::CreateInstance(kTextNodeCID,
                                                        nsnull,
                                                        nsITextContent::GetIID(),
                                                        getter_AddRefs(content));
                if (NS_FAILED(rv)) return rv;

                rv = content->SetText(text.GetUnicode(), text.Length(), PR_FALSE);
                if (NS_FAILED(rv)) return rv;

                rv = aRealNode->AppendChildTo(nsCOMPtr<nsIContent>( do_QueryInterface(content) ), aNotify);
                if (NS_FAILED(rv)) return rv;
            }
        }
        else {
            // It's just a generic element. Create it!
            rv = CreateElement(nameSpaceID, tag, nsnull, getter_AddRefs(realKid));
            if (NS_FAILED(rv)) return rv;
        }

        if (realKid && !realKidAlreadyExisted) {
            // save a reference (its ID) to the template node that was used
            nsAutoString templateID;
            rv = tmplKid->GetAttribute(kNameSpaceID_None, kIdAtom, templateID);
            if (NS_FAILED(rv)) return rv;

            rv = realKid->SetAttribute(kNameSpaceID_None, kTemplateAtom, templateID, PR_FALSE);
            if (NS_FAILED(rv)) return rv;

            // mark it as a container so that its contents get
            // generated. Bogus as all git up.
            rv = realKid->SetAttribute(kNameSpaceID_None, kLazyContentAtom, "true", PR_FALSE);
            if (NS_FAILED(rv)) return rv;

            // set natural order hint
            if ((aNaturalOrderPos > 0) && (isResourceElement)) {
                nsAutoString	pos, zero("0000");
                pos.Append(aNaturalOrderPos, 10);
                if (pos.Length() < 4) {
                    pos.Insert(zero, 0, 4-pos.Length());
                }

                realKid->SetAttribute(kNameSpaceID_None, kNaturalOrderPosAtom, pos, PR_FALSE);
            }

            // copy all attributes from template to new node
            PRInt32	numAttribs;
            rv = tmplKid->GetAttributeCount(numAttribs);
            if (NS_FAILED(rv)) return rv;

            for (PRInt32 attr = 0; attr < numAttribs; attr++) {
                PRInt32 attribNameSpaceID;
                nsCOMPtr<nsIAtom> attribName;

                rv = tmplKid->GetAttributeNameAt(attr, attribNameSpaceID, *getter_AddRefs(attribName));
                if (NS_FAILED(rv)) return rv;

                // only copy attributes that aren't already set on the
                // node. XXX Why would it already be set?!?
                {
                    nsAutoString attribValue;
                    rv = realKid->GetAttribute(attribNameSpaceID, attribName, attribValue);
                    if (NS_FAILED(rv)) return rv;

                    if (rv == NS_CONTENT_ATTR_HAS_VALUE) continue;
                }

                if (! IsIgnoreableAttribute(attribNameSpaceID, attribName)) {
                    nsAutoString attribValue;
                    rv = tmplKid->GetAttribute(attribNameSpaceID, attribName, attribValue);
                    if (NS_FAILED(rv)) return rv;

                    if (rv == NS_CONTENT_ATTR_HAS_VALUE) {
                        nsAutoString text;
                        rv = GetSubstitutionText(aChild, attribValue, text);
                        if (NS_FAILED(rv)) return rv;

                        rv = realKid->SetAttribute(attribNameSpaceID, attribName, text, PR_FALSE);
                        if (NS_FAILED(rv)) return rv;
                    }
                }
            }

            // We'll _already_ have added the unique elements.
            if (! aIsUnique) {
                // Add into content model, special casing treeitems.
                if ((nsnull != gXULSortService) && (isResourceElement) && (tag.get() == kTreeItemAtom)) {
                    rv = gXULSortService->InsertContainerNode(aRealNode, realKid, aNotify);
                    NS_ASSERTION(NS_SUCCEEDED(rv), "unable to insert element via sort service");
                }
                else {
                    rv = aRealNode->AppendChildTo(realKid, aNotify);
                    NS_ASSERTION(NS_SUCCEEDED(rv), "unable to insert element");
                }
            }

            if (nameSpaceID == kNameSpaceID_HTML) {
                // If we just built HTML, then we have to recurse "by
                // hand" because HTML won't build itself up lazily.
                rv = BuildContentFromTemplate(tmplKid, realKid, aIsUnique, aChild, -1, aNotify);
                if (NS_FAILED(rv)) return rv;
            }
        }
	}

	return NS_OK;
}


nsresult
RDFGenericBuilderImpl::CreateWidgetItem(nsIContent *aElement,
                                        nsIRDFResource *aProperty,
                                        nsIRDFResource *aChild,
                                        PRInt32 aNaturalOrderPos,
                                        PRBool aNotify)
{
	nsCOMPtr<nsIContent> tmpl;
	nsresult		rv;

	rv = FindTemplate(aElement, aProperty, aChild, getter_AddRefs(tmpl));
    if (NS_FAILED(rv)) {
        // No template. Just bail.
        return NS_OK;
    }

    rv = BuildContentFromTemplate(tmpl, aElement, PR_TRUE, aChild, aNaturalOrderPos, aNotify);

    NS_ASSERTION(NS_SUCCEEDED(rv), "unable to build content from template");
	return rv;
}



nsresult
RDFGenericBuilderImpl::SynchronizeUsingTemplate(nsIContent* aTemplateNode,
                                                nsIContent* aTreeItemElement,
                                                eUpdateAction aAction,
                                                nsIRDFResource* aProperty,
                                                nsIRDFNode* aValue)
{
	nsresult rv;

	PRInt32 nameSpaceID;
	nsCOMPtr<nsIAtom> tag;
	rv = mDocument->SplitProperty(aProperty, &nameSpaceID, getter_AddRefs(tag));
	if (NS_FAILED(rv)) return rv;

	// check all attributes on the template node; if they reference a resource,
	// update the equivalent attribute on the content node

	PRInt32	numAttribs;
	rv = aTemplateNode->GetAttributeCount(numAttribs);
	if (NS_FAILED(rv)) return rv;

	if (rv == NS_CONTENT_ATTR_HAS_VALUE) {
		for (PRInt32 aLoop=0; aLoop<numAttribs; aLoop++) {
			PRInt32	attribNameSpaceID;
			nsCOMPtr<nsIAtom> attribName;
			rv = aTemplateNode->GetAttributeNameAt(aLoop,
                                                   attribNameSpaceID,
                                                   *getter_AddRefs(attribName));
            if (NS_FAILED(rv)) return rv;

            nsAutoString attribValue;
			rv = aTemplateNode->GetAttribute(attribNameSpaceID,
                                             attribName,
                                             attribValue);
            if (NS_FAILED(rv)) return rv;

            if (rv != NS_CONTENT_ATTR_HAS_VALUE)
                continue;

            if (attribValue.Find("rdf:") != 0)
                continue;

            // found an attribute which wants to bind its value
            // to RDF so look it up in the graph
            attribValue.Cut(0,4);

            nsCOMPtr<nsIRDFResource> property;
            rv = gRDFService->GetUnicodeResource(attribValue.GetUnicode(),
                                                 getter_AddRefs(property));
            if (NS_FAILED(rv)) return rv;

            if (property.get() == aProperty) {
                nsAutoString text("");

                rv = nsRDFContentUtils::GetTextForNode(aValue, text);
                if (NS_FAILED(rv)) return rv;

                if ((text.Length() > 0) && (aAction == eSet)) {
                    aTreeItemElement->SetAttribute(attribNameSpaceID,
                                                   attribName,
                                                   text,
                                                   PR_TRUE);
                }
                else {
                    aTreeItemElement->UnsetAttribute(attribNameSpaceID,
                                                     attribName,
                                                     PR_TRUE);
                }
            }
        }
    }

    // See if we've generated kids for this node yet. If we have, then
	// recursively sync up template kids with content kids
    nsAutoString contentsGenerated;
    rv = aTreeItemElement->GetAttribute(kNameSpaceID_None,
                                        kTemplateContentsGeneratedAtom,
                                        contentsGenerated);
    if (NS_FAILED(rv)) return rv;

    if ((rv == NS_CONTENT_ATTR_HAS_VALUE) && (contentsGenerated.EqualsIgnoreCase("true"))) {
        PRInt32 count;
        rv = aTemplateNode->ChildCount(count);
        if (NS_FAILED(rv)) return rv;

        for (PRInt32 loop=0; loop<count; loop++) {
            nsCOMPtr<nsIContent> tmplKid;
            rv = aTemplateNode->ChildAt(loop, *getter_AddRefs(tmplKid));
            if (NS_FAILED(rv)) return rv;

            if (! tmplKid)
                break;

            nsCOMPtr<nsIContent> realKid;
            rv = aTreeItemElement->ChildAt(loop, *getter_AddRefs(realKid));
            if (NS_FAILED(rv)) return rv;

            if (! realKid)
                break;

            rv = SynchronizeUsingTemplate(tmplKid, realKid, aAction, aProperty, aValue);
            if (NS_FAILED(rv)) return rv;
        }
    }

    return NS_OK;
}



nsresult
RDFGenericBuilderImpl::RemoveWidgetItem(nsIContent* aElement,
                                        nsIRDFResource* aProperty,
                                        nsIRDFResource* aValue,
                                        PRBool aNotify)
{
    // This works as follows. It finds all of the elements in the
    // document that correspond to aValue. Any that are contained
    // within aElement are removed from their direct parent.
    nsresult rv;

    nsCOMPtr<nsISupportsArray> elements;
    rv = NS_NewISupportsArray(getter_AddRefs(elements));
    if (NS_FAILED(rv)) return rv;

    rv = mDocument->GetElementsForResource(aValue, elements);
    if (NS_FAILED(rv)) return rv;

    PRUint32 cnt;
    rv = elements->Count(&cnt);
    if (NS_FAILED(rv)) return rv;

    for (PRInt32 i = PRInt32(cnt) - 1; i >= 0; --i) {
        nsISupports* isupports = elements->ElementAt(i);
        nsCOMPtr<nsIContent> child( do_QueryInterface(isupports) );
        NS_IF_RELEASE(isupports);

        if (! nsRDFContentUtils::IsContainedBy(child, aElement))
            continue;

        nsCOMPtr<nsIContent> parent;
        rv = child->GetParent(*getter_AddRefs(parent));
        if (NS_FAILED(rv)) return rv;

        PRInt32 pos;
        rv = parent->IndexOf(child, pos);
        if (NS_FAILED(rv)) return rv;

        NS_ASSERTION(pos >= 0, "parent doesn't think this child has an index");
        if (pos < 0) continue;

        rv = parent->RemoveChildAt(pos, PR_TRUE);
        if (NS_FAILED(rv)) return rv;
    }

    return NS_OK;
}


nsresult
RDFGenericBuilderImpl::CreateContainerContents(nsIContent* aElement, nsIRDFResource* aResource, PRBool aNotify)
{
    // Create the contents of a container by iterating over all of the
    // "containment" arcs out of the element's resource.
    nsresult rv;

    // See if the item is even "open". If not, then just pretend it
    // doesn't have _any_ contents. We check this _before_ checking
    // the contents-generated attribute so that we don't eagerly set
    // contents-generated on a closed node.
    if (!IsOpen(aElement))
        return NS_OK;

    // See if the element's templates contents have been generated:
    // this prevents a re-entrant call from triggering another
    // generation.
    nsAutoString attrValue;
    rv = aElement->GetAttribute(kNameSpaceID_None,
                                kContainerContentsGeneratedAtom,
                                attrValue);
    NS_ASSERTION(NS_SUCCEEDED(rv), "unable to test container-contents-generated attribute");
    if (NS_FAILED(rv)) return rv;

    if ((rv == NS_CONTENT_ATTR_HAS_VALUE) && (attrValue.EqualsIgnoreCase("true")))
        return NS_RDF_ELEMENT_WAS_CREATED;

    // Now mark the element's contents as being generated so that
    // any re-entrant calls don't trigger an infinite recursion.
    rv = aElement->SetAttribute(kNameSpaceID_None,
                                kContainerContentsGeneratedAtom,
                                "true",
                                PR_FALSE);
    NS_ASSERTION(NS_SUCCEEDED(rv), "unable to set container-contents-generated attribute");
    if (NS_FAILED(rv)) return rv;

    // XXX Eventually, we may want to factor this into one method that
    // handles RDF containers (RDF:Bag, et al.) and another that
    // handles multi-attributes. For performance...

    // Create a cursor that'll enumerate all of the outbound arcs
    nsCOMPtr<nsISimpleEnumerator> properties;
    rv = mDB->ArcLabelsOut(aResource, getter_AddRefs(properties));
    if (NS_FAILED(rv)) return rv;

    while (1) {
        PRBool hasMore;
        rv = properties->HasMoreElements(&hasMore);
        if (NS_FAILED(rv)) return rv;

        if (! hasMore)
            break;

        nsCOMPtr<nsISupports> isupports;
        rv = properties->GetNext(getter_AddRefs(isupports));
        if (NS_FAILED(rv)) return rv;

        nsCOMPtr<nsIRDFResource> property = do_QueryInterface(isupports);

        // If it's not a containment property, then it doesn't specify an
        // object that is member of the current container element;
        // rather, it specifies a "simple" property of the container
        // element. Skip it.
        if (!IsContainmentProperty(aElement, property))
            continue;

        // Create a second cursor that'll enumerate all of the values
        // for all of the arcs.
        nsCOMPtr<nsISimpleEnumerator> targets;
        rv = mDB->GetTargets(aResource, property, PR_TRUE, getter_AddRefs(targets));
        if (NS_FAILED(rv)) return rv;

        while (1) {
            PRBool hasMoreElements;
            rv = targets->HasMoreElements(&hasMoreElements);
            if (NS_FAILED(rv)) return rv;

            if (! hasMoreElements)
                break;

            nsCOMPtr<nsISupports> isupportsNext;
            rv = targets->GetNext(getter_AddRefs(isupportsNext));
            if (NS_FAILED(rv)) return rv;

            nsCOMPtr<nsIRDFResource> target = do_QueryInterface(isupportsNext);
            NS_ASSERTION(target != nsnull, "not a resource");
            if (! target)
                continue;

            rv = CreateWidgetItem(aElement, property, target, -1, aNotify);
            NS_ASSERTION(NS_SUCCEEDED(rv), "unable to create item");
            if (NS_FAILED(rv)) return rv;
        }
    }

    return NS_OK;
}


nsresult
RDFGenericBuilderImpl::CreateTemplateContents(nsIContent* aElement, const nsString& aTemplateID)
{
    // Create the contents of an element using the templates
    nsresult rv;

    // See if the element's templates contents have been generated:
    // this prevents a re-entrant call from triggering another
    // generation.
    nsAutoString attrValue;
    rv = aElement->GetAttribute(kNameSpaceID_None,
                                kTemplateContentsGeneratedAtom,
                                attrValue);
    NS_ASSERTION(NS_SUCCEEDED(rv), "unable to test template-contents-generated attribute");
    if (NS_FAILED(rv)) return rv;

    if ((rv == NS_CONTENT_ATTR_HAS_VALUE) && (attrValue.EqualsIgnoreCase("true")))
        return NS_OK;

    // Now mark the element's contents as being generated so that
    // any re-entrant calls don't trigger an infinite recursion.
    rv = aElement->SetAttribute(kNameSpaceID_None,
                                kTemplateContentsGeneratedAtom,
                                "true",
                                PR_FALSE);
    NS_ASSERTION(NS_SUCCEEDED(rv), "unable to set template-contents-generated attribute");
    if (NS_FAILED(rv)) return rv;

    // Find the template node that corresponds to the "real" node for
    // which we're trying to generate contents.
    nsCOMPtr<nsIDOMXULDocument> xulDoc;
    xulDoc = do_QueryInterface(mDocument);
    if (! xulDoc)
        return NS_ERROR_UNEXPECTED;

    nsCOMPtr<nsIDOMElement> tmplNode;
    rv = xulDoc->GetElementById(aTemplateID, getter_AddRefs(tmplNode));
    if (NS_FAILED(rv)) return rv;

    nsCOMPtr<nsIContent> tmpl = do_QueryInterface(tmplNode);
    if (! tmpl)
        return NS_ERROR_UNEXPECTED;

    // Crawl up the content model until we find the "resource" element
    // that spawned this template.
    nsCOMPtr<nsIRDFResource> resource;

    nsCOMPtr<nsIContent> element = aElement;
    while (element) {
        rv = nsRDFContentUtils::GetElementRefResource(element, getter_AddRefs(resource));
        if (NS_SUCCEEDED(rv)) break;

        nsCOMPtr<nsIContent> parent;
        rv = element->GetParent(*getter_AddRefs(parent));
        if (NS_FAILED(rv)) return rv;

        element = parent;
    }

    rv = BuildContentFromTemplate(tmpl, aElement, PR_FALSE, resource, -1, PR_FALSE);
    if (NS_FAILED(rv)) return rv;

    return NS_OK;
}

nsresult
RDFGenericBuilderImpl::EnsureElementHasGenericChild(nsIContent* parent,
                                                    PRInt32 nameSpaceID,
                                                    nsIAtom* tag,
                                                    PRBool aNotify,
                                                    nsIContent** result)
{
    nsresult rv;

    rv = nsRDFContentUtils::FindChildByTag(parent, nameSpaceID, tag, result);
    if (NS_FAILED(rv))
        return rv;

    if (rv == NS_RDF_NO_VALUE) {
        // we need to construct a new child element.
        nsCOMPtr<nsIContent> element;

        rv = CreateElement(nameSpaceID, tag, nsnull, getter_AddRefs(element));
        if (NS_FAILED(rv)) return rv;

        // XXX Note that the notification ensures we won't batch insertions! This could be bad! - Dave
        rv = parent->AppendChildTo(element, aNotify);
        if (NS_FAILED(rv)) return rv;

        *result = element;
        NS_ADDREF(*result);
        return NS_RDF_ELEMENT_WAS_CREATED;
    }
    else {
        return NS_RDF_ELEMENT_WAS_THERE;
    }
}


PRBool
RDFGenericBuilderImpl::IsResourceElement(nsIContent* aElement)
{
    nsresult rv;
    nsAutoString str;
    rv = aElement->GetAttribute(kNameSpaceID_None, kIdAtom, str);
    if (NS_FAILED(rv)) return PR_FALSE;

    return (rv == NS_CONTENT_ATTR_HAS_VALUE) ? PR_TRUE : PR_FALSE;
}


PRBool
RDFGenericBuilderImpl::IsContainmentProperty(nsIContent* aElement, nsIRDFResource* aProperty)
{
    // XXX is this okay to _always_ treat ordinal properties as tree
    // properties? Probably not...
    nsresult rv;

    PRBool isOrdinal;
    rv = gRDFContainerUtils->IsOrdinalProperty(aProperty, &isOrdinal);
    if (NS_FAILED(rv))
        return PR_FALSE;

    if (isOrdinal)
        return PR_TRUE;

    const char		*propertyURI;
    if (NS_FAILED(rv = aProperty->GetValueConst( &propertyURI ))) {
        NS_ERROR("unable to get property URI");
        return PR_FALSE;
    }

    nsAutoString uri;

    // Walk up the content tree looking for the "rdf:containment"
    // attribute, so we can determine if the specified property
    // actually defines containment.
    nsCOMPtr<nsIContent> element( dont_QueryInterface(aElement) );
    while (element) {
        // Never ever ask an HTML element about non-HTML attributes.
        PRInt32 nameSpaceID;
        if (NS_FAILED(rv = element->GetNameSpaceID(nameSpaceID))) {
            NS_ERROR("unable to get element namespace");
            return PR_FALSE;
        }

        if (nameSpaceID != kNameSpaceID_HTML) {
            // Is this the "containment" property?
            if (NS_FAILED(rv = element->GetAttribute(kNameSpaceID_None, kContainmentAtom, uri))) {
                NS_ERROR("unable to get attribute value");
                return PR_FALSE;
            }

            if (rv == NS_CONTENT_ATTR_HAS_VALUE) {
                // Okay we've found the locally-scoped tree properties,
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

#define TREE_PROPERTY_HACK
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
RDFGenericBuilderImpl::IsIgnoredProperty(nsIContent* aElement, nsIRDFResource* aProperty)
{
    nsresult rv;

    const char		*propertyURI;
    rv = aProperty->GetValueConst(&propertyURI);
    if (NS_FAILED(rv)) return rv;

    nsAutoString uri;

    // Walk up the content tree looking for the "rdf:ignore"
    // attribute, so we can determine if the specified property should
    // be ignored.
    nsCOMPtr<nsIContent> element( dont_QueryInterface(aElement) );
    while (element) {
        PRInt32 nameSpaceID;
        rv = element->GetNameSpaceID(nameSpaceID);
        if (NS_FAILED(rv)) return rv;

        // Never ever ask an HTML element about non-HTML attributes
        if (nameSpaceID != kNameSpaceID_HTML) {
            rv = element->GetAttribute(kNameSpaceID_None, kIgnoreAtom, uri);
            if (NS_FAILED(rv)) return rv;

            if (rv == NS_CONTENT_ATTR_HAS_VALUE) {
                // Okay, we've found the locally-scoped ignore
                // properties, which is a whitespace-separated list of
                // property URIs. So we definitively know whether this
                // property should be ignored or not.
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

    // Walked _all_ the way to the top and couldn't find anything to
    // ignore.
    return PR_FALSE;
}

PRBool
RDFGenericBuilderImpl::IsContainer(nsIContent* aElement, nsIRDFResource* aResource)
{
    // Look at all of the arcs extending _out_ of the resource: if any
    // of them are that "containment" property, then we know we'll
    // have children.

    nsCOMPtr<nsISimpleEnumerator> arcs;
    nsresult rv;

    rv = mDB->ArcLabelsOut(aResource, getter_AddRefs(arcs));
    NS_ASSERTION(NS_SUCCEEDED(rv), "unable to get arcs out");
    if (NS_FAILED(rv))
        return PR_FALSE;

    while (1) {
        PRBool hasMore;
        rv = arcs->HasMoreElements(&hasMore);
        NS_ASSERTION(NS_SUCCEEDED(rv), "severe error advancing cursor");
        if (NS_FAILED(rv))
            return PR_FALSE;

        if (! hasMore)
            break;

        nsCOMPtr<nsISupports> isupports;
        rv = arcs->GetNext(getter_AddRefs(isupports));
        if (NS_FAILED(rv))
            return PR_FALSE;

        nsCOMPtr<nsIRDFResource> property = do_QueryInterface(isupports);

        if (! IsContainmentProperty(aElement, property))
            continue;

        return PR_TRUE;
    }

    return PR_FALSE;
}


PRBool
RDFGenericBuilderImpl::IsEmpty(nsIContent* aElement, nsIRDFResource* aContainer)
{
    // Look at all of the arcs extending _out_ of the resource: if any
    // of them are that "containment" property, then we know we'll
    // have children.

    nsCOMPtr<nsISimpleEnumerator> arcs;
    nsresult rv;

    rv = mDB->ArcLabelsOut(aContainer, getter_AddRefs(arcs));
    NS_ASSERTION(NS_SUCCEEDED(rv), "unable to get arcs out");
    if (NS_FAILED(rv))
        return PR_TRUE;

    while (1) {
        PRBool hasMore;
        rv = arcs->HasMoreElements(&hasMore);
        NS_ASSERTION(NS_SUCCEEDED(rv), "severe error advancing cursor");
        if (NS_FAILED(rv))
            return PR_TRUE;

        if (! hasMore)
            break;

        nsCOMPtr<nsISupports> isupports;
        rv = arcs->GetNext(getter_AddRefs(isupports));
        if (NS_FAILED(rv))
            return PR_TRUE;

        nsCOMPtr<nsIRDFResource> property = do_QueryInterface(isupports);

        if (! IsContainmentProperty(aElement, property))
            continue;

        // now that we know its a container, check to see if it's "empty"
        // by testing to see if the property has a target.
        nsCOMPtr<nsIRDFNode> dummy;
        rv = mDB->GetTarget(aContainer, property, PR_TRUE, getter_AddRefs(dummy));
        if (NS_FAILED(rv)) return rv;

        if (dummy)
            return PR_FALSE;
    }

    return PR_TRUE;
}


PRBool
RDFGenericBuilderImpl::IsOpen(nsIContent* aElement)
{
    nsresult rv;

    // needs to be a the valid insertion root or an item to begin with.
    PRInt32 nameSpaceID;
    if (NS_FAILED(rv = aElement->GetNameSpaceID(nameSpaceID))) {
        NS_ERROR("unable to get namespace ID");
        return PR_FALSE;
    }

    if (nameSpaceID != kNameSpaceID_XUL)
        return PR_TRUE;

    if (aElement == mRoot.get())
      return PR_TRUE;

    nsAutoString value;
    rv = aElement->GetAttribute(kNameSpaceID_None, kOpenAtom, value);
    NS_ASSERTION(NS_SUCCEEDED(rv), "unable to get open attribute");
    if (NS_FAILED(rv)) return rv;

    if (rv == NS_CONTENT_ATTR_HAS_VALUE) {
        if (value.EqualsIgnoreCase("true"))
            return PR_TRUE;
    }

	
    return PR_FALSE;
}


PRBool
RDFGenericBuilderImpl::IsElementInWidget(nsIContent* aElement)
{
    // Make sure that we're actually creating content for the tree
    // content model that we've been assigned to deal with.

    // Walk up the parent chain from us to the root and
    // see what we find.
    if (aElement == mRoot.get())
      return PR_TRUE;

    // walk up the tree until you find rootAtom
    nsCOMPtr<nsIContent> element(do_QueryInterface(aElement));
    nsCOMPtr<nsIContent> parent;
    element->GetParent(*getter_AddRefs(parent));
    element = parent;
    
    while (element) {
        
        if (element.get() == mRoot.get())
          return PR_TRUE;

        // up to the parent...
//        nsCOMPtr<nsIContent> parent;
        element->GetParent(*getter_AddRefs(parent));
        element = parent;
    }
    
    return PR_FALSE;
}

nsresult
RDFGenericBuilderImpl::GetDOMNodeResource(nsIDOMNode* aNode, nsIRDFResource** aResource)
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

    return nsRDFContentUtils::GetElementRefResource(element, aResource);
}


nsresult
RDFGenericBuilderImpl::GetResource(PRInt32 aNameSpaceID,
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
    PRUnichar *unicodeString;
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
RDFGenericBuilderImpl::FindInsertionPoint(nsIContent* aElement, nsIContent** aResult)
{
    nsresult rv;

    // XXX Hack-o-rama. This needs to be fixed to actually grovel over
    // the template n' stuff.
    nsCOMPtr<nsIAtom> tag;
    rv = aElement->GetTag(*getter_AddRefs(tag));
    if (NS_FAILED(rv)) return rv;

    nsAutoString tagName;
    rv = tag->ToString(tagName);
    if (NS_FAILED(rv)) return rv;

    if (tagName.Equals("tree") || tagName.Equals("treeitem")) {
        rv = nsRDFContentUtils::FindChildByTag(aElement, kNameSpaceID_XUL, NS_NewAtom("treechildren"), aResult);
        if (NS_FAILED(rv)) return rv;
    }
    else if (tagName.Equals("menu")) {
        rv = nsRDFContentUtils::FindChildByTag(aElement, kNameSpaceID_XUL, NS_NewAtom("menupopup"), aResult);
        if (NS_FAILED(rv)) return rv;
    }
    else {
        *aResult = aElement;
        NS_ADDREF(*aResult);
    }

    return NS_OK;
}


nsresult
RDFGenericBuilderImpl::RemoveGeneratedContent(nsIContent* aElement)
{
    // Remove all the content beneath aElement that has been generated
    // from a template.

    nsresult rv;

    PRInt32 count;
    rv = aElement->ChildCount(count);
    if (NS_FAILED(rv)) return rv;

    while (--count >= 0) {
        nsCOMPtr<nsIContent> child;
        rv = aElement->ChildAt(count, *getter_AddRefs(child));
        if (NS_FAILED(rv)) return rv;

        nsAutoString tmplID;
        rv = child->GetAttribute(kNameSpaceID_None, kTemplateAtom, tmplID);
        if (NS_FAILED(rv)) return rv;

        if (rv != NS_CONTENT_ATTR_HAS_VALUE)
            continue;

        // It's a generated element. Remove it, and set its document
        // to null so that it'll get knocked out of the XUL doc's
        // resource-to-element map.
        rv = aElement->RemoveChildAt(count, PR_TRUE);
        NS_ASSERTION(NS_SUCCEEDED(rv), "error removing child");

        rv = child->SetDocument(nsnull, PR_TRUE);
        if (NS_FAILED(rv)) return rv;
    }

    return NS_OK;
}


nsresult
RDFGenericBuilderImpl::FindFirstGeneratedChild(nsIContent* aElement, PRInt32* aIndex)
{
    // Find the first kid of aElement that has been generated from a
    // template.

    nsresult rv;

    PRInt32 count;
    rv = aElement->ChildCount(count);
    if (NS_FAILED(rv)) return rv;

    PRInt32 i = 0;
    while (i < count) {
        nsCOMPtr<nsIContent> child;
        rv = aElement->ChildAt(i, *getter_AddRefs(child));
        if (NS_FAILED(rv)) return rv;

        nsAutoString tmplID;
        rv = child->GetAttribute(kNameSpaceID_None, kTemplateAtom, tmplID);
        if (NS_FAILED(rv)) return rv;

        if (rv == NS_CONTENT_ATTR_HAS_VALUE)
            break;

        ++i;
    }

    *aIndex = (i < count) ? i : -1;
    return NS_OK;
}


PRBool
RDFGenericBuilderImpl::IsTreeWidgetItem(nsIContent* aElement)
{
    // Determine if this is a <tree> or a <treeitem> tag, in which
    // case, some special logic will kick in to force batched reflows.
    // XXX Should be removed when Bug 10818 is fixed.
    nsresult rv;

    PRInt32 nameSpaceID;
    rv = aElement->GetNameSpaceID(nameSpaceID);
    if (NS_FAILED(rv)) return PR_FALSE;

    nsCOMPtr<nsIAtom> tag;
    rv = aElement->GetTag(*getter_AddRefs(tag));
    if (NS_FAILED(rv)) return PR_FALSE;

    // If we're building content under a <tree> or a <treeitem>,
    // then DO NOT notify layout until we're all done.
    if ((nameSpaceID == kNameSpaceID_XUL) &&
        ((tag.get() == kTreeAtom) || (tag.get() == kTreeItemAtom))) {
        return PR_TRUE;
    }
    else {
        return PR_FALSE;
    }

}

PRBool
RDFGenericBuilderImpl::IsReflowScheduled()
{
    return (mTimer != nsnull);
}



nsresult
RDFGenericBuilderImpl::ScheduleReflow()
{
    NS_PRECONDITION(mTimer == nsnull, "reflow scheduled");
    if (mTimer)
        return NS_ERROR_UNEXPECTED;

    nsresult rv;

    rv = NS_NewTimer(getter_AddRefs(mTimer));
    if (NS_FAILED(rv)) return rv;

    mTimer->Init(RDFGenericBuilderImpl::ForceTreeReflow, this, 100);
    NS_ADDREF(this); // the timer will hold a reference to the builder
    
    return NS_OK;
}


void
RDFGenericBuilderImpl::ForceTreeReflow(nsITimer* aTimer, void* aClosure)
{
    // XXX Any lifetime issues we need to deal with here???
    RDFGenericBuilderImpl* builder = NS_STATIC_CAST(RDFGenericBuilderImpl*, aClosure);

    nsresult rv;

    nsCOMPtr<nsIDocument> doc = do_QueryInterface(builder->mDocument);

    // If we've been removed from the document, then this will have
    // been nulled out.
    if (! doc) return;

    // XXX What if kTreeChildrenAtom & kNameSpaceXUL are clobbered b/c
    // the generic builder has been destroyed?
    nsCOMPtr<nsIContent> treechildren;
    rv = nsRDFContentUtils::FindChildByTag(builder->mRoot,
                                           kNameSpaceID_XUL,
                                           kTreeChildrenAtom,
                                           getter_AddRefs(treechildren));

    if (NS_FAILED(rv)) return; // couldn't find

    rv = doc->ContentAppended(treechildren, 0);
    NS_ASSERTION(NS_SUCCEEDED(rv), "unable to notify content appended");

    builder->mTimer = nsnull;
    NS_RELEASE(builder);
}
