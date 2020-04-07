/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/ArrayUtils.h"

#include "nsCOMPtr.h"
#include "nsDirectoryService.h"
#include "nsLocalFile.h"
#include "nsDebug.h"
#include "nsGkAtoms.h"
#include "nsEnumeratorUtils.h"
#include "nsThreadUtils.h"

#include "mozilla/SimpleEnumerator.h"
#include "nsICategoryManager.h"
#include "nsISimpleEnumerator.h"

#if defined(XP_WIN)
#  include <windows.h>
#  include <shlobj.h>
#  include <stdlib.h>
#  include <stdio.h>
#elif defined(XP_UNIX)
#  include <unistd.h>
#  include <stdlib.h>
#  include <sys/param.h>
#  include "prenv.h"
#  ifdef MOZ_WIDGET_COCOA
#    include <CoreServices/CoreServices.h>
#    include <Carbon/Carbon.h>
#  endif
#endif

#include "SpecialSystemDirectory.h"
#include "nsAppFileLocationProvider.h"
#include "BinaryPath.h"

using namespace mozilla;

//----------------------------------------------------------------------------------------
nsresult nsDirectoryService::GetCurrentProcessDirectory(nsIFile** aFile)
//----------------------------------------------------------------------------------------
{
  if (NS_WARN_IF(!aFile)) {
    return NS_ERROR_INVALID_ARG;
  }
  *aFile = nullptr;

  //  Set the component registry location:
  if (!gService) {
    return NS_ERROR_FAILURE;
  }

  if (!mXCurProcD) {
    nsCOMPtr<nsIFile> file;
    if (NS_SUCCEEDED(BinaryPath::GetFile(getter_AddRefs(file)))) {
      nsresult rv = file->GetParent(getter_AddRefs(mXCurProcD));
      if (NS_FAILED(rv)) {
        return rv;
      }
    }
  }
  return mXCurProcD->Clone(aFile);
}  // GetCurrentProcessDirectory()

StaticRefPtr<nsDirectoryService> nsDirectoryService::gService;

nsDirectoryService::nsDirectoryService() : mHashtable(128) {}

nsresult nsDirectoryService::Create(nsISupports* aOuter, REFNSIID aIID,
                                    void** aResult) {
  if (NS_WARN_IF(!aResult)) {
    return NS_ERROR_INVALID_ARG;
  }
  if (NS_WARN_IF(aOuter)) {
    return NS_ERROR_NO_AGGREGATION;
  }

  if (!gService) {
    return NS_ERROR_NOT_INITIALIZED;
  }

  return gService->QueryInterface(aIID, aResult);
}

NS_IMETHODIMP
nsDirectoryService::Init() {
  MOZ_ASSERT_UNREACHABLE("nsDirectoryService::Init() for internal use only!");
  return NS_OK;
}

void nsDirectoryService::RealInit() {
  NS_ASSERTION(!gService,
               "nsDirectoryService::RealInit Mustn't initialize twice!");

  gService = new nsDirectoryService();

  // Let the list hold the only reference to the provider.
  nsAppFileLocationProvider* defaultProvider = new nsAppFileLocationProvider;
  gService->mProviders.AppendElement(defaultProvider);
}

nsDirectoryService::~nsDirectoryService() = default;

NS_IMPL_ISUPPORTS(nsDirectoryService, nsIProperties, nsIDirectoryService,
                  nsIDirectoryServiceProvider, nsIDirectoryServiceProvider2)

NS_IMETHODIMP
nsDirectoryService::Undefine(const char* aProp) {
  if (NS_WARN_IF(!aProp)) {
    return NS_ERROR_INVALID_ARG;
  }

  nsDependentCString key(aProp);
  return mHashtable.Remove(key) ? NS_OK : NS_ERROR_FAILURE;
}

NS_IMETHODIMP
nsDirectoryService::GetKeys(nsTArray<nsCString>& aKeys) {
  return NS_ERROR_NOT_IMPLEMENTED;
}

struct MOZ_STACK_CLASS FileData {
  FileData(const char* aProperty, const nsIID& aUUID)
      : property(aProperty), data(nullptr), persistent(true), uuid(aUUID) {}

  const char* property;
  nsCOMPtr<nsISupports> data;
  bool persistent;
  const nsIID& uuid;
};

