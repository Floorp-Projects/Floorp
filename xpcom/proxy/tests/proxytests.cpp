/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/*
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

#include <stdio.h>

#include "nsRepository.h"
#include "nsIServiceManager.h"
#include "nsCOMPtr.h"
#include "nsSpecialSystemDirectory.h"    // For exe dir

#include "nscore.h"
#include "nspr.h"
#include "prmon.h"


#include "../../../js/src/xpconnect/public/xpctest.h"

#include "nsProxyObjectManager.h"

static NS_DEFINE_IID(kProxyObjectManagerIID, NS_IPROXYEVENT_MANAGER_IID);
static NS_DEFINE_IID(kProxyObjectManagerCID, NS_PROXYEVENT_MANAGER_CID);


/***************************************************************************/
/* Setup nsIAllocator                                                      */
/***************************************************************************/

#include "nsIAllocator.h"

static NS_DEFINE_IID(kIAllocatorIID, NS_IALLOCATOR_IID);
static NS_DEFINE_IID(kAllocatorCID, NS_ALLOCATOR_CID);

#ifdef XP_PC
#define XPCOM_DLL  "xpcom32.dll"
#else
#ifdef XP_MAC
#define XPCOM_DLL  "XPCOM_DLL"
#else
#define XPCOM_DLL  "libxpcom.so"
#endif
#endif


#ifdef XP_PC
#define LIBPROXY_DLL  "libproxy.dll"
#else
#ifdef XP_MAC
#define LIBPROXY_DLL  "??????????????????"
#else
#define LIBPROXY_DLL  "libproxy.so"
#endif
#endif



/***************************************************************************/
extern "C" void
NS_SetupRegistry()
{
  // Autoregistration happens here. The rest of RegisterComponent() calls should happen
  // only for dlls not in the components directory.

  // Create exeDir/"components"
  nsSpecialSystemDirectory sysdir(nsSpecialSystemDirectory::OS_CurrentProcessDirectory);
  sysdir += "components";
  const char *componentsDir = sysdir.GetCString(); // native path
  if (componentsDir != NULL)
  {
#ifdef XP_PC
      /* The PC version of the directory from filePath is of the form
       *    /y|/moz/mozilla/dist/bin/components
       * We need to remove the initial / and change the | to :
       * for all this to work with NSPR.      
       */
#endif /* XP_PC */
      printf("nsComponentManager: Using components dir: %s\n", componentsDir);

#ifdef XP_MAC
      nsComponentManager::AutoRegister(nsIComponentManager::NS_Startup, nsnull);
#else
      nsComponentManager::AutoRegister(nsIComponentManager::NS_Startup, componentsDir);
#endif    /* XP_MAC */
      // XXX Look for user specific components
      // XXX UNIX: ~/.mozilla/components
  }


    nsComponentManager::RegisterComponent(kAllocatorCID, NULL, NULL, XPCOM_DLL,
                                    PR_FALSE, PR_FALSE);

    nsComponentManager::RegisterComponent(kProxyObjectManagerCID, NULL, NULL, LIBPROXY_DLL,
                                    PR_FALSE, PR_FALSE);

}


/***************************************************************************/
/* nsTestXPCFoo                                                            */
/***************************************************************************/
class nsTestXPCFoo : public nsITestXPCFoo
{
    NS_DECL_ISUPPORTS
    NS_IMETHOD Test(int p1, int p2, int* retval);
    NS_IMETHOD Test2();
    nsTestXPCFoo();
    virtual ~nsTestXPCFoo();

};

nsTestXPCFoo::nsTestXPCFoo()
{
    NS_INIT_REFCNT();
    NS_ADDREF_THIS();
}

nsTestXPCFoo::~nsTestXPCFoo()
{
}

static NS_DEFINE_IID(kITestXPCFooIID, NS_ITESTXPCFOO_IID);
NS_IMPL_ISUPPORTS(nsTestXPCFoo,kITestXPCFooIID)

NS_IMETHODIMP nsTestXPCFoo::Test(int p1, int p2, int* retval)
{
    printf("Thread (%d) Test Called successfully! Party on...\n", p1);
    *retval = 0;
    return NS_OK;
}


