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
#include "nsIThreadPool.h"
#include "nsIRunnable.h"
#include <stdio.h>
#include <stdlib.h>
#include "nspr.h"
#include "nsCOMPtr.h"
#include "nsIServiceManager.h"

class nsConcurrentRunner : public nsIRunnable {
public:
    NS_DECL_ISUPPORTS

    NS_IMETHOD Run() {
        nsCOMPtr<nsIThread> thread;
        nsresult rv = nsIThread::GetCurrent(getter_AddRefs(thread));
        if (NS_FAILED(rv)) {
            printf("failed to get current thread\n");
            return rv;
        }

        mTestInt = 666;
        PR_Sleep(PR_SecondsToInterval(1));

        if (mTestInt != 666)
            printf("Illegal access.  Data corruption detected.\n");

        mTestInt = 0;
        PR_Sleep(PR_SecondsToInterval(2));

        if (mTestInt != 0)
            printf("Illegal access.  Data corruption detected.\n");

        // if we don't do something slow, we'll never see the other
        // worker threads run
        PR_Sleep(PR_MillisecondsToInterval(1));

        return rv;
    }

    nsConcurrentRunner(){
        NS_INIT_REFCNT();
        mTestInt = 0;
    }

private:
    PRInt32 mTestInt;

};

NS_IMPL_THREADSAFE_ISUPPORTS1(nsConcurrentRunner, nsIRunnable);


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
        printf("running %d on thread %p\n", mNum, (void *)thread.get());

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

nsresult
TestThreadPoolsConcurrent(PRUint32 poolMinSize, 
                          PRUint32 poolMaxSize, 
                          PRUint32 nRequests)
{
    nsCOMPtr<nsIThreadPool> pool;
    nsresult rv = NS_NewThreadPool(getter_AddRefs(pool), poolMinSize, poolMaxSize);
    if (NS_FAILED(rv)) {
        printf("failed to create thead pool\n");
        return rv;
    }
    
    nsConcurrentRunner* runner = new nsConcurrentRunner();

    for (PRUint32 i = 0; i < nRequests; i++) {
        rv = pool->DispatchRequest(runner);
    }
    rv = pool->Shutdown();
    return rv;
}

class nsStressRunner : public nsIRunnable {
public:
    NS_DECL_ISUPPORTS

    NS_IMETHOD Run() {
        NS_ASSERTION(!mWasRun, "run twice!");
        mWasRun = PR_TRUE;
        PR_Sleep(1);
        if (!PR_AtomicDecrement(&gNum)) {
            printf("   last thread was %d\n", mNum);
        }
        return NS_OK;
    }

    nsStressRunner(int num) : mNum(num), mWasRun(PR_FALSE) {
        NS_INIT_REFCNT();
        PR_AtomicIncrement(&gNum);
    }

    virtual ~nsStressRunner() {
        NS_ASSERTION(mWasRun, "never run!");
    }

    static PRInt32 GetGlobalCount() {return gNum;}

protected:
    static PRInt32 gNum;
    PRInt32 mNum;
    PRBool mWasRun;
};

PRInt32 nsStressRunner::gNum = 0;

NS_IMPL_THREADSAFE_ISUPPORTS1(nsStressRunner, nsIRunnable);

static int Stress(int loops, int threads)
{

    for (int i = 0; i < loops; i++) {
        printf("Loop %d of %d\n", i+1, loops);

        int k;
        nsIThread** array = new nsIThread*[threads];
        NS_ASSERTION(array, "out of memory");

        NS_ASSERTION(!nsStressRunner::GetGlobalCount(), "bad count of runnables");
        
        for (k = 0; k < threads; k++) {
            nsCOMPtr<nsIThread> t;
            nsresult rv = NS_NewThread(getter_AddRefs(t), 
                                       new nsStressRunner(k),
                                       0, PR_JOINABLE_THREAD);
            NS_ASSERTION(NS_SUCCEEDED(rv), "can't create thread");
            NS_ADDREF(array[k] = t);
        }

        for (k = threads-1; k >= 0; k--) {
            array[k]->Join();
            NS_RELEASE(array[k]);    
        }
        delete [] array;
    }
    return 0;
}

PR_STATIC_CALLBACK(void) threadProc(void *arg)
{
    // printf("   running thread %d\n", (int) arg);
    PR_Sleep(1);
    PR_ASSERT(PR_JOINABLE_THREAD == PR_GetThreadState(PR_GetCurrentThread()));
}

static int StressNSPR(int loops, int threads)
{

    for (int i = 0; i < loops; i++) {
        printf("Loop %d of %d\n", i+1, loops);

        int k;
        PRThread** array = new PRThread*[threads];
        PR_ASSERT(array);

        for (k = 0; k < threads; k++) {
            array[k] = PR_CreateThread(PR_USER_THREAD,
                                       threadProc, (void*) k,
                                       PR_PRIORITY_NORMAL,
                                       PR_GLOBAL_THREAD,
                                       PR_JOINABLE_THREAD,
                                       0);
            PR_ASSERT(array[k]);
        }                               

        for (k = 0; k < threads; k++) {
            PR_ASSERT(PR_JOINABLE_THREAD == PR_GetThreadState(array[k]));
        }                               

        for (k = threads-1; k >= 0; k--) {
            PR_JoinThread(array[k]);
        }
        delete [] array;
    }
    return 0;
}


int
main(int argc, char** argv)
{
    int retval = 0;
    nsresult rv;
    
    rv = NS_InitXPCOM2(nsnull, nsnull, nsnull);
    if (NS_FAILED(rv)) return -1;

    if (argc > 1 && !strcmp(argv[1], "-stress")) {
        int loops;
        int threads;
        if (argc != 4 || *argv[2] != '-' || *argv[3] != '-' ||
            !(loops = atoi(argv[2]+1)) || !(threads = atoi(argv[3]+1))) {
           printf("To use -stress you must pass loop count and thread count...\n"
                  "   TestThreads -stress -1000 -50\n");
        } else {
           printf("Running stress test with %d loops of %d threads each\n",
                  loops, threads);
           retval = Stress(loops, threads);
        }
    } else if (argc > 1 && !strcmp(argv[1], "-stress-nspr")) {
        int loops;
        int threads;
        if (argc != 4 || *argv[2] != '-' || *argv[3] != '-' ||
            !(loops = atoi(argv[2]+1)) || !(threads = atoi(argv[3]+1))) {
           printf("To use -stress-nspr you must pass loop count and thread count...\n"
                  "   TestThreads -stress -1000 -50\n");
        } else {
           printf("Running stress test with %d loops of %d threads each\n",
                  loops, threads);
           retval = StressNSPR(loops, threads);
        }
    } else {
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

        rv = TestThreadPoolsConcurrent(4, 32, 1000);
        if (NS_FAILED(rv)) return -1;
    }

    rv = NS_ShutdownXPCOM(nsnull);
    if (NS_FAILED(rv)) return -1;
    return retval;
}
