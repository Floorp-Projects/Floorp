/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
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
 * Portions created by the Initial Developer are Copyright (C) 2002
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *  Brian Ryner <bryner@brianryner.com>
 *  Benjamin Smedberg <bsmedberg@covad.net>
 *  Ben Goodger <ben@mozilla.org>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
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

#include "nsAppRunner.h"
#include "nsXREDirProvider.h"

#include "jsapi.h"

#include "nsIJSContextStack.h"
#include "nsILocalFile.h"
#include "nsIObserverService.h"
#include "nsIProfileChangeStatus.h"

#include "nsAppDirectoryServiceDefs.h"
#include "nsDirectoryServiceDefs.h"
#include "nsXULAppAPI.h"

#include "nsINIParser.h"
#include "nsDependentString.h"
#include "nsCOMArray.h"
#include "nsArrayEnumerator.h"

#include <stdlib.h>

#ifdef XP_WIN
#include <windows.h>
#include <shlobj.h>
#endif
#ifdef XP_MACOSX
#include "nsILocalFileMac.h"
#endif
#ifdef XP_BEOS
#include <be/kernel/image.h>
#include <FindDirectory.h>
#endif
#ifdef XP_UNIX
#include <ctype.h>
#endif
#ifdef XP_OS2
#define INCL_DOS
#include <os2.h>
#endif

nsXREDirProvider* gDirServiceProvider = nsnull;

nsXREDirProvider::nsXREDirProvider() :
  mProfileNotified(PR_FALSE),
  mRegisterExtraComponents(PR_TRUE)
{
  gDirServiceProvider = this;
}

nsXREDirProvider::~nsXREDirProvider()
{
  gDirServiceProvider = nsnull;
}

nsresult
nsXREDirProvider::Initialize()
{ 
  // We need to use platform-specific hackery to find the
  // path of this executable. This is copied, with some modifications, from
  // nsGREDirServiceProvider.cpp
#ifdef XP_WIN
  char exePath[MAXPATHLEN];
  if ( ! ::GetModuleFileName(0, exePath, MAXPATHLEN) )
    return NS_ERROR_FAILURE;

  // chop off the executable name by finding the rightmost backslash
  char* lastSlash = strrchr(exePath, '\\');
  if (!lastSlash) return NS_ERROR_FAILURE;

  *(lastSlash) = '\0';
  return NS_NewNativeLocalFile(nsDependentCString(exePath), PR_TRUE,
                               getter_AddRefs(mAppDir));

#elif defined(XP_MACOSX)
  // Works even if we're not bundled.
  CFBundleRef appBundle = CFBundleGetMainBundle();
  if (!appBundle) return NS_ERROR_FAILURE;

  nsresult rv = NS_ERROR_FAILURE;

  CFURLRef bundleURL = CFBundleCopyExecutableURL(appBundle);
  if (bundleURL) {
    CFURLRef parentURL = CFURLCreateCopyDeletingLastPathComponent(kCFAllocatorDefault, bundleURL);
    if (parentURL) {
      rv = NS_NewNativeLocalFile(EmptyCString(), PR_TRUE,
                                 getter_AddRefs(mAppDir));
      if (NS_SUCCEEDED(rv)) {
        nsCOMPtr<nsILocalFileMac> appDirMac (do_QueryInterface(mAppDir, &rv));
        if (NS_SUCCEEDED(rv)) {
          rv = appDirMac->InitWithCFURL(parentURL);
        }
      }

      CFRelease(parentURL);
    }
    CFRelease(bundleURL);
  }

  return rv;

#elif defined(XP_UNIX)
  // Because we do have access to argv[0], we can get the application
  // path with certitude, instead of using MOZILLA_FIVE_HOME or
  // guessing from the CWD like nsGREDirServiceProvider does.
  char* lastSlash = strrchr(gBinaryPath, '/');
  if (!lastSlash) return NS_ERROR_FAILURE;

  nsDependentCSubstring appDir(gBinaryPath, lastSlash);
  return NS_NewNativeLocalFile(appDir, PR_TRUE, getter_AddRefs(mAppDir));

#elif defined(XP_OS2)
  PPIB ppib;
  PTIB ptib;
  char appDir[MAXPATHLEN];
  char* p;
  DosGetInfoBlocks( &ptib, &ppib);
  DosQueryModuleName( ppib->pib_hmte, MAXPATHLEN, appDir);
  p = strrchr( appDir, '\\'); // XXX DBCS misery
  if (!p) return NS_ERROR_FAILURE;

  *p  = '\0';
  return NS_NewNativeLocalFile(nsDependentCString(appDir), PR_TRUE, getter_AddRefs(mAppDir));

#elif defined(XP_BEOS)
  int32 cookie = 0;
  image_info info;
  char *p;

  if(get_next_image_info(0, &cookie, &info) != B_OK)
    return NS_ERROR_FAILURE;

  p = strrchr(info.name, '/');
  if (!p) return NS_ERROR_FAILURE;

  *p = 0;
  return NS_NewNativeLocalFile(nsDependentCString(info.name), PR_TRUE, getter_AddRefs(mAppDir));
#elif
#error Oops, you need platform-specific code here
#endif
}

