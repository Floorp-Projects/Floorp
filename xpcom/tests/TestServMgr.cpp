/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
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

#include "MyService.h"
#include "nsIServiceManager.h"
#include <stdio.h>

static NS_DEFINE_IID(kIMyServiceCID, NS_IMYSERVICE_CID);
static NS_DEFINE_IID(kIMyServiceIID, NS_IMYSERVICE_IID);

////////////////////////////////////////////////////////////////////////////////

IMyService* myServ = NULL;

nsresult
BeginTest(int testNumber, nsIShutdownListener* listener)
{
    nsresult err;
    NS_ASSERTION(myServ == NULL, "myServ not reset");
    err = nsServiceManager::GetService(kIMyServiceCID, kIMyServiceIID,
                                       (nsISupports**)&myServ, listener);
    return err;
}

nsresult
EndTest(int testNumber, nsIShutdownListener* listener)
{
    nsresult err = NS_OK;

    if (myServ) {
        err = myServ->Doit();
        if (err != NS_OK) return err;

        err = nsServiceManager::ReleaseService(kIMyServiceCID, myServ, listener);
        if (err != NS_OK) return err;
        myServ = NULL;
    }
    
    printf("test %d succeeded\n", testNumber);
    return NS_OK;
}

nsresult
SimpleTest(int testNumber)
{
    // This test just gets a service, uses it and releases it.

    nsresult err;
    err = BeginTest(testNumber, NULL);
    if (err != NS_OK) return err;
    err = EndTest(testNumber, NULL);
    return err;
}

////////////////////////////////////////////////////////////////////////////////

class HandleMyServiceShutdown : public nsIShutdownListener {
public:

    NS_IMETHOD OnShutdown(const nsCID& cid, nsISupports* service);

    HandleMyServiceShutdown(void) { NS_INIT_REFCNT(); }
    virtual ~HandleMyServiceShutdown(void) {}

    NS_DECL_ISUPPORTS
};

static NS_DEFINE_IID(kIShutdownListenerIID, NS_ISHUTDOWNLISTENER_IID);
NS_IMPL_ISUPPORTS(HandleMyServiceShutdown, kIShutdownListenerIID);

nsresult
HandleMyServiceShutdown::OnShutdown(const nsCID& cid, nsISupports* service)
{
    if (cid.Equals(kIMyServiceCID)) {
        NS_ASSERTION(service == myServ, "wrong service!");
        nsrefcnt cnt = myServ->Release();
        myServ = NULL;
    }
    return NS_OK;
}

nsresult
AsyncShutdown(int testNumber)
{
    nsresult err;

    // If the AsyncShutdown was truly asynchronous and happened on another
    // thread, we'd have to protect all accesses to myServ throughout this
    // code with a monitor.

    err = nsServiceManager::ShutdownService(kIMyServiceCID);
    if (err == NS_ERROR_SERVICE_IN_USE) {
        printf("async shutdown -- service still in use\n");
        return NS_OK;
    }
    if (err == NS_ERROR_SERVICE_NOT_FOUND) {
        printf("async shutdown -- service not found\n");
        return NS_OK;
    }
    return err;
}

nsresult
AsyncShutdownTest(int testNumber)
{
    // This test gets a service, but then gets an async request for 
    // shutdown before it gets a chance to use it. This causes the myServ
    // variable to become NULL and EndTest to do nothing. The async request
    // will actually unload the DLL in this test.

    nsresult err;
    HandleMyServiceShutdown* listener = new HandleMyServiceShutdown();
    listener->AddRef();

    err = BeginTest(testNumber, listener);
    if (err != NS_OK) return err;
    err = AsyncShutdown(testNumber);
    if (err != NS_OK) return err;
    err = EndTest(testNumber, listener);

    nsrefcnt cnt = listener->Release();
    NS_ASSERTION(cnt == 0, "failed to release listener");
    return err;
}

nsresult
AsyncNoShutdownTest(int testNumber)
{
    // This test gets a service, and also gets an async request for shutdown,
    // but the service doesn't get shut down because some other client (who's
    // not participating in the async shutdown game as he should) is 
    // continuing to hang onto the service. This causes myServ variable to 
    // get set to NULL, but the service doesn't get unloaded right away as
    // it should.

    nsresult err;
    HandleMyServiceShutdown* listener = new HandleMyServiceShutdown();
    listener->AddRef();

    err = BeginTest(testNumber, listener);
    if (err != NS_OK) return err;

    // Create some other user of kIMyServiceCID, preventing it from
    // really going away:
    IMyService* otherClient;
    err = nsServiceManager::GetService(kIMyServiceCID, kIMyServiceIID,
                                       (nsISupports**)&otherClient);
    if (err != NS_OK) return err;

    err = AsyncShutdown(testNumber);
    if (err != NS_OK) return err;
    err = EndTest(testNumber, listener);

    // Finally, release the other client.
    err = nsServiceManager::ReleaseService(kIMyServiceCID, otherClient);
    if (err != NS_OK) return err;

    nsrefcnt cnt = listener->Release();
    NS_ASSERTION(cnt == 0, "failed to release listener");
    return err;
}

////////////////////////////////////////////////////////////////////////////////

void
SetupFactories(void)
{
    nsresult err;
    // seed the repository (hack)
    err = nsRepository::RegisterComponent(kIMyServiceCID, NULL, NULL, "MyService.dll",
                                        PR_TRUE, PR_FALSE);
    NS_ASSERTION(err == NS_OK, "failed to register my factory");
}

int
main(void)
{
    nsresult err;
    int testNumber = 0;

    SetupFactories();

    err = SimpleTest(++testNumber);
    if (err != NS_OK)
        goto error;

    err = AsyncShutdownTest(++testNumber);
    if (err != NS_OK)
        goto error;

    err = AsyncNoShutdownTest(++testNumber);
    if (err != NS_OK)
        goto error;

    AsyncShutdown(++testNumber);

    printf("there was great success\n");
    return 0;

  error:
    printf("test %d failed\n", testNumber);
    return -1;
}

////////////////////////////////////////////////////////////////////////////////
