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

#include "nsIServiceManager.h"
#include "nsIInputStream.h"
#include "nsIOutputStream.h"
#include "nsIThread.h"
#include "nsISupportsArray.h"
#include "prinrval.h"
#include "nsIFileStream.h"
#include "nsFileSpec.h"
#include <stdio.h>

PRIntervalTime gTotalTime = 0;

typedef nsresult (*CreateFun)(nsIRunnable* *result,
                              const char* inPath, 
                              const char* outPath, 
                              PRUint32 bufferSize);

////////////////////////////////////////////////////////////////////////////////

nsresult
Copy(nsIInputStream* inStr, nsIOutputStream* outStr, 
     char* buf, PRUint32 bufSize, PRUint32 *copyCount)
{
    nsresult rv;
    while (PR_TRUE) {
        PRUint32 count;
        rv = inStr->Read(buf, bufSize, &count);
        if (rv == NS_BASE_STREAM_EOF) break;
        if (NS_FAILED(rv)) return rv;
            
        PRUint32 writeCount;
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

    NS_IMETHOD Run() {
        nsresult rv;

        PRIntervalTime startTime, endTime;
        startTime = PR_IntervalNow();

        nsIInputStream* inStr = nsnull;
        nsIOutputStream* outStr = nsnull;
        nsISupports* str = nsnull;

        PRUint32 copyCount = 0;
        nsFileSpec inSpec(mInPath);
        nsFileSpec outSpec(mOutPath);
        
        // Open the input stream:
        rv = NS_NewTypicalInputFileStream(&str, inSpec);
        if (NS_FAILED(rv)) goto done;
        rv = str->QueryInterface(nsIInputStream::GetIID(), (void**)&inStr);
        NS_RELEASE(str);
        if (NS_FAILED(rv)) goto done;
        
        // Open the output stream:
        rv = NS_NewTypicalOutputFileStream(&str, outSpec);
        if (NS_FAILED(rv)) goto done;
        rv = str->QueryInterface(nsIOutputStream::GetIID(), (void**)&outStr);
        NS_RELEASE(str);
        if (NS_FAILED(rv)) goto done;

        // Copy from one to the other
        rv = Copy(inStr, outStr, mBuffer, mBufferSize, &copyCount);
        if (NS_FAILED(rv)) goto done;
        
        endTime = PR_IntervalNow();
        printf("FileSpecWorker copied %s to %s\n\tsize = %d time = %dms\n",
               mInPath, mOutPath, copyCount,
               PR_IntervalToMilliseconds(endTime - startTime));
        gTotalTime += endTime - startTime;
        
      done:
        NS_IF_RELEASE(outStr);
        NS_IF_RELEASE(inStr);
        NS_IF_RELEASE(str);
        return rv;
    }

    NS_DECL_ISUPPORTS

    FileSpecWorker()
        : mInPath(nsnull), mOutPath(nsnull), mBuffer(nsnull),
          mBufferSize(0)
    {
        NS_INIT_REFCNT();
    }

    nsresult Init(const char* inPath, const char* outPath,
                  PRUint32 bufferSize)
    {
        mInPath = nsCRT::strdup(inPath);
        mOutPath = nsCRT::strdup(outPath);
        mBuffer = new char[bufferSize];
        mBufferSize = bufferSize;
        return (mInPath && mOutPath && mBuffer)
            ? NS_OK : NS_ERROR_OUT_OF_MEMORY;
    }

    static nsresult Create(nsIRunnable* *result,
                           const char* inPath, 
                           const char* outPath, 
                           PRUint32 bufferSize)
    {
        FileSpecWorker* worker = new FileSpecWorker();
        if (worker == nsnull)
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
        nsCRT::free(mInPath);
        nsCRT::free(mOutPath);
        delete[] mBuffer;
    }

protected:
    char* mInPath;
    char* mOutPath;
    char* mBuffer;
    PRUint32 mBufferSize;
};

NS_IMPL_ISUPPORTS(FileSpecWorker, nsIRunnable::GetIID());

////////////////////////////////////////////////////////////////////////////////

#include "nsIIOService.h"
#include "nsIFileChannel.h"
#include "nsIFileProtocolHandler.h"

static NS_DEFINE_CID(kIOServiceCID, NS_IOSERVICE_CID);

class FileChannelWorker : public nsIRunnable {
public:

    NS_IMETHOD Run() {
        nsresult rv;

        NS_WITH_SERVICE(nsIIOService, ioserv, kIOServiceCID, &rv);
        if (NS_FAILED(rv)) return rv;

        nsIProtocolHandler* handler;
        rv = ioserv->GetProtocolHandler("file", &handler);
        if (NS_FAILED(rv)) return rv;

        nsIFileProtocolHandler* fileHandler = nsnull;
        rv = handler->QueryInterface(nsIFileProtocolHandler::GetIID(), 
                                     (void**)&fileHandler);
        NS_RELEASE(handler);
        if (NS_FAILED(rv)) return rv;

        PRIntervalTime startTime, endTime;
        startTime = PR_IntervalNow();

        PRUint32 copyCount = 0;
        nsIInputStream* inStr = nsnull;
        nsIOutputStream* outStr = nsnull;
        nsIFileChannel* inCh = nsnull;
        nsIFileChannel* outCh = nsnull;

        rv = fileHandler->NewChannelFromNativePath(mInPath, &inCh);
        if (NS_FAILED(rv)) goto done;

        rv = inCh->OpenInputStream(0, -1, &inStr);
        if (NS_FAILED(rv)) goto done;

        rv = fileHandler->NewChannelFromNativePath(mOutPath, &outCh);
        if (NS_FAILED(rv)) goto done;

        rv = outCh->OpenOutputStream(0, &outStr);
        if (NS_FAILED(rv)) goto done;

        // Copy from one to the other
        rv = Copy(inStr, outStr, mBuffer, mBufferSize, &copyCount);
        if (NS_FAILED(rv)) goto done;
        
        endTime = PR_IntervalNow();
        printf("FileSpecWorker copied %s to %s\n\tsize = %d time = %dms\n",
               mInPath, mOutPath, copyCount,
               PR_IntervalToMilliseconds(endTime - startTime));
        gTotalTime += endTime - startTime;
        
      done:
        NS_RELEASE(fileHandler);
        NS_RELEASE(inStr);
        NS_RELEASE(inCh);
        NS_RELEASE(outStr);
        NS_RELEASE(outCh);
        return rv;
    }

    NS_DECL_ISUPPORTS

    FileChannelWorker()
        : mInPath(nsnull), mOutPath(nsnull), mBuffer(nsnull),
          mBufferSize(0)
    {
        NS_INIT_REFCNT();
    }

    nsresult Init(const char* inPath, const char* outPath,
                  PRUint32 bufferSize)
    {
        mInPath = nsCRT::strdup(inPath);
        mOutPath = nsCRT::strdup(outPath);
        mBuffer = new char[bufferSize];
        mBufferSize = bufferSize;
        return (mInPath && mOutPath && mBuffer)
            ? NS_OK : NS_ERROR_OUT_OF_MEMORY;
    }

    static nsresult Create(nsIRunnable* *result,
                           const char* inPath, 
                           const char* outPath, 
                           PRUint32 bufferSize)
    {
        FileChannelWorker* worker = new FileChannelWorker();
        if (worker == nsnull)
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
        nsCRT::free(mInPath);
        nsCRT::free(mOutPath);
        delete[] mBuffer;
    }

protected:
    char* mInPath;
    char* mOutPath;
    char* mBuffer;
    PRUint32 mBufferSize;
};

NS_IMPL_ISUPPORTS(FileChannelWorker, nsIRunnable::GetIID());

////////////////////////////////////////////////////////////////////////////////

void
Test(CreateFun create, PRUint32 count,
     const char* inDir, const char* outDir, PRUint32 bufSize)
{
    nsresult rv;
    PRUint32 i;

    printf("============Test: from %s to %s\n\tthreads = %d bufSize = %d\n",
           inDir, outDir, count, bufSize);
    gTotalTime = 0;
    
    nsISupportsArray* threads;
    rv = NS_NewISupportsArray(&threads);
    NS_ASSERTION(NS_SUCCEEDED(rv), "NS_NewISupportsArray failed");

    nsFileSpec inDirSpec(inDir);
    nsDirectoryIterator iter(inDirSpec);
    for (i = 0; i < count && iter.Exists(); i++, iter++) {
        nsIThread* thread;
        nsIRunnable* worker;

        nsFileSpec& inSpec = iter.Spec();

        nsFileSpec outSpec(outDir);
        outSpec += inSpec.GetLeafName();
        if (outSpec.Exists()) {
            outSpec.Delete(PR_FALSE);
        }
        
        rv = create(&worker, 
                    inSpec.GetNativePathCString(),
                    outSpec.GetNativePathCString(),
                    bufSize);
        if (NS_FAILED(rv)) goto done;

        rv = NS_NewThread(&thread, worker);
        NS_RELEASE(worker);
        if (NS_FAILED(rv)) goto done;

        PRBool inserted = threads->InsertElementAt(thread, i);
        NS_ASSERTION(inserted, "not inserted");
    }

    PRUint32 j;
    for (j = 0; j < i; j++) {
        nsIThread* thread = (nsIThread*)threads->ElementAt(j);
        if (NS_FAILED(rv)) goto done;
        thread->Join();
    }

  done:
    NS_RELEASE(threads);
    NS_ASSERTION(rv == NS_OK, "failed");

    printf("End Test: time = %dms\n", 
           PR_IntervalToMilliseconds(gTotalTime));
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

    rv = nsComponentManager::AutoRegister(nsIComponentManager::NS_Startup,
                                          "components");
    if (NS_FAILED(rv)) return rv;

    CreateFun create = FileChannelWorker::Create;       // FileSpecWorker::Create
    Test(create, 1, inDir, outDir, 16 * 1024);
    Test(create, 1, inDir, outDir, 16 * 1024);
#if 0
    printf("FileChannelWorker::Create *****************************\n");
    Test(create, 8, inDir, outDir, 16 * 1024);
    Test(create, 8, inDir, outDir, 16 * 1024);
    Test(create, 8, inDir, outDir, 16 * 1024);
    Test(create, 8, inDir, outDir, 16 * 1024);
    Test(create, 8, inDir, outDir, 16 * 1024);
    Test(create, 8, inDir, outDir, 16 * 1024);
    Test(create, 8, inDir, outDir, 16 * 1024);
    Test(create, 8, inDir, outDir, 16 * 1024);
    Test(create, 8, inDir, outDir, 16 * 1024);
#endif
#if 0
    create = FileSpecWorker::Create;
    printf("FileSpecWorker::Create ********************************\n");

    Test(create, 8, inDir, outDir, 16 * 1024);
    Test(create, 8, inDir, outDir, 16 * 1024);
    Test(create, 8, inDir, outDir, 16 * 1024);
    Test(create, 8, inDir, outDir, 16 * 1024);
    Test(create, 8, inDir, outDir, 16 * 1024);
    Test(create, 8, inDir, outDir, 16 * 1024);
    Test(create, 8, inDir, outDir, 16 * 1024);
    Test(create, 8, inDir, outDir, 16 * 1024);
    Test(create, 8, inDir, outDir, 16 * 1024);
#endif
#if 0
    Test(create, 8, inDir, outDir, 4 * 1024);
    Test(create, 8, inDir, outDir, 4 * 1024);
    Test(create, 8, inDir, outDir, 4 * 1024);
    Test(create, 8, inDir, outDir, 4 * 1024);
    Test(create, 8, inDir, outDir, 4 * 1024);
    Test(create, 8, inDir, outDir, 4 * 1024);
    Test(create, 8, inDir, outDir, 4 * 1024);
    Test(create, 8, inDir, outDir, 4 * 1024);
    Test(create, 8, inDir, outDir, 4 * 1024);
#endif

    return 0;
}

////////////////////////////////////////////////////////////////////////////////