nsresult
nsXREDirProvider::SetProfileDir(nsIFile* aDir)
{
  NS_ASSERTION(aDir, "We don't support no-profile apps yet!");

#ifdef DEBUG_bsmedberg
  nsCAutoString path;
  aDir->GetNativePath(path);
  printf("nsXREDirProvider::SetProfileDir('%s')\n", path.get());
#endif

  mProfileDir = aDir;
  return NS_OK;
}

NS_IMPL_QUERY_INTERFACE3(nsXREDirProvider,
                         nsIDirectoryServiceProvider,
                         nsIDirectoryServiceProvider2,
                         nsIProfileStartup)

NS_IMETHODIMP_(nsrefcnt)
nsXREDirProvider::AddRef()
{
  return 1;
}

NS_IMETHODIMP_(nsrefcnt)
nsXREDirProvider::Release()
{
  return 0;
}

NS_IMETHODIMP
nsXREDirProvider::GetFile(const char* aProperty, PRBool* aPersistent,
			  nsIFile** aFile)
{
  nsresult rv = NS_ERROR_FAILURE;
  *aPersistent = PR_TRUE;
  nsCOMPtr<nsIFile> file;

  if (!strcmp(aProperty, NS_OS_CURRENT_PROCESS_DIR) ||
      !strcmp(aProperty, NS_APP_INSTALL_CLEANUP_DIR)) {
    // NOTE: this is *different* than NS_XPCOM_CURRENT_PROCESS_DIR. This points
    // to the application dir. NS_XPCOM_CURRENT_PROCESS_DIR points to the toolkit.
    return mAppDir->Clone(aFile);
  }
  else if (!strcmp(aProperty, NS_APP_PROFILE_DEFAULTS_50_DIR) ||
           !strcmp(aProperty, NS_APP_PROFILE_DEFAULTS_NLOC_50_DIR)) {
    return GetProfileDefaultsDir(aFile);
  }
  else if (!strcmp(aProperty, NS_APP_PREF_DEFAULTS_50_DIR))
  {
    rv = mAppDir->Clone(getter_AddRefs(file));
    if (NS_SUCCEEDED(rv)) {
      rv = file->AppendNative(nsDependentCString("defaults"));
      if (NS_SUCCEEDED(rv))
        rv = file->AppendNative(nsDependentCString("pref"));
    }
  }
  else if (!strcmp(aProperty, NS_APP_USER_PROFILES_ROOT_DIR)) {
    rv = GetUserAppDataDirectory((nsILocalFile**)(nsIFile**) getter_AddRefs(file));

#if !defined(XP_UNIX) || defined(XP_MACOSX)
    rv |= file->AppendNative(nsDependentCString("Profiles"));
#endif

    // We must create the profile directory here if it does not exist.
    rv |= EnsureDirectoryExists(file);
  }
  else if (mProfileDir) {
    if (!strcmp(aProperty, NS_XPCOM_COMPONENT_REGISTRY_FILE)) {
      rv = mProfileDir->Clone(getter_AddRefs(file));
      rv |= file->AppendNative(nsDependentCString("compreg.dat"));
    }
    else if (!strcmp(aProperty, NS_XPCOM_XPTI_REGISTRY_FILE)) {
      rv = mProfileDir->Clone(getter_AddRefs(file));
      rv |= file->AppendNative(nsDependentCString("xpti.dat"));
    }
    if (mProfileNotified) {
      if (!strcmp(aProperty, NS_APP_USER_PROFILE_50_DIR) ||
          !strcmp(aProperty, NS_APP_PREFS_50_DIR)) {
        return mProfileDir->Clone(aFile);
      }
      else if (!strcmp(aProperty, NS_APP_PREFS_50_FILE)) {
        rv = mProfileDir->Clone(getter_AddRefs(file));
        rv |= file->AppendNative(nsDependentCString("prefs.js"));
      }
      // XXXbsmedberg this needs rethinking... many of these are app-specific,
      // and apps are going to add new stuff. I don't have a good solution,
      // yet.
      else if (!strcmp(aProperty, NS_APP_USER_CHROME_DIR)) {
        rv = mProfileDir->Clone(getter_AddRefs(file));
        rv |= file->AppendNative(nsDependentCString("chrome"));
      }
      else if (!strcmp(aProperty, NS_APP_LOCALSTORE_50_FILE)) {
        rv = mProfileDir->Clone(getter_AddRefs(file));
        rv |= file->AppendNative(nsDependentCString("localstore.rdf"));
        EnsureProfileFileExists(file);
      }
      else if (!strcmp(aProperty, NS_APP_HISTORY_50_FILE)) {
        rv = mProfileDir->Clone(getter_AddRefs(file));
        rv |= file->AppendNative(nsDependentCString("history.dat"));
      }
      else if (!strcmp(aProperty, NS_APP_USER_PANELS_50_FILE)) {
        rv = mProfileDir->Clone(getter_AddRefs(file));
        rv |= file->AppendNative(nsDependentCString("panels.rdf"));
        EnsureProfileFileExists(file);
      }
      else if (!strcmp(aProperty, NS_APP_USER_MIMETYPES_50_FILE)) {
        rv = mProfileDir->Clone(getter_AddRefs(file));
        rv |= file->AppendNative(nsDependentCString("mimeTypes.rdf"));
        EnsureProfileFileExists(file);
      }
      else if (!strcmp(aProperty, NS_APP_BOOKMARKS_50_FILE)) {
        rv = mProfileDir->Clone(getter_AddRefs(file));
        rv |= file->AppendNative(nsDependentCString("bookmarks.html"));
      }
      else if (!strcmp(aProperty, NS_APP_DOWNLOADS_50_FILE)) {
        rv = mProfileDir->Clone(getter_AddRefs(file));
        rv |= file->AppendNative(nsDependentCString("downloads.rdf"));
      }
      else if (!strcmp(aProperty, NS_APP_SEARCH_50_FILE)) {
        rv = mProfileDir->Clone(getter_AddRefs(file));
        rv |= file->AppendNative(nsDependentCString("search.rdf"));
        EnsureProfileFileExists(file);
      }
      else if (!strcmp(aProperty, NS_APP_MAIL_50_DIR)) {
        rv = mProfileDir->Clone(getter_AddRefs(file));
        rv |= file->AppendNative(nsDependentCString("Mail"));
      }
      else if (!strcmp(aProperty, NS_APP_IMAP_MAIL_50_DIR)) {
        rv = mProfileDir->Clone(getter_AddRefs(file));
        rv |= file->AppendNative(nsDependentCString("ImapMail"));
      }
      else if (!strcmp(aProperty, NS_APP_NEWS_50_DIR)) {
        rv = mProfileDir->Clone(getter_AddRefs(file));
        rv |= file->AppendNative(nsDependentCString("News"));
      }
      else if (!strcmp(aProperty, NS_APP_MESSENGER_FOLDER_CACHE_50_DIR)) {
        rv = mProfileDir->Clone(getter_AddRefs(file));
        rv |= file->AppendNative(nsDependentCString("panacea.dat"));
      }
    }
  }
  if (NS_FAILED(rv)) return NS_ERROR_FAILURE;
  if (!file) return NS_ERROR_FAILURE;

  NS_ADDREF(*aFile = file);
  return NS_OK;
}

