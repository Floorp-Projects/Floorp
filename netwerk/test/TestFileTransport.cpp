/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
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
 * use your version of this file under the terms of the NPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the NPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#include "nsIFileTransportService.h"
#include "nsITransport.h"
#include "nsIProgressEventSink.h"
#include "nsIInterfaceRequestor.h"
#include "nsIInterfaceRequestorUtils.h"
#include "nsIRequest.h"
#include "nsIServiceManager.h"
#include "nsIComponentManager.h"
#include "nsCOMPtr.h"
#include "nsMemory.h"
#include "nsString.h"
#include "nsIFileStreams.h"
#include "nsIStreamListener.h"
#include "nsIEventQueueService.h"
#include "nsIEventQueue.h"
#include "nsNetUtil.h"
#include "prlog.h"

////////////////////////////////////////////////////////////////////////////////

#if defined(PR_LOGGING)
static PRLogModuleInfo *gTestFileTransportLog = nsnull;
#define PRINTF(args) PR_LOG(gTestFileTransportLog, PR_LOG_ALWAYS, args)
#else
#define PRINTF(args)
#endif

#undef PRINTF
#define PRINTF(args) printf args

////////////////////////////////////////////////////////////////////////////////

static NS_DEFINE_CID(kFileTransportServiceCID, NS_FILETRANSPORTSERVICE_CID);
static NS_DEFINE_CID(kEventQueueServiceCID, NS_EVENTQUEUESERVICE_CID);
static NS_DEFINE_CID(kEventQueueCID, NS_EVENTQUEUE_CID);

PRBool gDone = PR_FALSE;
nsIEventQueue* gEventQ = nsnull;

////////////////////////////////////////////////////////////////////////////////

class MyProgressEventSink : public nsIProgressEventSink
                          , public nsIInterfaceRequestor {
public:
    NS_DECL_ISUPPORTS

    MyProgressEventSink() { NS_INIT_ISUPPORTS(); }

    NS_IMETHOD OnProgress(nsIRequest *request, nsISupports *ctxt,
                          PRUint32 progress, PRUint32 progressMax) {
        PRINTF(("progress: %u/%u\n", progress, progressMax));
        return NS_OK;
    }

    NS_IMETHOD OnStatus(nsIRequest *request, nsISupports *ctxt,
                        nsresult status, const PRUnichar *statusArg) {
        return NS_OK;
    }

    NS_IMETHOD GetInterface(const nsIID &iid, void **result) {
        if (iid.Equals(NS_GET_IID(nsIProgressEventSink)))
            return QueryInterface(iid, result);
        return NS_ERROR_NO_INTERFACE;
    }
};

NS_IMPL_THREADSAFE_ISUPPORTS2(MyProgressEventSink,
                              nsIProgressEventSink,
                              nsIInterfaceRequestor)

////////////////////////////////////////////////////////////////////////////////

class MyListener : public nsIStreamListener {
public:
    NS_DECL_ISUPPORTS

    NS_IMETHOD OnStartRequest(nsIRequest *request, nsISupports *ctxt) {
        PRINTF(("starting\n"));
        return NS_OK;
    }

    NS_IMETHOD OnStopRequest(nsIRequest *request, nsISupports *ctxt, 
                             nsresult aStatus) {
        PRINTF(("ending status=%0x total=%d\n", aStatus, mTotal));
        if (--mStopCount == 0)
            gDone = PR_TRUE;
        return NS_OK;
    }

    NS_IMETHOD OnDataAvailable(nsIRequest *request, nsISupports *ctxt, 
                               nsIInputStream *inStr, PRUint32 sourceOffset, 
                               PRUint32 count) {
        PRINTF(("receiving %d bytes\n", count));
        char buf[256];
        PRUint32 writeCount, givenCount=count;
        nsresult rv;
        while (count > 0) {
            PRUint32 amt = PR_MIN(count, 256);
            PRUint32 readCount;
            rv = inStr->Read(buf, amt, &readCount);
            if (NS_FAILED(rv)) return rv;
            NS_ASSERTION(readCount != 0, "premature EOF");
            rv = mOut->Write(buf, readCount, &writeCount);
            if (NS_FAILED(rv)) return rv;
            NS_ASSERTION(writeCount == readCount, "failed to write all the data");
            count -= readCount;
            mTotal += readCount;
        }
        PRINTF(("done reading data [read %u bytes]\n", givenCount - count));
        //PRINTF("sleeping for 100 ticks\n");FLUSH();
        //PR_Sleep(100);
        return NS_OK;
    }
    
    MyListener(PRUint32 stopCount = 1) : mTotal(0), mStopCount(stopCount) {
        NS_INIT_REFCNT();
    }
    
