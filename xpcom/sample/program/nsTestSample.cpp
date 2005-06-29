/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
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
 *   Suresh Duddi <dp@netscape.com>
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


/**
 * A Test application that exercises the sample moudule. This is intented
 * to be a sample application for using xpcom standalone.
 */

#include <stdio.h>

#include "nsISample.h"
#include "nsIServiceManager.h"
#include "nsIComponentRegistrar.h"

#ifdef XPCOM_GLUE
#include "nsXPCOMGlue.h"
#include "nsMemory.h"
#else
#include "nsXPIDLString.h"
#endif

#define NS_SAMPLE_CONTRACTID "@mozilla.org/sample;1"

int
main(void)
{
    nsresult rv;

#ifdef XPCOM_GLUE
    XPCOMGlueStartup(nsnull);
#endif

    // Initialize XPCOM
    nsCOMPtr<nsIServiceManager> servMan;
    rv = NS_InitXPCOM2(getter_AddRefs(servMan), nsnull, nsnull);
    if (NS_FAILED(rv))
    {
        printf("ERROR: XPCOM intialization error [%x].\n", rv);
        return -1;
    }
    // register all components in our default component directory
    nsCOMPtr<nsIComponentRegistrar> registrar = do_QueryInterface(servMan);
    NS_ASSERTION(registrar, "Null nsIComponentRegistrar");
    registrar->AutoRegister(nsnull);
    
    nsCOMPtr<nsIComponentManager> manager = do_QueryInterface(registrar);
    NS_ASSERTION(registrar, "Null nsIComponentManager");
    
    // Create an instance of our component
    nsCOMPtr<nsISample> mysample;
    rv = manager->CreateInstanceByContractID(NS_SAMPLE_CONTRACTID,
                                             nsnull,
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
#ifndef XPCOM_GLUE
    nsXPIDLCString str;
    rv = mysample->GetValue(getter_Copies(str));
#else
    char *str;
    rv = mysample->GetValue(&str);
#endif

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

#ifdef XPCOM_GLUE
    nsMemory::Free(str);
#endif
    rv = mysample->WriteValue("Final print :");
    printf("Test passed.\n");
    
    // All nsCOMPtr's must be deleted prior to calling shutdown XPCOM
    // as we should not hold references passed XPCOM Shutdown.
    servMan = 0;
    registrar = 0;
    manager = 0;
    mysample = 0;
    
    // Shutdown XPCOM
    NS_ShutdownXPCOM(nsnull);

#ifdef XPCOM_GLUE
    XPCOMGlueShutdown();
#endif
    return 0;
}
