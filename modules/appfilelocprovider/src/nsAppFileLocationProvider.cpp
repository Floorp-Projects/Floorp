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
#include "nsIChromeRegistry.h"

static nsresult GetDefaultUserProfileRoot(nsILocalFile **aLocalFile);

#if defined(XP_MAC)
#include <Folders.h>
#include <Script.h>
#include <Processes.h>
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

static nsresult GetChromeLocale(PRUnichar** localeName);
 
// IDs

static NS_DEFINE_CID(kChromeRegistryCID,    NS_CHROMEREGISTRY_CID);
 
// WARNING: These hard coded names need to go away. They need to
// come from localizable resources

#if XP_MAC
#define DEFAULTS_DIR_NAME           "Defaults"
#define DEFAULTS_PREF_DIR_NAME      "Pref"
#define DEFAULTS_PROFILE_DIR_NAME   "Profile"
#define RES_DIR_NAME                "Res"
#define CHROME_DIR_NAME             "Chrome"
#define PLUGINS_DIR_NAME            "Plugins"
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
// nsAppFileLocationProvider::Static Variables
//*****************************************************************************   

PRInt32 nsAppFileLocationProvider::sInstanceCount = 0;
 
nsIAtom* nsAppFileLocationProvider::sApp_DefaultsFolder50        = nsnull;
nsIAtom* nsAppFileLocationProvider::sApp_PrefDefaultsFolder50    = nsnull;
nsIAtom* nsAppFileLocationProvider::sApp_ProfileDefaultsFolder50 = nsnull;
nsIAtom* nsAppFileLocationProvider::sApp_ProfileDefaultsNoLocFolder50 = nsnull;

nsIAtom* nsAppFileLocationProvider::sApp_DefaultUserProfileRoot50 = nsnull;

nsIAtom* nsAppFileLocationProvider::sApp_ResDirectory            = nsnull;
nsIAtom* nsAppFileLocationProvider::sApp_ChromeDirectory         = nsnull;
nsIAtom* nsAppFileLocationProvider::sApp_PluginsDirectory        = nsnull;

nsIAtom* nsAppFileLocationProvider::sApp_SearchDirectory50       = nsnull;

//*****************************************************************************
// nsAppFileLocationProvider::Constructor/Destructor
//*****************************************************************************   

nsAppFileLocationProvider::nsAppFileLocationProvider()
{
    NS_INIT_ISUPPORTS();
    
    if (sInstanceCount++ == 0) {
    
      // Defaults
        sApp_DefaultsFolder50        = NS_NewAtom(NS_APP_DEFAULTS_50_DIR);
        sApp_PrefDefaultsFolder50    = NS_NewAtom(NS_APP_PREF_DEFAULTS_50_DIR);
        sApp_ProfileDefaultsFolder50 = NS_NewAtom(NS_APP_PROFILE_DEFAULTS_50_DIR);
        sApp_ProfileDefaultsNoLocFolder50 = NS_NewAtom(NS_APP_PROFILE_DEFAULTS_NLOC_50_DIR);
 
      // Profile Root
        sApp_DefaultUserProfileRoot50 = NS_NewAtom(NS_APP_USER_PROFILES_ROOT_DIR);
            
      // Application Directories
        sApp_ResDirectory            = NS_NewAtom(NS_APP_RES_DIR);
        sApp_ChromeDirectory         = NS_NewAtom(NS_APP_CHROME_DIR);
        sApp_PluginsDirectory        = NS_NewAtom(NS_APP_PLUGINS_DIR);
      
      // Search
        sApp_SearchDirectory50       = NS_NewAtom(NS_APP_SEARCH_DIR);
    }
    
    nsresult rv;
    
    // Get the mozilla bin directory
    // 1. Check the directory service first for NS_XPCOM_CURRENT_PROCESS_DIR
    //    This will be set if a directory was passed to NS_InitXPCOM
    // 2. If that doesn't work, set it to be the current process directory
    
    NS_WITH_SERVICE(nsIProperties, directoryService, NS_DIRECTORY_SERVICE_CONTRACTID, &rv);
    if (NS_SUCCEEDED(rv))
        rv = directoryService->Get(NS_XPCOM_CURRENT_PROCESS_DIR, NS_GET_IID(nsIFile), getter_AddRefs(mMozBinDirectory));

    if (NS_FAILED(rv)) {
        rv = directoryService->Get(NS_OS_CURRENT_PROCESS_DIR, NS_GET_IID(nsIFile), getter_AddRefs(mMozBinDirectory));
    }
}

