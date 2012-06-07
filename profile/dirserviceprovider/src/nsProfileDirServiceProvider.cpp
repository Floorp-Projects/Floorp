/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsProfileDirServiceProvider.h"
#include "nsProfileStringTypes.h"
#include "nsProfileLock.h"
#include "nsIFile.h"
#include "nsDirectoryServiceDefs.h"
#include "nsAppDirectoryServiceDefs.h"
#include "nsISupportsUtils.h"
#include "nsISimpleEnumerator.h"
#include "nsIObserverService.h"

// File Name Defines

#define PREFS_FILE_50_NAME           NS_LITERAL_CSTRING("prefs.js")
#define USER_CHROME_DIR_50_NAME      NS_LITERAL_CSTRING("chrome")
#define LOCAL_STORE_FILE_50_NAME     NS_LITERAL_CSTRING("localstore.rdf")
#define PANELS_FILE_50_NAME          NS_LITERAL_CSTRING("panels.rdf")
#define MIME_TYPES_FILE_50_NAME      NS_LITERAL_CSTRING("mimeTypes.rdf")
#define BOOKMARKS_FILE_50_NAME       NS_LITERAL_CSTRING("bookmarks.html")
#define DOWNLOADS_FILE_50_NAME       NS_LITERAL_CSTRING("downloads.rdf")
#define SEARCH_FILE_50_NAME          NS_LITERAL_CSTRING("search.rdf" )
#define STORAGE_FILE_50_NAME         NS_LITERAL_CSTRING("storage.sdb")

//*****************************************************************************
// nsProfileDirServiceProvider::nsProfileDirServiceProvider
//*****************************************************************************

nsProfileDirServiceProvider::nsProfileDirServiceProvider(bool aNotifyObservers) :
#ifdef MOZ_PROFILELOCKING
  mProfileDirLock(nsnull),
#endif
  mNotifyObservers(aNotifyObservers),
  mSharingEnabled(false)
{
}


nsProfileDirServiceProvider::~nsProfileDirServiceProvider()
{
#ifdef MOZ_PROFILELOCKING
  delete mProfileDirLock;
#endif
}

nsresult
nsProfileDirServiceProvider::SetProfileDir(nsIFile* aProfileDir,
                                           nsIFile* aLocalProfileDir)
{
  if (!aLocalProfileDir)
    aLocalProfileDir = aProfileDir;
  if (mProfileDir) {
    bool isEqual;
    if (aProfileDir &&
        NS_SUCCEEDED(aProfileDir->Equals(mProfileDir, &isEqual)) && isEqual) {
      NS_WARNING("Setting profile dir to same as current");
      return NS_OK;
    }
#ifdef MOZ_PROFILELOCKING
    mProfileDirLock->Unlock();
#endif
    UndefineFileLocations();
  }
  mProfileDir = aProfileDir;
  mLocalProfileDir = aLocalProfileDir;
  if (!mProfileDir)
    return NS_OK;

  nsresult rv = InitProfileDir(mProfileDir);
  if (NS_FAILED(rv))
    return rv;

  // Make sure that the local profile dir exists
  // we just try to create it - if it exists already, that'll fail; ignore
  // errors
  mLocalProfileDir->Create(nsIFile::DIRECTORY_TYPE, 0700);

#ifdef MOZ_PROFILELOCKING
  // Lock the non-shared sub-dir if we are sharing,
  // the whole profile dir if we are not.
  nsCOMPtr<nsIFile> dirToLock;
  if (mSharingEnabled)
    dirToLock = mNonSharedProfileDir;
  else
    dirToLock = mProfileDir;
  rv = mProfileDirLock->Lock(dirToLock, nsnull);
  if (NS_FAILED(rv))
    return rv;
#endif

  if (mNotifyObservers) {
    nsCOMPtr<nsIObserverService> observerService =
             do_GetService("@mozilla.org/observer-service;1");
    if (!observerService)
      return NS_ERROR_FAILURE;

    NS_NAMED_LITERAL_STRING(context, "startup");
    // Notify observers that the profile has changed - Here they respond to new profile
    observerService->NotifyObservers(nsnull, "profile-do-change", context.get());
    // Now observers can respond to something another observer did on "profile-do-change"
    observerService->NotifyObservers(nsnull, "profile-after-change", context.get());
  }

  return NS_OK;
}

