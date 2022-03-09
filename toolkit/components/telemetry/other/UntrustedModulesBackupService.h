/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/. */

#ifndef mozilla_UntrustedModulesBackupService_h
#define mozilla_UntrustedModulesBackupService_h

#include "mozilla/UntrustedModulesData.h"
#include "nsRefPtrHashtable.h"

namespace mozilla {

struct ProcessHashKey {
  GeckoProcessType mType;
  DWORD mPid;
  ProcessHashKey(GeckoProcessType aType, DWORD aPid);
  bool operator==(const ProcessHashKey& aOther) const;
  PLDHashNumber Hash() const;
};

// UntrustedModulesData should not be refcounted as it's exchanged via IPC.
// Instead, we define this container class owning UntrustedModulesData along
// with a refcount.
class MOZ_HEAP_CLASS UntrustedModulesDataContainer final {
  ~UntrustedModulesDataContainer() = default;

 public:
  UntrustedModulesData mData;

  explicit UntrustedModulesDataContainer(UntrustedModulesData&& aData)
      : mData(std::move(aData)) {}

  NS_INLINE_DECL_THREADSAFE_REFCOUNTING(UntrustedModulesDataContainer)
};

class UntrustedModulesBackupData
    : public nsRefPtrHashtable<nsGenericHashKey<ProcessHashKey>,
                               UntrustedModulesDataContainer> {
 public:
  void Add(UntrustedModulesData&& aData);
  void AddWithoutStacks(UntrustedModulesData&& aData);
};

class MOZ_HEAP_CLASS UntrustedModulesBackupService final {
 public:
  NS_INLINE_DECL_THREADSAFE_REFCOUNTING(UntrustedModulesBackupService)

  static UntrustedModulesBackupService* Get();

  // Back up data to mStaging
  void Backup(UntrustedModulesData&& aData);

  void SettleAllStagingData();

  const UntrustedModulesBackupData& Staging() const { return mStaging; }
  const UntrustedModulesBackupData& Settled() const { return mSettled; }

 private:
  // Data not yet submitted as telemetry
  UntrustedModulesBackupData mStaging;

  // Data already submitted as telemetry
  // (This does not have stack information)
  UntrustedModulesBackupData mSettled;

  ~UntrustedModulesBackupService() = default;
};

}  // namespace mozilla

#endif  // mozilla_UntrustedModulesBackupService_h