static bool FindProviderFile(nsIDirectoryServiceProvider* aElement,
                             FileData* aData) {
  nsresult rv;
  if (aData->uuid.Equals(NS_GET_IID(nsISimpleEnumerator))) {
    // Not all providers implement this iface
    nsCOMPtr<nsIDirectoryServiceProvider2> prov2 = do_QueryInterface(aElement);
    if (prov2) {
      nsCOMPtr<nsISimpleEnumerator> newFiles;
      rv = prov2->GetFiles(aData->property, getter_AddRefs(newFiles));
      if (NS_SUCCEEDED(rv) && newFiles) {
        if (aData->data) {
          nsCOMPtr<nsISimpleEnumerator> unionFiles;

          NS_NewUnionEnumerator(getter_AddRefs(unionFiles),
                                (nsISimpleEnumerator*)aData->data.get(),
                                newFiles);

          if (unionFiles) {
            unionFiles.swap(*(nsISimpleEnumerator**)&aData->data);
          }
        } else {
          aData->data = newFiles;
        }

        aData->persistent = false;  // Enumerators can never be persistent
        return rv == NS_SUCCESS_AGGREGATE_RESULT;
      }
    }
  } else {
    rv = aElement->GetFile(aData->property, &aData->persistent,
                           (nsIFile**)&aData->data);
    if (NS_SUCCEEDED(rv) && aData->data) {
      return false;
    }
  }

  return true;
}

NS_IMETHODIMP
nsDirectoryService::Get(const char* aProp, const nsIID& aUuid, void** aResult) {
  if (NS_WARN_IF(!aProp)) {
    return NS_ERROR_INVALID_ARG;
  }

  MOZ_ASSERT(NS_IsMainThread(), "Do not call dirsvc::get on non-main threads!");

  nsDependentCString key(aProp);

  nsCOMPtr<nsIFile> cachedFile = mHashtable.Get(key);

  if (cachedFile) {
    nsCOMPtr<nsIFile> cloneFile;
    cachedFile->Clone(getter_AddRefs(cloneFile));
    return cloneFile->QueryInterface(aUuid, aResult);
  }

  // it is not one of our defaults, lets check any providers
  FileData fileData(aProp, aUuid);

  for (int32_t i = mProviders.Length() - 1; i >= 0; i--) {
    if (!FindProviderFile(mProviders[i], &fileData)) {
      break;
    }
  }
  if (fileData.data) {
    if (fileData.persistent) {
      Set(aProp, static_cast<nsIFile*>(fileData.data.get()));
    }
    nsresult rv = (fileData.data)->QueryInterface(aUuid, aResult);
    fileData.data = nullptr;  // AddRef occurs in FindProviderFile()
    return rv;
  }

  FindProviderFile(static_cast<nsIDirectoryServiceProvider*>(this), &fileData);
  if (fileData.data) {
    if (fileData.persistent) {
      Set(aProp, static_cast<nsIFile*>(fileData.data.get()));
    }
    nsresult rv = (fileData.data)->QueryInterface(aUuid, aResult);
    fileData.data = nullptr;  // AddRef occurs in FindProviderFile()
    return rv;
  }

  return NS_ERROR_FAILURE;
}

NS_IMETHODIMP
nsDirectoryService::Set(const char* aProp, nsISupports* aValue) {
  if (NS_WARN_IF(!aProp)) {
    return NS_ERROR_INVALID_ARG;
  }
  if (!aValue) {
    return NS_ERROR_FAILURE;
  }

  nsDependentCString key(aProp);
  if (auto entry = mHashtable.LookupForAdd(key)) {
    return NS_ERROR_FAILURE;
  } else {
    nsCOMPtr<nsIFile> ourFile = do_QueryInterface(aValue);
    if (ourFile) {
      nsCOMPtr<nsIFile> cloneFile;
      ourFile->Clone(getter_AddRefs(cloneFile));
      entry.OrInsert([&cloneFile]() { return cloneFile.forget(); });
      return NS_OK;
    }
    mHashtable.Remove(key);  // another hashtable lookup, but should be rare
  }
  return NS_ERROR_FAILURE;
}

NS_IMETHODIMP
nsDirectoryService::Has(const char* aProp, bool* aResult) {
  if (NS_WARN_IF(!aProp)) {
    return NS_ERROR_INVALID_ARG;
  }

  *aResult = false;
  nsCOMPtr<nsIFile> value;
  nsresult rv = Get(aProp, NS_GET_IID(nsIFile), getter_AddRefs(value));
  if (NS_FAILED(rv)) {
    return NS_OK;
  }

  if (value) {
    *aResult = true;
  }

  return rv;
}

NS_IMETHODIMP
nsDirectoryService::RegisterProvider(nsIDirectoryServiceProvider* aProv) {
  if (!aProv) {
    return NS_ERROR_FAILURE;
  }

  mProviders.AppendElement(aProv);
  return NS_OK;
}

