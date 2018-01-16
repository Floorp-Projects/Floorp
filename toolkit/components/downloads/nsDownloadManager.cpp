/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsIPrefService.h"
#include "nsIPropertyBag2.h"
#include "nsCExternalHandlerService.h"
#include "nsDirectoryServiceDefs.h"
#include "nsDownloadManager.h"

#include "mozilla/Services.h"

using namespace mozilla;

#define DOWNLOAD_MANAGER_BUNDLE "chrome://mozapps/locale/downloads/downloads.properties"

#define NS_SYSTEMINFO_CONTRACTID "@mozilla.org/system-info;1"

////////////////////////////////////////////////////////////////////////////////
//// nsDownloadManager

NS_IMPL_ISUPPORTS(
  nsDownloadManager
, nsIDownloadManager
)

nsDownloadManager *nsDownloadManager::gDownloadManagerService = nullptr;

already_AddRefed<nsDownloadManager>
nsDownloadManager::GetSingleton()
{
  if (gDownloadManagerService) {
    return do_AddRef(gDownloadManagerService);
  }

  auto serv = MakeRefPtr<nsDownloadManager>();
  // Note: This is cleared in the nsDownloadManager constructor.
  gDownloadManagerService = serv.get();
  if (NS_SUCCEEDED(serv->Init())) {
    return serv.forget();
  }
  return nullptr;
}

nsDownloadManager::~nsDownloadManager()
{
  MOZ_ASSERT(gDownloadManagerService == this);
  gDownloadManagerService = nullptr;
}

nsresult
nsDownloadManager::Init()
{
  nsresult rv;

  nsCOMPtr<nsIStringBundleService> bundleService =
    mozilla::services::GetStringBundleService();
  if (!bundleService)
    return NS_ERROR_FAILURE;

  rv = bundleService->CreateBundle(DOWNLOAD_MANAGER_BUNDLE,
                                   getter_AddRefs(mBundle));
  NS_ENSURE_SUCCESS(rv, rv);

  return NS_OK;
}

////////////////////////////////////////////////////////////////////////////////
//// nsIDownloadManager

NS_IMETHODIMP
nsDownloadManager::GetActivePrivateDownloadCount(int32_t* aResult)
{
  return NS_ERROR_UNEXPECTED;
}

NS_IMETHODIMP
nsDownloadManager::GetActiveDownloadCount(int32_t *aResult)
{
  return NS_ERROR_UNEXPECTED;
}

NS_IMETHODIMP
nsDownloadManager::GetActiveDownloads(nsISimpleEnumerator **aResult)
{
  return NS_ERROR_UNEXPECTED;
}

NS_IMETHODIMP
nsDownloadManager::GetActivePrivateDownloads(nsISimpleEnumerator **aResult)
{
  return NS_ERROR_UNEXPECTED;
}

/**
 * For platforms where helper apps use the downloads directory (i.e. mobile),
 * this should be kept in sync with nsExternalHelperAppService.cpp
 */
