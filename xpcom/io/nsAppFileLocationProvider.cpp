/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
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
 * Copyright (C) 2000 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s):
 *     Conrad Carlen <conrad@ingress.com>
 */

#include "nsAppFileLocationProvider.h"
#include "nsAppDirectoryServiceDefs.h"
#include "nsDirectoryServiceDefs.h"
#include "nsIAtom.h"
#include "nsILocalFile.h"
#include "nsString.h"
#include "nsXPIDLString.h"

#if defined(XP_MAC)
#include <Folders.h>
#include <Script.h>
#include <Processes.h>
#include <Gestalt.h>
#include "nsILocalFileMac.h"
#endif
#if defined(XP_OS2)
#define INCL_DOSPROCESS
#define INCL_DOSMODULEMGR
#include <os2.h>
#elif defined(XP_PC)
#include <windows.h>
#include <shlobj.h>
#elif defined(XP_UNIX)
#include <unistd.h>
#include <stdlib.h>
#include <sys/param.h>
#include <prenv.h>
#elif defined(XP_BEOS)
#include <sys/param.h>
#include <kernel/image.h>
#include <storage/FindDirectory.h>
#endif


// WARNING: These hard coded names need to go away. They need to
// come from localizable resources

#ifdef XP_MAC
#define APP_REGISTRY_NAME "Application Registry"
#elif defined(XP_WIN) || defined(XP_OS2)
#define APP_REGISTRY_NAME "registry.dat"
#else
#define APP_REGISTRY_NAME "appreg"
#endif

// define default product directory
#if defined(XP_WIN) || defined(XP_MAC) || defined(XP_OS2) || defined(XP_BEOS)
#define DEFAULT_PRODUCT_DIR "Mozilla"
#elif defined (XP_UNIX)
#define DEFAULT_PRODUCT_DIR ".mozilla"
#endif


#if XP_MAC
#define DEFAULTS_DIR_NAME           "Defaults"
#define DEFAULTS_PREF_DIR_NAME      "Pref"
#define DEFAULTS_PROFILE_DIR_NAME   "Profile"
#define RES_DIR_NAME                "Res"
#define CHROME_DIR_NAME             "Chrome"
#define PLUGINS_DIR_NAME            "Plug-ins"
#define SEARCH_DIR_NAME             "Search Plugins"
#else
#define DEFAULTS_DIR_NAME           "defaults"
#define DEFAULTS_PREF_DIR_NAME      "pref"
#define DEFAULTS_PROFILE_DIR_NAME   "profile"
#define RES_DIR_NAME                "res"
#define CHROME_DIR_NAME             "chrome"
#define PLUGINS_DIR_NAME            "plugins"
#define SEARCH_DIR_NAME             "searchplugins"
#endif

//*****************************************************************************
// nsAppFileLocationProvider::Constructor/Destructor
//*****************************************************************************

nsAppFileLocationProvider::nsAppFileLocationProvider()
{
    NS_INIT_ISUPPORTS();
}

nsAppFileLocationProvider::~nsAppFileLocationProvider()
{
}

//*****************************************************************************
// nsAppFileLocationProvider::nsISupports
//*****************************************************************************

NS_IMPL_THREADSAFE_ISUPPORTS1(nsAppFileLocationProvider, nsIDirectoryServiceProvider)

//*****************************************************************************
// nsAppFileLocationProvider::nsIDirectoryServiceProvider
//*****************************************************************************