nsresult
nsProfileDirServiceProvider::Register()
{
  nsCOMPtr<nsIDirectoryService> directoryService =
          do_GetService(NS_DIRECTORY_SERVICE_CONTRACTID);
  if (!directoryService)
    return NS_ERROR_FAILURE;
  return directoryService->RegisterProvider(this);
}

nsresult
nsProfileDirServiceProvider::Shutdown()
{
  if (!mNotifyObservers)
    return NS_OK;

  nsCOMPtr<nsIObserverService> observerService =
           do_GetService("@mozilla.org/observer-service;1");
  if (!observerService)
    return NS_ERROR_FAILURE;

  NS_NAMED_LITERAL_STRING(context, "shutdown-persist");
  observerService->NotifyObservers(nsnull, "profile-before-change", context.get());
  return NS_OK;
}

//*****************************************************************************
// nsProfileDirServiceProvider::nsISupports
//*****************************************************************************

NS_IMPL_ISUPPORTS1(nsProfileDirServiceProvider,
                   nsIDirectoryServiceProvider)

//*****************************************************************************
// nsProfileDirServiceProvider::nsIDirectoryServiceProvider
//*****************************************************************************

NS_IMETHODIMP
nsProfileDirServiceProvider::GetFile(const char *prop, bool *persistant, nsIFile **_retval)
{
  NS_ENSURE_ARG(prop);
  NS_ENSURE_ARG_POINTER(persistant);
  NS_ENSURE_ARG_POINTER(_retval);

  // Don't assert - we can be called many times before SetProfileDir() has been called.
  if (!mProfileDir)
    return NS_ERROR_FAILURE;

  *persistant = true;
  nsIFile* domainDir = mProfileDir;

  nsCOMPtr<nsIFile>  localFile;
  nsresult rv = NS_ERROR_FAILURE;

  if (strcmp(prop, NS_APP_PREFS_50_DIR) == 0) {
    rv = domainDir->Clone(getter_AddRefs(localFile));
  }
  else if (strcmp(prop, NS_APP_PREFS_50_FILE) == 0) {
    rv = domainDir->Clone(getter_AddRefs(localFile));
    if (NS_SUCCEEDED(rv))
      rv = localFile->AppendNative(PREFS_FILE_50_NAME);
  }
  else if (strcmp(prop, NS_APP_USER_PROFILE_50_DIR) == 0) {
    rv = domainDir->Clone(getter_AddRefs(localFile));
  }
  else if (strcmp(prop, NS_APP_USER_PROFILE_LOCAL_50_DIR) == 0) {
    rv = mLocalProfileDir->Clone(getter_AddRefs(localFile));
  }
  else if (strcmp(prop, NS_APP_USER_CHROME_DIR) == 0) {
    rv = domainDir->Clone(getter_AddRefs(localFile));
    if (NS_SUCCEEDED(rv))
      rv = localFile->AppendNative(USER_CHROME_DIR_50_NAME);
  }
  else if (strcmp(prop, NS_APP_LOCALSTORE_50_FILE) == 0) {
    rv = domainDir->Clone(getter_AddRefs(localFile));
    if (NS_SUCCEEDED(rv)) {
      rv = localFile->AppendNative(LOCAL_STORE_FILE_50_NAME);
      if (NS_SUCCEEDED(rv)) {
        // it's OK if we can't copy the file... it will be created
        // by client code.
        (void) EnsureProfileFileExists(localFile, domainDir);
      }
    }
  }
  else if (strcmp(prop, NS_APP_USER_PANELS_50_FILE) == 0) {
    rv = domainDir->Clone(getter_AddRefs(localFile));
    if (NS_SUCCEEDED(rv)) {
      rv = localFile->AppendNative(PANELS_FILE_50_NAME);
      if (NS_SUCCEEDED(rv))
        rv = EnsureProfileFileExists(localFile, domainDir);
    }
  }
  else if (strcmp(prop, NS_APP_USER_MIMETYPES_50_FILE) == 0) {
    rv = domainDir->Clone(getter_AddRefs(localFile));
    if (NS_SUCCEEDED(rv)) {
      rv = localFile->AppendNative(MIME_TYPES_FILE_50_NAME);
      if (NS_SUCCEEDED(rv))
        rv = EnsureProfileFileExists(localFile, domainDir);
    }
  }
  else if (strcmp(prop, NS_APP_BOOKMARKS_50_FILE) == 0) {
    rv = domainDir->Clone(getter_AddRefs(localFile));
    if (NS_SUCCEEDED(rv))
      rv = localFile->AppendNative(BOOKMARKS_FILE_50_NAME);
  }
  else if (strcmp(prop, NS_APP_DOWNLOADS_50_FILE) == 0) {
    rv = domainDir->Clone(getter_AddRefs(localFile));
    if (NS_SUCCEEDED(rv))
      rv = localFile->AppendNative(DOWNLOADS_FILE_50_NAME);
  }
  else if (strcmp(prop, NS_APP_SEARCH_50_FILE) == 0) {
    rv = domainDir->Clone(getter_AddRefs(localFile));
    if (NS_SUCCEEDED(rv)) {
      rv = localFile->AppendNative(SEARCH_FILE_50_NAME);
      if (NS_SUCCEEDED(rv))
        rv = EnsureProfileFileExists(localFile, domainDir);
    }
  }
  else if (strcmp(prop, NS_APP_STORAGE_50_FILE) == 0) {
    rv = domainDir->Clone(getter_AddRefs(localFile));
    if (NS_SUCCEEDED(rv))
      rv = localFile->AppendNative(STORAGE_FILE_50_NAME);
  }

  
  if (localFile && NS_SUCCEEDED(rv))
    return CallQueryInterface(localFile, _retval);

  return rv;
}

