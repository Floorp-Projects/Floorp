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

#include "testproxyclasses.h"


#include "prlog.h"
#ifdef PR_LOGGING
PRLogModuleInfo *sLog = PR_NewLogModule("Test");
#define LOG(args) PR_LOG(sLog, PR_LOG_DEBUG, args)
#else
#define LOG(args) printf args
#endif




static nsresult
GetThreadFromPRThread(PRThread *prthread, nsIThread **result)
{
  LOG(("TEST: GetThreadFromPRThread [%p]\n", prthread));

  nsCOMPtr<nsIThreadManager> tm = do_GetService(NS_THREADMANAGER_CONTRACTID);
  NS_ENSURE_STATE(tm);
  return tm->GetThreadFromPRThread(prthread, result);
}



// I really do not like the forced SIMPLE_PROGRAMS Makefile paradigm
#include "testproxyclasses.cpp"



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


void TestCase_1_Failure()
{
        nsCOMPtr<nsIProxyObjectManager> manager =
                do_GetService(NS_XPCOMPROXY_CONTRACTID);

        LOG(("TestCase_1_Failure: ProxyObjectManager: %p\n", (void *) manager.get()));

        PR_ASSERT(manager);

        nsITestProxy         *proxyObject;
        nsTestXPCFoo*         foo   = new nsTestXPCFoo();

        PR_ASSERT(foo);

        LOG(("TestCase_1_Failure: xsTestXPCFoo Object Created [0x%08X] => \n", foo));

        PRInt32 a1;
        nsresult rv, r;

        a1=10;
        LOG(("TestCase_1_Failure: DIRECT 1_1: Entry: [%d] => \n", a1));
        printf("TestCase_1_Failure: DIRECT 1_1: Entry: [%d] => \n", a1);
        rv = foo->Test1_1(&a1, &r);
        LOG(("TestCase_1_Failure: DIRECT 1_1: Return:                                         => [%d] --> Returns %d, rv=%d\n", a1, r, rv));
        printf("TestCase_1_Failure: DIRECT 1_1: Return: => [%d] --> Returns %d, rv=%d\n", a1, r, rv);


        nsCOMPtr<nsIThread> eventLoopThread;
        NS_NewThread(getter_AddRefs(eventLoopThread));

        PRThread *eventLoopPRThread;
        eventLoopThread->GetPRThread(&eventLoopPRThread);
        PR_ASSERT(eventLoopPRThread);

        nsCOMPtr<nsIThread> thread;
        GetThreadFromPRThread(eventLoopPRThread, getter_AddRefs(thread));

        manager->GetProxyForObject(thread, NS_GET_IID(nsITestProxy), foo, NS_PROXY_SYNC, (void**)&proxyObject);

        LOG(("TestCase_1_Failure: Deleting real nsTestXPCFoo Object [0x%08X]\n", foo));
        NS_RELEASE(foo);

        if (proxyObject)
        {
                a1=10;
                LOG(("TestCase_1_Failure: PROXY 1_1: Entry: [%d] => \n", a1));
                printf("TestCase_1_Failure: PROXY 1_1: Entry: [%d] => \n", a1);
                rv = proxyObject->Test1_1(&a1, &r);
                LOG(("TestCase_1_Failure: PROXY 1_1: Return:                                         => [%d] --> Returns %d, rv=%d\n", a1, r, rv));
                printf("TestCase_1_Failure: PROXY 1_1: Return: => [%d] --> Returns %d, rv=%d\n", a1, r, rv);

                LOG(("TestCase_1_Failure: Deleting Proxy Object [0x%08X]\n", proxyObject));
                NS_RELEASE(proxyObject);
        }

        PR_Sleep( PR_MillisecondsToInterval(1000) );  // If your thread goes away, your stack goes away.  Only use ASYNC on calls that do not have out parameters
}


