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
 * The Original Code is Mozilla Communicator client code, released
 * March 31, 1998.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 2000
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Conrad Carlen <conrad@ingress.com>
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

#include "nsAppFileLocationProvider.h"
#include "nsAppDirectoryServiceDefs.h"
#include "nsDirectoryServiceDefs.h"
#include "nsIAtom.h"
#include "nsILocalFile.h"
#include "nsString.h"
#include "nsXPIDLString.h"
#include "nsISimpleEnumerator.h"
#include "prenv.h"
#include "nsCRT.h"

#if defined(MOZ_WIDGET_COCOA)
#include <Carbon/Carbon.h>
#include "nsILocalFileMac.h"
#elif defined(XP_OS2)
#define INCL_DOSPROCESS
#define INCL_DOSMODULEMGR
#include <os2.h>
#elif defined(XP_WIN)
#include <windows.h>
#include <shlobj.h>
#elif defined(XP_UNIX)
#include <unistd.h>
#include <stdlib.h>
#include <sys/param.h>
#endif


// WARNING: These hard coded names need to go away. They need to
// come from localizable resources

#if defined(MOZ_WIDGET_COCOA)
#define APP_REGISTRY_NAME NS_LITERAL_CSTRING("Application Registry")
#define ESSENTIAL_FILES   NS_LITERAL_CSTRING("Essential Files")
#elif defined(XP_WIN) || defined(XP_OS2)
#define APP_REGISTRY_NAME NS_LITERAL_CSTRING("registry.dat")
#else
#define APP_REGISTRY_NAME NS_LITERAL_CSTRING("appreg")
#endif

// define default product directory
#define DEFAULT_PRODUCT_DIR NS_LITERAL_CSTRING(MOZ_USER_DIR)

// Locally defined keys used by nsAppDirectoryEnumerator
#define NS_ENV_PLUGINS_DIR          "EnvPlugins"    // env var MOZ_PLUGIN_PATH
#define NS_USER_PLUGINS_DIR         "UserPlugins"

#ifdef MOZ_WIDGET_COCOA
#define NS_MACOSX_USER_PLUGIN_DIR   "OSXUserPlugins"
#define NS_MACOSX_LOCAL_PLUGIN_DIR  "OSXLocalPlugins"
#define NS_MACOSX_JAVA2_PLUGIN_DIR  "OSXJavaPlugins"
#elif XP_UNIX
#define NS_SYSTEM_PLUGINS_DIR       "SysPlugins"
#endif

#define DEFAULTS_DIR_NAME           NS_LITERAL_CSTRING("defaults")
#define DEFAULTS_PREF_DIR_NAME      NS_LITERAL_CSTRING("pref")
#define DEFAULTS_PROFILE_DIR_NAME   NS_LITERAL_CSTRING("profile")
#define RES_DIR_NAME                NS_LITERAL_CSTRING("res")
#define CHROME_DIR_NAME             NS_LITERAL_CSTRING("chrome")
#define PLUGINS_DIR_NAME            NS_LITERAL_CSTRING("plugins")
#define SEARCH_DIR_NAME             NS_LITERAL_CSTRING("searchplugins")

//*****************************************************************************
// nsAppFileLocationProvider::Constructor/Destructor
//*****************************************************************************

nsAppFileLocationProvider::nsAppFileLocationProvider()
{
}

//*****************************************************************************
// nsAppFileLocationProvider::nsISupports
//*****************************************************************************

NS_IMPL_THREADSAFE_ISUPPORTS2(nsAppFileLocationProvider, nsIDirectoryServiceProvider, nsIDirectoryServiceProvider2)

//*****************************************************************************
// nsAppFileLocationProvider::nsIDirectoryServiceProvider
//*****************************************************************************

