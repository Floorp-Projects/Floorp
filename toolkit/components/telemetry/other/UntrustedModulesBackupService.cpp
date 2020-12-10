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
  auto p = LookupForAdd(ProcessHashKey(aData.mProcessType, aData.mPid));
  if (p) {
    p.Data()->mData.Merge(std::move(aData));
  } else {
    auto data = MakeRefPtr<UntrustedModulesDataContainer>(std::move(aData));
    p.OrInsert([data = std::move(data)]() { return data; });
  }
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

        SchedulerGroup::Dispatch(
            TaskCategory::Other,
            NS_NewRunnableFunction(
                "mozilla::UntrustedModulesBackupService::Get",
                std::move(setClearOnShutdown)));

        return instance.forget();
      }());

  return sInstance;
}

void UntrustedModulesBackupService::Backup(UntrustedModulesData&& aData) {
  mBackup.Add(std::move(aData));
}

const UntrustedModulesBackupData& UntrustedModulesBackupService::Ref() const {
  return mBackup;
}

}  // namespace mozilla
