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
#include "nsIFileStream.h"
#include "nsFileSpec.h"
#include "nsIByteBufferInputStream.h"
#include <stdio.h>

static NS_DEFINE_IID(kISupportsIID, NS_ISUPPORTS_IID);
static NS_DEFINE_CID(kFileTransportServiceCID, NS_FILETRANSPORTSERVICE_CID);

PRIntervalTime gDuration = 0;

class nsReader : public nsIRunnable, public nsIStreamListener {
public:
    NS_DECL_ISUPPORTS

    NS_IMETHOD Run() {
        printf("waiting\n");
        PR_CEnterMonitor(this);
        if (mEventQueue == nsnull)
            PR_CWait(this, PR_INTERVAL_NO_TIMEOUT);
        PR_CExitMonitor(this);

        printf("running\n");
        PL_EventLoop(mEventQueue);
        printf("quitting\n");
        return NS_OK;
    }

    nsReader() : mEventQueue(nsnull), mStartTime(0) {
        NS_INIT_REFCNT();
    }

    nsresult Init(nsIThread* thread) {
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

    NS_IMETHOD OnStartBinding(nsIProtocolConnection* connection) {
//        printf("start binding\n"); 
        mStartTime = PR_IntervalNow();
        return NS_OK;
    }

    NS_IMETHOD OnDataAvailable(nsIProtocolConnection* connection,
                               nsIInputStream *aIStream, 
                               PRUint32 aLength) {
        char buf[1025];
        while (aLength > 0) {
            PRUint32 amt;
            nsresult rv = aIStream->Read(buf, 1024, &amt);
//            buf[amt] = '\0';
//            printf(buf);
            aLength -= amt;
        }
        return NS_OK;
    }

    NS_IMETHOD OnStopBinding(nsIProtocolConnection* connection,
                             nsresult aStatus,
                             nsIString* aMsg) {
        if (aStatus == NS_OK) {
            PRIntervalTime endTime = PR_IntervalNow();
            gDuration += (endTime - mStartTime);
        }
        else {
            printf("stop binding, %d\n", aStatus);
        }
        return NS_OK;
    }

protected:
    PLEventQueue*       mEventQueue;
    PRIntervalTime      mStartTime;
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

void
SerialReadTest()
{
    nsresult rv;
    PRIntervalTime startTime = PR_IntervalNow();
    for (PRUint32 i = 0; i < 100; i++) {
        nsISupports* fs;
        nsIInputStream* fileStr = nsnull;
        nsIByteBufferInputStream* bufStr = nsnull;
        nsFileSpec spec("test.txt");

        rv = NS_NewTypicalInputFileStream(&fs, spec);
        NS_ASSERTION(NS_SUCCEEDED(rv), "open failed");
        rv = fs->QueryInterface(nsIInputStream::GetIID(), (void**)&fileStr);
        NS_RELEASE(fs);
        NS_ASSERTION(NS_SUCCEEDED(rv), "QI failed");

#       define NS_FILE_TRANSPORT_BUFFER_SIZE (4*1024)

        rv = NS_NewByteBufferInputStream(NS_FILE_TRANSPORT_BUFFER_SIZE, &bufStr);
        NS_ASSERTION(NS_SUCCEEDED(rv), "new byte buffer input stream failed");

        while (PR_TRUE) {
            PRUint32 amt;
            rv = bufStr->Fill(fileStr, &amt);
            if (rv == NS_BASE_STREAM_EOF || amt == 0) {
                rv = NS_OK;
                break;
            }
            NS_ASSERTION(NS_SUCCEEDED(rv), "fill failed");

            char buffer[1025];
            while (amt > 0) {
                PRUint32 readAmt;
                rv = bufStr->Read(buffer, 1024, &readAmt);
                NS_ASSERTION(NS_SUCCEEDED(rv), "read failed");

//                buffer[readAmt] = '\0';
//                printf(buffer);

                amt -= readAmt;
            }
        }
        NS_IF_RELEASE(bufStr);
        NS_IF_RELEASE(fileStr);
        NS_ASSERTION(NS_SUCCEEDED(rv), "close failed");
    }
    PRIntervalTime endTime = PR_IntervalNow();
    printf("duration %d ms\n", PR_IntervalToMilliseconds(endTime - startTime));
}

int
main()
{
    nsresult rv;

    // XXX why do I have to do this?!
    rv = nsComponentManager::AutoRegister(nsIComponentManager::NS_Startup,
                                          "components");
    if (NS_FAILED(rv)) return rv;

    SerialReadTest();

    NS_WITH_SERVICE(nsIFileTransportService, fts, kFileTransportServiceCID, &rv);
    if (NS_FAILED(rv)) return rv;

    nsReader* reader = new nsReader();
    nsIThread* readerThread;
    rv = NS_NewThread(&readerThread, reader);
    if (NS_FAILED(rv)) return rv;

    reader->Init(readerThread);

    nsIStreamListener* listener;
    reader->QueryInterface(nsIStreamListener::GetIID(), (void**)&listener);

    for (PRUint32 i = 0; i < 100; i++) {
        nsITransport* trans;
        rv = fts->AsyncRead(reader->GetEventQueue(), 
                            (nsIProtocolConnection*)listener,   // protocol connection (cheating)
                            listener,
                            "test.txt",
                            &trans);
    }

    fts->Shutdown();
    readerThread->Interrupt();
    readerThread->Join();

    printf("duration %d ms\n", PR_IntervalToMilliseconds(gDuration));

    return 0;
}
