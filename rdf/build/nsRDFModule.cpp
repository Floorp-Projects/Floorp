/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.0 (the "NPL"); you may not use this file except in
 * compliance with the NPL.  You may obtain a copy of the NPL at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the NPL is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the NPL
 * for the specific language governing rights and limitations under the
 * NPL.
 *
 * The Initial Developer of this code under the NPL is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation.  All Rights
 * Reserved.
 */
#include "nsCOMPtr.h"
#include "nsRDFModule.h"
#include "nsIFactory.h"
#include "nsIGenericFactory.h"
#include "nsILocalStore.h"
#include "nsIRDFContainer.h"
#include "nsIRDFContainerUtils.h"
#include "nsIRDFCompositeDataSource.h"
#include "nsIRDFContentModelBuilder.h"
#include "nsIRDFContentSink.h"
#include "nsIRDFService.h"
#include "nsIXULContentSink.h"
#include "nsIXULPrototypeCache.h"
#include "nsISupports.h"
#include "nsRDFBaseDataSources.h"
#include "nsRDFBuiltInDataSources.h"
#include "nsIRDFFileSystem.h"
#include "nsRDFCID.h"
#include "nsIComponentManager.h"
#include "rdf.h"
#include "nsIXULContentUtils.h"
#include "nsIXULDocument.h"
#include "nsIXULSortService.h"
#include "nsIXULPopupListener.h"
#include "nsIXULKeyListener.h"
#include "nsIXULCommandDispatcher.h"
#include "nsIControllers.h"
#include "nsIServiceManager.h"

static NS_DEFINE_CID(kComponentManagerCID, NS_COMPONENTMANAGER_CID);
static NS_DEFINE_IID(kISupportsIID, NS_ISUPPORTS_IID);
static NS_DEFINE_IID(kIFactoryIID,  NS_IFACTORY_IID);

static NS_DEFINE_CID(kGenericFactoryCID,                  NS_GENERICFACTORY_CID);
static NS_DEFINE_CID(kLocalStoreCID,                      NS_LOCALSTORE_CID);
static NS_DEFINE_CID(kRDFCompositeDataSourceCID,          NS_RDFCOMPOSITEDATASOURCE_CID);
static NS_DEFINE_CID(kRDFContainerCID,                    NS_RDFCONTAINER_CID);
static NS_DEFINE_CID(kRDFContainerUtilsCID,               NS_RDFCONTAINERUTILS_CID);
static NS_DEFINE_CID(kRDFContentSinkCID,                  NS_RDFCONTENTSINK_CID);
static NS_DEFINE_CID(kRDFDefaultResourceCID,              NS_RDFDEFAULTRESOURCE_CID);
static NS_DEFINE_CID(kRDFFileSystemDataSourceCID,         NS_RDFFILESYSTEMDATASOURCE_CID);
static NS_DEFINE_CID(kRDFSearchDataSourceCID,             NS_RDFSEARCHDATASOURCE_CID);
static NS_DEFINE_CID(kRDFFindDataSourceCID,               NS_RDFFINDDATASOURCE_CID);
static NS_DEFINE_CID(kRDFFTPDataSourceCID,                NS_RDFFTPDATASOURCE_CID);
static NS_DEFINE_CID(kRDFInMemoryDataSourceCID,           NS_RDFINMEMORYDATASOURCE_CID);
static NS_DEFINE_CID(kRDFServiceCID,                      NS_RDFSERVICE_CID);
static NS_DEFINE_CID(kRDFXMLDataSourceCID,                NS_RDFXMLDATASOURCE_CID);
static NS_DEFINE_CID(kXULContentSinkCID,                  NS_XULCONTENTSINK_CID);
static NS_DEFINE_CID(kXULContentUtilsCID,                 NS_XULCONTENTUTILS_CID);
static NS_DEFINE_CID(kXULDocumentCID,                     NS_XULDOCUMENT_CID);
static NS_DEFINE_CID(kXULSortServiceCID,                  NS_XULSORTSERVICE_CID);
static NS_DEFINE_CID(kXULPopupListenerCID,                NS_XULPOPUPLISTENER_CID);
static NS_DEFINE_CID(kXULPrototypeCacheCID,               NS_XULPROTOTYPECACHE_CID);
static NS_DEFINE_CID(kXULKeyListenerCID,                  NS_XULKEYLISTENER_CID);
static NS_DEFINE_CID(kXULCommandDispatcherCID,            NS_XULCOMMANDDISPATCHER_CID);
static NS_DEFINE_CID(kXULControllersCID,                  NS_XULCONTROLLERS_CID);
static NS_DEFINE_CID(kXULTemplateBuilderCID,              NS_XULTEMPLATEBUILDER_CID);

