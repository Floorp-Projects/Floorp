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
#include "nsIRDFContentModelBuilder.h"
#include "nsIRDFContentSink.h"
#include "nsIRDFDocument.h"
#include "nsIRDFService.h"
#include "nsISupports.h"
#include "nsRDFBaseDataSources.h"
#include "nsRDFBuiltInDataSources.h"
#include "nsRDFCID.h"

static NS_DEFINE_IID(kISupportsIID, NS_ISUPPORTS_IID);
static NS_DEFINE_IID(kIFactoryIID,  NS_IFACTORY_IID);

static NS_DEFINE_CID(kRDFBookmarkDataSourceCID, NS_RDFBOOKMARKDATASOURCE_CID);
static NS_DEFINE_CID(kRDFDataBaseCID,           NS_RDFDATABASE_CID);
static NS_DEFINE_CID(kRDFDocumentCID,           NS_RDFDOCUMENT_CID);
static NS_DEFINE_CID(kRDFHTMLBuilderCID,        NS_RDFHTMLBUILDER_CID);
static NS_DEFINE_CID(kRDFInMemoryDataSourceCID, NS_RDFINMEMORYDATASOURCE_CID);
static NS_DEFINE_CID(kRDFServiceCID,            NS_RDFSERVICE_CID);
static NS_DEFINE_CID(kRDFSimpleContentSinkCID,  NS_RDFSIMPLECONTENTSINK_CID);
static NS_DEFINE_CID(kRDFStreamDataSourceCID,   NS_RDFSTREAMDATASOURCE_CID);
static NS_DEFINE_CID(kRDFTreeBuilderCID,        NS_RDFTREEBUILDER_CID);

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
    else if (mClassID.Equals(kRDFStreamDataSourceCID)) {
        if (NS_FAILED(rv = NS_NewRDFStreamDataSource((nsIRDFDataSource**) &inst)))
          return rv;
    }
    else if (mClassID.Equals(kRDFBookmarkDataSourceCID)) {
        if (NS_FAILED(rv = NS_NewRDFBookmarkDataSource((nsIRDFDataSource**) &inst)))
            return rv;
    }
    else if (mClassID.Equals(kRDFDataBaseCID)) {
        if (NS_FAILED(rv = NS_NewRDFDataBase((nsIRDFDataBase**) &inst)))
            return rv;
    }
    else if (mClassID.Equals(kRDFDocumentCID)) {
        if (NS_FAILED(rv = NS_NewRDFDocument((nsIRDFDocument**) &inst)))
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
    else if (mClassID.Equals(kRDFSimpleContentSinkCID)) {
        if (NS_FAILED(rv = NS_NewRDFSimpleContentSink((nsIRDFContentSink**) &inst)))
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
NSGetFactory(const nsCID &aClass, nsIFactory **aFactory)
{
    if (! aFactory)
        return NS_ERROR_NULL_POINTER;

    *aFactory = new RDFFactoryImpl(aClass);
    if (aFactory == nsnull)
        return NS_ERROR_OUT_OF_MEMORY;

    return (*aFactory)->QueryInterface(kIFactoryIID, (void**)aFactory);
}

