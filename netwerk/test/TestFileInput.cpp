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
#include "nsIStreamListener.h"
#include "nsIServiceManager.h"
#include "nsIInputStream.h"
#include "nsIThread.h"
#include "plevent.h"
#include "prinrval.h"
#include "prcmon.h"
#include "prio.h"
#include "nsIFileStream.h"
#include "nsFileSpec.h"
#include "nsIByteBufferInputStream.h"
#include "nsIThread.h"
#include "nsISupportsArray.h"
#include "nsITransport.h"
#include <stdio.h>

static NS_DEFINE_IID(kISupportsIID, NS_ISUPPORTS_IID);
static NS_DEFINE_CID(kFileTransportServiceCID, NS_FILETRANSPORTSERVICE_CID);

PRIntervalTime gDuration = 0;
PRUint32 gVolume = 0;

class nsReader : public nsIRunnable, public nsIStreamListener {
public:
    NS_DECL_ISUPPORTS

    NS_IMETHOD Run() {
//        printf("waiting\n");
        PR_CEnterMonitor(this);
        if (mEventQueue == nsnull)
            PR_CWait(this, PR_INTERVAL_NO_TIMEOUT);
        PR_CExitMonitor(this);

//        printf("running\n");
        PL_EventLoop(mEventQueue);
//        printf("quitting\n");
        return NS_OK;
    }

    nsReader()
        : mEventQueue(nsnull), mStartTime(0), mThread(nsnull)
    {
        NS_INIT_REFCNT();
    }

    virtual ~nsReader() {
        NS_IF_RELEASE(mThread);
    }

    nsresult Init(nsIThread* thread) {
        mThread = thread;
        NS_ADDREF(mThread);
        PRThread* prthread;
        thread->GetPRThread(&prthread);
        PR_CEnterMonitor(this);
        mEventQueue = PL_CreateEventQueue("runner event loop",
                                          prthread);
        // wake up event loop
        PR_CNotify(this);
        PR_CExitMonitor(this);
        
        return NS_OK;
    }

    PLEventQueue* GetEventQueue() { return mEventQueue; }

    NS_IMETHOD OnStartBinding(nsISupports* context) {
        PR_CEnterMonitor(this);
//        printf("start binding\n"); 
        mStartTime = PR_IntervalNow();
        PR_CExitMonitor(this);
        return NS_OK;
    }

    NS_IMETHOD OnDataAvailable(nsISupports* context,
                               nsIInputStream *aIStream, 
                               PRUint32 aLength) {
        PR_CEnterMonitor(this);
        char buf[1025];
        while (aLength > 0) {
            PRUint32 amt;
            nsresult rv = aIStream->Read(buf, 1024, &amt);
//            buf[amt] = '\0';
//            printf(buf);
            aLength -= amt;
            gVolume += amt;
        }
        PR_CExitMonitor(this);
        return NS_OK;
    }

    NS_IMETHOD OnStopBinding(nsISupports* context,
                             nsresult aStatus,
                             nsIString* aMsg) {
        nsresult rv;
        PR_CEnterMonitor(this);
        if (aStatus == NS_OK) {
            PRIntervalTime endTime = PR_IntervalNow();
            gDuration += (endTime - mStartTime);
        }
        else {
            printf("stop binding, %d\n", aStatus);
        }
        PR_CExitMonitor(this);

        // get me out of my event loop
        rv = mThread->Interrupt();
        if (NS_FAILED(rv)) return rv;

        return rv;
    }

protected:
    PLEventQueue*       mEventQueue;
    PRIntervalTime      mStartTime;
    nsIThread*          mThread;
};

NS_IMPL_ADDREF(nsReader);
NS_IMPL_RELEASE(nsReader);

