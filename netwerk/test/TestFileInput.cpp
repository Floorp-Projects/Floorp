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
#include "nsIStreamListener.h"
#include "nsIServiceManager.h"
#include "nsIInputStream.h"
#include "nsIEventQueue.h"
#include "nsIEventQueueService.h"
#include "prinrval.h"
#include "prmon.h"
#include "prcmon.h"
#include "prio.h"
#include "nsIFileStreams.h"
#include "nsILocalFile.h"
#include "nsNetUtil.h"
#include "nsIPipe.h"
#include "nsIOutputStream.h"
#include "nsIInputStream.h"
#include "nsIRunnable.h"
#include "nsIThread.h"
#include "nsISupportsArray.h"
#include "nsIChannel.h"
#include "nsCOMPtr.h"
#include <stdio.h>
#include "nsInt64.h"

static NS_DEFINE_CID(kFileTransportServiceCID, NS_FILETRANSPORTSERVICE_CID);
static NS_DEFINE_CID(kEventQueueServiceCID, NS_EVENTQUEUESERVICE_CID);

PRIntervalTime gDuration = 0;
PRUint32 gVolume = 0;

class nsReader : public nsIRunnable, public nsIStreamListener {
public:
    NS_DECL_ISUPPORTS

    NS_IMETHOD Run() {
        printf("waiting\n");
        if (!mMonitor)
            return NS_ERROR_OUT_OF_MEMORY;
        PR_EnterMonitor(mMonitor);
        if (mEventQueue == nsnull) 
            PR_CWait(this, PR_INTERVAL_NO_TIMEOUT);

        PR_ExitMonitor(mMonitor);

        printf("running\n");

        mEventQueue->EventLoop();

        printf("quitting\n");
        return NS_OK;
    }

    nsReader()
        : mEventQueue(nsnull), mStartTime(0), mThread(nsnull), mBytesRead(0)
    {
        NS_INIT_REFCNT();
        mMonitor = PR_NewMonitor();
    }

    virtual ~nsReader() {
        NS_IF_RELEASE(mThread);
        NS_IF_RELEASE(mEventQueue);
        PR_DestroyMonitor(mMonitor);
    }

    nsresult Init(nsIThread* thread) {
        nsresult rv;
        mThread = thread;
        NS_ADDREF(mThread);
        PRThread* prthread;
        thread->GetPRThread(&prthread);
        PR_EnterMonitor(mMonitor);
        nsCOMPtr<nsIEventQueueService> eventQService = 
                 do_GetService(kEventQueueServiceCID, &rv);
        if (NS_SUCCEEDED(rv)) {
          rv = eventQService->CreateThreadEventQueue();

          if (NS_FAILED(rv)) return rv;
  
          rv = eventQService->GetThreadEventQueue(NS_CURRENT_THREAD, &mEventQueue);
        }

        if (NS_FAILED(rv)) return rv;

        // wake up event loop
        PR_Notify(mMonitor);
        PR_ExitMonitor(mMonitor);
        
        return NS_OK;
    }

    NS_IMETHOD OnStartRequest(nsIRequest *request,
                              nsISupports* context) {
        PR_EnterMonitor(mMonitor);
        printf("start binding\n"); 
        mStartTime = PR_IntervalNow();
        PR_ExitMonitor(mMonitor);
        return NS_OK;
    }

    NS_IMETHOD OnDataAvailable(nsIRequest *request, 
                               nsISupports* context,
                               nsIInputStream *aIStream, 
                               PRUint32 aSourceOffset,
                               PRUint32 aLength) {
        PR_EnterMonitor(mMonitor);
        char buf[1025];
        while (aLength > 0) {
            PRUint32 amt;
            /*nsresult rv = */aIStream->Read(buf, 1024, &amt);
            if (amt == 0) break;
            buf[amt] = '\0';
            printf(buf);
            aLength -= amt;
            mBytesRead += amt;
            gVolume += amt;
        }
        PR_ExitMonitor(mMonitor);
        return NS_OK;
    }

    NS_IMETHOD OnStopRequest(nsIRequest *request, nsISupports* context,
                             nsresult aStatus) {
        nsresult rv;
        PR_EnterMonitor(mMonitor);
        PRIntervalTime endTime = PR_IntervalNow();
        gDuration += (endTime - mStartTime);
        printf("stop binding, %d\n", aStatus);
        if (NS_FAILED(aStatus)) printf("channel failed.\n");
        printf("read %d bytes\n", mBytesRead);
        PR_ExitMonitor(mMonitor);

        // get me out of my event loop
        rv = mThread->Interrupt();
        if (NS_FAILED(rv)) return rv;

        return rv;
    }

protected:
    nsIEventQueue*       mEventQueue;
    PRIntervalTime      mStartTime;
    nsIThread*          mThread;
    PRUint32            mBytesRead;

private:
    PRMonitor*          mMonitor;
};