void TestCase_1_DirectTests(nsTestXPCFoo *foo)
{
        PRInt32 a1, b1, c1, d1, e1, f1, g1, h1, i1, j1;
        nsresult rv, r;
        a1=10; b1=20; c1=30; d1=40; e1=50; f1=60; g1=70; h1=80; i1=90; j1=100;
        LOG(("NestedLoop: DIRECT TEST 10_1: Entry: [%d, %d, %d, %d, %d, %d, %d, %d, %d, %d] => \n", a1, b1, c1, d1, e1, f1, g1, h1, i1, j1));
        printf("NestedLoop: DIRECT TEST 10_1: Entry: [%d, %d, %d, %d, %d, %d, %d, %d, %d, %d] => \n", a1, b1, c1, d1, e1, f1, g1, h1, i1, j1);
        rv = foo->Test10_1(&a1, &b1, &c1, &d1, &e1, &f1, &g1, &h1, &i1, &j1, &r);
        LOG(("NestedLoop: DIRECT TEST 10_1: Return:                                         => [%d, %d, %d, %d, %d, %d, %d, %d, %d, %d] --> Returns %d, rv=%d\n", a1, b1, c1, d1, e1, f1, g1, h1, i1, j1, r, rv));
        printf("NestedLoop: DIRECT TEST 10_1: Return: => [%d, %d, %d, %d, %d, %d, %d, %d, %d, %d] --> Returns %d, rv=%d\n", a1, b1, c1, d1, e1, f1, g1, h1, i1, j1, r, rv);

        a1=10; b1=20; c1=30; d1=40; e1=50; f1=60; g1=70; h1=80; i1=90;
        LOG(("NestedLoop: DIRECT TEST 9_1: Entry: [%d, %d, %d, %d, %d, %d, %d, %d, %d] => \n", a1, b1, c1, d1, e1, f1, g1, h1, i1));
        printf("NestedLoop: DIRECT TEST 9_1: Entry: [%d, %d, %d, %d, %d, %d, %d, %d, %d] => \n", a1, b1, c1, d1, e1, f1, g1, h1, i1);
        rv = foo->Test9_1(&a1, &b1, &c1, &d1, &e1, &f1, &g1, &h1, &i1, &r);
        LOG(("NestedLoop: DIRECT TEST 9_1: Return:                                         => [%d, %d, %d, %d, %d, %d, %d, %d, %d] --> Returns %d, rv=%d\n", a1, b1, c1, d1, e1, f1, g1, h1, i1, r, rv));
        printf("NestedLoop: DIRECT TEST 9_1: Return: => [%d, %d, %d, %d, %d, %d, %d, %d, %d] --> Returns %d, rv=%d\n", a1, b1, c1, d1, e1, f1, g1, h1, i1, r, rv);

        a1=10; b1=20; c1=30; d1=40; e1=50; f1=60; g1=70; h1=80;
        LOG(("NestedLoop: DIRECT TEST 8_1: Entry: [%d, %d, %d, %d, %d, %d, %d, %d] => \n", a1, b1, c1, d1, e1, f1, g1, h1));
        printf("NestedLoop: DIRECT TEST 8_1: Entry: [%d, %d, %d, %d, %d, %d, %d, %d] => \n", a1, b1, c1, d1, e1, f1, g1, h1);
        rv = foo->Test8_1(&a1, &b1, &c1, &d1, &e1, &f1, &g1, &h1, &r);
        LOG(("NestedLoop: DIRECT TEST 8_1: Return:                                         => [%d, %d, %d, %d, %d, %d, %d, %d] --> Returns %d, rv=%d\n", a1, b1, c1, d1, e1, f1, g1, h1, r, rv));
        printf("NestedLoop: DIRECT TEST 8_1: Return: => [%d, %d, %d, %d, %d, %d, %d, %d] --> Returns %d, rv=%d\n", a1, b1, c1, d1, e1, f1, g1, h1, r, rv);

        a1=10; b1=20; c1=30; d1=40; e1=50; f1=60; g1=70;
        LOG(("NestedLoop: DIRECT TEST 7_1: Entry: [%d, %d, %d, %d, %d, %d, %d] => \n", a1, b1, c1, d1, e1, f1, g1));
        printf("NestedLoop: DIRECT TEST 7_1: Entry: [%d, %d, %d, %d, %d, %d, %d] => \n", a1, b1, c1, d1, e1, f1, g1);
        rv = foo->Test7_1(&a1, &b1, &c1, &d1, &e1, &f1, &g1, &r);
        LOG(("NestedLoop: DIRECT TEST 7_1: Return:                                         => [%d, %d, %d, %d, %d, %d, %d] --> Returns %d, rv=%d\n", a1, b1, c1, d1, e1, f1, g1, r, rv));
        printf("NestedLoop: DIRECT TEST 7_1: Return: => [%d, %d, %d, %d, %d, %d, %d] --> Returns %d, rv=%d\n", a1, b1, c1, d1, e1, f1, g1, r, rv);

        a1=10; b1=20; c1=30; d1=40; e1=50; f1=60;
        LOG(("NestedLoop: DIRECT TEST 6_1: Entry: [%d, %d, %d, %d, %d, %d] => \n", a1, b1, c1, d1, e1, f1));
        printf("NestedLoop: DIRECT TEST 6_1: Entry: [%d, %d, %d, %d, %d, %d] => \n", a1, b1, c1, d1, e1, f1);
        rv = foo->Test6_1(&a1, &b1, &c1, &d1, &e1, &f1, &r);
        LOG(("NestedLoop: DIRECT TEST 6_1: Return:                                         => [%d, %d, %d, %d, %d, %d] --> Returns %d, rv=%d\n", a1, b1, c1, d1, e1, f1, r, rv));
        printf("NestedLoop: DIRECT TEST 6_1: Return: => [%d, %d, %d, %d, %d, %d] --> Returns %d, rv=%d\n", a1, b1, c1, d1, e1, f1, r, rv);

        a1=10; b1=20; c1=30; d1=40; e1=50;
        LOG(("NestedLoop: DIRECT TEST 5_1: Entry: [%d, %d, %d, %d, %d] => \n", a1, b1, c1, d1, e1));
        printf("NestedLoop: DIRECT TEST 5_1: Entry: [%d, %d, %d, %d, %d] => \n", a1, b1, c1, d1, e1);
        rv = foo->Test5_1(&a1, &b1, &c1, &d1, &e1, &r);
        LOG(("NestedLoop: DIRECT TEST 5_1: Return:                                         => [%d, %d, %d, %d, %d] --> Returns %d, rv=%d\n", a1, b1, c1, d1, e1, r, rv));
        printf("NestedLoop: DIRECT TEST 5_1: Return: => [%d, %d, %d, %d, %d] --> Returns %d, rv=%d\n", a1, b1, c1, d1, e1, r, rv);

        a1=10; b1=20; c1=30; d1=40;
        LOG(("NestedLoop: DIRECT TEST 4_1: Entry: [%d, %d, %d, %d] => \n", a1, b1, c1, d1));
        printf("NestedLoop: DIRECT TEST 4_1: Entry: [%d, %d, %d, %d] => \n", a1, b1, c1, d1);
        rv = foo->Test4_1(&a1, &b1, &c1, &d1, &r);
        LOG(("NestedLoop: DIRECT TEST 4_1: Return:                                         => [%d, %d, %d, %d] --> Returns %d, rv=%d\n", a1, b1, c1, d1, r, rv));
        printf("NestedLoop: DIRECT TEST 4_1: Return: => [%d, %d, %d, %d] --> Returns %d, rv=%d\n", a1, b1, c1, d1, r, rv);

        a1=10; b1=20; c1=30;
        LOG(("NestedLoop: DIRECT TEST 3_1: Entry: [%d, %d, %d] => \n", a1, b1, c1));
        printf("NestedLoop: DIRECT TEST 3_1: Entry: [%d, %d, %d] => \n", a1, b1, c1);
        rv = foo->Test3_1(&a1, &b1, &c1, &r);
        LOG(("NestedLoop: DIRECT TEST 3_1: Return:                                         => [%d, %d, %d] --> Returns %d, rv=%d\n", a1, b1, c1, r, rv));
        printf("NestedLoop: DIRECT TEST 3_1: Return: => [%d, %d, %d] --> Returns %d, rv=%d\n", a1, b1, c1, r, rv);

        a1=10; b1=20;
        LOG(("NestedLoop: DIRECT TEST 2_1: Entry: [%d, %d] => \n", a1, b1));
        printf("NestedLoop: DIRECT TEST 2_1: Entry: [%d, %d] => \n", a1, b1);
        rv = foo->Test2_1(&a1, &b1, &r);
        LOG(("NestedLoop: DIRECT TEST 2_1: Return:                                         => [%d, %d] --> Returns %d, rv=%d\n", a1, b1, r, rv));
        printf("NestedLoop: DIRECT TEST 2_1: Return: => [%d, %d] --> Returns %d, rv=%d\n", a1, b1, r, rv);

        a1=10;
        LOG(("NestedLoop: DIRECT TEST 1_1: Entry: [%d] => \n", a1));
        printf("NestedLoop: DIRECT TEST 1_1: Entry: [%d] => \n", a1);
        rv = foo->Test1_1(&a1, &r);
        LOG(("NestedLoop: DIRECT TEST 1_1: Return:                                         => [%d] --> Returns %d, rv=%d\n", a1, r, rv));
        printf("NestedLoop: DIRECT TEST 1_1: Return: => [%d] --> Returns %d, rv=%d\n", a1, r, rv);
}


