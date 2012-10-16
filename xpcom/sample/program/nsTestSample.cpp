/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */


/**
 * A Test application that exercises the sample moudule. This is intented
 * to be a sample application for using xpcom standalone.
 */

#include <stdio.h>

#include "nsXPCOMGlue.h"
#include "nsXPCOM.h"
#include "nsCOMPtr.h"
#include "nsISample.h"
#include "nsIServiceManager.h"
#include "nsIComponentManager.h"
#include "nsIComponentRegistrar.h"

#define NS_SAMPLE_CONTRACTID "@mozilla.org/sample;1"

int
main(void)
{
    nsresult rv;

    XPCOMGlueStartup(nullptr);

    // Initialize XPCOM
    nsCOMPtr<nsIServiceManager> servMan;
    rv = NS_InitXPCOM2(getter_AddRefs(servMan), nullptr, nullptr);
    if (NS_FAILED(rv))
    {
        printf("ERROR: XPCOM intialization error [%x].\n", rv);
        return -1;
    }

    nsCOMPtr<nsIComponentManager> manager = do_QueryInterface(servMan);
    
    // Create an instance of our component
    nsCOMPtr<nsISample> mysample;
    rv = manager->CreateInstanceByContractID(NS_SAMPLE_CONTRACTID,
                                             nullptr,
                                             NS_GET_IID(nsISample),
                                             getter_AddRefs(mysample));
    if (NS_FAILED(rv))
    {
        printf("ERROR: Cannot create instance of component " NS_SAMPLE_CONTRACTID " [%x].\n"
               "Debugging hint:\n"
               "\tsetenv NSPR_LOG_MODULES nsComponentManager:5\n"
               "\tsetenv NSPR_LOG_FILE xpcom.log\n"
               "\t./nsTestSample\n"
               "\t<check the contents for xpcom.log for possible cause of error>.\n",
               rv);
        return -2;
    }

    // Call methods on our sample to test it out.
    rv = mysample->WriteValue("Inital print:");
    if (NS_FAILED(rv))
    {
        printf("ERROR: Calling nsISample::WriteValue() [%x]\n", rv);
        return -3;
    }

    const char *testValue = "XPCOM defies gravity";
    rv = mysample->SetValue(testValue);
    if (NS_FAILED(rv))
    {
        printf("ERROR: Calling nsISample::SetValue() [%x]\n", rv);
        return -3;
    }
    printf("Set value to: %s\n", testValue);
    char *str;
    rv = mysample->GetValue(&str);

    if (NS_FAILED(rv))
    {
        printf("ERROR: Calling nsISample::GetValue() [%x]\n", rv);
        return -3;
    }
    if (strcmp(str, testValue))
    {
        printf("Test FAILED.\n");
        return -4;
    }

    NS_Free(str);

    rv = mysample->WriteValue("Final print :");
    printf("Test passed.\n");
    
    // All nsCOMPtr's must be deleted prior to calling shutdown XPCOM
    // as we should not hold references passed XPCOM Shutdown.
    servMan = 0;
    manager = 0;
    mysample = 0;
    
    // Shutdown XPCOM
    NS_ShutdownXPCOM(nullptr);

    XPCOMGlueShutdown();
    return 0;
}