void 
nsXREDirProvider::RegisterExtraComponents()
{
  mRegisterExtraComponents = PR_TRUE;
}

static void
LoadDirsIntoArray(nsIFile* aComponentsList, nsCOMArray<nsIFile>& aDirectories)
{
  nsINIParser parser;
  nsCOMPtr<nsILocalFile> lf(do_QueryInterface(aComponentsList));
  parser.Init(lf);

  char parserBuf[MAXPATHLEN];
  nsresult rv = parser.GetString("Extra Files", "Count", parserBuf, MAXPATHLEN);
  if (NS_SUCCEEDED(rv)) {
    PRInt32 count = atoi(parserBuf);
    char buf[10];
    nsCOMPtr<nsIFile> parent;
    aComponentsList->GetParent(getter_AddRefs(parent));
    for (PRInt32 i = 0; i < count; ++i) {
      sprintf(buf, "File%d", i);

      rv = parser.GetString("Extra Files", buf, parserBuf, MAXPATHLEN);
      if (NS_SUCCEEDED(rv)) {
        nsCOMPtr<nsILocalFile> lfParent(do_QueryInterface(parent));
        nsCOMPtr<nsILocalFile> dir(do_CreateInstance("@mozilla.org/file/local;1"));
        dir->SetRelativeDescriptor(lfParent, nsDependentCString(parserBuf));
        nsCOMPtr<nsIFile> dirAsFile(do_QueryInterface(dir));
        aDirectories.AppendObject(dirAsFile);
      }
    }
  }
}

