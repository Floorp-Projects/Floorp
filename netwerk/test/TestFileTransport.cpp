/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * The contents of this file are subject to the Netscape Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
 */

#include "nsIFileTransportService.h"
#include "nsIChannel.h"
#include "nsIServiceManager.h"
#include "nsIComponentManager.h"
#include "nsCOMPtr.h"
#include "nsIAllocator.h"
#include "nsString.h"
#include "nsIFileStreams.h"
#include "nsIStreamListener.h"
#include "nsIEventQueueService.h"
#include "nsIEventQueue.h"

static NS_DEFINE_CID(kFileTransportServiceCID, NS_FILETRANSPORTSERVICE_CID);
static NS_DEFINE_CID(kEventQueueServiceCID, NS_EVENTQUEUESERVICE_CID);
static NS_DEFINE_CID(kEventQueueCID, NS_EVENTQUEUE_CID);

PRBool gDone = PR_FALSE;
nsIEventQueue* gEventQ = nsnull;

////////////////////////////////////////////////////////////////////////////////

class MyListener : public nsIStreamListener {
public:
    NS_DECL_ISUPPORTS

    NS_IMETHOD OnStartRequest(nsIChannel *channel, nsISupports *ctxt) {
        printf("starting\n");
        return NS_OK;
    }

    NS_IMETHOD OnStopRequest(nsIChannel *channel, nsISupports *ctxt, 
                             nsresult status, const PRUnichar *errorMsg) {
        printf("ending status=%0x total=%d\n", status, mTotal);
        if (--mStopCount == 0)
            gDone = PR_TRUE;
        return NS_OK;
    }

    NS_IMETHOD OnDataAvailable(nsIChannel *channel, nsISupports *ctxt, 
                               nsIInputStream *inStr, PRUint32 sourceOffset, 
                               PRUint32 count) {
        printf("receiving %d bytes\n", count);
        char buf[256];
        PRUint32 writeCount;
        nsresult rv;
        while (count > 0) {
            PRUint32 amt = PR_MIN(count, 256);
            PRUint32 readCount;
            rv = inStr->Read(buf, amt, &readCount);
            if (NS_FAILED(rv)) return rv;
            NS_ASSERTION(readCount != 0, "premature EOF");
            nsresult rv = mOut->Write(buf, readCount, &writeCount);
            if (NS_FAILED(rv)) return rv;
            NS_ASSERTION(writeCount == readCount, "failed to write all the data");
            count -= readCount;
            mTotal += readCount;
        }
        return NS_OK;
    }
    
    MyListener(PRUint32 stopCount = 1) : mTotal(0), mStopCount(stopCount) {
        NS_INIT_REFCNT();
    }
    
    nsresult Init(nsFileSpec& origFile) {
        nsresult rv;
        char* name = origFile.GetLeafName();
        if (name == nsnull)
            return NS_ERROR_OUT_OF_MEMORY;
        nsAutoString str; str.AssignWithConversion(name);
        nsAllocator::Free(name);
        str.AppendWithConversion(".bak");
        nsFileSpec spec(origFile);
        spec.SetLeafName(str);
        nsCOMPtr<nsILocalFile> file;
        rv = NS_NewLocalFile(spec, getter_AddRefs(file));
        rv = NS_NewLocalFileOutputStream(getter_AddRefs(mOut),
                                         file, 
                                         PR_CREATE_FILE | PR_WRONLY | PR_TRUNCATE,
                                         0664);
        return rv;
    }

    virtual ~MyListener() {
        nsresult rv = mOut->Close();
        NS_ASSERTION(NS_SUCCEEDED(rv), "Close failed");
    }

protected:
    nsCOMPtr<nsIOutputStream> mOut;
    PRUint32 mTotal;
    PRUint32 mStopCount;
};

NS_IMPL_ISUPPORTS2(MyListener, nsIStreamListener, nsIStreamObserver);

////////////////////////////////////////////////////////////////////////////////

