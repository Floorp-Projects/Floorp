/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
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

#include <stdio.h>

#include "nsXPCOM.h"
#include "nsXPCOMCIDInternal.h"
#include "nsIComponentManager.h"
#include "nsIComponentRegistrar.h"
#include "nsIServiceManager.h"
#include "nsAutoPtr.h"
#include "nsCOMPtr.h"
#include "nsCOMArray.h"

#include "nscore.h"
#include "nspr.h"
#include "prmon.h"

#include "nsITestProxy.h"

#include "nsIRunnable.h"
#include "nsIProxyObjectManager.h"
#include "nsIThreadPool.h"
#include "nsXPCOMCIDInternal.h"
#include "nsComponentManagerUtils.h"
#include "nsServiceManagerUtils.h"
#include "nsThreadUtils.h"

#include "prlog.h"
#ifdef PR_LOGGING
static PRLogModuleInfo *sLog = PR_NewLogModule("Test");
#define LOG(args) PR_LOG(sLog, PR_LOG_DEBUG, args)
#else
#define LOG(args) printf args
#endif

namespace proxytests {

static nsresult
GetThreadFromPRThread(PRThread *prthread, nsIThread **result)
{
  LOG(("TEST: GetThreadFromPRThread [%p]\n", prthread));

  nsCOMPtr<nsIThreadManager> tm = do_GetService(NS_THREADMANAGER_CONTRACTID);
  NS_ENSURE_STATE(tm);
  return tm->GetThreadFromPRThread(prthread, result);
}

/***************************************************************************/
/* nsTestXPCFoo                                                            */
/***************************************************************************/
class nsTestXPCFoo : public nsITestProxy
{
    NS_DECL_ISUPPORTS
    NS_IMETHOD Test(PRInt32 p1, PRInt32 p2, PRInt32* retval);
    NS_IMETHOD Test2();
    NS_IMETHOD Test3(nsISupports *p1, nsISupports **p2);

    nsTestXPCFoo();
};

nsTestXPCFoo::nsTestXPCFoo()
{
    NS_ADDREF_THIS();
}

NS_IMPL_ISUPPORTS1(nsTestXPCFoo, nsITestProxy)

NS_IMETHODIMP nsTestXPCFoo::Test(PRInt32 p1, PRInt32 p2, PRInt32* retval)
{
    LOG(("TEST: Thread (%d) Test Called successfully! Party on...\n", p1));
    *retval = p1+p2;
    return NS_OK;
}


NS_IMETHODIMP nsTestXPCFoo::Test2()
{
    LOG(("TEST: The quick brown netscape jumped over the old lazy ie..\n"));

    return NS_OK;
}

NS_IMETHODIMP nsTestXPCFoo::Test3(nsISupports *p1, nsISupports **p2)
{
    if (p1 != nsnull)
    {
        nsITestProxy *test;

        p1->QueryInterface(NS_GET_IID(nsITestProxy), (void**)&test);
        
        test->Test2();
        PRInt32 a;
        test->Test( 1, 2, &a);
        LOG(("TEST: \n1+2=%d\n",a));
    }


    *p2 = new nsTestXPCFoo();
    return NS_OK;
}

/***************************************************************************/
/* nsTestXPCFoo2                                                           */
/***************************************************************************/
class nsTestXPCFoo2 : public nsITestProxy
{
    NS_DECL_ISUPPORTS
    NS_IMETHOD Test(PRInt32 p1, PRInt32 p2, PRInt32* retval);
    NS_IMETHOD Test2();
    NS_IMETHOD Test3(nsISupports *p1, nsISupports **p2);