void TestCase_1_ProxyTests(nsITestProxy *proxyObject)
{
        PRInt32 a1, b1, c1, d1, e1, f1, g1, h1, i1, j1;
        nsresult rv, r;
        a1=10; b1=20; c1=30; d1=40; e1=50; f1=60; g1=70; h1=80; i1=90; j1=100;
        LOG(("NestedLoop: TEST 10_1: Entry: [%d, %d, %d, %d, %d, %d, %d, %d, %d, %d] => \n", a1, b1, c1, d1, e1, f1, g1, h1, i1, j1));
        printf("NestedLoop: TEST 10_1: Entry: [%d, %d, %d, %d, %d, %d, %d, %d, %d, %d] => \n", a1, b1, c1, d1, e1, f1, g1, h1, i1, j1);
        rv = proxyObject->Test10_1(&a1, &b1, &c1, &d1, &e1, &f1, &g1, &h1, &i1, &j1, &r);
        LOG(("NestedLoop: TEST 10_1: Return:                                         => [%d, %d, %d, %d, %d, %d, %d, %d, %d, %d] --> Returns %d, rv=%d\n", a1, b1, c1, d1, e1, f1, g1, h1, i1, j1, r, rv));
        printf("NestedLoop: TEST 10_1: Return: => [%d, %d, %d, %d, %d, %d, %d, %d, %d, %d] --> Returns %d, rv=%d\n", a1, b1, c1, d1, e1, f1, g1, h1, i1, j1, r, rv);

        a1=10; b1=20; c1=30; d1=40; e1=50; f1=60; g1=70; h1=80; i1=90;
        LOG(("NestedLoop: TEST 9_1: Entry: [%d, %d, %d, %d, %d, %d, %d, %d, %d] => \n", a1, b1, c1, d1, e1, f1, g1, h1, i1));
        printf("NestedLoop: TEST 9_1: Entry: [%d, %d, %d, %d, %d, %d, %d, %d, %d] => \n", a1, b1, c1, d1, e1, f1, g1, h1, i1);
        rv = proxyObject->Test9_1(&a1, &b1, &c1, &d1, &e1, &f1, &g1, &h1, &i1, &r);
        LOG(("NestedLoop: TEST 9_1: Return:                                         => [%d, %d, %d, %d, %d, %d, %d, %d, %d] --> Returns %d, rv=%d\n", a1, b1, c1, d1, e1, f1, g1, h1, i1, r, rv));
        printf("NestedLoop: TEST 9_1: Return: => [%d, %d, %d, %d, %d, %d, %d, %d, %d] --> Returns %d, rv=%d\n", a1, b1, c1, d1, e1, f1, g1, h1, i1, r, rv);

        a1=10; b1=20; c1=30; d1=40; e1=50; f1=60; g1=70; h1=80;
        LOG(("NestedLoop: TEST 8_1: Entry: [%d, %d, %d, %d, %d, %d, %d, %d] => \n", a1, b1, c1, d1, e1, f1, g1, h1));
        printf("NestedLoop: TEST 8_1: Entry: [%d, %d, %d, %d, %d, %d, %d, %d] => \n", a1, b1, c1, d1, e1, f1, g1, h1);
        rv = proxyObject->Test8_1(&a1, &b1, &c1, &d1, &e1, &f1, &g1, &h1, &r);
        LOG(("NestedLoop: TEST 8_1: Return:                                         => [%d, %d, %d, %d, %d, %d, %d, %d] --> Returns %d, rv=%d\n", a1, b1, c1, d1, e1, f1, g1, h1, r, rv));
        printf("NestedLoop: TEST 8_1: Return: => [%d, %d, %d, %d, %d, %d, %d, %d] --> Returns %d, rv=%d\n", a1, b1, c1, d1, e1, f1, g1, h1, r, rv);

        a1=10; b1=20; c1=30; d1=40; e1=50; f1=60; g1=70;
        LOG(("NestedLoop: TEST 7_1: Entry: [%d, %d, %d, %d, %d, %d, %d] => \n", a1, b1, c1, d1, e1, f1, g1));
        printf("NestedLoop: TEST 7_1: Entry: [%d, %d, %d, %d, %d, %d, %d] => \n", a1, b1, c1, d1, e1, f1, g1);
        rv = proxyObject->Test7_1(&a1, &b1, &c1, &d1, &e1, &f1, &g1, &r);
        LOG(("NestedLoop: TEST 7_1: Return:                                         => [%d, %d, %d, %d, %d, %d, %d] --> Returns %d, rv=%d\n", a1, b1, c1, d1, e1, f1, g1, r, rv));
        printf("NestedLoop: TEST 7_1: Return: => [%d, %d, %d, %d, %d, %d, %d] --> Returns %d, rv=%d\n", a1, b1, c1, d1, e1, f1, g1, r, rv);

        a1=10; b1=20; c1=30; d1=40; e1=50; f1=60;
        LOG(("NestedLoop: TEST 6_1: Entry: [%d, %d, %d, %d, %d, %d] => \n", a1, b1, c1, d1, e1, f1));
        printf("NestedLoop: TEST 6_1: Entry: [%d, %d, %d, %d, %d, %d] => \n", a1, b1, c1, d1, e1, f1);
        rv = proxyObject->Test6_1(&a1, &b1, &c1, &d1, &e1, &f1, &r);
        LOG(("NestedLoop: TEST 6_1: Return:                                         => [%d, %d, %d, %d, %d, %d] --> Returns %d, rv=%d\n", a1, b1, c1, d1, e1, f1, r, rv));
        printf("NestedLoop: TEST 6_1: Return: => [%d, %d, %d, %d, %d, %d] --> Returns %d, rv=%d\n", a1, b1, c1, d1, e1, f1, r, rv);

        a1=10; b1=20; c1=30; d1=40; e1=50;
        LOG(("NestedLoop: TEST 5_1: Entry: [%d, %d, %d, %d, %d] => \n", a1, b1, c1, d1, e1));
        printf("NestedLoop: TEST 5_1: Entry: [%d, %d, %d, %d, %d] => \n", a1, b1, c1, d1, e1);
        rv = proxyObject->Test5_1(&a1, &b1, &c1, &d1, &e1, &r);
        LOG(("NestedLoop: TEST 5_1: Return:                                         => [%d, %d, %d, %d, %d] --> Returns %d, rv=%d\n", a1, b1, c1, d1, e1, r, rv));
        printf("NestedLoop: TEST 5_1: Return: => [%d, %d, %d, %d, %d] --> Returns %d, rv=%d\n", a1, b1, c1, d1, e1, r, rv);

        a1=10; b1=20; c1=30; d1=40;
        LOG(("NestedLoop: TEST 4_1: Entry: [%d, %d, %d, %d] => \n", a1, b1, c1, d1));
        printf("NestedLoop: TEST 4_1: Entry: [%d, %d, %d, %d] => \n", a1, b1, c1, d1);
        rv = proxyObject->Test4_1(&a1, &b1, &c1, &d1, &r);
        LOG(("NestedLoop: TEST 4_1: Return:                                         => [%d, %d, %d, %d] --> Returns %d, rv=%d\n", a1, b1, c1, d1, r, rv));
        printf("NestedLoop: TEST 4_1: Return: => [%d, %d, %d, %d] --> Returns %d, rv=%d\n", a1, b1, c1, d1, r, rv);

        a1=10; b1=20; c1=30;
        LOG(("NestedLoop: TEST 3_1: Entry: [%d, %d, %d] => \n", a1, b1, c1));
        printf("NestedLoop: TEST 3_1: Entry: [%d, %d, %d] => \n", a1, b1, c1);
        rv = proxyObject->Test3_1(&a1, &b1, &c1, &r);
        LOG(("NestedLoop: TEST 3_1: Return:                                         => [%d, %d, %d] --> Returns %d, rv=%d\n", a1, b1, c1, r, rv));
        printf("NestedLoop: TEST 3_1: Return: => [%d, %d, %d] --> Returns %d, rv=%d\n", a1, b1, c1, r, rv);

        a1=10; b1=20;
        LOG(("NestedLoop: TEST 2_1: Entry: [%d, %d] => \n", a1, b1));
        printf("NestedLoop: TEST 2_1: Entry: [%d, %d] => \n", a1, b1);
        rv = proxyObject->Test2_1(&a1, &b1, &r);
        LOG(("NestedLoop: TEST 2_1: Return:                                         => [%d, %d] --> Returns %d, rv=%d\n", a1, b1, r, rv));
        printf("NestedLoop: TEST 2_1: Return: => [%d, %d] --> Returns %d, rv=%d\n", a1, b1, r, rv);

        a1=10;
        LOG(("NestedLoop: TEST 1_1: Entry: [%d] => \n", a1));
        printf("NestedLoop: TEST 1_1: Entry: [%d] => \n", a1);
        rv = proxyObject->Test1_1(&a1, &r);
        LOG(("NestedLoop: TEST 1_1: Return:                                         => [%d] --> Returns %d, rv=%d\n", a1, r, rv));
        printf("NestedLoop: TEST 1_1: Return: => [%d] --> Returns %d, rv=%d\n", a1, r, rv);
}