NS_IMETHODIMP
nsXREDirProvider::GetFiles(const char* aProperty, nsISimpleEnumerator** aResult)
{
  nsresult rv = NS_OK;
  *aResult = nsnull;

  if (!strcmp(aProperty, NS_XPCOM_COMPONENT_DIR_LIST)) {
    if (mRegisterExtraComponents) {
      nsCOMArray<nsIFile> directories;
  
      nsCOMPtr<nsIFile> appFile;
      mAppDir->Clone(getter_AddRefs(appFile));
      appFile->AppendNative(nsDependentCString("components.ini"));
      LoadDirsIntoArray(appFile, directories);

      nsCOMPtr<nsIFile> profileFile;
      if (mProfileDir) {
        mProfileDir->Clone(getter_AddRefs(profileFile));
        profileFile->AppendNative(nsDependentCString("components.ini"));
        LoadDirsIntoArray(profileFile, directories);
      }

      rv = NS_NewArrayEnumerator(aResult, directories);
    }
  }
  else if (!strcmp(aProperty, NS_APP_PREFS_DEFAULTS_DIR_LIST)) {
    nsCOMArray<nsIFile> directories;

    nsCOMPtr<nsIFile> appFile;
    mAppDir->Clone(getter_AddRefs(appFile));
    appFile->AppendNative(nsDependentCString("defaults.ini"));
    LoadDirsIntoArray(appFile, directories);

    nsCOMPtr<nsIFile> profileFile;
    if (mProfileDir) {
      mProfileDir->Clone(getter_AddRefs(profileFile));
      profileFile->AppendNative(nsDependentCString("defaults.ini"));
      LoadDirsIntoArray(profileFile, directories);
    }

    rv = NS_NewArrayEnumerator(aResult, directories);
  }
  else
    rv = NS_ERROR_FAILURE;

  return rv;
}

