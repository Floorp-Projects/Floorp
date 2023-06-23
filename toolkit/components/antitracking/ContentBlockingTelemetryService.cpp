/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "ContentBlockingTelemetryService.h"

#include "mozilla/ClearOnShutdown.h"
#include "mozilla/PermissionManager.h"
#include "mozilla/Services.h"
#include "mozilla/StaticPtr.h"
#include "mozilla/Telemetry.h"

#include "AntiTrackingLog.h"
#include "prtime.h"

#include "nsIObserverService.h"
#include "nsIPermission.h"
#include "nsTArray.h"

using namespace mozilla;

NS_IMPL_ISUPPORTS(ContentBlockingTelemetryService, nsIObserver)

static StaticRefPtr<ContentBlockingTelemetryService>
    sContentBlockingTelemetryService;

/* static */
already_AddRefed<ContentBlockingTelemetryService>
ContentBlockingTelemetryService::GetSingleton() {
  if (!sContentBlockingTelemetryService) {
    sContentBlockingTelemetryService = new ContentBlockingTelemetryService();
    ClearOnShutdown(&sContentBlockingTelemetryService);
  }

  RefPtr<ContentBlockingTelemetryService> service =
      sContentBlockingTelemetryService;

  return service.forget();
}

NS_IMETHODIMP
ContentBlockingTelemetryService::Observe(nsISupports* aSubject,
                                         const char* aTopic,
                                         const char16_t* aData) {
  if (strcmp(aTopic, "idle-daily") == 0) {
    ReportStoragePermissionExpire();
    return NS_OK;
  }

  return NS_OK;
}

void ContentBlockingTelemetryService::ReportStoragePermissionExpire() {
  MOZ_ASSERT(XRE_IsParentProcess());

  LOG(("Start to report storage permission expire."));

  PermissionManager* permManager = PermissionManager::GetInstance();
  if (NS_WARN_IF(!permManager)) {
    LOG(("Permission manager is null, bailing out early"));
    return;
  }

  nsTArray<RefPtr<nsIPermission>> permissions;
  nsresult rv =
      permManager->GetAllWithTypePrefix("3rdPartyStorage"_ns, permissions);

  if (NS_WARN_IF(NS_FAILED(rv))) {
    LOG(("Fail to get all storage access permissions."));
    return;
  }
  nsTArray<RefPtr<nsIPermission>> framePermissions;
  rv = permManager->GetAllWithTypePrefix("3rdPartyFrameStorage"_ns,
                                         framePermissions);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    LOG(("Fail to get all frame storage access permissions."));
    return;
  }
  if (!permissions.AppendElements(framePermissions, fallible)) {
    LOG(("Fail to combine all storage access permissions."));
    return;
  }

  nsTArray<uint32_t> records;

  for (const auto& permission : permissions) {
    if (!permission) {
      LOG(("Couldn't get the permission for unknown reasons"));
      continue;
    }

    uint32_t expireType;
    rv = permission->GetExpireType(&expireType);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      LOG(("Couldn't get the expire type."));
      continue;
    }

    // We only care about permissions that have a EXPIRE_TIME as the expire
    // type.
    if (expireType != nsIPermissionManager::EXPIRE_TIME) {
      continue;
    }

    // Collect how much longer the storage permission will be valid for, in
    // days.
    int64_t expirationTime = 0;
    rv = permission->GetExpireTime(&expirationTime);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      LOG(("Couldn't get the expire time."));
      continue;
    }

    expirationTime -= (PR_Now() / PR_USEC_PER_MSEC);

    // Skip expired permissions.
    if (expirationTime <= 0) {
      continue;
    }

    int64_t expireDays = expirationTime / 1000 / 60 / 60 / 24;

    records.AppendElement(expireDays);
  }

  if (!records.IsEmpty()) {
    Telemetry::Accumulate(Telemetry::STORAGE_ACCESS_REMAINING_DAYS, records);
  }
}
