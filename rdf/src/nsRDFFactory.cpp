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
#include "nsRDFTreeDataModel.h"
#include "nsRDFToolbarDataModel.h"
#include "nsRDFCIDs.h"

static NS_DEFINE_IID(kISupportsIID, NS_ISUPPORTS_IID);
static NS_DEFINE_IID(kIFactoryIID, NS_IFACTORY_IID);
static NS_DEFINE_CID(kRDFTreeDataModelCID, NS_RDFTREEDATAMODEL_CID);
static NS_DEFINE_CID(kRDFToolbarDataModelCID, NS_RDFTOOLBARDATAMODEL_CID);

class nsRDFFactory : public nsIFactory
{
public:
    nsRDFFactory(const nsCID &aClass);

    ////////////////////////////////////////
    // nsISupports methods
    //

    NS_DECL_ISUPPORTS

    ////////////////////////////////////////
    // nsIFactory methods
    //
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
    if (aResult)
        return NS_ERROR_NULL_POINTER;

    // Always NULL result, in case of failure
    *aResult = NULL;

    if (aIID.Equals(kISupportsIID)) {
        *aResult = static_cast<nsISupports*>(this);
        AddRef();
        return NS_OK;
    } else if (aIID.Equals(kIFactoryIID)) {
        *aResult = static_cast<nsIFactory*>(this);
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

    *aResult = NULL;

    nsISupports *inst = NULL;
    if (mClassID.Equals(kRDFTreeDataModelCID)) {
        inst = static_cast<nsITreeDataModel*>(new nsRDFTreeDataModel());
    }
    else if (mClassID.Equals(kRDFToolbarDataModelCID)) {
#if 0
        inst = static_cast<nsIToolbarDataModel*>(new nsRDFToolbarDataModel());
#endif
    }

    if (! inst)
        return NS_ERROR_OUT_OF_MEMORY;

    nsresult res = inst->QueryInterface(aIID, aResult);
    if (NS_FAILED(res))
        // We didn't get the right interface, so clean up
        delete inst;

    return res;
}

nsresult nsRDFFactory::LockFactory(PRBool aLock)
{
    // Not implemented in simplest case.
    return NS_OK;
}

// return the proper factory to the caller
extern "C" nsresult
NSGetFactory(const nsCID &aClass, nsIFactory **aFactory)
{
    if (! aFactory)
        return NS_ERROR_NULL_POINTER;

    *aFactory = new nsRDFFactory(aClass);

    if (nsnull == aFactory) {
        return NS_ERROR_OUT_OF_MEMORY;
    }

    return (*aFactory)->QueryInterface(kIFactoryIID, (void**)aFactory);
}