void TestCase_2_ProxyTests(nsITestProxy *proxyObject)
{
        PRInt64 a1, b1, c1, d1, e1, f1, g1, h1, i1, j1;
        nsresult rv, r;
        a1=100; b1=200; c1=300; d1=400; e1=500; f1=600; g1=700; h1=800; i1=900; j1=1000;
        LOG(("NestedLoop: TEST 10_2: Entry: [%g, %g, %g, %g, %g, %g, %g, %g, %g, %g] => \n", a1, b1, c1, d1, e1, f1, g1, h1, i1, j1));
        printf("NestedLoop: TEST 10_2: Entry: [%g, %g, %g, %g, %g, %g, %g, %g, %g, %g] => \n", a1, b1, c1, d1, e1, f1, g1, h1, i1, j1);
        rv = proxyObject->Test10_2(&a1, &b1, &c1, &d1, &e1, &f1, &g1, &h1, &i1, &j1, &r);
        LOG(("NestedLoop: TEST 10_2: Return:                                         => [%g, %g, %g, %g, %g, %g, %g, %g, %g, %g] --> Returns %d, rv=%d\n", a1, b1, c1, d1, e1, f1, g1, h1, i1, j1, r, rv));
        printf("NestedLoop: TEST 10_2: Return: => [%g, %g, %g, %g, %g, %g, %g, %g, %g, %g] --> Returns %d, rv=%d\n", a1, b1, c1, d1, e1, f1, g1, h1, i1, j1, r, rv);

        a1=100; b1=200; c1=300; d1=400; e1=500; f1=600; g1=700; h1=800; i1=900;
        LOG(("NestedLoop: TEST 9_2: Entry: [%g, %g, %g, %g, %g, %g, %g, %g, %g] => \n", a1, b1, c1, d1, e1, f1, g1, h1, i1));
        printf("NestedLoop: TEST 9_2: Entry: [%g, %g, %g, %g, %g, %g, %g, %g, %g] => \n", a1, b1, c1, d1, e1, f1, g1, h1, i1);
        rv = proxyObject->Test9_2(&a1, &b1, &c1, &d1, &e1, &f1, &g1, &h1, &i1, &r);
        LOG(("NestedLoop: TEST 9_2: Return:                                         => [%g, %g, %g, %g, %g, %g, %g, %g, %g] --> Returns %d, rv=%d\n", a1, b1, c1, d1, e1, f1, g1, h1, i1, r, rv));
        printf("NestedLoop: TEST 9_2: Return: => [%g, %g, %g, %g, %g, %g, %g, %g, %g] --> Returns %d, rv=%d\n", a1, b1, c1, d1, e1, f1, g1, h1, i1, r, rv);

        a1=100; b1=200; c1=300; d1=400; e1=500; f1=600; g1=700; h1=800;
        LOG(("NestedLoop: TEST 8_2: Entry: [%g, %g, %g, %g, %g, %g, %g, %g] => \n", a1, b1, c1, d1, e1, f1, g1, h1));
        printf("NestedLoop: TEST 8_2: Entry: [%g, %g, %g, %g, %g, %g, %g, %g] => \n", a1, b1, c1, d1, e1, f1, g1, h1);
        rv = proxyObject->Test8_2(&a1, &b1, &c1, &d1, &e1, &f1, &g1, &h1, &r);
        LOG(("NestedLoop: TEST 8_2: Return:                                         => [%g, %g, %g, %g, %g, %g, %g, %g] --> Returns %d, rv=%d\n", a1, b1, c1, d1, e1, f1, g1, h1, r, rv));
        printf("NestedLoop: TEST 8_2: Return: => [%g, %g, %g, %g, %g, %g, %g, %g] --> Returns %d, rv=%d\n", a1, b1, c1, d1, e1, f1, g1, h1, r, rv);

        a1=100; b1=200; c1=300; d1=400; e1=500; f1=600; g1=700;
        LOG(("NestedLoop: TEST 7_2: Entry: [%g, %g, %g, %g, %g, %g, %g] => \n", a1, b1, c1, d1, e1, f1, g1));
        printf("NestedLoop: TEST 7_2: Entry: [%g, %g, %g, %g, %g, %g, %g] => \n", a1, b1, c1, d1, e1, f1, g1);
        rv = proxyObject->Test7_2(&a1, &b1, &c1, &d1, &e1, &f1, &g1, &r);
        LOG(("NestedLoop: TEST 7_2: Return:                                         => [%g, %g, %g, %g, %g, %g, %g] --> Returns %d, rv=%d\n", a1, b1, c1, d1, e1, f1, g1, r, rv));
        printf("NestedLoop: TEST 7_2: Return: => [%g, %g, %g, %g, %g, %g, %g] --> Returns %d, rv=%d\n", a1, b1, c1, d1, e1, f1, g1, r, rv);

        a1=100; b1=200; c1=300; d1=400; e1=500; f1=600;
        LOG(("NestedLoop: TEST 6_2: Entry: [%g, %g, %g, %g, %g, %g] => \n", a1, b1, c1, d1, e1, f1));
        printf("NestedLoop: TEST 6_2: Entry: [%g, %g, %g, %g, %g, %g] => \n", a1, b1, c1, d1, e1, f1);
        rv = proxyObject->Test6_2(&a1, &b1, &c1, &d1, &e1, &f1, &r);
        LOG(("NestedLoop: TEST 6_2: Return:                                         => [%g, %g, %g, %g, %g, %g] --> Returns %d, rv=%d\n", a1, b1, c1, d1, e1, f1, r, rv));
        printf("NestedLoop: TEST 6_2: Return: => [%g, %g, %g, %g, %g, %g] --> Returns %d, rv=%d\n", a1, b1, c1, d1, e1, f1, r, rv);

        a1=100; b1=200; c1=300; d1=400; e1=500;
        LOG(("NestedLoop: TEST 5_2: Entry: [%g, %g, %g, %g, %g] => \n", a1, b1, c1, d1, e1));
        printf("NestedLoop: TEST 5_2: Entry: [%g, %g, %g, %g, %g] => \n", a1, b1, c1, d1, e1);
        rv = proxyObject->Test5_2(&a1, &b1, &c1, &d1, &e1, &r);
        LOG(("NestedLoop: TEST 5_2: Return:                                         => [%g, %g, %g, %g, %g] --> Returns %d, rv=%d\n", a1, b1, c1, d1, e1, r, rv));
        printf("NestedLoop: TEST 5_2: Return: => [%g, %g, %g, %g, %g] --> Returns %d, rv=%d\n", a1, b1, c1, d1, e1, r, rv);

        a1=100; b1=200; c1=300; d1=400;
        LOG(("NestedLoop: TEST 4_2: Entry: [%g, %g, %g, %g] => \n", a1, b1, c1, d1));
        printf("NestedLoop: TEST 4_2: Entry: [%g, %g, %g, %g] => \n", a1, b1, c1, d1);
        rv = proxyObject->Test4_2(&a1, &b1, &c1, &d1, &r);
        LOG(("NestedLoop: TEST 4_2: Return:                                         => [%g, %g, %g, %g] --> Returns %d, rv=%d\n", a1, b1, c1, d1, r, rv));
        printf("NestedLoop: TEST 4_2: Return: => [%g, %g, %g, %g] --> Returns %d, rv=%d\n", a1, b1, c1, d1, r, rv);

        a1=100; b1=200; c1=300;
        LOG(("NestedLoop: TEST 3_2: Entry: [%g, %g, %g] => \n", a1, b1, c1));
        printf("NestedLoop: TEST 3_2: Entry: [%g, %g, %g] => \n", a1, b1, c1);
        rv = proxyObject->Test3_2(&a1, &b1, &c1, &r);
        LOG(("NestedLoop: TEST 3_2: Return:                                         => [%g, %g, %g] --> Returns %d, rv=%d\n", a1, b1, c1, r, rv));
        printf("NestedLoop: TEST 3_2: Return: => [%g, %g, %g] --> Returns %d, rv=%d\n", a1, b1, c1, r, rv);

        a1=100; b1=200;
        LOG(("NestedLoop: TEST 2_2: Entry: [%g, %g] => \n", a1, b1));
        printf("NestedLoop: TEST 2_2: Entry: [%g, %g] => \n", a1, b1);
        rv = proxyObject->Test2_2(&a1, &b1, &r);
        LOG(("NestedLoop: TEST 2_2: Return:                                         => [%g, %g] --> Returns %d, rv=%d\n", a1, b1, r, rv));
        printf("NestedLoop: TEST 2_2: Return: => [%g, %g] --> Returns %d, rv=%d\n", a1, b1, r, rv);

        a1=100;
        LOG(("NestedLoop: TEST 1_2: Entry: [%g] => \n", a1));
        printf("NestedLoop: TEST 1_2: Entry: [%g] => \n", a1);
        rv = proxyObject->Test1_2(&a1, &r);
        LOG(("NestedLoop: TEST 1_2: Return:                                         => [%g] --> Returns %d, rv=%d\n", a1, r, rv));
        printf("NestedLoop: TEST 1_2: Return: => [%g] --> Returns %d, rv=%d\n", a1, r, rv);
}


