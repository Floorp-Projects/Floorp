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

/*

  The RDF factory implementation.

 */

#include "nsIFactory.h"
#include "nsIGenericFactory.h"
#include "nsILocalStore.h"
#include "nsIRDFContainer.h"
#include "nsIRDFContainerUtils.h"
#include "nsIRDFCompositeDataSource.h"
#include "nsIRDFContentModelBuilder.h"
#include "nsIRDFContentSink.h"
#include "nsIRDFDocument.h"
#include "nsIRDFService.h"
#include "nsIXULContentSink.h"
#include "nsISupports.h"
#include "nsRDFBaseDataSources.h"
#include "nsRDFBuiltInDataSources.h"
#include "nsIRDFFileSystem.h"
#include "nsIRDFSearch.h"
#include "nsIRDFFind.h"
#include "nsRDFCID.h"
#include "nsIComponentManager.h"
#include "rdf.h"
#include "nsIXULSortService.h"
#include "nsIXULDocumentInfo.h"
#include "nsIXULPopupListener.h"
#include "nsIXULKeyListener.h"
#include "nsIXULFocusTracker.h"
#include "nsIServiceManager.h"
#include "nsCOMPtr.h"

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
static NS_DEFINE_CID(kRDFXULBuilderCID,                   NS_RDFXULBUILDER_CID);
static NS_DEFINE_CID(kXULContentSinkCID,                  NS_XULCONTENTSINK_CID);
static NS_DEFINE_CID(kXULDocumentCID,                     NS_XULDOCUMENT_CID);
static NS_DEFINE_CID(kXULSortServiceCID,                  NS_XULSORTSERVICE_CID);
static NS_DEFINE_CID(kXULDocumentInfoCID,                 NS_XULDOCUMENTINFO_CID);
static NS_DEFINE_CID(kXULPopupListenerCID,                NS_XULPOPUPLISTENER_CID);
static NS_DEFINE_CID(kXULKeyListenerCID,                  NS_XULKEYLISTENER_CID);
static NS_DEFINE_CID(kXULFocusTrackerCID,                 NS_XULFOCUSTRACKER_CID);
static NS_DEFINE_CID(kXULTemplateBuilderCID,              NS_XULTEMPLATEBUILDER_CID);

class RDFFactoryImpl : public nsIFactory
{
public:
    RDFFactoryImpl(const nsCID &aClass, const char* className, const char* progID);

    // nsISupports methods
    NS_DECL_ISUPPORTS

    // nsIFactory methods
    NS_IMETHOD CreateInstance(nsISupports *aOuter,
                              const nsIID &aIID,
                              void **aResult);

    NS_IMETHOD LockFactory(PRBool aLock);

protected:
    virtual ~RDFFactoryImpl();

protected:
    nsCID       mClassID;
    const char* mClassName;
    const char* mProgID;
};

////////////////////////////////////////////////////////////////////////

RDFFactoryImpl::RDFFactoryImpl(const nsCID &aClass, 
                               const char* className,
                               const char* progID)
    : mClassID(aClass), mClassName(className), mProgID(progID)
{
    NS_INIT_REFCNT();
}

RDFFactoryImpl::~RDFFactoryImpl()
{
    NS_ASSERTION(mRefCnt == 0, "non-zero refcnt at destruction");
}

NS_IMETHODIMP
RDFFactoryImpl::QueryInterface(const nsIID &aIID, void **aResult)
{
    if (! aResult)
        return NS_ERROR_NULL_POINTER;

    // Always NULL result, in case of failure
    *aResult = nsnull;

    if (aIID.Equals(kISupportsIID)) {
        *aResult = NS_STATIC_CAST(nsISupports*, this);
        AddRef();
        return NS_OK;
    } else if (aIID.Equals(kIFactoryIID)) {
        *aResult = NS_STATIC_CAST(nsIFactory*, this);
        AddRef();
        return NS_OK;
    }
    return NS_NOINTERFACE;
}

NS_IMPL_ADDREF(RDFFactoryImpl);
NS_IMPL_RELEASE(RDFFactoryImpl);

extern nsresult
NS_NewDefaultResource(nsIRDFResource** aResult);

