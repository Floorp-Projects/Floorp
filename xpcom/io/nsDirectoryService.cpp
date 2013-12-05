/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: sw=4 ts=4 sts=4 et
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/Util.h"

#include "nsCOMPtr.h"
#include "nsAutoPtr.h"
#include "nsDirectoryService.h"
#include "nsDirectoryServiceDefs.h"
#include "nsLocalFile.h"
#include "nsDebug.h"
#include "nsStaticAtom.h"
#include "nsEnumeratorUtils.h"

#include "nsICategoryManager.h"
#include "nsISimpleEnumerator.h"
#include "nsIStringEnumerator.h"

#if defined(XP_WIN)
#include <windows.h>
#include <shlobj.h>
#include <stdlib.h>
#include <stdio.h>
#elif defined(XP_UNIX)
#include <unistd.h>
#include <stdlib.h>
#include <sys/param.h>
#include "prenv.h"
#ifdef MOZ_WIDGET_COCOA
#include <CoreServices/CoreServices.h>
#include <Carbon/Carbon.h>
#endif
#elif defined(XP_OS2)
#define MAX_PATH _MAX_PATH
#endif

#include "SpecialSystemDirectory.h"
#include "nsAppFileLocationProvider.h"

using namespace mozilla;

// define home directory
// For Windows platform, We are choosing Appdata folder as HOME
#if defined (XP_WIN)
#define HOME_DIR NS_WIN_APPDATA_DIR
#elif defined (MOZ_WIDGET_COCOA)
#define HOME_DIR NS_OSX_HOME_DIR
#elif defined (XP_UNIX)
#define HOME_DIR NS_UNIX_HOME_DIR
#elif defined (XP_OS2)
#define HOME_DIR NS_OS2_HOME_DIR
#endif