void nsDirectoryService::RegisterCategoryProviders() {
  nsCOMPtr<nsICategoryManager> catman(
      do_GetService(NS_CATEGORYMANAGER_CONTRACTID));
  if (!catman) {
    return;
  }

  nsCOMPtr<nsISimpleEnumerator> entries;
  catman->EnumerateCategory(XPCOM_DIRECTORY_PROVIDER_CATEGORY,
                            getter_AddRefs(entries));

  for (auto& categoryEntry : SimpleEnumerator<nsICategoryEntry>(entries)) {
    nsAutoCString contractID;
    categoryEntry->GetValue(contractID);

    if (nsCOMPtr<nsIDirectoryServiceProvider> provider =
            do_GetService(contractID.get())) {
      RegisterProvider(provider);
    }
  }
}

NS_IMETHODIMP
nsDirectoryService::UnregisterProvider(nsIDirectoryServiceProvider* aProv) {
  if (!aProv) {
    return NS_ERROR_FAILURE;
  }

  mProviders.RemoveElement(aProv);
  return NS_OK;
}

#if defined(MOZ_SANDBOX) && defined(XP_WIN)
static nsresult GetLowIntegrityTempBase(nsIFile** aLowIntegrityTempBase) {
  nsCOMPtr<nsIFile> localFile;
  nsresult rv =
      GetSpecialSystemDirectory(Win_LocalAppdataLow, getter_AddRefs(localFile));
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  rv = localFile->Append(NS_LITERAL_STRING(MOZ_USER_DIR));
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  localFile.forget(aLowIntegrityTempBase);
  return rv;
}
#endif

// DO NOT ADD ANY LOCATIONS TO THIS FUNCTION UNTIL YOU TALK TO:
// dougt@netscape.com. This is meant to be a place of xpcom or system specific
// file locations, not application specific locations.  If you need the later,
// register a callback for your application.

