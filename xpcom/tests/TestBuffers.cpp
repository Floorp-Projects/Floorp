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

#include "nsIBuffer.h"
#include "nsIThread.h"
#include "nsIComponentManager.h"
#include "nsIServiceManager.h"
#include "nsAutoLock.h"
#include "nsIPageManager.h"
#include "nsCRT.h"
#include "prprf.h"
#include "prmem.h"
#include "prinrval.h"
#include <stdio.h>

PRBool gVerbose = PR_FALSE;

class Reader : public nsIRunnable {
public:
    NS_DECL_ISUPPORTS

    NS_IMETHOD Run() {
        nsresult rv;
        nsAutoCMonitor mon(this);

        char buf[100];
        PRUint32 readCnt;
        const char* readBuf;
        rv = mReadBuffer->GetReadSegment(0, &readBuf, &readCnt);
        if (NS_FAILED(rv)) return rv;
        while (!(mDone && readCnt == 0)) {
            rv = mReadBuffer->Read(buf, 99, &readCnt);
            if (NS_FAILED(rv)) {
                printf("read failed\n");
                return rv;
            }
            if (readCnt == 0) {
                if (mDone)
                    break;
                mon.Notify(); // wake up writer
                mon.Wait();   // wait for more
                rv = mReadBuffer->GetReadSegment(0, &readBuf, &readCnt);
                if (NS_FAILED(rv)) return rv;
            }
            else {
                if (gVerbose) {
                    buf[readCnt] = 0;
                    printf(buf);
                }
                mBytesRead += readCnt;
            }
        }
        if (gVerbose)
            printf("reader done\n");
        return rv;
    }

    Reader(nsIBuffer* readBuffer)
        : mReadBuffer(readBuffer), mBytesRead(0), mDone(PR_FALSE) {
        NS_INIT_REFCNT();
        NS_ADDREF(mReadBuffer);
    }

    virtual ~Reader() {
        NS_RELEASE(mReadBuffer);
        if (gVerbose)
            printf("bytes read = %d\n", mBytesRead);
    }

    void SetEOF() {
        mDone = PR_TRUE;
    }

    PRUint32 GetBytesRead() { return mBytesRead; }

protected:
    nsIBuffer*  mReadBuffer;
    PRUint32    mBytesRead;
    PRBool      mDone;
};

NS_IMPL_ISUPPORTS1(Reader, nsIRunnable)

////////////////////////////////////////////////////////////////////////////////

#define ITERATIONS      20000

nsresult
WriteMessages(nsIBuffer* buffer)
{
    nsresult rv;
    nsIThread* thread;
    Reader* reader = new Reader(buffer);
    NS_ADDREF(reader);
    rv = NS_NewThread(&thread, reader, 0, PR_JOINABLE_THREAD);
    if (NS_FAILED(rv)) {
        printf("failed to create thread\n");
        return rv;
    }

    char* mem = nsnull;
    char* buf = nsnull;
    PRUint32 total = 0;
    PRUint32 cnt = 0;
    PRUint32 bufLen = 0;
    PRIntervalTime start = PR_IntervalNow();
    for (PRUint32 i = 0; i < ITERATIONS;) {
        if (bufLen == 0) {
            PR_FREEIF(mem);
            mem = PR_smprintf("%d. My hovercraft is full of eels.\n", i);
            buf = mem;
            NS_ASSERTION(buf, "out of memory");
            bufLen = nsCRT::strlen(buf);
            i++;
        }
        rv = buffer->Write(buf, bufLen, &cnt);
        if (NS_FAILED(rv)) {
            printf("failed to write %d\n", i);
            return rv;
        }
        if (cnt == 0) {
            nsAutoCMonitor mon(reader);
            mon.Notify();       // wake up reader
            mon.Wait();         // and wait for reader to read all
        }
        else {
            total += cnt;
            buf += cnt;
            bufLen -= cnt;
        }
    }

    PR_FREEIF(mem);
    {
        reader->SetEOF();
        nsAutoCMonitor mon(reader);
        mon.Notify();       // wake up reader
    }

    if (NS_FAILED(rv)) return rv;
    rv = thread->Join();
    PRIntervalTime end = PR_IntervalNow();
    if (NS_FAILED(rv)) return rv;
    printf("writer done: %d bytes %d ms\n", total,
           PR_IntervalToMilliseconds(end - start));
    NS_ASSERTION(reader->GetBytesRead() == total, "didn't read everything");

    NS_RELEASE(reader);
    NS_RELEASE(thread);
    NS_RELEASE(buffer);
    return NS_OK;
}