//----------------------------------------------------------------------------------------
nsresult 
nsDirectoryService::GetCurrentProcessDirectory(nsIFile** aFile)
//----------------------------------------------------------------------------------------
{
    if (NS_WARN_IF(!aFile))
        return NS_ERROR_INVALID_ARG;
    *aFile = nullptr;
    
   //  Set the component registry location:
    if (!gService)
        return NS_ERROR_FAILURE;

    nsresult rv; 
 
    nsCOMPtr<nsIProperties> dirService;
    rv = nsDirectoryService::Create(nullptr, 
                                    NS_GET_IID(nsIProperties), 
                                    getter_AddRefs(dirService));  // needs to be around for life of product
    if (NS_FAILED(rv))
        return rv;

    if (dirService)
    {
      nsCOMPtr <nsIFile> aLocalFile;
      dirService->Get(NS_XPCOM_INIT_CURRENT_PROCESS_DIR, NS_GET_IID(nsIFile), getter_AddRefs(aLocalFile));
      if (aLocalFile)
      {
        *aFile = aLocalFile;
        NS_ADDREF(*aFile);
        return NS_OK;
      }
    }

    nsLocalFile* localFile = new nsLocalFile;

    if (localFile == nullptr)
        return NS_ERROR_OUT_OF_MEMORY;
    NS_ADDREF(localFile);



#ifdef XP_WIN
    wchar_t buf[MAX_PATH + 1];
    SetLastError(ERROR_SUCCESS);
    if (GetModuleFileNameW(0, buf, mozilla::ArrayLength(buf)) &&
        GetLastError() != ERROR_INSUFFICIENT_BUFFER) {
        // chop off the executable name by finding the rightmost backslash
        wchar_t* lastSlash = wcsrchr(buf, L'\\');
        if (lastSlash)
            *(lastSlash + 1) = L'\0';

        localFile->InitWithPath(nsDependentString(buf));
        *aFile = localFile;
        return NS_OK;
    }

#elif defined(MOZ_WIDGET_COCOA)
    // Works even if we're not bundled.
    CFBundleRef appBundle = CFBundleGetMainBundle();
    if (appBundle != nullptr)
    {
        CFURLRef bundleURL = CFBundleCopyExecutableURL(appBundle);
        if (bundleURL != nullptr)
        {
            CFURLRef parentURL = CFURLCreateCopyDeletingLastPathComponent(kCFAllocatorDefault, bundleURL);
            if (parentURL)
            {
                // Pass true for the "resolveAgainstBase" arg to CFURLGetFileSystemRepresentation.
                // This will resolve the relative portion of the CFURL against it base, giving a full
                // path, which CFURLCopyFileSystemPath doesn't do.
                char buffer[PATH_MAX];
                if (CFURLGetFileSystemRepresentation(parentURL, true, (UInt8 *)buffer, sizeof(buffer)))
                {
#ifdef DEBUG_conrad
                    printf("nsDirectoryService - CurrentProcessDir is: %s\n", buffer);
#endif
                    rv = localFile->InitWithNativePath(nsDependentCString(buffer));
                    if (NS_SUCCEEDED(rv))
                        *aFile = localFile;
                }
                CFRelease(parentURL);
            }
            CFRelease(bundleURL);
        }
    }
    
    NS_ASSERTION(*aFile, "nsDirectoryService - Could not determine CurrentProcessDir.\n");
    if (*aFile)
        return NS_OK;

#elif defined(XP_UNIX)

    // In the absence of a good way to get the executable directory let
    // us try this for unix:
    //    - if MOZILLA_FIVE_HOME is defined, that is it
    //    - else give the current directory
    char buf[MAXPATHLEN];

    // The MOZ_DEFAULT_MOZILLA_FIVE_HOME variable can be set at configure time with
    // a --with-default-mozilla-five-home=foo autoconf flag.
    // 
    // The idea here is to allow for builds that have a default MOZILLA_FIVE_HOME
    // regardless of the environment.  This makes it easier to write apps that
    // embed mozilla without having to worry about setting up the environment 
    //
    // We do this by putenv()ing the default value into the environment.  Note that
    // we only do this if it is not already set.
#ifdef MOZ_DEFAULT_MOZILLA_FIVE_HOME
    const char *home = PR_GetEnv("MOZILLA_FIVE_HOME");
    if (!home || !*home)
    {
        putenv("MOZILLA_FIVE_HOME=" MOZ_DEFAULT_MOZILLA_FIVE_HOME);
    }
#endif

    char *moz5 = PR_GetEnv("MOZILLA_FIVE_HOME");
    if (moz5 && *moz5)
    {
        if (realpath(moz5, buf)) {
            localFile->InitWithNativePath(nsDependentCString(buf));
            *aFile = localFile;
            return NS_OK;
        }
    }
#if defined(DEBUG)
    static bool firstWarning = true;

    if((!moz5 || !*moz5) && firstWarning) {
        // Warn that MOZILLA_FIVE_HOME not set, once.
        printf("Warning: MOZILLA_FIVE_HOME not set.\n");
        firstWarning = false;
    }
#endif /* DEBUG */

    // Fall back to current directory.
    if (getcwd(buf, sizeof(buf)))
    {
        localFile->InitWithNativePath(nsDependentCString(buf));
        *aFile = localFile;
        return NS_OK;
    }

#elif defined(XP_OS2)
    PPIB ppib;
    PTIB ptib;
    char buffer[CCHMAXPATH];
    DosGetInfoBlocks( &ptib, &ppib);
    DosQueryModuleName( ppib->pib_hmte, CCHMAXPATH, buffer);
    *strrchr( buffer, '\\') = '\0'; // XXX DBCS misery
    localFile->InitWithNativePath(nsDependentCString(buffer));
    *aFile = localFile;
    return NS_OK;

#endif
    
    NS_RELEASE(localFile);

    NS_ERROR("unable to get current process directory");
    return NS_ERROR_FAILURE;
} // GetCurrentProcessDirectory()

nsDirectoryService* nsDirectoryService::gService = nullptr;

nsDirectoryService::nsDirectoryService()
    : mHashtable(256)
{
}

nsresult
nsDirectoryService::Create(nsISupports *outer, REFNSIID aIID, void **aResult)
{
    if (NS_WARN_IF(!aResult))
        return NS_ERROR_INVALID_ARG;
    if (NS_WARN_IF(outer))
        return NS_ERROR_NO_AGGREGATION;

    if (!gService)
    {
        return NS_ERROR_NOT_INITIALIZED;
    }

    return gService->QueryInterface(aIID, aResult);
}

#define DIR_ATOM(name_, value_) nsIAtom* nsDirectoryService::name_ = nullptr;
#include "nsDirectoryServiceAtomList.h"
#undef DIR_ATOM

#define DIR_ATOM(name_, value_) NS_STATIC_ATOM_BUFFER(name_##_buffer, value_)
#include "nsDirectoryServiceAtomList.h"
#undef DIR_ATOM

static const nsStaticAtom directory_atoms[] = {
#define DIR_ATOM(name_, value_) NS_STATIC_ATOM(name_##_buffer, &nsDirectoryService::name_),
#include "nsDirectoryServiceAtomList.h"
#undef DIR_ATOM
};    

