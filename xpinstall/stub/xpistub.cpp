/* -*- Mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
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
 * The Original Code is Mozilla Communicator client code, 
 * released March 31, 1998. 
 *
 * The Initial Developer of the Original Code is Netscape Communications 
 * Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
 *     Daniel Veditz <dveditz@netscape.com>
 *     Pierre Phaneuf <pp@ludusdesign.com>
 */

#include "xpistub.h"

#include "nsIComponentManager.h"
#include "nsIComponentRegistrar.h"
#include "nsIServiceManager.h"
#include "nsCOMPtr.h"
#include "nsLocalFile.h"

#include "nscore.h"
#include "nspr.h"

#include "nsStubNotifier.h"

#include "nsISoftwareUpdate.h"
#include "nsSoftwareUpdateIIDs.h"
#include "nsPIXPIStubHook.h"

#include "plstr.h"

#if defined(XP_WIN) || defined(XP_OS2)
#if defined(XP_OS2)
#include <stdlib.h>
#define MAX_PATH _MAX_PATH
#else
#include <windows.h>
#endif
#ifndef XP_OS2_EMX
#include <direct.h>
#endif /*emx*/
#define COMPONENT_REG "component.reg"
#endif

#ifdef XP_MAC
#define COMPONENT_REG "\pComponent Registry"
#include "MoreFilesExtras.h"
#endif

#ifdef XP_UNIX
#include <unistd.h>
#define COMPONENT_REG "component.reg"
#endif

//------------------------------------------------------------------------
//          globals
//------------------------------------------------------------------------

static nsIXPIListener      *gListener = 0;
static nsISoftwareUpdate   *gXPI = 0;
static nsIServiceManager   *gServiceMgr = 0;

static NS_DEFINE_IID(kSoftwareUpdateCID, NS_SoftwareUpdate_CID);

PRInt32 gInstallStatus;



