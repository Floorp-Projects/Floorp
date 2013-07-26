/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "TestCommon.h"
#include "nsXPCOM.h"
#include "nsIServiceManager.h"
#include "nsServiceManagerUtils.h"
#include "nsIEventTarget.h"
#include "nsCOMPtr.h"
#include "nsNetCID.h"
#include "prlog.h"

#if defined(PR_LOGGING)
//
// set NSPR_LOG_MODULES=Test:5
//
static PRLogModuleInfo *gTestLog = nullptr;
#endif
#define LOG(args) PR_LOG(gTestLog, PR_LOG_DEBUG, args)

class nsIOEvent : public nsIRunnable {
public:
    NS_DECL_THREADSAFE_ISUPPORTS

    nsIOEvent(int i) : mIndex(i) {}

    NS_IMETHOD Run() {
        LOG(("Run [%d]\n", mIndex));
        return NS_OK;
    }

private:
    int mIndex;
};
NS_IMPL_ISUPPORTS1(nsIOEvent, nsIRunnable)

static nsresult RunTest()
{
    nsresult rv;
    nsCOMPtr<nsIEventTarget> target =
        do_GetService(NS_STREAMTRANSPORTSERVICE_CONTRACTID, &rv);
    if (NS_FAILED(rv))
        return rv;

    for (int i=0; i<10; ++i) {
        nsCOMPtr<nsIRunnable> event = new nsIOEvent(i); 
        LOG(("Dispatch %d\n", i));
        target->Dispatch(event, NS_DISPATCH_NORMAL);
    }

    return NS_OK;
}

int main(int argc, char **argv)
{
    if (test_common_init(&argc, &argv) != 0)
        return -1;

    nsresult rv;

#if defined(PR_LOGGING)
    gTestLog = PR_NewLogModule("Test");
#endif

    rv = NS_InitXPCOM2(nullptr, nullptr, nullptr);
    if (NS_FAILED(rv))
        return rv;

    rv = RunTest();
    if (NS_FAILED(rv))
        LOG(("RunTest failed [rv=%x]\n", rv));

    LOG(("sleeping main thread for 2 seconds...\n"));
    PR_Sleep(PR_SecondsToInterval(2));
    
    NS_ShutdownXPCOM(nullptr);
    return 0;
}