NS_IMETHODIMP
nsDirectoryService::Init()
{
    NS_NOTREACHED("nsDirectoryService::Init() for internal use only!");
    return NS_OK;
}

void
nsDirectoryService::RealInit()
{
    NS_ASSERTION(!gService, 
                 "nsDirectoryService::RealInit Mustn't initialize twice!");

    nsRefPtr<nsDirectoryService> self = new nsDirectoryService();

    NS_RegisterStaticAtoms(directory_atoms);
    
    // Let the list hold the only reference to the provider.
    nsAppFileLocationProvider *defaultProvider = new nsAppFileLocationProvider;
    self->mProviders.AppendElement(defaultProvider);

    self.swap(gService);
}

nsDirectoryService::~nsDirectoryService()
{
}

NS_IMPL_ISUPPORTS4(nsDirectoryService, nsIProperties, nsIDirectoryService, nsIDirectoryServiceProvider, nsIDirectoryServiceProvider2)


NS_IMETHODIMP
nsDirectoryService::Undefine(const char* prop)
{
    if (NS_WARN_IF(!prop))
        return NS_ERROR_INVALID_ARG;

    nsDependentCString key(prop);
    if (!mHashtable.Get(key, nullptr))
        return NS_ERROR_FAILURE;

    mHashtable.Remove(key);
    return NS_OK;
 }

NS_IMETHODIMP
nsDirectoryService::GetKeys(uint32_t *count, char ***keys)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

struct FileData
{
  FileData(const char* aProperty,
           const nsIID& aUUID) :
    property(aProperty),
    data(nullptr),
    persistent(true),
    uuid(aUUID) {}
    
  const char*   property;
  nsISupports*  data;
  bool          persistent;
  const nsIID&  uuid;
};

static bool FindProviderFile(nsIDirectoryServiceProvider* aElement,
                             FileData* aData)
{
  nsresult rv;
  if (aData->uuid.Equals(NS_GET_IID(nsISimpleEnumerator))) {
      // Not all providers implement this iface
      nsCOMPtr<nsIDirectoryServiceProvider2> prov2 = do_QueryInterface(aElement);
      if (prov2)
      {
          nsCOMPtr<nsISimpleEnumerator> newFiles;
          rv = prov2->GetFiles(aData->property, getter_AddRefs(newFiles));
          if (NS_SUCCEEDED(rv) && newFiles) {
              if (aData->data) {
                  nsCOMPtr<nsISimpleEnumerator> unionFiles;

                  NS_NewUnionEnumerator(getter_AddRefs(unionFiles),
                                        (nsISimpleEnumerator*) aData->data, newFiles);

                  if (unionFiles)
                      unionFiles.swap(* (nsISimpleEnumerator**) &aData->data);
              }
              else
              {
                  NS_ADDREF(aData->data = newFiles);
              }
                  
              aData->persistent = false; // Enumerators can never be persistent
              return rv == NS_SUCCESS_AGGREGATE_RESULT;
          }
      }
  }
  else
  {
      rv = aElement->GetFile(aData->property, &aData->persistent,
                             (nsIFile **)&aData->data);
      if (NS_SUCCEEDED(rv) && aData->data)
          return false;
  }

  return true;
}

NS_IMETHODIMP
nsDirectoryService::Get(const char* prop, const nsIID & uuid, void* *result)
{
    if (NS_WARN_IF(!prop))
        return NS_ERROR_INVALID_ARG;

    nsDependentCString key(prop);

    nsCOMPtr<nsIFile> cachedFile = mHashtable.Get(key);

    if (cachedFile) {
        nsCOMPtr<nsIFile> cloneFile;
        cachedFile->Clone(getter_AddRefs(cloneFile));
        return cloneFile->QueryInterface(uuid, result);
    }

    // it is not one of our defaults, lets check any providers
    FileData fileData(prop, uuid);

    for (int32_t i = mProviders.Length() - 1; i >= 0; i--) {
        if (!FindProviderFile(mProviders[i], &fileData)) {
            break;
        }
    }
    if (fileData.data)
    {
        if (fileData.persistent)
        {
            Set(prop, static_cast<nsIFile*>(fileData.data));
        }
        nsresult rv = (fileData.data)->QueryInterface(uuid, result);
        NS_RELEASE(fileData.data);  // addref occurs in FindProviderFile()
        return rv;
    }

    FindProviderFile(static_cast<nsIDirectoryServiceProvider*>(this), &fileData);
    if (fileData.data)
    {
        if (fileData.persistent)
        {
            Set(prop, static_cast<nsIFile*>(fileData.data));
        }
        nsresult rv = (fileData.data)->QueryInterface(uuid, result);
        NS_RELEASE(fileData.data);  // addref occurs in FindProviderFile()
        return rv;
    }

    return NS_ERROR_FAILURE;
}

