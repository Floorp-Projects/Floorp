/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 4 -*-
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

#include "nsIServiceManager.h"
#include "nsIComponentManager.h"
#include "nsIStreamConverterService.h"
#include "nsIStreamConverter.h"
#include "nsIStringStream.h"
#include "nsIRegistry.h"
#include "nsIFactory.h"

static NS_DEFINE_CID(kStreamConverterServiceCID, NS_STREAMCONVERTERSERVICE_CID);
static NS_DEFINE_CID(kComponentManagerCID, NS_COMPONENTMANAGER_CID);
static NS_DEFINE_CID(kRegistryCID, NS_REGISTRY_CID);

#define NS_TESTCONVERTER_CID                             \
{ /* B8A067B0-4450-11d3-A16E-0050041CAF44 */         \
    0xb8a067b0,                                      \
    0x4450,                                          \
    0x11d3,                                          \
    {0xa1, 0x6e, 0x00, 0x50, 0x04, 0x1c, 0xaf, 0x44} \
}

static NS_DEFINE_CID(kTestConverterCID,          NS_TESTCONVERTER_CID);
static NS_DEFINE_IID(kIFactoryIID,         NS_IFACTORY_IID);



class TestConverter : public nsIStreamConverter {
public:
    NS_DECL_ISUPPORTS

    TestConverter() { NS_INIT_REFCNT();};

    // nsIStreamConverter methods
    NS_IMETHOD Convert(nsIInputStream *aFromStream, const PRUnichar *aFromType, const PRUnichar *aToType, nsISupports *ctxt, nsIInputStream **_retval)
    { 
        char buf[1024];
        PRUint32 read;
        nsresult rv = aFromStream->Read(buf, 1024, &read);
        if (NS_FAILED(rv)) return rv;

        nsString2 to(aToType);
        char *toMIME = to.ToNewCString();
        char toChar = *toMIME;

        for (PRUint32 i = 0; i < read; i++) 
            buf[i] = toChar;

        nsString2 convDataStr(buf);
        nsIInputStream *inputData = nsnull;
        nsISupports *inputDataSup = nsnull;

        rv = NS_NewStringInputStream(&inputDataSup, convDataStr);
        if (NS_FAILED(rv)) return rv;

        rv = inputDataSup->QueryInterface(nsCOMTypeInfo<nsIInputStream>::GetIID(), (void**)&inputData);
        NS_RELEASE(inputDataSup);
        *_retval = inputData;
        if (NS_FAILED(rv)) return rv;

        return NS_OK; 
    };

    NS_IMETHOD AsyncConvertData(const PRUnichar *aFromType, const PRUnichar *aToType, nsIStreamListener *aListener, nsISupports *ctxt)
    { return NS_OK; };

    // nsIStreamListener method
    NS_IMETHOD OnDataAvailable(nsIChannel *channel, nsISupports *ctxt, nsIInputStream *inStr, PRUint32 sourceOffset, PRUint32 count)
    { return NS_OK; };

    // nsIStreamObserver methods
    NS_IMETHOD OnStartRequest(nsIChannel *channel, nsISupports *ctxt) 
    { return NS_OK; };

    NS_IMETHOD OnStopRequest(nsIChannel *channel, nsISupports *ctxt, nsresult status, const PRUnichar *errorMsg)
    { return NS_OK; };

};

NS_IMPL_ISUPPORTS(TestConverter,nsCOMTypeInfo<nsIStreamConverter>::GetIID());

class TestConverterFactory : public nsIFactory
{
public:
    TestConverterFactory(const nsCID &aClass, const char* className, const char* progID);

    // nsISupports methods
    NS_DECL_ISUPPORTS

    // nsIFactory methods
    NS_IMETHOD CreateInstance(nsISupports *aOuter,
                              const nsIID &aIID,
                              void **aResult);

    NS_IMETHOD LockFactory(PRBool aLock);

protected:
    virtual ~TestConverterFactory();

protected:
    nsCID       mClassID;
    const char* mClassName;
    const char* mProgID;
};