//----------------------------------------------------------------------

// Functions used to create new instances of a given object by the
// generic factory.

#define MAKE_CTOR(_func,_new,_ifname)                                \
static NS_IMETHODIMP                                                 \
CreateNew##_func(nsISupports* aOuter, REFNSIID aIID, void **aResult) \
{                                                                    \
    if (!aResult) {                                                  \
        return NS_ERROR_INVALID_POINTER;                             \
    }                                                                \
    if (aOuter) {                                                    \
        *aResult = nsnull;                                           \
        return NS_ERROR_NO_AGGREGATION;                              \
    }                                                                \
    nsI##_ifname* inst;                                              \
    nsresult rv = NS_New##_new(&inst);                               \
    if (NS_FAILED(rv)) {                                             \
        *aResult = nsnull;                                           \
        return rv;                                                   \
    }                                                                \
    rv = inst->QueryInterface(aIID, aResult);                        \
    if (NS_FAILED(rv)) {                                             \
        *aResult = nsnull;                                           \
    }                                                                \
    NS_RELEASE(inst);             /* get rid of extra refcnt */      \
    return rv;                                                       \
}

extern nsresult
NS_NewDefaultResource(nsIRDFResource** aResult);

extern nsresult
NS_NewXULControllers(nsIControllers** result);


MAKE_CTOR(RDFService,RDFService,RDFService)
MAKE_CTOR(XULSortService,XULSortService,XULSortService)
MAKE_CTOR(XULPopupListener,XULPopupListener,XULPopupListener)
MAKE_CTOR(XULKeyListener,XULKeyListener,XULKeyListener)
MAKE_CTOR(XULCommandDispatcher,XULCommandDispatcher,XULCommandDispatcher)
MAKE_CTOR(XULControllers,XULControllers,Controllers)

MAKE_CTOR(RDFXMLDataSource,RDFXMLDataSource,RDFDataSource)
MAKE_CTOR(RDFFileSystemDataSource,RDFFileSystemDataSource,RDFDataSource)
MAKE_CTOR(RDFFTPDataSource,RDFFTPDataSource,RDFDataSource)
MAKE_CTOR(RDFCompositeDataSource,RDFCompositeDataSource,RDFCompositeDataSource)
MAKE_CTOR(RDFContainer,RDFContainer,RDFContainer)

MAKE_CTOR(RDFContainerUtils,RDFContainerUtils,RDFContainerUtils)
MAKE_CTOR(XULDocument,XULDocument,XULDocument)
MAKE_CTOR(XULTemplateBuilder,XULTemplateBuilder,RDFContentModelBuilder)

MAKE_CTOR(RDFContentSink,RDFContentSink,RDFContentSink)
MAKE_CTOR(XULContentSink,XULContentSink,XULContentSink)
MAKE_CTOR(RDFDefaultResource,DefaultResource,RDFResource)
MAKE_CTOR(LocalStore,LocalStore,LocalStore)

//----------------------------------------------------------------------

static NS_DEFINE_IID(kIModuleIID, NS_IMODULE_IID);

