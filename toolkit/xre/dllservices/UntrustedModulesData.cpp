/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/. */

#include "UntrustedModulesData.h"

#include <windows.h>

#include "mozilla/CmdLineAndEnvUtils.h"
#include "mozilla/DynamicallyLinkedFunctionPtr.h"
#include "mozilla/FileUtilsWin.h"
#include "mozilla/Likely.h"
#include "mozilla/MathAlgorithms.h"
#include "mozilla/UniquePtr.h"
#include "mozilla/Unused.h"
#include "mozilla/WinDllServices.h"
#include "ModuleEvaluator.h"
#include "nsCOMPtr.h"
#include "nsDebug.h"
#include "nsXULAppAPI.h"
#include "WinUtils.h"

// Some utility functions

static LONGLONG GetQPCFreq() {
  static const LONGLONG sFreq = []() -> LONGLONG {
    LARGE_INTEGER freq;
    ::QueryPerformanceFrequency(&freq);
    return freq.QuadPart;
  }();

  return sFreq;
}

template <typename ReturnT>
static ReturnT QPCToTimeUnits(const LONGLONG aTimeStamp,
                              const LONGLONG aUnitsPerSec) {
  return ReturnT(aTimeStamp * aUnitsPerSec) / ReturnT(GetQPCFreq());
}

template <typename ReturnT>
static ReturnT QPCToMilliseconds(const LONGLONG aTimeStamp) {
  const LONGLONG kMillisecondsPerSec = 1000;
  return QPCToTimeUnits<ReturnT>(aTimeStamp, kMillisecondsPerSec);
}

template <typename ReturnT>
static ReturnT QPCToMicroseconds(const LONGLONG aTimeStamp) {
  const LONGLONG kMicrosecondsPerSec = 1000000;
  return QPCToTimeUnits<ReturnT>(aTimeStamp, kMicrosecondsPerSec);
}

static LONGLONG TimeUnitsToQPC(const LONGLONG aTimeStamp,
                               const LONGLONG aUnitsPerSec) {
  MOZ_ASSERT(aUnitsPerSec != 0);

  LONGLONG result = aTimeStamp;
  result *= GetQPCFreq();
  result /= aUnitsPerSec;
  return result;
}

