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

#include "nsISupports.h"
#include "nsIFactory.h"
#include "nsRDFBuiltInDataSources.h"
#include "nsRDFResourceManager.h"
#include "nsRDFDocument.h"
#include "nsRDFRegistryImpl.h"
#include "nsRDFCID.h"

static NS_DEFINE_IID(kISupportsIID, NS_ISUPPORTS_IID);
static NS_DEFINE_IID(kIFactoryIID,  NS_IFACTORY_IID);

static NS_DEFINE_CID(kRDFBookmarkDataSourceCID, NS_RDFBOOKMARKDATASOURCE_CID);
static NS_DEFINE_CID(kRDFMemoryDataSourceCID,   NS_RDFMEMORYDATASOURCE_CID);
static NS_DEFINE_CID(kRDFRegistryCID,           NS_RDFREGISTRY_CID);
static NS_DEFINE_CID(kRDFResourceManagerCID,    NS_RDFRESOURCEMANAGER_CID);
static NS_DEFINE_CID(kRDFSimpleDataBaseCID,     NS_RDFSIMPLEDATABASE_CID);
static NS_DEFINE_CID(kRDFHTMLDocumentCID,       NS_RDFHTMLDOCUMENT_CID);
static NS_DEFINE_CID(kRDFTreeDocumentCID,       NS_RDFTREEDOCUMENT_CID);

class nsRDFFactory : public nsIFactory
{
public:
    nsRDFFactory(const nsCID &aClass);

    // nsISupports methods
    NS_DECL_ISUPPORTS

    // nsIFactory methods
    NS_IMETHOD CreateInstance(nsISupports *aOuter,
                              const nsIID &aIID,
                              void **aResult);

    NS_IMETHOD LockFactory(PRBool aLock);

protected:
    virtual ~nsRDFFactory();

private:
    nsCID     mClassID;
};

////////////////////////////////////////////////////////////////////////

nsRDFFactory::nsRDFFactory(const nsCID &aClass)
{
    NS_INIT_REFCNT();
    mClassID = aClass;
}

nsRDFFactory::~nsRDFFactory()
{
    NS_ASSERTION(mRefCnt == 0, "non-zero refcnt at destruction");
}

NS_IMETHODIMP
nsRDFFactory::QueryInterface(const nsIID &aIID,
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

NS_IMPL_ADDREF(nsRDFFactory);
NS_IMPL_RELEASE(nsRDFFactory);


NS_IMETHODIMP
nsRDFFactory::CreateInstance(nsISupports *aOuter,
                             const nsIID &aIID,
                             void **aResult)
{
    if (! aResult)
        return NS_ERROR_NULL_POINTER;

    if (aOuter)
        return NS_ERROR_NO_AGGREGATION;

    *aResult = nsnull;

    nsresult rv;
    PRBool wasRefCounted = PR_FALSE;
    nsISupports *inst = nsnull;
    if (mClassID.Equals(kRDFResourceManagerCID)) {
        inst = NS_STATIC_CAST(nsISupports*, new nsRDFResourceManager());
    }
    else if (mClassID.Equals(kRDFMemoryDataSourceCID)) {
        if (NS_FAILED(rv = NS_NewRDFMemoryDataSource((nsIRDFDataSource**) &inst)))
            return rv;

        wasRefCounted = PR_TRUE;
    }
    else if (mClassID.Equals(kRDFBookmarkDataSourceCID)) {
        if (NS_FAILED(rv = NS_NewRDFBookmarkDataSource((nsIRDFDataSource**) &inst)))
            return rv;

        wasRefCounted = PR_TRUE;
    }
    else if (mClassID.Equals(kRDFRegistryCID)) {
        inst = NS_STATIC_CAST(nsISupports*, new nsRDFRegistryImpl());
    }
    else if (mClassID.Equals(kRDFSimpleDataBaseCID)) {
        if (NS_FAILED(rv = NS_NewRDFSimpleDataBase((nsIRDFDataBase**) &inst)))
            return rv;

        wasRefCounted = PR_TRUE;
    }
    else if (mClassID.Equals(kRDFHTMLDocumentCID)) {
        if (NS_FAILED(rv = NS_NewRDFHTMLDocument((nsIRDFDocument**) &inst)))
            return rv;

        wasRefCounted = PR_TRUE;
    }
    else if (mClassID.Equals(kRDFTreeDocumentCID)) {
        if (NS_FAILED(rv = NS_NewRDFTreeDocument((nsIRDFDocument**) &inst)))
            return rv;

        wasRefCounted = PR_TRUE;
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

nsresult nsRDFFactory::LockFactory(PRBool aLock)
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

    *aFactory = new nsRDFFactory(aClass);
    if (aFactory == nsnull)
        return NS_ERROR_OUT_OF_MEMORY;

    return (*aFactory)->QueryInterface(kIFactoryIID, (void**)aFactory);
}