nsresult
TestAsyncRead(const char* fileName, PRUint32 offset, PRInt32 length)
{
    nsresult rv;

    NS_WITH_SERVICE(nsIFileTransportService, fts, kFileTransportServiceCID, &rv);
    if (NS_FAILED(rv)) return rv;

    nsFileSpec fs(fileName);
    nsIChannel* fileTrans;
    nsCOMPtr<nsILocalFile> file;
    rv = NS_NewLocalFile(fs, getter_AddRefs(file));
    if (NS_FAILED(rv)) return rv;
    rv = fts->CreateTransport(file, PR_RDONLY, 0, &fileTrans);
    if (NS_FAILED(rv)) return rv;

    MyListener* listener = new MyListener();
    if (listener == nsnull)
        return NS_ERROR_OUT_OF_MEMORY;
    NS_ADDREF(listener);
    rv = listener->Init(fs);
    if (NS_FAILED(rv)) return rv;

    gDone = PR_FALSE;
    rv = fileTrans->SetTransferOffset(offset);
    if (NS_FAILED(rv)) return rv;
    rv = fileTrans->SetTransferCount(length);
    if (NS_FAILED(rv)) return rv;
    rv = fileTrans->AsyncRead(nsnull, listener);
    if (NS_FAILED(rv)) return rv;

    while (!gDone) {
        PLEvent* event;
        rv = gEventQ->GetEvent(&event);
        if (NS_FAILED(rv)) return rv;
        rv = gEventQ->HandleEvent(event);
        if (NS_FAILED(rv)) return rv;
    }

    NS_RELEASE(listener);
    NS_RELEASE(fileTrans);
    return NS_OK;
}

////////////////////////////////////////////////////////////////////////////////

nsresult
TestAsyncWrite(const char* fileName, PRUint32 offset, PRInt32 length)
{
    nsresult rv;

    NS_WITH_SERVICE(nsIFileTransportService, fts, kFileTransportServiceCID, &rv);
    if (NS_FAILED(rv)) return rv;

    nsString outFile; outFile.AssignWithConversion(fileName);
    outFile.AppendWithConversion(".out");
    nsFileSpec fs(outFile);
    nsIChannel* fileTrans;
    nsCOMPtr<nsILocalFile> file;
    rv = NS_NewLocalFile(fs, getter_AddRefs(file));
    if (NS_FAILED(rv)) return rv;
    rv = fts->CreateTransport(file,
                              PR_CREATE_FILE | PR_WRONLY | PR_TRUNCATE,
                              0664, &fileTrans);
    if (NS_FAILED(rv)) return rv;

    MyListener* listener = new MyListener();
    if (listener == nsnull)
        return NS_ERROR_OUT_OF_MEMORY;
    NS_ADDREF(listener);
    rv = listener->Init(fs);
    if (NS_FAILED(rv)) return rv;

    nsFileSpec spec(fileName);
    nsCOMPtr<nsILocalFile> f;
    rv = NS_NewLocalFile(spec, getter_AddRefs(f));
    if (NS_FAILED(rv)) return rv;
    nsCOMPtr<nsIInputStream> inStr;
    rv = NS_NewLocalFileInputStream(getter_AddRefs(inStr), f);
    if (NS_FAILED(rv)) return rv;

    gDone = PR_FALSE;
    rv = fileTrans->SetTransferOffset(offset);
    if (NS_FAILED(rv)) return rv;
    rv = fileTrans->SetTransferCount(length);
    if (NS_FAILED(rv)) return rv;
    rv = fileTrans->AsyncWrite(inStr, nsnull, listener);
    if (NS_FAILED(rv)) return rv;

    while (!gDone) {
        PLEvent* event;
        rv = gEventQ->GetEvent(&event);
        if (NS_FAILED(rv)) return rv;
        rv = gEventQ->HandleEvent(event);
        if (NS_FAILED(rv)) return rv;
    }

    NS_RELEASE(listener);
    NS_RELEASE(fileTrans);
    return NS_OK;
}

////////////////////////////////////////////////////////////////////////////////

class MyOpenObserver : public nsIStreamObserver
{
public:
    NS_DECL_ISUPPORTS

    NS_IMETHOD OnStartRequest(nsIChannel *channel, nsISupports *ctxt) {
        nsresult rv;
        char* contentType;
        rv = channel->GetContentType(&contentType);
        if (NS_FAILED(rv)) return rv;
        PRInt32 length;
        rv = channel->GetContentLength(&length);
        if (NS_FAILED(rv)) return rv;
        printf("stream opened: content type = %s, length = %d\n",
               contentType, length);
        nsCRT::free(contentType);
        return NS_OK;
    }

    NS_IMETHOD OnStopRequest(nsIChannel *channel, nsISupports *ctxt,
                             nsresult status, const PRUnichar *errorMsg) {
        printf("stream closed: status %x\n", status);
        return NS_OK;
    }