//------------------------------------------------------------------------
//          XPI_Init()
//------------------------------------------------------------------------
PR_PUBLIC_API(nsresult) XPI_Init(   
#ifdef XP_MAC
                                    const FSSpec&       aXPIStubDir, 
                                    const FSSpec&       aProgramDir,
#else
                                    const char*         aProgramDir,
#endif
                                    const char*         aLogName,
                                    pfnXPIProgress      progressCB )
{
    nsresult              rv;

    //--------------------------------------------------------------------
    // Initialize XPCOM and AutoRegister() its components
    //--------------------------------------------------------------------
#ifdef XP_MAC
    nsCOMPtr<nsILocalFileMac>	binDir;
    rv = NS_NewLocalFileWithFSSpec((FSSpec*)&aXPIStubDir, PR_FALSE, getter_AddRefs(binDir));
    if (NS_FAILED(rv)) return rv;
    
    rv = NS_InitXPCOM2(&gServiceMgr, binDir, nsnull);
    
    // binDir is contaminated now.  Need compDir to pass to AutoRegister.
    nsCOMPtr<nsILocalFileMac>	compDir;
    rv = NS_NewLocalFileWithFSSpec((FSSpec*)&aXPIStubDir, PR_FALSE, getter_AddRefs(compDir));
    if (NS_FAILED(rv)) return rv;

#elif defined(XP_WIN) || defined(XP_OS2)

 #ifdef XP_OS2_EMX
    char componentPath[MAX_PATH];
    _getcwd2(componentPath, MAX_PATH);
    int len = strlen(componentPath);
    for (int i = 0; i < len; i++) {
      if (componentPath[i] == '/') {
        componentPath[i] = '\\';
      }
    }
 #else
    char componentPath[MAX_PATH];
    getcwd(componentPath, MAX_PATH);
 #endif

    nsCOMPtr<nsILocalFile> file;
    rv = NS_NewNativeLocalFile(nsDependentCString(componentPath), PR_TRUE, getter_AddRefs(file));
    if (NS_FAILED(rv)) return rv;
    
    rv = NS_InitXPCOM2(&gServiceMgr, file, nsnull); 

#elif defined(XP_UNIX)

    rv = NS_InitXPCOM2(&gServiceMgr, nsnull, nsnull); 

    char cwd[1024];
    char compDirPath[1024];

    memset(cwd, 0, 1024);
    memset(compDirPath, 0, 1024);
    getcwd(cwd, 1024);
    sprintf(compDirPath, "%s/components", cwd);

    nsCOMPtr<nsILocalFile> compDir;
    NS_NewNativeLocalFile(nsDependentCString(compDirPath), PR_TRUE, getter_AddRefs(compDir));

#else

    rv = NS_InitXPCOM2(&gServiceMgr, NULL, NULL);

#endif

    if (NS_FAILED(rv))
        return rv;

    nsCOMPtr<nsIComponentRegistrar> registrar = do_QueryInterface(gServiceMgr);
    NS_ASSERTION(registrar, "Null nsIComponentRegistrar");

#if defined(XP_UNIX) || defined(XP_MAC)
    rv = registrar->AutoRegister(compDir);
#else
    rv = registrar->AutoRegister(nsnull);
#endif
    if (NS_FAILED(rv))
        return rv;


    //--------------------------------------------------------------------
    // Get the SoftwareUpdate (XPInstall) service.
    //
    // Since AppShell is not started by XPIStub the XPI service is never 
    // registered with the service manager. We keep a local pointer to it 
    // so it stays alive througout.
    //--------------------------------------------------------------------
    rv = nsComponentManager::CreateInstance(kSoftwareUpdateCID, 
                                            nsnull,
                                            NS_GET_IID(nsISoftwareUpdate),
                                            (void**) &gXPI);
    if (NS_FAILED(rv))
        return rv;


    //--------------------------------------------------------------------
    // Override XPInstall's natural assumption that the current executable
    // is Mozilla. Use the given directory as the "Program" folder.
    //--------------------------------------------------------------------
    nsCOMPtr<nsPIXPIStubHook>   hook = do_QueryInterface(gXPI);
    nsCOMPtr<nsILocalFile>      iDirSpec;
  
#if XP_MAC
	nsCOMPtr<nsILocalFileMac> iMacDirSpec;
	NS_NewLocalFileWithFSSpec((FSSpec *)&aProgramDir, PR_TRUE, getter_AddRefs(iMacDirSpec));
	iDirSpec = do_QueryInterface(iMacDirSpec);
#else
	NS_NewNativeLocalFile(nsDependentCString(aProgramDir), PR_TRUE, getter_AddRefs(iDirSpec));
#endif    
    
    if (hook && iDirSpec)
    {
        rv = hook->StubInitialize( iDirSpec, aLogName );
        if (NS_FAILED(rv)) return rv;
    }
    else
        return NS_ERROR_NULL_POINTER;


    //--------------------------------------------------------------------
    // Save the install wizard's callbacks as a nsIXPINotifer for later
    //--------------------------------------------------------------------
    nsStubListener* stub = new nsStubListener( progressCB );
    if (!stub)
    {
        gXPI->Release();
        rv = NS_ERROR_OUT_OF_MEMORY;
    }
    else
    {
        rv = stub->QueryInterface(NS_GET_IID(nsIXPIListener), (void**)&gListener);
    }
    return rv;
}



//------------------------------------------------------------------------
//          XPI_Exit()
//------------------------------------------------------------------------
PR_PUBLIC_API(void) XPI_Exit()
{
    if (gListener)
        gListener->Release();

    if (gXPI)
        gXPI->Release();

    NS_ShutdownXPCOM(gServiceMgr);

}




//------------------------------------------------------------------------
//          XPI_Install()
//------------------------------------------------------------------------
PR_PUBLIC_API(PRInt32) XPI_Install(
#ifdef XP_MAC
                                    const FSSpec& aFile,
#else
                                    const char*   aFile,
#endif
                                    const char*   aArgs, 
                                    long          aFlags )
{
    nsresult                rv = NS_ERROR_NULL_POINTER;
    nsString                args; args.AssignWithConversion(aArgs);
    nsCOMPtr<nsILocalFile>  iFile;

    gInstallStatus = -322; // unique stub error code
    
#if XP_MAC
	nsCOMPtr<nsILocalFileMac> iMacFile;
	NS_NewLocalFileWithFSSpec((FSSpec *)&aFile, PR_TRUE, getter_AddRefs(iMacFile));
	iFile = do_QueryInterface(iMacFile);
#else
	NS_NewNativeLocalFile(nsDependentCString(aFile), PR_TRUE, getter_AddRefs(iFile));
#endif  

    if (iFile && gXPI)
        rv = gXPI->InstallJar( iFile,
                               nsnull,
                               args.get(),
                               nsnull,
                               (aFlags | XPI_NO_NEW_THREAD),
                               gListener );

    return gInstallStatus;
}
