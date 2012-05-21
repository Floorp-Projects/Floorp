/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "TestCommon.h"
#include <stdio.h>
#ifdef WIN32 
#include <windows.h>
#endif
#include "nspr.h"
#include "nscore.h"
#include "nsCOMPtr.h"
#include "nsIIOService.h"
#include "nsIServiceManager.h"
#include "nsIStreamListener.h"
#include "nsIInputStream.h"
#include "nsIChannel.h"
#include "nsIURL.h"
#include "nsIInterfaceRequestor.h" 
#include "nsIInterfaceRequestorUtils.h"
#include "nsIDNSService.h" 

#include "nsISimpleEnumerator.h"
#include "nsNetUtil.h"
#include "nsStringAPI.h"

static NS_DEFINE_CID(kIOServiceCID,              NS_IOSERVICE_CID);

static bool gError = false;
static PRInt32 gKeepRunning = 0;

#define NS_IEQUALS_IID \
    { 0x11c5c8ee, 0x1dd2, 0x11b2, \
      { 0xa8, 0x93, 0xbb, 0x23, 0xa1, 0xb6, 0x27, 0x76 }}

class nsIEquals : public nsISupports {
public:
    NS_DECLARE_STATIC_IID_ACCESSOR(NS_IEQUALS_IID)
    NS_IMETHOD Equals(void *aPtr, bool *_retval) = 0;
};

NS_DEFINE_STATIC_IID_ACCESSOR(nsIEquals, NS_IEQUALS_IID)

class ConsumerContext : public nsIEquals {
public:
    NS_DECL_ISUPPORTS

    ConsumerContext() { }

    NS_IMETHOD Equals(void *aPtr, bool *_retval) {
        *_retval = true;
        if (aPtr != this) *_retval = false;
        return NS_OK;
    };
};

NS_IMPL_THREADSAFE_ISUPPORTS1(ConsumerContext, nsIEquals)

class Consumer : public nsIStreamListener {
public:
    NS_DECL_ISUPPORTS
    NS_DECL_NSIREQUESTOBSERVER
    NS_DECL_NSISTREAMLISTENER

    Consumer();
    virtual ~Consumer();
    nsresult Init(nsIURI *aURI, nsIChannel *aChannel, nsISupports *aContext);
    nsresult Validate(nsIRequest *request, nsISupports *aContext);

    // member data
    bool    mOnStart; // have we received an OnStart?
    bool    mOnStop;  // have we received an onStop?
    PRInt32 mOnDataCount; // number of times OnData was called.
    nsCOMPtr<nsIURI>     mURI;
    nsCOMPtr<nsIChannel> mChannel;
    nsCOMPtr<nsIEquals>  mContext;
};

// nsISupports implementation
NS_IMPL_THREADSAFE_ISUPPORTS2(Consumer, nsIStreamListener, nsIRequestObserver)


// nsIRequestObserver implementation
NS_IMETHODIMP
Consumer::OnStartRequest(nsIRequest *request, nsISupports* aContext) {
    fprintf(stderr, "Consumer::OnStart() -> in\n\n");

    if (mOnStart) {
        fprintf(stderr, "INFO: multiple OnStarts received\n");
    }
    mOnStart = true;

    nsresult rv = Validate(request, aContext);
    if (NS_FAILED(rv)) return rv;

    fprintf(stderr, "Consumer::OnStart() -> out\n\n");
    return rv;
}

NS_IMETHODIMP
Consumer::OnStopRequest(nsIRequest *request, nsISupports *aContext,
                        nsresult aStatus) {
    fprintf(stderr, "Consumer::OnStop() -> in\n\n");

    if (!mOnStart) {
        gError = true;
        fprintf(stderr, "ERROR: No OnStart received\n");
    }

    if (mOnStop) {
        fprintf(stderr, "INFO: multiple OnStops received\n");
    }

    fprintf(stderr, "INFO: received %d OnData()s\n", mOnDataCount);

    mOnStop = true;

    nsresult rv = Validate(request, aContext);
    if (NS_FAILED(rv)) return rv;

    fprintf(stderr, "Consumer::OnStop() -> out\n\n");
    return rv;
}