NS_IMETHODIMP
nsXREDirProvider::GetDirectory(nsIFile* *aResult)
{
  NS_ENSURE_TRUE(mProfileDir, NS_ERROR_NOT_INITIALIZED);

  return mProfileDir->Clone(aResult);
}

NS_IMETHODIMP
nsXREDirProvider::DoStartup()
{
  if (!mProfileNotified) {
    nsCOMPtr<nsIObserverService> obsSvc
      (do_GetService("@mozilla.org/observer-service;1"));
    if (!obsSvc) return NS_ERROR_FAILURE;

    mProfileNotified = PR_TRUE;

    static const PRUnichar kStartup[] = {'s','t','a','r','t','u','p','\0'};
    obsSvc->NotifyObservers(nsnull, "profile-do-change", kStartup);
    obsSvc->NotifyObservers(nsnull, "profile-after-change", kStartup);
  }
  return NS_OK;
}

class ProfileChangeStatusImpl : public nsIProfileChangeStatus
{
public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSIPROFILECHANGESTATUS
  ProfileChangeStatusImpl() { }
private:
  ~ProfileChangeStatusImpl() { }
};

NS_IMPL_ISUPPORTS1(ProfileChangeStatusImpl, nsIProfileChangeStatus)

NS_IMETHODIMP
ProfileChangeStatusImpl::VetoChange()
{
  NS_ERROR("Can't veto change!");
  return NS_ERROR_FAILURE;
}

NS_IMETHODIMP
ProfileChangeStatusImpl::ChangeFailed()
{
  NS_ERROR("Profile change cancellation.");
  return NS_ERROR_FAILURE;
}

void
nsXREDirProvider::DoShutdown()
{
  if (mProfileNotified) {
    nsCOMPtr<nsIObserverService> obssvc
      (do_GetService("@mozilla.org/observer-service;1"));
    NS_ASSERTION(obssvc, "No observer service?");
    if (obssvc) {
      nsCOMPtr<nsIProfileChangeStatus> cs = new ProfileChangeStatusImpl();
      static const PRUnichar kShutdownPersist[] =
        {'s','h','u','t','d','o','w','n','-','p','e','r','s','i','s','t','\0'};
      obssvc->NotifyObservers(cs, "profile-change-net-teardown", kShutdownPersist);
      obssvc->NotifyObservers(cs, "profile-change-teardown", kShutdownPersist);

      // Phase 2c: Now that things are torn down, force JS GC so that things which depend on
      // resources which are about to go away in "profile-before-change" are destroyed first.
      nsCOMPtr<nsIThreadJSContextStack> stack
        (do_GetService("@mozilla.org/js/xpc/ContextStack;1"));
      if (stack)
      {
        JSContext *cx = nsnull;
        stack->GetSafeJSContext(&cx);
        if (cx)
          ::JS_GC(cx);
      }

      // Phase 3: Notify observers of a profile change
      obssvc->NotifyObservers(cs, "profile-before-change", kShutdownPersist);
    }
    mProfileNotified = PR_FALSE;
  }
}

static void 
GetProfileFolderName(char* aProfileFolderName, const char* aSource)
{
  const char* reading = aSource;

  while (*reading) {
    *aProfileFolderName = tolower(*reading);
    ++aProfileFolderName; ++reading;
  }
  *aProfileFolderName = '\0';
}