//*****************************************************************************
// Protected methods
//*****************************************************************************

nsresult
nsProfileDirServiceProvider::Initialize()
{
#ifdef MOZ_PROFILELOCKING
  mProfileDirLock = new nsProfileLock;
  if (!mProfileDirLock)
    return NS_ERROR_OUT_OF_MEMORY;
#endif

  return NS_OK;
}

nsresult
nsProfileDirServiceProvider::InitProfileDir(nsIFile *profileDir)
{
  // Make sure our "Profile" folder exists.
  // If it does not, copy the profile defaults to its location.

  nsresult rv;
  bool exists;
  rv = profileDir->Exists(&exists);
  if (NS_FAILED(rv))
    return rv;

  if (!exists) {
    nsCOMPtr<nsIFile> profileDefaultsDir;
    nsCOMPtr<nsIFile> profileDirParent;
    nsCAutoString profileDirName;

    (void)profileDir->GetParent(getter_AddRefs(profileDirParent));
    if (!profileDirParent)
      return NS_ERROR_FAILURE;
    rv = profileDir->GetNativeLeafName(profileDirName);
    if (NS_FAILED(rv))
      return rv;

    rv = NS_GetSpecialDirectory(NS_APP_PROFILE_DEFAULTS_50_DIR, getter_AddRefs(profileDefaultsDir));
    if (NS_FAILED(rv)) {
      rv = NS_GetSpecialDirectory(NS_APP_PROFILE_DEFAULTS_NLOC_50_DIR, getter_AddRefs(profileDefaultsDir));
      if (NS_FAILED(rv))
        return rv;
    }
    rv = profileDefaultsDir->CopyToNative(profileDirParent, profileDirName);
    if (NS_FAILED(rv)) {
        // if copying failed, lets just ensure that the profile directory exists.
        profileDirParent->AppendNative(profileDirName);
        rv = profileDirParent->Create(nsIFile::DIRECTORY_TYPE, 0700);
        if (NS_FAILED(rv))
            return rv;
    }

    rv = profileDir->SetPermissions(0700);
    if (NS_FAILED(rv))
      return rv;
  }
  else {
    bool isDir;
    rv = profileDir->IsDirectory(&isDir);

    if (NS_FAILED(rv))
      return rv;
    if (!isDir)
      return NS_ERROR_FILE_NOT_DIRECTORY;
  }

  if (mNonSharedDirName.Length())
    rv = InitNonSharedProfileDir();

  return rv;
}

