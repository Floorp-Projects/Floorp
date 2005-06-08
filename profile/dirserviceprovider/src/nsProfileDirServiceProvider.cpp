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
 *   Conrad Carlen <ccarlen@netscape.com>
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

#include "nsProfileDirServiceProvider.h"
#include "nsProfileStringTypes.h"
#include "nsProfileLock.h"
#include "nsILocalFile.h"
#include "nsDirectoryServiceDefs.h"
#include "nsAppDirectoryServiceDefs.h"
#include "nsISupportsUtils.h"
#include "nsISimpleEnumerator.h"
#include "nsIObserverService.h"

#ifdef MOZ_PROFILESHARING
#include "nsIProfileSharingSetup.h"
#include "ipcITransactionService.h"
#endif

// File Name Defines

#define PREFS_FILE_50_NAME           NS_LITERAL_CSTRING("prefs.js")
#define USER_CHROME_DIR_50_NAME      NS_LITERAL_CSTRING("chrome")
#define LOCAL_STORE_FILE_50_NAME     NS_LITERAL_CSTRING("localstore.rdf")
#define HISTORY_FILE_50_NAME         NS_LITERAL_CSTRING("history.dat")
#define PANELS_FILE_50_NAME          NS_LITERAL_CSTRING("panels.rdf")
#define MIME_TYPES_FILE_50_NAME      NS_LITERAL_CSTRING("mimeTypes.rdf")
#define BOOKMARKS_FILE_50_NAME       NS_LITERAL_CSTRING("bookmarks.html")
#define DOWNLOADS_FILE_50_NAME       NS_LITERAL_CSTRING("downloads.rdf")
#define SEARCH_FILE_50_NAME          NS_LITERAL_CSTRING("search.rdf" )
#define MAIL_DIR_50_NAME             NS_LITERAL_CSTRING("Mail")
#define IMAP_MAIL_DIR_50_NAME        NS_LITERAL_CSTRING("ImapMail")
#define NEWS_DIR_50_NAME             NS_LITERAL_CSTRING("News")
#define MSG_FOLDER_CACHE_DIR_50_NAME NS_LITERAL_CSTRING("panacea.dat")
#define STORAGE_FILE_50_NAME         NS_LITERAL_CSTRING("storage.sdb")

//*****************************************************************************
// nsProfileDirServiceProvider::nsProfileDirServiceProvider
//*****************************************************************************

nsProfileDirServiceProvider::nsProfileDirServiceProvider(PRBool aNotifyObservers) :
#ifdef MOZ_PROFILELOCKING
  mProfileDirLock(nsnull),
#endif
  mNotifyObservers(aNotifyObservers),
  mSharingEnabled(PR_FALSE)
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
    PRBool isEqual;
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

#ifdef MOZ_PROFILESHARING
  if (mSharingEnabled) {
    nsCOMPtr<ipcITransactionService> transServ =
        do_GetService(IPC_TRANSACTIONSERVICE_CONTRACTID, &rv);
    if (NS_SUCCEEDED(rv)) {
      nsCAutoString nativePath;
      rv = mProfileDir->GetNativePath(nativePath);
      if (NS_SUCCEEDED(rv))
        rv = transServ->Init(nativePath);
    }
    if (NS_FAILED(rv)) {
      NS_WARNING("Unable to initialize transaction service");
    }
  }
#endif

#ifdef MOZ_PROFILELOCKING
  // Lock the non-shared sub-dir if we are sharing,
  // the whole profile dir if we are not.
  nsCOMPtr<nsILocalFile> dirToLock;
  if (mSharingEnabled)
    dirToLock = do_QueryInterface(mNonSharedProfileDir);
  else
    dirToLock = do_QueryInterface(mProfileDir);
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

NS_IMPL_THREADSAFE_ISUPPORTS1(nsProfileDirServiceProvider,
                              nsIDirectoryServiceProvider)

//*****************************************************************************
// nsProfileDirServiceProvider::nsIDirectoryServiceProvider
//*****************************************************************************