nsresult
nsXREDirProvider::GetUserAppDataDirectory(nsILocalFile** aFile)
{
  NS_ASSERTION(gAppData, "gAppData not initialized!");

  // Copied from nsAppFileLocationProvider (more or less)
  nsresult rv;
  nsCOMPtr<nsILocalFile> localDir;

#if defined(XP_MACOSX)
   FSRef fsRef;
#ifdef MOZ_THUNDERBIRD 
  OSErr err = ::FSFindFolder(kUserDomain, kDomainLibraryFolderType, kCreateFolder, &fsRef);
#else
  OSErr err = ::FSFindFolder(kUserDomain, kApplicationSupportFolderType, kCreateFolder, &fsRef);
#endif 
  if (err) return NS_ERROR_FAILURE;

  rv = NS_NewNativeLocalFile(EmptyCString(), PR_TRUE, getter_AddRefs(localDir));
  NS_ENSURE_SUCCESS(rv, rv);

  nsCOMPtr<nsILocalFileMac> dirFileMac = do_QueryInterface(localDir);
  NS_ENSURE_TRUE(dirFileMac, NS_ERROR_UNEXPECTED);

  rv = dirFileMac->InitWithFSRef(&fsRef);
  NS_ENSURE_SUCCESS(rv, rv);

  // Note that MacOS ignores the vendor when creating the profile hierarchy - all
  // application preferences directories live alongside one another in 
  // ~/Library/Application Support/
  rv = dirFileMac->AppendNative(nsDependentCString(gAppData->appName));
  NS_ENSURE_SUCCESS(rv, rv);
#elif defined(XP_WIN)
  LPMALLOC pMalloc;
  LPITEMIDLIST pItemIDList = NULL;

  if (!SUCCEEDED(SHGetMalloc(&pMalloc)))
    return NS_ERROR_OUT_OF_MEMORY;

  char appDataPath[MAXPATHLEN];

  if (SUCCEEDED(SHGetSpecialFolderLocation(NULL, CSIDL_APPDATA, &pItemIDList)) &&
      SUCCEEDED(SHGetPathFromIDList(pItemIDList, appDataPath))) {
  } else {
    if (!GetWindowsDirectory(appDataPath, MAXPATHLEN)) {
      NS_WARNING("Aaah, no windows directory!");
      return NS_ERROR_FAILURE;
    }
  }

  if (pItemIDList) pMalloc->Free(pItemIDList);
  pMalloc->Release();

  rv = NS_NewNativeLocalFile(nsDependentCString(appDataPath),
                             PR_TRUE, getter_AddRefs(localDir));
  NS_ENSURE_SUCCESS(rv, rv);

  if (gAppData->appVendor) {
    rv = localDir->AppendNative(nsDependentCString(gAppData->appVendor));
    NS_ENSURE_SUCCESS(rv, rv);
  }
  rv = localDir->AppendNative(nsDependentCString(gAppData->appName));
  NS_ENSURE_SUCCESS(rv, rv);

#elif defined(XP_OS2)
#if 0 /* For OS/2 we want to always use MOZILLA_HOME */
  // we want an environment variable of the form
  // FIREFOX_HOME, etc
  nsDependentCString envVar(nsDependentCString(gAppData->appName));
  envVar.Append("_HOME");
  char *pHome = getenv(envVar.get());
#endif
  char *pHome = getenv("MOZILLA_HOME");
  if (pHome && *pHome) {
    rv = NS_NewNativeLocalFile(nsDependentCString(pHome), PR_TRUE,
                               getter_AddRefs(localDir));
  } else {
    PPIB ppib;
    PTIB ptib;
    char appDir[CCHMAXPATH];

    DosGetInfoBlocks(&ptib, &ppib);
    DosQueryModuleName(ppib->pib_hmte, CCHMAXPATH, appDir);
    *strrchr(appDir, '\\') = '\0';
    rv = NS_NewNativeLocalFile(nsDependentCString(appDir), PR_TRUE, getter_AddRefs(localDir));
  }
  NS_ENSURE_SUCCESS(rv, rv);

  if (gAppData->appVendor) {
    rv = localDir->AppendNative(nsDependentCString(gAppData->appVendor));
    NS_ENSURE_SUCCESS(rv, rv);
  }
  rv = localDir->AppendNative(nsDependentCString(gAppData->appName));
  NS_ENSURE_SUCCESS(rv, rv);

#elif defined(XP_BEOS)
  char appDir[MAXPATHLEN];
  if (find_directory(B_USER_SETTINGS_DIRECTORY, NULL, true, appDir, MAXPATHLEN))
    return NS_ERROR_FAILURE;

  int len = strlen(appDir);
  appDir[len]   = '/';
  appDir[len+1] = '\0';

  rv = NS_NewNativeLocalFile(nsDependentCString(appDir), PR_TRUE,
                             getter_AddRefs(localDir));
  NS_ENSURE_SUCCESS(rv, rv);

  if (gAppData->appVendor) {
    rv = localDir->AppendNative(nsDependentCString(gAppData->appVendor));
    NS_ENSURE_SUCCESS(rv, rv);
  }
  rv = localDir->AppendNative(nsDependentCString(gAppData->appName));
  NS_ENSURE_SUCCESS(rv, rv);

#elif defined(XP_UNIX)
  const char* homeDir = getenv("HOME");
  if (!homeDir || !*homeDir)
    return NS_ERROR_FAILURE;

  rv = NS_NewNativeLocalFile(nsDependentCString(homeDir), PR_TRUE,
                             getter_AddRefs(localDir));
  NS_ENSURE_SUCCESS(rv, rv);
 
  char* appNameFolder = nsnull;
  char profileFolderName[MAXPATHLEN] = ".";
 
  // Offset 1 for the outermost folder to make it hidden (i.e. using the ".")
  char* writing = profileFolderName + 1;
  if (gAppData->appVendor) {
    GetProfileFolderName(writing, gAppData->appVendor);
    
    rv = localDir->AppendNative(nsDependentCString(profileFolderName));
    NS_ENSURE_SUCCESS(rv, rv);
 
    char temp[MAXPATHLEN];
    GetProfileFolderName(temp, gAppData->appName);
    appNameFolder = temp;
  }
  else {
    GetProfileFolderName(writing, gAppData->appName);
    appNameFolder = profileFolderName;
  }
  rv = localDir->AppendNative(nsDependentCString(appNameFolder));
  NS_ENSURE_SUCCESS(rv, rv);
#else
#error dont_know_how_to_get_product_dir_on_your_platform
#endif

  rv = EnsureDirectoryExists(localDir);
  NS_ENSURE_SUCCESS(rv, rv);

  *aFile = localDir;
  NS_ADDREF(*aFile);
  return NS_OK;
}

