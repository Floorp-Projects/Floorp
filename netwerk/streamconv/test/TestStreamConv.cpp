/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsIServiceManager.h"
#include "nsIComponentManager.h"
#include "nsIComponentRegistrar.h"
#include "nsIStreamConverterService.h"
#include "nsIStreamConverter.h"
#include "nsICategoryManager.h"
#include "mozilla/Module.h"
#include "nsXULAppAPI.h"
#include "nsIStringStream.h"
#include "nsCOMPtr.h"
#include "nsNetUtil.h"
#include "nsThreadUtils.h"

#include "nspr.h"

#define ASYNC_TEST // undefine this if you want to test sycnronous conversion.

/////////////////////////////////
// Event pump setup
/////////////////////////////////
#ifdef XP_WIN
#include <windows.h>
#endif
#ifdef XP_OS2
#include <os2.h>
#endif

static int gKeepRunning = 0;
/////////////////////////////////
// Event pump END
/////////////////////////////////


/////////////////////////////////
// Test converters include
/////////////////////////////////
#include "Converters.h"

// CID setup
static NS_DEFINE_CID(kStreamConverterServiceCID, NS_STREAMCONVERTERSERVICE_CID);

////////////////////////////////////////////////////////////////////////
// EndListener - This listener is the final one in the chain. It
//   receives the fully converted data, although it doesn't do anything with
//   the data.
////////////////////////////////////////////////////////////////////////
class EndListener : public nsIStreamListener {
public:
    // nsISupports declaration
    NS_DECL_ISUPPORTS

    EndListener() {};

    // nsIStreamListener method
    NS_IMETHOD OnDataAvailable(nsIRequest* request, nsISupports *ctxt, nsIInputStream *inStr, 
                               PRUint32 sourceOffset, PRUint32 count)
    {
        nsresult rv;
        PRUint32 read, len;
        rv = inStr->Available(&len);
        if (NS_FAILED(rv)) return rv;

        char *buffer = (char*)nsMemory::Alloc(len + 1);
        if (!buffer) return NS_ERROR_OUT_OF_MEMORY;

        rv = inStr->Read(buffer, len, &read);
        buffer[len] = '\0';
        if (NS_SUCCEEDED(rv)) {
            printf("CONTEXT %p: Received %u bytes and the following data: \n %s\n\n",
                   static_cast<void*>(ctxt), read, buffer);
        }
        nsMemory::Free(buffer);

        return NS_OK;
    }

    // nsIRequestObserver methods
    NS_IMETHOD OnStartRequest(nsIRequest* request, nsISupports *ctxt) { return NS_OK; }

    NS_IMETHOD OnStopRequest(nsIRequest* request, nsISupports *ctxt, 
                             nsresult aStatus) { return NS_OK; }
};

NS_IMPL_ISUPPORTS2(EndListener,
                   nsIStreamListener,
                   nsIRequestObserver)

////////////////////////////////////////////////////////////////////////
// EndListener END
////////////////////////////////////////////////////////////////////////


nsresult SendData(const char * aData, nsIStreamListener* aListener, nsIRequest* request) {
    nsresult rv;

    nsCOMPtr<nsIStringInputStream> dataStream
      (do_CreateInstance("@mozilla.org/io/string-input-stream;1", &rv));
    NS_ENSURE_SUCCESS(rv, rv);

    rv = dataStream->SetData(aData, strlen(aData));
    NS_ENSURE_SUCCESS(rv, rv);

    PRUint32 avail;
    dataStream->Available(&avail);

    return aListener->OnDataAvailable(request, nsnull, dataStream, 0, avail);
}
#define SEND_DATA(x) SendData(x, converterListener, request)

static const mozilla::Module::CIDEntry kTestCIDs[] = {
    { &kTestConverterCID, false, NULL, CreateTestConverter },
    { NULL }
};

static const mozilla::Module::ContractIDEntry kTestContracts[] = {
    { NS_ISTREAMCONVERTER_KEY "?from=a/foo&to=b/foo", &kTestConverterCID },
    { NS_ISTREAMCONVERTER_KEY "?from=b/foo&to=c/foo", &kTestConverterCID },
    { NS_ISTREAMCONVERTER_KEY "?from=b/foo&to=d/foo", &kTestConverterCID },
    { NS_ISTREAMCONVERTER_KEY "?from=c/foo&to=d/foo", &kTestConverterCID },
    { NS_ISTREAMCONVERTER_KEY "?from=d/foo&to=e/foo", &kTestConverterCID },
    { NS_ISTREAMCONVERTER_KEY "?from=d/foo&to=f/foo", &kTestConverterCID },
    { NS_ISTREAMCONVERTER_KEY "?from=t/foo&to=k/foo", &kTestConverterCID },
    { NULL }
};

