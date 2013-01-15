/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "TestHarness.h"

#include "nsIThread.h"
#include "nsIRunnable.h"
#include "nsThreadUtils.h"
#include "prprf.h"
#include "prinrval.h"
#include "nsCRT.h"
#include "nsIPipe.h"    // new implementation

#include "mozilla/Monitor.h"
using namespace mozilla;

/** NS_NewPipe2 reimplemented, because it's not exported by XPCOM */
nsresult TP_NewPipe2(nsIAsyncInputStream** input,
                     nsIAsyncOutputStream** output,
                     bool nonBlockingInput,
                     bool nonBlockingOutput,
                     uint32_t segmentSize,
                     uint32_t segmentCount,
                     nsIMemory* segmentAlloc)
{
  nsCOMPtr<nsIPipe> pipe = do_CreateInstance("@mozilla.org/pipe;1");
  if (!pipe)
    return NS_ERROR_OUT_OF_MEMORY;

  nsresult rv = pipe->Init(nonBlockingInput,
                           nonBlockingOutput,
                           segmentSize,
                           segmentCount,
                           segmentAlloc);

  if (NS_FAILED(rv))
    return rv;

  pipe->GetInputStream(input);
  pipe->GetOutputStream(output);
  return NS_OK;
}

/** NS_NewPipe reimplemented, because it's not exported by XPCOM */
#define TP_DEFAULT_SEGMENT_SIZE  4096
nsresult TP_NewPipe(nsIInputStream **pipeIn,
                    nsIOutputStream **pipeOut,
                    uint32_t segmentSize = 0,
                    uint32_t maxSize = 0,
                    bool nonBlockingInput = false,
                    bool nonBlockingOutput = false,
                    nsIMemory *segmentAlloc = nullptr);
nsresult TP_NewPipe(nsIInputStream **pipeIn,
                    nsIOutputStream **pipeOut,
                    uint32_t segmentSize,
                    uint32_t maxSize,
                    bool nonBlockingInput,
                    bool nonBlockingOutput,
                    nsIMemory *segmentAlloc)
{
    if (segmentSize == 0)
        segmentSize = TP_DEFAULT_SEGMENT_SIZE;

    // Handle maxSize of UINT32_MAX as a special case
    uint32_t segmentCount;
    if (maxSize == UINT32_MAX)
        segmentCount = UINT32_MAX;
    else
        segmentCount = maxSize / segmentSize;

    nsIAsyncInputStream *in;
    nsIAsyncOutputStream *out;
    nsresult rv = TP_NewPipe2(&in, &out, nonBlockingInput, nonBlockingOutput,
                              segmentSize, segmentCount, segmentAlloc);
    if (NS_FAILED(rv)) return rv;

    *pipeIn = in;
    *pipeOut = out;
    return NS_OK;
}


#define KEY             0xa7
#define ITERATIONS      33333
char kTestPattern[] = "My hovercraft is full of eels.\n";

bool gTrace = false;

static nsresult
WriteAll(nsIOutputStream *os, const char *buf, uint32_t bufLen, uint32_t *lenWritten)
{
    const char *p = buf;
    *lenWritten = 0;
    while (bufLen) {
        uint32_t n;
        nsresult rv = os->Write(p, bufLen, &n);
        if (NS_FAILED(rv)) return rv;
        p += n;
        bufLen -= n;
        *lenWritten += n;
    }
    return NS_OK;
}

class nsReceiver : public nsIRunnable {
public:
    NS_DECL_ISUPPORTS

    NS_IMETHOD Run() {
        nsresult rv;
        char buf[101];
        uint32_t count;
        PRIntervalTime start = PR_IntervalNow();
        while (true) {
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
                buf[count] = '\0';
                printf("read: %s\n", buf);
            }
            mCount += count;
        }
        PRIntervalTime end = PR_IntervalNow();
        printf("read  %d bytes, time = %dms\n", mCount,
               PR_IntervalToMilliseconds(end - start));
        return rv;
    }

    nsReceiver(nsIInputStream* in) : mIn(in), mCount(0) {
    }

    uint32_t GetBytesRead() { return mCount; }

protected:
    nsCOMPtr<nsIInputStream> mIn;
    uint32_t            mCount;
};

NS_IMPL_THREADSAFE_ISUPPORTS1(nsReceiver, nsIRunnable)