NS_IMETHODIMP
nsDownloadManager::GetDefaultDownloadsDirectory(nsIFile **aResult)
{
  nsCOMPtr<nsIFile> downloadDir;

  nsresult rv;
  nsCOMPtr<nsIProperties> dirService =
     do_GetService(NS_DIRECTORY_SERVICE_CONTRACTID, &rv);
  NS_ENSURE_SUCCESS(rv, rv);

  // OSX 10.4:
  // Desktop
  // OSX 10.5:
  // User download directory
  // Vista:
  // Downloads
  // XP/2K:
  // My Documents/Downloads
  // Linux:
  // XDG user dir spec, with a fallback to Home/Downloads

  nsAutoString folderName;
  mBundle->GetStringFromName("downloadsFolder", folderName);

#if defined (XP_MACOSX)
  rv = dirService->Get(NS_OSX_DEFAULT_DOWNLOAD_DIR,
                       NS_GET_IID(nsIFile),
                       getter_AddRefs(downloadDir));
  NS_ENSURE_SUCCESS(rv, rv);
#elif defined(XP_WIN)
  rv = dirService->Get(NS_WIN_DEFAULT_DOWNLOAD_DIR,
                       NS_GET_IID(nsIFile),
                       getter_AddRefs(downloadDir));
  NS_ENSURE_SUCCESS(rv, rv);

  // Check the os version
  nsCOMPtr<nsIPropertyBag2> infoService =
     do_GetService(NS_SYSTEMINFO_CONTRACTID, &rv);
  NS_ENSURE_SUCCESS(rv, rv);

  int32_t version;
  NS_NAMED_LITERAL_STRING(osVersion, "version");
  rv = infoService->GetPropertyAsInt32(osVersion, &version);
  NS_ENSURE_SUCCESS(rv, rv);
  if (version < 6) { // XP/2K
    // First get "My Documents"
    rv = dirService->Get(NS_WIN_PERSONAL_DIR,
                         NS_GET_IID(nsIFile),
                         getter_AddRefs(downloadDir));
    NS_ENSURE_SUCCESS(rv, rv);

    rv = downloadDir->Append(folderName);
    NS_ENSURE_SUCCESS(rv, rv);

    // This could be the first time we are creating the downloads folder in My
    // Documents, so make sure it exists.
    bool exists;
    rv = downloadDir->Exists(&exists);
    NS_ENSURE_SUCCESS(rv, rv);
    if (!exists) {
      rv = downloadDir->Create(nsIFile::DIRECTORY_TYPE, 0755);
      NS_ENSURE_SUCCESS(rv, rv);
    }
  }
#elif defined(XP_UNIX)
#if defined(MOZ_WIDGET_ANDROID)
    // Android doesn't have a $HOME directory, and by default we only have
    // write access to /data/data/org.mozilla.{$APP} and /sdcard
    char* downloadDirPath = getenv("DOWNLOADS_DIRECTORY");
    if (downloadDirPath) {
      rv = NS_NewNativeLocalFile(nsDependentCString(downloadDirPath),
                                 true, getter_AddRefs(downloadDir));
      NS_ENSURE_SUCCESS(rv, rv);
    }
    else {
      rv = NS_ERROR_FAILURE;
    }
#else
  rv = dirService->Get(NS_UNIX_DEFAULT_DOWNLOAD_DIR,
                       NS_GET_IID(nsIFile),
                       getter_AddRefs(downloadDir));
  // fallback to Home/Downloads
  if (NS_FAILED(rv)) {
    rv = dirService->Get(NS_UNIX_HOME_DIR,
                         NS_GET_IID(nsIFile),
                         getter_AddRefs(downloadDir));
    NS_ENSURE_SUCCESS(rv, rv);
    rv = downloadDir->Append(folderName);
    NS_ENSURE_SUCCESS(rv, rv);
  }
#endif
#else
  rv = dirService->Get(NS_OS_HOME_DIR,
                       NS_GET_IID(nsIFile),
                       getter_AddRefs(downloadDir));
  NS_ENSURE_SUCCESS(rv, rv);
  rv = downloadDir->Append(folderName);
  NS_ENSURE_SUCCESS(rv, rv);
#endif

  downloadDir.forget(aResult);

  return NS_OK;
}

#define NS_BRANCH_DOWNLOAD     "browser.download."
#define NS_PREF_FOLDERLIST     "folderList"
#define NS_PREF_DIR            "dir"

NS_IMETHODIMP
nsDownloadManager::GetUserDownloadsDirectory(nsIFile **aResult)
{
  nsresult rv;
  nsCOMPtr<nsIProperties> dirService =
     do_GetService(NS_DIRECTORY_SERVICE_CONTRACTID, &rv);
  NS_ENSURE_SUCCESS(rv, rv);

  nsCOMPtr<nsIPrefService> prefService =
     do_GetService(NS_PREFSERVICE_CONTRACTID, &rv);
  NS_ENSURE_SUCCESS(rv, rv);

  nsCOMPtr<nsIPrefBranch> prefBranch;
  rv = prefService->GetBranch(NS_BRANCH_DOWNLOAD,
                              getter_AddRefs(prefBranch));
  NS_ENSURE_SUCCESS(rv, rv);

  int32_t val;
  rv = prefBranch->GetIntPref(NS_PREF_FOLDERLIST,
                              &val);
  NS_ENSURE_SUCCESS(rv, rv);

  switch(val) {
    case 0: // Desktop
      {
        nsCOMPtr<nsIFile> downloadDir;
        rv = dirService->Get(NS_OS_DESKTOP_DIR,
                             NS_GET_IID(nsIFile),
                             getter_AddRefs(downloadDir));
        NS_ENSURE_SUCCESS(rv, rv);
        downloadDir.forget(aResult);
        return NS_OK;
      }
      break;
    case 1: // Downloads
      return GetDefaultDownloadsDirectory(aResult);
    case 2: // Custom
      {
        nsCOMPtr<nsIFile> customDirectory;
        prefBranch->GetComplexValue(NS_PREF_DIR,
                                    NS_GET_IID(nsIFile),
                                    getter_AddRefs(customDirectory));
        if (customDirectory) {
          bool exists = false;
          (void)customDirectory->Exists(&exists);

          if (!exists) {
            rv = customDirectory->Create(nsIFile::DIRECTORY_TYPE, 0755);
            if (NS_SUCCEEDED(rv)) {
              customDirectory.forget(aResult);
              return NS_OK;
            }

            // Create failed, so it still doesn't exist.  Fall out and get the
            // default downloads directory.
          }

          bool writable = false;
          bool directory = false;
          (void)customDirectory->IsWritable(&writable);
          (void)customDirectory->IsDirectory(&directory);

          if (exists && writable && directory) {
            customDirectory.forget(aResult);
            return NS_OK;
          }
        }
        rv = GetDefaultDownloadsDirectory(aResult);
        if (NS_SUCCEEDED(rv)) {
          (void)prefBranch->SetComplexValue(NS_PREF_DIR,
                                            NS_GET_IID(nsIFile),
                                            *aResult);
        }
        return rv;
      }
      break;
  }
  return NS_ERROR_INVALID_ARG;
}