nsRDFModule::nsRDFModule()
  : mInitialized(PR_FALSE)
{
    NS_INIT_ISUPPORTS();
}

nsRDFModule::~nsRDFModule()
{
    Shutdown();
}

NS_IMPL_ISUPPORTS(nsRDFModule, kIModuleIID)

// Perform our one-time intialization for this module
nsresult
nsRDFModule::Initialize()
{
    if (mInitialized) {
        return NS_OK;
    }

    return NS_OK;
}

// Shutdown this module, releasing all of the module resources
void
nsRDFModule::Shutdown()
{
}

NS_IMETHODIMP
nsRDFModule::GetClassObject(nsIComponentManager *aCompMgr,
                            const nsCID& aClass,
                            const nsIID& aIID,
                            void** r_classObj)
{
    nsresult rv;

    // Defensive programming: Initialize *r_classObj in case of error below
    if (!r_classObj) {
        return NS_ERROR_INVALID_POINTER;
    }
    *r_classObj = NULL;

    if (!mInitialized) {
        rv = Initialize();
        if (NS_FAILED(rv)) {
            return rv;
        }
        mInitialized = PR_TRUE;
    }

    nsCOMPtr<nsIGenericFactory> fact;

    // Note: these are in the same order as the registration table
    // below, not in frequency of use order.
    if (aClass.Equals(kRDFCompositeDataSourceCID)) {
        rv = NS_NewGenericFactory(getter_AddRefs(fact), CreateNewRDFCompositeDataSource);
    }
    else if (aClass.Equals(kRDFFileSystemDataSourceCID)) {
        rv = NS_NewGenericFactory(getter_AddRefs(fact), CreateNewRDFFileSystemDataSource);
    }
    else if (aClass.Equals(kRDFFTPDataSourceCID)) {
        rv = NS_NewGenericFactory(getter_AddRefs(fact), CreateNewRDFFTPDataSource);
    }
    else if (aClass.Equals(kRDFInMemoryDataSourceCID)) {
        rv = NS_NewGenericFactory(getter_AddRefs(fact), NS_NewRDFInMemoryDataSource);
    }
    else if (aClass.Equals(kLocalStoreCID)) {
        rv = NS_NewGenericFactory(getter_AddRefs(fact), CreateNewLocalStore);
    }
    else if (aClass.Equals(kRDFXMLDataSourceCID)) {
        rv = NS_NewGenericFactory(getter_AddRefs(fact), CreateNewRDFXMLDataSource);
    }
    else if (aClass.Equals(kRDFDefaultResourceCID)) {
        rv = NS_NewGenericFactory(getter_AddRefs(fact), CreateNewRDFDefaultResource);
    }
    else if (aClass.Equals(kRDFContentSinkCID)) {
        rv = NS_NewGenericFactory(getter_AddRefs(fact), CreateNewRDFContentSink);
    }
    else if (aClass.Equals(kRDFContainerCID)) {
        rv = NS_NewGenericFactory(getter_AddRefs(fact), CreateNewRDFContainer);
    }
    else if (aClass.Equals(kRDFContainerUtilsCID)) {
        rv = NS_NewGenericFactory(getter_AddRefs(fact), CreateNewRDFContainerUtils);
    }
    else if (aClass.Equals(kRDFServiceCID)) {
        rv = NS_NewGenericFactory(getter_AddRefs(fact), CreateNewRDFService);
    }
    else if (aClass.Equals(kXULSortServiceCID)) {
        rv = NS_NewGenericFactory(getter_AddRefs(fact), CreateNewXULSortService);
    }
    else if (aClass.Equals(kXULTemplateBuilderCID)) {
        rv = NS_NewGenericFactory(getter_AddRefs(fact), CreateNewXULTemplateBuilder);
    }
    else if (aClass.Equals(kXULContentSinkCID)) {
        rv = NS_NewGenericFactory(getter_AddRefs(fact), CreateNewXULContentSink);
    }
    else if (aClass.Equals(kXULDocumentCID)) {
        rv = NS_NewGenericFactory(getter_AddRefs(fact), CreateNewXULDocument);
    }
    else if (aClass.Equals(kXULPopupListenerCID)) {
        rv = NS_NewGenericFactory(getter_AddRefs(fact), CreateNewXULPopupListener);
    }
    else if (aClass.Equals(kXULKeyListenerCID)) {
        rv = NS_NewGenericFactory(getter_AddRefs(fact), CreateNewXULKeyListener);
    }
    else if (aClass.Equals(kXULCommandDispatcherCID)) {
        rv = NS_NewGenericFactory(getter_AddRefs(fact), CreateNewXULCommandDispatcher);
    }
    else if (aClass.Equals(kXULControllersCID)) {
        rv = NS_NewGenericFactory(getter_AddRefs(fact), CreateNewXULControllers);
    }
    else if (aClass.Equals(kXULContentUtilsCID)) {
        rv = NS_NewGenericFactory(getter_AddRefs(fact), NS_NewXULContentUtils);
    }
    else if (aClass.Equals(kXULPrototypeCacheCID)) {
        rv = NS_NewGenericFactory(getter_AddRefs(fact), NS_NewXULPrototypeCache);
    }
    else {
		rv = NS_ERROR_FACTORY_NOT_REGISTERED;
#ifdef DEBUG
        char* cs = aClass.ToString();
        printf("+++ nsRDFModule: unable to create factory for %s\n", cs);
        nsCRT::free(cs);
#endif
    }

    if (fact) {
        rv = fact->QueryInterface(aIID, r_classObj);
    }

    return rv;
}

