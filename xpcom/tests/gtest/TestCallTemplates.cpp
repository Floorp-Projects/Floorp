/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim:cindent:ts=8:et:sw=4:
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/*
 * This test is NOT intended to be run.  It's a test to make sure
 * a group of functions BUILD correctly.
 */

#include "nsISupportsUtils.h"
#include "nsIWeakReference.h"
#include "nsIComponentManager.h"
#include "nsIServiceManager.h"
#include "nsWeakReference.h"
#include "nsIInterfaceRequestor.h"
#include "nsIInterfaceRequestorUtils.h"
#include "nsComponentManagerUtils.h"
#include "nsServiceManagerUtils.h"
#include "nsAutoPtr.h"
#include "mozilla/Attributes.h"

#define NS_ITESTSERVICE_IID \
  {0x127b5253, 0x37b1, 0x43c7, \
    { 0x96, 0x2b, 0xab, 0xf1, 0x2d, 0x22, 0x56, 0xae }}

class NS_NO_VTABLE nsITestService : public nsISupports {
  public:
    NS_DECLARE_STATIC_IID_ACCESSOR(NS_ITESTSERVICE_IID)
};

NS_DEFINE_STATIC_IID_ACCESSOR(nsITestService, NS_ITESTSERVICE_IID)

class nsTestService final : public nsITestService,
                            public nsSupportsWeakReference
{
    ~nsTestService() {}
  public:
    NS_DECL_ISUPPORTS
};

NS_IMPL_ISUPPORTS(nsTestService, nsITestService, nsISupportsWeakReference)

#define NS_TEST_SERVICE_CONTRACTID "@mozilla.org/test/testservice;1"
#define NS_TEST_SERVICE_CID \
  {0xa00c1406, 0x283a, 0x45c9, \
    {0xae, 0xd2, 0x1a, 0xb6, 0xdd, 0xba, 0xfe, 0x53}}
static NS_DEFINE_CID(kTestServiceCID, NS_TEST_SERVICE_CID);

void JustTestingCompilation()
{
    /*
     * NOTE:  This does NOT demonstrate how these functions are
     * intended to be used.  They are intended for filling in out
     * parameters that need to be |AddRef|ed.  I'm just too lazy
     * to write lots of little getter functions for a test program
     * when I don't need to.
     */

    MOZ_ASSERT_UNREACHABLE("This test is not intended to run, only to compile!");

    /* Test CallQueryInterface */

    nsISupports *mySupportsPtr = reinterpret_cast<nsISupports*>(0x1000);

    nsITestService *myITestService = nullptr;
    CallQueryInterface(mySupportsPtr, &myITestService);

    nsTestService *myTestService =
        reinterpret_cast<nsTestService*>(mySupportsPtr);
    nsISupportsWeakReference *mySupportsWeakRef;
    CallQueryInterface(myTestService, &mySupportsWeakRef);

    nsCOMPtr<nsISupports> mySupportsCOMPtr = mySupportsPtr;
    CallQueryInterface(mySupportsCOMPtr, &myITestService);

    RefPtr<nsTestService> myTestServiceRefPtr = myTestService;
    CallQueryInterface(myTestServiceRefPtr, &mySupportsWeakRef);

    /* Test CallQueryReferent */

    nsIWeakReference *myWeakRef =
        static_cast<nsIWeakReference*>(mySupportsPtr);
    CallQueryReferent(myWeakRef, &myITestService);

    /* Test CallCreateInstance */

    CallCreateInstance(kTestServiceCID, mySupportsPtr, &myITestService);
    CallCreateInstance(kTestServiceCID, &myITestService);
    CallCreateInstance(NS_TEST_SERVICE_CONTRACTID, mySupportsPtr,
                       &myITestService);
    CallCreateInstance(NS_TEST_SERVICE_CONTRACTID, &myITestService);

    /* Test CallGetService */
    CallGetService(kTestServiceCID, &myITestService);
    CallGetService(NS_TEST_SERVICE_CONTRACTID, &myITestService);

    /* Test CallGetInterface */
    nsIInterfaceRequestor *myInterfaceRequestor =
        static_cast<nsIInterfaceRequestor*>(mySupportsPtr);
    CallGetInterface(myInterfaceRequestor, &myITestService);
}
