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
#include "nsIRDFCompositeDataSource.h"
#include "nsIRDFContentModelBuilder.h"
#include "nsIRDFContentSink.h"
#include "nsIRDFDocument.h"
#include "nsIRDFService.h"
#include "nsIRDFXMLDataSource.h"
#include "nsISupports.h"
#include "nsRDFBaseDataSources.h"
#include "nsRDFBuiltInDataSources.h"
#include "nsRDFCID.h"
#include "nsRepository.h"

static NS_DEFINE_IID(kISupportsIID, NS_ISUPPORTS_IID);
static NS_DEFINE_IID(kIFactoryIID,  NS_IFACTORY_IID);

static NS_DEFINE_CID(kRDFBookmarkDataSourceCID,  NS_RDFBOOKMARKDATASOURCE_CID);
static NS_DEFINE_CID(kRDFCompositeDataSourceCID, NS_RDFCOMPOSITEDATASOURCE_CID);
static NS_DEFINE_CID(kRDFContentSinkCID,         NS_RDFCONTENTSINK_CID);
static NS_DEFINE_CID(kRDFHTMLBuilderCID,         NS_RDFHTMLBUILDER_CID);
static NS_DEFINE_CID(kRDFInMemoryDataSourceCID,  NS_RDFINMEMORYDATASOURCE_CID);
static NS_DEFINE_CID(kRDFServiceCID,             NS_RDFSERVICE_CID);
static NS_DEFINE_CID(kRDFTreeBuilderCID,         NS_RDFTREEBUILDER_CID);
static NS_DEFINE_CID(kRDFXMLDataSourceCID,       NS_RDFXMLDATASOURCE_CID);
static NS_DEFINE_CID(kRDFXULBuilderCID,          NS_RDFXULBUILDER_CID);
static NS_DEFINE_CID(kXULContentSinkCID,         NS_XULCONTENTSINK_CID);
static NS_DEFINE_CID(kXULDataSourceCID,			 NS_XULDATASOURCE_CID);
static NS_DEFINE_CID(kXULDocumentCID,            NS_XULDOCUMENT_CID);

class RDFFactoryImpl : public nsIFactory
{
public:
    RDFFactoryImpl(const nsCID &aClass);

    // nsISupports methods
    NS_DECL_ISUPPORTS

    // nsIFactory methods
    NS_IMETHOD CreateInstance(nsISupports *aOuter,
                              const nsIID &aIID,
                              void **aResult);

    NS_IMETHOD LockFactory(PRBool aLock);

protected:
    virtual ~RDFFactoryImpl();

private:
    nsCID     mClassID;
};

////////////////////////////////////////////////////////////////////////

RDFFactoryImpl::RDFFactoryImpl(const nsCID &aClass)
{
    NS_INIT_REFCNT();
    mClassID = aClass;
}

RDFFactoryImpl::~RDFFactoryImpl()
{
    NS_ASSERTION(mRefCnt == 0, "non-zero refcnt at destruction");
}