namespace mozilla {

static Maybe<double> QPCLoadDurationToMilliseconds(
    const ModuleLoadInfo& aNtInfo) {
  if (aNtInfo.IsBare()) {
    return Nothing();
  }

  return Some(QPCToMilliseconds<double>(aNtInfo.mLoadTimeInfo.QuadPart));
}

ModuleRecord::ModuleRecord() : mTrustFlags(ModuleTrustFlags::None) {}

ModuleRecord::ModuleRecord(const nsAString& aResolvedNtPath)
    : mResolvedNtName(aResolvedNtPath), mTrustFlags(ModuleTrustFlags::None) {
  if (aResolvedNtPath.IsEmpty()) {
    return;
  }

  MOZ_ASSERT(XRE_IsParentProcess());

  nsAutoString resolvedDosPath;
  if (!NtPathToDosPath(aResolvedNtPath, resolvedDosPath)) {
#if defined(DEBUG)
    nsAutoCString msg;
    msg.AppendLiteral("NtPathToDosPath failed for path \"");
    msg.Append(NS_ConvertUTF16toUTF8(aResolvedNtPath));
    msg.AppendLiteral("\"");
    NS_WARNING(msg.get());
#endif  // defined(DEBUG)
    return;
  }

  nsresult rv =
      NS_NewLocalFile(resolvedDosPath, false, getter_AddRefs(mResolvedDosName));
  if (NS_FAILED(rv) || !mResolvedDosName) {
    return;
  }

  GetVersionAndVendorInfo(resolvedDosPath);

  // Now sanitize the resolved DLL name. If we cannot sanitize this then this
  // record must not be considered valid.
  nsAutoString strSanitizedPath(resolvedDosPath);
  if (!widget::WinUtils::PreparePathForTelemetry(strSanitizedPath)) {
    return;
  }

  mSanitizedDllName = strSanitizedPath;
}

void ModuleRecord::GetVersionAndVendorInfo(const nsAString& aPath) {
  RefPtr<DllServices> dllSvc(DllServices::Get());

  // WinVerifyTrust is too slow and of limited utility for our purposes, so
  // we pass SkipTrustVerification here to avoid it.
  UniquePtr<wchar_t[]> signedBy(
      dllSvc->GetBinaryOrgName(PromiseFlatString(aPath).get(),
                               AuthenticodeFlags::SkipTrustVerification));
  if (signedBy) {
    mVendorInfo = Some(VendorInfo(VendorInfo::Source::Signature,
                                  nsDependentString(signedBy.get())));
  }

  ModuleVersionInfo verInfo;
  if (!verInfo.GetFromImage(aPath)) {
    return;
  }

  if (verInfo.mFileVersion.Version64()) {
    mVersion = Some(ModuleVersion(verInfo.mFileVersion.Version64()));
  }

  if (!mVendorInfo && !verInfo.mCompanyName.IsEmpty()) {
    mVendorInfo =
        Some(VendorInfo(VendorInfo::Source::VersionInfo, verInfo.mCompanyName));
  }
}

bool ModuleRecord::IsXUL() const {
  if (!mResolvedDosName) {
    return false;
  }

  nsAutoString leafName;
  nsresult rv = mResolvedDosName->GetLeafName(leafName);
  if (NS_FAILED(rv)) {
    return false;
  }

  return leafName.EqualsIgnoreCase("xul.dll");
}

int32_t ModuleRecord::GetScoreThreshold() const {
#ifdef ENABLE_TESTS
  // Check whether we are running as an xpcshell test.
  if (MOZ_UNLIKELY(mozilla::EnvHasValue("XPCSHELL_TEST_PROFILE_DIR"))) {
    nsAutoString dllLeaf;
    if (NS_SUCCEEDED(mResolvedDosName->GetLeafName(dllLeaf))) {
      // During xpcshell tests, this DLL is hard-coded to pass through all
      // criteria checks and still result in "untrusted" status, so it shows up
      // in the untrusted modules ping for the test to examine.
      // Setting the threshold very high ensures the test will cover all
      // criteria.
      if (dllLeaf.EqualsIgnoreCase("modules-test.dll")) {
        return 99999;
      }
    }
  }
#endif

  return 100;
}

bool ModuleRecord::IsTrusted() const {
  if (mTrustFlags == ModuleTrustFlags::None) {
    return false;
  }

  // These flags are immediate passes
  if (mTrustFlags &
      (ModuleTrustFlags::MicrosoftWindowsSignature |
       ModuleTrustFlags::MozillaSignature | ModuleTrustFlags::JitPI)) {
    return true;
  }

  // The remaining flags, when set, each count for 50 points toward a
  // trustworthiness score.
  int32_t score = static_cast<int32_t>(
                      CountPopulation32(static_cast<uint32_t>(mTrustFlags))) *
                  50;
  return score >= GetScoreThreshold();
}

ProcessedModuleLoadEvent::ProcessedModuleLoadEvent()
    : mProcessUptimeMS(0ULL),
      mThreadId(0UL),
      mBaseAddress(0U),
      mIsDependent(false),
      mLoadStatus(0) {}

ProcessedModuleLoadEvent::ProcessedModuleLoadEvent(
    glue::EnhancedModuleLoadInfo&& aModLoadInfo,
    RefPtr<ModuleRecord>&& aModuleRecord)
    : mProcessUptimeMS(QPCTimeStampToProcessUptimeMilliseconds(
          aModLoadInfo.mNtLoadInfo.mBeginTimestamp)),
      mLoadDurationMS(QPCLoadDurationToMilliseconds(aModLoadInfo.mNtLoadInfo)),
      mThreadId(aModLoadInfo.mNtLoadInfo.mThreadId),
      mThreadName(std::move(aModLoadInfo.mThreadName)),
      mBaseAddress(
          reinterpret_cast<uintptr_t>(aModLoadInfo.mNtLoadInfo.mBaseAddr)),
      mModule(std::move(aModuleRecord)),
      mIsDependent(aModLoadInfo.mNtLoadInfo.mIsDependent),
      mLoadStatus(static_cast<uint32_t>(aModLoadInfo.mNtLoadInfo.mStatus)) {
  if (!mModule || !(*mModule)) {
    return;
  }

  mRequestedDllName = aModLoadInfo.mNtLoadInfo.mRequestedDllName.AsString();

  // If we're in the main process, sanitize the requested DLL name here.
  // If not, we cannot use PreparePathForTelemetry because it may try to
  // delayload shlwapi.dll and could fail if the process is sandboxed.
  // We leave mRequestedDllName unsanitized here and sanitize it when
  // transferring it to the main process.
  // (See ParamTraits<mozilla::UntrustedModulesData>::ReadEvent)
  if (XRE_IsParentProcess()) {
    SanitizeRequestedDllName();
  }
}

void ProcessedModuleLoadEvent::SanitizeRequestedDllName() {
  if (!mRequestedDllName.IsEmpty() &&
      !widget::WinUtils::PreparePathForTelemetry(mRequestedDllName)) {
    // If we cannot sanitize a path, we simply do not provide that field to
    // Telemetry.
    mRequestedDllName.Truncate();
  }
}

/* static */
Maybe<LONGLONG>
ProcessedModuleLoadEvent::ComputeQPCTimeStampForProcessCreation() {
  // This is similar to the algorithm used by TimeStamp::ProcessCreation:

  // 1. Get the process creation timestamp as FILETIME;
  FILETIME creationTime, exitTime, kernelTime, userTime;
  if (!::GetProcessTimes(::GetCurrentProcess(), &creationTime, &exitTime,
                         &kernelTime, &userTime)) {
    return Nothing();
  }

  // 2. Get current timestamps as both QPC and FILETIME;
  LARGE_INTEGER nowQPC;
  ::QueryPerformanceCounter(&nowQPC);

  static const StaticDynamicallyLinkedFunctionPtr<void(WINAPI*)(LPFILETIME)>
      pGetSystemTimePreciseAsFileTime(L"kernel32.dll",
                                      "GetSystemTimePreciseAsFileTime");

  FILETIME nowFile;
  if (pGetSystemTimePreciseAsFileTime) {
    pGetSystemTimePreciseAsFileTime(&nowFile);
  } else {
    ::GetSystemTimeAsFileTime(&nowFile);
  }

  // 3. Take the difference between the FILETIMEs from (1) and (2),
  //    respectively, yielding the elapsed process uptime in microseconds.
  ULARGE_INTEGER ulCreation = {
      {creationTime.dwLowDateTime, creationTime.dwHighDateTime}};
  ULARGE_INTEGER ulNow = {{nowFile.dwLowDateTime, nowFile.dwHighDateTime}};

  ULONGLONG timeSinceCreationMicroSec =
      (ulNow.QuadPart - ulCreation.QuadPart) / 10ULL;

  // 4. Convert the QPC timestamp from (1) to microseconds.
  LONGLONG nowQPCMicroSec = QPCToMicroseconds<LONGLONG>(nowQPC.QuadPart);

  // 5. Convert the elapsed uptime to an absolute timestamp by subtracting
  //    from (4), which yields the absolute timestamp for process creation.
  //    We convert back to QPC units before returning.
  const LONGLONG kMicrosecondsPerSec = 1000000;
  return Some(TimeUnitsToQPC(nowQPCMicroSec - timeSinceCreationMicroSec,
                             kMicrosecondsPerSec));
}

/* static */
uint64_t ProcessedModuleLoadEvent::QPCTimeStampToProcessUptimeMilliseconds(
    const LARGE_INTEGER& aTimeStamp) {
  static const Maybe<LONGLONG> sProcessCreationTimeStamp =
      ComputeQPCTimeStampForProcessCreation();

  if (!sProcessCreationTimeStamp) {
    return 0ULL;
  }

  LONGLONG diff = aTimeStamp.QuadPart - sProcessCreationTimeStamp.value();
  return QPCToMilliseconds<uint64_t>(diff);
}

bool ProcessedModuleLoadEvent::IsXULLoad() const {
  if (!mModule) {
    return false;
  }

  return mModule->IsXUL();
}

bool ProcessedModuleLoadEvent::IsTrusted() const {
  if (!mModule) {
    return false;
  }

  return mModule->IsTrusted();
}

void UntrustedModulesData::AddNewLoads(
    const ModulesMap& aModules, UntrustedModuleLoadingEvents&& aEvents,
    Vector<Telemetry::ProcessedStack>&& aStacks) {
  MOZ_ASSERT(aEvents.length() == aStacks.length());
  for (const auto& entry : aModules) {
    if (entry.GetData()->IsTrusted()) {
      // Filter out trusted module records
      continue;
    }

    Unused << mModules.LookupOrInsert(entry.GetKey(), entry.GetData());
  }

  MOZ_ASSERT(mEvents.length() <= kMaxEvents);

  mNumEvents += aStacks.length();
  mEvents.extendBack(std::move(aEvents));
  for (auto&& stack : aStacks) {
    mStacks.AddStack(stack);
  }
}

void UntrustedModulesData::MergeModules(UntrustedModulesData& aNewData) {
  for (auto item : aNewData.mEvents) {
    mModules.WithEntryHandle(item->mEvent.mModule->mResolvedNtName,
                             [&](auto&& addPtr) {
                               if (addPtr) {
                                 // Even though the path of a ModuleRecord
                                 // matches, the object of ModuleRecord can be
                                 // different. Make sure the event's mModule
                                 // points to an object in mModules.
                                 item->mEvent.mModule = addPtr.Data();
                               } else {
                                 addPtr.Insert(item->mEvent.mModule);
                               }
                             });
  }
}

void UntrustedModulesData::Merge(UntrustedModulesData&& aNewData) {
  // Don't merge loading events of a different process
  MOZ_ASSERT((mProcessType == aNewData.mProcessType) &&
             (mPid == aNewData.mPid));

  UntrustedModulesData newData(std::move(aNewData));

  if (!mNumEvents) {
    mNumEvents = newData.mNumEvents;
    mModules = std::move(newData.mModules);
    mEvents = std::move(newData.mEvents);
    mStacks = std::move(newData.mStacks);
    return;
  }

  MergeModules(newData);
  mNumEvents += newData.mNumEvents;
  mEvents.extendBack(std::move(newData.mEvents));
  mStacks.AddStacks(newData.mStacks);
}

void UntrustedModulesData::Truncate() {
  mStacks.Clear();

  if (mNumEvents <= kMaxEvents) {
    return;
  }

  UntrustedModuleLoadingEvents events;
  events.splice(0, mEvents, mNumEvents - kMaxEvents, kMaxEvents);
  std::swap(events, mEvents);
  mNumEvents = kMaxEvents;
}

void UntrustedModulesData::MergeWithoutStacks(UntrustedModulesData&& aNewData) {
  // Don't merge loading events of a different process
  MOZ_ASSERT((mProcessType == aNewData.mProcessType) &&
             (mPid == aNewData.mPid));
  MOZ_ASSERT(!mStacks.GetStackCount());

  UntrustedModulesData newData(std::move(aNewData));

  if (mNumEvents > 0) {
    MergeModules(newData);
  } else {
    mModules = std::move(newData.mModules);
  }

  mNumEvents += newData.mNumEvents;
  mEvents.extendBack(std::move(newData.mEvents));

  Truncate();
}

void UntrustedModulesData::Swap(UntrustedModulesData& aOther) {
  GeckoProcessType tmpProcessType = mProcessType;
  mProcessType = aOther.mProcessType;
  aOther.mProcessType = tmpProcessType;

  DWORD tmpPid = mPid;
  mPid = aOther.mPid;
  aOther.mPid = tmpPid;

  TimeDuration tmpElapsed = mElapsed;
  mElapsed = aOther.mElapsed;
  aOther.mElapsed = tmpElapsed;

  mModules.SwapElements(aOther.mModules);
  std::swap(mNumEvents, aOther.mNumEvents);
  std::swap(mEvents, aOther.mEvents);
  mStacks.Swap(aOther.mStacks);

  Maybe<double> tmpXULLoadDurationMS = mXULLoadDurationMS;
  mXULLoadDurationMS = aOther.mXULLoadDurationMS;
  aOther.mXULLoadDurationMS = tmpXULLoadDurationMS;

  uint32_t tmpSanitizationFailures = mSanitizationFailures;
  mSanitizationFailures = aOther.mSanitizationFailures;
  aOther.mSanitizationFailures = tmpSanitizationFailures;

  uint32_t tmpTrustTestFailures = mTrustTestFailures;
  mTrustTestFailures = aOther.mTrustTestFailures;
  aOther.mTrustTestFailures = tmpTrustTestFailures;
}

}  // namespace mozilla
