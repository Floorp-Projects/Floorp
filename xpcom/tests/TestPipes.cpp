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

#include "nsIThread.h"
#if 0   // obsolete old implementation
#include "nsIByteBufferInputStream.h"
#endif
#include "nsIBuffer.h"
#include "nsIBufferInputStream.h"
#include "nsIBufferOutputStream.h"
#include "nsIServiceManager.h"
#include "prprf.h"
#include "prinrval.h"
#include "plstr.h"
#include "nsCRT.h"
#include <stdio.h>
#include "nsPipe2.h"    // new implementation
#include "nsAutoLock.h"
#include <stdlib.h>     // for rand

#define KEY             0xa7
#define ITERATIONS      33333
char kTestPattern[] = "My hovercraft is full of eels.\n";

PRBool gTrace = PR_FALSE;

class nsReceiver : public nsIRunnable {
public:
    NS_DECL_ISUPPORTS

    NS_IMETHOD Run() {
        nsresult rv;
        char buf[101];
        PRUint32 count;
        PRIntervalTime start = PR_IntervalNow();
        while (PR_TRUE) {
            rv = mIn->Read(buf, 100, &count);
            if (rv == NS_BASE_STREAM_EOF) {
//                printf("EOF count = %d\n", mCount);
                rv = NS_OK;
                break;
            }
            if (NS_FAILED(rv)) {
                printf("read failed\n");
                break;
            }

            if (gTrace) {
                printf("read:  ");
                buf[count] = '\0';
                printf(buf);
                printf("\n");
            }
            mCount += count;
        }
        PRIntervalTime end = PR_IntervalNow();
        printf("read  %d bytes, time = %dms\n", mCount,
               PR_IntervalToMilliseconds(end - start));
        return rv;
    }

    nsReceiver(nsIInputStream* in) : mIn(in), mCount(0) {
        NS_INIT_REFCNT();
        NS_ADDREF(in);
    }

    virtual ~nsReceiver() {
        NS_RELEASE(mIn);
    }

    PRUint32 GetBytesRead() { return mCount; }

protected:
    nsIInputStream*     mIn;
    PRUint32            mCount;
};

NS_IMPL_ISUPPORTS(nsReceiver, nsIRunnable::GetIID());

nsresult
TestPipe(nsIInputStream* in, nsIOutputStream* out)
{
    nsresult rv;
    nsIThread* thread;
    nsReceiver* receiver = new nsReceiver(in);
    if (receiver == nsnull) return NS_ERROR_OUT_OF_MEMORY;
    NS_ADDREF(receiver);

    rv = NS_NewThread(&thread, receiver);
    if (NS_FAILED(rv)) return rv;

    PRUint32 total = 0;
    PRIntervalTime start = PR_IntervalNow();
    for (PRUint32 i = 0; i < ITERATIONS; i++) {
        PRUint32 writeCount;
        char* buf = PR_smprintf("%d %s", i, kTestPattern);
        rv = out->Write(buf, nsCRT::strlen(buf), &writeCount);
        if (gTrace) {
            printf("wrote: ");
            for (PRUint32 j = 0; j < writeCount; j++) {
                putc(buf[j], stdout);
            }
            printf("\n");
        }
        PR_smprintf_free(buf);
        if (NS_FAILED(rv)) return rv;
        total += writeCount;
    }
    rv = out->Close();
    if (NS_FAILED(rv)) return rv;

    PRIntervalTime end = PR_IntervalNow();

    thread->Join();

    printf("write %d bytes, time = %dms\n", total,
           PR_IntervalToMilliseconds(end - start));
    NS_ASSERTION(receiver->GetBytesRead() == total, "didn't read everything");

    NS_RELEASE(thread);
    NS_RELEASE(receiver);

    return NS_OK;
}

////////////////////////////////////////////////////////////////////////////////

class nsShortReader : public nsIRunnable {
public:
    NS_DECL_ISUPPORTS

    NS_IMETHOD Run() {
        nsresult rv;
        char buf[101];
        PRUint32 count;
        while (PR_TRUE) {
            rv = mIn->Read(buf, 100, &count);
            if (rv == NS_BASE_STREAM_EOF) {
                rv = NS_OK;
                break;
            }
            if (NS_FAILED(rv)) {
                printf("read failed\n");
                break;
            }
            buf[count] = '\0';
            if (gTrace)
                printf("read %d bytes: %s\n", count, buf);
            Received(count);
        }
        return rv;
    }

    nsShortReader(nsIInputStream* in) : mIn(in), mReceived(0) {
        NS_INIT_REFCNT();
        NS_ADDREF(in);
    }

    virtual ~nsShortReader() {
        NS_RELEASE(mIn);
    }

    void Received(PRUint32 count) {
        nsAutoCMonitor mon(this);
        mReceived += count;
        mon.Notify();
    }

    PRUint32 WaitForReceipt() {
        PRUint32 result = mReceived;
        nsAutoCMonitor mon(this);
        if (mReceived == 0) {
            mon.Wait();
            NS_ASSERTION(mReceived >= 0, "failed to receive");
            result = mReceived;
        }
        mReceived = 0;
        return result;
    }

protected:
    nsIInputStream*     mIn;
    PRUint32            mReceived;
};