nsresult
nsProfileDirServiceProvider::InitNonSharedProfileDir()
{
  nsresult rv;

  NS_ENSURE_STATE(mProfileDir);
  NS_ENSURE_STATE(mNonSharedDirName.Length());

  nsCOMPtr<nsIFile> localDir;
  rv = mProfileDir->Clone(getter_AddRefs(localDir));
  if (NS_SUCCEEDED(rv)) {
    rv = localDir->Append(mNonSharedDirName);
    if (NS_SUCCEEDED(rv)) {
      bool exists;
      rv = localDir->Exists(&exists);
      if (NS_SUCCEEDED(rv)) {
        if (!exists) {
          rv = localDir->Create(nsIFile::DIRECTORY_TYPE, 0700);
        }
        else {
          bool isDir;
          rv = localDir->IsDirectory(&isDir);
          if (NS_SUCCEEDED(rv)) {
            if (!isDir)
              rv = NS_ERROR_FILE_NOT_DIRECTORY;
          }
        }
        if (NS_SUCCEEDED(rv))
          mNonSharedProfileDir = localDir;
      }
    }
  }
  return rv;
}

nsresult
nsProfileDirServiceProvider::EnsureProfileFileExists(nsIFile *aFile, nsIFile *destDir)
{
  nsresult rv;
  bool exists;

  rv = aFile->Exists(&exists);
  if (NS_FAILED(rv))
    return rv;
  if (exists)
    return NS_OK;

  nsCOMPtr<nsIFile> defaultsFile;

  // Attempt first to get the localized subdir of the defaults
  rv = NS_GetSpecialDirectory(NS_APP_PROFILE_DEFAULTS_50_DIR, getter_AddRefs(defaultsFile));
  if (NS_FAILED(rv)) {
    // If that has not been defined, use the top level of the defaults
    rv = NS_GetSpecialDirectory(NS_APP_PROFILE_DEFAULTS_NLOC_50_DIR, getter_AddRefs(defaultsFile));
    if (NS_FAILED(rv))
      return rv;
  }

  nsCAutoString leafName;
  rv = aFile->GetNativeLeafName(leafName);
  if (NS_FAILED(rv))
    return rv;
  rv = defaultsFile->AppendNative(leafName);
  if (NS_FAILED(rv))
    return rv;
  
  return defaultsFile->CopyTo(destDir, EmptyString());
}

nsresult
nsProfileDirServiceProvider::UndefineFileLocations()
{
  nsresult rv;

  nsCOMPtr<nsIProperties> directoryService =
           do_GetService(NS_DIRECTORY_SERVICE_CONTRACTID, &rv);
  NS_ENSURE_TRUE(directoryService, NS_ERROR_FAILURE);

  (void) directoryService->Undefine(NS_APP_PREFS_50_DIR);
  (void) directoryService->Undefine(NS_APP_PREFS_50_FILE);
  (void) directoryService->Undefine(NS_APP_USER_PROFILE_50_DIR);
  (void) directoryService->Undefine(NS_APP_USER_CHROME_DIR);
  (void) directoryService->Undefine(NS_APP_LOCALSTORE_50_FILE);
  (void) directoryService->Undefine(NS_APP_USER_PANELS_50_FILE);
  (void) directoryService->Undefine(NS_APP_USER_MIMETYPES_50_FILE);
  (void) directoryService->Undefine(NS_APP_BOOKMARKS_50_FILE);
  (void) directoryService->Undefine(NS_APP_DOWNLOADS_50_FILE);
  (void) directoryService->Undefine(NS_APP_SEARCH_50_FILE);

  return NS_OK;
}

//*****************************************************************************
// Global creation function
//*****************************************************************************

nsresult NS_NewProfileDirServiceProvider(bool aNotifyObservers,
                                         nsProfileDirServiceProvider** aProvider)
{
  NS_ENSURE_ARG_POINTER(aProvider);
  *aProvider = nsnull;

  nsProfileDirServiceProvider *prov = new nsProfileDirServiceProvider(aNotifyObservers);
  if (!prov)
    return NS_ERROR_OUT_OF_MEMORY;
  nsresult rv = prov->Initialize();
  if (NS_FAILED(rv)) {
    delete prov;
    return rv;
  }
  NS_ADDREF(*aProvider = prov);
  return NS_OK;
}