    nsresult Init(const char* origFile) {
        nsresult rv;
        nsCOMPtr<nsILocalFile> file;
        rv = NS_NewLocalFile(origFile, PR_FALSE, getter_AddRefs(file));
        if (NS_FAILED(rv)) return rv;
        char* name;
        rv = file->GetLeafName(&name);
        if (NS_FAILED(rv)) return rv;
        nsCAutoString str(name);
        nsMemory::Free(name);
        str.Append(".bak");
        rv = file->SetLeafName(str);
        if (NS_FAILED(rv)) return rv;
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

NS_IMPL_THREADSAFE_ISUPPORTS2(MyListener, nsIStreamListener, nsIRequestObserver);

////////////////////////////////////////////////////////////////////////////////

nsresult
TestAsyncRead(const char* fileName, PRUint32 offset, PRInt32 length)
{
    nsresult rv;

    PRINTF(("TestAsyncRead\n"));

    nsCOMPtr<nsIFileTransportService> fts = 
             do_GetService(kFileTransportServiceCID, &rv);
    if (NS_FAILED(rv)) return rv;

    nsITransport* fileTrans;
    nsCOMPtr<nsILocalFile> file;
    rv = NS_NewLocalFile(fileName, PR_FALSE, getter_AddRefs(file));
    if (NS_FAILED(rv)) return rv;
    rv = fts->CreateTransport(file, PR_RDONLY, 0, &fileTrans);
    if (NS_FAILED(rv)) return rv;

    MyListener* listener = new MyListener();
    if (listener == nsnull)
        return NS_ERROR_OUT_OF_MEMORY;
    NS_ADDREF(listener);
    rv = listener->Init(fileName);
    if (NS_FAILED(rv)) return rv;

    MyProgressEventSink* progressSink = new MyProgressEventSink();
    if (progressSink == nsnull)
        return NS_ERROR_OUT_OF_MEMORY;
    NS_ADDREF(progressSink);
    rv = fileTrans->SetNotificationCallbacks(progressSink, PR_FALSE);
    if (NS_FAILED(rv)) return rv;

    gDone = PR_FALSE;
    nsCOMPtr<nsIRequest> request;
    rv = fileTrans->AsyncRead(listener, nsnull, offset, length, 0, getter_AddRefs(request));
    if (NS_FAILED(rv)) return rv;

    while (!gDone) {
        PLEvent* event;
        rv = gEventQ->WaitForEvent(&event);
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

    PRINTF(("TestAsyncWrite\n"));

    nsCOMPtr<nsIFileTransportService> fts = 
             do_GetService(kFileTransportServiceCID, &rv);
    if (NS_FAILED(rv)) return rv;

    nsCAutoString outFile(fileName);
    outFile.Append(".out");
    nsITransport* fileTrans;
    nsCOMPtr<nsILocalFile> file;
    rv = NS_NewLocalFile(outFile, PR_FALSE, getter_AddRefs(file));
    if (NS_FAILED(rv)) return rv;
    rv = fts->CreateTransport(file,
                              PR_CREATE_FILE | PR_WRONLY | PR_TRUNCATE,
                              0664, &fileTrans);
    if (NS_FAILED(rv)) return rv;

    MyListener* listener = new MyListener();
    if (listener == nsnull)
        return NS_ERROR_OUT_OF_MEMORY;
    NS_ADDREF(listener);
    rv = listener->Init(outFile);
    if (NS_FAILED(rv)) return rv;

    MyProgressEventSink* progressSink = new MyProgressEventSink();
    if (progressSink == nsnull)
        return NS_ERROR_OUT_OF_MEMORY;
    NS_ADDREF(progressSink);
    rv = fileTrans->SetNotificationCallbacks(progressSink, PR_FALSE);
    if (NS_FAILED(rv)) return rv;

    nsCOMPtr<nsILocalFile> f;
    rv = NS_NewLocalFile(fileName, PR_FALSE, getter_AddRefs(f));
    if (NS_FAILED(rv)) return rv;
    nsCOMPtr<nsIInputStream> inStr;
    rv = NS_NewLocalFileInputStream(getter_AddRefs(inStr), f);
    if (NS_FAILED(rv)) return rv;

    gDone = PR_FALSE;
    nsCOMPtr<nsIRequest> request;
    rv = NS_AsyncWriteFromStream(getter_AddRefs(request), fileTrans, inStr, offset, length, 0, listener);
    if (NS_FAILED(rv)) return rv;

    while (!gDone) {
        PLEvent* event;
        rv = gEventQ->WaitForEvent(&event);
        if (NS_FAILED(rv)) return rv;
        rv = gEventQ->HandleEvent(event);
        if (NS_FAILED(rv)) return rv;
    }

    NS_RELEASE(listener);
    NS_RELEASE(fileTrans);
    return NS_OK;
}

////////////////////////////////////////////////////////////////////////////////

#if 0
class MyOpenObserver : public nsIRequestObserver
{
public:
    NS_DECL_ISUPPORTS

    NS_IMETHOD OnStartRequest(nsIRequest *request, nsISupports *ctxt) {
        nsresult rv;
        char* contentType;
        nsCOMPtr<nsIChannel> channel = do_QueryInterface(request);
        rv = channel->GetContentType(&contentType);
        if (NS_FAILED(rv)) return rv;
        PRInt32 length;
        rv = cr->GetContentLength(&length);
        if (NS_FAILED(rv)) return rv;
        PRINTF(("stream opened: content type = %s, length = %d\n",
                contentType, length));
        nsCRT::free(contentType);
        return NS_OK;
    }

    NS_IMETHOD OnStopRequest(nsIRequest *request, nsISupports *ctxt,
                             nsresult aStatus) {
        PRINTF(("stream closed: status %x\n", aStatus));
        return NS_OK;
    }

    MyOpenObserver() { NS_INIT_REFCNT(); }
    virtual ~MyOpenObserver() {}
};

NS_IMPL_ISUPPORTS1(MyOpenObserver, nsIRequestObserver);
#endif

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

#if defined(PR_LOGGING)
    gTestFileTransportLog = PR_NewLogModule("TestFileTransport");
#endif

    if (argc < 2) {
        printf("usage: %s <file-to-read>\n", argv[0]);
        return -1;
    }
    char* fileName = argv[1];

    rv = NS_AutoregisterComponents();
    if (NS_FAILED(rv)) return rv;

    nsCOMPtr<nsIEventQueueService> eventQService = 
             do_GetService(kEventQueueServiceCID, &rv);
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

    NS_RELEASE(gEventQ);
    return NS_OK;
}