////////////////////////////////////////////////////////////////////////////////

nsresult
TestMallocBuffers(PRUint32 growByPages, PRUint32 pageCount)
{
    nsresult rv;
    nsIBuffer* buffer;
    printf("Malloc Buffer Test: %d pages, grow by %d pages\n",
           pageCount, growByPages);
    rv = NS_NewBuffer(&buffer,
                      NS_PAGEMGR_PAGE_SIZE * growByPages, 
                      NS_PAGEMGR_PAGE_SIZE * pageCount,
                      nsnull);
    if (NS_FAILED(rv)) {
        printf("failed to create buffer\n");
        return rv;
    }

    rv = WriteMessages(buffer);
    return rv;
}

nsresult
TestPageBuffers(PRUint32 growByPages, PRUint32 pageCount)
{
    nsresult rv;
    nsIBuffer* buffer;
    printf("Page Buffer Test: %d pages, grow by %d pages\n",
           pageCount, growByPages);
    rv = NS_NewPageBuffer(&buffer,
                          NS_PAGEMGR_PAGE_SIZE * growByPages,
                          NS_PAGEMGR_PAGE_SIZE * pageCount,
                          nsnull);
    if (NS_FAILED(rv)) {
        printf("failed to create buffer\n");
        return rv;
    }

    rv = WriteMessages(buffer);
    return rv;
}

////////////////////////////////////////////////////////////////////////////////

void
TestSearch(const char* delim, PRUint32 segDataSize)
{
    nsresult rv;
    nsIBuffer* buffer;
    // need at least 2 segments to test boundary conditions:
    PRUint32 bufDataSize = segDataSize * 2;
    PRUint32 segSize = segDataSize + nsIBuffer::SEGMENT_OVERHEAD;
    PRUint32 bufSize = segSize * 2;
    rv = NS_NewBuffer(&buffer, segSize, bufSize, nsnull);
    NS_ASSERTION(NS_SUCCEEDED(rv), "NewBuffer failed");

    PRUint32 i, j, amt;
    PRUint32 delimLen = nsCRT::strlen(delim);
    for (i = 0; i < bufDataSize; i++) {
        // first fill the buffer
        for (j = 0; j < i; j++) {
            rv = buffer->Write("-", 1, &amt);
            NS_ASSERTION(NS_SUCCEEDED(rv) && amt == 1, "Write failed");
        }
        rv = buffer->Write(delim, delimLen, &amt);
        NS_ASSERTION(NS_SUCCEEDED(rv), "Write failed");
        if (i + amt < bufDataSize) {
            for (j = i + amt; j < bufDataSize; j++) {
                rv = buffer->Write("+", 1, &amt);
                NS_ASSERTION(NS_SUCCEEDED(rv) && amt == 1, "Write failed");
            }
        }
        
        // now search for the delimiter
        PRBool found;
        PRUint32 offset;
        rv = buffer->Search(delim, PR_FALSE, &found, &offset);
        NS_ASSERTION(NS_SUCCEEDED(rv), "Search failed");

        // print the results
        char* bufferContents = new char[bufDataSize + 1];
        rv = buffer->Read(bufferContents, bufDataSize, &amt);
        NS_ASSERTION(NS_SUCCEEDED(rv) && amt == bufDataSize, "Read failed");
        bufferContents[bufDataSize] = '\0';
        printf("Buffer: %s\nDelim: %s %s offset: %d\n", bufferContents,
               delim, (found ? "found" : "not found"), offset);
    }
}

////////////////////////////////////////////////////////////////////////////////

NS_METHOD
FailingReader(void* closure,
              char* toRawSegment,
              PRUint32 fromOffset,
              PRUint32 count,
              PRUint32 *readCount)
{
    nsresult* rv = (nsresult*)closure;
    *readCount = 0;
    return *rv;
}