    nsTestXPCFoo2();
};

nsTestXPCFoo2::nsTestXPCFoo2()
{
    NS_ADDREF_THIS();
}

NS_IMPL_THREADSAFE_ISUPPORTS1(nsTestXPCFoo2, nsITestProxy)

NS_IMETHODIMP nsTestXPCFoo2::Test(PRInt32 p1, PRInt32 p2, PRInt32* retval)
{
    LOG(("TEST: calling back to caller!\n"));

    nsCOMPtr<nsIProxyObjectManager> manager =
            do_GetService(NS_XPCOMPROXY_CONTRACTID);

    LOG(("TEST: ProxyObjectManager: %p \n", (void *) manager.get()));
    
    PR_ASSERT(manager);

    nsCOMPtr<nsIThread> thread;
    GetThreadFromPRThread((PRThread *) p1, getter_AddRefs(thread));
    NS_ENSURE_STATE(thread);

    nsCOMPtr<nsITestProxy> proxyObject;
    manager->GetProxyForObject(thread, NS_GET_IID(nsITestProxy), this, NS_PROXY_SYNC, (void**)&proxyObject);
    proxyObject->Test3(nsnull, nsnull);
    
    LOG(("TEST: Deleting Proxy Object\n"));
    return NS_OK;
}


NS_IMETHODIMP nsTestXPCFoo2::Test2()
{
    LOG(("TEST: nsTestXPCFoo2::Test2() called\n"));

    return NS_OK;
}


NS_IMETHODIMP nsTestXPCFoo2::Test3(nsISupports *p1, nsISupports **p2)
{
    LOG(("TEST: Got called"));
    return NS_OK;
}



#if 0
struct ArgsStruct {
    nsIThread* thread;
    PRInt32    threadNumber;
};



// This will create two objects both descendants of a single IID.
void TestCase_TwoClassesOneInterface(void *arg)
{
    ArgsStruct *argsStruct = (ArgsStruct*) arg;


    nsCOMPtr<nsIProxyObjectManager> manager =
            do_GetService(NS_XPCOMPROXY_CONTRACTID);

    printf("ProxyObjectManager: %p \n", (void *) manager.get());
    
    PR_ASSERT(manager);

    nsITestProxy         *proxyObject;
    nsITestProxy         *proxyObject2;

    nsTestXPCFoo*        foo   = new nsTestXPCFoo();
    nsTestXPCFoo2*       foo2  = new nsTestXPCFoo2();
    
    PR_ASSERT(foo);
    PR_ASSERT(foo2);
    
    
    manager->GetProxyForObject(argsStruct->thread, NS_GET_IID(nsITestProxy), foo, NS_PROXY_SYNC, (void**)&proxyObject);
    
    manager->GetProxyForObject(argsStruct->thread, NS_GET_IID(nsITestProxy), foo2, NS_PROXY_SYNC, (void**)&proxyObject2);

    
    
    if (proxyObject && proxyObject2)
    {
    // release ownership of the real object. 
        
        PRInt32 a;
        nsresult rv;
        PRInt32 threadNumber = argsStruct->threadNumber;
        
        printf("Deleting real Object (%d)\n", threadNumber);
        NS_RELEASE(foo);
   
        printf("Deleting real Object 2 (%d)\n", threadNumber);
        NS_RELEASE(foo2);


        printf("Thread (%d) Prior to calling proxyObject->Test.\n", threadNumber);
        rv = proxyObject->Test(threadNumber, 0, &a);   
        printf("Thread (%d) error: %d.\n", threadNumber, rv);


        printf("Thread (%d) Prior to calling proxyObject->Test2.\n", threadNumber);
        rv = proxyObject->Test2();   
        printf("Thread (%d) error: %d.\n", threadNumber, rv);

        printf("Thread (%d) Prior to calling proxyObject2->Test2.\n", threadNumber);
        rv = proxyObject2->Test2();   
        printf("Thread (%d) proxyObject2 error: %d.\n", threadNumber, rv);

        printf("Deleting Proxy Object (%d)\n", threadNumber );
        NS_RELEASE(proxyObject);

        printf("Deleting Proxy Object 2 (%d)\n", threadNumber );
        NS_RELEASE(proxyObject2);
    }    

    PR_Sleep( PR_MillisecondsToInterval(1000) );  // If your thread goes away, your stack goes away.  Only use ASYNC on calls that do not have out parameters
}
#endif



void TestCase_NestedLoop(nsIThread *thread, PRInt32 index)
{
    nsCOMPtr<nsIProxyObjectManager> manager =
            do_GetService(NS_XPCOMPROXY_CONTRACTID);

    LOG(("TEST: ProxyObjectManager: %p\n", (void *) manager.get()));
    
    PR_ASSERT(manager);

    nsITestProxy         *proxyObject;
    nsTestXPCFoo2*        foo   = new nsTestXPCFoo2();
    
    PR_ASSERT(foo);
    
    
    manager->GetProxyForObject(thread, NS_GET_IID(nsITestProxy), foo, NS_PROXY_SYNC, (void**)&proxyObject);
    
    if (proxyObject)
    {
        // release ownership of the real object. 
        
        nsresult rv;
        
        LOG(("TEST: Deleting real Object (%d)\n", index));
        NS_RELEASE(foo);
   
        PRInt32 retval;
        
        LOG(("TEST: Getting EventThread...\n"));

        //nsCOMPtr<nsIThread> curThread = do_GetCurrentThread();
        PRThread *curThread = PR_GetCurrentThread();
        if (curThread)
        {
            LOG(("TEST: Thread (%d) Prior to calling proxyObject->Test.\n", index));
            rv = proxyObject->Test(NS_PTR_TO_INT32((void*)curThread), 0, &retval);   // XXX broken on 64-bit arch
            LOG(("TEST: Thread (%d) proxyObject error: %x.\n", index, rv));

            LOG(("TEST: Deleting Proxy Object (%d)\n", index));
            NS_RELEASE(proxyObject);
        }    

        PR_Sleep( PR_MillisecondsToInterval(1000) );  // If your thread goes away, your stack goes away.  Only use ASYNC on calls that do not have out parameters
    }
}


#if 0
void TestCase_nsISupports(void *arg)
{

    ArgsStruct *argsStruct = (ArgsStruct*) arg;

    nsCOMPtr<nsIProxyObjectManager> manager =
            do_GetService(NS_XPCOMPROXY_CONTRACTID);
    
    PR_ASSERT(manager);

    nsITestProxy         *proxyObject;
    nsTestXPCFoo*         foo   = new nsTestXPCFoo();
    
    PR_ASSERT(foo);

     manager->GetProxyForObject(argsStruct->thread, NS_GET_IID(nsITestProxy), foo, NS_PROXY_SYNC, (void**)&proxyObject);
    
    if (proxyObject != nsnull)
    {   
        nsISupports *bISupports = nsnull, *cISupports = nsnull;
        
        proxyObject->Test3(foo, &bISupports);
        proxyObject->Test3(bISupports, &cISupports);
        
        nsITestProxy *test;
        bISupports->QueryInterface(NS_GET_IID(nsITestProxy), (void**)&test);
        
        test->Test2();

        NS_RELEASE(foo);
        NS_RELEASE(proxyObject);
    }
}
#endif

/***************************************************************************/
/* ProxyTest                                                               */
/***************************************************************************/

class ProxyTest : public nsIRunnable
{
public:
    NS_DECL_ISUPPORTS