NS_IMETHODIMP
RDFFactoryImpl::CreateInstance(nsISupports *aOuter,
                               const nsIID &aIID,
                               void **aResult)
{
    if (! aResult)
        return NS_ERROR_NULL_POINTER;

    
    if (aOuter)
        return NS_ERROR_NO_AGGREGATION;

    *aResult = nsnull;

    nsresult rv;

    nsISupports *inst = nsnull;
    if (mClassID.Equals(kRDFServiceCID)) {
        if (NS_FAILED(rv = NS_NewRDFService((nsIRDFService**) &inst)))
            return rv;
    }
    else if (mClassID.Equals(kXULSortServiceCID)) {
        if (NS_FAILED(rv = NS_NewXULSortService((nsIXULSortService**) &inst)))
            return rv;
    }
    else if (mClassID.Equals(kXULPopupListenerCID)) {
        if (NS_FAILED(rv = NS_NewXULPopupListener((nsIXULPopupListener**) &inst)))
            return rv;
    }
    else if (mClassID.Equals(kXULKeyListenerCID)) {
        if (NS_FAILED(rv = NS_NewXULKeyListener((nsIXULKeyListener**) &inst)))
            return rv;
    }
    else if (mClassID.Equals(kXULFocusTrackerCID)) {
        if (NS_FAILED(rv = NS_NewXULFocusTracker((nsIXULFocusTracker**) &inst)))
            return rv;
    }
    else if (mClassID.Equals(kRDFXMLDataSourceCID)) {
        if (NS_FAILED(rv = NS_NewRDFXMLDataSource((nsIRDFDataSource**) &inst)))
          return rv;
    }
    else if (mClassID.Equals(kRDFFileSystemDataSourceCID)) {
        if (NS_FAILED(rv = NS_NewRDFFileSystemDataSource((nsIRDFDataSource**) &inst)))
            return rv;
    }
    else if (mClassID.Equals(kRDFSearchDataSourceCID)) {
        if (NS_FAILED(rv = NS_NewRDFSearchDataSource((nsIRDFDataSource**) &inst)))
            return rv;
    }
    else if (mClassID.Equals(kRDFFindDataSourceCID)) {
        if (NS_FAILED(rv = NS_NewRDFFindDataSource((nsIRDFDataSource**) &inst)))
            return rv;
    }
    else if (mClassID.Equals(kRDFFTPDataSourceCID)) {
        if (NS_FAILED(rv = NS_NewRDFFTPDataSource((nsIRDFDataSource**) &inst)))
            return rv;
    }
    else if (mClassID.Equals(kRDFCompositeDataSourceCID)) {
        if (NS_FAILED(rv = NS_NewRDFCompositeDataSource((nsIRDFCompositeDataSource**) &inst)))
            return rv;
    }
    else if (mClassID.Equals(kRDFContainerCID)) {
        if (NS_FAILED(rv = NS_NewRDFContainer((nsIRDFContainer**) &inst)))
            return rv;
    }
    else if (mClassID.Equals(kRDFContainerUtilsCID)) {
        if (NS_FAILED(rv = NS_NewRDFContainerUtils((nsIRDFContainerUtils**) &inst)))
            return rv;
    }
    else if (mClassID.Equals(kXULDocumentCID)) {
        if (NS_FAILED(rv = NS_NewXULDocument((nsIRDFDocument**) &inst)))
            return rv;
    }
    else if (mClassID.Equals(kXULDocumentInfoCID)) {
        if (NS_FAILED(rv = NS_NewXULDocumentInfo((nsIXULDocumentInfo**) &inst)))
            return rv;
    }
    else if (mClassID.Equals(kXULTemplateBuilderCID)) {
        if (NS_FAILED(rv = NS_NewXULTemplateBuilder((nsIRDFContentModelBuilder**) &inst)))
            return rv;
    }
    else if (mClassID.Equals(kRDFXULBuilderCID)) {
        if (NS_FAILED(rv = NS_NewRDFXULBuilder((nsIRDFContentModelBuilder**) &inst)))
            return rv;
    }
    else if (mClassID.Equals(kRDFContentSinkCID)) {
        if (NS_FAILED(rv = NS_NewRDFContentSink((nsIRDFContentSink**) &inst)))
            return rv;
    }
	else if (mClassID.Equals(kXULContentSinkCID)) {
        if (NS_FAILED(rv = NS_NewXULContentSink((nsIXULContentSink**) &inst)))
            return rv;
    }
	else if (mClassID.Equals(kRDFDefaultResourceCID)) {
        if (NS_FAILED(rv = NS_NewDefaultResource((nsIRDFResource**) &inst)))
            return rv;
    }
    else if (mClassID.Equals(kLocalStoreCID)) {
        if (NS_FAILED(rv = NS_NewLocalStore((nsILocalStore**) &inst)))
            return rv;
    }
    else {
        return NS_ERROR_NO_INTERFACE;
    }

    if (! inst)
        return NS_ERROR_OUT_OF_MEMORY;

    if (NS_FAILED(rv = inst->QueryInterface(aIID, aResult))) {
        // We didn't get the right interface.
        NS_ERROR("didn't support the interface you wanted");
    }

    NS_IF_RELEASE(inst);
    return rv;
}