//----------------------------------------

struct Components {
    const char* mDescription;
    const nsID* mCID;
    const char* mProgID;
};

// The list of components we register
static Components gComponents[] = {
    // register our build-in datasources:
    { "RDF Composite Data Source", &kRDFCompositeDataSourceCID,
      NS_RDF_DATASOURCE_PROGID_PREFIX "composite-datasource", },
    { "RDF File System Data Source", &kRDFFileSystemDataSourceCID,
      NS_RDF_DATASOURCE_PROGID_PREFIX "files", },
    { "RDF FTP Data Source", &kRDFFTPDataSourceCID,
      NS_RDF_DATASOURCE_PROGID_PREFIX "ftp", },
    { "RDF In-Memory Data Source", &kRDFInMemoryDataSourceCID,
      NS_RDF_DATASOURCE_PROGID_PREFIX "in-memory-datasource", },
    { "Local Store", &kLocalStoreCID,
      NS_RDF_DATASOURCE_PROGID_PREFIX "local-store", },
    { "RDF XML Data Source", &kRDFXMLDataSourceCID,
      NS_RDF_DATASOURCE_PROGID_PREFIX "xml-datasource", },

    // register our built-in resource factories:
    { "RDF Default Resource Factory", &kRDFDefaultResourceCID,
      // Note: default resource factory has no name= part
      NS_RDF_RESOURCE_FACTORY_PROGID, },

    // register all the other rdf components:
    { "RDF Content Sink", &kRDFContentSinkCID,
      NS_RDF_PROGID "/content-sink", },
    { "RDF Container", &kRDFContainerCID,
      NS_RDF_PROGID "/container", },
    { "RDF Container Utilities", &kRDFContainerUtilsCID,
      NS_RDF_PROGID "/container-utils", },
    { "RDF Service", &kRDFServiceCID,
      NS_RDF_PROGID "/rdf-service", },
    { "XUL Sort Service", &kXULSortServiceCID,
      NS_RDF_PROGID "/xul-sort-service", },
    { "XUL Template Builder", &kXULTemplateBuilderCID,
      NS_RDF_PROGID "/xul-template-builder", },
    { "XUL Content Sink", &kXULContentSinkCID,
      NS_RDF_PROGID "/xul-content-sink", },
    { "XUL Document", &kXULDocumentCID,
      NS_RDF_PROGID "/xul-document", },
    { "XUL PopupListener", &kXULPopupListenerCID,
      NS_RDF_PROGID "/xul-popup-listener", },
    { "XUL KeyListener", &kXULKeyListenerCID,
      NS_RDF_PROGID "/xul-key-listener", },
    { "XUL CommandDispatcher", &kXULCommandDispatcherCID,
      NS_RDF_PROGID "/xul-command-dispatcher", },
    { "XUL Controllers", &kXULControllersCID,
      NS_RDF_PROGID "/xul-controllers", },
    { "XUL Content Utilities", &kXULContentUtilsCID,
      NS_RDF_PROGID "/xul-content-utils", },
    { "XUL Prototype Cache", &kXULPrototypeCacheCID,
      NS_RDF_PROGID "/xul-prototype-cache", },
};
#define NUM_COMPONENTS (sizeof(gComponents) / sizeof(gComponents[0]))