NS_IMETHODIMP
nsDirectoryService::Set(const char* prop, nsISupports* value)
{
    if (NS_WARN_IF(!prop))
        return NS_ERROR_INVALID_ARG;

    nsDependentCString key(prop);
    if (mHashtable.Get(key, nullptr) || !value) {
        return NS_ERROR_FAILURE;
    }

    nsCOMPtr<nsIFile> ourFile = do_QueryInterface(value);
    if (ourFile) {
      nsCOMPtr<nsIFile> cloneFile;
      ourFile->Clone (getter_AddRefs (cloneFile));
      mHashtable.Put(key, cloneFile);

      return NS_OK;
    }

    return NS_ERROR_FAILURE;
}

NS_IMETHODIMP
nsDirectoryService::Has(const char *prop, bool *_retval)
{
    if (NS_WARN_IF(!prop))
        return NS_ERROR_INVALID_ARG;

    *_retval = false;
    nsCOMPtr<nsIFile> value;
    nsresult rv = Get(prop, NS_GET_IID(nsIFile), getter_AddRefs(value));
    if (NS_FAILED(rv))
        return NS_OK;
    
    if (value)
    {
        *_retval = true;
    }
    
    return rv;
}

NS_IMETHODIMP
nsDirectoryService::RegisterProvider(nsIDirectoryServiceProvider *prov)
{
    if (!prov)
        return NS_ERROR_FAILURE;

    mProviders.AppendElement(prov);
    return NS_OK;
}

void
nsDirectoryService::RegisterCategoryProviders()
{
    nsCOMPtr<nsICategoryManager> catman
        (do_GetService(NS_CATEGORYMANAGER_CONTRACTID));
    if (!catman)
        return;

    nsCOMPtr<nsISimpleEnumerator> entries;
    catman->EnumerateCategory(XPCOM_DIRECTORY_PROVIDER_CATEGORY,
                              getter_AddRefs(entries));

    nsCOMPtr<nsIUTF8StringEnumerator> strings(do_QueryInterface(entries));
    if (!strings)
        return;

    bool more;
    while (NS_SUCCEEDED(strings->HasMore(&more)) && more) {
        nsAutoCString entry;
        strings->GetNext(entry);

        nsXPIDLCString contractID;
        catman->GetCategoryEntry(XPCOM_DIRECTORY_PROVIDER_CATEGORY, entry.get(), getter_Copies(contractID));

        if (contractID) {
            nsCOMPtr<nsIDirectoryServiceProvider> provider = do_GetService(contractID.get());
            if (provider)
                RegisterProvider(provider);
        }
    }
}

NS_IMETHODIMP
nsDirectoryService::UnregisterProvider(nsIDirectoryServiceProvider *prov)
{
    if (!prov)
        return NS_ERROR_FAILURE;

    mProviders.RemoveElement(prov);
    return NS_OK;
}

// DO NOT ADD ANY LOCATIONS TO THIS FUNCTION UNTIL YOU TALK TO: dougt@netscape.com.
// This is meant to be a place of xpcom or system specific file locations, not
// application specific locations.  If you need the later, register a callback for
// your application.  

