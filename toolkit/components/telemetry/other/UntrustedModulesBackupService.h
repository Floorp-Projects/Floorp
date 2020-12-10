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
};

class MOZ_HEAP_CLASS UntrustedModulesBackupService final {
 public:
  enum class BackupType : uint32_t {
    Staging = 0,
    Settled,

    Count
  };

  NS_INLINE_DECL_THREADSAFE_REFCOUNTING(UntrustedModulesBackupService)

  static UntrustedModulesBackupService* Get();
  void Backup(BackupType aType, UntrustedModulesData&& aData);
  void SettleAllStagingData();
  const UntrustedModulesBackupData& Ref(BackupType aType) const;

 private:
  UntrustedModulesBackupData mBackup[static_cast<uint32_t>(BackupType::Count)];

  ~UntrustedModulesBackupService() = default;
};

}  // namespace mozilla

#endif  // mozilla_UntrustedModulesBackupService_h