////////////////////////////////////////////////////////////////////////

TestConverterFactory::TestConverterFactory(const nsCID &aClass, 
                                   const char* className,
                                   const char* progID)
    : mClassID(aClass), mClassName(className), mProgID(progID)
{
    NS_INIT_REFCNT();
}

TestConverterFactory::~TestConverterFactory()
{
    NS_ASSERTION(mRefCnt == 0, "non-zero refcnt at destruction");
}

NS_IMETHODIMP
TestConverterFactory::QueryInterface(const nsIID &aIID, void **aResult)
{
    if (! aResult)
        return NS_ERROR_NULL_POINTER;

    // Always NULL result, in case of failure
    *aResult = nsnull;

    if (aIID.Equals(nsCOMTypeInfo<nsISupports>::GetIID())) {
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

NS_IMPL_ADDREF(TestConverterFactory);
NS_IMPL_RELEASE(TestConverterFactory);

NS_IMETHODIMP
TestConverterFactory::CreateInstance(nsISupports *aOuter,
                                 const nsIID &aIID,
                                 void **aResult)
{
    if (! aResult)
        return NS_ERROR_NULL_POINTER;

    if (aOuter)
        return NS_ERROR_NO_AGGREGATION;

    *aResult = nsnull;

    nsresult rv = NS_OK;

    nsISupports *inst = nsnull;
    if (mClassID.Equals(kTestConverterCID)) {
        TestConverter *conv = new TestConverter();
        if (!conv) return NS_ERROR_OUT_OF_MEMORY;
        conv->QueryInterface(nsCOMTypeInfo<nsISupports>::GetIID(), (void**)&inst);
    }
    else {
        return NS_ERROR_NO_INTERFACE;
    }

    if (!inst)
        return NS_ERROR_OUT_OF_MEMORY;
    NS_ADDREF(inst);
    *aResult = inst;
    NS_RELEASE(inst);
    return rv;
}

nsresult TestConverterFactory::LockFactory(PRBool aLock)
{
    // Not implemented in simplest case.
    return NS_OK;
}

nsresult NS_AutoregisterComponents()
{
  nsresult rv = nsComponentManager::AutoRegister(nsIComponentManager::NS_Startup, NULL /* default */);
  return rv;
}

int
main(int argc, char* argv[])
{
    nsresult rv;
    /* 
      The following code only deals with XPCOM registration stuff. and setting
      up the event queues. Copied from TestSocketIO.cpp
    */

    rv = NS_AutoregisterComponents();
    if (NS_FAILED(rv)) return rv;

    TestConverterFactory *convFactory = new TestConverterFactory(kTestConverterCID, "TestConverter", NS_ISTREAMCONVERTER_KEY);
    nsIFactory *convFactSup = nsnull;
    rv = convFactory->QueryInterface(nsCOMTypeInfo<nsIFactory>::GetIID(), (void**)&convFactSup);
    if (NS_FAILED(rv)) return rv;

    // register the TestConverter with the component manager. One progid registration
    // per conversion ability.
    rv = nsComponentManager::RegisterFactory(kTestConverterCID,
                                             "TestConverter",
                                             NS_ISTREAMCONVERTER_KEY "?from=a/foo?to=b/foo",
                                             convFactSup,
                                             PR_TRUE);

    rv = nsComponentManager::RegisterFactory(kTestConverterCID,
                                             "TestConverter",
                                             NS_ISTREAMCONVERTER_KEY "?from=b/foo?to=c/foo",
                                             convFactSup,
                                             PR_TRUE);
    rv = nsComponentManager::RegisterFactory(kTestConverterCID,
                                             "TestConverter",
                                             NS_ISTREAMCONVERTER_KEY "?from=b/foo?to=d/foo",
                                             convFactSup,
                                             PR_TRUE);
    rv = nsComponentManager::RegisterFactory(kTestConverterCID,
                                             "TestConverter",
                                             NS_ISTREAMCONVERTER_KEY "?from=c/foo?to=d/foo",
                                             convFactSup,
                                             PR_TRUE);

    rv = nsComponentManager::RegisterFactory(kTestConverterCID,
                                             "TestConverter",
                                             NS_ISTREAMCONVERTER_KEY "?from=d/foo?to=e/foo",
                                             convFactSup,
                                             PR_TRUE);
    rv = nsComponentManager::RegisterFactory(kTestConverterCID,
                                             "TestConverter",
                                             NS_ISTREAMCONVERTER_KEY "?from=d/foo?to=f/foo",
                                             convFactSup,
                                             PR_TRUE);
    rv = nsComponentManager::RegisterFactory(kTestConverterCID,
                                             "TestConverter",
                                             NS_ISTREAMCONVERTER_KEY "?from=t/foo?to=k/foo",
                                             convFactSup,
                                             PR_TRUE);
    
    if (NS_FAILED(rv)) return rv;

    
    // register the TestConverter with the registry. One progid registration
    // per conversion ability.
    NS_WITH_SERVICE(nsIRegistry, registry, kRegistryCID, &rv);
    if (NS_FAILED(rv)) return rv;

    // open the registry
    rv = registry->OpenWellKnownRegistry(nsIRegistry::ApplicationComponentRegistry);
    if (NS_FAILED(rv)) return rv;

    // set the key
    nsIRegistry::Key key, key1;

    rv = registry->AddSubtree(nsIRegistry::Common, NS_ISTREAMCONVERTER_KEY, &key);
    if (NS_FAILED(rv)) return rv;
    rv = registry->AddSubtreeRaw(key, "?from=a/foo?to=b/foo", &key1);
    if (NS_FAILED(rv)) return rv;

    rv = registry->AddSubtreeRaw(key, "?from=b/foo?to=c/foo", &key1);
    if (NS_FAILED(rv)) return rv;

    rv = registry->AddSubtreeRaw(key, "?from=b/foo?to=d/foo", &key1);
    if (NS_FAILED(rv)) return rv;

    rv = registry->AddSubtreeRaw(key, "?from=c/foo?to=d/foo", &key1);
    if (NS_FAILED(rv)) return rv;

    rv = registry->AddSubtreeRaw(key, "?from=d/foo?to=e/foo", &key1);
    if (NS_FAILED(rv)) return rv;

    rv = registry->AddSubtreeRaw(key, "?from=d/foo?to=f/foo", &key1);
    if (NS_FAILED(rv)) return rv;

    rv = registry->AddSubtreeRaw(key, "?from=t/foo?to=k/foo", &key1);
    if (NS_FAILED(rv)) return rv;


    // get the converter service and ask it to convert
    NS_WITH_SERVICE(nsIStreamConverterService, StreamConvService, kStreamConverterServiceCID, &rv);
    if (NS_FAILED(rv)) return rv;

    // use a dummy string as a stream to convert.
    nsString2 dummyData("aaaaaaaaaaaaaaa");
    nsIInputStream *inputData = nsnull;
    nsISupports *inputDataSup = nsnull;
    nsIInputStream *convertedData = nsnull;

    rv = NS_NewStringInputStream(&inputDataSup, dummyData);
    if (NS_FAILED(rv)) return rv;

    PRUnichar *from, *to;
    nsString2 fromStr("a/foo");
    from = fromStr.ToNewUnicode();
    nsString2 toStr  ("e/foo");
    to = toStr.ToNewUnicode();

    rv = inputDataSup->QueryInterface(nsCOMTypeInfo<nsIInputStream>::GetIID(), (void**)&inputData);
    if (NS_FAILED(rv)) return rv;


    rv = StreamConvService->Convert(inputData, from, to, &convertedData);
    NS_RELEASE(convFactSup);
    nsServiceManager::ReleaseService(kStreamConverterServiceCID, StreamConvService, nsnull);

    if (NS_FAILED(rv)) return rv;

    return rv;
}
