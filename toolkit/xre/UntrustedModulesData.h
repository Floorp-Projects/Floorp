/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/. */

#ifndef mozilla_UntrustedModulesData_h
#define mozilla_UntrustedModulesData_h

#if defined(XP_WIN)

#  include "ipc/IPCMessageUtils.h"
#  include "mozilla/CombinedStacks.h"
#  include "mozilla/DebugOnly.h"
#  include "mozilla/Maybe.h"
#  include "mozilla/RefPtr.h"
#  include "mozilla/TypedEnumBits.h"
#  include "mozilla/Unused.h"
#  include "mozilla/Variant.h"
#  include "mozilla/Vector.h"
#  include "mozilla/WinHeaderOnlyUtils.h"
#  include "nsCOMPtr.h"
#  include "nsHashKeys.h"
#  include "nsIFile.h"
#  include "nsISupportsImpl.h"
#  include "nsRefPtrHashtable.h"
#  include "nsString.h"
#  include "nsXULAppAPI.h"

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
    None,
    Signature,
    VersionInfo,
  };

  VendorInfo() : mSource(Source::None) {}
  VendorInfo(const Source aSource, const nsAString& aVendor)
      : mSource(aSource), mVendor(aVendor) {
    MOZ_ASSERT(aSource != Source::None && !aVendor.IsEmpty());
  }

  Source mSource;
  nsString mVendor;
};

class ModulesMap;

class ModuleRecord final {
 public:
  explicit ModuleRecord(const nsAString& aResolvedNtPath);

  NS_INLINE_DECL_THREADSAFE_REFCOUNTING(ModuleRecord)

  nsString mResolvedNtName;
  nsCOMPtr<nsIFile> mResolvedDosName;
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
  ModuleRecord();
  ~ModuleRecord() = default;
  void GetVersionAndVendorInfo(const nsAString& aPath);
  int32_t GetScoreThreshold() const;

  friend struct ::IPC::ParamTraits<ModulesMap>;
};

/**
 * This type holds module path data using one of two internal representations.
 * It may be created from either a nsTHashtable or a Vector, and may be
 * serialized from either representation into a common format over the wire.
 * Deserialization always uses the Vector representation.
 */
struct ModulePaths final {
  using SetType = nsTHashtable<nsStringCaseInsensitiveHashKey>;
  using VecType = Vector<nsString>;

  Variant<SetType, VecType> mModuleNtPaths;

  template <typename T>
  explicit ModulePaths(T&& aPaths)
      : mModuleNtPaths(AsVariant(std::forward<T>(aPaths))) {}

  ModulePaths() : mModuleNtPaths(VecType()) {}

  ModulePaths(const ModulePaths& aOther) = delete;
  ModulePaths(ModulePaths&& aOther) = default;
  ModulePaths& operator=(const ModulePaths&) = delete;
  ModulePaths& operator=(ModulePaths&&) = default;
};

class ProcessedModuleLoadEvent final {
 public:
  ProcessedModuleLoadEvent();
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
  bool mIsDependent;
  uint32_t mLoadStatus;  // corresponding to enum ModuleLoadInfo::Status

  ProcessedModuleLoadEvent(const ProcessedModuleLoadEvent&) = delete;
  ProcessedModuleLoadEvent& operator=(const ProcessedModuleLoadEvent&) = delete;

  ProcessedModuleLoadEvent(ProcessedModuleLoadEvent&&) = default;
  ProcessedModuleLoadEvent& operator=(ProcessedModuleLoadEvent&&) = default;

 private:
  static Maybe<LONGLONG> ComputeQPCTimeStampForProcessCreation();
  static uint64_t QPCTimeStampToProcessUptimeMilliseconds(
      const LARGE_INTEGER& aTimeStamp);
};

// Declaring ModulesMap this way makes it much easier to forward declare than
// if we had used |using| or |typedef|.
class ModulesMap final
    : public nsRefPtrHashtable<nsStringCaseInsensitiveHashKey, ModuleRecord> {
 public:
  ModulesMap()
      : nsRefPtrHashtable<nsStringCaseInsensitiveHashKey, ModuleRecord>() {}
};

class UntrustedModulesData final {
 public:
  // Ensure mEvents will never retain more than kMaxEvents events.
  // This constant matches the maximum in Telemetry::CombinedStacks.
  static constexpr size_t kMaxEvents = 50;

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
  void Merge(UntrustedModulesData&& aNewData);

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

class ModulesMapResult final {
 public:
  ModulesMapResult() : mTrustTestFailures(0) {}

