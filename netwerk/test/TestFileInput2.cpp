/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsIServiceManager.h"
#include "nsIComponentRegistrar.h"
#include "nsIInputStream.h"
#include "nsIOutputStream.h"
#include "nsIRunnable.h"
#include "nsIThread.h"
#include "nsCOMArray.h"
#include "nsISimpleEnumerator.h"
#include "prinrval.h"
#include "nsIFileStreams.h"
#include "nsIFileChannel.h"
#include "nsIFile.h"
#include "nsNetUtil.h"
#include <stdio.h>

////////////////////////////////////////////////////////////////////////////////

#include <math.h>
#include "prprf.h"
#include "nsAutoLock.h"

class nsTimeSampler {
public:
    nsTimeSampler();
    void Reset();
    void StartTime();
    void EndTime();
    void AddTime(PRIntervalTime time);
    PRIntervalTime LastInterval() { return mLastInterval; }
    char* PrintStats();
protected:
    PRIntervalTime      mStartTime;
    double              mSquares;
    double              mTotalTime;
    uint32_t            mCount;
    PRIntervalTime      mLastInterval;
};

nsTimeSampler::nsTimeSampler()
{
    Reset();
}

void
nsTimeSampler::Reset()
{
    mStartTime = 0;
    mSquares = 0;
    mTotalTime = 0;
    mCount = 0;
    mLastInterval = 0;
}

void
nsTimeSampler::StartTime()
{
    mStartTime = PR_IntervalNow();
}

void
nsTimeSampler::EndTime()
{
    NS_ASSERTION(mStartTime != 0, "Forgot to call StartTime");
    PRIntervalTime endTime = PR_IntervalNow();
    mLastInterval = endTime - mStartTime;
    AddTime(mLastInterval);
    mStartTime = 0;
}

void
nsTimeSampler::AddTime(PRIntervalTime time)
{
    nsAutoCMonitor mon(this);
    mTotalTime += time;
    mSquares += (double)time * (double)time;
    mCount++;
}

char*
nsTimeSampler::PrintStats()
{
    double mean = mTotalTime / mCount;
    double variance = fabs(mSquares / mCount - mean * mean);
    double stddev = sqrt(variance);
    uint32_t imean = (uint32_t)mean;
    uint32_t istddev = (uint32_t)stddev;
    return PR_smprintf("%d +/- %d ms", 
                       PR_IntervalToMilliseconds(imean),
                       PR_IntervalToMilliseconds(istddev));
}

////////////////////////////////////////////////////////////////////////////////

nsTimeSampler gTimeSampler;

typedef nsresult (*CreateFun)(nsIRunnable* *result,
                              nsIFile* inPath, 
                              nsIFile* outPath, 
                              uint32_t bufferSize);

////////////////////////////////////////////////////////////////////////////////

nsresult
Copy(nsIInputStream* inStr, nsIOutputStream* outStr, 
     char* buf, uint32_t bufSize, uint32_t *copyCount)
{
    nsresult rv;
    while (true) {
        uint32_t count;
        rv = inStr->Read(buf, bufSize, &count);
        if (NS_FAILED(rv)) return rv;
        if (count == 0) break;

        uint32_t writeCount;
        rv = outStr->Write(buf, count, &writeCount);
        if (NS_FAILED(rv)) return rv;
        NS_ASSERTION(writeCount == count, "didn't write all the data");
        *copyCount += writeCount;
    }
    rv = outStr->Flush();
    return rv;
}

////////////////////////////////////////////////////////////////////////////////

class FileSpecWorker : public nsIRunnable {
public:

    NS_IMETHOD Run() override {
        nsresult rv;

        PRIntervalTime startTime = PR_IntervalNow();
        PRIntervalTime endTime;
        nsCOMPtr<nsIInputStream> inStr;
        nsCOMPtr<nsIOutputStream> outStr;
        uint32_t copyCount = 0;

        // Open the input stream:
        nsCOMPtr<nsIInputStream> fileIn;
        rv = NS_NewLocalFileInputStream(getter_AddRefs(fileIn), mInPath);
        if (NS_FAILED(rv)) return rv;

        rv = NS_NewBufferedInputStream(getter_AddRefs(inStr), fileIn, 65535);
        if (NS_FAILED(rv)) return rv;

        // Open the output stream:
        nsCOMPtr<nsIOutputStream> fileOut;
        rv = NS_NewLocalFileOutputStream(getter_AddRefs(fileOut),
                                         mOutPath, 
                                         PR_CREATE_FILE | PR_WRONLY | PR_TRUNCATE,
                                         0664);
        if (NS_FAILED(rv)) return rv;

        rv = NS_NewBufferedOutputStream(getter_AddRefs(outStr), fileOut, 65535);
        if (NS_FAILED(rv)) return rv;

        // Copy from one to the other
        rv = Copy(inStr, outStr, mBuffer, mBufferSize, &copyCount);
        if (NS_FAILED(rv)) return rv;

        endTime = PR_IntervalNow();
        gTimeSampler.AddTime(endTime - startTime);

        return rv;
    }

    NS_DECL_ISUPPORTS

    FileSpecWorker()
        : mInPath(nullptr), mOutPath(nullptr), mBuffer(nullptr),
          mBufferSize(0)
    {
    }

    nsresult Init(nsIFile* inPath, nsIFile* outPath,
                  uint32_t bufferSize)
    {
        mInPath = inPath;
        mOutPath = outPath;
        mBuffer = new char[bufferSize];
        mBufferSize = bufferSize;
        return (mInPath && mOutPath && mBuffer)
            ? NS_OK : NS_ERROR_OUT_OF_MEMORY;
    }

    static nsresult Create(nsIRunnable* *result,
                           nsIFile* inPath, 
                           nsIFile* outPath, 
                           uint32_t bufferSize)
    {
        FileSpecWorker* worker = new FileSpecWorker();
        if (worker == nullptr)
            return NS_ERROR_OUT_OF_MEMORY;
        NS_ADDREF(worker);

        nsresult rv = worker->Init(inPath, outPath, bufferSize);
        if (NS_FAILED(rv)) {
            NS_RELEASE(worker);
            return rv;
        }
        *result = worker;
        return NS_OK;
    }

    virtual ~FileSpecWorker() {
        delete[] mBuffer;
    }

protected:
    nsCOMPtr<nsIFile>   mInPath;
    nsCOMPtr<nsIFile>   mOutPath;
    char*               mBuffer;
    uint32_t            mBufferSize;
};

NS_IMPL_ISUPPORTS(FileSpecWorker, nsIRunnable)

////////////////////////////////////////////////////////////////////////////////

#include "nsIIOService.h"
#include "nsIChannel.h"

class FileChannelWorker : public nsIRunnable {
public:

    NS_IMETHOD Run() override {
        nsresult rv;

        PRIntervalTime startTime = PR_IntervalNow();
        PRIntervalTime endTime;
        uint32_t copyCount = 0;
        nsCOMPtr<nsIFileChannel> inCh;
        nsCOMPtr<nsIFileChannel> outCh;
        nsCOMPtr<nsIInputStream> inStr;
        nsCOMPtr<nsIOutputStream> outStr;

        rv = NS_NewLocalFileChannel(getter_AddRefs(inCh), mInPath);
        if (NS_FAILED(rv)) return rv;

        rv = inCh->Open(getter_AddRefs(inStr));
        if (NS_FAILED(rv)) return rv;

        //rv = NS_NewLocalFileChannel(getter_AddRefs(outCh), mOutPath);
        //if (NS_FAILED(rv)) return rv;

        //rv = outCh->OpenOutputStream(0, -1, 0, getter_AddRefs(outStr));
        //if (NS_FAILED(rv)) return rv;

        // Copy from one to the other
        rv = Copy(inStr, outStr, mBuffer, mBufferSize, &copyCount);
        if (NS_FAILED(rv)) return rv;

        endTime = PR_IntervalNow();
        gTimeSampler.AddTime(endTime - startTime);

        return rv;
    }

    NS_DECL_ISUPPORTS

    FileChannelWorker()
        : mInPath(nullptr), mOutPath(nullptr), mBuffer(nullptr),
          mBufferSize(0)
    {
    }