NS_IMETHODIMP nsTestXPCFoo::Test2()
{
    printf("The quick brown netscape jumped over the old lazy ie..\n");

    return NS_OK;
}



/***************************************************************************/
/* nsTestXPCFoo2                                                           */
/***************************************************************************/
class nsTestXPCFoo2 : public nsITestXPCFoo
{
    NS_DECL_ISUPPORTS
    NS_IMETHOD Test(int p1, int p2, int* retval);
    NS_IMETHOD Test2();
    nsTestXPCFoo2();
    virtual ~nsTestXPCFoo2();

};

nsTestXPCFoo2::nsTestXPCFoo2()
{
    NS_INIT_REFCNT();
    NS_ADDREF_THIS();
}

nsTestXPCFoo2::~nsTestXPCFoo2()
{
}
//kITestXPCFooIID defined above for nsTestXPCFoo(1)
NS_IMPL_ISUPPORTS(nsTestXPCFoo2,kITestXPCFooIID)

NS_IMETHODIMP nsTestXPCFoo2::Test(int p1, int p2, int* retval)
{
    printf("Thread (%d) nsTestXPCFoo2::Test2() called successfully! Party on...\n", p1);
    *retval = 0;
    return NS_OK;
}


NS_IMETHODIMP nsTestXPCFoo2::Test2()
{
    printf("nsTestXPCFoo2::Test2() called\n");

    return NS_OK;
}






typedef struct _ArgsStruct
{
    PLEventQueue* queue;
    int           threadNumber;
}ArgsStruct;













// This will create two objects both descendants of a single IID.
void TestCase_TwoClassesOneInterface(void *arg)
{

    ArgsStruct *argsStruct = (ArgsStruct*) arg;


    nsIProxyObjectManager*  proxyObjectFactory;
    
    nsComponentManager::CreateInstance(kProxyObjectManagerCID,   
                                       nsnull,
                                       nsIProxyObjectManager::GetIID(),
                                       (void**)&proxyObjectFactory);

    PR_ASSERT(proxyObjectFactory);

    nsITestXPCFoo         *proxyObject;
    nsITestXPCFoo         *proxyObject2;

    nsTestXPCFoo*        foo   = new nsTestXPCFoo();
    nsTestXPCFoo2*       foo2  = new nsTestXPCFoo2();
    
    
    PR_ASSERT(foo);
    PR_ASSERT(foo2);

    proxyObjectFactory->GetProxyObject(argsStruct->queue, nsITestXPCFoo::GetIID(), foo, (void**)&proxyObject);
    
    proxyObjectFactory->GetProxyObject(argsStruct->queue, nsITestXPCFoo::GetIID(), foo2, (void**)&proxyObject2);

    if (proxyObject && proxyObject2)
    {
    // release ownership of the real object.        
        //printf("Deleting real Object (%ld)\n", threadNumber);
        NS_RELEASE(foo);
   
        //printf("Deleting real Object 2 (%ld)\n", threadNumber);
        NS_RELEASE(foo2);

    
        int a;
        nsresult rv;
        int threadNumber = argsStruct->threadNumber;

        //printf("Thread (%ld) Prior to calling proxyObject->Test.\n", threadNumber);
        rv = proxyObject->Test(threadNumber, 0, &a);   
        //printf("Thread (%ld) error: %ld.\n", threadNumber, rv);


        //printf("Thread (%ld) Prior to calling proxyObject->Test2.\n", threadNumber);
        rv = proxyObject->Test2();   
        //printf("Thread (%ld) error: %ld.\n", threadNumber, rv);


        //printf("Thread (%ld) Prior to calling proxyObject2->Test.\n", threadNumber);
        rv = proxyObject2->Test(threadNumber, 0, &a);   
        //printf("Thread (%ld) proxyObject2 error: %ld.\n", threadNumber, rv);

        //printf("Thread (%ld) Prior to calling proxyObject2->Test2.\n", threadNumber);
        rv = proxyObject2->Test2();   
        //printf("Thread (%ld) proxyObject2 error: %ld.\n", threadNumber, rv);

        //printf("Deleting Proxy Object (%ld)\n", threadNumber );
        NS_RELEASE(proxyObject);

        //printf("Deleting Proxy Object 2 (%ld)\n", threadNumber );
        NS_RELEASE(proxyObject2);
    }    
}