  ModulesMapResult(const ModulesMapResult& aOther) = delete;
  ModulesMapResult(ModulesMapResult&& aOther) = default;
  ModulesMapResult& operator=(const ModulesMapResult& aOther) = delete;
  ModulesMapResult& operator=(ModulesMapResult&& aOther) = default;

  ModulesMap mModules;
  uint32_t mTrustTestFailures;
};

}  // namespace mozilla

namespace IPC {

template <>
struct ParamTraits<mozilla::ModuleVersion> {
  typedef mozilla::ModuleVersion paramType;

  static void Write(Message* aMsg, const paramType& aParam) {
    aMsg->WriteUInt64(aParam.AsInteger());
  }

  static bool Read(const Message* aMsg, PickleIterator* aIter,
                   paramType* aResult) {
    uint64_t ver;
    if (!aMsg->ReadUInt64(aIter, &ver)) {
      return false;
    }

    *aResult = ver;
    return true;
  }
};

template <>
struct ParamTraits<mozilla::VendorInfo> {
  typedef mozilla::VendorInfo paramType;

  static void Write(Message* aMsg, const paramType& aParam) {
    aMsg->WriteUInt32(static_cast<uint32_t>(aParam.mSource));
    WriteParam(aMsg, aParam.mVendor);
  }

  static bool Read(const Message* aMsg, PickleIterator* aIter,
                   paramType* aResult) {
    uint32_t source;
    if (!aMsg->ReadUInt32(aIter, &source)) {
      return false;
    }

    aResult->mSource = static_cast<mozilla::VendorInfo::Source>(source);

    if (!ReadParam(aMsg, aIter, &aResult->mVendor)) {
      return false;
    }

    return true;
  }
};

template <>
struct ParamTraits<mozilla::ModuleRecord> {
  typedef mozilla::ModuleRecord paramType;

  static void Write(Message* aMsg, const paramType& aParam) {
    WriteParam(aMsg, aParam.mResolvedNtName);

    nsAutoString resolvedDosName;
    if (aParam.mResolvedDosName) {
      mozilla::DebugOnly<nsresult> rv =
          aParam.mResolvedDosName->GetPath(resolvedDosName);
      MOZ_ASSERT(NS_SUCCEEDED(rv));
    }

    WriteParam(aMsg, resolvedDosName);
    WriteParam(aMsg, aParam.mSanitizedDllName);
    WriteParam(aMsg, aParam.mVersion);
    WriteParam(aMsg, aParam.mVendorInfo);
    aMsg->WriteUInt32(static_cast<uint32_t>(aParam.mTrustFlags));
  }

  static bool Read(const Message* aMsg, PickleIterator* aIter,
                   paramType* aResult) {
    if (!ReadParam(aMsg, aIter, &aResult->mResolvedNtName)) {
      return false;
    }

    nsAutoString resolvedDosName;
    if (!ReadParam(aMsg, aIter, &resolvedDosName)) {
      return false;
    }

    if (resolvedDosName.IsEmpty()) {
      aResult->mResolvedDosName = nullptr;
    } else if (NS_FAILED(NS_NewLocalFile(
                   resolvedDosName, false,
                   getter_AddRefs(aResult->mResolvedDosName)))) {
      return false;
    }

    if (!ReadParam(aMsg, aIter, &aResult->mSanitizedDllName)) {
      return false;
    }

    if (!ReadParam(aMsg, aIter, &aResult->mVersion)) {
      return false;
    }

    if (!ReadParam(aMsg, aIter, &aResult->mVendorInfo)) {
      return false;
    }

    uint32_t trustFlags;
    if (!aMsg->ReadUInt32(aIter, &trustFlags)) {
      return false;
    }

    aResult->mTrustFlags = static_cast<mozilla::ModuleTrustFlags>(trustFlags);
    return true;
  }
};

template <>
struct ParamTraits<mozilla::ModulesMap> {
  typedef mozilla::ModulesMap paramType;

  static void Write(Message* aMsg, const paramType& aParam) {
    aMsg->WriteUInt32(aParam.Count());

    for (const auto& entry : aParam) {
      MOZ_RELEASE_ASSERT(entry.GetData());
      WriteParam(aMsg, entry.GetKey());
      WriteParam(aMsg, *(entry.GetData()));
    }
  }