    ProxyTest(PRThread *eventLoopThread, PRInt32 index)
        : mEventLoopThread(eventLoopThread)
        , mIndex(index)
    {}

    NS_IMETHOD Run()
    {
        //TestCase_TwoClassesOneInterface(arg);
        //TestCase_nsISupports(arg);
        nsCOMPtr<nsIThread> thread;
        GetThreadFromPRThread(mEventLoopThread, getter_AddRefs(thread));
        TestCase_NestedLoop(thread, mIndex);

        return NS_OK;
    }

private:
    PRThread *mEventLoopThread;
    PRInt32 mIndex;
};
NS_IMPL_THREADSAFE_ISUPPORTS1(ProxyTest, nsIRunnable)

class TestSyncProxyToSelf : public nsIRunnable
{
public:
    NS_DECL_ISUPPORTS

    NS_IMETHOD Run()
    {
        LOG(("TEST: Verifing calling Proxy on eventQ thread.\n"));

        nsCOMPtr<nsIThread> thread = do_GetCurrentThread();

        nsITestProxy *proxyObject;
        nsTestXPCFoo *foo = new nsTestXPCFoo();
        NS_ENSURE_STATE(foo);

        nsCOMPtr<nsIProxyObjectManager> manager =
                do_GetService(NS_XPCOMPROXY_CONTRACTID);

        manager->GetProxyForObject(thread,
                                   NS_GET_IID(nsITestProxy), foo,
                                   NS_PROXY_SYNC, (void**)&proxyObject);

        PRInt32 a;
        proxyObject->Test(1, 2, &a);
        proxyObject->Test2();
        
        NS_RELEASE(proxyObject);
        delete foo;

        LOG(("TEST: End of Verification calling Proxy on eventQ thread.\n"));

        return NS_OK;
    }
};
NS_IMPL_THREADSAFE_ISUPPORTS1(TestSyncProxyToSelf, nsIRunnable)

//---------------------------------------------------------------------------
// Test to make sure we can call methods on a "main thread only" object from
// a background thread.

class MainThreadOnly : public nsIRunnable {
public:
    NS_DECL_ISUPPORTS
    NS_IMETHOD Run() {
        NS_ASSERTION(NS_IsMainThread(), "method called on wrong thread");
        *mNumRuns -= 1;
        return NS_OK;
    }
    MainThreadOnly(PRUint32 *numRuns) : mNumRuns(numRuns) {}
    ~MainThreadOnly() {
        NS_ASSERTION(NS_IsMainThread(), "method called on wrong thread");
    }
    bool IsDone() { return mNumRuns == 0; }
private:
    PRUint32 *mNumRuns;
};
NS_IMPL_ISUPPORTS1(MainThreadOnly, nsIRunnable)  // not threadsafe!

static nsresult
RunApartmentTest()
{
    LOG(("RunApartmentTest: start\n"));

    const PRUint32 numDispatched = 160;

    PRUint32 numCompleted = 0;
    nsCOMPtr<nsIRunnable> obj = new MainThreadOnly(&numCompleted);

    nsCOMPtr<nsIProxyObjectManager> manager =
            do_GetService(NS_XPCOMPROXY_CONTRACTID);

    nsCOMPtr<nsIRunnable> objProxy;
    manager->GetProxyForObject(NS_PROXY_TO_CURRENT_THREAD,
                               NS_GET_IID(nsIRunnable),
                               obj,
                               NS_PROXY_ASYNC,
                               getter_AddRefs(objProxy));
    nsCOMPtr<nsIThread> thread;
    NS_NewThread(getter_AddRefs(thread));

    obj = nsnull;

    nsCOMPtr<nsIThreadPool> pool = do_CreateInstance(NS_THREADPOOL_CONTRACTID);

    pool->SetThreadLimit(8);
    for (PRUint32 i = 0; i < numDispatched; ++i)
        pool->Dispatch(objProxy, NS_DISPATCH_NORMAL);

    objProxy = nsnull;

    nsCOMPtr<nsIThread> curThread = do_GetCurrentThread();
    while (numCompleted < numDispatched) {
        NS_ProcessNextEvent(curThread);
    }

    pool->Shutdown();

    LOG(("RunApartmentTest: end\n"));
    return NS_OK;
}

} // namespace