NS_IMETHODIMP
nsAppFileLocationProvider::GetFile(const char *prop, PRBool *persistant, nsIFile **_retval)
{
    nsCOMPtr<nsILocalFile>  localFile;
    nsresult rv = NS_ERROR_FAILURE;

    *_retval = nsnull;
    *persistant = PR_TRUE;
    
    if (nsCRT::strcmp(prop, NS_APP_APPLICATION_REGISTRY_DIR) == 0)
    {
        rv = GetProductDirectory(getter_AddRefs(localFile));
    }
    else if (nsCRT::strcmp(prop, NS_APP_APPLICATION_REGISTRY_FILE) == 0)
    {
        rv = GetProductDirectory(getter_AddRefs(localFile));
        if (NS_SUCCEEDED(rv))
            rv = localFile->Append(APP_REGISTRY_NAME);
    }
    else if (nsCRT::strcmp(prop, NS_APP_DEFAULTS_50_DIR) == 0)
    {
        rv = CloneMozBinDirectory(getter_AddRefs(localFile));
        if (NS_SUCCEEDED(rv))
            rv = localFile->AppendRelativePath(DEFAULTS_DIR_NAME);
    }
    else if (nsCRT::strcmp(prop, NS_APP_PREF_DEFAULTS_50_DIR) == 0)
    {
        rv = CloneMozBinDirectory(getter_AddRefs(localFile));
        if (NS_SUCCEEDED(rv)) {
            rv = localFile->AppendRelativePath(DEFAULTS_DIR_NAME);
            if (NS_SUCCEEDED(rv))
                rv = localFile->AppendRelativePath(DEFAULTS_PREF_DIR_NAME);
        }
    }
    else if (nsCRT::strcmp(prop, NS_APP_PROFILE_DEFAULTS_50_DIR) == 0 ||
             nsCRT::strcmp(prop, NS_APP_PROFILE_DEFAULTS_NLOC_50_DIR) == 0)
    {
        rv = CloneMozBinDirectory(getter_AddRefs(localFile));
        if (NS_SUCCEEDED(rv)) {
            rv = localFile->AppendRelativePath(DEFAULTS_DIR_NAME);
            if (NS_SUCCEEDED(rv))
                rv = localFile->AppendRelativePath(DEFAULTS_PROFILE_DIR_NAME);
        }
    }
    else if (nsCRT::strcmp(prop, NS_APP_USER_PROFILES_ROOT_DIR) == 0)
    {
        rv = GetDefaultUserProfileRoot(getter_AddRefs(localFile));
    }
    else if (nsCRT::strcmp(prop, NS_APP_RES_DIR) == 0)
    {
        rv = CloneMozBinDirectory(getter_AddRefs(localFile));
        if (NS_SUCCEEDED(rv))
            rv = localFile->AppendRelativePath(RES_DIR_NAME);
    }
    else if (nsCRT::strcmp(prop, NS_APP_CHROME_DIR) == 0)
    {
        rv = CloneMozBinDirectory(getter_AddRefs(localFile));
        if (NS_SUCCEEDED(rv))
            rv = localFile->AppendRelativePath(CHROME_DIR_NAME);
    }
    else if (nsCRT::strcmp(prop, NS_APP_PLUGINS_DIR) == 0)
    {
        rv = CloneMozBinDirectory(getter_AddRefs(localFile));
        if (NS_SUCCEEDED(rv))
            rv = localFile->AppendRelativePath(PLUGINS_DIR_NAME);
    }
    else if (nsCRT::strcmp(prop, NS_APP_SEARCH_DIR) == 0)
    {
        rv = CloneMozBinDirectory(getter_AddRefs(localFile));
        if (NS_SUCCEEDED(rv))
            rv = localFile->AppendRelativePath(SEARCH_DIR_NAME);
    }
 

    if (localFile && NS_SUCCEEDED(rv))
        return localFile->QueryInterface(NS_GET_IID(nsIFile), (void**)_retval);
        
    return rv;
}


NS_METHOD nsAppFileLocationProvider::CloneMozBinDirectory(nsILocalFile **aLocalFile)
{
    NS_ENSURE_ARG_POINTER(aLocalFile);
    nsresult rv;

    if (!mMozBinDirectory)
    {
        // Get the mozilla bin directory
        // 1. Check the directory service first for NS_XPCOM_CURRENT_PROCESS_DIR
        //    This will be set if a directory was passed to NS_InitXPCOM
        // 2. If that doesn't work, set it to be the current process directory
        nsCOMPtr<nsIProperties>
          directoryService(do_GetService(NS_DIRECTORY_SERVICE_CONTRACTID, &rv));
        if (NS_FAILED(rv))
            return rv;

        rv = directoryService->Get(NS_XPCOM_CURRENT_PROCESS_DIR, NS_GET_IID(nsIFile), getter_AddRefs(mMozBinDirectory));
        if (NS_FAILED(rv)) {
            rv = directoryService->Get(NS_OS_CURRENT_PROCESS_DIR, NS_GET_IID(nsIFile), getter_AddRefs(mMozBinDirectory));
            if (NS_FAILED(rv))
                return rv;
        }
    }

    nsCOMPtr<nsIFile> aFile;
    rv = mMozBinDirectory->Clone(getter_AddRefs(aFile));
    if (NS_FAILED(rv))
        return rv;

    nsCOMPtr<nsILocalFile> lfile = do_QueryInterface (aFile);
    if (!lfile)
        return NS_ERROR_FAILURE;

    NS_IF_ADDREF(*aLocalFile = lfile);
    return NS_OK;
}