nsresult
TestWriteSegments(nsresult error)
{
    nsresult rv;
    nsIBuffer* buffer;
    printf("WriteSegments Test: error %x\n", error);
    rv = NS_NewBuffer(&buffer, 10, 10, nsnull);
    if (NS_FAILED(rv)) {
        printf("failed to create buffer\n");
        return rv;
    }

    PRUint32 writeCount;
    rv = buffer->WriteSegments(FailingReader, &error, 1, &writeCount);
    NS_ASSERTION(rv == error, "WriteSegments failed");

    NS_RELEASE(buffer);
    return NS_OK;
}

////////////////////////////////////////////////////////////////////////////////

NS_METHOD
FailingWriter(void* closure,
              const char* fromRawSegment,
              PRUint32 toOffset,
              PRUint32 count,
              PRUint32 *writeCount)
{
    nsresult* rv = (nsresult*)closure;
    *writeCount = 0;
    return *rv;
}

nsresult
TestReadSegments(nsresult error)
{
    nsresult rv;
    nsIBuffer* buffer;
    printf("ReadSegments Test: error %x\n", error);
    rv = NS_NewBuffer(&buffer, 10, 10, nsnull);
    if (NS_FAILED(rv)) {
        printf("failed to create buffer\n");
        return rv;
    }

    PRUint32 writeCount;
    rv = buffer->Write("x", 1, &writeCount);
    NS_ASSERTION(NS_SUCCEEDED(rv) && writeCount == 1, "Write failed");

    PRUint32 readCount;
    rv = buffer->ReadSegments(FailingWriter, &error, 1, &readCount);
    NS_ASSERTION(rv == error, "ReadSegments failed");

    NS_RELEASE(buffer);
    return NS_OK;
}

////////////////////////////////////////////////////////////////////////////////

int
main(int argc, char* argv[])
{
    nsresult rv;
    nsIServiceManager* servMgr;

    if (argc >= 2 && nsCRT::strcmp(argv[1], "-verbose") == 0) {
        gVerbose = PR_TRUE;
    }

    rv = NS_InitXPCOM(&servMgr, NULL);
    if (NS_FAILED(rv)) return rv;

    rv = TestWriteSegments(NS_OK);
    NS_ASSERTION(NS_SUCCEEDED(rv), "TestWriteSegments failed");
    rv = TestWriteSegments(NS_ERROR_FAILURE);
    NS_ASSERTION(NS_SUCCEEDED(rv), "TestWriteSegments failed");
    rv = TestWriteSegments(NS_BASE_STREAM_CLOSED);
    NS_ASSERTION(NS_SUCCEEDED(rv), "TestWriteSegments failed");
    rv = TestWriteSegments(NS_BASE_STREAM_WOULD_BLOCK);
    NS_ASSERTION(NS_SUCCEEDED(rv), "TestWriteSegments failed");

    rv = TestReadSegments(NS_OK);
    NS_ASSERTION(NS_SUCCEEDED(rv), "TestReadSegments failed");
    rv = TestReadSegments(NS_ERROR_FAILURE);
    NS_ASSERTION(NS_SUCCEEDED(rv), "TestReadSegments failed");
//    rv = TestReadSegments(NS_BASE_STREAM_CLOSED);
//    NS_ASSERTION(NS_SUCCEEDED(rv), "TestReadSegments failed");
    rv = TestReadSegments(NS_BASE_STREAM_WOULD_BLOCK);
    NS_ASSERTION(NS_SUCCEEDED(rv), "TestReadSegments failed");

    rv = TestMallocBuffers(1, 1);
    NS_ASSERTION(NS_SUCCEEDED(rv), "TestMallocBuffers failed");

    rv = TestPageBuffers(1, 1);
    NS_ASSERTION(NS_SUCCEEDED(rv), "TestPageBuffers failed");

    rv = TestMallocBuffers(1, 10);
    NS_ASSERTION(NS_SUCCEEDED(rv), "TestMallocBuffers failed");

    rv = TestPageBuffers(1, 10);
    NS_ASSERTION(NS_SUCCEEDED(rv), "TestPageBuffers failed");

    rv = TestMallocBuffers(5, 10);
    NS_ASSERTION(NS_SUCCEEDED(rv), "TestMallocBuffers failed");

    rv = TestPageBuffers(5, 10);
    NS_ASSERTION(NS_SUCCEEDED(rv), "TestPageBuffers failed");

    TestSearch("foo", 8);
    TestSearch("bar", 6);
    TestSearch("baz", 2);

    return 0;
}

////////////////////////////////////////////////////////////////////////////////
