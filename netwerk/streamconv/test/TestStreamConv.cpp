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
#include "nsIRegistry.h"
#include "nsIFactory.h"
#include "nsIStringStream.h"
#include "nsIIOService.h"
static NS_DEFINE_CID(kIOServiceCID, NS_IOSERVICE_CID);


#define ASYNC_TEST // undefine this if you want to test sycnronous conversion.

/////////////////////////////////
// Event pump setup
/////////////////////////////////
#include "nsIEventQueueService.h"
#ifdef WIN32 
#include <windows.h>
#endif
static int gKeepRunning = 0;
static nsIEventQueue* gEventQ = nsnull;
/////////////////////////////////
// Event pump END
/////////////////////////////////


/////////////////////////////////
// Test converters include
/////////////////////////////////
#include "Converters.h"
#include "nsMultiMixedConv.h"

// CID setup
static NS_DEFINE_CID(kEventQueueServiceCID,      NS_EVENTQUEUESERVICE_CID);
static NS_DEFINE_CID(kStreamConverterServiceCID, NS_STREAMCONVERTERSERVICE_CID);
static NS_DEFINE_CID(kComponentManagerCID,       NS_COMPONENTMANAGER_CID);
static NS_DEFINE_CID(kRegistryCID,               NS_REGISTRY_CID);

////////////////////////////////////////////////////////////////////////
// EndListener - This listener is the final one in the chain. It
//   receives the fully converted data, although it doesn't do anything with
//   the data.
////////////////////////////////////////////////////////////////////////
class EndListener : public nsIStreamListener {
public:
    // nsISupports declaration
    NS_DECL_ISUPPORTS

    EndListener() {NS_INIT_ISUPPORTS();};

    // nsIStreamListener method
    NS_IMETHOD OnDataAvailable(nsIChannel *channel, nsISupports *ctxt, nsIInputStream *inStr, 
                               PRUint32 sourceOffset, PRUint32 count)
    {
        nsresult rv;
        PRUint32 read, len;
        rv = inStr->GetLength(&len);
        if (NS_FAILED(rv)) return rv;

        char *buffer = (char*)nsAllocator::Alloc(len + 1);
        if (!buffer) return NS_ERROR_OUT_OF_MEMORY;

        rv = inStr->Read(buffer, len, &read);
        buffer[len] = '\0';
        if (NS_FAILED(rv)) return rv;

        printf("CONTEXT %x: Received %d bytes and the following data: \n %s\n\n", ctxt, read, buffer);

        nsAllocator::Free(buffer);

        return NS_OK;
    }

    // nsIStreamObserver methods
    NS_IMETHOD OnStartRequest(nsIChannel *channel, nsISupports *ctxt) 
    {
        return NS_OK;
    }

    NS_IMETHOD OnStopRequest(nsIChannel *channel, nsISupports *ctxt, nsresult status, const PRUnichar *errorMsg)
    {
        return NS_OK;
    }
};

NS_IMPL_ISUPPORTS(EndListener, NS_GET_IID(nsIStreamListener));
////////////////////////////////////////////////////////////////////////
// EndListener END
////////////////////////////////////////////////////////////////////////