//----------------------------------------------------------------------------------------
// GetProductDirectory - Gets the directory which contains the application data folder
//
// UNIX   : ~/.mozilla/
// WIN    : <Application Data folder on user's machine>\Mozilla
// Mac    : :Documents:Mozilla:
//----------------------------------------------------------------------------------------
NS_METHOD nsAppFileLocationProvider::GetProductDirectory(nsILocalFile **aLocalFile)
{
    NS_ENSURE_ARG_POINTER(aLocalFile);

    nsresult rv;
    PRBool exists;
    nsCOMPtr<nsILocalFile> localDir;

#if defined(XP_MAC)
    nsCOMPtr<nsIProperties> directoryService = 
             do_GetService(NS_DIRECTORY_SERVICE_CONTRACTID, &rv);
    if (NS_FAILED(rv)) return rv;
    OSErr   err;
    long    response;
    err = ::Gestalt(gestaltSystemVersion, &response);
    const char *prop = (!err && response >= 0x00001000) ? NS_MAC_USER_LIB_DIR : NS_MAC_DOCUMENTS_DIR;
    rv = directoryService->Get(prop, NS_GET_IID(nsILocalFile), getter_AddRefs(localDir));
    if (NS_FAILED(rv)) return rv;   
#elif defined(XP_OS2)
    nsCOMPtr<nsIProperties> directoryService = 
             do_GetService(NS_DIRECTORY_SERVICE_CONTRACTID, &rv);
    if (NS_FAILED(rv)) return rv;
    rv = directoryService->Get(NS_OS2_HOME_DIR, NS_GET_IID(nsILocalFile), getter_AddRefs(localDir));
    if (NS_FAILED(rv)) return rv;
#elif defined(XP_PC)
    nsCOMPtr<nsIProperties> directoryService = 
             do_GetService(NS_DIRECTORY_SERVICE_CONTRACTID, &rv);
    if (NS_FAILED(rv)) return rv;
    rv = directoryService->Get(NS_WIN_APPDATA_DIR, NS_GET_IID(nsILocalFile), getter_AddRefs(localDir));
    if (NS_SUCCEEDED(rv))
        rv = localDir->Exists(&exists);
    if (NS_FAILED(rv) || !exists)
    {
        // On some Win95 machines, NS_WIN_APPDATA_DIR does not exist - revert to NS_WIN_WINDOWS_DIR
        localDir = nsnull;
        rv = directoryService->Get(NS_WIN_WINDOWS_DIR, NS_GET_IID(nsILocalFile), getter_AddRefs(localDir));
    }
    if (NS_FAILED(rv)) return rv;
#elif defined(XP_UNIX)
    rv = NS_NewLocalFile(PR_GetEnv("HOME"), PR_TRUE, getter_AddRefs(localDir));
    if (NS_FAILED(rv)) return rv;
#elif defined(XP_BEOS)
    char path[MAXPATHLEN];
    find_directory(B_USER_SETTINGS_DIRECTORY, 0, 0, path, MAXPATHLEN);
    // Need enough space to add the trailing backslash
    int len = strlen(path);
    if (len > MAXPATHLEN-2)
        return NS_ERROR_FAILURE;
    path[len]   = '/';
    path[len+1] = '\0';
    rv = NS_NewLocalFile(path, PR_TRUE, getter_AddRefs(localDir));
    if (NS_FAILED(rv)) return rv;
#else
#error dont_know_how_to_get_product_dir_on_your_platform
#endif

    rv = localDir->AppendRelativePath(DEFAULT_PRODUCT_DIR);
    if (NS_FAILED(rv)) return rv;
    rv = localDir->Exists(&exists);
    if (NS_SUCCEEDED(rv) && !exists)
        rv = localDir->Create(nsIFile::DIRECTORY_TYPE, 0775);
    if (NS_FAILED(rv)) return rv;

    *aLocalFile = localDir;
    NS_ADDREF(*aLocalFile);

   return rv;
}


//----------------------------------------------------------------------------------------
// GetDefaultUserProfileRoot - Gets the directory which contains each user profile dir
//
// UNIX   : ~/.mozilla/
// WIN    : <Application Data folder on user's machine>\Mozilla\Profiles
// Mac    : :Documents:Mozilla:Profiles:
//----------------------------------------------------------------------------------------
NS_METHOD nsAppFileLocationProvider::GetDefaultUserProfileRoot(nsILocalFile **aLocalFile)
{
    NS_ENSURE_ARG_POINTER(aLocalFile);

    nsresult rv;
    nsCOMPtr<nsILocalFile> localDir;

    rv = GetProductDirectory(getter_AddRefs(localDir));
    if (NS_FAILED(rv)) return rv;

#if defined(XP_MAC) || defined(XP_OS2) || defined(XP_PC)
    // These 3 platforms share this part of the path - do them as one
    rv = localDir->AppendRelativePath("Profiles");
    if (NS_FAILED(rv)) return rv;

    PRBool exists;
    rv = localDir->Exists(&exists);
    if (NS_SUCCEEDED(rv) && !exists)
        rv = localDir->Create(nsIFile::DIRECTORY_TYPE, 0775);
    if (NS_FAILED(rv)) return rv;
#endif

    *aLocalFile = localDir;
    NS_ADDREF(*aLocalFile);

   return rv;
}