NS_IMPL_ADDREF(nsReader);
NS_IMPL_RELEASE(nsReader);

NS_IMETHODIMP
nsReader::QueryInterface(const nsIID& aIID, void* *aInstancePtr)
{
    if (NULL == aInstancePtr) {
        return NS_ERROR_NULL_POINTER; 
    } 
    if (aIID.Equals(NS_GET_IID(nsIRunnable)) ||
        aIID.Equals(NS_GET_IID(nsISupports))) {
        *aInstancePtr = NS_STATIC_CAST(nsIRunnable*, this); 
        NS_ADDREF_THIS(); 
        return NS_OK; 
    } 
    if (aIID.Equals(NS_GET_IID(nsIStreamListener))) {
        *aInstancePtr = NS_STATIC_CAST(nsIStreamListener*, this); 
        NS_ADDREF_THIS(); 
        return NS_OK; 
    } 
    return NS_NOINTERFACE; 
}


nsresult
Simulated_nsFileTransport_Run(nsReader* reader, const char* path)
{
    // duplicate the work of nsFileTransport::Run here

#define NS_FILE_TRANSPORT_BUFFER_SIZE (4*1024)

    nsresult rv;
    nsCOMPtr<nsIInputStream> fileStr;
    nsIInputStream* bufStr = nsnull;
    PRUint32 sourceOffset = 0;
    nsCOMPtr<nsIOutputStream> out;
    nsCOMPtr<nsILocalFile> file;

    rv = reader->OnStartRequest(nsnull, nsnull);
    if (NS_FAILED(rv)) goto done;       // XXX should this abort the transfer?

    rv = NS_NewLocalFile(path, PR_FALSE, getter_AddRefs(file));
    if (NS_FAILED(rv)) goto done;

    rv = NS_NewLocalFileInputStream(getter_AddRefs(fileStr), file);
    if (NS_FAILED(rv)) goto done;

    rv = NS_NewPipe(&bufStr, getter_AddRefs(out),
                    NS_FILE_TRANSPORT_BUFFER_SIZE,
                    NS_FILE_TRANSPORT_BUFFER_SIZE);
    if (NS_FAILED(rv)) goto done;

    /*
    if ( spec.GetFileSize() == 0) goto done;
    */

    while (PR_TRUE) {
        PRUint32 amt;
		    /* id'l change to FillFrom... */
        PRInt64 size;
        rv = file->GetFileSize(&size);
        if (NS_FAILED(rv)) break;
        rv = out->WriteFrom(fileStr, nsInt64(size), &amt);
        if (NS_FAILED(rv) || amt == 0) break;

        rv = reader->OnDataAvailable(nsnull, nsnull, bufStr, sourceOffset, amt);
        if (NS_FAILED(rv)) break;

        sourceOffset += amt;
    }

  done:
    NS_IF_RELEASE(bufStr);

    rv = reader->OnStopRequest(nsnull, nsnull, rv);
    return rv;
}

void
SerialReadTest(char* dirName)
{
    nsresult rv;
    PRStatus status;

    PRDir* dir = PR_OpenDir(dirName);
    NS_ASSERTION(dir, "bad dir");

    nsISupportsArray* threads;
    rv = NS_NewISupportsArray(&threads);
    NS_ASSERTION(NS_SUCCEEDED(rv), "NS_NewISupportsArray failed");

    PRIntervalTime startTime = PR_IntervalNow();
    PRDirEntry* entry;
    while ((entry = PR_ReadDir(dir, PR_SKIP_BOTH)) != nsnull) {
        nsFileSpec spec(dirName);
        spec += entry->name;

        nsReader* reader = new nsReader();
        NS_ASSERTION(reader, "out of memory");
        NS_ADDREF(reader);

        nsIThread* readerThread;
        rv = NS_NewThread(&readerThread, reader, 0, PR_JOINABLE_THREAD);
        NS_ASSERTION(NS_SUCCEEDED(rv), "new thread failed");

        rv = reader->Init(readerThread);
        NS_ASSERTION(NS_SUCCEEDED(rv), "init failed");

        nsIStreamListener* listener;
        reader->QueryInterface(NS_GET_IID(nsIStreamListener), (void**)&listener);
        NS_ASSERTION(listener, "QI failed");

        rv = Simulated_nsFileTransport_Run(reader, spec);
        NS_ASSERTION(NS_SUCCEEDED(rv), "Simulated_nsFileTransport_Run failed");

        // the reader thread will hang on to these objects until it quits
        NS_RELEASE(listener);
        NS_RELEASE(readerThread);
        NS_RELEASE(reader);
    }
    PRIntervalTime endTime = PR_IntervalNow();
    printf("duration %d ms, volume %d\n",
           PR_IntervalToMilliseconds(endTime - startTime),
           gVolume);
    gVolume = 0;

    // now that we've forked all the async requests, wait until they're done
    PRUint32 threadCount;
    rv = threads->Count(&threadCount);
    for (PRUint32 i = 0; i < threadCount; i++) {
        nsIThread* thread = (nsIThread*)threads->ElementAt(i);
        thread->Join();
        NS_RELEASE(thread);
    }
    NS_RELEASE(threads);

    status = PR_CloseDir(dir);
    NS_ASSERTION(status == PR_SUCCESS, "can't close dir");
}