nsresult RDFFactoryImpl::LockFactory(PRBool aLock)
{
    // Not implemented in simplest case.
    return NS_OK;
}

////////////////////////////////////////////////////////////////////////



// return the proper factory to the caller
extern "C" PR_IMPLEMENT(nsresult)
NSGetFactory(nsISupports* aServiceMgr,
             const nsCID &aClass,
             const char *aClassName,
             const char *aProgID,
             nsIFactory **aFactory)
{
    if (! aFactory)
        return NS_ERROR_NULL_POINTER;

    if (aClass.Equals(kRDFInMemoryDataSourceCID)) {
        nsIGenericFactory::ConstructorProcPtr constructor;

        constructor = NS_NewRDFInMemoryDataSource;

        // XXX Factor this part out if we get more of these
        nsresult rv;
        NS_WITH_SERVICE1(nsIComponentManager, compMgr, aServiceMgr, kComponentManagerCID, &rv);
        if (NS_FAILED(rv)) return rv;

        nsCOMPtr<nsIGenericFactory> factory;
        rv = compMgr->CreateInstance(kGenericFactoryCID,
                                     nsnull,
                                     nsIGenericFactory::GetIID(),
                                     getter_AddRefs(factory));

        if (NS_FAILED(rv)) return rv;

        rv = factory->SetConstructor(constructor);
        if (NS_FAILED(rv)) return rv;

        *aFactory = factory;
        NS_ADDREF(*aFactory);
    }
    else {
        RDFFactoryImpl* factory = new RDFFactoryImpl(aClass, aClassName, aProgID);
        if (factory == nsnull)
            return NS_ERROR_OUT_OF_MEMORY;

        NS_ADDREF(factory);
        *aFactory = factory;
    }

    return NS_OK;
}




