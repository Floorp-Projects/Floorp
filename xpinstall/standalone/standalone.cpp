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

#include "nsSpecialSystemDirectory.h" 

#include "nscore.h"
#include "nspr.h"

#include "nsSimpleConsoleProgressNotifier.h"

/*********************************************
 SoftwareUpdate
*********************************************/
#include "nsISoftwareUpdate.h"
#include "nsSoftwareUpdateIIDs.h"

static nsISoftwareUpdate *softwareUpdate= NULL;
static NS_DEFINE_IID(kFileLocatorIID, NS_IFILELOCATOR_IID);
static NS_DEFINE_IID(kFileLocatorCID, NS_FILELOCATOR_CID);
/*********************************************/


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

        nsSimpleConsoleProgressNotifier *progress = new nsSimpleConsoleProgressNotifier();

        softwareUpdate->RegisterNotifier(progress);


        nsFileSpec jarFile(argv[i]);
        nsFileURL jarFileURL(jarFile);
        
        softwareUpdate->InstallJar(  nsString( jarFileURL.GetAsString() ) ,
                                     nsString( nsNSPRPath(jarFile) ), 
                                     0x00000001);

    }

    return rv;
}
