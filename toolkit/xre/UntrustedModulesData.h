/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/. */

#ifndef mozilla_UntrustedModulesData_h
#define mozilla_UntrustedModulesData_h

#include "mozilla/CombinedStacks.h"
#include "mozilla/Maybe.h"
#include "mozilla/RefPtr.h"
#include "mozilla/TypedEnumBits.h"
#include "mozilla/Vector.h"
#include "mozilla/WinHeaderOnlyUtils.h"
#include "nsCOMPtr.h"
#include "nsIFile.h"
#include "nsISupportsImpl.h"
#include "nsRefPtrHashtable.h"
#include "nsString.h"
#include "nsXULAppAPI.h"

namespace mozilla {
namespace glue {
struct EnhancedModuleLoadInfo;
}  // namespace glue

enum class ModuleTrustFlags : uint32_t {
  None = 0,
  MozillaSignature = 1,
  MicrosoftWindowsSignature = 2,
  MicrosoftVersion = 4,
  FirefoxDirectory = 8,
  FirefoxDirectoryAndVersion = 0x10,
  SystemDirectory = 0x20,
  KeyboardLayout = 0x40,
  JitPI = 0x80,
  WinSxSDirectory = 0x100,
};

MOZ_MAKE_ENUM_CLASS_BITWISE_OPERATORS(ModuleTrustFlags);

class VendorInfo final {
 public:
  enum class Source : uint32_t {
    Signature,
    VersionInfo,
  };

  VendorInfo(const Source aSource, const nsAString& aVendor)
      : mSource(aSource), mVendor(aVendor) {
    MOZ_ASSERT(!aVendor.IsEmpty());
  }

  Source mSource;
  nsString mVendor;
};

class ModuleRecord final {
 public:
  explicit ModuleRecord(const nsAString& aResolvedPath);

  NS_INLINE_DECL_THREADSAFE_REFCOUNTING(ModuleRecord)

  nsCOMPtr<nsIFile> mResolvedDllName;
  nsString mSanitizedDllName;
  Maybe<ModuleVersion> mVersion;
  Maybe<VendorInfo> mVendorInfo;
  ModuleTrustFlags mTrustFlags;

  explicit operator bool() const { return !mSanitizedDllName.IsEmpty(); }
  bool IsXUL() const;
  bool IsTrusted() const;

  ModuleRecord(const ModuleRecord&) = delete;
  ModuleRecord(ModuleRecord&&) = delete;

  ModuleRecord& operator=(const ModuleRecord&) = delete;
  ModuleRecord& operator=(ModuleRecord&&) = delete;

 private:
  ~ModuleRecord() = default;
  void GetVersionAndVendorInfo(const nsAString& aPath);
  int32_t GetScoreThreshold() const;
};

class ProcessedModuleLoadEvent final {
 public:
  ProcessedModuleLoadEvent(glue::EnhancedModuleLoadInfo&& aModLoadInfo,
                           RefPtr<ModuleRecord>&& aModuleRecord);

  explicit operator bool() const { return mModule && *mModule; }
  bool IsXULLoad() const;
  bool IsTrusted() const;

  uint64_t mProcessUptimeMS;
  Maybe<double> mLoadDurationMS;
  DWORD mThreadId;
  nsCString mThreadName;
  nsString mRequestedDllName;
  // We intentionally store mBaseAddress as part of the event and not the
  // module, as relocation may cause it to change between loads. If so, we want
  // to know about it.
  uintptr_t mBaseAddress;
  RefPtr<ModuleRecord> mModule;

  ProcessedModuleLoadEvent(const ProcessedModuleLoadEvent&) = delete;
  ProcessedModuleLoadEvent& operator=(const ProcessedModuleLoadEvent&) = delete;

  ProcessedModuleLoadEvent(ProcessedModuleLoadEvent&&) = default;
  ProcessedModuleLoadEvent& operator=(ProcessedModuleLoadEvent&&) = default;

 private:
  static Maybe<LONGLONG> ComputeQPCTimeStampForProcessCreation();
  static uint64_t QPCTimeStampToProcessUptimeMilliseconds(
      const LARGE_INTEGER& aTimeStamp);
};

class UntrustedModulesData final {
 public:
  using ModulesMap = nsRefPtrHashtable<nsStringHashKey, ModuleRecord>;

  UntrustedModulesData()
      : mProcessType(XRE_GetProcessType()),
        mPid(::GetCurrentProcessId()),
        mSanitizationFailures(0),
        mTrustTestFailures(0) {}

  UntrustedModulesData(UntrustedModulesData&&) = default;
  UntrustedModulesData& operator=(UntrustedModulesData&&) = default;

  UntrustedModulesData(const UntrustedModulesData&) = delete;
  UntrustedModulesData& operator=(const UntrustedModulesData&) = delete;

  explicit operator bool() const {
    return !mEvents.empty() || mSanitizationFailures || mTrustTestFailures ||
           mXULLoadDurationMS.isSome();
  }

  void AddNewLoads(const ModulesMap& aModulesMap,
                   Vector<ProcessedModuleLoadEvent>&& aEvents,
                   Vector<Telemetry::ProcessedStack>&& aStacks);

  void Swap(UntrustedModulesData& aOther);

  GeckoProcessType mProcessType;
  DWORD mPid;
  TimeDuration mElapsed;
  ModulesMap mModules;
  Vector<ProcessedModuleLoadEvent> mEvents;
  Telemetry::CombinedStacks mStacks;
  Maybe<double> mXULLoadDurationMS;
  uint32_t mSanitizationFailures;
  uint32_t mTrustTestFailures;
};

}  // namespace mozilla

#endif  // mozilla_UntrustedModulesData_h