void TestCase_3_ProxyTests(nsITestProxy *proxyObject)
{
        float a1, b1, c1, d1, e1, f1, g1, h1, i1, j1;
        nsresult rv, r;
        a1=10.0; b1=20.0; c1=30.0; d1=40.0; e1=50.0; f1=60.0; g1=70.0; h1=80.0; i1=90.0; j1=100.0;
        LOG(("NestedLoop: TEST 10_3: Entry: [%lf, %lf, %lf, %lf, %lf, %lf, %lf, %lf, %lf, %lf] => \n", a1, b1, c1, d1, e1, f1, g1, h1, i1, j1));
        printf("NestedLoop: TEST 10_3: Entry: [%lf, %lf, %lf, %lf, %lf, %lf, %lf, %lf, %lf, %lf] => \n", a1, b1, c1, d1, e1, f1, g1, h1, i1, j1);
        rv = proxyObject->Test10_3(&a1, &b1, &c1, &d1, &e1, &f1, &g1, &h1, &i1, &j1, &r);
        LOG(("NestedLoop: TEST 10_3: Return:                                         => [%lf, %lf, %lf, %lf, %lf, %lf, %lf, %lf, %lf, %lf] --> Returns %d, rv=%d\n", a1, b1, c1, d1, e1, f1, g1, h1, i1, j1, r, rv));
        printf("NestedLoop: TEST 10_3: Return: => [%lf, %lf, %lf, %lf, %lf, %lf, %lf, %lf, %lf, %lf] --> Returns %d, rv=%d\n", a1, b1, c1, d1, e1, f1, g1, h1, i1, j1, r, rv);

        a1=10.0; b1=20.0; c1=30.0; d1=40.0; e1=50.0; f1=60.0; g1=70.0; h1=80.0; i1=90.0;
        LOG(("NestedLoop: TEST 9_3: Entry: [%lf, %lf, %lf, %lf, %lf, %lf, %lf, %lf, %lf] => \n", a1, b1, c1, d1, e1, f1, g1, h1, i1));
        printf("NestedLoop: TEST 9_3: Entry: [%lf, %lf, %lf, %lf, %lf, %lf, %lf, %lf, %lf] => \n", a1, b1, c1, d1, e1, f1, g1, h1, i1);
        rv = proxyObject->Test9_3(&a1, &b1, &c1, &d1, &e1, &f1, &g1, &h1, &i1, &r);
        LOG(("NestedLoop: TEST 9_3: Return:                                         => [%lf, %lf, %lf, %lf, %lf, %lf, %lf, %lf, %lf] --> Returns %d, rv=%d\n", a1, b1, c1, d1, e1, f1, g1, h1, i1, r, rv));
        printf("NestedLoop: TEST 9_3: Return: => [%lf, %lf, %lf, %lf, %lf, %lf, %lf, %lf, %lf] --> Returns %d, rv=%d\n", a1, b1, c1, d1, e1, f1, g1, h1, i1, r, rv);

        a1=10.0; b1=20.0; c1=30.0; d1=40.0; e1=50.0; f1=60.0; g1=70.0; h1=80.0;
        LOG(("NestedLoop: TEST 8_3: Entry: [%lf, %lf, %lf, %lf, %lf, %lf, %lf, %lf] => \n", a1, b1, c1, d1, e1, f1, g1, h1));
        printf("NestedLoop: TEST 8_3: Entry: [%lf, %lf, %lf, %lf, %lf, %lf, %lf, %lf] => \n", a1, b1, c1, d1, e1, f1, g1, h1);
        rv = proxyObject->Test8_3(&a1, &b1, &c1, &d1, &e1, &f1, &g1, &h1, &r);
        LOG(("NestedLoop: TEST 8_3: Return:                                         => [%lf, %lf, %lf, %lf, %lf, %lf, %lf, %lf] --> Returns %d, rv=%d\n", a1, b1, c1, d1, e1, f1, g1, h1, r, rv));
        printf("NestedLoop: TEST 8_3: Return: => [%lf, %lf, %lf, %lf, %lf, %lf, %lf, %lf] --> Returns %d, rv=%d\n", a1, b1, c1, d1, e1, f1, g1, h1, r, rv);

        a1=10.0; b1=20.0; c1=30.0; d1=40.0; e1=50.0; f1=60.0; g1=70.0;
        LOG(("NestedLoop: TEST 7_3: Entry: [%lf, %lf, %lf, %lf, %lf, %lf, %lf] => \n", a1, b1, c1, d1, e1, f1, g1));
        printf("NestedLoop: TEST 7_3: Entry: [%lf, %lf, %lf, %lf, %lf, %lf, %lf] => \n", a1, b1, c1, d1, e1, f1, g1);
        rv = proxyObject->Test7_3(&a1, &b1, &c1, &d1, &e1, &f1, &g1, &r);
        LOG(("NestedLoop: TEST 7_3: Return:                                         => [%lf, %lf, %lf, %lf, %lf, %lf, %lf] --> Returns %d, rv=%d\n", a1, b1, c1, d1, e1, f1, g1, r, rv));
        printf("NestedLoop: TEST 7_3: Return: => [%lf, %lf, %lf, %lf, %lf, %lf, %lf] --> Returns %d, rv=%d\n", a1, b1, c1, d1, e1, f1, g1, r, rv);

        a1=10.0; b1=20.0; c1=30.0; d1=40.0; e1=50.0; f1=60.0;
        LOG(("NestedLoop: TEST 6_3: Entry: [%lf, %lf, %lf, %lf, %lf, %lf] => \n", a1, b1, c1, d1, e1, f1));
        printf("NestedLoop: TEST 6_3: Entry: [%lf, %lf, %lf, %lf, %lf, %lf] => \n", a1, b1, c1, d1, e1, f1);
        rv = proxyObject->Test6_3(&a1, &b1, &c1, &d1, &e1, &f1, &r);
        LOG(("NestedLoop: TEST 6_3: Return:                                         => [%lf, %lf, %lf, %lf, %lf, %lf] --> Returns %d, rv=%d\n", a1, b1, c1, d1, e1, f1, r, rv));
        printf("NestedLoop: TEST 6_3: Return: => [%lf, %lf, %lf, %lf, %lf, %lf] --> Returns %d, rv=%d\n", a1, b1, c1, d1, e1, f1, r, rv);

        a1=10.0; b1=20.0; c1=30.0; d1=40.0; e1=50.0;
        LOG(("NestedLoop: TEST 5_3: Entry: [%lf, %lf, %lf, %lf, %lf] => \n", a1, b1, c1, d1, e1));
        printf("NestedLoop: TEST 5_3: Entry: [%lf, %lf, %lf, %lf, %lf] => \n", a1, b1, c1, d1, e1);
        rv = proxyObject->Test5_3(&a1, &b1, &c1, &d1, &e1, &r);
        LOG(("NestedLoop: TEST 5_3: Return:                                         => [%lf, %lf, %lf, %lf, %lf] --> Returns %d, rv=%d\n", a1, b1, c1, d1, e1, r, rv));
        printf("NestedLoop: TEST 5_3: Return: => [%lf, %lf, %lf, %lf, %lf] --> Returns %d, rv=%d\n", a1, b1, c1, d1, e1, r, rv);

        a1=10.0; b1=20.0; c1=30.0; d1=40.0;
        LOG(("NestedLoop: TEST 4_3: Entry: [%lf, %lf, %lf, %lf] => \n", a1, b1, c1, d1));
        printf("NestedLoop: TEST 4_3: Entry: [%lf, %lf, %lf, %lf] => \n", a1, b1, c1, d1);
        rv = proxyObject->Test4_3(&a1, &b1, &c1, &d1, &r);
        LOG(("NestedLoop: TEST 4_3: Return:                                         => [%lf, %lf, %lf, %lf] --> Returns %d, rv=%d\n", a1, b1, c1, d1, r, rv));
        printf("NestedLoop: TEST 4_3: Return: => [%lf, %lf, %lf, %lf] --> Returns %d, rv=%d\n", a1, b1, c1, d1, r, rv);

        a1=10.0; b1=20.0; c1=30.0;
        LOG(("NestedLoop: TEST 3_3: Entry: [%lf, %lf, %lf] => \n", a1, b1, c1));
        printf("NestedLoop: TEST 3_3: Entry: [%lf, %lf, %lf] => \n", a1, b1, c1);
        rv = proxyObject->Test3_3(&a1, &b1, &c1, &r);
        LOG(("NestedLoop: TEST 3_3: Return:                                         => [%lf, %lf, %lf] --> Returns %d, rv=%d\n", a1, b1, c1, r, rv));
        printf("NestedLoop: TEST 3_3: Return: => [%lf, %lf, %lf] --> Returns %d, rv=%d\n", a1, b1, c1, r, rv);

        a1=10.0; b1=20.0;
        LOG(("NestedLoop: TEST 2_3: Entry: [%lf, %lf] => \n", a1, b1));
        printf("NestedLoop: TEST 2_3: Entry: [%lf, %lf] => \n", a1, b1);
        rv = proxyObject->Test2_3(&a1, &b1, &r);
        LOG(("NestedLoop: TEST 2_3: Return:                                         => [%lf, %lf] --> Returns %d, rv=%d\n", a1, b1, r, rv));
        printf("NestedLoop: TEST 2_3: Return: => [%lf, %lf] --> Returns %d, rv=%d\n", a1, b1, r, rv);

        a1=10.0;
        LOG(("NestedLoop: TEST 1_3: Entry: [%lf] => \n", a1));
        printf("NestedLoop: TEST 1_3: Entry: [%lf] => \n", a1);
        rv = proxyObject->Test1_3(&a1, &r);
        LOG(("NestedLoop: TEST 1_3: Return:                                         => [%lf] --> Returns %d, rv=%d\n", a1, r, rv));
        printf("NestedLoop: TEST 1_3: Return: => [%lf] --> Returns %d, rv=%d\n", a1, r, rv);
}