nsAppFileLocationProvider::~nsAppFileLocationProvider()
{
    if (--sInstanceCount == 0) {

        NS_IF_RELEASE(sApp_DefaultsFolder50);        
        NS_IF_RELEASE(sApp_PrefDefaultsFolder50);
        NS_IF_RELEASE(sApp_ProfileDefaultsFolder50);
        NS_IF_RELEASE(sApp_ProfileDefaultsNoLocFolder50);

        NS_IF_RELEASE(sApp_DefaultUserProfileRoot50);
              
        NS_IF_RELEASE(sApp_ResDirectory);
        NS_IF_RELEASE(sApp_ChromeDirectory);
        NS_IF_RELEASE(sApp_PluginsDirectory);

        NS_IF_RELEASE(sApp_SearchDirectory50);
    }
}


//*****************************************************************************
// nsAppFileLocationProvider::nsISupports
//*****************************************************************************   

NS_IMPL_ISUPPORTS1(nsAppFileLocationProvider, nsIDirectoryServiceProvider)


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
	
    nsIAtom* inAtom = NS_NewAtom(prop);
    NS_ENSURE_TRUE(inAtom, NS_ERROR_OUT_OF_MEMORY);

    if (inAtom == sApp_DefaultsFolder50)
    {
        rv = CloneMozBinDirectory(getter_AddRefs(localFile));
        if (NS_SUCCEEDED(rv))
            rv = localFile->AppendRelativePath(DEFAULTS_DIR_NAME);
    }
    else if (inAtom == sApp_PrefDefaultsFolder50)
    {
        rv = CloneMozBinDirectory(getter_AddRefs(localFile));
        if (NS_SUCCEEDED(rv)) {
            rv = localFile->AppendRelativePath(DEFAULTS_DIR_NAME);
            if (NS_SUCCEEDED(rv))
                rv = localFile->AppendRelativePath(DEFAULTS_PREF_DIR_NAME);
        }
    }
    else if (inAtom == sApp_ProfileDefaultsFolder50)
    {
        rv = CloneMozBinDirectory(getter_AddRefs(localFile));
        if (NS_SUCCEEDED(rv)) {
            rv = localFile->AppendRelativePath(DEFAULTS_DIR_NAME);
            if (NS_SUCCEEDED(rv)) {
                rv = localFile->AppendRelativePath(DEFAULTS_PROFILE_DIR_NAME);
                if (NS_SUCCEEDED(rv)) {
                    nsXPIDLString localeName;
                    rv = GetChromeLocale(getter_Copies(localeName));
                    if (NS_SUCCEEDED(rv))
                        rv = localFile->AppendRelativeUnicodePath(localeName);
                }
            }
        }
    }
    else if (inAtom == sApp_ProfileDefaultsNoLocFolder50)
    {
        rv = CloneMozBinDirectory(getter_AddRefs(localFile));
        if (NS_SUCCEEDED(rv)) {
            rv = localFile->AppendRelativePath(DEFAULTS_DIR_NAME);
            if (NS_SUCCEEDED(rv))
                rv = localFile->AppendRelativePath(DEFAULTS_PROFILE_DIR_NAME);
        }
    }
    else if (inAtom == sApp_DefaultUserProfileRoot50)
    {
        rv = GetDefaultUserProfileRoot(getter_AddRefs(localFile));   
    }
    else if (inAtom == sApp_ResDirectory)
    {
        rv = CloneMozBinDirectory(getter_AddRefs(localFile));
        if (NS_SUCCEEDED(rv))
            rv = localFile->AppendRelativePath(RES_DIR_NAME);
    }
    else if (inAtom == sApp_ChromeDirectory)
    {
        rv = CloneMozBinDirectory(getter_AddRefs(localFile));
        if (NS_SUCCEEDED(rv))
            rv = localFile->AppendRelativePath(CHROME_DIR_NAME);
    }
    else if (inAtom == sApp_PluginsDirectory)
    {
        rv = CloneMozBinDirectory(getter_AddRefs(localFile));
        if (NS_SUCCEEDED(rv))
            rv = localFile->AppendRelativePath(PLUGINS_DIR_NAME);
    }
    else if (inAtom == sApp_SearchDirectory50)
    {
        rv = CloneMozBinDirectory(getter_AddRefs(localFile));
        if (NS_SUCCEEDED(rv))
            rv = localFile->AppendRelativePath(SEARCH_DIR_NAME);
    }
    
    NS_RELEASE(inAtom);

	if (localFile && NS_SUCCEEDED(rv))
		return localFile->QueryInterface(NS_GET_IID(nsIFile), (void**)_retval);
		
	return rv;
}


