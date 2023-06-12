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
#  include "mozilla/LinkedList.h"
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
  VendorInfo(const Source aSource, const nsAString& aVendor,
             bool aHasNestedMicrosoftSignature)
      : mSource(aSource),
        mVendor(aVendor),
        mHasNestedMicrosoftSignature(aHasNestedMicrosoftSignature) {
    MOZ_ASSERT(aSource != Source::None && !aVendor.IsEmpty());
  }

  Source mSource;
  nsString mVendor;
  bool mHasNestedMicrosoftSignature;
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

  void SanitizeRequestedDllName();

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

struct ProcessedModuleLoadEventContainer final
    : public LinkedListElement<ProcessedModuleLoadEventContainer> {
  ProcessedModuleLoadEvent mEvent;
  ProcessedModuleLoadEventContainer() = default;
  explicit ProcessedModuleLoadEventContainer(ProcessedModuleLoadEvent&& aEvent)
      : mEvent(std::move(aEvent)) {}

  ProcessedModuleLoadEventContainer(ProcessedModuleLoadEventContainer&&) =
      default;
  ProcessedModuleLoadEventContainer& operator=(
      ProcessedModuleLoadEventContainer&&) = default;
  ProcessedModuleLoadEventContainer(const ProcessedModuleLoadEventContainer&) =
      delete;
  ProcessedModuleLoadEventContainer& operator=(
      const ProcessedModuleLoadEventContainer&) = delete;
};
using UntrustedModuleLoadingEvents =
    AutoCleanLinkedList<ProcessedModuleLoadEventContainer>;

class UntrustedModulesData final {
  // Merge aNewData.mEvents into this->mModules and also
  // make module entries in aNewData point to items in this->mModules.
  void MergeModules(UntrustedModulesData& aNewData);

 public:
  // Ensure mEvents will never retain more than kMaxEvents events.
  // This constant matches the maximum in Telemetry::CombinedStacks.
  // Truncate() relies on these being the same.
  static constexpr size_t kMaxEvents = 50;

  UntrustedModulesData()
      : mProcessType(XRE_GetProcessType()),
        mPid(::GetCurrentProcessId()),
        mNumEvents(0),
        mSanitizationFailures(0),
        mTrustTestFailures(0) {
    MOZ_ASSERT(kMaxEvents == mStacks.GetMaxStacksCount());
  }

  UntrustedModulesData(UntrustedModulesData&&) = default;
  UntrustedModulesData& operator=(UntrustedModulesData&&) = default;

  UntrustedModulesData(const UntrustedModulesData&) = delete;
  UntrustedModulesData& operator=(const UntrustedModulesData&) = delete;

  explicit operator bool() const {
    return !mEvents.isEmpty() || mSanitizationFailures || mTrustTestFailures ||
           mXULLoadDurationMS.isSome();
  }

  void AddNewLoads(const ModulesMap& aModulesMap,
                   UntrustedModuleLoadingEvents&& aEvents,
                   Vector<Telemetry::ProcessedStack>&& aStacks);
  void Merge(UntrustedModulesData&& aNewData);
  void MergeWithoutStacks(UntrustedModulesData&& aNewData);
  void Swap(UntrustedModulesData& aOther);

  // Drop callstack data and old loading events.
  void Truncate(bool aDropCallstackData);

  GeckoProcessType mProcessType;
  DWORD mPid;
  TimeDuration mElapsed;
  ModulesMap mModules;
  uint32_t mNumEvents;
  UntrustedModuleLoadingEvents mEvents;
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

  static void Write(MessageWriter* aWriter, const paramType& aParam) {
    aWriter->WriteUInt64(aParam.AsInteger());
  }

  static bool Read(MessageReader* aReader, paramType* aResult) {
    uint64_t ver;
    if (!aReader->ReadUInt64(&ver)) {
      return false;
    }

    *aResult = ver;
    return true;
  }
};

template <>
struct ParamTraits<mozilla::VendorInfo> {
  typedef mozilla::VendorInfo paramType;

  static void Write(MessageWriter* aWriter, const paramType& aParam) {
    aWriter->WriteUInt32(static_cast<uint32_t>(aParam.mSource));
    WriteParam(aWriter, aParam.mVendor);
    WriteParam(aWriter, aParam.mHasNestedMicrosoftSignature);
  }

  static bool Read(MessageReader* aReader, paramType* aResult) {
    uint32_t source;
    if (!aReader->ReadUInt32(&source)) {
      return false;
    }

    aResult->mSource = static_cast<mozilla::VendorInfo::Source>(source);

    if (!ReadParam(aReader, &aResult->mVendor)) {
      return false;
    }

    if (!ReadParam(aReader, &aResult->mHasNestedMicrosoftSignature)) {
      return false;
    }

    return true;
  }
};

