/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/. */

#include "UntrustedModulesBackupService.h"

#include "mozilla/ClearOnShutdown.h"
#include "mozilla/HashFunctions.h"
#include "mozilla/SchedulerGroup.h"
#include "mozilla/StaticLocalPtr.h"

namespace mozilla {

ProcessHashKey::ProcessHashKey(GeckoProcessType aType, DWORD aPid)
    : mType(aType), mPid(aPid) {}

bool ProcessHashKey::operator==(const ProcessHashKey& aOther) const {
  return mPid == aOther.mPid && mType == aOther.mType;
}

PLDHashNumber ProcessHashKey::Hash() const { return HashGeneric(mPid, mType); }

void UntrustedModulesBackupData::Add(UntrustedModulesData&& aData) {
  WithEntryHandle(
      ProcessHashKey(aData.mProcessType, aData.mPid), [&](auto&& p) {
        if (p) {
          p.Data()->mData.Merge(std::move(aData));
        } else {
          p.Insert(MakeRefPtr<UntrustedModulesDataContainer>(std::move(aData)));
        }
      });
}

void UntrustedModulesBackupData::AddWithoutStacks(
    UntrustedModulesData&& aData) {
  WithEntryHandle(
      ProcessHashKey(aData.mProcessType, aData.mPid), [&](auto&& p) {
        if (p) {
          p.Data()->mData.MergeWithoutStacks(std::move(aData));
        } else {
          aData.Truncate(true);
          p.Insert(MakeRefPtr<UntrustedModulesDataContainer>(std::move(aData)));
        }
      });
}

/* static */
UntrustedModulesBackupService* UntrustedModulesBackupService::Get() {
  if (!XRE_IsParentProcess()) {
    return nullptr;
  }

  static StaticLocalRefPtr<UntrustedModulesBackupService> sInstance(
      []() -> already_AddRefed<UntrustedModulesBackupService> {
        RefPtr<UntrustedModulesBackupService> instance(
            new UntrustedModulesBackupService());

        auto setClearOnShutdown = [ptr = &sInstance]() -> void {
          ClearOnShutdown(ptr);
        };

        if (NS_IsMainThread()) {
          setClearOnShutdown();
          return instance.forget();
        }

        SchedulerGroup::Dispatch(NS_NewRunnableFunction(
            "mozilla::UntrustedModulesBackupService::Get",
            std::move(setClearOnShutdown)));

        return instance.forget();
      }());

  return sInstance;
}

void UntrustedModulesBackupService::Backup(UntrustedModulesData&& aData) {
  mStaging.Add(std::move(aData));
}

void UntrustedModulesBackupService::SettleAllStagingData() {
  UntrustedModulesBackupData staging;
  staging.SwapElements(mStaging);

  for (auto&& iter = staging.Iter(); !iter.Done(); iter.Next()) {
    if (!iter.Data()) {
      continue;
    }
    mSettled.AddWithoutStacks(std::move(iter.Data()->mData));
  }
}

}  // namespace mozilla
