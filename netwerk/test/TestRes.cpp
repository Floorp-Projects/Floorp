/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsIResProtocolHandler.h"
#include "nsIServiceManager.h"
#include "nsIIOService.h"
#include "nsIInputStream.h"
#include "nsIComponentManager.h"
#include "nsIComponentRegistrar.h"
#include "nsIStreamListener.h"
#include "nsIEventQueueService.h"
#include "nsIURI.h"
#include "nsCRT.h"
#include "nsNetCID.h"

static NS_DEFINE_CID(kIOServiceCID, NS_IOSERVICE_CID);
static NS_DEFINE_CID(kEventQueueServiceCID, NS_EVENTQUEUESERVICE_CID);

////////////////////////////////////////////////////////////////////////////////

nsresult
SetupMapping()
{
    nsresult rv;

    nsCOMPtr<nsIIOService> serv(do_GetService(kIOServiceCID, &rv));
    if (NS_FAILED(rv)) return rv;

    nsCOMPtr<nsIProtocolHandler> ph;
    rv = serv->GetProtocolHandler("res", getter_AddRefs(ph));
    if (NS_FAILED(rv)) return rv;

    nsCOMPtr<nsIResProtocolHandler> resPH = do_QueryInterface(ph, &rv);
    if (NS_FAILED(rv)) return rv;

    rv = resPH->AppendSubstitution("foo", "file://y|/");
    if (NS_FAILED(rv)) return rv;

    rv = resPH->AppendSubstitution("foo", "file://y|/mozilla/dist/win32_D.OBJ/bin/");
    if (NS_FAILED(rv)) return rv;

    return rv;
}

////////////////////////////////////////////////////////////////////////////////

nsresult
TestOpenInputStream(const char* url)
{
    nsresult rv;

    nsCOMPtr<nsIIOService> serv(do_GetService(kIOServiceCID, &rv));
    if (NS_FAILED(rv)) return rv;

    nsCOMPtr<nsIChannel> channel;
    rv = serv->NewChannel(url,
                          nullptr, // base uri
                          getter_AddRefs(channel));
    if (NS_FAILED(rv)) return rv;

    nsCOMPtr<nsIInputStream> in;
    rv = channel->Open(getter_AddRefs(in));
    if (NS_FAILED(rv)) {
        fprintf(stdout, "failed to OpenInputStream for %s\n", url);
        return NS_OK;
    }

    char buf[1024];
    while (1) {
        uint32_t amt;
        rv = in->Read(buf, sizeof(buf), &amt);
        if (NS_FAILED(rv)) return rv;
        if (amt == 0) break;    // eof

        char* str = buf;
        while (amt-- > 0) {
            fputc(*str++, stdout);
        }
    }
    nsCOMPtr<nsIURI> uri;
    char* str;

    rv = channel->GetOriginalURI(getter_AddRefs(uri));
    if (NS_FAILED(rv)) return rv;
    rv = uri->GetSpec(&str);
    if (NS_FAILED(rv)) return rv;
    fprintf(stdout, "%s resolved to ", str);
    free(str);

    rv = channel->GetURI(getter_AddRefs(uri));
    if (NS_FAILED(rv)) return rv;
    rv = uri->GetSpec(&str);
    if (NS_FAILED(rv)) return rv;
    fprintf(stdout, "%s\n", str);
    free(str);

    return NS_OK;
}

////////////////////////////////////////////////////////////////////////////////

bool gDone = false;
nsIEventQueue* gEventQ = nullptr;

class Listener : public nsIStreamListener 
{
public:
    NS_DECL_ISUPPORTS

    Listener() {}
    virtual ~Listener() {}

    NS_IMETHOD OnStartRequest(nsIRequest *request, nsISupports *ctxt) {
        nsresult rv;
        nsCOMPtr<nsIURI> uri;
        nsCOMPtr<nsIChannel> channel = do_QueryInterface(request);

        rv = channel->GetURI(getter_AddRefs(uri));
        if (NS_SUCCEEDED(rv)) {
            char* str;
            rv = uri->GetSpec(&str);
            if (NS_SUCCEEDED(rv)) {
                fprintf(stdout, "Starting to load %s\n", str);
                free(str);
            }
        }
        return NS_OK;
    }