int
main(int argc, char* argv[])
{
    nsresult rv;

    // Create the Event Queue for this thread...
    NS_WITH_SERVICE(nsIEventQueueService, eventQService, kEventQueueServiceCID, &rv);
    if (NS_FAILED(rv)) return rv;

    rv = eventQService->CreateThreadEventQueue();
    if (NS_FAILED(rv)) return rv;

    eventQService->GetThreadEventQueue(PR_CurrentThread(), &gEventQ);

    rv = nsComponentManager::AutoRegister(nsIComponentManager::NS_Startup, NULL /* default */);
    if (NS_FAILED(rv)) return rv;

    ///////////////////////////////////////////
    // BEGIN - Stream converter registration
    //   All stream converters must register with the ComponentManager, _and_ make
    //   a registry entry.
    ///////////////////////////////////////////
    TestConverterFactory *convFactory = new TestConverterFactory(kTestConverterCID, "TestConverter", NS_ISTREAMCONVERTER_KEY);
    nsIFactory *convFactSup = nsnull;
    rv = convFactory->QueryInterface(NS_GET_IID(nsIFactory), (void**)&convFactSup);
    if (NS_FAILED(rv)) return rv;

    TestConverterFactory *convFactory1 = new TestConverterFactory(kTestConverter1CID, "TestConverter1", NS_ISTREAMCONVERTER_KEY);
    nsIFactory *convFactSup1 = nsnull;
    rv = convFactory1->QueryInterface(NS_GET_IID(nsIFactory), (void**)&convFactSup1);
    if (NS_FAILED(rv)) return rv;

    // multipart mixed converter registration
    nsIFactory *multiMixedFactSup = nsnull;
    rv = nsComponentManager::FindFactory(kMultiMixedConverterCID, &multiMixedFactSup);
    if (NS_FAILED(rv)) return rv;

    // register the TestConverter with the component manager. One progid registration
    // per conversion pair (from - to pair).
    rv = nsComponentManager::RegisterFactory(kTestConverterCID,
                                             "TestConverter",
                                             NS_ISTREAMCONVERTER_KEY "?from=a/foo?to=b/foo",
                                             convFactSup,
                                             PR_TRUE);
    if (NS_FAILED(rv)) return rv;

    rv = nsComponentManager::RegisterFactory(kTestConverter1CID,
                                             "TestConverter1",
                                             NS_ISTREAMCONVERTER_KEY "?from=b/foo?to=c/foo",
                                             convFactSup1,
                                             PR_TRUE);
    if (NS_FAILED(rv)) return rv;

    rv = nsComponentManager::RegisterFactory(kMultiMixedConverterCID,
                                             "MultiMixedConverter",
                                             NS_ISTREAMCONVERTER_KEY "?from=multipart/x-mixed-replace?to=text/html",
                                             multiMixedFactSup,
                                             PR_TRUE);
    if (NS_FAILED(rv)) return rv;

    rv = nsComponentManager::RegisterFactory(kTestConverterCID,
                                             "TestConverter",
                                             NS_ISTREAMCONVERTER_KEY "?from=b/foo?to=d/foo",
                                             convFactSup,
                                             PR_TRUE);
    if (NS_FAILED(rv)) return rv;

    rv = nsComponentManager::RegisterFactory(kTestConverterCID,
                                             "TestConverter",
                                             NS_ISTREAMCONVERTER_KEY "?from=c/foo?to=d/foo",
                                             convFactSup,
                                             PR_TRUE);
    if (NS_FAILED(rv)) return rv;

    rv = nsComponentManager::RegisterFactory(kTestConverterCID,
                                             "TestConverter",
                                             NS_ISTREAMCONVERTER_KEY "?from=d/foo?to=e/foo",
                                             convFactSup,
                                             PR_TRUE);
    if (NS_FAILED(rv)) return rv;

    rv = nsComponentManager::RegisterFactory(kTestConverterCID,
                                             "TestConverter",
                                             NS_ISTREAMCONVERTER_KEY "?from=d/foo?to=f/foo",
                                             convFactSup,
                                             PR_TRUE);
    if (NS_FAILED(rv)) return rv;

    rv = nsComponentManager::RegisterFactory(kTestConverterCID,
                                             "TestConverter",
                                             NS_ISTREAMCONVERTER_KEY "?from=t/foo?to=k/foo",
                                             convFactSup,
                                             PR_TRUE);
    if (NS_FAILED(rv)) return rv;
    
    // register the converters with the registry. One progid registration
    // per conversion pair.
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

    rv = registry->AddSubtreeRaw(key, "?from=multipart/x-mixed-replace?to=text/html", &key1);
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

    NS_WITH_SERVICE(nsIStreamConverterService, StreamConvService, kStreamConverterServiceCID, &rv);
    if (NS_FAILED(rv)) return rv;

    // use a dummy string as a stream to convert. This is the original from data.
    nsString2 dummyData("--aBoundary Test ");
    nsIInputStream *inputData = nsnull;
    nsISupports *inputDataSup = nsnull;
    nsIInputStream *convertedData = nsnull;

    rv = NS_NewStringInputStream(&inputDataSup, dummyData);
    if (NS_FAILED(rv)) return rv;

    PRUnichar *from, *to;
    nsString2 fromStr("multipart/x-mixed-replace");
    from = fromStr.ToNewUnicode();
    nsString2 toStr  ("text/html");
    to = toStr.ToNewUnicode();

    rv = inputDataSup->QueryInterface(NS_GET_IID(nsIInputStream), (void**)&inputData);
    NS_RELEASE(inputDataSup);
    if (NS_FAILED(rv)) return rv;

#ifdef ASYNC_TEST
    // ASYNCRONOUS conversion

    NS_WITH_SERVICE(nsIIOService, serv, kIOServiceCID, &rv);
    if (NS_FAILED(rv)) return rv;

    // we need a dummy channel for the async calls.
    nsIChannel *dummyChannel = nsnull;
    nsIURI *dummyURI = nsnull;
    rv = serv->NewURI("http://neverneverland.com", nsnull, &dummyURI);
    if (NS_FAILED(rv)) return rv;
    rv = serv->NewInputStreamChannel(dummyURI, "multipart/x-mixed-replace;boundary= --aBoundary",
                                     nsnull, &dummyChannel);
    NS_RELEASE(dummyURI);
    if (NS_FAILED(rv)) return rv;


    // setup a listener to receive the converted data. This guy is the end
    // listener in the chain, he wants the fully converted (toType) data.
    // An example of this listener in mozilla would be the DocLoader.
    nsIStreamListener *dataReceiver = new EndListener();

    // setup a listener to push the data into. This listener sits inbetween the
    // unconverted data of fromType, and the final listener in the chain (in this case
    // the dataReceiver.
    nsIStreamListener *converterListener = nsnull;
    rv = StreamConvService->AsyncConvertData(from, to, dataReceiver, &converterListener);
    if (NS_FAILED(rv)) return rv;

    // at this point we have a stream listener to push data to, and the one
    // that will receive the converted data. Let's mimic On*() calls and get the conversion
    // going. Typically these On*() calls would be made inside their respective wrappers On*()
    // methods.
    rv = converterListener->OnStartRequest(dummyChannel, nsnull);
    if (NS_FAILED(rv)) return rv;

    // For the OnDataAvailable() call, we'll simulate multiple (two) calls.
    rv = converterListener->OnDataAvailable(dummyChannel, nsnull, inputData, 0, -1);
    NS_RELEASE(inputData);
    if (NS_FAILED(rv)) return rv;

    // And the second...
    nsString2 dummyData2("--aBoundary me. --aBoundary--");
    nsIInputStream *inputData2 = nsnull;
    nsISupports *inputDataSup2 = nsnull;

    rv = NS_NewStringInputStream(&inputDataSup2, dummyData2);
    if (NS_FAILED(rv)) return rv;

    rv = inputDataSup2->QueryInterface(NS_GET_IID(nsIInputStream), (void**)&inputData2);
    NS_RELEASE(inputDataSup2);
    if (NS_FAILED(rv)) return rv;

    rv = converterListener->OnDataAvailable(dummyChannel, nsnull, inputData2, 0, -1);
    NS_RELEASE(inputData2);
    if (NS_FAILED(rv)) return rv;


    rv = converterListener->OnStopRequest(dummyChannel, nsnull, rv, nsnull);
    if (NS_FAILED(rv)) return rv;

    NS_RELEASE(dummyChannel);

#else
    // SYNCRONOUS conversion
    rv = StreamConvService->Convert(inputData, from, to, &convertedData);
#endif

    NS_RELEASE(convFactSup);
    nsAllocator::Free(from);
    nsAllocator::Free(to);

    if (NS_FAILED(rv)) return rv;

    // Enter the message pump to allow the URL load to proceed.
    while ( gKeepRunning ) {
#ifdef WIN32
        MSG msg;

        if (GetMessage(&msg, NULL, 0, 0)) {
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        } else {
            gKeepRunning = 0;
    }
#else
#ifdef XP_MAC
    /* Mac stuff is missing here! */
#else
    PLEvent *gEvent;
    rv = gEventQ->GetEvent(&gEvent);
    rv = gEventQ->HandleEvent(gEvent);
#endif /* XP_UNIX */
#endif /* !WIN32 */
    }

    return rv;
}