NS_IMETHODIMP
nsAppFileLocationProvider::GetFile(const char *prop, PRBool *persistent, nsIFile **_retval)
{
    nsCOMPtr<nsILocalFile>  localFile;
    nsresult rv = NS_ERROR_FAILURE;

    NS_ENSURE_ARG(prop);
    *_retval = nsnull;
    *persistent = PR_TRUE;

#ifdef MOZ_WIDGET_COCOA
    FSRef fileRef;
    nsCOMPtr<nsILocalFileMac> macFile;
#endif
    
    if (nsCRT::strcmp(prop, NS_APP_APPLICATION_REGISTRY_DIR) == 0)
    {
        rv = GetProductDirectory(getter_AddRefs(localFile));
    }
    else if (nsCRT::strcmp(prop, NS_APP_APPLICATION_REGISTRY_FILE) == 0)
    {
        rv = GetProductDirectory(getter_AddRefs(localFile));
        if (NS_SUCCEEDED(rv))
            rv = localFile->AppendNative(APP_REGISTRY_NAME);
    }
    else if (nsCRT::strcmp(prop, NS_APP_DEFAULTS_50_DIR) == 0)
    {
        rv = CloneMozBinDirectory(getter_AddRefs(localFile));
        if (NS_SUCCEEDED(rv))
            rv = localFile->AppendRelativeNativePath(DEFAULTS_DIR_NAME);
    }
    else if (nsCRT::strcmp(prop, NS_APP_PREF_DEFAULTS_50_DIR) == 0)
    {
        rv = CloneMozBinDirectory(getter_AddRefs(localFile));
        if (NS_SUCCEEDED(rv)) {
            rv = localFile->AppendRelativeNativePath(DEFAULTS_DIR_NAME);
            if (NS_SUCCEEDED(rv))
                rv = localFile->AppendRelativeNativePath(DEFAULTS_PREF_DIR_NAME);
        }
    }
    else if (nsCRT::strcmp(prop, NS_APP_PROFILE_DEFAULTS_50_DIR) == 0 ||
             nsCRT::strcmp(prop, NS_APP_PROFILE_DEFAULTS_NLOC_50_DIR) == 0)
    {
        rv = CloneMozBinDirectory(getter_AddRefs(localFile));
        if (NS_SUCCEEDED(rv)) {
            rv = localFile->AppendRelativeNativePath(DEFAULTS_DIR_NAME);
            if (NS_SUCCEEDED(rv))
                rv = localFile->AppendRelativeNativePath(DEFAULTS_PROFILE_DIR_NAME);
        }
    }
    else if (nsCRT::strcmp(prop, NS_APP_USER_PROFILES_ROOT_DIR) == 0)
    {
        rv = GetDefaultUserProfileRoot(getter_AddRefs(localFile));
    }
    else if (nsCRT::strcmp(prop, NS_APP_USER_PROFILES_LOCAL_ROOT_DIR) == 0)
    {
        rv = GetDefaultUserProfileRoot(getter_AddRefs(localFile), PR_TRUE);
    }
    else if (nsCRT::strcmp(prop, NS_APP_RES_DIR) == 0)
    {
        rv = CloneMozBinDirectory(getter_AddRefs(localFile));
        if (NS_SUCCEEDED(rv))
            rv = localFile->AppendRelativeNativePath(RES_DIR_NAME);
    }
    else if (nsCRT::strcmp(prop, NS_APP_CHROME_DIR) == 0)
    {
        rv = CloneMozBinDirectory(getter_AddRefs(localFile));
        if (NS_SUCCEEDED(rv))
            rv = localFile->AppendRelativeNativePath(CHROME_DIR_NAME);
    }
    else if (nsCRT::strcmp(prop, NS_APP_PLUGINS_DIR) == 0)
    {
        rv = CloneMozBinDirectory(getter_AddRefs(localFile));
        if (NS_SUCCEEDED(rv))
            rv = localFile->AppendRelativeNativePath(PLUGINS_DIR_NAME);
    }
#ifdef MOZ_WIDGET_COCOA
    else if (nsCRT::strcmp(prop, NS_MACOSX_USER_PLUGIN_DIR) == 0)
    {
        if (::FSFindFolder(kUserDomain, kInternetPlugInFolderType, false, &fileRef) == noErr) {
            rv = NS_NewLocalFileWithFSRef(&fileRef, PR_TRUE, getter_AddRefs(macFile));
            if (NS_SUCCEEDED(rv))
                localFile = macFile;
        }
    }
    else if (nsCRT::strcmp(prop, NS_MACOSX_LOCAL_PLUGIN_DIR) == 0)
    {
        if (::FSFindFolder(kLocalDomain, kInternetPlugInFolderType, false, &fileRef) == noErr) {
            rv = NS_NewLocalFileWithFSRef(&fileRef, PR_TRUE, getter_AddRefs(macFile));
            if (NS_SUCCEEDED(rv))
                localFile = macFile;
        }
    }
    else if (nsCRT::strcmp(prop, NS_MACOSX_JAVA2_PLUGIN_DIR) == 0)
    {
      static const char *const java2PluginDirPath =
        "/System/Library/Java/Support/Deploy.bundle/Contents/Resources/";
      rv = NS_NewNativeLocalFile(nsDependentCString(java2PluginDirPath), PR_TRUE, getter_AddRefs(localFile));
    }
#else
    else if (nsCRT::strcmp(prop, NS_ENV_PLUGINS_DIR) == 0)
    {
        NS_ERROR("Don't use nsAppFileLocationProvider::GetFile(NS_ENV_PLUGINS_DIR, ...). "
                 "Use nsAppFileLocationProvider::GetFiles(...).");
        const char *pathVar = PR_GetEnv("MOZ_PLUGIN_PATH");
        if (pathVar && *pathVar)
            rv = NS_NewNativeLocalFile(nsDependentCString(pathVar), PR_TRUE, getter_AddRefs(localFile));
    }
    else if (nsCRT::strcmp(prop, NS_USER_PLUGINS_DIR) == 0)
    {
        rv = GetProductDirectory(getter_AddRefs(localFile));
        if (NS_SUCCEEDED(rv))
            rv = localFile->AppendRelativeNativePath(PLUGINS_DIR_NAME);
    }
#ifdef XP_UNIX
    else if (nsCRT::strcmp(prop, NS_SYSTEM_PLUGINS_DIR) == 0) {
        static const char *const sysLPlgDir = 
#if defined(HAVE_USR_LIB64_DIR) && defined(__LP64__)
          "/usr/lib64/mozilla/plugins";
#else
          "/usr/lib/mozilla/plugins";
#endif
        rv = NS_NewNativeLocalFile(nsDependentCString(sysLPlgDir),
                                   PR_FALSE, getter_AddRefs(localFile));
    }
#endif
#endif
    else if (nsCRT::strcmp(prop, NS_APP_SEARCH_DIR) == 0)
    {
        rv = CloneMozBinDirectory(getter_AddRefs(localFile));
        if (NS_SUCCEEDED(rv))
            rv = localFile->AppendRelativeNativePath(SEARCH_DIR_NAME);
    }
    else if (nsCRT::strcmp(prop, NS_APP_USER_SEARCH_DIR) == 0)
    {
        rv = NS_GetSpecialDirectory(NS_APP_USER_PROFILE_50_DIR, _retval);
        if (NS_SUCCEEDED(rv))
            rv = (*_retval)->AppendNative(SEARCH_DIR_NAME);
    }
    else if (nsCRT::strcmp(prop, NS_APP_INSTALL_CLEANUP_DIR) == 0)
    {   
        // This is cloned so that embeddors will have a hook to override
        // with their own cleanup dir.  See bugzilla bug #105087 
        rv = CloneMozBinDirectory(getter_AddRefs(localFile));
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
NS_METHOD nsAppFileLocationProvider::GetProductDirectory(nsILocalFile **aLocalFile, PRBool aLocal)
{
    NS_ENSURE_ARG_POINTER(aLocalFile);

    nsresult rv;
    PRBool exists;
    nsCOMPtr<nsILocalFile> localDir;

#if defined(MOZ_WIDGET_COCOA)
    FSRef fsRef;
    OSType folderType = aLocal ? (OSType) kCachedDataFolderType : (OSType) kDomainLibraryFolderType;
    OSErr err = ::FSFindFolder(kUserDomain, folderType, kCreateFolder, &fsRef);
    if (err) return NS_ERROR_FAILURE;
    NS_NewLocalFile(EmptyString(), PR_TRUE, getter_AddRefs(localDir));
    if (!localDir) return NS_ERROR_FAILURE;
    nsCOMPtr<nsILocalFileMac> localDirMac(do_QueryInterface(localDir));
    rv = localDirMac->InitWithFSRef(&fsRef);
    if (NS_FAILED(rv)) return rv;
#elif defined(XP_OS2)
    nsCOMPtr<nsIProperties> directoryService = 
             do_GetService(NS_DIRECTORY_SERVICE_CONTRACTID, &rv);
    if (NS_FAILED(rv)) return rv;
    rv = directoryService->Get(NS_OS2_HOME_DIR, NS_GET_IID(nsILocalFile), getter_AddRefs(localDir));
    if (NS_FAILED(rv)) return rv;
#elif defined(XP_WIN)
    nsCOMPtr<nsIProperties> directoryService = 
             do_GetService(NS_DIRECTORY_SERVICE_CONTRACTID, &rv);
    if (NS_FAILED(rv)) return rv;
    const char* prop = aLocal ? NS_WIN_LOCAL_APPDATA_DIR : NS_WIN_APPDATA_DIR;
    rv = directoryService->Get(prop, NS_GET_IID(nsILocalFile), getter_AddRefs(localDir));
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
    rv = NS_NewNativeLocalFile(nsDependentCString(PR_GetEnv("HOME")), PR_TRUE, getter_AddRefs(localDir));
    if (NS_FAILED(rv)) return rv;
#else
#error dont_know_how_to_get_product_dir_on_your_platform
#endif

    rv = localDir->AppendRelativeNativePath(DEFAULT_PRODUCT_DIR);
    if (NS_FAILED(rv)) return rv;
    rv = localDir->Exists(&exists);

    if (NS_SUCCEEDED(rv) && !exists)
        rv = localDir->Create(nsIFile::DIRECTORY_TYPE, 0700);

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
NS_METHOD nsAppFileLocationProvider::GetDefaultUserProfileRoot(nsILocalFile **aLocalFile, PRBool aLocal)
{
    NS_ENSURE_ARG_POINTER(aLocalFile);

    nsresult rv;
    nsCOMPtr<nsILocalFile> localDir;

    rv = GetProductDirectory(getter_AddRefs(localDir), aLocal);
    if (NS_FAILED(rv)) return rv;

#if defined(MOZ_WIDGET_COCOA) || defined(XP_OS2) || defined(XP_WIN)
    // These 3 platforms share this part of the path - do them as one
    rv = localDir->AppendRelativeNativePath(NS_LITERAL_CSTRING("Profiles"));
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

//*****************************************************************************
// nsAppFileLocationProvider::nsIDirectoryServiceProvider2
//*****************************************************************************

class nsAppDirectoryEnumerator : public nsISimpleEnumerator
{
  public:
    NS_DECL_ISUPPORTS

    /**
     * aKeyList is a null-terminated list of properties which are provided by aProvider
     * They do not need to be publicly defined keys.
     */
    nsAppDirectoryEnumerator(nsIDirectoryServiceProvider *aProvider,
                             const char* aKeyList[]) :
        mProvider(aProvider),
        mCurrentKey(aKeyList)
    {
    }

    NS_IMETHOD HasMoreElements(PRBool *result) 
    {
        while (!mNext && *mCurrentKey)
        {
            PRBool dontCare;
            nsCOMPtr<nsIFile> testFile;
            (void)mProvider->GetFile(*mCurrentKey++, &dontCare, getter_AddRefs(testFile));
            // Don't return a file which does not exist.
            PRBool exists;
            if (testFile && NS_SUCCEEDED(testFile->Exists(&exists)) && exists)
                mNext = testFile;
        }
        *result = mNext != nsnull;
        return NS_OK;
    }

    NS_IMETHOD GetNext(nsISupports **result) 
    {
        NS_ENSURE_ARG_POINTER(result);
        *result = nsnull;

        PRBool hasMore;
        HasMoreElements(&hasMore);
        if (!hasMore)
            return NS_ERROR_FAILURE;
            
        *result = mNext;
        NS_IF_ADDREF(*result);
        mNext = nsnull;
        
        return *result ? NS_OK : NS_ERROR_FAILURE;
    }

    // Virtual destructor since subclass nsPathsDirectoryEnumerator
    // does not re-implement Release()

    virtual ~nsAppDirectoryEnumerator()
    {
    }

  protected:
    nsIDirectoryServiceProvider *mProvider;
    const char** mCurrentKey;
    nsCOMPtr<nsIFile> mNext;
};

NS_IMPL_ISUPPORTS1(nsAppDirectoryEnumerator, nsISimpleEnumerator)

/* nsPathsDirectoryEnumerator and PATH_SEPARATOR
 * are not used on MacOS/X. */

#if defined(XP_WIN) || defined(XP_OS2) /* Win32 and OS/2 */
#define PATH_SEPARATOR ';'
#else
#define PATH_SEPARATOR ':'
#endif

class nsPathsDirectoryEnumerator : public nsAppDirectoryEnumerator
{
  public:
    /**
     * aKeyList is a null-terminated list.
     * The first element is a path list.
     * The remainder are properties provided by aProvider.
     * They do not need to be publicly defined keys.
     */
    nsPathsDirectoryEnumerator(nsIDirectoryServiceProvider *aProvider,
                               const char* aKeyList[]) :
    nsAppDirectoryEnumerator(aProvider, aKeyList+1),
    mEndPath(aKeyList[0])
    {
    }

    NS_IMETHOD HasMoreElements(PRBool *result) 
    {
        if (mEndPath)
            while (!mNext && *mEndPath)
            {
                const char *pathVar = mEndPath;
           
                // skip PATH_SEPARATORs at the begining of the mEndPath
                while (*pathVar == PATH_SEPARATOR) pathVar++;

                do { ++mEndPath; } while (*mEndPath && *mEndPath != PATH_SEPARATOR);

                nsCOMPtr<nsILocalFile> localFile;
                NS_NewNativeLocalFile(Substring(pathVar, mEndPath),
                                      PR_TRUE,
                                      getter_AddRefs(localFile));
                if (*mEndPath == PATH_SEPARATOR)
                    ++mEndPath;
                // Don't return a "file" (directory) which does not exist.
                PRBool exists;
                if (localFile &&
                    NS_SUCCEEDED(localFile->Exists(&exists)) &&
                    exists)
                    mNext = localFile;
            }
        if (mNext)
            *result = PR_TRUE;
        else
            nsAppDirectoryEnumerator::HasMoreElements(result);

        return NS_OK;
    }

  protected:
    const char *mEndPath;
};

NS_IMETHODIMP
nsAppFileLocationProvider::GetFiles(const char *prop, nsISimpleEnumerator **_retval)
{
    NS_ENSURE_ARG_POINTER(_retval);
    *_retval = nsnull;
    nsresult rv = NS_ERROR_FAILURE;
    
    if (!nsCRT::strcmp(prop, NS_APP_PLUGINS_DIR_LIST))
    {
#ifdef MOZ_WIDGET_COCOA
        // As of Java for Mac OS X 10.5 Update 10, Apple has (in effect) deprecated Java Plugin2 on
        // on OS X 10.5, and removed the soft link to it from /Library/Internet Plug-Ins/.  Java
        // Plugin2 is still present and usable, but there are no longer any links to it in the
        // "normal" locations.  So we won't be able to find it unless we look in the "non-normal"
        // location where it actually is.  Safari can use the WebKit-specific JavaPluginCocoa.bundle,
        // which (of course) is still fully supported on OS X 10.5.  But we have no alternative to
        // using Java Plugin2.  For more information see bug 668639.
        static const char* keys[] = { NS_APP_PLUGINS_DIR, NS_MACOSX_USER_PLUGIN_DIR,
                                      NS_MACOSX_LOCAL_PLUGIN_DIR,
                                      IsOSXLeopard() ? NS_MACOSX_JAVA2_PLUGIN_DIR : nsnull, nsnull };
        *_retval = new nsAppDirectoryEnumerator(this, keys);
#else
#ifdef XP_UNIX
        static const char* keys[] = { nsnull, NS_USER_PLUGINS_DIR, NS_APP_PLUGINS_DIR, NS_SYSTEM_PLUGINS_DIR, nsnull };
#else
        static const char* keys[] = { nsnull, NS_USER_PLUGINS_DIR, NS_APP_PLUGINS_DIR, nsnull };
#endif
        if (!keys[0] && !(keys[0] = PR_GetEnv("MOZ_PLUGIN_PATH"))) {
            static const char nullstr = 0;
            keys[0] = &nullstr;
        }
        *_retval = new nsPathsDirectoryEnumerator(this, keys);
#endif
        NS_IF_ADDREF(*_retval);
        rv = *_retval ? NS_OK : NS_ERROR_OUT_OF_MEMORY;        
    }
    if (!nsCRT::strcmp(prop, NS_APP_SEARCH_DIR_LIST))
    {
        static const char* keys[] = { nsnull, NS_APP_SEARCH_DIR, NS_APP_USER_SEARCH_DIR, nsnull };
        if (!keys[0] && !(keys[0] = PR_GetEnv("MOZ_SEARCH_ENGINE_PATH"))) {
            static const char nullstr = 0;
            keys[0] = &nullstr;
        }
        *_retval = new nsPathsDirectoryEnumerator(this, keys);
        NS_IF_ADDREF(*_retval);
        rv = *_retval ? NS_OK : NS_ERROR_OUT_OF_MEMORY;        
    }
    return rv;
}

#if defined(XP_MACOSX)
PRBool
nsAppFileLocationProvider::IsOSXLeopard()
{
    static SInt32 version = 0;

    if (!version) {
        OSErr err = ::Gestalt(gestaltSystemVersion, &version);
        if (err != noErr) {
            version = 0;
        } else {
            version &= 0xFFFF; // The system version is in the low order word
        }
    }

    return ((version >= 0x1050) && (version < 0x1060));
}
#endif
