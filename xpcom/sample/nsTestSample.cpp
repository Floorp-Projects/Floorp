/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
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
 *   Suresh Duddi <dp@netscape.com>
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


/**
 * A Test application that exercises the sample moudule. This is intented
 * to be a sample application for using xpcom standalone.
 */

#include <nsISample.h>
#include <nsIServiceManager.h>
#include <nsXPIDLString.h>

#define NS_SAMPLE_CONTRACTID "@mozilla.org/sample;1"

int
main(void)
{
    nsresult rv;

    // Initialize XPCOM
    rv = NS_InitXPCOM(nsnull, nsnull);
    if (NS_FAILED(rv))
    {
        printf("ERROR: XPCOM intialization error [%x].\n", rv);
        return -1;
    }

    // Do Autoreg to make sure our component is registered. The real way of
    // doing this is running the xpcom registraion tool, regxpcom, at install
    // time to get components registered and not make this call everytime.
    // Ignore return value.
    //
    // Here we use the global component manager. Note that this will cause
    // linkage dependency to XPCOM library. We feel that linkage dependency
    // to XPCOM is inevitable and this is simpler to code.
    // To break free from such dependencies, we can GetService() the component
    // manager from the service manager that is returned from NS_InitXPCOM().
    // We feel that linkage dependency to XPCOM library is inevitable.
    (void) nsComponentManager::AutoRegister(nsIComponentManager::NS_Startup, nsnull);

    // Create an instance of our component
    nsCOMPtr<nsISample> mysample = do_CreateInstance(NS_SAMPLE_CONTRACTID, &rv);
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
    nsXPIDLCString str;
    rv = mysample->GetValue(getter_Copies(str));
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
    rv = mysample->WriteValue("Final print :");
    printf("Test passed.\n");

    // Shutdown XPCOM
    NS_ShutdownXPCOM(nsnull);
    return 0;
}