void
ParallelReadTest(char* dirName, nsIFileTransportService* fts)
{
    nsresult rv;
    PRStatus status;

    PRDir* dir = PR_OpenDir(dirName);
    NS_ASSERTION(dir, "bad dir");

    nsISupportsArray* threads;
    rv = NS_NewISupportsArray(&threads);
    NS_ASSERTION(NS_SUCCEEDED(rv), "NS_NewISupportsArray failed");

    PRDirEntry* entry;
    while ((entry = PR_ReadDir(dir, PR_SKIP_BOTH)) != nsnull) {
        nsCOMPtr<nsILocalFile> file;
        rv = NS_NewLocalFile(dirName, PR_FALSE, getter_AddRefs(file));
        NS_ASSERTION(NS_SUCCEEDED(rv), "NS_NewLocalFile failed");

        rv = file->Append(entry->name);
        NS_ASSERTION(NS_SUCCEEDED(rv), "AppendPath failed");

        nsReader* reader = new nsReader();
        NS_ASSERTION(reader, "out of memory");
        NS_ADDREF(reader);

        nsIThread* readerThread;

        rv = NS_NewThread(&readerThread, reader, 0, PR_JOINABLE_THREAD);

        NS_ASSERTION(NS_SUCCEEDED(rv), "new thread failed");

        rv = reader->Init(readerThread);
        NS_ASSERTION(NS_SUCCEEDED(rv), "init failed");

        nsIStreamListener* listener;
        reader->QueryInterface(NS_GET_IID(nsIStreamListener), (void**)&listener);
        NS_ASSERTION(listener, "QI failed");
    
        nsITransport* trans;
        rv = fts->CreateTransport(file, PR_RDONLY, 0, &trans);
        NS_ASSERTION(NS_SUCCEEDED(rv), "create failed");
        nsCOMPtr<nsIRequest> request;
        rv = trans->AsyncRead(nsnull, listener, 0, -1, 0, getter_AddRefs(request));
        NS_ASSERTION(NS_SUCCEEDED(rv), "AsyncRead failed");

        // the reader thread will hang on to these objects until it quits
        NS_RELEASE(trans);
        NS_RELEASE(listener);
        NS_RELEASE(reader);
        rv = threads->AppendElement(readerThread) ? NS_OK : NS_ERROR_FAILURE;  // XXX this method incorrectly returns a bool
        NS_ASSERTION(NS_SUCCEEDED(rv), "AppendElement failed");
        NS_RELEASE(readerThread);
    }

    // now that we've forked all the async requests, wait until they're done
    PRUint32 threadCount;
    rv = threads->Count(&threadCount);
    for (PRUint32 i = 0; i < threadCount; i++) {
        nsIThread* thread = (nsIThread*)threads->ElementAt(i);
        thread->Join();
        NS_RELEASE(thread);
    }
    NS_RELEASE(threads);

    status = PR_CloseDir(dir);
    NS_ASSERTION(status == PR_SUCCESS, "can't close dir");
}

nsresult NS_AutoregisterComponents()
{
  nsresult rv = nsComponentManager::AutoRegister(nsIComponentManager::NS_Startup, NULL /* default */);
  return rv;
}

int
main(int argc, char* argv[])
{
    nsresult rv;

    if (argc < 2) {
        printf("usage: %s <dir-to-read-all-files-from>\n", argv[0]);
        return -1;
    }
    char* dirName = argv[1];

    rv = NS_AutoregisterComponents();
    if (NS_FAILED(rv)) return rv;

    nsCOMPtr<nsIFileTransportService> fts = 
             do_GetService(kFileTransportServiceCID, &rv);
    if (NS_FAILED(rv)) return rv;

    SerialReadTest(dirName);

    //ParallelReadTest(dirName, fts);

    fts->ProcessPendingRequests();

    printf("duration %d ms, volume %d\n",
           PR_IntervalToMilliseconds(gDuration),
           gVolume);
    gVolume = 0;

    return 0;
}