  static bool Read(const Message* aMsg, PickleIterator* aIter,
                   paramType* aResult) {
    uint32_t count;
    if (!ReadParam(aMsg, aIter, &count)) {
      return false;
    }

    for (uint32_t current = 0; current < count; ++current) {
      nsAutoString key;
      if (!ReadParam(aMsg, aIter, &key) || key.IsEmpty()) {
        return false;
      }

      RefPtr<mozilla::ModuleRecord> rec(new mozilla::ModuleRecord());
      if (!ReadParam(aMsg, aIter, rec.get())) {
        return false;
      }

      aResult->InsertOrUpdate(key, std::move(rec));
    }

    return true;
  }
};

template <>
struct ParamTraits<mozilla::ModulePaths> {
  typedef mozilla::ModulePaths paramType;

  static void Write(Message* aMsg, const paramType& aParam) {
    aParam.mModuleNtPaths.match(
        [aMsg](const paramType::SetType& aSet) { WriteSet(aMsg, aSet); },
        [aMsg](const paramType::VecType& aVec) { WriteVector(aMsg, aVec); });
  }

  static bool Read(const Message* aMsg, PickleIterator* aIter,
                   paramType* aResult) {
    uint32_t len;
    if (!aMsg->ReadUInt32(aIter, &len)) {
      return false;
    }

    // As noted in the comments for ModulePaths, we only deserialize using the
    // Vector representation.
    auto& vec = aResult->mModuleNtPaths.as<paramType::VecType>();
    if (!vec.reserve(len)) {
      return false;
    }

    for (uint32_t idx = 0; idx < len; ++idx) {
      nsString str;
      if (!ReadParam(aMsg, aIter, &str)) {
        return false;
      }

      if (!vec.emplaceBack(std::move(str))) {
        return false;
      }
    }

    return true;
  }

 private:
  // NB: This function must write out the set in the same format as WriteVector
  static void WriteSet(Message* aMsg, const paramType::SetType& aSet) {
    aMsg->WriteUInt32(aSet.Count());
    for (const auto& key : aSet.Keys()) {
      WriteParam(aMsg, key);
    }
  }

  // NB: This function must write out the vector in the same format as WriteSet
  static void WriteVector(Message* aMsg, const paramType::VecType& aVec) {
    aMsg->WriteUInt32(aVec.length());
    for (auto const& item : aVec) {
      WriteParam(aMsg, item);
    }
  }
};

template <>
struct ParamTraits<mozilla::UntrustedModulesData> {
  typedef mozilla::UntrustedModulesData paramType;

  static void Write(Message* aMsg, const paramType& aParam) {
    aMsg->WriteUInt32(aParam.mProcessType);
    aMsg->WriteULong(aParam.mPid);
    WriteParam(aMsg, aParam.mElapsed);
    WriteParam(aMsg, aParam.mModules);

    aMsg->WriteUInt32(aParam.mEvents.length());
    for (auto& evt : aParam.mEvents) {
      WriteEvent(aMsg, evt);
    }

    WriteParam(aMsg, aParam.mStacks);
    WriteParam(aMsg, aParam.mXULLoadDurationMS);
    aMsg->WriteUInt32(aParam.mSanitizationFailures);
    aMsg->WriteUInt32(aParam.mTrustTestFailures);
  }

  static bool Read(const Message* aMsg, PickleIterator* aIter,
                   paramType* aResult) {
    uint32_t processType;
    if (!aMsg->ReadUInt32(aIter, &processType)) {
      return false;
    }

    aResult->mProcessType = static_cast<GeckoProcessType>(processType);

    if (!aMsg->ReadULong(aIter, &aResult->mPid)) {
      return false;
    }

    if (!ReadParam(aMsg, aIter, &aResult->mElapsed)) {
      return false;
    }

    if (!ReadParam(aMsg, aIter, &aResult->mModules)) {
      return false;
    }

    // We read mEvents manually so that we can use ReadEvent defined below.
    uint32_t eventsLen;
    if (!ReadParam(aMsg, aIter, &eventsLen)) {
      return false;
    }

    if (!aResult->mEvents.resize(eventsLen)) {
      return false;
    }

    for (uint32_t curEventIdx = 0; curEventIdx < eventsLen; ++curEventIdx) {
      if (!ReadEvent(aMsg, aIter, &(aResult->mEvents[curEventIdx]),
                     aResult->mModules)) {
        return false;
      }
    }

    if (!ReadParam(aMsg, aIter, &aResult->mStacks)) {
      return false;
    }

    if (!ReadParam(aMsg, aIter, &aResult->mXULLoadDurationMS)) {
      return false;
    }

    if (!aMsg->ReadUInt32(aIter, &aResult->mSanitizationFailures)) {
      return false;
    }

    if (!aMsg->ReadUInt32(aIter, &aResult->mTrustTestFailures)) {
      return false;
    }

    return true;
  }