extern "C" PR_IMPLEMENT(nsresult)
NSRegisterSelf(nsISupports* aServMgr , const char* aPath)
{
    nsresult rv;

    nsCOMPtr<nsIServiceManager> servMgr(do_QueryInterface(aServMgr, &rv));
    if (NS_FAILED(rv)) return rv;

    nsIComponentManager* compMgr;
    rv = servMgr->GetService(kComponentManagerCID, 
                             nsIComponentManager::GetIID(), 
                             (nsISupports**)&compMgr);
    if (NS_FAILED(rv)) return rv;

    // register our build-in datasources:
    rv = compMgr->RegisterComponent(kRDFCompositeDataSourceCID, 
                                         "RDF Composite Data Source",
                                         NS_RDF_DATASOURCE_PROGID_PREFIX "composite-datasource",
                                         aPath, PR_TRUE, PR_TRUE);
    if (NS_FAILED(rv)) goto done;
    rv = compMgr->RegisterComponent(kRDFFileSystemDataSourceCID,  
                                         "RDF File System Data Source",
                                         NS_RDF_DATASOURCE_PROGID_PREFIX "files",
                                         aPath, PR_TRUE, PR_TRUE);
    if (NS_FAILED(rv)) goto done;
    rv = compMgr->RegisterComponent(kRDFSearchDataSourceCID,  
                                         "RDF Internet Search Data Source",
                                         NS_RDF_DATASOURCE_PROGID_PREFIX "internetsearch",
                                         aPath, PR_TRUE, PR_TRUE);
    if (NS_FAILED(rv)) goto done;
    rv = compMgr->RegisterComponent(kRDFFindDataSourceCID,  
                                         "RDF Find Data Source",
                                         NS_RDF_DATASOURCE_PROGID_PREFIX "find",
                                         aPath, PR_TRUE, PR_TRUE);
    if (NS_FAILED(rv)) goto done;
    rv = compMgr->RegisterComponent(kRDFFTPDataSourceCID,  
                                         "RDF FTP Data Source",
                                         NS_RDF_DATASOURCE_PROGID_PREFIX "ftp",
                                         aPath, PR_TRUE, PR_TRUE);
    if (NS_FAILED(rv)) goto done;
    rv = compMgr->RegisterComponent(kRDFInMemoryDataSourceCID,
                                         "RDF In-Memory Data Source",
                                         NS_RDF_DATASOURCE_PROGID_PREFIX "in-memory-datasource",
                                         aPath, PR_TRUE, PR_TRUE);
    if (NS_FAILED(rv)) goto done;
    rv = compMgr->RegisterComponent(kLocalStoreCID,
                                    "Local Store",
                                    NS_RDF_DATASOURCE_PROGID_PREFIX "local-store",
                                    aPath, PR_TRUE, PR_TRUE);
    if (NS_FAILED(rv)) goto done;
    rv = compMgr->RegisterComponent(kRDFXMLDataSourceCID,
                                         "RDF XML Data Source",
                                         NS_RDF_DATASOURCE_PROGID_PREFIX "xml-datasource",
                                         aPath, PR_TRUE, PR_TRUE);
    if (NS_FAILED(rv)) goto done;

    // register our built-in resource factories:
    rv = compMgr->RegisterComponent(kRDFDefaultResourceCID,
                                         "RDF Default Resource Factory",
                                         NS_RDF_RESOURCE_FACTORY_PROGID,        // default resource factory has no name= part
                                         aPath, PR_TRUE, PR_TRUE);
    if (NS_FAILED(rv)) goto done;

    // register all the other rdf components:
    rv = compMgr->RegisterComponent(kRDFContentSinkCID,
                                         "RDF Content Sink",
                                         NS_RDF_PROGID "/content-sink",
                                         aPath, PR_TRUE, PR_TRUE);
    if (NS_FAILED(rv)) goto done;
    rv = compMgr->RegisterComponent(kRDFContainerCID, 
                                         "RDF Container",
                                         NS_RDF_PROGID "/container",
                                         aPath, PR_TRUE, PR_TRUE);
    if (NS_FAILED(rv)) goto done;
    rv = compMgr->RegisterComponent(kRDFContainerUtilsCID, 
                                         "RDF Container Utilities",
                                         NS_RDF_PROGID "/container-utils",
                                         aPath, PR_TRUE, PR_TRUE);
    if (NS_FAILED(rv)) goto done;
    rv = compMgr->RegisterComponent(kRDFServiceCID,
                                         "RDF Service",
                                         NS_RDF_PROGID "/rdf-service",
                                         aPath, PR_TRUE, PR_TRUE);
    if (NS_FAILED(rv)) goto done;
    rv = compMgr->RegisterComponent(kXULSortServiceCID,
                                         "XUL Sort Service",
                                         NS_RDF_PROGID "/xul-sort-service",
                                         aPath, PR_TRUE, PR_TRUE);
    if (NS_FAILED(rv)) goto done;
    rv = compMgr->RegisterComponent(kXULTemplateBuilderCID,
                                         "XUL Template Builder",
                                         NS_RDF_PROGID "/xul-template-builder",
                                         aPath, PR_TRUE, PR_TRUE);
    if (NS_FAILED(rv)) goto done;
    rv = compMgr->RegisterComponent(kRDFXULBuilderCID,
                                         "RDF XUL Builder",
                                         NS_RDF_PROGID "/xul-builder",
                                         aPath, PR_TRUE, PR_TRUE);
    if (NS_FAILED(rv)) goto done;
    rv = compMgr->RegisterComponent(kXULContentSinkCID,
                                         "XUL Content Sink",
                                         NS_RDF_PROGID "/xul-content-sink",
                                         aPath, PR_TRUE, PR_TRUE);
    if (NS_FAILED(rv)) goto done;
    rv = compMgr->RegisterComponent(kXULDocumentCID,
                                         "XUL Document",
                                         NS_RDF_PROGID "/xul-document",
                                         aPath, PR_TRUE, PR_TRUE);

    if (NS_FAILED(rv)) goto done;
    rv = compMgr->RegisterComponent(kXULDocumentInfoCID,
                                         "XUL Document Info",
                                         NS_RDF_PROGID "/xul-document-info",
                                         aPath, PR_TRUE, PR_TRUE);

    if (NS_FAILED(rv)) goto done;
    rv = compMgr->RegisterComponent(kXULPopupListenerCID,
                                         "XUL PopupListener",
                                         NS_RDF_PROGID "/xul-popup-listener",
                                         aPath, PR_TRUE, PR_TRUE);
    
    if (NS_FAILED(rv)) goto done;
    rv = compMgr->RegisterComponent(kXULKeyListenerCID,
                                         "XUL KeyListener",
                                         NS_RDF_PROGID "/xul-key-listener",
                                         aPath, PR_TRUE, PR_TRUE);
                                         
    if (NS_FAILED(rv)) goto done;
    rv = compMgr->RegisterComponent(kXULFocusTrackerCID,
                                         "XUL FocusTracker",
                                         NS_RDF_PROGID "/xul-focus-tracker",
                                         aPath, PR_TRUE, PR_TRUE);

  done:
    (void)servMgr->ReleaseService(kComponentManagerCID, compMgr);
    return rv;
}


