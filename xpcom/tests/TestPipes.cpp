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
 *   Pierre Phaneuf <pp@ludusdesign.com>
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

#include "nsIThread.h"
#include "nsIRunnable.h"
#include "nsIInputStream.h"
#include "nsIOutputStream.h"
#include "nsIServiceManager.h"
#include "prprf.h"
#include "prinrval.h"
#include "plstr.h"
#include "nsCRT.h"
#include "nsCOMPtr.h"
#include <stdio.h>
#include "nsIPipe.h"    // new implementation
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
            if (NS_FAILED(rv)) {
                printf("read failed\n");
                break;
            }
            if (count == 0) {
//                printf("EOF count = %d\n", mCount);
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

NS_IMPL_THREADSAFE_ISUPPORTS1(nsReceiver, nsIRunnable);

nsresult
TestPipe(nsIInputStream* in, nsIOutputStream* out)
{
    nsresult rv;
    nsIThread* thread;
    nsReceiver* receiver = new nsReceiver(in);
    if (receiver == nsnull) return NS_ERROR_OUT_OF_MEMORY;
    NS_ADDREF(receiver);

    rv = NS_NewThread(&thread, receiver, 0, PR_JOINABLE_THREAD);
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

    printf("wrote %d bytes, time = %dms\n", total,
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
        PRUint32 total = 0;
        while (PR_TRUE) {
            //if (gTrace)
            //    printf("calling Read\n");
            rv = mIn->Read(buf, 100, &count);
            if (NS_FAILED(rv)) {
                printf("read failed\n");
                break;
            }
            if (count == 0) {
                break;
            }
            buf[count] = '\0';
            if (gTrace)
                printf("read %d bytes: %s\n", count, buf);
            Received(count);
            total += count;
        }
        printf("read  %d bytes\n", total);
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

NS_IMPL_THREADSAFE_ISUPPORTS1(nsShortReader, nsIRunnable);

nsresult
TestShortWrites(nsIInputStream* in, nsIOutputStream* out)
{
    nsresult rv;
    nsIThread* thread;
    nsShortReader* receiver = new nsShortReader(in);
    if (receiver == nsnull) return NS_ERROR_OUT_OF_MEMORY;
    NS_ADDREF(receiver);

    rv = NS_NewThread(&thread, receiver, 0, PR_JOINABLE_THREAD);
    if (NS_FAILED(rv)) return rv;

    PRUint32 total = 0;
    for (PRUint32 i = 0; i < ITERATIONS; i++) {
        PRUint32 writeCount;
        char* buf = PR_smprintf("%d %s", i, kTestPattern);
        PRUint32 len = nsCRT::strlen(buf);
        len = len * rand() / RAND_MAX;
        len = PR_MAX(1, len);
        rv = out->Write(buf, len, &writeCount);
        if (NS_FAILED(rv)) return rv;
        NS_ASSERTION(writeCount == len, "didn't write enough");
        total += writeCount;

        if (gTrace)
            printf("wrote %d bytes: %s\n", writeCount, buf);
        PR_smprintf_free(buf);
        //printf("calling Flush\n");
        out->Flush();
        //printf("calling WaitForReceipt\n");
        PRUint32 received = receiver->WaitForReceipt();
        NS_ASSERTION(received == writeCount, "received wrong amount");
    }
    rv = out->Close();
    if (NS_FAILED(rv)) return rv;

    thread->Join();
    printf("wrote %d bytes\n", total);

    NS_RELEASE(thread);
    NS_RELEASE(receiver);

    return NS_OK;
}

////////////////////////////////////////////////////////////////////////////////

class nsPipeObserver : public nsIInputStreamObserver, 
                       public nsIOutputStreamObserver
{
public:
    NS_DECL_ISUPPORTS

    NS_IMETHOD OnFull(nsIOutputStream *outStr) {
        printf("OnFull outStr=%p\n", outStr);
        return NS_OK;
    }

    NS_IMETHOD OnWrite(nsIOutputStream *outStr, PRUint32 amount) {
        printf("OnWrite outStr=%p amount=%d\n", outStr, amount);
        return NS_OK;
    }

    NS_IMETHOD OnEmpty(nsIInputStream* inStr) {
        printf("OnEmpty inStr=%p\n", inStr);
        return NS_OK;
    }

    NS_IMETHOD OnClose(nsIInputStream* inStr) {
        printf("OnClose inStr=%p\n", inStr);
        return NS_OK;
    }

    nsPipeObserver() { NS_INIT_REFCNT(); }
    virtual ~nsPipeObserver() {}
};

NS_IMPL_ISUPPORTS2(nsPipeObserver, nsIInputStreamObserver, nsIOutputStreamObserver);

nsresult
TestPipeObserver()
{
    nsresult rv;
    nsPipeObserver* obs = new nsPipeObserver();
    if (obs == nsnull) return NS_ERROR_OUT_OF_MEMORY;
    NS_ADDREF(obs);

    printf("TestPipeObserver: OnWrite and OnFull should be called once, OnEmpty should be called twice.\n");
    nsIInputStream* in;
    nsIOutputStream* out;
    rv = NS_NewPipe(&in, &out, 18, 36, PR_TRUE, PR_TRUE);
    if (NS_FAILED(rv)) return rv;

    rv = in->SetObserver(obs);
    if (NS_FAILED(rv)) return rv;
    rv = out->SetObserver(obs);
    if (NS_FAILED(rv)) return rv;

    char buf[] = "puirt a beul: a style of Gaelic vocal music intended for dancing.";
    PRUint32 cnt;
    printf("> should see OnWrite message:\n");
    rv = out->Write(buf, 20, &cnt);
    if (NS_FAILED(rv)) return rv;
    NS_ASSERTION(cnt == 20, "Write failed");

    printf("> should see OnWrite message followed by OnFull message:\n");
    rv = out->Write(buf + 20, 20, &cnt);
    if (NS_FAILED(rv)) return rv;
    NS_ASSERTION(cnt == 16, "Write failed");

    rv = in->Available(&cnt);
    if (NS_FAILED(rv)) return rv;
    printf("available = %u\n", cnt);
    NS_ASSERTION(cnt == 36, "Available failed");

    char buf2[40];
    printf("> should see OnEmpty message:\n");
    rv = in->Read(buf2, 40, &cnt);
    if (NS_FAILED(rv)) return rv;
    printf("cnt = %u\n", cnt);
    NS_ASSERTION(cnt == 36, "Read failed");
    NS_ASSERTION(nsCRT::strncmp(buf, buf2, 36) == 0, "Read wrong stuff");

    rv = in->Available(&cnt);
    if (NS_FAILED(rv)) return rv;
    printf("available = %u\n", cnt);
    NS_ASSERTION(cnt == 0, "Available failed");

    printf("> should see OnEmpty message:\n");
    rv = in->Read(buf2, 2, &cnt);
    if (NS_FAILED(rv) && rv != NS_BASE_STREAM_WOULD_BLOCK) return rv;
    NS_ASSERTION(cnt == 0 && rv == NS_BASE_STREAM_WOULD_BLOCK, "Read failed");

    printf("> should see OnWrite message:\n");
    rv = out->Write(buf, 20, &cnt);
    if (NS_FAILED(rv)) return rv;
    NS_ASSERTION(cnt == 20, "Write failed");

    rv = in->Available(&cnt);
    if (NS_FAILED(rv)) return rv;
    printf("available = %u\n", cnt);
    NS_ASSERTION(cnt == 20, "Available failed");

    printf("> should see OnEmpty message:\n");
    rv = in->Read(buf2, 20, &cnt);
    if (NS_FAILED(rv)) return rv;
    NS_ASSERTION(cnt == 20, "Read failed");

    printf("> should see OnClose message:\n");
    NS_RELEASE(obs);
    NS_RELEASE(in);
    NS_RELEASE(out);
    return NS_OK;
}

////////////////////////////////////////////////////////////////////////////////

class nsPump : public nsIInputStreamObserver, 
               public nsIOutputStreamObserver,
               public nsIRunnable
{
public:
    NS_DECL_ISUPPORTS

    NS_IMETHOD OnFull(nsIOutputStream *outStr) {
        printf("OnFull outStr=%p\n", outStr);
        nsAutoCMonitor mon(this);
        mon.Notify();
        return NS_OK;
    }

    NS_IMETHOD OnWrite(nsIOutputStream *outStr, PRUint32 amount) {
        printf("OnWrite outStr=%p amount=%d\n", outStr, amount);
        return NS_OK;
    }

    NS_IMETHOD OnEmpty(nsIInputStream *inStr) {
        printf("OnEmpty inStr=%p\n", inStr);
        nsAutoCMonitor mon(this);
        mon.Notify();
        return NS_OK;
    }

    NS_IMETHOD OnClose(nsIInputStream *inStr) {
        printf("OnClose inStr=%p\n", inStr);
        nsAutoCMonitor mon(this);
        mon.Notify();
        return NS_OK;
    }

    NS_IMETHOD Run() {
        nsresult rv;
        PRUint32 count;
        while (PR_TRUE) {
            nsAutoCMonitor mon(this);
            rv = mOut->WriteFrom(mIn, -1, &count);
            if (NS_FAILED(rv)) {
                printf("Write failed\n");
                break;
            }
            if (count == 0) {
                printf("EOF count = %d\n", mCount);
                break;
            }

            if (gTrace) {
                printf("Wrote: %d\n", count);
            }
            mCount += count;
        }
        mOut->Close();
        return rv;
    }

    nsPump(nsIInputStream* in,
           nsIOutputStream* out)
        : mIn(in), mOut(out), mCount(0) {
        NS_INIT_REFCNT();
    }

    virtual ~nsPump() {
    }

protected:
    nsCOMPtr<nsIInputStream>      mIn;
    nsCOMPtr<nsIOutputStream>     mOut;
    PRUint32                            mCount;
};

NS_IMPL_THREADSAFE_ISUPPORTS3(nsPump, nsIInputStreamObserver, 
                              nsIOutputStreamObserver, nsIRunnable)

nsresult
TestChainedPipes()
{
    nsresult rv;
    printf("TestChainedPipes\n");

    nsIInputStream* in1;
    nsIOutputStream* out1;
    rv = NS_NewPipe(&in1, &out1, 20, 1999);
    if (NS_FAILED(rv)) return rv;

    nsIInputStream* in2;
    nsIOutputStream* out2;
    rv = NS_NewPipe(&in2, &out2, 200, 401);
    if (NS_FAILED(rv)) return rv;

    nsIThread* thread;
    nsPump* pump = new nsPump(in1, out2);
    if (pump == nsnull) return NS_ERROR_OUT_OF_MEMORY;
    NS_ADDREF(pump);

    rv = NS_NewThread(&thread, pump, 0, PR_JOINABLE_THREAD);
    if (NS_FAILED(rv)) return rv;

    nsIThread* receiverThread;
    nsReceiver* receiver = new nsReceiver(in2);
    if (receiver == nsnull) return NS_ERROR_OUT_OF_MEMORY;
    NS_ADDREF(receiver);

    rv = NS_NewThread(&receiverThread, receiver, 0, PR_JOINABLE_THREAD);
    if (NS_FAILED(rv)) return rv;

    PRUint32 total = 0;
    for (PRUint32 i = 0; i < ITERATIONS; i++) {
        PRUint32 writeCount;
        char* buf = PR_smprintf("%d %s", i, kTestPattern);
        PRUint32 len = nsCRT::strlen(buf);
        len = len * rand() / RAND_MAX;
        len = PR_MAX(1, len);
        rv = out1->Write(buf, len, &writeCount);
        if (NS_FAILED(rv)) return rv;
        NS_ASSERTION(writeCount == len, "didn't write enough");
        total += writeCount;

        if (gTrace)
            printf("wrote %d bytes: %s\n", writeCount, buf);
        //out1->Flush();  // wakes up the pump

        PR_smprintf_free(buf);
    }
    printf("wrote total of %d bytes\n", total);
    rv = out1->Close();
    if (NS_FAILED(rv)) return rv;

    thread->Join();
    receiverThread->Join();

    NS_RELEASE(thread);
    NS_RELEASE(pump);
    NS_RELEASE(receiverThread);
    NS_RELEASE(receiver);
    return NS_OK;
}

////////////////////////////////////////////////////////////////////////////////

void
RunTests(PRUint32 segSize, PRUint32 segCount)
{
    nsresult rv;
    nsIInputStream* in;
    nsIOutputStream* out;
    PRUint32 bufSize;

    bufSize = segSize * segCount;
    printf("Testing New Pipes: segment size %d buffer size %d\n", segSize, bufSize);

    printf("Testing long writes...\n");
    rv = NS_NewPipe(&in, &out, segSize, bufSize);
    NS_ASSERTION(NS_SUCCEEDED(rv), "NS_NewPipe failed");
    rv = TestPipe(in, out);
    NS_RELEASE(in);
    NS_RELEASE(out);
    NS_ASSERTION(NS_SUCCEEDED(rv), "TestPipe failed");

    printf("Testing short writes...\n");
    rv = NS_NewPipe(&in, &out, segSize, bufSize);
    NS_ASSERTION(NS_SUCCEEDED(rv), "NS_NewPipe failed");
    rv = TestShortWrites(in, out);
    NS_RELEASE(in);
    NS_RELEASE(out);
    NS_ASSERTION(NS_SUCCEEDED(rv), "TestPipe failed");
}

////////////////////////////////////////////////////////////////////////////////
#if 0
void
TestSearch(const char* delim, PRUint32 segSize)
{
    nsresult rv;
    // need at least 2 segments to test boundary conditions:
    PRUint32 bufDataSize = segSize * 2;
    PRUint32 bufSize = segSize * 2;
    nsIInputStream* in;
    nsIOutputStream* out;
    rv = NS_NewPipe(&in, &out, segSize, bufSize);
    NS_ASSERTION(NS_SUCCEEDED(rv), "NS_NewPipe failed");
    out->SetNonBlocking(PR_TRUE);

    PRUint32 i, j, amt;
    PRUint32 delimLen = nsCRT::strlen(delim);
    for (i = 0; i < bufDataSize; i++) {
        // first fill the buffer
        for (j = 0; j < i; j++) {
            rv = out->Write("-", 1, &amt);
            NS_ASSERTION(NS_SUCCEEDED(rv) && amt == 1, "Write failed");
        }
        rv = out->Write(delim, delimLen, &amt);
        NS_ASSERTION(NS_SUCCEEDED(rv), "Write failed");
        if (i + amt < bufDataSize) {
            for (j = i + amt; j < bufDataSize; j++) {
                rv = out->Write("+", 1, &amt);
                NS_ASSERTION(NS_SUCCEEDED(rv) && amt == 1, "Write failed");
            }
        }

        // now search for the delimiter
        PRBool found;
        PRUint32 offset;
        rv = in->Search(delim, PR_FALSE, &found, &offset);
        NS_ASSERTION(NS_SUCCEEDED(rv), "Search failed");

        // print the results
        char* bufferContents = new char[bufDataSize + 1];
        rv = in->Read(bufferContents, bufDataSize, &amt);
        NS_ASSERTION(NS_SUCCEEDED(rv) && amt == bufDataSize, "Read failed");
        bufferContents[bufDataSize] = '\0';
        printf("Buffer: %s\nDelim: %s %s offset: %d\n", bufferContents,
               delim, (found ? "found" : "not found"), offset);
    }
    NS_RELEASE(in);
    NS_RELEASE(out);
}
#endif
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

    rv = NS_InitXPCOM2(&servMgr, NULL, NULL);
    if (NS_FAILED(rv)) return rv;

    if (argc > 1 && nsCRT::strcmp(argv[1], "-trace") == 0)
        gTrace = PR_TRUE;

#ifdef DEBUG
    TestSegmentedBuffer();
#endif

#if 0   // obsolete old implementation
    rv = NS_NewPipe(&in, &out, 4096 * 4);
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
#if 0
    TestSearch("foo", 8);
    TestSearch("bar", 6);
    TestSearch("baz", 2);
#endif

    rv = TestPipeObserver();
    NS_ASSERTION(NS_SUCCEEDED(rv), "TestPipeObserver failed");
    rv = TestChainedPipes();
    NS_ASSERTION(NS_SUCCEEDED(rv), "TestChainedPipes failed");
    RunTests(16, 1);
    RunTests(4096, 16);
    return 0;
}

////////////////////////////////////////////////////////////////////////////////
