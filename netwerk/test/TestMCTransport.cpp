/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: NPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.1 (the "License"); you may not use this file except in
 * compliance with the License. You may obtain a copy of the License at
 * http://www.mozilla.org/NPL/
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
 * Portions created by the Initial Developer are Copyright (C) 2001
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Darin Fisher <darin@netscape.com> (original author)
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or 
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the NPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the NPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#include <stdio.h>

#include "nsIServiceManager.h"
#include "nsIEventQueueService.h"
#include "nsIOutputStream.h"
#include "nsIStreamListener.h"
#include "nsITransport.h"
#include "nsIInputStream.h"
#include "nsIOutputStream.h"
#include "nsCOMPtr.h"
#include "plstr.h"
#include "prprf.h"

#ifndef USE_CREATE_INSTANCE
#include "nsICacheService.h"
#include "nsICacheSession.h"
#include "nsICacheEntryDescriptor.h"
#include "nsNetCID.h"
static NS_DEFINE_CID(kCacheServiceCID, NS_CACHESERVICE_CID);
static nsICacheSession *session = nsnull;
static nsICacheEntryDescriptor *desc = nsnull;
#endif

/**
 * This test program exercises the memory cache's nsITransport implementation.
 *
 * This test program loads a file into the memory cache (using OpenOutputStream),
 * and then reads the file back out (using AsyncRead).  The data read from the
 * memory cache is written to a new file (with .out added as a suffix to the file
 * name).
 */

static NS_DEFINE_CID(kEventQueueServiceCID, NS_EVENTQUEUESERVICE_CID);
static NS_DEFINE_IID(kEventQueueCID, NS_EVENTQUEUE_CID);
static nsIEventQueue *gEventQ = nsnull;

class TestListener : public nsIStreamListener
{
public:
    NS_DECL_ISUPPORTS
    NS_DECL_NSISTREAMLISTENER
    NS_DECL_NSIREQUESTOBSERVER

    TestListener(char *);
    virtual ~TestListener();

private:
    char *mFilename;
    FILE *mFile;
};

NS_IMPL_ISUPPORTS2(TestListener,
                   nsIStreamListener,
                   nsIRequestObserver)

TestListener::TestListener(char *filename)
    : mFilename(filename)
    , mFile(nsnull)
{
    NS_INIT_ISUPPORTS();
}

TestListener::~TestListener()
{
}

NS_IMETHODIMP
TestListener::OnStartRequest(nsIRequest *req, nsISupports *ctx)
{
    printf("OnStartRequest\n");

    mFile = fopen(mFilename, "w");
    if (!mFile)
        return NS_ERROR_FAILURE;

    return NS_OK;
}

NS_IMETHODIMP
TestListener::OnStopRequest(nsIRequest *req, nsISupports *ctx, nsresult status)
{
    printf("OnStopRequest: status=%x\n", status);

    if (mFile)
        fclose(mFile);

    return NS_OK;
}

NS_IMETHODIMP
TestListener::OnDataAvailable(nsIRequest *req, nsISupports *ctx,
                              nsIInputStream *is,
                              PRUint32 offset, PRUint32 count)
{
    printf("OnDataAvailable: offset=%u count=%u\n", offset, count);

    if (!mFile) return NS_ERROR_FAILURE;

    char buf[128];
    nsresult rv;
    PRUint32 nread = 0;

    while (count) {
        PRUint32 amount = PR_MIN(count, sizeof(buf));

        rv = is->Read(buf, amount, &nread);
        if (NS_FAILED(rv)) return rv;

        fwrite(buf, nread, 1, mFile);
        count -= nread;
    }
    return NS_OK;
}

nsresult TestMCTransport(const char *filename)
{
    nsresult rv = NS_OK;
    nsCOMPtr<nsITransport> transport;

#ifdef USE_CREATE_INSTANCE
    rv = nsComponentManager::CreateInstance(
                    "@mozilla.org/network/memory-cache-transport;1",
                    nsnull,
                    NS_GET_IID(nsITransport),
                    getter_AddRefs(transport));
    if (NS_FAILED(rv))
        return rv;
#else
    nsCOMPtr<nsICacheService> serv(do_GetService(kCacheServiceCID, &rv));
    if (NS_FAILED(rv)) return rv;

    rv = serv->CreateSession("TestMCTransport",
                             nsICache::STORE_IN_MEMORY, PR_TRUE,
                             &session);
    if (NS_FAILED(rv)) return rv;

    rv = session->OpenCacheEntry(filename,
                                 nsICache::ACCESS_READ_WRITE,
                                 nsICache::BLOCKING,
                                 &desc);
    if (NS_FAILED(rv)) return rv;

    rv = desc->MarkValid();
    if (NS_FAILED(rv)) return rv;

    rv = desc->GetTransport(getter_AddRefs(transport));
    if (NS_FAILED(rv)) return rv;
#endif

    nsCOMPtr<nsIOutputStream> os;
    rv = transport->OpenOutputStream(0, (PRUint32) -1, 0, getter_AddRefs(os));
    if (NS_FAILED(rv)) return rv;

    char *out = PR_smprintf("%s.out", filename);
    nsCOMPtr<nsIStreamListener> listener = new TestListener(out);
    if (!listener)
        return NS_ERROR_OUT_OF_MEMORY;

    nsCOMPtr<nsIRequest> req;
    rv = transport->AsyncRead(listener, nsnull, 0, (PRUint32) -1, 0, getter_AddRefs(req));
    if (NS_FAILED(rv)) return rv;

    FILE *file = fopen(filename, "r");
    if (!file)
        return NS_ERROR_FILE_NOT_FOUND;

    char buf[256];
    PRUint32 count, total=0;

    while ((count = fread(buf, 1, sizeof(buf), file)) > 0) {
        printf("writing %u bytes\n", count);
        total += count;
        rv = os->Write(buf, count, &count);
        if (NS_FAILED(rv)) return rv;

        // process an event
        PLEvent *event = nsnull;
        gEventQ->GetEvent(&event);
        if (event) gEventQ->HandleEvent(event);
    }

    printf("wrote %u bytes\n", total);

    return rv;
}

int main(int argc, char **argv)
{
    nsresult rv;

    if (argc < 2) {
        printf("usage: %s filename\n", argv[0]);
        return -1;
    }

    nsCOMPtr<nsIEventQueueService> eqs = 
             do_GetService(kEventQueueServiceCID, &rv);
    if (NS_FAILED(rv)) {
        printf("failed to create event queue service: rv=%x\n", rv);
        return -1;
    }

    rv = eqs->CreateMonitoredThreadEventQueue();
    if (NS_FAILED(rv)) {
        printf("failed to create monitored event queue: rv=%x\n", rv);
        return -1;
    }

    rv = eqs->GetThreadEventQueue(NS_CURRENT_THREAD, &gEventQ);
    if (NS_FAILED(rv)) {
        printf("failed to get thread event queue: %x\n", rv);
        return -1;
    }

    rv = TestMCTransport(argv[1]);
    printf("TestMCTransport returned %x\n", rv);

    gEventQ->ProcessPendingEvents();

#ifndef USE_CREATE_INSTANCE
    NS_IF_RELEASE(desc);
    NS_IF_RELEASE(session);
#endif
    return 0;
}