NS_IMETHODIMP
nsDirectoryService::GetFile(const char *prop, bool *persistent, nsIFile **_retval)
{
    nsCOMPtr<nsIFile> localFile;
    nsresult rv = NS_ERROR_FAILURE;

    *_retval = nullptr;
    *persistent = true;

    nsCOMPtr<nsIAtom> inAtom = do_GetAtom(prop);

    // check to see if it is one of our defaults
        
    if (inAtom == nsDirectoryService::sCurrentProcess || 
        inAtom == nsDirectoryService::sOS_CurrentProcessDirectory )
    {
        rv = GetCurrentProcessDirectory(getter_AddRefs(localFile));
    }
    
    // Unless otherwise set, the core pieces of the GRE exist
    // in the current process directory.
    else if (inAtom == nsDirectoryService::sGRE_Directory)
    {
        rv = GetCurrentProcessDirectory(getter_AddRefs(localFile));
    }
    else if (inAtom == nsDirectoryService::sOS_DriveDirectory)
    {
        rv = GetSpecialSystemDirectory(OS_DriveDirectory, getter_AddRefs(localFile));
    }
    else if (inAtom == nsDirectoryService::sOS_TemporaryDirectory)
    {
        rv = GetSpecialSystemDirectory(OS_TemporaryDirectory, getter_AddRefs(localFile));
    }
    else if (inAtom == nsDirectoryService::sOS_CurrentProcessDirectory)
    {
        rv = GetSpecialSystemDirectory(OS_CurrentProcessDirectory, getter_AddRefs(localFile)); 
    }
    else if (inAtom == nsDirectoryService::sOS_CurrentWorkingDirectory)
    {
        rv = GetSpecialSystemDirectory(OS_CurrentWorkingDirectory, getter_AddRefs(localFile)); 
    }
       
#if defined(MOZ_WIDGET_COCOA)
    else if (inAtom == nsDirectoryService::sDirectory)
    {
        rv = GetOSXFolderType(kClassicDomain, kSystemFolderType, getter_AddRefs(localFile));
    }
    else if (inAtom == nsDirectoryService::sTrashDirectory)
    {
        rv = GetOSXFolderType(kClassicDomain, kTrashFolderType, getter_AddRefs(localFile));
    }
    else if (inAtom == nsDirectoryService::sStartupDirectory)
    {
        rv = GetOSXFolderType(kClassicDomain, kStartupFolderType, getter_AddRefs(localFile));
    }
    else if (inAtom == nsDirectoryService::sShutdownDirectory)
    {
        rv = GetOSXFolderType(kClassicDomain, kShutdownFolderType, getter_AddRefs(localFile));
    }
    else if (inAtom == nsDirectoryService::sAppleMenuDirectory)
    {
        rv = GetOSXFolderType(kClassicDomain, kAppleMenuFolderType, getter_AddRefs(localFile));
    }
    else if (inAtom == nsDirectoryService::sControlPanelDirectory)
    {
        rv = GetOSXFolderType(kClassicDomain, kControlPanelFolderType, getter_AddRefs(localFile));
    }
    else if (inAtom == nsDirectoryService::sExtensionDirectory)
    {
        rv = GetOSXFolderType(kClassicDomain, kExtensionFolderType, getter_AddRefs(localFile));
    }
    else if (inAtom == nsDirectoryService::sFontsDirectory)
    {
        rv = GetOSXFolderType(kClassicDomain, kFontsFolderType, getter_AddRefs(localFile));
    }
    else if (inAtom == nsDirectoryService::sPreferencesDirectory)
    {
        rv = GetOSXFolderType(kClassicDomain, kPreferencesFolderType, getter_AddRefs(localFile));
    }
    else if (inAtom == nsDirectoryService::sDocumentsDirectory)
    {
        rv = GetOSXFolderType(kClassicDomain, kDocumentsFolderType, getter_AddRefs(localFile));
    }
    else if (inAtom == nsDirectoryService::sInternetSearchDirectory)
    {
        rv = GetOSXFolderType(kClassicDomain, kInternetSearchSitesFolderType, getter_AddRefs(localFile));
    }   
    else if (inAtom == nsDirectoryService::sUserLibDirectory)
    {
        rv = GetOSXFolderType(kUserDomain, kDomainLibraryFolderType, getter_AddRefs(localFile));
    }
    else if (inAtom == nsDirectoryService::sOS_HomeDirectory)
    {
        rv = GetOSXFolderType(kUserDomain, kDomainTopLevelFolderType, getter_AddRefs(localFile));
    }
    else if (inAtom == nsDirectoryService::sDefaultDownloadDirectory)
    {
        // 10.5 and later, we can use kDownloadsFolderType which is defined in
        // Folders.h as "down".  However, in order to support 10.4 still, we
        // cannot use the named constant.  We'll use it's value, and if it
        // fails, fall back to the desktop.
#ifndef kDownloadsFolderType
#define kDownloadsFolderType 'down'
#endif

        rv = GetOSXFolderType(kUserDomain, kDownloadsFolderType,
                              getter_AddRefs(localFile));
        if (NS_FAILED(rv)) {
            rv = GetOSXFolderType(kUserDomain, kDesktopFolderType,
                                  getter_AddRefs(localFile));
        }
    }
    else if (inAtom == nsDirectoryService::sUserDesktopDirectory ||
             inAtom == nsDirectoryService::sOS_DesktopDirectory)
    {
        rv = GetOSXFolderType(kUserDomain, kDesktopFolderType, getter_AddRefs(localFile));
    }
    else if (inAtom == nsDirectoryService::sLocalDesktopDirectory)
    {
        rv = GetOSXFolderType(kLocalDomain, kDesktopFolderType, getter_AddRefs(localFile));
    }
    else if (inAtom == nsDirectoryService::sUserApplicationsDirectory)
    {
        rv = GetOSXFolderType(kUserDomain, kApplicationsFolderType, getter_AddRefs(localFile));
    }
    else if (inAtom == nsDirectoryService::sLocalApplicationsDirectory)
    {
        rv = GetOSXFolderType(kLocalDomain, kApplicationsFolderType, getter_AddRefs(localFile));
    }
    else if (inAtom == nsDirectoryService::sUserDocumentsDirectory)
    {
        rv = GetOSXFolderType(kUserDomain, kDocumentsFolderType, getter_AddRefs(localFile));
    }
    else if (inAtom == nsDirectoryService::sLocalDocumentsDirectory)
    {
        rv = GetOSXFolderType(kLocalDomain, kDocumentsFolderType, getter_AddRefs(localFile));
    }
    else if (inAtom == nsDirectoryService::sUserInternetPlugInDirectory)
    {
        rv = GetOSXFolderType(kUserDomain, kInternetPlugInFolderType, getter_AddRefs(localFile));
    }
    else if (inAtom == nsDirectoryService::sLocalInternetPlugInDirectory)
    {
        rv = GetOSXFolderType(kLocalDomain, kInternetPlugInFolderType, getter_AddRefs(localFile));
    }
    else if (inAtom == nsDirectoryService::sUserFrameworksDirectory)
    {
        rv = GetOSXFolderType(kUserDomain, kFrameworksFolderType, getter_AddRefs(localFile));
    }
    else if (inAtom == nsDirectoryService::sLocalFrameworksDirectory)
    {
        rv = GetOSXFolderType(kLocalDomain, kFrameworksFolderType, getter_AddRefs(localFile));
    }
    else if (inAtom == nsDirectoryService::sUserPreferencesDirectory)
    {
        rv = GetOSXFolderType(kUserDomain, kPreferencesFolderType, getter_AddRefs(localFile));
    }
    else if (inAtom == nsDirectoryService::sLocalPreferencesDirectory)
    {
        rv = GetOSXFolderType(kLocalDomain, kPreferencesFolderType, getter_AddRefs(localFile));
    }
    else if (inAtom == nsDirectoryService::sPictureDocumentsDirectory)
    {
        rv = GetOSXFolderType(kUserDomain, kPictureDocumentsFolderType, getter_AddRefs(localFile));
    }
    else if (inAtom == nsDirectoryService::sMovieDocumentsDirectory)
    {
        rv = GetOSXFolderType(kUserDomain, kMovieDocumentsFolderType, getter_AddRefs(localFile));
    }
    else if (inAtom == nsDirectoryService::sMusicDocumentsDirectory)
    {
        rv = GetOSXFolderType(kUserDomain, kMusicDocumentsFolderType, getter_AddRefs(localFile));
    }
    else if (inAtom == nsDirectoryService::sInternetSitesDirectory)
    {
        rv = GetOSXFolderType(kUserDomain, kInternetSitesFolderType, getter_AddRefs(localFile));
    }
#elif defined (XP_WIN)
    else if (inAtom == nsDirectoryService::sSystemDirectory)
    {
        rv = GetSpecialSystemDirectory(Win_SystemDirectory, getter_AddRefs(localFile)); 
    }
    else if (inAtom == nsDirectoryService::sWindowsDirectory)
    {
        rv = GetSpecialSystemDirectory(Win_WindowsDirectory, getter_AddRefs(localFile)); 
    }
    else if (inAtom == nsDirectoryService::sWindowsProgramFiles)
    {
        rv = GetSpecialSystemDirectory(Win_ProgramFiles, getter_AddRefs(localFile));
    }
    else if (inAtom == nsDirectoryService::sOS_HomeDirectory)
    {
        rv = GetSpecialSystemDirectory(Win_HomeDirectory, getter_AddRefs(localFile)); 
    }
    else if (inAtom == nsDirectoryService::sDesktop)
    {
        rv = GetSpecialSystemDirectory(Win_Desktop, getter_AddRefs(localFile)); 
    }
    else if (inAtom == nsDirectoryService::sPrograms)
    {
        rv = GetSpecialSystemDirectory(Win_Programs, getter_AddRefs(localFile)); 
    }
    else if (inAtom == nsDirectoryService::sControls)
    {
        rv = GetSpecialSystemDirectory(Win_Controls, getter_AddRefs(localFile)); 
    }
    else if (inAtom == nsDirectoryService::sPrinters)
    {
        rv = GetSpecialSystemDirectory(Win_Printers, getter_AddRefs(localFile)); 
    }
    else if (inAtom == nsDirectoryService::sPersonal)
    {
        rv = GetSpecialSystemDirectory(Win_Personal, getter_AddRefs(localFile)); 
    }
    else if (inAtom == nsDirectoryService::sFavorites)
    {
        rv = GetSpecialSystemDirectory(Win_Favorites, getter_AddRefs(localFile)); 
    }
    else if (inAtom == nsDirectoryService::sStartup)
    {
        rv = GetSpecialSystemDirectory(Win_Startup, getter_AddRefs(localFile));
    }
    else if (inAtom == nsDirectoryService::sRecent)
    {
        rv = GetSpecialSystemDirectory(Win_Recent, getter_AddRefs(localFile)); 
    }
    else if (inAtom == nsDirectoryService::sSendto)
    {
        rv = GetSpecialSystemDirectory(Win_Sendto, getter_AddRefs(localFile)); 
    }
    else if (inAtom == nsDirectoryService::sBitbucket)
    {
        rv = GetSpecialSystemDirectory(Win_Bitbucket, getter_AddRefs(localFile)); 
    }
    else if (inAtom == nsDirectoryService::sStartmenu)
    {
        rv = GetSpecialSystemDirectory(Win_Startmenu, getter_AddRefs(localFile)); 
    }
    else if (inAtom == nsDirectoryService::sDesktopdirectory ||
             inAtom == nsDirectoryService::sOS_DesktopDirectory)
    {
        rv = GetSpecialSystemDirectory(Win_Desktopdirectory, getter_AddRefs(localFile)); 
    }
    else if (inAtom == nsDirectoryService::sDrives)
    {
        rv = GetSpecialSystemDirectory(Win_Drives, getter_AddRefs(localFile));
    }
    else if (inAtom == nsDirectoryService::sNetwork)
    {
        rv = GetSpecialSystemDirectory(Win_Network, getter_AddRefs(localFile)); 
    }
    else if (inAtom == nsDirectoryService::sNethood)
    {
        rv = GetSpecialSystemDirectory(Win_Nethood, getter_AddRefs(localFile)); 
    }
    else if (inAtom == nsDirectoryService::sFonts)
    {
        rv = GetSpecialSystemDirectory(Win_Fonts, getter_AddRefs(localFile));
    }
    else if (inAtom == nsDirectoryService::sTemplates)
    {
        rv = GetSpecialSystemDirectory(Win_Templates, getter_AddRefs(localFile)); 
    }
    else if (inAtom == nsDirectoryService::sCommon_Startmenu)
    {
        rv = GetSpecialSystemDirectory(Win_Common_Startmenu, getter_AddRefs(localFile)); 
    }
    else if (inAtom == nsDirectoryService::sCommon_Programs)
    {
        rv = GetSpecialSystemDirectory(Win_Common_Programs, getter_AddRefs(localFile)); 
    }
    else if (inAtom == nsDirectoryService::sCommon_Startup)
    {
        rv = GetSpecialSystemDirectory(Win_Common_Startup, getter_AddRefs(localFile)); 
    }
    else if (inAtom == nsDirectoryService::sCommon_Desktopdirectory)
    {
        rv = GetSpecialSystemDirectory(Win_Common_Desktopdirectory, getter_AddRefs(localFile)); 
    }
    else if (inAtom == nsDirectoryService::sCommon_AppData)
    {
        rv = GetSpecialSystemDirectory(Win_Common_AppData, getter_AddRefs(localFile)); 
    }
    else if (inAtom == nsDirectoryService::sAppdata)
    {
        rv = GetSpecialSystemDirectory(Win_Appdata, getter_AddRefs(localFile)); 
    }
    else if (inAtom == nsDirectoryService::sLocalAppdata)
    {
        rv = GetSpecialSystemDirectory(Win_LocalAppdata, getter_AddRefs(localFile)); 
    }
    else if (inAtom == nsDirectoryService::sPrinthood)
    {
        rv = GetSpecialSystemDirectory(Win_Printhood, getter_AddRefs(localFile)); 
    }
    else if (inAtom == nsDirectoryService::sWinCookiesDirectory)
    {
        rv = GetSpecialSystemDirectory(Win_Cookies, getter_AddRefs(localFile)); 
    }
    else if (inAtom == nsDirectoryService::sDefaultDownloadDirectory)
    {
        rv = GetSpecialSystemDirectory(Win_Downloads, getter_AddRefs(localFile));
    }
    else if (inAtom == nsDirectoryService::sDocs)
    {
        rv = GetSpecialSystemDirectory(Win_Documents, getter_AddRefs(localFile));
    }
    else if (inAtom == nsDirectoryService::sPictures)
    {
        rv = GetSpecialSystemDirectory(Win_Pictures, getter_AddRefs(localFile));
    }
    else if (inAtom == nsDirectoryService::sMusic)
    {
        rv = GetSpecialSystemDirectory(Win_Music, getter_AddRefs(localFile));
    }
    else if (inAtom == nsDirectoryService::sVideos)
    {
        rv = GetSpecialSystemDirectory(Win_Videos, getter_AddRefs(localFile));
    }
#elif defined (XP_UNIX)

    else if (inAtom == nsDirectoryService::sLocalDirectory)
    {
        rv = GetSpecialSystemDirectory(Unix_LocalDirectory, getter_AddRefs(localFile)); 
    }
    else if (inAtom == nsDirectoryService::sLibDirectory)
    {
        rv = GetSpecialSystemDirectory(Unix_LibDirectory, getter_AddRefs(localFile));
    }
    else if (inAtom == nsDirectoryService::sOS_HomeDirectory)
    {
        rv = GetSpecialSystemDirectory(Unix_HomeDirectory, getter_AddRefs(localFile)); 
    }
    else if (inAtom == nsDirectoryService::sXDGDesktop ||
             inAtom == nsDirectoryService::sOS_DesktopDirectory)
    {
        rv = GetSpecialSystemDirectory(Unix_XDG_Desktop, getter_AddRefs(localFile));
        *persistent = false;
    }
    else if (inAtom == nsDirectoryService::sXDGDocuments)
    {
        rv = GetSpecialSystemDirectory(Unix_XDG_Documents, getter_AddRefs(localFile));
        *persistent = false;
    }
    else if (inAtom == nsDirectoryService::sXDGDownload ||
             inAtom == nsDirectoryService::sDefaultDownloadDirectory)
    {
        rv = GetSpecialSystemDirectory(Unix_XDG_Download, getter_AddRefs(localFile));
        *persistent = false;
    }
    else if (inAtom == nsDirectoryService::sXDGMusic)
    {
        rv = GetSpecialSystemDirectory(Unix_XDG_Music, getter_AddRefs(localFile));
        *persistent = false;
    }
    else if (inAtom == nsDirectoryService::sXDGPictures)
    {
        rv = GetSpecialSystemDirectory(Unix_XDG_Pictures, getter_AddRefs(localFile));
        *persistent = false;
    }
    else if (inAtom == nsDirectoryService::sXDGPublicShare)
    {
        rv = GetSpecialSystemDirectory(Unix_XDG_PublicShare, getter_AddRefs(localFile));
        *persistent = false;
    }
    else if (inAtom == nsDirectoryService::sXDGTemplates)
    {
        rv = GetSpecialSystemDirectory(Unix_XDG_Templates, getter_AddRefs(localFile));
        *persistent = false;
    }
    else if (inAtom == nsDirectoryService::sXDGVideos)
    {
        rv = GetSpecialSystemDirectory(Unix_XDG_Videos, getter_AddRefs(localFile));
        *persistent = false;
    }
#elif defined (XP_OS2)
    else if (inAtom == nsDirectoryService::sSystemDirectory)
    {
        rv = GetSpecialSystemDirectory(OS2_SystemDirectory, getter_AddRefs(localFile)); 
    }
    else if (inAtom == nsDirectoryService::sOS2Directory)
    {
        rv = GetSpecialSystemDirectory(OS2_OS2Directory, getter_AddRefs(localFile)); 
    }
    else if (inAtom == nsDirectoryService::sOS_HomeDirectory)
    {
        rv = GetSpecialSystemDirectory(OS2_HomeDirectory, getter_AddRefs(localFile)); 
    }
    else if (inAtom == nsDirectoryService::sOS_DesktopDirectory)
    {
        rv = GetSpecialSystemDirectory(OS2_DesktopDirectory, getter_AddRefs(localFile)); 
    }
#endif

    if (NS_FAILED(rv))
        return rv;

    if (!localFile)
        return NS_ERROR_FAILURE;

    return CallQueryInterface(localFile, _retval);
}

NS_IMETHODIMP
nsDirectoryService::GetFiles(const char *prop, nsISimpleEnumerator **_retval)
{
    if (NS_WARN_IF(!_retval))
        return NS_ERROR_INVALID_ARG;
    *_retval = nullptr;
        
    return NS_ERROR_FAILURE;
}