void TestCase_4_ProxyTests(nsITestProxy *proxyObject)
{
        double a1, b1, c1, d1, e1, f1, g1, h1, i1, j1;
        nsresult rv, r;
        a1=100.0; b1=200.0; c1=300.0; d1=400.0; e1=500.0; f1=600.0; g1=700.0; h1=800.0; i1=900.0; j1=1000.0;
        LOG(("NestedLoop: TEST 10_4: Entry: [%le, %le, %le, %le, %le, %le, %le, %le, %le, %le] => \n", a1, b1, c1, d1, e1, f1, g1, h1, i1, j1));
        printf("NestedLoop: TEST 10_4: Entry: [%le, %le, %le, %le, %le, %le, %le, %le, %le, %le] => \n", a1, b1, c1, d1, e1, f1, g1, h1, i1, j1);
        rv = proxyObject->Test10_4(&a1, &b1, &c1, &d1, &e1, &f1, &g1, &h1, &i1, &j1, &r);
        LOG(("NestedLoop: TEST 10_4: Return:                                         => [%le, %le, %le, %le, %le, %le, %le, %le, %le, %le] --> Returns %d, rv=%d\n", a1, b1, c1, d1, e1, f1, g1, h1, i1, j1, r, rv));
        printf("NestedLoop: TEST 10_4: Return: => [%le, %le, %le, %le, %le, %le, %le, %le, %le, %le] --> Returns %d, rv=%d\n", a1, b1, c1, d1, e1, f1, g1, h1, i1, j1, r, rv);

        a1=100.0; b1=200.0; c1=300.0; d1=400.0; e1=500.0; f1=600.0; g1=700.0; h1=800.0; i1=900.0;
        LOG(("NestedLoop: TEST 9_4: Entry: [%le, %le, %le, %le, %le, %le, %le, %le, %le] => \n", a1, b1, c1, d1, e1, f1, g1, h1, i1));
        printf("NestedLoop: TEST 9_4: Entry: [%le, %le, %le, %le, %le, %le, %le, %le, %le] => \n", a1, b1, c1, d1, e1, f1, g1, h1, i1);
        rv = proxyObject->Test9_4(&a1, &b1, &c1, &d1, &e1, &f1, &g1, &h1, &i1, &r);
        LOG(("NestedLoop: TEST 9_4: Return:                                         => [%le, %le, %le, %le, %le, %le, %le, %le, %le] --> Returns %d, rv=%d\n", a1, b1, c1, d1, e1, f1, g1, h1, i1, r, rv));
        printf("NestedLoop: TEST 9_4: Return: => [%le, %le, %le, %le, %le, %le, %le, %le, %le] --> Returns %d, rv=%d\n", a1, b1, c1, d1, e1, f1, g1, h1, i1, r, rv);

        a1=100.0; b1=200.0; c1=300.0; d1=400.0; e1=500.0; f1=600.0; g1=700.0; h1=800.0;
        LOG(("NestedLoop: TEST 8_4: Entry: [%le, %le, %le, %le, %le, %le, %le, %le] => \n", a1, b1, c1, d1, e1, f1, g1, h1));
        printf("NestedLoop: TEST 8_4: Entry: [%le, %le, %le, %le, %le, %le, %le, %le] => \n", a1, b1, c1, d1, e1, f1, g1, h1);
        rv = proxyObject->Test8_4(&a1, &b1, &c1, &d1, &e1, &f1, &g1, &h1, &r);
        LOG(("NestedLoop: TEST 8_4: Return:                                         => [%le, %le, %le, %le, %le, %le, %le, %le] --> Returns %d, rv=%d\n", a1, b1, c1, d1, e1, f1, g1, h1, r, rv));
        printf("NestedLoop: TEST 8_4: Return: => [%le, %le, %le, %le, %le, %le, %le, %le] --> Returns %d, rv=%d\n", a1, b1, c1, d1, e1, f1, g1, h1, r, rv);

        a1=100.0; b1=200.0; c1=300.0; d1=400.0; e1=500.0; f1=600.0; g1=700.0;
        LOG(("NestedLoop: TEST 7_4: Entry: [%le, %le, %le, %le, %le, %le, %le] => \n", a1, b1, c1, d1, e1, f1, g1));
        printf("NestedLoop: TEST 7_4: Entry: [%le, %le, %le, %le, %le, %le, %le] => \n", a1, b1, c1, d1, e1, f1, g1);
        rv = proxyObject->Test7_4(&a1, &b1, &c1, &d1, &e1, &f1, &g1, &r);
        LOG(("NestedLoop: TEST 7_4: Return:                                         => [%le, %le, %le, %le, %le, %le, %le] --> Returns %d, rv=%d\n", a1, b1, c1, d1, e1, f1, g1, r, rv));
        printf("NestedLoop: TEST 7_4: Return: => [%le, %le, %le, %le, %le, %le, %le] --> Returns %d, rv=%d\n", a1, b1, c1, d1, e1, f1, g1, r, rv);

        a1=100.0; b1=200.0; c1=300.0; d1=400.0; e1=500.0; f1=600.0;
        LOG(("NestedLoop: TEST 6_4: Entry: [%le, %le, %le, %le, %le, %le] => \n", a1, b1, c1, d1, e1, f1));
        printf("NestedLoop: TEST 6_4: Entry: [%le, %le, %le, %le, %le, %le] => \n", a1, b1, c1, d1, e1, f1);
        rv = proxyObject->Test6_4(&a1, &b1, &c1, &d1, &e1, &f1, &r);
        LOG(("NestedLoop: TEST 6_4: Return:                                         => [%le, %le, %le, %le, %le, %le] --> Returns %d, rv=%d\n", a1, b1, c1, d1, e1, f1, r, rv));
        printf("NestedLoop: TEST 6_4: Return: => [%le, %le, %le, %le, %le, %le] --> Returns %d, rv=%d\n", a1, b1, c1, d1, e1, f1, r, rv);

        a1=100.0; b1=200.0; c1=300.0; d1=400.0; e1=500.0;
        LOG(("NestedLoop: TEST 5_4: Entry: [%le, %le, %le, %le, %le] => \n", a1, b1, c1, d1, e1));
        printf("NestedLoop: TEST 5_4: Entry: [%le, %le, %le, %le, %le] => \n", a1, b1, c1, d1, e1);
        rv = proxyObject->Test5_4(&a1, &b1, &c1, &d1, &e1, &r);
        LOG(("NestedLoop: TEST 5_4: Return:                                         => [%le, %le, %le, %le, %le] --> Returns %d, rv=%d\n", a1, b1, c1, d1, e1, r, rv));
        printf("NestedLoop: TEST 5_4: Return: => [%le, %le, %le, %le, %le] --> Returns %d, rv=%d\n", a1, b1, c1, d1, e1, r, rv);

        a1=100.0; b1=200.0; c1=300.0; d1=400.0;
        LOG(("NestedLoop: TEST 4_4: Entry: [%le, %le, %le, %le] => \n", a1, b1, c1, d1));
        printf("NestedLoop: TEST 4_4: Entry: [%le, %le, %le, %le] => \n", a1, b1, c1, d1);
        rv = proxyObject->Test4_4(&a1, &b1, &c1, &d1, &r);
        LOG(("NestedLoop: TEST 4_4: Return:                                         => [%le, %le, %le, %le] --> Returns %d, rv=%d\n", a1, b1, c1, d1, r, rv));
        printf("NestedLoop: TEST 4_4: Return: => [%le, %le, %le, %le] --> Returns %d, rv=%d\n", a1, b1, c1, d1, r, rv);

        a1=100.0; b1=200.0; c1=300.0;
        LOG(("NestedLoop: TEST 3_4: Entry: [%le, %le, %le] => \n", a1, b1, c1));
        printf("NestedLoop: TEST 3_4: Entry: [%le, %le, %le] => \n", a1, b1, c1);
        rv = proxyObject->Test3_4(&a1, &b1, &c1, &r);
        LOG(("NestedLoop: TEST 3_4: Return:                                         => [%le, %le, %le] --> Returns %d, rv=%d\n", a1, b1, c1, r, rv));
        printf("NestedLoop: TEST 3_4: Return: => [%le, %le, %le] --> Returns %d, rv=%d\n", a1, b1, c1, r, rv);

        a1=100.0; b1=200.0;
        LOG(("NestedLoop: TEST 2_4: Entry: [%le, %le] => \n", a1, b1));
        printf("NestedLoop: TEST 2_4: Entry: [%le, %le] => \n", a1, b1);
        rv = proxyObject->Test2_4(&a1, &b1, &r);
        LOG(("NestedLoop: TEST 2_4: Return:                                         => [%le, %le] --> Returns %d, rv=%d\n", a1, b1, r, rv));
        printf("NestedLoop: TEST 2_4: Return: => [%le, %le] --> Returns %d, rv=%d\n", a1, b1, r, rv);

        a1=100.0;
        LOG(("NestedLoop: TEST 1_4: Entry: [%le] => \n", a1));
        printf("NestedLoop: TEST 1_4: Entry: [%le] => \n", a1);
        rv = proxyObject->Test1_4(&a1, &r);
        LOG(("NestedLoop: TEST 1_4: Return:                                         => [%le] --> Returns %d, rv=%d\n", a1, r, rv));
        printf("NestedLoop: TEST 1_4: Return: => [%le] --> Returns %d, rv=%d\n", a1, r, rv);
}