NS_IMETHODIMP
nsDownloadManager::AddDownload(int16_t aDownloadType,
                               nsIURI *aSource,
                               nsIURI *aTarget,
                               const nsAString& aDisplayName,
                               nsIMIMEInfo *aMIMEInfo,
                               PRTime aStartTime,
                               nsIFile *aTempFile,
                               nsICancelable *aCancelable,
                               bool aIsPrivate,
                               nsIDownload **aDownload)
{
  return NS_ERROR_UNEXPECTED;
}

NS_IMETHODIMP
nsDownloadManager::GetDownload(uint32_t aID, nsIDownload **aDownloadItem)
{
  return NS_ERROR_UNEXPECTED;
}

NS_IMETHODIMP
nsDownloadManager::GetDownloadByGUID(const nsACString& aGUID,
                                     nsIDownloadManagerResult* aCallback)
{
  return NS_ERROR_UNEXPECTED;
}

NS_IMETHODIMP
nsDownloadManager::CancelDownload(uint32_t aID)
{
  return NS_ERROR_UNEXPECTED;
}

NS_IMETHODIMP
nsDownloadManager::RetryDownload(uint32_t aID)
{
  return NS_ERROR_UNEXPECTED;
}

NS_IMETHODIMP
nsDownloadManager::RemoveDownload(uint32_t aID)
{
  return NS_ERROR_UNEXPECTED;
}

NS_IMETHODIMP
nsDownloadManager::RemoveDownloadsByTimeframe(int64_t aStartTime,
                                              int64_t aEndTime)
{
  return NS_ERROR_UNEXPECTED;
}

NS_IMETHODIMP
nsDownloadManager::CleanUp()
{
  return NS_ERROR_UNEXPECTED;
}

NS_IMETHODIMP
nsDownloadManager::CleanUpPrivate()
{
  return NS_ERROR_UNEXPECTED;
}

NS_IMETHODIMP
nsDownloadManager::GetCanCleanUp(bool *aResult)
{
  return NS_ERROR_UNEXPECTED;
}

NS_IMETHODIMP
nsDownloadManager::GetCanCleanUpPrivate(bool *aResult)
{
  return NS_ERROR_UNEXPECTED;
}

NS_IMETHODIMP
nsDownloadManager::PauseDownload(uint32_t aID)
{
  return NS_ERROR_UNEXPECTED;
}

NS_IMETHODIMP
nsDownloadManager::ResumeDownload(uint32_t aID)
{
  return NS_ERROR_UNEXPECTED;
}

NS_IMETHODIMP
nsDownloadManager::GetDBConnection(mozIStorageConnection **aDBConn)
{
  return NS_ERROR_UNEXPECTED;
}

NS_IMETHODIMP
nsDownloadManager::GetPrivateDBConnection(mozIStorageConnection **aDBConn)
{
  return NS_ERROR_UNEXPECTED;
}

NS_IMETHODIMP
nsDownloadManager::AddListener(nsIDownloadProgressListener *aListener)
{
  return NS_ERROR_UNEXPECTED;
}

NS_IMETHODIMP
nsDownloadManager::AddPrivacyAwareListener(nsIDownloadProgressListener *aListener)
{
  return NS_ERROR_UNEXPECTED;
}

NS_IMETHODIMP
nsDownloadManager::RemoveListener(nsIDownloadProgressListener *aListener)
{
  return NS_ERROR_UNEXPECTED;
}