using namespace proxytests;

int
main(int argc, char **argv)
{
    int numberOfThreads = 1;

    if (argc > 1)
        numberOfThreads = atoi(argv[1]);

    NS_InitXPCOM2(nsnull, nsnull, nsnull);

    // Scope code so everything is destroyed before we run call NS_ShutdownXPCOM
    {
        RunApartmentTest();

        nsCOMPtr<nsIThread> eventLoopThread;
        NS_NewThread(getter_AddRefs(eventLoopThread));

        nsCOMPtr<nsIRunnable> test = new TestSyncProxyToSelf();
        eventLoopThread->Dispatch(test, NS_DISPATCH_NORMAL);

        PRThread *eventLoopPRThread;
        eventLoopThread->GetPRThread(&eventLoopPRThread);
        PR_ASSERT(eventLoopPRThread);
        
        LOG(("TEST: Spawn Threads:\n"));
        nsCOMArray<nsIThread> threads;
        for (PRInt32 spawn = 0; spawn < numberOfThreads; spawn++)
        {
            test = new ProxyTest(eventLoopPRThread, spawn);

            nsCOMPtr<nsIThread> thread;
            NS_NewThread(getter_AddRefs(thread), test);

            threads.AppendObject(thread);

            LOG(("TEST: \tThread (%d) spawned\n", spawn));

            PR_Sleep( PR_MillisecondsToInterval(250) );
        }

        LOG(("TEST: All Threads Spawned.\n"));
        
        LOG(("TEST: Wait for threads.\n"));
        for (PRInt32 i = 0; i < numberOfThreads; i++)
        {
            LOG(("TEST: Thread (%d) Join...\n", i));
            nsresult rv = threads[i]->Shutdown();
            LOG(("TEST: Thread (%d) Joined. (error: %x).\n", i, rv));
        }

        LOG(("TEST: Shutting down event loop thread\n"));
        eventLoopThread->Shutdown();
    }

    LOG(("TEST: Calling Cleanup.\n"));
    NS_ShutdownXPCOM(nsnull);

    LOG(("TEST: Return zero.\n"));
    return 0;
}
