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
static nsresult GetMacFolder(OSType folderType, nsILocalFile** aFile);
#endif
#if defined(XP_OS2)
#define INCL_DOSPROCESS
#define INCL_DOSMODULEMGR
#include <os2.h>
#elif defined(XP_PC)
#include <windows.h>
#include <shlobj.h>
static nsresult GetWindowsFolder(int folder, nsILocalFile** aFile);
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
static nsresult GetCurrentProcessDirectory(nsILocalFile** aFile);
 
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
    // 1. Check the directory service first for xpcom.currentProcessDirectory
    //    This will be set if a directory was passed to NS_InitXPCOM
    // 2. If that doesn't work, set it to be the current process directory
    
    NS_WITH_SERVICE(nsIProperties, directoryService, NS_DIRECTORY_SERVICE_PROGID, &rv);
    if (NS_SUCCEEDED(rv))
        rv = directoryService->Get(NS_XPCOM_CURRENT_PROCESS_DIR, NS_GET_IID(nsIFile), getter_AddRefs(mMozBinDirectory));

    if (NS_FAILED(rv)) {
        nsCOMPtr<nsILocalFile> aLocalFile;
        rv = GetCurrentProcessDirectory(getter_AddRefs(aLocalFile));
        if (NS_SUCCEEDED(rv))
            mMozBinDirectory = do_QueryInterface(aLocalFile);
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

static nsresult GetCurrentProcessDirectory(nsILocalFile** aFile)
{
    nsCOMPtr<nsILocalFile> localFile;
    NS_NewLocalFile(nsnull, PR_FALSE, getter_AddRefs(localFile));
    if (!localFile)
        return NS_ERROR_OUT_OF_MEMORY;
   

#ifdef XP_PC
#ifdef XP_OS2
    PPIB ppib;
    PTIB ptib;
    char buffer[CCHMAXPATH];
    DosGetInfoBlocks( &ptib, &ppib);
    DosQueryModuleName( ppib->pib_hmte, CCHMAXPATH, buffer);
    *strrchr( buffer, '\\') = '\0'; // XXX DBCS misery
    localFile->InitWithPath(buffer);
    NS_IF_ADDREF(*aFile = localFile);
    return NS_OK;
#else
    char buf[MAX_PATH];
    if ( GetModuleFileName(0, buf, sizeof(buf)) ) 
    {
        // chop of the executable name by finding the rightmost backslash
        char* lastSlash = PL_strrchr(buf, '\\');
        if (lastSlash)
            *(lastSlash + 1) = '\0';
        
        localFile->InitWithPath(buf);
        NS_IF_ADDREF(*aFile = localFile);
        return NS_OK;
    }
#endif

#elif defined(XP_MAC)
    // get info for the the current process to determine the directory
    // its located in
    OSErr err;
    ProcessSerialNumber psn;
    if (!(err = GetCurrentProcess(&psn)))
    {
        ProcessInfoRec pInfo;
        FSSpec         tempSpec;

        // initialize ProcessInfoRec before calling
        // GetProcessInformation() or die horribly.
        pInfo.processName = nil;
        pInfo.processAppSpec = &tempSpec;
        pInfo.processInfoLength = sizeof(ProcessInfoRec);

        if (!(err = GetProcessInformation(&psn, &pInfo)))
        {
            FSSpec appFSSpec = *(pInfo.processAppSpec);
            
            // Truncate the nsame so the spec is just to the app directory
            appFSSpec.name[0] = 0;

        	nsCOMPtr<nsILocalFileMac> localFileMac = do_QueryInterface((nsIFile*)localFile);
		    if (localFileMac) 
            {
                localFileMac->InitWithFSSpec(&appFSSpec);
                NS_IF_ADDREF(*aFile = localFile);
                return NS_OK;
            }
        }
    }

#elif defined(XP_UNIX)

    // In the absence of a good way to get the executable directory let
    // us try this for unix:
    //	- if MOZILLA_FIVE_HOME is defined, that is it
    //	- else give the current directory
    char buf[MAXPATHLEN];
    char *moz5 = PR_GetEnv("MOZILLA_FIVE_HOME");
    if (moz5)
    {
        localFile->InitWithPath(moz5);
        localFile->Normalize();
        NS_IF_ADDREF(*aFile = localFile);
        return NS_OK;
    }
    else
    {
        static PRBool firstWarning = PR_TRUE;

        if(firstWarning) {
            // Warn that MOZILLA_FIVE_HOME not set, once.
            printf("Warning: MOZILLA_FIVE_HOME not set.\n");
            firstWarning = PR_FALSE;
        }

        // Fall back to current directory.
        if (getcwd(buf, sizeof(buf)))
        {
            localFile->InitWithPath(buf);
            NS_IF_ADDREF(*aFile = localFile);
            return NS_OK;
        }
    }

#elif defined(XP_BEOS)

    char *moz5 = getenv("MOZILLA_FIVE_HOME");
    if (moz5)
    {
        localFile->InitWithPath(moz5);
        NS_IF_ADDREF(*aFile = localFile);
        return NS_OK;
    }
    else
    {
      static char buf[MAXPATHLEN];
      int32 cookie = 0;
      image_info info;
      char *p;
      *buf = 0;
      if(get_next_image_info(0, &cookie, &info) == B_OK)
      {
        strcpy(buf, info.name);
        if((p = strrchr(buf, '/')) != 0)
        {
          *p = 0;
          localFile->InitWithPath(buf);
          NS_IF_ADDREF(*aFile = localFile);
          return NS_OK;
        }
      }
    }

#endif
    
    NS_ERROR("unable to get current process directory");
    return NS_ERROR_FAILURE;
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
   nsresult rv;
   PRBool exists;
   nsILocalFile *pLocalFile;
   
#if defined(XP_MAC)
    rv = GetMacFolder(kDocumentsFolderType, aLocalFile);
    NS_ENSURE_SUCCESS(rv, rv);
    pLocalFile = *aLocalFile;
    pLocalFile->AppendRelativePath("Mozilla");
    rv = pLocalFile->Exists(&exists);
    NS_ENSURE_SUCCESS(rv, rv);
    if (!exists) {
      rv = pLocalFile->Create(nsIFile::DIRECTORY_TYPE, 0);
      NS_ENSURE_SUCCESS(rv, rv);
    }
    pLocalFile->AppendRelativePath("Users50");
    rv = pLocalFile->Exists(&exists);
    NS_ENSURE_SUCCESS(rv, rv);
    if (!exists) {
      rv = pLocalFile->Create(nsIFile::DIRECTORY_TYPE, 0);
      NS_ENSURE_SUCCESS(rv, rv);
    }    
#elif defined(XP_OS2)
    PPIB ppib;
    PTIB ptib;
    char buffer[CCHMAXPATH];
    DosGetInfoBlocks( &ptib, &ppib);
    DosQueryModuleName( ppib->pib_hmte, CCHMAXPATH, buffer);
    *strrchr( buffer, '\\') = '\0'; // OS2TODO DBCS misery
    *strrchr( buffer, '\\') = '\0'; // OS2TODO DBCS misery
    rv = NS_NewLocalFile(buffer, PR_TRUE, aLocalFile);
    NS_ENSURE_SUCCESS(rv, rv);
    pLocalFile = *aLocalFile;
    pLocalFile->AppendRelativePath("Users50");
    rv = pLocalFile->Exists(&exists);
    NS_ENSURE_SUCCESS(rv, rv);
    if (!exists) {
      rv = pLocalFile->Create(nsIFile::DIRECTORY_TYPE, 0);
      NS_ENSURE_SUCCESS(rv, rv);
    }
#elif defined(XP_PC)
    rv = GetWindowsFolder(CSIDL_APPDATA, aLocalFile);
    if (NS_SUCCEEDED(rv)) {
        rv = (*aLocalFile)->Exists(&exists);
        if (NS_SUCCEEDED(rv) && !exists) {            
            char path[_MAX_PATH];
            PRInt32 len = GetWindowsDirectory( path, _MAX_PATH );
            
            // Need enough space to add the trailing backslash
            if (len <= _MAX_PATH-2) {
                path[len]   = '\\';
                path[len+1] = '\0';
            }
            rv = NS_NewLocalFile(path, PR_TRUE, aLocalFile);
        }
    }
    NS_ENSURE_SUCCESS(rv, rv);
    pLocalFile = *aLocalFile;
    pLocalFile->AppendRelativePath("Mozilla");
    rv = pLocalFile->Exists(&exists);
    NS_ENSURE_SUCCESS(rv, rv);
    if (!exists) {
      rv = pLocalFile->Create(nsIFile::DIRECTORY_TYPE, 0);
      NS_ENSURE_SUCCESS(rv, rv);
    }
    pLocalFile->AppendRelativePath("Users50");
    rv = pLocalFile->Exists(&exists);
    NS_ENSURE_SUCCESS(rv, rv);
    if (!exists) {
      rv = pLocalFile->Create(nsIFile::DIRECTORY_TYPE, 0);
      NS_ENSURE_SUCCESS(rv, rv);
    }
#elif defined(XP_UNIX)
    rv = NS_NewLocalFile(PR_GetEnv("HOME"), PR_TRUE, aLocalFile);
    NS_ENSURE_SUCCESS(rv, rv);
    pLocalFile = *aLocalFile;
    pLocalFile->AppendRelativePath(".mozilla");
    rv = pLocalFile->Exists(&exists);
    NS_ENSURE_SUCCESS(rv, rv);
    if (!exists) {
      rv = pLocalFile->Create(nsIFile::DIRECTORY_TYPE, 0775);
      NS_ENSURE_SUCCESS(rv, rv);
    }
#elif defined(XP_BEOS)
    char path[MAXPATHLEN];
    find_directory(B_USER_SETTINGS_DIRECTORY, 0, 0, path, MAXPATHLEN);
    // Need enough space to add the trailing backslash
    int len = strlen(path);
    if (len > MAXPATHLEN-2)
        return NS_ERROR_FAILURE;
    path[len]   = '/';
    path[len+1] = '\0';
    rv = NS_NewLocalFile(path, PR_TRUE, aLocalFile);
    NS_ENSURE_SUCCESS(rv, rv);
    pLocalFile = *aLocalFile;
    pLocalFile->AppendRelativePath("mozilla");
    rv = pLocalFile->Exists(&exists);
    NS_ENSURE_SUCCESS(rv, rv);
    if (!exists) {
      rv = pLocalFile->Create(nsIFile::DIRECTORY_TYPE, 0);
      NS_ENSURE_SUCCESS(rv, rv);
    }
#else
#error dont_know_how_to_do_profiles_on_your_platform
#endif

   return rv; 
}

#if defined (XP_MAC)
//----------------------------------------------------------------------------------------
// GetMacFolder - Gets a folder which is defined by the OS
//----------------------------------------------------------------------------------------
static nsresult GetMacFolder(OSType folderType, nsILocalFile** aFile)
{
    OSErr   err;
    CInfoPBRec cinfo;
    DirInfo *dipb=(DirInfo *)&cinfo;
    FSSpec  tempSpec;
    
    // Call FindFolder to fill in the vrefnum and dirid
    for (int attempts = 0; attempts < 2; attempts++)
    {
        err = FindFolder(kOnSystemDisk, folderType, kCreateFolder,
                         &dipb->ioVRefNum, &dipb->ioDrDirID);
        if (err == noErr)
            break;
        if (attempts > 0)
		    break;
		
		// This business with the Documents folder is probably wrong. If it
		// didn't exist and we are running a system on which it is not called
		// "Documents", the second pass through FindFolder is still not
		// going to find it. In this case, we are probably better off having
		// a Documents folder with the wrong name than not having one at all.
		      
		switch (folderType)
		{		    
    	    case kDocumentsFolderType:
    	        {
    	        const char* kDocumentsFolderName = "Documents";
    	        // Find folder will find this, as long as it exists.
    	        // The "create" parameter, however, is sadly ignored.
    	        // How do we internationalize this?    	        
    	        err = FindFolder(kOnSystemDisk, kVolumeRootFolderType, kCreateFolder,
                                 &tempSpec.vRefNum, &tempSpec.parID);
                
                PRUint32 len = nsCRT::strlen(kDocumentsFolderName);
                nsCRT::memcpy(&tempSpec.name[1], kDocumentsFolderName, len);
                tempSpec.name[0] = len;                 
                err = FSpDirCreate(&tempSpec, smSystemScript, &dipb->ioDrDirID);
                }
    	        break;
		}
    }
    NS_ENSURE_TRUE(err == noErr, NS_ERROR_FILE_NOT_FOUND);

    StrFileName filename;
    filename[0] = '\0';
    dipb->ioNamePtr = (StringPtr)&filename;
    dipb->ioFDirIndex = -1;
    
    err = PBGetCatInfoSync(&cinfo);
    NS_ENSURE_TRUE(err == noErr, NS_ERROR_FILE_NOT_FOUND);
    err = FSMakeFSSpec(dipb->ioVRefNum, dipb->ioDrParID, filename, &tempSpec);
    NS_ENSURE_TRUE(err == noErr, NS_ERROR_FILE_NOT_FOUND);

	 nsCOMPtr<nsILocalFileMac> macDir;
    nsresult rv = NS_NewLocalFileWithFSSpec(&tempSpec, PR_TRUE, getter_AddRefs(macDir));
    NS_ENSURE_SUCCESS(rv, rv);
    rv = macDir->QueryInterface(NS_GET_IID(nsILocalFile), aFile);

    return rv;
}
#endif

#if defined (XP_PC) && !defined (XP_OS2)
//----------------------------------------------------------------------------------------
// MakeUpperCase - Windows does not care about case.  push to uppercase:
//----------------------------------------------------------------------------------------
static char* MakeUpperCase(char* aPath)
{
  int length = strlen(aPath);
  for (int i = 0; i < length; i++)
      if (islower(aPath[i]))
        aPath[i] = _toupper(aPath[i]);
    
  return aPath;
}

//----------------------------------------------------------------------------------------
// GetWindowsFolder - Gets a folder which is defined by the OS
//----------------------------------------------------------------------------------------
static nsresult GetWindowsFolder(int folder, nsILocalFile** aFile)
{
    nsresult rv = NS_ERROR_FAILURE;
    LPMALLOC pMalloc = NULL;
    LPSTR pBuffer = NULL;
    LPITEMIDLIST pItemIDList = NULL;
    int len;
    char *outDirectory = NULL;
    nsCOMPtr<nsILocalFile> newFile;
 
    // Get the shell's allocator. 
    if (!SUCCEEDED(SHGetMalloc(&pMalloc))) 
        return NS_ERROR_FAILURE;

    // Allocate a buffer
    if ((pBuffer = (LPSTR) pMalloc->Alloc(MAX_PATH + 2)) == NULL) 
        return NS_ERROR_OUT_OF_MEMORY; 
 
    // Get the PIDL for the folder. 
    if (!SUCCEEDED(SHGetSpecialFolderLocation(NULL, folder, &pItemIDList)))
        goto Clean;
 
    if (!SUCCEEDED(SHGetPathFromIDList(pItemIDList, pBuffer)))
        goto Clean;

    // Append the trailing slash
    len = PL_strlen(pBuffer);
    pBuffer[len]   = '\\';
    pBuffer[len + 1] = '\0';

    // Assign the directory
    outDirectory = MakeUpperCase(pBuffer);
    rv = NS_NewLocalFile(outDirectory, TRUE, getter_AddRefs(newFile));
    if (NS_FAILED(rv))
        goto Clean;
    *aFile = newFile;
    NS_ADDREF(*aFile);
    
    rv = NS_OK;
    
Clean:
    // Clean up. 
    if (pItemIDList)
        pMalloc->Free(pItemIDList); 
    if (pBuffer)
        pMalloc->Free(pBuffer); 

	 pMalloc->Release();
	 
	 return rv;
}
#endif // XP_PC && !XP_OS2

