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

int
main(int argc, char* argv[])
{
    nsresult rv;
    nsIBufferInputStream* in;
    nsIBufferOutputStream* out;
    nsIServiceManager* servMgr;

    rv = NS_InitXPCOM(&servMgr);
    if (NS_FAILED(rv)) return rv;

    if (argc > 1 && nsCRT::strcmp(argv[1], "-trace") == 0)
        gTrace = PR_TRUE;
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
    // test for small buffers
    rv = NS_NewPipe(&in, &out, 4096, 4096*16, PR_TRUE, nsnull);
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

    // test for large buffers
    rv = NS_NewPipe(&in, &out, 4096, 4096*16, PR_TRUE, nsnull);
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

    return 0;
}