NS_IMPL_ISUPPORTS(nsShortReader, nsIRunnable::GetIID());

nsresult
TestShortWrites(nsIInputStream* in, nsIOutputStream* out)
{
    nsresult rv;
    nsIThread* thread;
    nsShortReader* receiver = new nsShortReader(in);
    if (receiver == nsnull) return NS_ERROR_OUT_OF_MEMORY;
    NS_ADDREF(receiver);

    rv = NS_NewThread(&thread, receiver);
    if (NS_FAILED(rv)) return rv;

    for (PRUint32 i = 0; i < ITERATIONS/100; i++) {
        PRUint32 writeCount;
        char* buf = PR_smprintf("%d %s", i, kTestPattern);
        PRUint32 len = nsCRT::strlen(buf);
        len = len * rand() / RAND_MAX;
        len = PR_MAX(1, len);
        rv = out->Write(buf, len, &writeCount);
        if (NS_FAILED(rv)) return rv;
        NS_ASSERTION(writeCount == len, "didn't write enough");

        if (gTrace)
            printf("wrote %d bytes: %s\n", writeCount, buf);
        PR_smprintf_free(buf);
        out->Flush();
        PRUint32 received = receiver->WaitForReceipt();
        NS_ASSERTION(received == writeCount, "received wrong amount");
    }
    rv = out->Close();
    if (NS_FAILED(rv)) return rv;

    thread->Join();

    NS_RELEASE(thread);
    NS_RELEASE(receiver);

    return NS_OK;
}

////////////////////////////////////////////////////////////////////////////////

void
RunTests(PRUint32 segSize, PRUint32 segCount)
{
    nsresult rv;
    nsIBufferInputStream* in;
    nsIBufferOutputStream* out;
    PRUint32 bufSize;
#if 1
    bufSize = (segSize + nsIBuffer::SEGMENT_OVERHEAD) * segCount;
    printf("Testing Old Pipes: segment size %d buffer size %d\n", segSize, segSize * segCount);

    printf("Testing long writes...\n");
    rv = NS_NewPipe(&in, &out, segSize + nsIBuffer::SEGMENT_OVERHEAD, bufSize, PR_TRUE, nsnull);
    NS_ASSERTION(NS_SUCCEEDED(rv), "NS_NewPipe failed");
    rv = TestPipe(in, out);
    NS_RELEASE(in);
    NS_RELEASE(out);
    NS_ASSERTION(NS_SUCCEEDED(rv), "TestPipe failed");

    printf("Testing short writes...\n");
    rv = NS_NewPipe(&in, &out, segSize + nsIBuffer::SEGMENT_OVERHEAD, bufSize, PR_TRUE, nsnull);
    NS_ASSERTION(NS_SUCCEEDED(rv), "NS_NewPipe failed");
    rv = TestShortWrites(in, out);
    NS_RELEASE(in);
    NS_RELEASE(out);
    NS_ASSERTION(NS_SUCCEEDED(rv), "TestPipe failed");
#endif
    bufSize = segSize * segCount;
    printf("Testing New Pipes: segment size %d buffer size %d\n", segSize, bufSize);

    printf("Testing long writes...\n");
    rv = NS_NewPipe(&in, &out, nsnull, segSize, bufSize);
    NS_ASSERTION(NS_SUCCEEDED(rv), "NS_NewPipe failed");
    rv = TestPipe(in, out);
    NS_RELEASE(in);
    NS_RELEASE(out);
    NS_ASSERTION(NS_SUCCEEDED(rv), "TestPipe failed");

    printf("Testing short writes...\n");
    rv = NS_NewPipe(&in, &out, nsnull, segSize, bufSize);
    NS_ASSERTION(NS_SUCCEEDED(rv), "NS_NewPipe failed");
    rv = TestShortWrites(in, out);
    NS_RELEASE(in);
    NS_RELEASE(out);
    NS_ASSERTION(NS_SUCCEEDED(rv), "TestPipe failed");
}

////////////////////////////////////////////////////////////////////////////////

#ifdef DEBUG
extern NS_COM void
TestSegmentedBuffer();
#endif

int
main(int argc, char* argv[])
{
    nsresult rv;
    nsIServiceManager* servMgr;

    rv = NS_InitXPCOM(&servMgr);
    if (NS_FAILED(rv)) return rv;

    if (argc > 1 && nsCRT::strcmp(argv[1], "-trace") == 0)
        gTrace = PR_TRUE;

#ifdef DEBUG
    TestSegmentedBuffer();
#endif

#if 0   // obsolete old implementation
    rv = NS_NewPipe(&in, &out, PR_TRUE, 4096 * 4);
    if (NS_FAILED(rv)) {
        printf("NewPipe failed\n");
        return -1;
    }

    rv = TestPipe(in, out);
    NS_RELEASE(in);
    NS_RELEASE(out);
    if (NS_FAILED(rv)) {
        printf("TestPipe failed\n");
        return -1;
    }
#endif
    RunTests(16, 1);
    RunTests(4096, 16);
    return 0;
}

////////////////////////////////////////////////////////////////////////////////