    nsresult Init(nsIFile* inPath, nsIFile* outPath,
                  uint32_t bufferSize)
    {
        mInPath = inPath;
        mOutPath = outPath;
        mBuffer = new char[bufferSize];
        mBufferSize = bufferSize;
        return (mInPath && mOutPath && mBuffer)
            ? NS_OK : NS_ERROR_OUT_OF_MEMORY;
    }

    static nsresult Create(nsIRunnable* *result,
                           nsIFile* inPath, 
                           nsIFile* outPath, 
                           uint32_t bufferSize)
    {
        FileChannelWorker* worker = new FileChannelWorker();
        if (worker == nullptr)
            return NS_ERROR_OUT_OF_MEMORY;
        NS_ADDREF(worker);

        nsresult rv = worker->Init(inPath, outPath, bufferSize);
        if (NS_FAILED(rv)) {
            NS_RELEASE(worker);
            return rv;
        }
        *result = worker;
        return NS_OK;
    }

    virtual ~FileChannelWorker() {
        delete[] mBuffer;
    }

protected:
    nsCOMPtr<nsIFile>   mInPath;
    nsCOMPtr<nsIFile>   mOutPath;
    char*               mBuffer;
    uint32_t            mBufferSize;
};

NS_IMPL_ISUPPORTS(FileChannelWorker, nsIRunnable)

////////////////////////////////////////////////////////////////////////////////

void
Test(CreateFun create, uint32_t count,
     nsIFile* inDirSpec, nsIFile* outDirSpec, uint32_t bufSize)
{
    nsresult rv;
    uint32_t i;

    nsAutoCString inDir;
    nsAutoCString outDir;
    (void)inDirSpec->GetNativePath(inDir);
    (void)outDirSpec->GetNativePath(outDir);
    printf("###########\nTest: from %s to %s, bufSize = %d\n",
           inDir.get(), outDir.get(), bufSize);
    gTimeSampler.Reset();
    nsTimeSampler testTime;
    testTime.StartTime();

    nsCOMArray<nsIThread> threads;

    nsCOMPtr<nsISimpleEnumerator> entries;
    rv = inDirSpec->GetDirectoryEntries(getter_AddRefs(entries));
    NS_ASSERTION(NS_SUCCEEDED(rv), "GetDirectoryEntries failed");

    i = 0;
    bool hasMore;
    while (i < count && NS_SUCCEEDED(entries->HasMoreElements(&hasMore)) && hasMore) {
        nsCOMPtr<nsISupports> next;
        rv = entries->GetNext(getter_AddRefs(next));
        if (NS_FAILED(rv)) goto done;

        nsCOMPtr<nsIFile> inSpec = do_QueryInterface(next, &rv);
        if (NS_FAILED(rv)) goto done;

        nsCOMPtr<nsIFile> outSpec;
        rv = outDirSpec->Clone(getter_AddRefs(outSpec)); // don't munge the original
        if (NS_FAILED(rv)) goto done;

        nsAutoCString leafName;
        rv = inSpec->GetNativeLeafName(leafName);
        if (NS_FAILED(rv)) goto done;

        rv = outSpec->AppendNative(leafName);
        if (NS_FAILED(rv)) goto done;

        bool exists;
        rv = outSpec->Exists(&exists);
        if (NS_FAILED(rv)) goto done;

        if (exists) {
            rv = outSpec->Remove(false);
            if (NS_FAILED(rv)) goto done;
        }

        nsCOMPtr<nsIThread> thread;
        nsCOMPtr<nsIRunnable> worker;
        rv = create(getter_AddRefs(worker), 
                    inSpec,
                    outSpec,
                    bufSize);
        if (NS_FAILED(rv)) goto done;

        rv = NS_NewThread(getter_AddRefs(thread), worker, 0, PR_JOINABLE_THREAD);
        if (NS_FAILED(rv)) goto done;

        bool inserted = threads.InsertObjectAt(thread, i);
        NS_ASSERTION(inserted, "not inserted");

        i++;
    }

    uint32_t j;
    for (j = 0; j < i; j++) {
        nsIThread* thread = threads.ObjectAt(j);
        thread->Join();
    }

  done:
    NS_ASSERTION(rv == NS_OK, "failed");

    testTime.EndTime();
    char* testStats = testTime.PrintStats();
    char* workerStats = gTimeSampler.PrintStats();
    printf("  threads = %d\n  work time = %s,\n  test time = %s\n",
           i, workerStats, testStats);
    PR_smprintf_free(workerStats);
    PR_smprintf_free(testStats);
}