// nsIStreamListener implementation
NS_IMETHODIMP
Consumer::OnDataAvailable(nsIRequest *request, nsISupports *aContext,
                          nsIInputStream *aIStream,
                          PRUint32 aOffset, PRUint32 aLength) {
    fprintf(stderr, "Consumer::OnData() -> in\n\n");

    if (!mOnStart) {
        gError = true;
        fprintf(stderr, "ERROR: No OnStart received\n");
    }

    mOnDataCount += 1;

    nsresult rv = Validate(request, aContext);
    if (NS_FAILED(rv)) return rv;

    fprintf(stderr, "Consumer::OnData() -> out\n\n");
    return rv;
}

// Consumer implementation
Consumer::Consumer() {
    mOnStart = mOnStop = false;
    mOnDataCount = 0;
    gKeepRunning++;
}

Consumer::~Consumer() {
    fprintf(stderr, "Consumer::~Consumer -> in\n\n");

    if (!mOnStart) {
        gError = true;
        fprintf(stderr, "ERROR: Never got an OnStart\n");
    }

    if (!mOnStop) {
        gError = true;
        fprintf(stderr, "ERROR: Never got an OnStop \n");
    }

    fprintf(stderr, "Consumer::~Consumer -> out\n\n");
    if (--gKeepRunning == 0)
      QuitPumpingEvents();
}

nsresult
Consumer::Init(nsIURI *aURI, nsIChannel* aChannel, nsISupports *aContext) {
    mURI     = aURI;
    mChannel = aChannel;
    mContext = do_QueryInterface(aContext);
    return NS_OK;
}

nsresult
Consumer::Validate(nsIRequest* request, nsISupports *aContext) {
    nsresult rv = NS_OK;
    nsCOMPtr<nsIURI> uri;
    nsCOMPtr<nsIChannel> aChannel = do_QueryInterface(request);

    rv = aChannel->GetURI(getter_AddRefs(uri));
    if (NS_FAILED(rv)) return rv;

    bool same = false;

    rv = mURI->Equals(uri, &same);
    if (NS_FAILED(rv)) return rv;

    if (!same)
        fprintf(stderr, "INFO: URIs do not match\n");

    rv = mContext->Equals((void*)aContext, &same);
    if (NS_FAILED(rv)) return rv;

    if (!same) {
        gError = true;
        fprintf(stderr, "ERROR: Contexts do not match\n");
    }
    return rv;
}

nsresult StartLoad(const char *);

int main(int argc, char *argv[]) {
    if (test_common_init(&argc, &argv) != 0)
        return -1;

    nsresult rv = NS_OK;
    bool cmdLineURL = false;

    if (argc > 1) {
        // run in signle url mode
        cmdLineURL = true;
    }

    rv = NS_InitXPCOM2(nsnull, nsnull, nsnull);
    if (NS_FAILED(rv)) return rv;

    if (cmdLineURL) {
        rv = StartLoad(argv[1]);
    } else {
        rv = StartLoad("http://badhostnamexyz/test.txt");
    }
    if (NS_FAILED(rv)) return rv;

    // Enter the message pump to allow the URL load to proceed.
    PumpEvents();

    NS_ShutdownXPCOM(nsnull);
    if (gError) {
        fprintf(stderr, "\n\n-------ERROR-------\n\n");
    }
    return rv;
}

nsresult StartLoad(const char *aURISpec) {
    nsresult rv = NS_OK;

    // create a context
    ConsumerContext *context = new ConsumerContext;
    nsCOMPtr<nsISupports> contextSup = do_QueryInterface(context, &rv);
    if (NS_FAILED(rv)) return rv;


    nsCOMPtr<nsIIOService> serv = do_GetService(kIOServiceCID, &rv);
    if (NS_FAILED(rv)) return rv;

    // create a uri
    nsCOMPtr<nsIURI> uri;
    rv = NS_NewURI(getter_AddRefs(uri), aURISpec);
    if (NS_FAILED(rv)) return rv;

    // create a channel
    nsCOMPtr<nsIChannel> channel;
    rv = serv->NewChannelFromURI(uri, getter_AddRefs(channel));
    if (NS_FAILED(rv)) return rv;

    Consumer *consumer = new Consumer;
    rv = consumer->Init(uri, channel, contextSup);
    if (NS_FAILED(rv)) return rv;

    // kick off the load
    nsCOMPtr<nsIRequest> request;
    return channel->AsyncOpen(static_cast<nsIStreamListener*>(consumer), contextSup);
}