template <>
struct ParamTraits<mozilla::ModuleRecord> {
  typedef mozilla::ModuleRecord paramType;

  static void Write(MessageWriter* aWriter, const paramType& aParam) {
    WriteParam(aWriter, aParam.mResolvedNtName);

    nsAutoString resolvedDosName;
    if (aParam.mResolvedDosName) {
      mozilla::DebugOnly<nsresult> rv =
          aParam.mResolvedDosName->GetPath(resolvedDosName);
      MOZ_ASSERT(NS_SUCCEEDED(rv));
    }

    WriteParam(aWriter, resolvedDosName);
    WriteParam(aWriter, aParam.mSanitizedDllName);
    WriteParam(aWriter, aParam.mVersion);
    WriteParam(aWriter, aParam.mVendorInfo);
    aWriter->WriteUInt32(static_cast<uint32_t>(aParam.mTrustFlags));
  }

  static bool Read(MessageReader* aReader, paramType* aResult) {
    if (!ReadParam(aReader, &aResult->mResolvedNtName)) {
      return false;
    }

    nsAutoString resolvedDosName;
    if (!ReadParam(aReader, &resolvedDosName)) {
      return false;
    }

    if (resolvedDosName.IsEmpty()) {
      aResult->mResolvedDosName = nullptr;
    } else if (NS_FAILED(NS_NewLocalFile(
                   resolvedDosName, false,
                   getter_AddRefs(aResult->mResolvedDosName)))) {
      return false;
    }

    if (!ReadParam(aReader, &aResult->mSanitizedDllName)) {
      return false;
    }

    if (!ReadParam(aReader, &aResult->mVersion)) {
      return false;
    }

    if (!ReadParam(aReader, &aResult->mVendorInfo)) {
      return false;
    }

    uint32_t trustFlags;
    if (!aReader->ReadUInt32(&trustFlags)) {
      return false;
    }

    aResult->mTrustFlags = static_cast<mozilla::ModuleTrustFlags>(trustFlags);
    return true;
  }
};

template <>
struct ParamTraits<mozilla::ModulesMap> {
  typedef mozilla::ModulesMap paramType;

  static void Write(MessageWriter* aWriter, const paramType& aParam) {
    aWriter->WriteUInt32(aParam.Count());

    for (const auto& entry : aParam) {
      MOZ_RELEASE_ASSERT(entry.GetData());
      WriteParam(aWriter, entry.GetKey());
      WriteParam(aWriter, *(entry.GetData()));
    }
  }

