/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include <algorithm>
#include "nsIAsyncInputStream.h"
#include "nsIAsyncOutputStream.h"
#include "nsIThread.h"
#include "nsIRunnable.h"
#include "nsThreadUtils.h"
#include "prprf.h"
#include "prinrval.h"
#include "nsCRT.h"
#include "nsIPipe.h"    // new implementation

#include "mozilla/ReentrantMonitor.h"

#include "gtest/gtest.h"
using namespace mozilla;

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

class nsReceiver MOZ_FINAL : public nsIRunnable {
public:
    NS_DECL_THREADSAFE_ISUPPORTS

    NS_IMETHOD Run() MOZ_OVERRIDE {
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

    explicit nsReceiver(nsIInputStream* in) : mIn(in), mCount(0) {
    }

    uint32_t GetBytesRead() { return mCount; }

private:
    ~nsReceiver() {}

protected:
    nsCOMPtr<nsIInputStream> mIn;
    uint32_t            mCount;
};

NS_IMPL_ISUPPORTS(nsReceiver, nsIRunnable)

nsresult
TestPipe(nsIInputStream* in, nsIOutputStream* out)
{
    nsRefPtr<nsReceiver> receiver = new nsReceiver(in);
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
    EXPECT_EQ(receiver->GetBytesRead(), total);

    return NS_OK;
}

////////////////////////////////////////////////////////////////////////////////

class nsShortReader MOZ_FINAL : public nsIRunnable {
public:
    NS_DECL_THREADSAFE_ISUPPORTS

    NS_IMETHOD Run() MOZ_OVERRIDE {
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

    explicit nsShortReader(nsIInputStream* in) : mIn(in), mReceived(0) {
        mMon = new ReentrantMonitor("nsShortReader");
    }

    void Received(uint32_t count) {
        ReentrantMonitorAutoEnter mon(*mMon);
        mReceived += count;
        mon.Notify();
    }

    uint32_t WaitForReceipt(const uint32_t aWriteCount) {
        ReentrantMonitorAutoEnter mon(*mMon);
        uint32_t result = mReceived;

        while (result < aWriteCount) {
            mon.Wait();

            EXPECT_TRUE(mReceived > result);
            result = mReceived;
        }

        mReceived = 0;
        return result;
    }

private:
    ~nsShortReader() {}

protected:
    nsCOMPtr<nsIInputStream> mIn;
    uint32_t                 mReceived;
    ReentrantMonitor*        mMon;
};

NS_IMPL_ISUPPORTS(nsShortReader, nsIRunnable)

nsresult
TestShortWrites(nsIInputStream* in, nsIOutputStream* out)
{
    nsRefPtr<nsShortReader> receiver = new nsShortReader(in);
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
        len = std::min(1u, len);
        rv = WriteAll(out, buf, len, &writeCount);
        if (NS_FAILED(rv)) return rv;
        EXPECT_EQ(writeCount, len);
        total += writeCount;

        if (gTrace)
            printf("wrote %d bytes: %s\n", writeCount, buf);
        PR_smprintf_free(buf);
        //printf("calling Flush\n");
        out->Flush();
        //printf("calling WaitForReceipt\n");

#ifdef DEBUG
        const uint32_t received =
          receiver->WaitForReceipt(writeCount);
        EXPECT_EQ(received, writeCount);
#endif
    }
    rv = out->Close();
    if (NS_FAILED(rv)) return rv;

    thread->Shutdown();

    printf("wrote %d bytes\n", total);

    return NS_OK;
}

////////////////////////////////////////////////////////////////////////////////

class nsPump MOZ_FINAL : public nsIRunnable
{
public:
    NS_DECL_THREADSAFE_ISUPPORTS

    NS_IMETHOD Run() MOZ_OVERRIDE {
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

private:
    ~nsPump() {}

protected:
    nsCOMPtr<nsIInputStream>      mIn;
    nsCOMPtr<nsIOutputStream>     mOut;
    uint32_t                            mCount;
};

NS_IMPL_ISUPPORTS(nsPump, nsIRunnable)

TEST(Pipes, ChainedPipes)
{
    nsresult rv;
    if (gTrace) {
        printf("TestChainedPipes\n");
    }

    nsCOMPtr<nsIInputStream> in1;
    nsCOMPtr<nsIOutputStream> out1;
    rv = NS_NewPipe(getter_AddRefs(in1), getter_AddRefs(out1), 20, 1999);
    if (NS_FAILED(rv)) return;

    nsCOMPtr<nsIInputStream> in2;
    nsCOMPtr<nsIOutputStream> out2;
    rv = NS_NewPipe(getter_AddRefs(in2), getter_AddRefs(out2), 200, 401);
    if (NS_FAILED(rv)) return;

    nsRefPtr<nsPump> pump = new nsPump(in1, out2);
    if (pump == nullptr) return;

    nsCOMPtr<nsIThread> thread;
    rv = NS_NewThread(getter_AddRefs(thread), pump);
    if (NS_FAILED(rv)) return;

    nsRefPtr<nsReceiver> receiver = new nsReceiver(in2);
    if (receiver == nullptr) return;

    nsCOMPtr<nsIThread> receiverThread;
    rv = NS_NewThread(getter_AddRefs(receiverThread), receiver);
    if (NS_FAILED(rv)) return;

    uint32_t total = 0;
    for (uint32_t i = 0; i < ITERATIONS; i++) {
        uint32_t writeCount;
        char* buf = PR_smprintf("%d %s", i, kTestPattern);
        uint32_t len = strlen(buf);
        len = len * rand() / RAND_MAX;
        len = std::max(1u, len);
        rv = WriteAll(out1, buf, len, &writeCount);
        if (NS_FAILED(rv)) return;
        EXPECT_EQ(writeCount, len);
        total += writeCount;

        if (gTrace)
            printf("wrote %d bytes: %s\n", writeCount, buf);

        PR_smprintf_free(buf);
    }
    if (gTrace) {
        printf("wrote total of %d bytes\n", total);
    }
    rv = out1->Close();
    if (NS_FAILED(rv)) return;

    thread->Shutdown();
    receiverThread->Shutdown();
}

////////////////////////////////////////////////////////////////////////////////

void
RunTests(uint32_t segSize, uint32_t segCount)
{
    nsresult rv;
    nsCOMPtr<nsIInputStream> in;
    nsCOMPtr<nsIOutputStream> out;
    uint32_t bufSize = segSize * segCount;
    if (gTrace) {
        printf("Testing New Pipes: segment size %d buffer size %d\n", segSize, bufSize);
        printf("Testing long writes...\n");
    }
    rv = NS_NewPipe(getter_AddRefs(in), getter_AddRefs(out), segSize, bufSize);
    EXPECT_TRUE(NS_SUCCEEDED(rv));
    rv = TestPipe(in, out);
    EXPECT_TRUE(NS_SUCCEEDED(rv));

    if (gTrace) {
        printf("Testing short writes...\n");
    }
    rv = NS_NewPipe(getter_AddRefs(in), getter_AddRefs(out), segSize, bufSize);
    EXPECT_TRUE(NS_SUCCEEDED(rv));
    rv = TestShortWrites(in, out);
    EXPECT_TRUE(NS_SUCCEEDED(rv));
}

TEST(Pipes, Main)
{
    RunTests(16, 1);
    RunTests(4096, 16);
}