void TestCase_5_ProxyTests(nsITestProxy *proxyObject)
{
        //nsITestProxy *p1, *p2, *p3, *p4, *p5, *p6, *p7, *p8, *p9, *p10;
        nsISupports *p1, *p2, *p3, *p4, *p5, *p6, *p7, *p8, *p9, *p10;
        p1 = p2 = p3 = p4 = p5 = p6 = p7 = p8 = p9 = p10 = NULL;
        nsresult rv, r;

        printf("\n\nNestedLoop: TestCase_5_ProxyTests: **** NOTE **** You can verify that the sub-tests of TEST 1_5 worked by looking at the \\NSPR.LOG file on your device!\n\n");

        LOG(("NestedLoop: TEST 1_5: Entry: [0x%08X] => \n",
             p1));
        printf("NestedLoop: TEST 1_5: Entry: [0x%08X] => \n",
             p1);
        rv = proxyObject->Test1_5(&p1, &r);
        LOG(("NestedLoop: TEST 1_5: Return:                                         => [0x%08X] --> Returns %d, rv=%d\n",
             p1, r, rv));
        printf("NestedLoop: TEST 1_5: Return: => [0x%08X] --> Returns %d, rv=%d\n",
             p1, r, rv);

        LOG(("NestedLoop: TEST 2_5: Entry: [0x%08X, 0x%08X] => \n",
             p1, p2));
        printf("NestedLoop: TEST 2_5: Entry: [0x%08X, 0x%08X] => \n",
             p1, p2);
        rv = proxyObject->Test2_5(&p1, &p2, &r);
        LOG(("NestedLoop: TEST 2_5: Return:                                         => [0x%08X, 0x%08X] --> Returns %d, rv=%d\n",
             p1, p2, r, rv));
        printf("NestedLoop: TEST 2_5: Return: => [0x%08X, 0x%08X] --> Returns %d, rv=%d\n",
             p1, p2, r, rv);

        LOG(("NestedLoop: TEST 3_5: Entry: [0x%08X, 0x%08X, 0x%08X] => \n",
             p1, p2, p3));
        printf("NestedLoop: TEST 3_5: Entry: [0x%08X, 0x%08X, 0x%08X] => \n",
             p1, p2, p3);
        rv = proxyObject->Test3_5(&p1, &p2, &p3, &r);
        LOG(("NestedLoop: TEST 3_5: Return:                                         => [0x%08X, 0x%08X, 0x%08X] --> Returns %d, rv=%d\n",
             p1, p2, p3, r, rv));
        printf("NestedLoop: TEST 3_5: Return: => [0x%08X, 0x%08X, 0x%08X] --> Returns %d, rv=%d\n",
             p1, p2, p3, r, rv);

        LOG(("NestedLoop: TEST 4_5: Entry: [0x%08X, 0x%08X, 0x%08X, 0x%08X] => \n",
             p1, p2, p3, p4));
        printf("NestedLoop: TEST 4_5: Entry: [0x%08X, 0x%08X, 0x%08X, 0x%08X] => \n",
             p1, p2, p3, p4);
        rv = proxyObject->Test4_5(&p1, &p2, &p3, &p4, &r);
        LOG(("NestedLoop: TEST 4_5: Return:                                         => [0x%08X, 0x%08X, 0x%08X, 0x%08X] --> Returns %d, rv=%d\n",
             p1, p2, p3, p4, r, rv));
        printf("NestedLoop: TEST 4_5: Return: => [0x%08X, 0x%08X, 0x%08X, 0x%08X] --> Returns %d, rv=%d\n",
             p1, p2, p3, p4, r, rv);

        LOG(("NestedLoop: TEST 5_5: Entry: [0x%08X, 0x%08X, 0x%08X, 0x%08X, 0x%08X] => \n",
             p1, p2, p3, p4, p5));
        printf("NestedLoop: TEST 5_5: Entry: [0x%08X, 0x%08X, 0x%08X, 0x%08X, 0x%08X] => \n",
             p1, p2, p3, p4, p5);
        rv = proxyObject->Test5_5(&p1, &p2, &p3, &p4, &p5, &r);
        LOG(("NestedLoop: TEST 5_5: Return:                                         => [0x%08X, 0x%08X, 0x%08X, 0x%08X, 0x%08X] --> Returns %d, rv=%d\n",
             p1, p2, p3, p4, p5, r, rv));
        printf("NestedLoop: TEST 5_5: Return: => [0x%08X, 0x%08X, 0x%08X, 0x%08X, 0x%08X] --> Returns %d, rv=%d\n",
             p1, p2, p3, p4, p5, r, rv);

        LOG(("NestedLoop: TEST 6_5: Entry: [0x%08X, 0x%08X, 0x%08X, 0x%08X, 0x%08X, 0x%08X] => \n",
             p1, p2, p3, p4, p5, p6));
        printf("NestedLoop: TEST 6_5: Entry: [0x%08X, 0x%08X, 0x%08X, 0x%08X, 0x%08X, 0x%08X] => \n",
             p1, p2, p3, p4, p5, p6);
        rv = proxyObject->Test6_5(&p1, &p2, &p3, &p4, &p5, &p6, &r);
        LOG(("NestedLoop: TEST 6_5: Return:                                         => [0x%08X, 0x%08X, 0x%08X, 0x%08X, 0x%08X, 0x%08X] --> Returns %d, rv=%d\n",
             p1, p2, p3, p4, p5, p6, r, rv));
        printf("NestedLoop: TEST 6_5: Return: => [0x%08X, 0x%08X, 0x%08X, 0x%08X, 0x%08X, 0x%08X] --> Returns %d, rv=%d\n",
             p1, p2, p3, p4, p5, p6, r, rv);

        LOG(("NestedLoop: TEST 7_5: Entry: [0x%08X, 0x%08X, 0x%08X, 0x%08X, 0x%08X, 0x%08X, 0x%08X] => \n",
             p1, p2, p3, p4, p5, p6, p7));
        printf("NestedLoop: TEST 7_5: Entry: [0x%08X, 0x%08X, 0x%08X, 0x%08X, 0x%08X, 0x%08X, 0x%08X] => \n",
             p1, p2, p3, p4, p5, p6, p7);
        rv = proxyObject->Test7_5(&p1, &p2, &p3, &p4, &p5, &p6, &p7, &r);
        LOG(("NestedLoop: TEST 7_5: Return:                                         => [0x%08X, 0x%08X, 0x%08X, 0x%08X, 0x%08X, 0x%08X, 0x%08X] --> Returns %d, rv=%d\n",
             p1, p2, p3, p4, p5, p6, p7, r, rv));
        printf("NestedLoop: TEST 7_5: Return: => [0x%08X, 0x%08X, 0x%08X, 0x%08X, 0x%08X, 0x%08X, 0x%08X] --> Returns %d, rv=%d\n",
             p1, p2, p3, p4, p5, p6, p7, r, rv);

        LOG(("NestedLoop: TEST 8_5: Entry: [0x%08X, 0x%08X, 0x%08X, 0x%08X, 0x%08X, 0x%08X, 0x%08X, 0x%08X] => \n",
             p1, p2, p3, p4, p5, p6, p7, p8));
        printf("NestedLoop: TEST 8_5: Entry: [0x%08X, 0x%08X, 0x%08X, 0x%08X, 0x%08X, 0x%08X, 0x%08X, 0x%08X] => \n",
             p1, p2, p3, p4, p5, p6, p7, p8);
        rv = proxyObject->Test8_5(&p1, &p2, &p3, &p4, &p5, &p6, &p7, &p8, &r);
        LOG(("NestedLoop: TEST 8_5: Return:                                         => [0x%08X, 0x%08X, 0x%08X, 0x%08X, 0x%08X, 0x%08X, 0x%08X, 0x%08X] --> Returns %d, rv=%d\n",
             p1, p2, p3, p4, p5, p6, p7, p8, r, rv));
        printf("NestedLoop: TEST 8_5: Return: => [0x%08X, 0x%08X, 0x%08X, 0x%08X, 0x%08X, 0x%08X, 0x%08X, 0x%08X] --> Returns %d, rv=%d\n",
             p1, p2, p3, p4, p5, p6, p7, p8, r, rv);

        LOG(("NestedLoop: TEST 9_5: Entry: [0x%08X, 0x%08X, 0x%08X, 0x%08X, 0x%08X, 0x%08X, 0x%08X, 0x%08X, 0x%08X] => \n",
             p1, p2, p3, p4, p5, p6, p7, p8, p9));
        printf("NestedLoop: TEST 9_5: Entry: [0x%08X, 0x%08X, 0x%08X, 0x%08X, 0x%08X, 0x%08X, 0x%08X, 0x%08X, 0x%08X] => \n",
             p1, p2, p3, p4, p5, p6, p7, p8, p9);
        rv = proxyObject->Test9_5(&p1, &p2, &p3, &p4, &p5, &p6, &p7, &p8, &p9, &r);
        LOG(("NestedLoop: TEST 9_5: Return:                                         => [0x%08X, 0x%08X, 0x%08X, 0x%08X, 0x%08X, 0x%08X, 0x%08X, 0x%08X, 0x%08X] --> Returns %d, rv=%d\n",
             p1, p2, p3, p4, p5, p6, p7, p8, p9, r, rv));
        printf("NestedLoop: TEST 9_5: Return: => [0x%08X, 0x%08X, 0x%08X, 0x%08X, 0x%08X, 0x%08X, 0x%08X, 0x%08X, 0x%08X] --> Returns %d, rv=%d\n",
             p1, p2, p3, p4, p5, p6, p7, p8, p9, r, rv);

        LOG(("NestedLoop: TEST 10_5: Entry: [0x%08X, 0x%08X, 0x%08X, 0x%08X, 0x%08X, 0x%08X, 0x%08X, 0x%08X, 0x%08X, 0x%08X] => \n",
             p1, p2, p3, p4, p5, p6, p7, p8, p9, p10));
        printf("NestedLoop: TEST 10_5: Entry: [0x%08X, 0x%08X, 0x%08X, 0x%08X, 0x%08X, 0x%08X, 0x%08X, 0x%08X, 0x%08X, 0x%08X] => \n",
             p1, p2, p3, p4, p5, p6, p7, p8, p9, p10);
        rv = proxyObject->Test10_5(&p1, &p2, &p3, &p4, &p5, &p6, &p7, &p8, &p9, &p10, &r);
        LOG(("NestedLoop: TEST 10_5: Return:                                         => [0x%08X, 0x%08X, 0x%08X, 0x%08X, 0x%08X, 0x%08X, 0x%08X, 0x%08X, 0x%08X, 0x%08X] --> Returns %d, rv=%d\n",
             p1, p2, p3, p4, p5, p6, p7, p8, p9, p10, r, rv));
        printf("NestedLoop: TEST 10_5: Return: => [0x%08X, 0x%08X, 0x%08X, 0x%08X, 0x%08X, 0x%08X, 0x%08X, 0x%08X, 0x%08X, 0x%08X] --> Returns %d, rv=%d\n",
             p1, p2, p3, p4, p5, p6, p7, p8, p9, p10, r, rv);

        NS_RELEASE(p10);
        NS_RELEASE(p9);
        NS_RELEASE(p8);
        NS_RELEASE(p7);
        NS_RELEASE(p6);
        NS_RELEASE(p5);
        NS_RELEASE(p4);
        NS_RELEASE(p3);
        NS_RELEASE(p2);
        NS_RELEASE(p1);
}


