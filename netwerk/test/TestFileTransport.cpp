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

#include "nsIFileTransportService.h"
#include "nsIChannel.h"
#include "nsIServiceManager.h"
#include "nsIComponentManager.h"
#include "nsCOMPtr.h"
#include "nsIAllocator.h"
#include "nsString.h"
#include "nsIFileStream.h"
#include "nsIStreamListener.h"
#include "nsIEventQueueService.h"
#include "nsIEventQueue.h"

static NS_DEFINE_CID(kFileTransportServiceCID, NS_FILETRANSPORTSERVICE_CID);
static NS_DEFINE_CID(kEventQueueServiceCID, NS_EVENTQUEUESERVICE_CID);
static NS_DEFINE_IID(kEventQueueCID, NS_EVENTQUEUE_CID);

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
    
    MyListener() : mTotal(0) {
        NS_INIT_REFCNT();
    }
    
    nsresult Init(nsFileSpec& origFile) {
        nsresult rv;
        char* name = origFile.GetLeafName();
        if (name == nsnull)
            return NS_ERROR_OUT_OF_MEMORY;
        nsAutoString str(name);
        nsAllocator::Free(name);
        str += ".bak";
        nsFileSpec spec(origFile);
        spec.SetLeafName(str);
        nsISupports* out;
        rv = NS_NewTypicalOutputFileStream(&out, spec);
        if (NS_FAILED(rv)) return rv;
        mOut = do_QueryInterface(out);
        return NS_OK;
    }

    virtual ~MyListener() {
        nsresult rv = mOut->Close();
        NS_ASSERTION(NS_SUCCEEDED(rv), "Close failed");
    }

protected:
    nsCOMPtr<nsIOutputStream> mOut;
    PRUint32 mTotal;
};

NS_IMPL_ISUPPORTS(MyListener, nsIStreamListener::GetIID());

////////////////////////////////////////////////////////////////////////////////

nsresult
TestAsyncRead(const char* fileName, PRUint32 offset, PRInt32 length)
{
    nsresult rv;

    NS_WITH_SERVICE(nsIEventQueueService, eventQService, kEventQueueServiceCID, &rv);
    if (NS_FAILED(rv)) return rv;

    rv = eventQService->CreateThreadEventQueue();
    if (NS_FAILED(rv)) return rv;

    rv = eventQService->GetThreadEventQueue(PR_CurrentThread(), &gEventQ);
    if (NS_FAILED(rv)) return rv;

    NS_WITH_SERVICE(nsIFileTransportService, fts, kFileTransportServiceCID, &rv);
    if (NS_FAILED(rv)) return rv;

    nsFileSpec fs(fileName);
    nsIChannel* fileTrans;
    rv = fts->CreateTransport(fs, "load", nsnull, &fileTrans);
    if (NS_FAILED(rv)) return rv;

    MyListener* listener = new MyListener();
    if (listener == nsnull)
        return NS_ERROR_OUT_OF_MEMORY;
    NS_ADDREF(listener);
    rv = listener->Init(fs);
    if (NS_FAILED(rv)) return rv;

    gDone = PR_FALSE;
    rv = fileTrans->AsyncRead(offset, length, nsnull, listener);
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

    NS_WITH_SERVICE(nsIEventQueueService, eventQService, kEventQueueServiceCID, &rv);
    if (NS_FAILED(rv)) return rv;

    rv = eventQService->CreateThreadEventQueue();
    if (NS_FAILED(rv)) return rv;

    rv = eventQService->GetThreadEventQueue(PR_CurrentThread(), &gEventQ);
    if (NS_FAILED(rv)) return rv;

    NS_WITH_SERVICE(nsIFileTransportService, fts, kFileTransportServiceCID, &rv);
    if (NS_FAILED(rv)) return rv;

    nsString outFile(fileName);
    outFile += ".out";
    nsFileSpec fs(outFile);
    nsIChannel* fileTrans;
    rv = fts->CreateTransport(fs, "load", nsnull, &fileTrans);
    if (NS_FAILED(rv)) return rv;

    MyListener* listener = new MyListener();
    if (listener == nsnull)
        return NS_ERROR_OUT_OF_MEMORY;
    NS_ADDREF(listener);
    rv = listener->Init(fs);
    if (NS_FAILED(rv)) return rv;

    nsFileSpec spec(fileName);
    nsCOMPtr<nsISupports> in;
    rv = NS_NewTypicalInputFileStream(getter_AddRefs(in), spec);
    if (NS_FAILED(rv)) return rv;
    nsCOMPtr<nsIInputStream> inStr = do_QueryInterface(in);

    gDone = PR_FALSE;
    rv = fileTrans->AsyncWrite(inStr, offset, length, nsnull, listener);
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

    rv = TestAsyncRead(fileName, 0, -1);
    NS_ASSERTION(NS_SUCCEEDED(rv), "TestAsyncRead failed");

    rv = TestAsyncWrite(fileName, 0, -1);
    NS_ASSERTION(NS_SUCCEEDED(rv), "TestAsyncWrite failed");

    rv = TestAsyncRead(fileName, 10, 100);
    NS_ASSERTION(NS_SUCCEEDED(rv), "TestAsyncRead failed");

    return NS_OK;
}