NS_IMETHODIMP
nsProfileDirServiceProvider::GetFile(const char *prop, PRBool *persistant, nsIFile **_retval)
{
  NS_ENSURE_ARG(prop);
  NS_ENSURE_ARG_POINTER(persistant);
  NS_ENSURE_ARG_POINTER(_retval);

  // Don't assert - we can be called many times before SetProfileDir() has been called.
  if (!mProfileDir)
    return NS_ERROR_FAILURE;

  *persistant = PR_TRUE;
  nsIFile* domainDir = mProfileDir;

#ifdef MOZ_PROFILESHARING
  // If the prop is prefixed with NS_SHARED,
  // the location is in the shared domain.
  PRBool bUseShared = PR_FALSE;
  if (strncmp(prop, NS_SHARED, sizeof(NS_SHARED)-1) == 0) {
    prop += (sizeof(NS_SHARED)-1);
    bUseShared = PR_TRUE;
  }
  if (!bUseShared && mNonSharedProfileDir)
    domainDir = mNonSharedProfileDir;
#endif

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
  else if (strcmp(prop, NS_APP_HISTORY_50_FILE) == 0) {
    rv = domainDir->Clone(getter_AddRefs(localFile));
    if (NS_SUCCEEDED(rv))
      rv = localFile->AppendNative(HISTORY_FILE_50_NAME);
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
  else if (strcmp(prop, NS_APP_MAIL_50_DIR) == 0) {
    rv = domainDir->Clone(getter_AddRefs(localFile));
    if (NS_SUCCEEDED(rv))
      rv = localFile->AppendNative(MAIL_DIR_50_NAME);
  }
  else if (strcmp(prop, NS_APP_IMAP_MAIL_50_DIR) == 0) {
    rv = domainDir->Clone(getter_AddRefs(localFile));
    if (NS_SUCCEEDED(rv))
      rv = localFile->AppendNative(IMAP_MAIL_DIR_50_NAME);
  }
  else if (strcmp(prop, NS_APP_NEWS_50_DIR) == 0) {
    rv = domainDir->Clone(getter_AddRefs(localFile));
    if (NS_SUCCEEDED(rv))
      rv = localFile->AppendNative(NEWS_DIR_50_NAME);
  }
  else if (strcmp(prop, NS_APP_MESSENGER_FOLDER_CACHE_50_DIR) == 0) {
    rv = domainDir->Clone(getter_AddRefs(localFile));
    if (NS_SUCCEEDED(rv))
      rv = localFile->AppendNative(MSG_FOLDER_CACHE_DIR_50_NAME);
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

#ifdef MOZ_PROFILESHARING
  nsCOMPtr<nsIProfileSharingSetup> sharingSetup =
      do_GetService("@mozilla.org/embedcomp/profile-sharing-setup;1");
  if (sharingSetup) {
    PRBool tempBool;
    if (NS_SUCCEEDED(sharingSetup->GetIsSharingEnabled(&tempBool)))
      mSharingEnabled = tempBool;
    if (mSharingEnabled)
      sharingSetup->GetClientName(mNonSharedDirName);
  }
#endif

  return NS_OK;
}

nsresult
nsProfileDirServiceProvider::InitProfileDir(nsIFile *profileDir)
{
  // Make sure our "Profile" folder exists.
  // If it does not, copy the profile defaults to its location.

  nsresult rv;
  PRBool exists;
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

#if !defined(XP_MAC) && !defined(WINCE)
    rv = profileDir->SetPermissions(0700);
    if (NS_FAILED(rv))
      return rv;
#endif

  }
  else {
    PRBool isDir;
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
      PRBool exists;
      rv = localDir->Exists(&exists);
      if (NS_SUCCEEDED(rv)) {
        if (!exists) {
          rv = localDir->Create(nsIFile::DIRECTORY_TYPE, 0700);
        }
        else {
          PRBool isDir;
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
  PRBool exists;

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
  (void) directoryService->Undefine(NS_APP_HISTORY_50_FILE);
  (void) directoryService->Undefine(NS_APP_USER_PANELS_50_FILE);
  (void) directoryService->Undefine(NS_APP_USER_MIMETYPES_50_FILE);
  (void) directoryService->Undefine(NS_APP_BOOKMARKS_50_FILE);
  (void) directoryService->Undefine(NS_APP_DOWNLOADS_50_FILE);
  (void) directoryService->Undefine(NS_APP_SEARCH_50_FILE);
  (void) directoryService->Undefine(NS_APP_MAIL_50_DIR);
  (void) directoryService->Undefine(NS_APP_IMAP_MAIL_50_DIR);
  (void) directoryService->Undefine(NS_APP_NEWS_50_DIR);
  (void) directoryService->Undefine(NS_APP_MESSENGER_FOLDER_CACHE_50_DIR);

  return NS_OK;
}

//*****************************************************************************
// Global creation function
//*****************************************************************************

nsresult NS_NewProfileDirServiceProvider(PRBool aNotifyObservers,
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
