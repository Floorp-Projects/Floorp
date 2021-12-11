/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "Omnijar.h"

#include "nsDirectoryService.h"
#include "nsDirectoryServiceDefs.h"
#include "nsIFile.h"
#include "nsZipArchive.h"
#include "nsNetUtil.h"

namespace mozilla {

StaticRefPtr<nsIFile> Omnijar::sPath[2];
StaticRefPtr<nsZipArchive> Omnijar::sReader[2];
StaticRefPtr<nsZipArchive> Omnijar::sOuterReader[2];
bool Omnijar::sInitialized = false;
bool Omnijar::sIsUnified = false;

static const char* sProp[2] = {NS_GRE_DIR, NS_XPCOM_CURRENT_PROCESS_DIR};

#define SPROP(Type) ((Type == mozilla::Omnijar::GRE) ? sProp[GRE] : sProp[APP])

void Omnijar::CleanUpOne(Type aType) {
  if (sReader[aType]) {
    sReader[aType]->CloseArchive();
    sReader[aType] = nullptr;
  }
  if (sOuterReader[aType]) {
    sOuterReader[aType]->CloseArchive();
    sOuterReader[aType] = nullptr;
  }
  sPath[aType] = nullptr;
}

void Omnijar::InitOne(nsIFile* aPath, Type aType) {
  nsCOMPtr<nsIFile> file;
  if (aPath) {
    file = aPath;
  } else {
    nsCOMPtr<nsIFile> dir;
    nsDirectoryService::gService->Get(SPROP(aType), NS_GET_IID(nsIFile),
                                      getter_AddRefs(dir));
    constexpr auto kOmnijarName = nsLiteralCString{MOZ_STRINGIFY(OMNIJAR_NAME)};
    if (NS_FAILED(dir->Clone(getter_AddRefs(file))) ||
        NS_FAILED(file->AppendNative(kOmnijarName))) {
      return;
    }
  }
  bool isFile;
  if (NS_FAILED(file->IsFile(&isFile)) || !isFile) {
    // If we're not using an omni.jar for GRE, and we don't have an
    // omni.jar for APP, check if both directories are the same.
    if ((aType == APP) && (!sPath[GRE])) {
      nsCOMPtr<nsIFile> greDir, appDir;
      bool equals;
      nsDirectoryService::gService->Get(sProp[GRE], NS_GET_IID(nsIFile),
                                        getter_AddRefs(greDir));
      nsDirectoryService::gService->Get(sProp[APP], NS_GET_IID(nsIFile),
                                        getter_AddRefs(appDir));
      if (NS_SUCCEEDED(greDir->Equals(appDir, &equals)) && equals) {
        sIsUnified = true;
      }
    }
    return;
  }

  bool equals;
  if ((aType == APP) && (sPath[GRE]) &&
      NS_SUCCEEDED(sPath[GRE]->Equals(file, &equals)) && equals) {
    // If we're using omni.jar on both GRE and APP and their path
    // is the same, we're in the unified case.
    sIsUnified = true;
    return;
  }

  RefPtr<nsZipArchive> zipReader = new nsZipArchive();
  if (NS_FAILED(zipReader->OpenArchive(file))) {
    return;
  }

  RefPtr<nsZipArchive> outerReader;
  RefPtr<nsZipHandle> handle;
  if (NS_SUCCEEDED(nsZipHandle::Init(zipReader, MOZ_STRINGIFY(OMNIJAR_NAME),
                                     getter_AddRefs(handle)))) {
    outerReader = zipReader;
    zipReader = new nsZipArchive();
    if (NS_FAILED(zipReader->OpenArchive(handle))) {
      return;
    }
  }

  CleanUpOne(aType);
  sReader[aType] = zipReader;
  sOuterReader[aType] = outerReader;
  sPath[aType] = file;
}

void Omnijar::Init(nsIFile* aGrePath, nsIFile* aAppPath) {
  InitOne(aGrePath, GRE);
  InitOne(aAppPath, APP);
  sInitialized = true;
}

void Omnijar::CleanUp() {
  CleanUpOne(GRE);
  CleanUpOne(APP);
  sInitialized = false;
}

already_AddRefed<nsZipArchive> Omnijar::GetReader(nsIFile* aPath) {
  MOZ_ASSERT(IsInitialized(), "Omnijar not initialized");

  bool equals;
  nsresult rv;

  if (sPath[GRE]) {
    rv = sPath[GRE]->Equals(aPath, &equals);
    if (NS_SUCCEEDED(rv) && equals) {
      return IsNested(GRE) ? GetOuterReader(GRE) : GetReader(GRE);
    }
  }
  if (sPath[APP]) {
    rv = sPath[APP]->Equals(aPath, &equals);
    if (NS_SUCCEEDED(rv) && equals) {
      return IsNested(APP) ? GetOuterReader(APP) : GetReader(APP);
    }
  }
  return nullptr;
}

already_AddRefed<nsZipArchive> Omnijar::GetInnerReader(
    nsIFile* aPath, const nsACString& aEntry) {
  MOZ_ASSERT(IsInitialized(), "Omnijar not initialized");

  if (!aEntry.EqualsLiteral(MOZ_STRINGIFY(OMNIJAR_NAME))) {
    return nullptr;
  }

  bool equals;
  nsresult rv;

  if (sPath[GRE]) {
    rv = sPath[GRE]->Equals(aPath, &equals);
    if (NS_SUCCEEDED(rv) && equals) {
      return IsNested(GRE) ? GetReader(GRE) : nullptr;
    }
  }
  if (sPath[APP]) {
    rv = sPath[APP]->Equals(aPath, &equals);
    if (NS_SUCCEEDED(rv) && equals) {
      return IsNested(APP) ? GetReader(APP) : nullptr;
    }
  }
  return nullptr;
}

nsresult Omnijar::GetURIString(Type aType, nsACString& aResult) {
  MOZ_ASSERT(IsInitialized(), "Omnijar not initialized");

  aResult.Truncate();

  // Return an empty string for APP in the unified case.
  if ((aType == APP) && sIsUnified) {
    return NS_OK;
  }

  nsAutoCString omniJarSpec;
  if (sPath[aType]) {
    nsresult rv = NS_GetURLSpecFromActualFile(sPath[aType], omniJarSpec);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }

    aResult = "jar:";
    if (IsNested(aType)) {
      aResult += "jar:";
    }
    aResult += omniJarSpec;
    aResult += "!";
    if (IsNested(aType)) {
      aResult += "/" MOZ_STRINGIFY(OMNIJAR_NAME) "!";
    }
  } else {
    nsCOMPtr<nsIFile> dir;
    nsDirectoryService::gService->Get(SPROP(aType), NS_GET_IID(nsIFile),
                                      getter_AddRefs(dir));
    nsresult rv = NS_GetURLSpecFromActualFile(dir, aResult);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }
  }
  aResult += "/";
  return NS_OK;
}

} /* namespace mozilla */