void TestCase_2(void *arg)
{

    ArgsStruct *argsStruct = (ArgsStruct*) arg;

    nsIProxyObjectManager*  proxyObjectFactory;
    
    nsComponentManager::CreateInstance(kProxyObjectManagerCID,   
                                       nsnull,
                                       nsIProxyObjectManager::GetIID(),
                                       (void**)&proxyObjectFactory);
    
    PR_ASSERT(proxyObjectFactory);

    nsITestXPCFoo         *proxyObject;

    proxyObjectFactory->GetProxyObject(argsStruct->queue,
                                       nsITestXPCFoo::GetIID(),  // should be a cid 
                                       nsnull, 
                                       nsITestXPCFoo::GetIID(), 
                                       (void**)&proxyObject);
    
    if (proxyObject != nsnull)
    {
        nsISupports *a;
        nsISupports *b;

        NS_RELEASE(proxyObject);
    
    }
}





/***************************************************************************/
/* ProxyTest                                                               */
/***************************************************************************/

static void PR_CALLBACK ProxyTest( void *arg )
{
    TestCase_TwoClassesOneInterface(arg);
    TestCase_2(arg);

    free((void*) arg);
}


PLEventQueue *gEventQueue = nsnull;

static void PR_CALLBACK EventLoop( void *arg )
{
    printf("Creating EventQueue...\n");

    gEventQueue = PL_CreateEventQueue("mainqueue", PR_GetCurrentThread());
     
    printf("Looping for events.\n");
    
    while ( PR_SUCCESS == PR_Sleep( PR_MillisecondsToInterval(1)) )
    {
		PL_HandleEvent(PL_GetEvent(gEventQueue));
    }

    PL_ProcessPendingEvents(gEventQueue);

    printf("Closing down Event Queue.\n");
    PL_DestroyEventQueue( gEventQueue );


    printf("End looping for events.\n\n");
}

int
main(int argc, char **argv)
{
    int numberOfThreads = 1;

    if (argc > 1)
        numberOfThreads = atoi(argv[1]);

    NS_SetupRegistry();


    static PRThread** threads = (PRThread**) calloc(sizeof(PRThread*), numberOfThreads);
    static PRThread*  aEventThread;
    
    aEventThread =   PR_CreateThread(PR_USER_THREAD,
                                     EventLoop,
                                     NULL,
                                     PR_PRIORITY_NORMAL,
                                     PR_GLOBAL_THREAD,
                                     PR_JOINABLE_THREAD,
                                     0 );

    
    PR_Sleep(PR_MillisecondsToInterval(5000));

    PR_ASSERT(gEventQueue); // BAD BAD BAD.  EVENT THREAD DID NOT CREATE QUEUE.  This may be a timing issue, set the 
                            // sleep about longer, and try again.

    printf("Spawn Threads:\n");
    for (int spawn = 0; spawn < numberOfThreads; spawn++)
    {

        ArgsStruct *args = (ArgsStruct *) malloc (sizeof(ArgsStruct));
        
        args->queue = gEventQueue;
        args->threadNumber = spawn;

        threads[spawn]  =   PR_CreateThread(PR_USER_THREAD,
                                            ProxyTest,
                                            args,
                                            PR_PRIORITY_NORMAL,
                                            PR_GLOBAL_THREAD,
                                            PR_JOINABLE_THREAD,
                                            0 );

        printf("\tThread (%d) spawned\n", spawn);

        //PR_Sleep( PR_MillisecondsToInterval(250) );
    }

    printf("All Threads Spawned.\n\n");
    
   
    printf("Wait for threads.\n");
    for (int i = 0; i < numberOfThreads; i++)
    {
        PRStatus rv;
        printf("Thread (%d) Join...\n", i);
        rv = PR_JoinThread(threads[i]);
        printf("Thread (%d) Joined. (error: %ld).\n", i, rv);
    }

    PR_Interrupt(aEventThread);
    PR_JoinThread(aEventThread);
    

    printf("Calling Cleanup.\n");
    PR_Cleanup();

    printf("Return zero.\n");
    return 0;
}