nsresult
TestPipe(nsIInputStream* in, nsIOutputStream* out)
{
    nsCOMPtr<nsReceiver> receiver = new nsReceiver(in);
    if (!receiver)
        return NS_ERROR_OUT_OF_MEMORY;

    nsresult rv;

    nsCOMPtr<nsIThread> thread;
    rv = NS_NewThread(getter_AddRefs(thread), receiver);
    if (NS_FAILED(rv)) return rv;

    uint32_t total = 0;
    PRIntervalTime start = PR_IntervalNow();
    for (uint32_t i = 0; i < ITERATIONS; i++) {
        uint32_t writeCount;
        char *buf = PR_smprintf("%d %s", i, kTestPattern);
        uint32_t len = strlen(buf);
        rv = WriteAll(out, buf, len, &writeCount);
        if (gTrace) {
            printf("wrote: ");
            for (uint32_t j = 0; j < writeCount; j++) {
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

    thread->Shutdown();

    printf("wrote %d bytes, time = %dms\n", total,
           PR_IntervalToMilliseconds(end - start));
    NS_ASSERTION(receiver->GetBytesRead() == total, "didn't read everything");

    return NS_OK;
}

////////////////////////////////////////////////////////////////////////////////

class nsShortReader : public nsIRunnable {
public:
    NS_DECL_ISUPPORTS

    NS_IMETHOD Run() {
        nsresult rv;
        char buf[101];
        uint32_t count;
        uint32_t total = 0;
        while (true) {
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

            if (gTrace) {
                // For next |printf()| call and possible others elsewhere.
                buf[count] = '\0';

                printf("read %d bytes: %s\n", count, buf);
            }

            Received(count);
            total += count;
        }
        printf("read  %d bytes\n", total);
        return rv;
    }

    nsShortReader(nsIInputStream* in) : mIn(in), mReceived(0) {
        mMon = new Monitor("nsShortReader");
    }

    void Received(uint32_t count) {
        MonitorAutoEnter mon(*mMon);
        mReceived += count;
        mon.Notify();
    }

    uint32_t WaitForReceipt(const uint32_t aWriteCount) {
        MonitorAutoEnter mon(*mMon);
        uint32_t result = mReceived;

        while (result < aWriteCount) {
            mon.Wait();

            NS_ASSERTION(mReceived > result, "failed to receive");
            result = mReceived;
        }

        mReceived = 0;
        return result;
    }

protected:
    nsCOMPtr<nsIInputStream> mIn;
    uint32_t                 mReceived;
    Monitor*                 mMon;
};

NS_IMPL_THREADSAFE_ISUPPORTS1(nsShortReader, nsIRunnable)

nsresult
TestShortWrites(nsIInputStream* in, nsIOutputStream* out)
{
    nsCOMPtr<nsShortReader> receiver = new nsShortReader(in);
    if (!receiver)
        return NS_ERROR_OUT_OF_MEMORY;

    nsresult rv;

    nsCOMPtr<nsIThread> thread;
    rv = NS_NewThread(getter_AddRefs(thread), receiver);
    if (NS_FAILED(rv)) return rv;

    uint32_t total = 0;
    for (uint32_t i = 0; i < ITERATIONS; i++) {
        uint32_t writeCount;
        char* buf = PR_smprintf("%d %s", i, kTestPattern);
        uint32_t len = strlen(buf);
        len = len * rand() / RAND_MAX;
        len = XPCOM_MAX(1, len);
        rv = WriteAll(out, buf, len, &writeCount);
        if (NS_FAILED(rv)) return rv;
        NS_ASSERTION(writeCount == len, "didn't write enough");
        total += writeCount;

        if (gTrace)
            printf("wrote %d bytes: %s\n", writeCount, buf);
        PR_smprintf_free(buf);
        //printf("calling Flush\n");
        out->Flush();
        //printf("calling WaitForReceipt\n");

#ifdef DEBUG
        const uint32_t received =
#endif
          receiver->WaitForReceipt(writeCount);
        NS_ASSERTION(received == writeCount, "received wrong amount");
    }
    rv = out->Close();
    if (NS_FAILED(rv)) return rv;

    thread->Shutdown();

    printf("wrote %d bytes\n", total);

    return NS_OK;
}

////////////////////////////////////////////////////////////////////////////////

class nsPump : public nsIRunnable
{
public:
    NS_DECL_ISUPPORTS

    NS_IMETHOD Run() {
        nsresult rv;
        uint32_t count;
        while (true) {
            rv = mOut->WriteFrom(mIn, ~0U, &count);
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
    }

protected:
    nsCOMPtr<nsIInputStream>      mIn;
    nsCOMPtr<nsIOutputStream>     mOut;
    uint32_t                            mCount;
};

NS_IMPL_THREADSAFE_ISUPPORTS1(nsPump,
                              nsIRunnable)

nsresult
TestChainedPipes()
{
    nsresult rv;
    printf("TestChainedPipes\n");

    nsCOMPtr<nsIInputStream> in1;
    nsCOMPtr<nsIOutputStream> out1;
    rv = TP_NewPipe(getter_AddRefs(in1), getter_AddRefs(out1), 20, 1999);
    if (NS_FAILED(rv)) return rv;

    nsCOMPtr<nsIInputStream> in2;
    nsCOMPtr<nsIOutputStream> out2;
    rv = TP_NewPipe(getter_AddRefs(in2), getter_AddRefs(out2), 200, 401);
    if (NS_FAILED(rv)) return rv;

    nsCOMPtr<nsPump> pump = new nsPump(in1, out2);
    if (pump == nullptr) return NS_ERROR_OUT_OF_MEMORY;

    nsCOMPtr<nsIThread> thread;
    rv = NS_NewThread(getter_AddRefs(thread), pump);
    if (NS_FAILED(rv)) return rv;

    nsCOMPtr<nsReceiver> receiver = new nsReceiver(in2);
    if (receiver == nullptr) return NS_ERROR_OUT_OF_MEMORY;

    nsCOMPtr<nsIThread> receiverThread;
    rv = NS_NewThread(getter_AddRefs(receiverThread), receiver);
    if (NS_FAILED(rv)) return rv;

    uint32_t total = 0;
    for (uint32_t i = 0; i < ITERATIONS; i++) {
        uint32_t writeCount;
        char* buf = PR_smprintf("%d %s", i, kTestPattern);
        uint32_t len = strlen(buf);
        len = len * rand() / RAND_MAX;
        len = XPCOM_MAX(1, len);
        rv = WriteAll(out1, buf, len, &writeCount);
        if (NS_FAILED(rv)) return rv;
        NS_ASSERTION(writeCount == len, "didn't write enough");
        total += writeCount;

        if (gTrace)
            printf("wrote %d bytes: %s\n", writeCount, buf);

        PR_smprintf_free(buf);
    }
    printf("wrote total of %d bytes\n", total);
    rv = out1->Close();
    if (NS_FAILED(rv)) return rv;

    thread->Shutdown();
    receiverThread->Shutdown();

    return NS_OK;
}

////////////////////////////////////////////////////////////////////////////////

void
RunTests(uint32_t segSize, uint32_t segCount)
{
    nsresult rv;
    nsCOMPtr<nsIInputStream> in;
    nsCOMPtr<nsIOutputStream> out;
    uint32_t bufSize = segSize * segCount;
    printf("Testing New Pipes: segment size %d buffer size %d\n", segSize, bufSize);

    printf("Testing long writes...\n");
    rv = TP_NewPipe(getter_AddRefs(in), getter_AddRefs(out), segSize, bufSize);
    NS_ASSERTION(NS_SUCCEEDED(rv), "TP_NewPipe failed");
    rv = TestPipe(in, out);
    NS_ASSERTION(NS_SUCCEEDED(rv), "TestPipe failed");

    printf("Testing short writes...\n");
    rv = TP_NewPipe(getter_AddRefs(in), getter_AddRefs(out), segSize, bufSize);
    NS_ASSERTION(NS_SUCCEEDED(rv), "TP_NewPipe failed");
    rv = TestShortWrites(in, out);
    NS_ASSERTION(NS_SUCCEEDED(rv), "TestPipe failed");
}

////////////////////////////////////////////////////////////////////////////////

#if 0
extern void
TestSegmentedBuffer();
#endif

int
main(int argc, char* argv[])
{
    nsresult rv;

    nsCOMPtr<nsIServiceManager> servMgr;
    rv = NS_InitXPCOM2(getter_AddRefs(servMgr), NULL, NULL);
    if (NS_FAILED(rv)) return rv;

    if (argc > 1 && nsCRT::strcmp(argv[1], "-trace") == 0)
        gTrace = true;

    rv = TestChainedPipes();
    NS_ASSERTION(NS_SUCCEEDED(rv), "TestChainedPipes failed");
    RunTests(16, 1);
    RunTests(4096, 16);

    servMgr = 0;
    rv = NS_ShutdownXPCOM( NULL );
    NS_ASSERTION(NS_SUCCEEDED(rv), "NS_ShutdownXPCOM failed");

    return 0;
}

////////////////////////////////////////////////////////////////////////////////
