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
#include "nsIHistoryDataSource.h"
#include "nsIRDFCompositeDataSource.h"
#include "nsIRDFContentModelBuilder.h"
#include "nsIRDFContentSink.h"
#include "nsIRDFDocument.h"
#include "nsIRDFService.h"
#include "nsIRDFXMLDataSource.h"
#include "nsIXULContentSink.h"
#include "nsISupports.h"
#include "nsRDFBaseDataSources.h"
#include "nsRDFBuiltInDataSources.h"
#include "nsIRDFFileSystem.h"
#include "nsIRDFFind.h"
#include "nsRDFCID.h"
#include "nsIComponentManager.h"
#include "rdf.h"
#include "nsIXULSortService.h"
#include "nsIXULDocumentInfo.h"
#include "nsIServiceManager.h"
#include "nsCOMPtr.h"

static NS_DEFINE_CID(kComponentManagerCID, NS_COMPONENTMANAGER_CID);
static NS_DEFINE_IID(kISupportsIID, NS_ISUPPORTS_IID);
static NS_DEFINE_IID(kIFactoryIID,  NS_IFACTORY_IID);

static NS_DEFINE_CID(kRDFBookmarkDataSourceCID,  NS_RDFBOOKMARKDATASOURCE_CID);
static NS_DEFINE_CID(kRDFCompositeDataSourceCID, NS_RDFCOMPOSITEDATASOURCE_CID);
static NS_DEFINE_CID(kRDFContentSinkCID,         NS_RDFCONTENTSINK_CID);
static NS_DEFINE_CID(kRDFDefaultResourceCID,     NS_RDFDEFAULTRESOURCE_CID);
static NS_DEFINE_CID(kRDFFileSystemDataSourceCID,NS_RDFFILESYSTEMDATASOURCE_CID);
static NS_DEFINE_CID(kRDFFindDataSourceCID,      NS_RDFFINDDATASOURCE_CID);
static NS_DEFINE_CID(kRDFFTPDataSourceCID,       NS_RDFFTPDATASOURCE_CID);
static NS_DEFINE_CID(kRDFHTMLBuilderCID,         NS_RDFHTMLBUILDER_CID);
static NS_DEFINE_CID(kRDFHistoryDataSourceCID,   NS_RDFHISTORYDATASOURCE_CID);
static NS_DEFINE_CID(kRDFInMemoryDataSourceCID,  NS_RDFINMEMORYDATASOURCE_CID);
static NS_DEFINE_CID(kRDFMenuBuilderCID,         NS_RDFMENUBUILDER_CID);
static NS_DEFINE_CID(kRDFServiceCID,             NS_RDFSERVICE_CID);
static NS_DEFINE_CID(kRDFToolbarBuilderCID,      NS_RDFTOOLBARBUILDER_CID);
static NS_DEFINE_CID(kRDFTreeBuilderCID,         NS_RDFTREEBUILDER_CID);
static NS_DEFINE_CID(kRDFXMLDataSourceCID,       NS_RDFXMLDATASOURCE_CID);
static NS_DEFINE_CID(kRDFXULBuilderCID,          NS_RDFXULBUILDER_CID);
static NS_DEFINE_CID(kXULContentSinkCID,         NS_XULCONTENTSINK_CID);
static NS_DEFINE_CID(kXULDataSourceCID,	         NS_XULDATASOURCE_CID);
static NS_DEFINE_CID(kXULDocumentCID,            NS_XULDOCUMENT_CID);
static NS_DEFINE_CID(kXULSortServiceCID,         NS_XULSORTSERVICE_CID);
static NS_DEFINE_CID(kXULDocumentInfoCID,        NS_XULDOCUMENTINFO_CID);

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
    else if (mClassID.Equals(kRDFInMemoryDataSourceCID)) {

        if (NS_FAILED(rv = NS_NewRDFInMemoryDataSource((nsIRDFDataSource**) &inst)))
            return rv;
    }
    else if (mClassID.Equals(kRDFXMLDataSourceCID)) {
        if (NS_FAILED(rv = NS_NewRDFXMLDataSource((nsIRDFXMLDataSource**) &inst)))
          return rv;
    }
    else if (mClassID.Equals(kRDFBookmarkDataSourceCID)) {
        if (NS_FAILED(rv = NS_NewRDFBookmarkDataSource((nsIRDFDataSource**) &inst)))
            return rv;
    }
    else if (mClassID.Equals(kRDFFileSystemDataSourceCID)) {
        if (NS_FAILED(rv = NS_NewRDFFileSystemDataSource((nsIRDFDataSource**) &inst)))
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
    else if (mClassID.Equals(kRDFHistoryDataSourceCID)) {
        if (NS_FAILED(rv = NS_NewHistoryDataSource((nsIHistoryDataSource**) &inst)))
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
    else if (mClassID.Equals(kRDFHTMLBuilderCID)) {
        if (NS_FAILED(rv = NS_NewRDFHTMLBuilder((nsIRDFContentModelBuilder**) &inst)))
            return rv;
    }
    else if (mClassID.Equals(kRDFMenuBuilderCID)) {
        if (NS_FAILED(rv = NS_NewRDFMenuBuilder((nsIRDFContentModelBuilder**) &inst)))
            return rv;
    }
    else if (mClassID.Equals(kRDFToolbarBuilderCID)) {
        if (NS_FAILED(rv = NS_NewRDFToolbarBuilder((nsIRDFContentModelBuilder**) &inst)))
            return rv;
    }
    else if (mClassID.Equals(kRDFTreeBuilderCID)) {
        if (NS_FAILED(rv = NS_NewRDFTreeBuilder((nsIRDFContentModelBuilder**) &inst)))
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
	else if (mClassID.Equals(kXULDataSourceCID)) {
		if (NS_FAILED(rv = NS_NewXULDataSource((nsIRDFXMLDataSource**) &inst)))
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
NSGetFactory(nsISupports* aServMgr,
             const nsCID &aClass,
             const char *aClassName,
             const char *aProgID,
             nsIFactory **aFactory)
{
    if (! aFactory)
        return NS_ERROR_NULL_POINTER;

    RDFFactoryImpl* factory = new RDFFactoryImpl(aClass, aClassName, aProgID);
    if (factory == nsnull)
        return NS_ERROR_OUT_OF_MEMORY;

    NS_ADDREF(factory);
    *aFactory = factory;
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
    rv = compMgr->RegisterComponent(kRDFBookmarkDataSourceCID,  
                                         "Bookmarks",
                                         NS_RDF_DATASOURCE_PROGID_PREFIX "bookmarks",
                                         aPath, PR_TRUE, PR_TRUE);
    if (NS_FAILED(rv)) goto done;
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
    rv = compMgr->RegisterComponent(kRDFHistoryDataSourceCID,  
                                         "RDF History Data Source",
                                         NS_RDF_DATASOURCE_PROGID_PREFIX "history",
                                         aPath, PR_TRUE, PR_TRUE);
    if (NS_FAILED(rv)) goto done;
    rv = compMgr->RegisterComponent(kRDFInMemoryDataSourceCID,
                                         "RDF In-Memory Data Source",
                                         NS_RDF_DATASOURCE_PROGID_PREFIX "in-memory-datasource",
                                         aPath, PR_TRUE, PR_TRUE);
    if (NS_FAILED(rv)) goto done;
    rv = compMgr->RegisterComponent(kRDFXMLDataSourceCID,
                                         "RDF XML Data Source",
                                         NS_RDF_DATASOURCE_PROGID_PREFIX "xml-datasource",
                                         aPath, PR_TRUE, PR_TRUE);
    if (NS_FAILED(rv)) goto done;
    rv = compMgr->RegisterComponent(kXULDataSourceCID,
                                         "XUL Data Source",
                                         NS_RDF_DATASOURCE_PROGID_PREFIX "xul-datasource",
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
                                         NS_RDF_PROGID "|content-sink",
                                         aPath, PR_TRUE, PR_TRUE);
    if (NS_FAILED(rv)) goto done;
    rv = compMgr->RegisterComponent(kRDFHTMLBuilderCID,
                                         "RDF HTML Builder",
                                         NS_RDF_PROGID "/html-builder",
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
    rv = compMgr->RegisterComponent(kRDFTreeBuilderCID,
                                         "RDF Tree Builder",
                                         NS_RDF_PROGID "/tree-builder",
                                         aPath, PR_TRUE, PR_TRUE);
    if (NS_FAILED(rv)) goto done;
    rv = compMgr->RegisterComponent(kRDFMenuBuilderCID,
                                         "RDF Menu Builder",
                                         NS_RDF_PROGID "/menu-builder",
                                         aPath, PR_TRUE, PR_TRUE);
    if (NS_FAILED(rv)) goto done;
    rv = compMgr->RegisterComponent(kRDFToolbarBuilderCID,
                                         "RDF Toolbar Builder",
                                         NS_RDF_PROGID "/toolbar-builder",
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

    rv = compMgr->UnregisterComponent(kRDFBookmarkDataSourceCID,  aPath);
    if (NS_FAILED(rv)) goto done;
    rv = compMgr->UnregisterComponent(kRDFFileSystemDataSourceCID,aPath);
    if (NS_FAILED(rv)) goto done;
    rv = compMgr->UnregisterComponent(kRDFFindDataSourceCID,      aPath);
    if (NS_FAILED(rv)) goto done;
    rv = compMgr->UnregisterComponent(kRDFFTPDataSourceCID,       aPath);
    if (NS_FAILED(rv)) goto done;
    rv = compMgr->UnregisterComponent(kRDFHistoryDataSourceCID,   aPath);
    if (NS_FAILED(rv)) goto done;
    rv = compMgr->UnregisterComponent(kRDFCompositeDataSourceCID, aPath);
    if (NS_FAILED(rv)) goto done;
    rv = compMgr->UnregisterComponent(kRDFInMemoryDataSourceCID,  aPath);
    if (NS_FAILED(rv)) goto done;
    rv = compMgr->UnregisterComponent(kRDFXMLDataSourceCID,       aPath);
    if (NS_FAILED(rv)) goto done;
    rv = compMgr->UnregisterComponent(kXULDataSourceCID,          aPath);
    if (NS_FAILED(rv)) goto done;

    rv = compMgr->UnregisterComponent(kRDFDefaultResourceCID,     aPath);
    if (NS_FAILED(rv)) goto done;

    rv = compMgr->UnregisterComponent(kRDFContentSinkCID,         aPath);
    if (NS_FAILED(rv)) goto done;
    rv = compMgr->UnregisterComponent(kRDFHTMLBuilderCID,         aPath);
    if (NS_FAILED(rv)) goto done;
    rv = compMgr->UnregisterComponent(kRDFServiceCID,             aPath);
    if (NS_FAILED(rv)) goto done;
    rv = compMgr->UnregisterComponent(kXULSortServiceCID,         aPath);
    if (NS_FAILED(rv)) goto done;
    rv = compMgr->UnregisterComponent(kRDFTreeBuilderCID,         aPath);
    if (NS_FAILED(rv)) goto done;
    rv = compMgr->UnregisterComponent(kRDFMenuBuilderCID,         aPath);
    if (NS_FAILED(rv)) goto done;
    rv = compMgr->UnregisterComponent(kRDFToolbarBuilderCID,      aPath);
    if (NS_FAILED(rv)) goto done;
    rv = compMgr->UnregisterComponent(kRDFXULBuilderCID,          aPath);
    if (NS_FAILED(rv)) goto done;
    rv = compMgr->UnregisterComponent(kXULContentSinkCID,         aPath);
    if (NS_FAILED(rv)) goto done;
    rv = compMgr->UnregisterComponent(kXULDocumentCID,            aPath);
    if (NS_FAILED(rv)) goto done;
    rv = compMgr->UnregisterComponent(kXULDocumentInfoCID,        aPath);

  done:
    (void)servMgr->ReleaseService(kComponentManagerCID, compMgr);
    return rv;
}