nsresult
nsXREDirProvider::EnsureDirectoryExists(nsIFile* aDirectory)
{
  PRBool exists;
  nsresult rv = aDirectory->Exists(&exists);
  NS_ENSURE_SUCCESS(rv, rv);
  if (!exists)
    rv = aDirectory->Create(nsIFile::DIRECTORY_TYPE, 0775);

  return rv;
}

void
nsXREDirProvider::EnsureProfileFileExists(nsIFile *aFile)
{
  nsresult rv;
  PRBool exists;
    
  rv = aFile->Exists(&exists);
  if (NS_FAILED(rv) || exists) return;
  
  nsCAutoString leafName;
  rv = aFile->GetNativeLeafName(leafName);
  if (NS_FAILED(rv)) return;

  nsCOMPtr<nsIFile> defaultsFile;
  rv = GetProfileDefaultsDir(getter_AddRefs(defaultsFile));
  if (NS_FAILED(rv)) return;

  rv = defaultsFile->AppendNative(leafName);
  if (NS_FAILED(rv)) return;
  
  defaultsFile->CopyToNative(mProfileDir, EmptyCString());
}

nsresult
nsXREDirProvider::GetProfileDefaultsDir(nsIFile* *aResult)
{
  NS_ASSERTION(mAppDir, "nsXREDirProvider not initialized.");
  NS_PRECONDITION(aResult, "Null out-param");

  nsresult rv;
  nsCOMPtr<nsIFile> defaultsDir;

  rv = mAppDir->Clone(getter_AddRefs(defaultsDir));
  NS_ENSURE_SUCCESS(rv, rv);

  rv = defaultsDir->AppendNative(nsDependentCString("defaults"));
  rv |= defaultsDir->AppendNative(nsDependentCString("profile"));
  NS_ENSURE_SUCCESS(rv, rv);

  NS_ADDREF(*aResult = defaultsDir);
  return NS_OK;
}