NS_IMETHODIMP
nsDirectoryService::GetFile(const char* aProp, bool* aPersistent,
                            nsIFile** aResult) {
  nsCOMPtr<nsIFile> localFile;
  nsresult rv = NS_ERROR_FAILURE;

  *aResult = nullptr;
  *aPersistent = true;

  RefPtr<nsAtom> inAtom = NS_Atomize(aProp);

  // check to see if it is one of our defaults

  if (inAtom == nsGkAtoms::DirectoryService_CurrentProcess ||
      inAtom == nsGkAtoms::DirectoryService_OS_CurrentProcessDirectory) {
    rv = GetCurrentProcessDirectory(getter_AddRefs(localFile));
  }

  // Unless otherwise set, the core pieces of the GRE exist
  // in the current process directory.
  else if (inAtom == nsGkAtoms::DirectoryService_GRE_Directory ||
           inAtom == nsGkAtoms::DirectoryService_GRE_BinDirectory) {
    rv = GetCurrentProcessDirectory(getter_AddRefs(localFile));
  } else if (inAtom == nsGkAtoms::DirectoryService_OS_TemporaryDirectory) {
    rv = GetSpecialSystemDirectory(OS_TemporaryDirectory,
                                   getter_AddRefs(localFile));
  } else if (inAtom == nsGkAtoms::DirectoryService_OS_CurrentWorkingDirectory) {
    rv = GetSpecialSystemDirectory(OS_CurrentWorkingDirectory,
                                   getter_AddRefs(localFile));
  }
#if defined(MOZ_WIDGET_COCOA)
  else if (inAtom == nsGkAtoms::DirectoryService_SystemDirectory) {
    rv = GetOSXFolderType(kClassicDomain, kSystemFolderType,
                          getter_AddRefs(localFile));
  } else if (inAtom == nsGkAtoms::DirectoryService_UserLibDirectory) {
    rv = GetOSXFolderType(kUserDomain, kDomainLibraryFolderType,
                          getter_AddRefs(localFile));
  } else if (inAtom == nsGkAtoms::Home) {
    rv = GetOSXFolderType(kUserDomain, kDomainTopLevelFolderType,
                          getter_AddRefs(localFile));
  } else if (inAtom == nsGkAtoms::DirectoryService_DefaultDownloadDirectory) {
    rv = GetOSXFolderType(kUserDomain, kDownloadsFolderType,
                          getter_AddRefs(localFile));
    if (NS_FAILED(rv)) {
      rv = GetOSXFolderType(kUserDomain, kDesktopFolderType,
                            getter_AddRefs(localFile));
    }
  } else if (inAtom == nsGkAtoms::DirectoryService_OS_DesktopDirectory) {
    rv = GetOSXFolderType(kUserDomain, kDesktopFolderType,
                          getter_AddRefs(localFile));
  } else if (inAtom == nsGkAtoms::DirectoryService_LocalApplicationsDirectory) {
    rv = GetOSXFolderType(kLocalDomain, kApplicationsFolderType,
                          getter_AddRefs(localFile));
  } else if (inAtom == nsGkAtoms::DirectoryService_UserPreferencesDirectory) {
    rv = GetOSXFolderType(kUserDomain, kPreferencesFolderType,
                          getter_AddRefs(localFile));
  } else if (inAtom == nsGkAtoms::DirectoryService_PictureDocumentsDirectory) {
    rv = GetOSXFolderType(kUserDomain, kPictureDocumentsFolderType,
                          getter_AddRefs(localFile));
  }
#elif defined(XP_WIN)
  else if (inAtom == nsGkAtoms::DirectoryService_SystemDirectory) {
    rv = GetSpecialSystemDirectory(Win_SystemDirectory,
                                   getter_AddRefs(localFile));
  } else if (inAtom == nsGkAtoms::DirectoryService_WindowsDirectory) {
    rv = GetSpecialSystemDirectory(Win_WindowsDirectory,
                                   getter_AddRefs(localFile));
  } else if (inAtom == nsGkAtoms::DirectoryService_WindowsProgramFiles) {
    rv = GetSpecialSystemDirectory(Win_ProgramFiles, getter_AddRefs(localFile));
  } else if (inAtom == nsGkAtoms::Home) {
    rv =
        GetSpecialSystemDirectory(Win_HomeDirectory, getter_AddRefs(localFile));
  } else if (inAtom == nsGkAtoms::DirectoryService_Programs) {
    rv = GetSpecialSystemDirectory(Win_Programs, getter_AddRefs(localFile));
  } else if (inAtom == nsGkAtoms::DirectoryService_Favorites) {
    rv = GetSpecialSystemDirectory(Win_Favorites, getter_AddRefs(localFile));
  } else if (inAtom == nsGkAtoms::DirectoryService_OS_DesktopDirectory) {
    rv = GetSpecialSystemDirectory(Win_Desktopdirectory,
                                   getter_AddRefs(localFile));
  } else if (inAtom == nsGkAtoms::DirectoryService_Appdata) {
    rv = GetSpecialSystemDirectory(Win_Appdata, getter_AddRefs(localFile));
  } else if (inAtom == nsGkAtoms::DirectoryService_LocalAppdata) {
    rv = GetSpecialSystemDirectory(Win_LocalAppdata, getter_AddRefs(localFile));
#  if defined(MOZ_SANDBOX)
  } else if (inAtom == nsGkAtoms::DirectoryService_LocalAppdataLow) {
    rv = GetSpecialSystemDirectory(Win_LocalAppdataLow,
                                   getter_AddRefs(localFile));
  } else if (inAtom == nsGkAtoms::DirectoryService_LowIntegrityTempBase) {
    rv = GetLowIntegrityTempBase(getter_AddRefs(localFile));
#  endif
  } else if (inAtom == nsGkAtoms::DirectoryService_WinCookiesDirectory) {
    rv = GetSpecialSystemDirectory(Win_Cookies, getter_AddRefs(localFile));
  } else if (inAtom == nsGkAtoms::DirectoryService_DefaultDownloadDirectory) {
    rv = GetSpecialSystemDirectory(Win_Downloads, getter_AddRefs(localFile));
  }
#elif defined(XP_UNIX)
  else if (inAtom == nsGkAtoms::Home) {
    rv = GetSpecialSystemDirectory(Unix_HomeDirectory,
                                   getter_AddRefs(localFile));
  } else if (inAtom == nsGkAtoms::DirectoryService_OS_DesktopDirectory) {
    rv = GetSpecialSystemDirectory(Unix_XDG_Desktop, getter_AddRefs(localFile));
    *aPersistent = false;
  } else if (inAtom == nsGkAtoms::DirectoryService_DefaultDownloadDirectory) {
    rv =
        GetSpecialSystemDirectory(Unix_XDG_Download, getter_AddRefs(localFile));
    *aPersistent = false;
  }
#endif

  if (NS_FAILED(rv)) {
    return rv;
  }

  if (!localFile) {
    return NS_ERROR_FAILURE;
  }

  localFile.forget(aResult);
  return NS_OK;
}

NS_IMETHODIMP
nsDirectoryService::GetFiles(const char* aProp, nsISimpleEnumerator** aResult) {
  if (NS_WARN_IF(!aResult)) {
    return NS_ERROR_INVALID_ARG;
  }
  *aResult = nullptr;

  return NS_ERROR_FAILURE;
}