NS_METHOD nsAppFileLocationProvider::CloneMozBinDirectory(nsILocalFile **aLocalFile)
{
    NS_ENSURE_TRUE(mMozBinDirectory, NS_ERROR_FAILURE);
    NS_ENSURE_ARG_POINTER(aLocalFile);
    
    nsCOMPtr<nsIFile> aFile;
    nsresult rv = mMozBinDirectory->Clone(getter_AddRefs(aFile));
    if (NS_FAILED(rv))
        return rv;

    nsCOMPtr<nsILocalFile> lfile = do_QueryInterface (aFile);
    if (!lfile)
        return NS_ERROR_FAILURE;
    
    NS_IF_ADDREF(*aLocalFile = lfile);
    return NS_OK;
}

//****************************************************************************************
// Static Routines
//****************************************************************************************

static nsresult GetChromeLocale(PRUnichar** localeName)
{
    NS_ENSURE_ARG_POINTER(localeName);

    nsresult rv;    
    *localeName = nsnull;
    nsCOMPtr<nsIChromeRegistry> chromeRegistry = do_GetService(kChromeRegistryCID, &rv);

    if (NS_SUCCEEDED(rv)) {
        nsString tmpstr; tmpstr.AssignWithConversion("navigator");
        rv = chromeRegistry->GetSelectedLocale(tmpstr.GetUnicode(), localeName);
    }
    return rv;
}


//----------------------------------------------------------------------------------------
// GetDefaultUserProfileRoot - Gets the directory which contains each user profile dir
//
// UNIX   : ~/.mozilla/
// WIN    : <Application Data folder on user's machine>\Mozilla\Users50 
// Mac    : :Documents:Mozilla:Users50:
//----------------------------------------------------------------------------------------
static nsresult GetDefaultUserProfileRoot(nsILocalFile **aLocalFile)
{
    NS_ENSURE_ARG_POINTER(aLocalFile);
    
    nsresult rv;
    PRBool exists;
    nsCOMPtr<nsILocalFile> localDir;
   
#if defined(XP_MAC)
    NS_WITH_SERVICE(nsIProperties, directoryService, NS_DIRECTORY_SERVICE_CONTRACTID, &rv);
    if (NS_FAILED(rv)) return rv;
    rv = directoryService->Get(NS_MAC_DOCUMENTS_DIR, NS_GET_IID(nsILocalFile), getter_AddRefs(localDir));
    if (NS_FAILED(rv)) return rv;   
#elif defined(XP_OS2)
    NS_WITH_SERVICE(nsIProperties, directoryService, NS_DIRECTORY_SERVICE_CONTRACTID, &rv);
    if (NS_FAILED(rv)) return rv;
    rv = directoryService->Get(NS_OS2_HOME_DIR, NS_GET_IID(nsILocalFile), getter_AddRefs(localDir));
    if (NS_FAILED(rv)) return rv;
#elif defined(XP_PC)
    NS_WITH_SERVICE(nsIProperties, directoryService, NS_DIRECTORY_SERVICE_CONTRACTID, &rv);
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
    rv = localDir->AppendRelativePath(".mozilla");
    if (NS_FAILED(rv)) return rv;
    rv = localDir->Exists(&exists);
    if (NS_SUCCEEDED(rv) && !exists)
      rv = localDir->Create(nsIFile::DIRECTORY_TYPE, 0775);
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
    rv = localDir->AppendRelativePath("mozilla");
    if (NS_FAILED(rv)) return rv;
    rv = localDir->Exists(&exists);
    if (NS_SUCCEEDED(rv) && !exists)
      rv = localDir->Create(nsIFile::DIRECTORY_TYPE, 0);
    if (NS_FAILED(rv)) return rv;
#else
#error dont_know_how_to_do_profiles_on_your_platform
#endif

#if defined(XP_MAC) || defined(XP_OS2) || defined(XP_PC)
    // These 3 platforms share this part of the path - do them as one
    rv = localDir->AppendRelativePath("Mozilla");
    if (NS_FAILED(rv)) return rv;
    rv = localDir->AppendRelativePath("Users50");
    if (NS_FAILED(rv)) return rv;
    rv = localDir->Exists(&exists);
    if (NS_SUCCEEDED(rv) && !exists)
        rv = localDir->Create(nsIFile::DIRECTORY_TYPE, 0775);
    if (NS_FAILED(rv)) return rv;
#endif

    *aLocalFile = localDir;
    NS_ADDREF(*aLocalFile);

   return rv; 
}

