/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/*
 * The contents of this file are subject to the Netscape Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
 */

#include <stdio.h>

#include "nsIComponentManager.h"
#include "nsIServiceManager.h"
#include "nsCOMPtr.h"

#include "nsSpecialSystemDirectory.h" 

#include "nscore.h"
#include "nspr.h"

#include "nsSimpleNotifier.h"

/*********************************************
 SoftwareUpdate
*********************************************/
#include "nsISoftwareUpdate.h"
#include "nsSoftwareUpdateIIDs.h"

static nsISoftwareUpdate *softwareUpdate= NULL;
static NS_DEFINE_IID(kISoftwareUpdateIID, NS_ISOFTWAREUPDATE_IID);
static NS_DEFINE_IID(kSoftwareUpdateCID, NS_SoftwareUpdate_CID);
/*********************************************/


extern "C" void
NS_SetupRegistry()
{
    nsComponentManager::AutoRegister(nsIComponentManager::NS_Startup,
                                     nsnull /* default */);
}
  /***************************************************************************/

static void
xpinstall_usage(int argc, char *argv[])
{
    fprintf(stderr, "Usage: %s [-f flags] [-a arguments] filename\n", argv[0]);
}

int
main(int argc, char **argv)
{
 
    for (int i = 1; i < argc; i++) 
    {
        if (argv[i][0] != '-')
            break;

        switch (argv[i][1]) 
        {
            case 'f':
            if (argv[i][2] == '\0' && i == argc) 
            {
                fputs("ERROR: missing path after -f\n", stderr);
                xpinstall_usage(argc, argv);
                return 1;
            }
            //foo = argv[i + 1];
            i++;
            break;

          default:
            fprintf(stderr, "unknown option %s\n", argv[i]);
            xpinstall_usage(argc, argv);
            return 1;
        }
    }



    NS_SetupRegistry();
    
    

    nsresult rv = nsComponentManager::CreateInstance(kSoftwareUpdateCID, 
                                                     nsnull,
                                                     kISoftwareUpdateIID,
                                                     (void**) &softwareUpdate);
     

    if (NS_SUCCEEDED(rv))
    {

        nsSimpleNotifier *progress = new nsSimpleNotifier();


        nsFileSpec jarFile(argv[i]);
        nsFileURL jarFileURL(jarFile);
        
        softwareUpdate->InstallJar(  nsString( jarFileURL.GetAsString() ) ,
                                     nsString( nsNSPRPath(jarFile) ), 
                                     0x0000FFFF,
                                     progress);

    }

    return rv;
}