static const mozilla::Module::CategoryEntry kTestCategories[] = {
    { NS_ISTREAMCONVERTER_KEY, "?from=a/foo&to=b/foo", "x" },
    { NS_ISTREAMCONVERTER_KEY, "?from=b/foo&to=c/foo", "x" },
    { NS_ISTREAMCONVERTER_KEY, "?from=b/foo&to=d/foo", "x" },
    { NS_ISTREAMCONVERTER_KEY, "?from=c/foo&to=d/foo", "x" },
    { NS_ISTREAMCONVERTER_KEY, "?from=d/foo&to=e/foo", "x" },
    { NS_ISTREAMCONVERTER_KEY, "?from=d/foo&to=f/foo", "x" },
    { NS_ISTREAMCONVERTER_KEY, "?from=t/foo&to=k/foo", "x" },
    { NULL }
};

static const mozilla::Module kTestModule = {
    mozilla::Module::kVersion,
    kTestCIDs,
    kTestContracts,
    kTestCategories
};

int
main(int argc, char* argv[])
{
    nsresult rv;
    {
        XRE_AddStaticComponent(&kTestModule);

        nsCOMPtr<nsIServiceManager> servMan;
        NS_InitXPCOM2(getter_AddRefs(servMan), nsnull, nsnull);
    
        nsCOMPtr<nsIThread> thread = do_GetCurrentThread();

        nsCOMPtr<nsICategoryManager> catman =
            do_GetService(NS_CATEGORYMANAGER_CONTRACTID, &rv);
        if (NS_FAILED(rv)) return rv;
        nsCString previous;

        nsCOMPtr<nsIStreamConverterService> StreamConvService =
                 do_GetService(kStreamConverterServiceCID, &rv);
        if (NS_FAILED(rv)) return rv;

        // Define the *from* content type and *to* content-type for conversion.
        static const char fromStr[] = "a/foo";
        static const char toStr[] = "c/foo";
    
#ifdef ASYNC_TEST
        // ASYNCHRONOUS conversion

        // Build up a channel that represents the content we're
        // starting the transaction with.
        //
        // sample multipart mixed content-type string:
        // "multipart/x-mixed-replacE;boundary=thisrandomstring"
#if 0
        nsCOMPtr<nsIChannel> channel;
        nsCOMPtr<nsIURI> dummyURI;
        rv = NS_NewURI(getter_AddRefs(dummyURI), "http://meaningless");
        if (NS_FAILED(rv)) return rv;

        rv = NS_NewInputStreamChannel(getter_AddRefs(channel),
                                      dummyURI,
                                      nsnull,   // inStr
                                      "text/plain", // content-type
                                      -1);      // XXX fix contentLength
        if (NS_FAILED(rv)) return rv;

        nsCOMPtr<nsIRequest> request(do_QueryInterface(channel));
#endif

        nsCOMPtr<nsIRequest> request;

        // setup a listener to receive the converted data. This guy is the end
        // listener in the chain, he wants the fully converted (toType) data.
        // An example of this listener in mozilla would be the DocLoader.
        nsIStreamListener *dataReceiver = new EndListener();
        NS_ADDREF(dataReceiver);

        // setup a listener to push the data into. This listener sits inbetween the
        // unconverted data of fromType, and the final listener in the chain (in this case
        // the dataReceiver.
        nsIStreamListener *converterListener = nsnull;
        rv = StreamConvService->AsyncConvertData(fromStr, toStr,
                                                 dataReceiver, nsnull, &converterListener);
        if (NS_FAILED(rv)) return rv;
        NS_RELEASE(dataReceiver);

        // at this point we have a stream listener to push data to, and the one
        // that will receive the converted data. Let's mimic On*() calls and get the conversion
        // going. Typically these On*() calls would be made inside their respective wrappers On*()
        // methods.
        rv = converterListener->OnStartRequest(request, nsnull);
        if (NS_FAILED(rv)) return rv;

        rv = SEND_DATA("aaa");
        if (NS_FAILED(rv)) return rv;

        rv = SEND_DATA("aaa");
        if (NS_FAILED(rv)) return rv;

        // Finish the request.
        rv = converterListener->OnStopRequest(request, nsnull, rv);
        if (NS_FAILED(rv)) return rv;

        NS_RELEASE(converterListener);
#else
        // SYNCHRONOUS conversion
        nsCOMPtr<nsIInputStream> convertedData;
        rv = StreamConvService->Convert(inputData, fromStr, toStr,
                                        nsnull, getter_AddRefs(convertedData));
        if (NS_FAILED(rv)) return rv;
#endif

        // Enter the message pump to allow the URL load to proceed.
        while ( gKeepRunning ) {
            if (!NS_ProcessNextEvent(thread))
                break;
        }
    } // this scopes the nsCOMPtrs
    // no nsCOMPtrs are allowed to be alive when you call NS_ShutdownXPCOM
    NS_ShutdownXPCOM(nsnull);
    return rv;
}
