/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
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
 * either of the GNU General Public License Version 2 or later (the "GPL"),
 * or the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#include "nsThreadUtils.h"
#include <stdio.h>
#include <stdlib.h>
#include "nspr.h"
#include "nsCOMPtr.h"
#include "nsIServiceManager.h"
#include "nsXPCOM.h"

class nsRunner : public nsIRunnable {
public:
    NS_DECL_ISUPPORTS

    NS_IMETHOD Run() {
        nsCOMPtr<nsIThread> thread;
        nsresult rv = NS_GetCurrentThread(getter_AddRefs(thread));
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
    }

protected:
    int mNum;
};

NS_IMPL_THREADSAFE_ISUPPORTS1(nsRunner, nsIRunnable)

nsresult
TestThreads()
{
    nsresult rv;

    nsCOMPtr<nsIRunnable> event = new nsRunner(0);
    if (!event)
        return NS_ERROR_OUT_OF_MEMORY;

    nsCOMPtr<nsIThread> runner;
    rv = NS_NewThread(getter_AddRefs(runner), event);
    if (NS_FAILED(rv)) {
        printf("failed to create thread\n");
        return rv;
    }

    nsCOMPtr<nsIThread> thread;
    rv = NS_GetCurrentThread(getter_AddRefs(thread));
    if (NS_FAILED(rv)) {
        printf("failed to get current thread\n");
        return rv;
    }

    rv = runner->Shutdown();     // wait for the runner to die before quitting
    if (NS_FAILED(rv)) {
        printf("join failed\n");        
    }

    PR_Sleep(PR_MillisecondsToInterval(100));       // hopefully the runner will quit here

    return NS_OK;
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
        PR_AtomicIncrement(&gNum);
    }

    static PRInt32 GetGlobalCount() {return gNum;}

private:
    ~nsStressRunner() {
        NS_ASSERTION(mWasRun, "never run!");
    }

protected:
    static PRInt32 gNum;
    PRInt32 mNum;
    bool mWasRun;
};

PRInt32 nsStressRunner::gNum = 0;

NS_IMPL_THREADSAFE_ISUPPORTS1(nsStressRunner, nsIRunnable)

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
            nsresult rv = NS_NewThread(getter_AddRefs(t), new nsStressRunner(k));
            if (NS_FAILED(rv)) {
                NS_ERROR("can't create thread");
                return -1;
            }
            NS_ADDREF(array[k] = t);
        }

        for (k = threads-1; k >= 0; k--) {
            array[k]->Shutdown();
            NS_RELEASE(array[k]);    
        }
        delete [] array;
    }
    return 0;
}

static void threadProc(void *arg)
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
    }

    rv = NS_ShutdownXPCOM(nsnull);
    if (NS_FAILED(rv)) return -1;
    return retval;
}
