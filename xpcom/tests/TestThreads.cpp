/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * The contents of this file are subject to the Netscape Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
 *   Pierre Phaneuf <pp@ludusdesign.com>
 */

#include "nsIThread.h"
#include "nsIThreadPool.h"
#include "nsIRunnable.h"
#include <stdio.h>
#include "nsCOMPtr.h"
#include "nsIServiceManager.h"

class nsRunner : public nsIRunnable {
public:
    NS_DECL_ISUPPORTS

    NS_IMETHOD Run() {
        nsCOMPtr<nsIThread> thread;
        nsresult rv = nsIThread::GetCurrent(getter_AddRefs(thread));
        if (NS_FAILED(rv)) {
            printf("failed to get current thread\n");
            return rv;
        }
        printf("running %d on thread %x\n", mNum, thread);

        // if we don't do something slow, we'll never see the other
        // worker threads run
        PR_Sleep(PR_MillisecondsToInterval(100));

        return rv;
    }

    nsRunner(int num) : mNum(num) {
        NS_INIT_REFCNT();
    }

protected:
    int mNum;
};

NS_IMPL_THREADSAFE_ISUPPORTS1(nsRunner, nsIRunnable);

nsresult
TestThreads()
{
    nsresult rv;

    nsCOMPtr<nsIThread> runner;
    rv = NS_NewThread(getter_AddRefs(runner), new nsRunner(0), 0, PR_JOINABLE_THREAD);
    if (NS_FAILED(rv)) {
        printf("failed to create thread\n");
        return rv;
    }

    nsCOMPtr<nsIThread> thread;
    rv = nsIThread::GetCurrent(getter_AddRefs(thread));
    if (NS_FAILED(rv)) {
        printf("failed to get current thread\n");
        return rv;
    }

    PRThreadScope scope;
    rv = runner->GetScope(&scope);
    if (NS_FAILED(rv)) {
        printf("runner already exited\n");        
    }

    rv = runner->Join();     // wait for the runner to die before quitting
    if (NS_FAILED(rv)) {
        printf("join failed\n");        
    }

    rv = runner->GetScope(&scope);      // this should fail after Join
    if (NS_SUCCEEDED(rv)) {
        printf("get scope failed\n");        
    }

    rv = runner->Interrupt();   // this should fail after Join
    if (NS_SUCCEEDED(rv)) {
        printf("interrupt failed\n");        
    }

    ////////////////////////////////////////////////////////////////////////////
    // try an unjoinable thread 
    rv = NS_NewThread(getter_AddRefs(runner), new nsRunner(1));
    if (NS_FAILED(rv)) {
        printf("failed to create thread\n");
        return rv;
    }

    rv = runner->Join();     // wait for the runner to die before quitting
    if (NS_SUCCEEDED(rv)) {
        printf("shouldn't have been able to join an unjoinable thread\n");        
    }

    PR_Sleep(PR_MillisecondsToInterval(100));       // hopefully the runner will quit here

    return NS_OK;
}

nsresult
TestThreadPools(PRUint32 poolMinSize, PRUint32 poolMaxSize, 
                PRUint32 nRequests, PRIntervalTime dispatchWaitInterval = 0)
{
    nsCOMPtr<nsIThreadPool> pool;
    nsresult rv = NS_NewThreadPool(getter_AddRefs(pool), poolMinSize, poolMaxSize);
    if (NS_FAILED(rv)) {
        printf("failed to create thead pool\n");
        return rv;
    }

    for (PRUint32 i = 0; i < nRequests; i++) {
        rv = pool->DispatchRequest(new nsRunner(i+2));
        if (dispatchWaitInterval && i % poolMaxSize == poolMaxSize - 1) {
            PR_Sleep(dispatchWaitInterval);
        }
    }
    rv = pool->Shutdown();
    return rv;
}

int
main()
{
    nsresult rv;
    rv = NS_InitXPCOM(nsnull, nsnull);
    if (NS_FAILED(rv)) return -1;

    rv = TestThreads();
    if (NS_FAILED(rv)) return -1;

    rv = TestThreadPools(1, 4, 100);
    if (NS_FAILED(rv)) return -1;

    rv = TestThreadPools(4, 16, 100);
    if (NS_FAILED(rv)) return -1;

    // this test delays between each request to give threads a chance to 
    // decide to go away:
    rv = TestThreadPools(4, 8, 32, PR_MillisecondsToInterval(1000));
    if (NS_FAILED(rv)) return -1;

    rv = NS_ShutdownXPCOM(nsnull);
    if (NS_FAILED(rv)) return -1;
    return 0;
}