NS_IMETHODIMP
RDFFactoryImpl::QueryInterface(const nsIID &aIID,
                                      void **aResult)
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
    PRBool wasRefCounted = PR_TRUE;
    nsISupports *inst = nsnull;
    if (mClassID.Equals(kRDFServiceCID)) {
        if (NS_FAILED(rv = NS_NewRDFService((nsIRDFService**) &inst)))
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
    else if (mClassID.Equals(kRDFCompositeDataSourceCID)) {
        if (NS_FAILED(rv = NS_NewRDFCompositeDataSource((nsIRDFCompositeDataSource**) &inst)))
            return rv;
    }
    else if (mClassID.Equals(kXULDocumentCID)) {
        if (NS_FAILED(rv = NS_NewXULDocument((nsIRDFDocument**) &inst)))
            return rv;
    }
    else if (mClassID.Equals(kRDFHTMLBuilderCID)) {
        if (NS_FAILED(rv = NS_NewRDFHTMLBuilder((nsIRDFContentModelBuilder**) &inst)))
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
        if (NS_FAILED(rv = NS_NewXULContentSink((nsIRDFContentSink**) &inst)))
            return rv;
    }
    else {
        return NS_ERROR_NO_INTERFACE;
    }

    if (! inst)
        return NS_ERROR_OUT_OF_MEMORY;

    if (NS_FAILED(rv = inst->QueryInterface(aIID, aResult)))
        // We didn't get the right interface, so clean up
        delete inst;

    if (wasRefCounted)
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
NSGetFactory(const nsCID &aClass, nsISupports* aServiceManager, nsIFactory **aFactory)
{
    if (! aFactory)
        return NS_ERROR_NULL_POINTER;

    RDFFactoryImpl* factory = new RDFFactoryImpl(aClass);
    if (factory == nsnull)
        return NS_ERROR_OUT_OF_MEMORY;

    NS_ADDREF(factory);
    *aFactory = factory;
    return NS_OK;
}




extern "C" PR_IMPLEMENT(nsresult)
NSRegisterSelf(const char* aPath)
{
    nsRepository::RegisterFactory(kRDFBookmarkDataSourceCID,  aPath, PR_TRUE, PR_TRUE);
    nsRepository::RegisterFactory(kRDFCompositeDataSourceCID, aPath, PR_TRUE, PR_TRUE);
    nsRepository::RegisterFactory(kRDFContentSinkCID,         aPath, PR_TRUE, PR_TRUE);
    nsRepository::RegisterFactory(kRDFHTMLBuilderCID,         aPath, PR_TRUE, PR_TRUE);
    nsRepository::RegisterFactory(kRDFInMemoryDataSourceCID,  aPath, PR_TRUE, PR_TRUE);
    nsRepository::RegisterFactory(kRDFServiceCID,             aPath, PR_TRUE, PR_TRUE);
    nsRepository::RegisterFactory(kRDFTreeBuilderCID,         aPath, PR_TRUE, PR_TRUE);
    nsRepository::RegisterFactory(kRDFXMLDataSourceCID,       aPath, PR_TRUE, PR_TRUE);
    nsRepository::RegisterFactory(kRDFXULBuilderCID,          aPath, PR_TRUE, PR_TRUE);
    nsRepository::RegisterFactory(kXULContentSinkCID,         aPath, PR_TRUE, PR_TRUE);
    nsRepository::RegisterFactory(kXULDataSourceCID,          aPath, PR_TRUE, PR_TRUE);
    nsRepository::RegisterFactory(kXULDocumentCID,            aPath, PR_TRUE, PR_TRUE);
    return NS_OK;
}


extern "C" PR_IMPLEMENT(nsresult)
NSUnregisterSelf(const char* aPath)
{
    nsRepository::UnregisterFactory(kRDFBookmarkDataSourceCID,  aPath);
    nsRepository::UnregisterFactory(kRDFCompositeDataSourceCID, aPath);
    nsRepository::UnregisterFactory(kRDFContentSinkCID,         aPath);
    nsRepository::UnregisterFactory(kRDFHTMLBuilderCID,         aPath);
    nsRepository::UnregisterFactory(kRDFInMemoryDataSourceCID,  aPath);
    nsRepository::UnregisterFactory(kRDFServiceCID,             aPath);
    nsRepository::UnregisterFactory(kRDFTreeBuilderCID,         aPath);
    nsRepository::UnregisterFactory(kRDFXMLDataSourceCID,       aPath);
    nsRepository::UnregisterFactory(kRDFXULBuilderCID,          aPath);
    nsRepository::UnregisterFactory(kXULContentSinkCID,         aPath);
    nsRepository::UnregisterFactory(kXULDataSourceCID,          aPath);
    nsRepository::UnregisterFactory(kXULDocumentCID,            aPath);
    return NS_OK;
}