    NS_IMETHOD OnStopRequest(nsIRequest *request, nsISupports *ctxt, 
                             nsresult aStatus) {
        nsresult rv;
        nsCOMPtr<nsIURI> uri;
        nsCOMPtr<nsIChannel> channel = do_QueryInterface(request);

        rv = channel->GetURI(getter_AddRefs(uri));
        if (NS_SUCCEEDED(rv)) {
            char* str;
            rv = uri->GetSpec(&str);
            if (NS_SUCCEEDED(rv)) {
                fprintf(stdout, "Ending load %s, status=%x\n", str, aStatus);
                free(str);
            }
        }
        gDone = true;
        return NS_OK;
    }

    NS_IMETHOD OnDataAvailable(nsIRequest *request, nsISupports *ctxt, 
                               nsIInputStream *inStr,
                               uint64_t sourceOffset, uint32_t count) {
        nsresult rv;
        char buf[1024];
        while (count > 0) {
            uint32_t amt;
            rv = inStr->Read(buf, sizeof(buf), &amt);
            count -= amt;
            char* c = buf;
            while (amt-- > 0) {
                fputc(*c++, stdout);
            }
        }
        return NS_OK;
    }
};

NS_IMPL_ISUPPORTS2(Listener, nsIStreamListener, nsIRequestObserver)

nsresult
TestAsyncRead(const char* url)
{
    nsresult rv;

    nsCOMPtr<nsIEventQueueService> eventQService = 
             do_GetService(kEventQueueServiceCID, &rv);
    if (NS_FAILED(rv)) return rv;

    rv = eventQService->GetThreadEventQueue(NS_CURRENT_THREAD, &gEventQ);
    if (NS_FAILED(rv)) return rv;

    nsCOMPtr<nsIIOService> serv(do_GetService(kIOServiceCID, &rv));
    if (NS_FAILED(rv)) return rv;

    nsCOMPtr<nsIChannel> channel;
    rv = serv->NewChannel(url,
                          nullptr, // base uri
                          getter_AddRefs(channel));
    if (NS_FAILED(rv)) return rv;

    nsCOMPtr<nsIStreamListener> listener = new Listener();
    if (listener == nullptr)
        return NS_ERROR_OUT_OF_MEMORY;
    rv = channel->AsyncOpen(nullptr, listener);
    if (NS_FAILED(rv)) return rv;

    while (!gDone) {
        PLEvent* event;
        rv = gEventQ->GetEvent(&event);
        if (NS_FAILED(rv)) return rv;
        rv = gEventQ->HandleEvent(event);
        if (NS_FAILED(rv)) return rv;
    }

    return rv;
}

int
main(int argc, char* argv[])
{
    nsresult rv;
    {
        nsCOMPtr<nsIServiceManager> servMan;
        NS_InitXPCOM2(getter_AddRefs(servMan), nullptr, nullptr);
        nsCOMPtr<nsIComponentRegistrar> registrar = do_QueryInterface(servMan);
        NS_ASSERTION(registrar, "Null nsIComponentRegistrar");
        if (registrar)
            registrar->AutoRegister(nullptr);

        NS_ASSERTION(NS_SUCCEEDED(rv), "AutoregisterComponents failed");

        if (argc < 2) {
            printf("usage: %s resource://foo/<path-to-resolve>\n", argv[0]);
            return -1;
        }

        rv = SetupMapping();
        NS_ASSERTION(NS_SUCCEEDED(rv), "SetupMapping failed");
        if (NS_FAILED(rv)) return rv;

        rv = TestOpenInputStream(argv[1]);
        NS_ASSERTION(NS_SUCCEEDED(rv), "TestOpenInputStream failed");

        rv = TestAsyncRead(argv[1]);
        NS_ASSERTION(NS_SUCCEEDED(rv), "TestAsyncRead failed");
    } // this scopes the nsCOMPtrs
    // no nsCOMPtrs are allowed to be alive when you call NS_ShutdownXPCOM
    rv = NS_ShutdownXPCOM(nullptr);
    NS_ASSERTION(NS_SUCCEEDED(rv), "NS_ShutdownXPCOM failed");
    return rv;
}

////////////////////////////////////////////////////////////////////////////////