 private:
  // Because ProcessedModuleLoadEvent depends on a hash table from
  // UntrustedModulesData, we do its serialization as part of this
  // specialization.
  static void WriteEvent(Message* aMsg,
                         const mozilla::ProcessedModuleLoadEvent& aParam) {
    aMsg->WriteUInt64(aParam.mProcessUptimeMS);
    WriteParam(aMsg, aParam.mLoadDurationMS);
    aMsg->WriteULong(aParam.mThreadId);
    WriteParam(aMsg, aParam.mThreadName);
    WriteParam(aMsg, aParam.mRequestedDllName);
    WriteParam(aMsg, aParam.mBaseAddress);
    WriteParam(aMsg, aParam.mIsDependent);
    WriteParam(aMsg, aParam.mLoadStatus);

    // We don't write the ModuleRecord directly; we write its key into the
    // UntrustedModulesData::mModules hash table.
    MOZ_ASSERT(aParam.mModule && !aParam.mModule->mResolvedNtName.IsEmpty());
    WriteParam(aMsg, aParam.mModule->mResolvedNtName);
  }

  // Because ProcessedModuleLoadEvent depends on a hash table from
  // UntrustedModulesData, we do its deserialization as part of this
  // specialization.
  static bool ReadEvent(const Message* aMsg, PickleIterator* aIter,
                        mozilla::ProcessedModuleLoadEvent* aResult,
                        const mozilla::ModulesMap& aModulesMap) {
    if (!aMsg->ReadUInt64(aIter, &aResult->mProcessUptimeMS)) {
      return false;
    }

    if (!ReadParam(aMsg, aIter, &aResult->mLoadDurationMS)) {
      return false;
    }

    if (!aMsg->ReadULong(aIter, &aResult->mThreadId)) {
      return false;
    }

    if (!ReadParam(aMsg, aIter, &aResult->mThreadName)) {
      return false;
    }

    if (!ReadParam(aMsg, aIter, &aResult->mRequestedDllName)) {
      return false;
    }

    if (!ReadParam(aMsg, aIter, &aResult->mBaseAddress)) {
      return false;
    }

    if (!ReadParam(aMsg, aIter, &aResult->mIsDependent)) {
      return false;
    }

    if (!ReadParam(aMsg, aIter, &aResult->mLoadStatus)) {
      return false;
    }

    nsAutoString resolvedNtName;
    if (!ReadParam(aMsg, aIter, &resolvedNtName)) {
      return false;
    }

    // NB: While bad data integrity might for some reason result in a null
    // mModule, we do not fail the deserialization; this is a data error,
    // rather than an IPC error. The error is detected and dealt with in
    // telemetry.
    aResult->mModule = aModulesMap.Get(resolvedNtName);

    return true;
  }
};

template <>
struct ParamTraits<mozilla::ModulesMapResult> {
  typedef mozilla::ModulesMapResult paramType;

  static void Write(Message* aMsg, const paramType& aParam) {
    WriteParam(aMsg, aParam.mModules);
    aMsg->WriteUInt32(aParam.mTrustTestFailures);
  }

  static bool Read(const Message* aMsg, PickleIterator* aIter,
                   paramType* aResult) {
    if (!ReadParam(aMsg, aIter, &aResult->mModules)) {
      return false;
    }

    if (!aMsg->ReadUInt32(aIter, &aResult->mTrustTestFailures)) {
      return false;
    }

    return true;
  }
};

}  // namespace IPC

#else  // defined(XP_WIN)

namespace mozilla {

// For compiling IPDL on non-Windows platforms
using UntrustedModulesData = uint32_t;
using ModulePaths = uint32_t;
using ModulesMapResult = uint32_t;

}  // namespace mozilla

#endif  // defined(XP_WIN)

#endif  // mozilla_UntrustedModulesData_h