////////////////////////////////////////////////////////////////////////////////

int
main(int argc, char* argv[])
{
    nsresult rv;

    if (argc < 2) {
        printf("usage: %s <in-dir> <out-dir>\n", argv[0]);
        return -1;
    }
    char* inDir = argv[1];
    char* outDir = argv[2];

    {
        nsCOMPtr<nsIServiceManager> servMan;
        NS_InitXPCOM2(getter_AddRefs(servMan), nullptr, nullptr);
        nsCOMPtr<nsIComponentRegistrar> registrar = do_QueryInterface(servMan);
        NS_ASSERTION(registrar, "Null nsIComponentRegistrar");
        if (registrar)
            registrar->AutoRegister(nullptr);

        nsCOMPtr<nsIFile> inDirFile;
        rv = NS_NewNativeLocalFile(nsDependentCString(inDir), false, getter_AddRefs(inDirFile));
        if (NS_FAILED(rv)) return rv;

        nsCOMPtr<nsIFile> outDirFile;
        rv = NS_NewNativeLocalFile(nsDependentCString(outDir), false, getter_AddRefs(outDirFile));
        if (NS_FAILED(rv)) return rv;

        CreateFun create = FileChannelWorker::Create;
        Test(create, 1, inDirFile, outDirFile, 16 * 1024);
#if 1
        printf("FileChannelWorker *****************************\n");
        Test(create, 20, inDirFile, outDirFile, 16 * 1024);
        Test(create, 20, inDirFile, outDirFile, 16 * 1024);
        Test(create, 20, inDirFile, outDirFile, 16 * 1024);
        Test(create, 20, inDirFile, outDirFile, 16 * 1024);
        Test(create, 20, inDirFile, outDirFile, 16 * 1024);
        Test(create, 20, inDirFile, outDirFile, 16 * 1024);
        Test(create, 20, inDirFile, outDirFile, 16 * 1024);
        Test(create, 20, inDirFile, outDirFile, 16 * 1024);
        Test(create, 20, inDirFile, outDirFile, 16 * 1024);
#endif
        create = FileSpecWorker::Create;
        printf("FileSpecWorker ********************************\n");
#if 1
        Test(create, 20, inDirFile, outDirFile, 16 * 1024);
        Test(create, 20, inDirFile, outDirFile, 16 * 1024);
        Test(create, 20, inDirFile, outDirFile, 16 * 1024);
        Test(create, 20, inDirFile, outDirFile, 16 * 1024);
        Test(create, 20, inDirFile, outDirFile, 16 * 1024);
        Test(create, 20, inDirFile, outDirFile, 16 * 1024);
        Test(create, 20, inDirFile, outDirFile, 16 * 1024);
        Test(create, 20, inDirFile, outDirFile, 16 * 1024);
        Test(create, 20, inDirFile, outDirFile, 16 * 1024);
#endif
#if 1
        Test(create, 20, inDirFile, outDirFile, 4 * 1024);
        Test(create, 20, inDirFile, outDirFile, 4 * 1024);
        Test(create, 20, inDirFile, outDirFile, 4 * 1024);
        Test(create, 20, inDirFile, outDirFile, 4 * 1024);
        Test(create, 20, inDirFile, outDirFile, 4 * 1024);
        Test(create, 20, inDirFile, outDirFile, 4 * 1024);
        Test(create, 20, inDirFile, outDirFile, 4 * 1024);
        Test(create, 20, inDirFile, outDirFile, 4 * 1024);
        Test(create, 20, inDirFile, outDirFile, 4 * 1024);
#endif
    } // this scopes the nsCOMPtrs
    // no nsCOMPtrs are allowed to be alive when you call NS_ShutdownXPCOM
    rv = NS_ShutdownXPCOM(nullptr);
    NS_ASSERTION(NS_SUCCEEDED(rv), "NS_ShutdownXPCOM failed");
    return 0;
}

////////////////////////////////////////////////////////////////////////////////