    MyOpenObserver() { NS_INIT_REFCNT(); }
    virtual ~MyOpenObserver() {}
};

NS_IMPL_ISUPPORTS1(MyOpenObserver, nsIStreamObserver);

nsresult
TestAsyncOpen(const char* fileName, PRUint32 offset, PRInt32 length)
{
    nsresult rv;

    NS_WITH_SERVICE(nsIFileTransportService, fts, kFileTransportServiceCID, &rv);
    if (NS_FAILED(rv)) return rv;

    nsFileSpec fs(fileName);
    nsIChannel* fileTrans;
    nsCOMPtr<nsILocalFile> file;
    rv = NS_NewLocalFile(fs, getter_AddRefs(file));
    if (NS_FAILED(rv)) return rv;
    rv = fts->CreateTransport(file, PR_RDONLY, 0, &fileTrans);
    if (NS_FAILED(rv)) return rv;

    MyListener* listener = new MyListener(1);
    if (listener == nsnull)
        return NS_ERROR_OUT_OF_MEMORY;
    NS_ADDREF(listener);
    rv = listener->Init(fs);
    if (NS_FAILED(rv)) return rv;

    MyOpenObserver* openObserver = new MyOpenObserver();
    if (openObserver == nsnull)
        return NS_ERROR_OUT_OF_MEMORY;
    NS_ADDREF(openObserver);

    gDone = PR_FALSE;
    rv = fileTrans->AsyncOpen(openObserver, nsnull);
    if (NS_FAILED(rv)) return rv;

    rv = fileTrans->SetTransferOffset(offset + 10);
    if (NS_FAILED(rv)) return rv;
    rv = fileTrans->SetTransferCount(length);
    if (NS_FAILED(rv)) return rv;
    rv = fileTrans->AsyncRead(nsnull, listener);
    if (NS_FAILED(rv)) return rv;
#if 0
    rv = fileTrans->AsyncRead(offset, length, nsnull, listener);
    if (NS_FAILED(rv)) return rv;

    rv = fileTrans->AsyncRead(offset + 100, length, nsnull, listener);
    if (NS_FAILED(rv)) return rv;
#endif
    while (!gDone) {
        PLEvent* event;
        rv = gEventQ->GetEvent(&event);
        if (NS_FAILED(rv)) return rv;
        rv = gEventQ->HandleEvent(event);
        if (NS_FAILED(rv)) return rv;
    }

    NS_RELEASE(openObserver);
    NS_RELEASE(listener);
    NS_RELEASE(fileTrans);
    return NS_OK;
}

////////////////////////////////////////////////////////////////////////////////

nsresult
NS_AutoregisterComponents()
{
  nsresult rv = nsComponentManager::AutoRegister(nsIComponentManager::NS_Startup, NULL /* default */);
  return rv;
}

int
main(int argc, char* argv[])
{
    nsresult rv;

    if (argc < 2) {
        printf("usage: %s <file-to-read>\n", argv[0]);
        return -1;
    }
    char* fileName = argv[1];

    rv = NS_AutoregisterComponents();
    if (NS_FAILED(rv)) return rv;

    NS_WITH_SERVICE(nsIEventQueueService, eventQService, kEventQueueServiceCID, &rv);
    if (NS_FAILED(rv)) return rv;

    rv = eventQService->CreateThreadEventQueue();
    if (NS_FAILED(rv)) return rv;

    rv = eventQService->GetThreadEventQueue(NS_CURRENT_THREAD, &gEventQ);
    if (NS_FAILED(rv)) return rv;

    rv = TestAsyncRead(fileName, 0, -1);
    NS_ASSERTION(NS_SUCCEEDED(rv), "TestAsyncRead failed");

    rv = TestAsyncWrite(fileName, 0, -1);
    NS_ASSERTION(NS_SUCCEEDED(rv), "TestAsyncWrite failed");

    rv = TestAsyncRead(fileName, 10, 100);
    NS_ASSERTION(NS_SUCCEEDED(rv), "TestAsyncRead failed");

    rv = TestAsyncOpen(fileName, 10, 100);
    NS_ASSERTION(NS_SUCCEEDED(rv), "TestAsyncOpen failed");

    NS_RELEASE(gEventQ);
    return NS_OK;
}