NS_IMETHODIMP
nsReader::QueryInterface(const nsIID& aIID, void* *aInstancePtr)
{
    if (NULL == aInstancePtr) {
        return NS_ERROR_NULL_POINTER; 
    } 
    if (aIID.Equals(nsIRunnable::GetIID()) ||
        aIID.Equals(kISupportsIID)) {
        *aInstancePtr = NS_STATIC_CAST(nsIRunnable*, this); 
        NS_ADDREF_THIS(); 
        return NS_OK; 
    } 
    if (aIID.Equals(nsIStreamListener::GetIID())) {
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
    nsISupports* fs;
    nsIInputStream* fileStr = nsnull;
    nsIByteBufferInputStream* bufStr = nsnull;
    nsFileSpec spec(path);

    rv = reader->OnStartBinding(nsnull);
    if (NS_FAILED(rv)) goto done;       // XXX should this abort the transfer?

    rv = NS_NewTypicalInputFileStream(&fs, spec);
    if (NS_FAILED(rv)) goto done;

    rv = fs->QueryInterface(nsIInputStream::GetIID(), (void**)&fileStr);
    NS_RELEASE(fs);
    if (NS_FAILED(rv)) goto done;

    rv = NS_NewByteBufferInputStream(NS_FILE_TRANSPORT_BUFFER_SIZE, &bufStr);
    if (NS_FAILED(rv)) goto done;

    while (PR_TRUE) {
        PRUint32 amt;
        rv = bufStr->Fill(fileStr, &amt);
        if (rv == NS_BASE_STREAM_EOF) {
            rv = NS_OK;
            break;
        }
        if (NS_FAILED(rv)) break;

        rv = reader->OnDataAvailable(nsnull, bufStr, amt);
        if (NS_FAILED(rv)) break;
    }

  done:
    NS_IF_RELEASE(bufStr);
    NS_IF_RELEASE(fileStr);

    rv = reader->OnStopBinding(nsnull, rv, nsnull);
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
        rv = NS_NewThread(&readerThread, reader);
        NS_ASSERTION(NS_SUCCEEDED(rv), "new thread failed");

        rv = reader->Init(readerThread);
        NS_ASSERTION(NS_SUCCEEDED(rv), "init failed");

        nsIStreamListener* listener;
        reader->QueryInterface(nsIStreamListener::GetIID(), (void**)&listener);
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
    PRUint32 threadCount = threads->Count();
    for (PRUint32 i = 0; i < threadCount; i++) {
        nsIThread* thread = (nsIThread*)(*threads)[i];
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
        nsFileSpec spec(dirName);
        spec += entry->name;

        nsReader* reader = new nsReader();
        NS_ASSERTION(reader, "out of memory");
        NS_ADDREF(reader);

        nsIThread* readerThread;
        rv = NS_NewThread(&readerThread, reader);
        NS_ASSERTION(NS_SUCCEEDED(rv), "new thread failed");

        rv = reader->Init(readerThread);
        NS_ASSERTION(NS_SUCCEEDED(rv), "init failed");

        nsIStreamListener* listener;
        reader->QueryInterface(nsIStreamListener::GetIID(), (void**)&listener);
        NS_ASSERTION(listener, "QI failed");
    
        nsITransport* trans;
        rv = fts->AsyncRead(reader->GetEventQueue(), 
                            nsnull, listener, spec, &trans);
        NS_ASSERTION(NS_SUCCEEDED(rv), "AsyncRead failed");

        // the reader thread will hang on to these objects until it quits
        NS_RELEASE(trans);
        NS_RELEASE(listener);
        NS_RELEASE(reader);
        threads->AppendElement(readerThread);
        NS_RELEASE(readerThread);
    }

    // now that we've forked all the async requests, wait until they're done
    PRUint32 threadCount = threads->Count();
    for (PRUint32 i = 0; i < threadCount; i++) {
        nsIThread* thread = (nsIThread*)(*threads)[i];
        thread->Join();
        NS_RELEASE(thread);
    }
    NS_RELEASE(threads);

    status = PR_CloseDir(dir);
    NS_ASSERTION(status == PR_SUCCESS, "can't close dir");
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

    // XXX why do I have to do this?!
    rv = nsComponentManager::AutoRegister(nsIComponentManager::NS_Startup,
                                          "components");
    if (NS_FAILED(rv)) return rv;

    NS_WITH_SERVICE(nsIFileTransportService, fts, kFileTransportServiceCID, &rv);
    if (NS_FAILED(rv)) return rv;

    SerialReadTest(dirName);
    ParallelReadTest(dirName, fts);

    fts->Shutdown();

    printf("duration %d ms, volume %d\n",
           PR_IntervalToMilliseconds(gDuration),
           gVolume);
    gVolume = 0;

    return 0;
}