  static bool Read(MessageReader* aReader, paramType* aResult) {
    uint32_t count;
    if (!ReadParam(aReader, &count)) {
      return false;
    }

    for (uint32_t current = 0; current < count; ++current) {
      nsAutoString key;
      if (!ReadParam(aReader, &key) || key.IsEmpty()) {
        return false;
      }

      RefPtr<mozilla::ModuleRecord> rec(new mozilla::ModuleRecord());
      if (!ReadParam(aReader, rec.get())) {
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

  static void Write(MessageWriter* aWriter, const paramType& aParam) {
    aParam.mModuleNtPaths.match(
        [aWriter](const paramType::SetType& aSet) { WriteSet(aWriter, aSet); },
        [aWriter](const paramType::VecType& aVec) {
          WriteVector(aWriter, aVec);
        });
  }

  static bool Read(MessageReader* aReader, paramType* aResult) {
    uint32_t len;
    if (!aReader->ReadUInt32(&len)) {
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
      if (!ReadParam(aReader, &str)) {
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
  static void WriteSet(MessageWriter* aWriter, const paramType::SetType& aSet) {
    aWriter->WriteUInt32(aSet.Count());
    for (const auto& key : aSet.Keys()) {
      WriteParam(aWriter, key);
    }
  }

  // NB: This function must write out the vector in the same format as WriteSet
  static void WriteVector(MessageWriter* aWriter,
                          const paramType::VecType& aVec) {
    aWriter->WriteUInt32(aVec.length());
    for (auto const& item : aVec) {
      WriteParam(aWriter, item);
    }
  }
};

template <>
struct ParamTraits<mozilla::UntrustedModulesData> {
  typedef mozilla::UntrustedModulesData paramType;

  static void Write(MessageWriter* aWriter, const paramType& aParam) {
    aWriter->WriteUInt32(aParam.mProcessType);
    aWriter->WriteULong(aParam.mPid);
    WriteParam(aWriter, aParam.mElapsed);
    WriteParam(aWriter, aParam.mModules);

    aWriter->WriteUInt32(aParam.mNumEvents);
    for (auto event : aParam.mEvents) {
      WriteEvent(aWriter, event->mEvent);
    }

    WriteParam(aWriter, aParam.mStacks);
    WriteParam(aWriter, aParam.mXULLoadDurationMS);
    aWriter->WriteUInt32(aParam.mSanitizationFailures);
    aWriter->WriteUInt32(aParam.mTrustTestFailures);
  }

  static bool Read(MessageReader* aReader, paramType* aResult) {
    uint32_t processType;
    if (!aReader->ReadUInt32(&processType)) {
      return false;
    }

    aResult->mProcessType = static_cast<GeckoProcessType>(processType);

    if (!aReader->ReadULong(&aResult->mPid)) {
      return false;
    }

    if (!ReadParam(aReader, &aResult->mElapsed)) {
      return false;
    }

    if (!ReadParam(aReader, &aResult->mModules)) {
      return false;
    }

    // We read mEvents manually so that we can use ReadEvent defined below.
    if (!ReadParam(aReader, &aResult->mNumEvents)) {
      return false;
    }

    for (uint32_t curEventIdx = 0; curEventIdx < aResult->mNumEvents;
         ++curEventIdx) {
      auto newEvent =
          mozilla::MakeUnique<mozilla::ProcessedModuleLoadEventContainer>();
      if (!ReadEvent(aReader, &newEvent->mEvent, aResult->mModules)) {
        return false;
      }
      aResult->mEvents.insertBack(newEvent.release());
    }

    if (!ReadParam(aReader, &aResult->mStacks)) {
      return false;
    }

    if (!ReadParam(aReader, &aResult->mXULLoadDurationMS)) {
      return false;
    }

    if (!aReader->ReadUInt32(&aResult->mSanitizationFailures)) {
      return false;
    }

    if (!aReader->ReadUInt32(&aResult->mTrustTestFailures)) {
      return false;
    }

    return true;
  }

 private:
  // Because ProcessedModuleLoadEvent depends on a hash table from
  // UntrustedModulesData, we do its serialization as part of this
  // specialization.
  static void WriteEvent(MessageWriter* aWriter,
                         const mozilla::ProcessedModuleLoadEvent& aParam) {
    aWriter->WriteUInt64(aParam.mProcessUptimeMS);
    WriteParam(aWriter, aParam.mLoadDurationMS);
    aWriter->WriteULong(aParam.mThreadId);
    WriteParam(aWriter, aParam.mThreadName);
    WriteParam(aWriter, aParam.mRequestedDllName);
    WriteParam(aWriter, aParam.mBaseAddress);
    WriteParam(aWriter, aParam.mIsDependent);
    WriteParam(aWriter, aParam.mLoadStatus);

    // We don't write the ModuleRecord directly; we write its key into the
    // UntrustedModulesData::mModules hash table.
    MOZ_ASSERT(aParam.mModule && !aParam.mModule->mResolvedNtName.IsEmpty());
    WriteParam(aWriter, aParam.mModule->mResolvedNtName);
  }

  // Because ProcessedModuleLoadEvent depends on a hash table from
  // UntrustedModulesData, we do its deserialization as part of this
  // specialization.
  static bool ReadEvent(MessageReader* aReader,
                        mozilla::ProcessedModuleLoadEvent* aResult,
                        const mozilla::ModulesMap& aModulesMap) {
    if (!aReader->ReadUInt64(&aResult->mProcessUptimeMS)) {
      return false;
    }

    if (!ReadParam(aReader, &aResult->mLoadDurationMS)) {
      return false;
    }

    if (!aReader->ReadULong(&aResult->mThreadId)) {
      return false;
    }

    if (!ReadParam(aReader, &aResult->mThreadName)) {
      return false;
    }

    if (!ReadParam(aReader, &aResult->mRequestedDllName)) {
      return false;
    }

    // When ProcessedModuleLoadEvent was constructed in a child process, we left
    // mRequestedDllName unsanitized, so now is a good time to sanitize it.
    aResult->SanitizeRequestedDllName();

    if (!ReadParam(aReader, &aResult->mBaseAddress)) {
      return false;
    }

    if (!ReadParam(aReader, &aResult->mIsDependent)) {
      return false;
    }

    if (!ReadParam(aReader, &aResult->mLoadStatus)) {
      return false;
    }

    nsAutoString resolvedNtName;
    if (!ReadParam(aReader, &resolvedNtName)) {
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

  static void Write(MessageWriter* aWriter, const paramType& aParam) {
    WriteParam(aWriter, aParam.mModules);
    aWriter->WriteUInt32(aParam.mTrustTestFailures);
  }

  static bool Read(MessageReader* aReader, paramType* aResult) {
    if (!ReadParam(aReader, &aResult->mModules)) {
      return false;
    }

    if (!aReader->ReadUInt32(&aResult->mTrustTestFailures)) {
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
