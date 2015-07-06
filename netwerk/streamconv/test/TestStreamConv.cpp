/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsIServiceManager.h"
#include "nsIStreamConverterService.h"
#include "nsIStreamConverter.h"
#include "nsICategoryManager.h"
#include "mozilla/Module.h"
#include "nsXULAppAPI.h"
#include "nsIStringStream.h"
#include "nsCOMPtr.h"
#include "nsThreadUtils.h"
#include "mozilla/Attributes.h"
#include "nsMemory.h"
#include "nsServiceManagerUtils.h"
#include "nsComponentManagerUtils.h"
#include "nsIRequest.h"
#include "nsNetCID.h"
#include "nsNetUtil.h"

#define ASYNC_TEST // undefine this if you want to test sycnronous conversion.

/////////////////////////////////
// Event pump setup
/////////////////////////////////
#ifdef XP_WIN
#include <windows.h>
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
class EndListener final : public nsIStreamListener {
    ~EndListener() {}
public:
    // nsISupports declaration
    NS_DECL_ISUPPORTS

    EndListener() {}

    // nsIStreamListener method
    NS_IMETHOD OnDataAvailable(nsIRequest* request, nsISupports *ctxt, nsIInputStream *inStr, 
                               uint64_t sourceOffset, uint32_t count) override
    {
        nsresult rv;
        uint32_t read;
        uint64_t len64;
        rv = inStr->Available(&len64);
        if (NS_FAILED(rv)) return rv;
        uint32_t len = (uint32_t)std::min(len64, (uint64_t)(UINT32_MAX - 1));

        char *buffer = (char*)moz_xmalloc(len + 1);
        if (!buffer) return NS_ERROR_OUT_OF_MEMORY;

        rv = inStr->Read(buffer, len, &read);
        buffer[len] = '\0';
        if (NS_SUCCEEDED(rv)) {
            printf("CONTEXT %p: Received %u bytes and the following data: \n %s\n\n",
                   static_cast<void*>(ctxt), read, buffer);
        }
        free(buffer);

        return NS_OK;
    }

    // nsIRequestObserver methods
    NS_IMETHOD OnStartRequest(nsIRequest* request, nsISupports *ctxt) override { return NS_OK; }

    NS_IMETHOD OnStopRequest(nsIRequest* request, nsISupports *ctxt, 
                             nsresult aStatus) override { return NS_OK; }
};

NS_IMPL_ISUPPORTS(EndListener,
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

    uint64_t avail = 0;
    dataStream->Available(&avail);

    uint64_t offset = 0;
    while (avail > 0) {
        uint32_t count = saturated(avail);
        rv = aListener->OnDataAvailable(request, nullptr, dataStream,
                                        offset, count);
        if (NS_FAILED(rv)) return rv;

        offset += count;
        avail -= count;
    }
    return NS_OK;
}
#define SEND_DATA(x) SendData(x, converterListener, request)

static const mozilla::Module::CIDEntry kTestCIDs[] = {
    { &kTestConverterCID, false, nullptr, CreateTestConverter },
    { nullptr }
};

static const mozilla::Module::ContractIDEntry kTestContracts[] = {
    { NS_ISTREAMCONVERTER_KEY "?from=a/foo&to=b/foo", &kTestConverterCID },
    { NS_ISTREAMCONVERTER_KEY "?from=b/foo&to=c/foo", &kTestConverterCID },
    { NS_ISTREAMCONVERTER_KEY "?from=b/foo&to=d/foo", &kTestConverterCID },
    { NS_ISTREAMCONVERTER_KEY "?from=c/foo&to=d/foo", &kTestConverterCID },
    { NS_ISTREAMCONVERTER_KEY "?from=d/foo&to=e/foo", &kTestConverterCID },
    { NS_ISTREAMCONVERTER_KEY "?from=d/foo&to=f/foo", &kTestConverterCID },
    { NS_ISTREAMCONVERTER_KEY "?from=t/foo&to=k/foo", &kTestConverterCID },
    { nullptr }
};

static const mozilla::Module::CategoryEntry kTestCategories[] = {
    { NS_ISTREAMCONVERTER_KEY, "?from=a/foo&to=b/foo", "x" },
    { NS_ISTREAMCONVERTER_KEY, "?from=b/foo&to=c/foo", "x" },
    { NS_ISTREAMCONVERTER_KEY, "?from=b/foo&to=d/foo", "x" },
    { NS_ISTREAMCONVERTER_KEY, "?from=c/foo&to=d/foo", "x" },
    { NS_ISTREAMCONVERTER_KEY, "?from=d/foo&to=e/foo", "x" },
    { NS_ISTREAMCONVERTER_KEY, "?from=d/foo&to=f/foo", "x" },
    { NS_ISTREAMCONVERTER_KEY, "?from=t/foo&to=k/foo", "x" },
    { nullptr }
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
        NS_InitXPCOM2(getter_AddRefs(servMan), nullptr, nullptr);
    
        nsCOMPtr<nsIThread> thread = do_GetCurrentThread();

        nsCOMPtr<nsICategoryManager> catman =
            do_GetService(NS_CATEGORYMANAGER_CONTRACTID, &rv);
        if (NS_FAILED(rv)) return -1;
        nsCString previous;

        nsCOMPtr<nsIStreamConverterService> StreamConvService =
                 do_GetService(kStreamConverterServiceCID, &rv);
        if (NS_FAILED(rv)) return -1;

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
        if (NS_FAILED(rv)) return -1;

        rv = NS_NewInputStreamChannel(getter_AddRefs(channel),
                                      dummyURI,
                                      nullptr,   // inStr
                                      "text/plain", // content-type
                                      -1);      // XXX fix contentLength
        if (NS_FAILED(rv)) return -1;

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
        nsIStreamListener *converterListener = nullptr;
        rv = StreamConvService->AsyncConvertData(fromStr, toStr,
                                                 dataReceiver, nullptr, &converterListener);
        if (NS_FAILED(rv)) return -1;
        NS_RELEASE(dataReceiver);

        // at this point we have a stream listener to push data to, and the one
        // that will receive the converted data. Let's mimic On*() calls and get the conversion
        // going. Typically these On*() calls would be made inside their respective wrappers On*()
        // methods.
        rv = converterListener->OnStartRequest(request, nullptr);
        if (NS_FAILED(rv)) return -1;

        rv = SEND_DATA("aaa");
        if (NS_FAILED(rv)) return -1;

        rv = SEND_DATA("aaa");
        if (NS_FAILED(rv)) return -1;

        // Finish the request.
        rv = converterListener->OnStopRequest(request, nullptr, rv);
        if (NS_FAILED(rv)) return -1;

        NS_RELEASE(converterListener);
#else
        // SYNCHRONOUS conversion
        nsCOMPtr<nsIInputStream> convertedData;
        rv = StreamConvService->Convert(inputData, fromStr, toStr,
                                        nullptr, getter_AddRefs(convertedData));
        if (NS_FAILED(rv)) return -1;
#endif

        // Enter the message pump to allow the URL load to proceed.
        while ( gKeepRunning ) {
            if (!NS_ProcessNextEvent(thread))
                break;
        }
    } // this scopes the nsCOMPtrs
    // no nsCOMPtrs are allowed to be alive when you call NS_ShutdownXPCOM
    NS_ShutdownXPCOM(nullptr);
    return 0;
}