NS_IMETHODIMP
nsRDFModule::RegisterSelf(nsIComponentManager *aCompMgr,
                          nsIFileSpec* aPath,
                          const char* registryLocation,
                          const char* componentType)
{
    nsresult rv = NS_OK;

#ifdef DEBUG
    printf("*** Registering rdf components\n");
#endif

    Components* cp = gComponents;
    Components* end = cp + NUM_COMPONENTS;
    while (cp < end) {
        rv = aCompMgr->RegisterComponentSpec(*cp->mCID, cp->mDescription,
                                             cp->mProgID, aPath, PR_TRUE,
                                             PR_TRUE);
        if (NS_FAILED(rv)) {
#ifdef DEBUG
            printf("nsRDFModule: unable to register %s component => %x\n",
                   cp->mDescription, rv);
#endif
            break;
        }
        cp++;
    }

    return rv;
}

NS_IMETHODIMP
nsRDFModule::UnregisterSelf(nsIComponentManager* aCompMgr,
                            nsIFileSpec* aPath,
                            const char* registryLocation)
{
#ifdef DEBUG
    printf("*** Unregistering rdf components\n");
#endif
    Components* cp = gComponents;
    Components* end = cp + NUM_COMPONENTS;
    while (cp < end) {
        nsresult rv = aCompMgr->UnregisterComponentSpec(*cp->mCID, aPath);
        if (NS_FAILED(rv)) {
#ifdef DEBUG
            printf("nsRDFModule: unable to unregister %s component => %x\n",
                   cp->mDescription, rv);
#endif
        }
        cp++;
    }

    return NS_OK;
}

NS_IMETHODIMP
nsRDFModule::CanUnload(nsIComponentManager *aCompMgr, PRBool *okToUnload)
{
    if (!okToUnload) {
        return NS_ERROR_INVALID_POINTER;
    }
    *okToUnload = PR_FALSE;
    return NS_ERROR_FAILURE;
}

//----------------------------------------------------------------------

static nsRDFModule *gModule = NULL;

extern "C" NS_EXPORT nsresult NSGetModule(nsIComponentManager *servMgr,
                                          nsIFileSpec* location,
                                          nsIModule** return_cobj)
{
    nsresult rv = NS_OK;

    NS_ASSERTION(return_cobj, "Null argument");
    NS_ASSERTION(gModule == NULL, "nsRDFModule: Module already created.");

    // Create an initialize the layout module instance
    nsRDFModule *m = new nsRDFModule();
    if (!m) {
        return NS_ERROR_OUT_OF_MEMORY;
    }

    // Increase refcnt and store away nsIModule interface to m in return_cobj
    rv = m->QueryInterface(nsIModule::GetIID(), (void**)return_cobj);
    if (NS_FAILED(rv)) {
        delete m;
        m = nsnull;
    }
    gModule = m;                  // WARNING: Weak Reference
    return rv;
}
