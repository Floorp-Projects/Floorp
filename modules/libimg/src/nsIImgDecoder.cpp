/* -*- Mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*-
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
#include "nsImgDecCID.h"


static NS_DEFINE_IID(kISupportsIID, NS_ISUPPORTS_IID);
static NS_DEFINE_IID(kIFactoryIID,  NS_IFACTORY_IID);


NS_DEFINE_CID(kGIFDecoderCID, NS_IMGDECODER_CID);
NS_DEFINE_CID(kJPGDecoderCID, NS_IMGDECODER_CID);

class ImgFactoryImpl : public nsIFactory
{
public:
    ImgFactoryImpl(const nsCID &aClass, const char* className, const char* progID);

    NS_DECL_ISUPPORTS

    NS_IMETHOD CreateInstance(nsISupports *aOuter,
                              const nsIID &aIID,
                              void **aResult);

    NS_IMETHOD LockFactory(PRBool aLock);

protected:
    virtual ~ImgFactoryImpl();

protected:
    nsCID       mClassID;
    const char* mClassName;
    const char* mProgID;
};

/*---------------------*/

ImgFactoryImpl::ImgFactoryImpl(const nsCID &aClass, 
                               const char* className,
                               const char* progID)
    : mClassID(aClass), mClassName(className), mProgID(progID)
{
    NS_INIT_REFCNT();
}

ImgFactoryImpl::~ImgFactoryImpl()
{
    NS_ASSERTION(mRefCnt == 0, "non-zero refcnt at destruction");
}

NS_IMETHODIMP
ImgFactoryImpl::QueryInterface(const nsIID &aIID, void **aResult)
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

NS_IMPL_ADDREF(ImgFactoryImpl);
NS_IMPL_RELEASE(ImgFactoryImpl);

extern nsresult
NS_NewDefaultResource(nsIRDFResource** aResult);

NS_IMETHODIMP
ImgFactoryImpl::CreateInstance(nsISupports *aOuter,
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

    if (mClassID.Equals(kGIFDecoderCID)) {
        if (NS_FAILED(rv = NS_NewGIFDecoder((nsIGIFDecoder**) &inst)))
            return rv;
    }
    if (mClassID.Equals(kJPEGDecoderCID)) {
        if (NS_FAILED(rv = NS_NewJPGDecoder((nsIJPGDecoder**) &inst)))
            return rv;
    }
    else {
        return NS_ERROR_NO_INTERFACE;
    }

    if (! inst)
        return NS_ERROR_OUT_OF_MEMORY;

    if (NS_FAILED(rv = inst->QueryInterface(aIID, aResult)))
        delete inst;

    if (wasRefCounted)
        NS_IF_RELEASE(inst);

    return rv;
}

nsresult ImgFactoryImpl::LockFactory(PRBool aLock)
{
    return NS_OK;
}

/*---------------------*/


/* return the proper factory to the caller */
extern "C" PR_IMPLEMENT(nsresult)
NSGetFactory(nsISupports* serviceMgr,
             const nsCID &aClass,
             const char *aClassName,
             const char *aProgID,
             nsIFactory **aFactory)
{
    if (! aFactory)
        return NS_ERROR_NULL_POINTER;

    ImgFactoryImpl* factory = new ImgFactoryImpl(aClass, aClassName, aProgID);
    if (factory == nsnull)
        return NS_ERROR_OUT_OF_MEMORY;

    NS_ADDREF(factory);
    *aFactory = factory;
    return NS_OK;
}