extern "C" PR_IMPLEMENT(nsresult)
NSUnregisterSelf(nsISupports* aServMgr, const char* aPath)
{
    nsresult rv;

    nsCOMPtr<nsIServiceManager> servMgr(do_QueryInterface(aServMgr, &rv));
    if (NS_FAILED(rv)) return rv;

    nsIComponentManager* compMgr;
    rv = servMgr->GetService(kComponentManagerCID, 
                             nsIComponentManager::GetIID(), 
                             (nsISupports**)&compMgr);
    if (NS_FAILED(rv)) return rv;

    rv = compMgr->UnregisterComponent(kRDFFileSystemDataSourceCID,aPath);
    if (NS_FAILED(rv)) goto done;
    rv = compMgr->UnregisterComponent(kRDFSearchDataSourceCID,aPath);
    if (NS_FAILED(rv)) goto done;
    rv = compMgr->UnregisterComponent(kRDFFindDataSourceCID,      aPath);
    if (NS_FAILED(rv)) goto done;
    rv = compMgr->UnregisterComponent(kRDFFTPDataSourceCID,       aPath);
    if (NS_FAILED(rv)) goto done;
    rv = compMgr->UnregisterComponent(kRDFCompositeDataSourceCID, aPath);
    if (NS_FAILED(rv)) goto done;
    rv = compMgr->UnregisterComponent(kRDFInMemoryDataSourceCID,  aPath);
    if (NS_FAILED(rv)) goto done;
    rv = compMgr->UnregisterComponent(kLocalStoreCID,             aPath);
    if (NS_FAILED(rv)) goto done;
    rv = compMgr->UnregisterComponent(kRDFXMLDataSourceCID,       aPath);
    if (NS_FAILED(rv)) goto done;
    rv = compMgr->UnregisterComponent(kRDFDefaultResourceCID,     aPath);
    if (NS_FAILED(rv)) goto done;
    rv = compMgr->UnregisterComponent(kRDFContentSinkCID,         aPath);
    if (NS_FAILED(rv)) goto done;
    rv = compMgr->UnregisterComponent(kRDFServiceCID,             aPath);
    if (NS_FAILED(rv)) goto done;
    rv = compMgr->UnregisterComponent(kXULSortServiceCID,         aPath);
    if (NS_FAILED(rv)) goto done;
    rv = compMgr->UnregisterComponent(kXULTemplateBuilderCID,     aPath);
    if (NS_FAILED(rv)) goto done;
    rv = compMgr->UnregisterComponent(kRDFXULBuilderCID,          aPath);
    if (NS_FAILED(rv)) goto done;
    rv = compMgr->UnregisterComponent(kXULContentSinkCID,         aPath);
    if (NS_FAILED(rv)) goto done;
    rv = compMgr->UnregisterComponent(kXULDocumentCID,            aPath);
    if (NS_FAILED(rv)) goto done;
    rv = compMgr->UnregisterComponent(kXULDocumentInfoCID,        aPath);
    if (NS_FAILED(rv)) goto done;
    rv = compMgr->UnregisterComponent(kXULPopupListenerCID,       aPath);
    if (NS_FAILED(rv)) goto done;
    rv = compMgr->UnregisterComponent(kXULFocusTrackerCID,        aPath);

  done:
    (void)servMgr->ReleaseService(kComponentManagerCID, compMgr);
    return rv;
}

