/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#include "TestCommon.h"
#include <stdio.h>
#ifdef WIN32 
#include <windows.h>
#endif
#include "nspr.h"
#include "nscore.h"
#include "nsCOMPtr.h"
#include "nsIEventQueueService.h"
#include "nsIIOService.h"
#include "nsIServiceManager.h"
#include "nsIStreamListener.h"
#include "nsIInputStream.h"
#include "nsCRT.h"
#include "nsIChannel.h"
#include "nsIURL.h"
#include "nsIInterfaceRequestor.h" 
#include "nsIInterfaceRequestorUtils.h"
#include "nsIDNSService.h" 

#include "nsISimpleEnumerator.h"
#include "nsXPIDLString.h"
#include "nsNetUtil.h"

static NS_DEFINE_CID(kEventQueueServiceCID,      NS_EVENTQUEUESERVICE_CID);
static NS_DEFINE_CID(kIOServiceCID,              NS_IOSERVICE_CID);

static int gKeepRunning = 0;
static nsIEventQueue* gEventQ = nsnull;
static PRBool gError = PR_FALSE;

#define NS_IEQUALS_IID \
    { 0x11c5c8ee, 0x1dd2, 0x11b2, \
      { 0xa8, 0x93, 0xbb, 0x23, 0xa1, 0xb6, 0x27, 0x76 }}

class nsIEquals : public nsISupports {
public:
    NS_DEFINE_STATIC_IID_ACCESSOR(NS_IEQUALS_IID)
    NS_IMETHOD Equals(void *aPtr, PRBool *_retval) = 0;
};

class ConsumerContext : public nsIEquals {
public:
    NS_DECL_ISUPPORTS

    ConsumerContext() { }

    NS_IMETHOD Equals(void *aPtr, PRBool *_retval) {
        *_retval = PR_TRUE;
        if (aPtr != this) *_retval = PR_FALSE;
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
    PRBool  mOnStart; // have we received an OnStart?
    PRBool  mOnStop;  // have we received an onStop?
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
    mOnStart = PR_TRUE;

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
        gError = PR_TRUE;
        fprintf(stderr, "ERROR: No OnStart received\n");
    }

    if (mOnStop) {
        fprintf(stderr, "INFO: multiple OnStops received\n");
    }

    fprintf(stderr, "INFO: received %d OnData()s\n", mOnDataCount);

    mOnStop = PR_TRUE;

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
        gError = PR_TRUE;
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
    mOnStart = mOnStop = PR_FALSE;
    mOnDataCount = 0;
    gKeepRunning++;
}

Consumer::~Consumer() {
    fprintf(stderr, "Consumer::~Consumer -> in\n\n");

    if (!mOnStart) {
        gError = PR_TRUE;
        fprintf(stderr, "ERROR: Never got an OnStart\n");
    }

    if (!mOnStop) {
        gError = PR_TRUE;
        fprintf(stderr, "ERROR: Never got an OnStop \n");
    }

    fprintf(stderr, "Consumer::~Consumer -> out\n\n");
    gKeepRunning--;
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

    PRBool same = PR_FALSE;

    rv = mURI->Equals(uri, &same);
    if (NS_FAILED(rv)) return rv;

    if (!same)
        fprintf(stderr, "INFO: URIs do not match\n");

    rv = mContext->Equals((void*)aContext, &same);
    if (NS_FAILED(rv)) return rv;

    if (!same) {
        gError = PR_TRUE;
        fprintf(stderr, "ERROR: Contexts do not match\n");
    }
    return rv;
}

nsresult StartLoad(const char *);

int main(int argc, char *argv[]) {
    if (test_common_init(&argc, &argv) != 0)
        return -1;

    nsresult rv = NS_OK;
    PRBool cmdLineURL = PR_FALSE;

    if (argc > 1) {
        // run in signle url mode
        cmdLineURL = PR_TRUE;
    }

    rv = NS_InitXPCOM2(nsnull, nsnull, nsnull);
    if (NS_FAILED(rv)) return rv;

    // Create the Event Queue for this thread...
    nsCOMPtr<nsIEventQueueService> eventQService = 
             do_GetService(kEventQueueServiceCID, &rv);
    if (NS_FAILED(rv)) return rv;

    rv = eventQService->GetThreadEventQueue(NS_CURRENT_THREAD, &gEventQ);
    if (NS_FAILED(rv)) return rv;

    if (cmdLineURL) {
        rv = StartLoad(argv[1]);
    } else {
        rv = StartLoad("http://badhostnamexyz/test.txt");
    }
    if (NS_FAILED(rv)) return rv;

    // Enter the message pump to allow the URL load to proceed.
    while ( gKeepRunning ) {
        PLEvent *gEvent;
        gEventQ->WaitForEvent(&gEvent);
        gEventQ->HandleEvent(gEvent);
    }

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


    nsCOMPtr<nsIIOService> serv;
    rv = nsServiceManager::GetService(kIOServiceCID, NS_GET_IID(nsIIOService),
                                      getter_AddRefs(serv));
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
    return channel->AsyncOpen(NS_STATIC_CAST(nsIStreamListener*, consumer), contextSup);
}