void TestCase_NestedLoop(nsIThread *thread, PRInt32 index)
{
    nsCOMPtr<nsIProxyObjectManager> manager =
            do_GetService(NS_XPCOMPROXY_CONTRACTID);

    LOG(("NestedLoop: ProxyObjectManager: %p\n", (void *) manager.get()));
    
    PR_ASSERT(manager);

    nsITestProxy         *proxyObject;
    nsTestXPCFoo*         foo   = new nsTestXPCFoo();
    
    PR_ASSERT(foo);
    
    TestCase_1_DirectTests(foo);
    
    manager->GetProxyForObject(thread, NS_GET_IID(nsITestProxy), foo, NS_PROXY_SYNC, (void**)&proxyObject);
    
    if (proxyObject)
    {
        // release ownership of the real object. 
        
        nsresult rv;
        
        LOG(("NestedLoop: Deleting real Object (%d)\n", index));
        NS_RELEASE(foo);
   
        PRInt32 retval;
        
        LOG(("NestedLoop: Getting EventThread...\n"));

        //nsCOMPtr<nsIThread> curThread = do_GetCurrentThread();
        PRThread *curThread = PR_GetCurrentThread();
        if (curThread)
        {
            LOG(("NestedLoop: Thread (%d) Prior to calling proxyObject->Test.\n", index));
            rv = proxyObject->Test(NS_PTR_TO_INT32((void*)curThread), 0, &retval);   // XXX broken on 64-bit arch
            LOG(("NestedLoop: Thread (%d) proxyObject error: %x.\n", index, rv));

        } else {
                LOG(("NestedLoop: No EventThread Found!\n"));
        }

        TestCase_1_ProxyTests(proxyObject);
        TestCase_2_ProxyTests(proxyObject);
        TestCase_3_ProxyTests(proxyObject);
        TestCase_4_ProxyTests(proxyObject);
        TestCase_5_ProxyTests(proxyObject);

        LOG(("NestedLoop: Deleting Proxy Object (%d)\n", index));
        NS_RELEASE(proxyObject);

        PR_Sleep( PR_MillisecondsToInterval(1000) );  // If your thread goes away, your stack goes away.  Only use ASYNC on calls that do not have out parameters
    }
    else
    {
        LOG(("NestedLoop: COULD NOT GET PROXY OBJECT!!!\n"));
        printf("NestedLoop: COULD NOT GET PROXY OBJECT!!!\n");
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

        nsresult rv;
        proxyObject->Test2(&rv);
        
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
    PRBool IsDone() { return mNumRuns == 0; }
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


int do_my_test(int numberOfThreads)
{

    if ( sLog == NULL )
		sLog = PR_NewLogModule("Test");

    NS_InitXPCOM2(nsnull, nsnull, nsnull);

    // Scope code so everything is destroyed before we run call NS_ShutdownXPCOM
    {
        nsCOMPtr<nsIComponentRegistrar> registrar;
        NS_GetComponentRegistrar(getter_AddRefs(registrar));
        registrar->AutoRegister(nsnull);

#if 0
        TestCase_1_Failure();
#else
        nsCOMPtr<nsIThread> eventLoopThread;
        NS_NewThread(getter_AddRefs(eventLoopThread));

	nsCOMPtr<nsIRunnable> test;
        //test = new TestSyncProxyToSelf();
        //eventLoopThread->Dispatch(test, NS_DISPATCH_NORMAL);

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
#endif
       
    }

    LOG(("TEST: Calling Cleanup.\n"));
    NS_ShutdownXPCOM(nsnull);

    LOG(("TEST: Return zero.\n"));
    return 0;
}



int
   main(void *myThis, int argc, char **argv)
{
        int numberOfThreads = 1;

        if (argc > 1)
                numberOfThreads = atoi(argv[1]);

        return do_my_test(numberOfThreads);
}






LRESULT APIENTRY wWinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpszCmdLine, int nCmdShow)
{
        do_my_test(1);
        
        return 0;
}





