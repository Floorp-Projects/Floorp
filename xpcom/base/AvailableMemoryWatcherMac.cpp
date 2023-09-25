/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include <sys/sysctl.h>
#include <sys/types.h>
#include <time.h>

#include "AvailableMemoryWatcher.h"
#include "Logging.h"
#include "mozilla/Preferences.h"
#include "nsICrashReporter.h"
#include "nsISupports.h"
#include "nsITimer.h"
#include "nsMemoryPressure.h"
#include "nsPrintfCString.h"

#define MP_LOG(...) MOZ_LOG(gMPLog, mozilla::LogLevel::Debug, (__VA_ARGS__))
static mozilla::LazyLogModule gMPLog("MemoryPressure");

namespace mozilla {

/*
 * The Mac AvailableMemoryWatcher works as follows. When the OS memory pressure
 * level changes on macOS, nsAvailableMemoryWatcher::OnMemoryPressureChanged()
 * is called with the new memory pressure level. The level is represented in
 * Gecko by a MacMemoryPressureLevel instance and represents the states of
 * normal, warning, or critical which correspond to the native levels. When the
 * browser launches, the initial level is determined using a sysctl. Which
 * actions are taken in the browser in response to memory pressure, and the
 * level (warning or critical) which trigger the reponse is configurable with
 * prefs to make it easier to perform experiments to study how the response
 * affects the user experience.
 *
 * By default, the browser responds by attempting to reduce memory use when the
 * OS transitions to the critical level and while it stays in the critical
 * level. i.e., "critical" OS memory pressure is the default threshold for the
 * low memory response. Setting pref "browser.lowMemoryResponseOnWarn" to true
 * changes the memory response to occur at the "warning" level which is less
 * severe than "critical". When entering the critical level, we begin polling
 * the memory pressure level every 'n' milliseconds (specified via the pref
 * "browser.lowMemoryPollingIntervalMS"). Each time the poller wakes up and
 * finds the OS still under memory pressure, the low memory response is
 * executed.
 *
 * By default, the memory pressure response is, in order, to
 *   1) call nsITabUnloader::UnloadTabAsync(),
 *   2) if no tabs could be unloaded, issue a Gecko
 *      MemoryPressureState::LowMemory notification.
 * The response can be changed via the pref "browser.lowMemoryResponseMask" to
 * limit the actions to only tab unloading or Gecko memory pressure
 * notifications.
 *
 * Polling occurs on the main thread because, at each polling interval, we
 * call into the tab unloader which requires being on the main thread.
 * Polling only occurs while under OS memory pressure at the critical (by
 * default) level.
 */
class nsAvailableMemoryWatcher final : public nsITimerCallback,
                                       public nsINamed,
                                       public nsAvailableMemoryWatcherBase {
 public:
  NS_DECL_ISUPPORTS_INHERITED
  NS_DECL_NSIOBSERVER
  NS_DECL_NSITIMERCALLBACK
  NS_DECL_NSINAMED

  nsAvailableMemoryWatcher();
  nsresult Init() override;

  void OnMemoryPressureChanged(MacMemoryPressureLevel aLevel) override;
  void AddChildAnnotations(
      const UniquePtr<ipc::CrashReporterHost>& aCrashReporter) override;

 private:
  ~nsAvailableMemoryWatcher(){};

  void OnMemoryPressureChangedInternal(MacMemoryPressureLevel aNewLevel,
                                       bool aIsInitialLevel);

  // Override OnUnloadAttemptCompleted() so that we can control whether
  // or not a Gecko memory-pressure event is sent after a tab unload attempt.
  // This method is called externally by the tab unloader after a tab unload
  // attempt. It is used internally when tab unloading is disabled in
  // mResponseMask.
  nsresult OnUnloadAttemptCompleted(nsresult aResult) override;

  void OnShutdown();
  void OnPrefChange();

  void InitParentAnnotations();
  void UpdateParentAnnotations();

  void AddParentAnnotation(CrashReporter::Annotation aAnnotation,
                           nsAutoCString aString) {
    CrashReporter::AnnotateCrashReport(aAnnotation, aString);
  }
  void AddParentAnnotation(CrashReporter::Annotation aAnnotation,
                           uint32_t aData) {
    CrashReporter::AnnotateCrashReport(aAnnotation, aData);
  }

  void LowMemoryResponse();
  void StartPolling();
  void StopPolling();
  void RestartPolling();
  inline bool IsPolling() { return mTimer; }

  void ReadSysctls();

  // This enum represents the allowed values for the pref that controls
  // the low memory response - "browser.lowMemoryResponseMask". Specifically,
  // whether or not we unload tabs and/or issue the Gecko "memory-pressure"
  // internal notification. For tab unloading, the pref
  // "browser.tabs.unloadOnLowMemory" must also be set.
  enum ResponseMask {
    eNone = 0x0,
    eTabUnload = 0x1,
    eInternalMemoryPressure = 0x2,
    eAll = 0x3,
  };
  static constexpr char kResponseMask[] = "browser.lowMemoryResponseMask";
  static const uint32_t kResponseMaskDefault;
  static const uint32_t kResponseMaskMax;

  // Pref for controlling how often we wake up during an OS memory pressure
  // time period. At each wakeup, we unload tabs and issue the Gecko
  // "memory-pressure" internal notification. When not under OS memory pressure,
  // polling is disabled.
  static constexpr char kPollingIntervalMS[] =
      "browser.lowMemoryPollingIntervalMS";
  static const uint32_t kPollingIntervalMaxMS;
  static const uint32_t kPollingIntervalMinMS;
  static const uint32_t kPollingIntervalDefaultMS;

  static constexpr char kResponseOnWarn[] = "browser.lowMemoryResponseOnWarn";
  static const bool kResponseLevelOnWarnDefault = false;

  // Init has been called.
  bool mInitialized;

  // The memory pressure reported to the application by macOS.
  MacMemoryPressureLevel mLevel;

  // The OS memory pressure level that triggers the response.
  MacMemoryPressureLevel mResponseLevel;

  // The value of the kern.memorystatus_vm_pressure_level sysctl. The OS
  // notifies the application when the memory pressure level changes,
  // but the sysctl value can be read at any time. Unofficially, the sysctl
  // value corresponds to the OS memory pressure level with 4=>critical,
  // 2=>warning, and 1=>normal (values from kernel event.h file).
  uint32_t mLevelSysctl;
  static const int kSysctlLevelNormal = 0x1;
  static const int kSysctlLevelWarning = 0x2;
  static const int kSysctlLevelCritical = 0x4;

  // The value of the kern.memorystatus_level sysctl. Unofficially,
  // this is the percentage of available memory. (Also readable
  // via the undocumented memorystatus_get_level syscall.)
  int mAvailMemSysctl;

  // The string representation of `mLevel`. i.e., normal, warning, or critical.
  // Set to "unset" until a memory pressure change is reported to the process
  // by the OS.
  nsAutoCString mLevelStr;

  // Timestamps for memory pressure level changes. Specifically, the Unix
  // time in string form. Saved as Unix time to allow comparisons with
  // the crash time.
  nsAutoCString mNormalTimeStr;
  nsAutoCString mWarningTimeStr;
  nsAutoCString mCriticalTimeStr;

  nsCOMPtr<nsITimer> mTimer;  // non-null indicates the timer is active

  // Saved pref values.
  uint32_t mPollingInterval;
  uint32_t mResponseMask;
};

const uint32_t nsAvailableMemoryWatcher::kResponseMaskDefault =
    ResponseMask::eAll;
const uint32_t nsAvailableMemoryWatcher::kResponseMaskMax = ResponseMask::eAll;

// 10 seconds
const uint32_t nsAvailableMemoryWatcher::kPollingIntervalDefaultMS = 10'000;
// 10 minutes
const uint32_t nsAvailableMemoryWatcher::kPollingIntervalMaxMS = 600'000;
// 100 milliseconds
const uint32_t nsAvailableMemoryWatcher::kPollingIntervalMinMS = 100;

NS_IMPL_ISUPPORTS_INHERITED(nsAvailableMemoryWatcher,
                            nsAvailableMemoryWatcherBase, nsIObserver,
                            nsITimerCallback, nsINamed)

nsAvailableMemoryWatcher::nsAvailableMemoryWatcher()
    : mInitialized(false),
      mLevel(MacMemoryPressureLevel::Value::eUnset),
      mResponseLevel(MacMemoryPressureLevel::Value::eCritical),
      mLevelSysctl(0xFFFFFFFF),
      mAvailMemSysctl(-1),
      mLevelStr("Unset"),
      mNormalTimeStr("Unset"),
      mWarningTimeStr("Unset"),
      mCriticalTimeStr("Unset"),
      mPollingInterval(0),
      mResponseMask(ResponseMask::eAll) {}

nsresult nsAvailableMemoryWatcher::Init() {
  nsresult rv = nsAvailableMemoryWatcherBase::Init();
  if (NS_FAILED(rv)) {
    return rv;
  }

  // Users of nsAvailableMemoryWatcher should use
  // nsAvailableMemoryWatcherBase::GetSingleton() and not call Init directly.
  MOZ_ASSERT(!mInitialized);
  if (mInitialized) {
    return NS_ERROR_ALREADY_INITIALIZED;
  }

  // Read polling frequency pref
  mPollingInterval =
      Preferences::GetUint(kPollingIntervalMS, kPollingIntervalDefaultMS);
  mPollingInterval = std::clamp(mPollingInterval, kPollingIntervalMinMS,
                                kPollingIntervalMaxMS);

  // Read response bitmask pref which (along with the main tab unloading
  // preference) controls whether or not tab unloading and Gecko (internal)
  // memory pressure notifications will be sent. The main tab unloading
  // preference must also be enabled for tab unloading to occur.
  mResponseMask = Preferences::GetUint(kResponseMask, kResponseMaskDefault);
  if (mResponseMask > kResponseMaskMax) {
    mResponseMask = kResponseMaskMax;
  }

  // Read response level pref
  if (Preferences::GetBool(kResponseOnWarn, kResponseLevelOnWarnDefault)) {
    mResponseLevel = MacMemoryPressureLevel::Value::eWarning;
  } else {
    mResponseLevel = MacMemoryPressureLevel::Value::eCritical;
  }

  ReadSysctls();
  MP_LOG("Initial memory pressure sysctl: %d", mLevelSysctl);
  MP_LOG("Initial available memory sysctl: %d", mAvailMemSysctl);

  // Set the initial state of all annotations for parent crash reports.
  // Content process crash reports are set when a crash occurs and
  // AddChildAnnotations() is called.
  CrashReporter::AnnotateCrashReport(
      CrashReporter::Annotation::MacMemoryPressure, mLevelStr);
  CrashReporter::AnnotateCrashReport(
      CrashReporter::Annotation::MacMemoryPressureNormalTime, mNormalTimeStr);
  CrashReporter::AnnotateCrashReport(
      CrashReporter::Annotation::MacMemoryPressureWarningTime, mWarningTimeStr);
  CrashReporter::AnnotateCrashReport(
      CrashReporter::Annotation::MacMemoryPressureCriticalTime,
      mCriticalTimeStr);
  CrashReporter::AnnotateCrashReport(
      CrashReporter::Annotation::MacMemoryPressureSysctl, mLevelSysctl);
  CrashReporter::AnnotateCrashReport(
      CrashReporter::Annotation::MacAvailableMemorySysctl, mAvailMemSysctl);

  // To support running experiments, handle pref
  // changes without requiring a browser restart.
  rv = Preferences::AddStrongObserver(this, kResponseMask);
  if (NS_FAILED(rv)) {
    NS_WARNING(
        nsPrintfCString("Failed to add %s observer", kResponseMask).get());
  }
  rv = Preferences::AddStrongObserver(this, kPollingIntervalMS);
  if (NS_FAILED(rv)) {
    NS_WARNING(
        nsPrintfCString("Failed to add %s observer", kPollingIntervalMS).get());
  }
  rv = Preferences::AddStrongObserver(this, kResponseOnWarn);
  if (NS_FAILED(rv)) {
    NS_WARNING(
        nsPrintfCString("Failed to add %s observer", kResponseOnWarn).get());
  }

  // Use the memory pressure sysctl to initialize our memory pressure state.
  MacMemoryPressureLevel initialLevel;
  switch (mLevelSysctl) {
    case kSysctlLevelNormal:
      initialLevel = MacMemoryPressureLevel::Value::eNormal;
      break;
    case kSysctlLevelWarning:
      initialLevel = MacMemoryPressureLevel::Value::eWarning;
      break;
    case kSysctlLevelCritical:
      initialLevel = MacMemoryPressureLevel::Value::eCritical;
      break;
    default:
      initialLevel = MacMemoryPressureLevel::Value::eUnexpected;
  }

  OnMemoryPressureChangedInternal(initialLevel, /* aIsInitialLevel */ true);
  mInitialized = true;
  return NS_OK;
}

already_AddRefed<nsAvailableMemoryWatcherBase> CreateAvailableMemoryWatcher() {
  // Users of nsAvailableMemoryWatcher should use
  // nsAvailableMemoryWatcherBase::GetSingleton().
  RefPtr watcher(new nsAvailableMemoryWatcher());
  watcher->Init();
  return watcher.forget();
}

// Update the memory pressure level, level change timestamps, and sysctl
// level crash report annotations.
void nsAvailableMemoryWatcher::UpdateParentAnnotations() {
  // Generate a string representation of the current Unix time.
  time_t timeChanged = time(NULL);
  nsAutoCString timeChangedString;
  timeChangedString =
      nsPrintfCString("%" PRIu64, static_cast<uint64_t>(timeChanged));

  nsAutoCString pressureLevelString;
  Maybe<CrashReporter::Annotation> pressureLevelKey;

  switch (mLevel.GetValue()) {
    case MacMemoryPressureLevel::Value::eNormal:
      mNormalTimeStr = timeChangedString;
      pressureLevelString = "Normal";
      pressureLevelKey.emplace(
          CrashReporter::Annotation::MacMemoryPressureNormalTime);
      break;
    case MacMemoryPressureLevel::Value::eWarning:
      mWarningTimeStr = timeChangedString;
      pressureLevelString = "Warning";
      pressureLevelKey.emplace(
          CrashReporter::Annotation::MacMemoryPressureWarningTime);
      break;
    case MacMemoryPressureLevel::Value::eCritical:
      mCriticalTimeStr = timeChangedString;
      pressureLevelString = "Critical";
      pressureLevelKey.emplace(
          CrashReporter::Annotation::MacMemoryPressureCriticalTime);
      break;
    default:
      pressureLevelString = "Unexpected";
      break;
  }

  // Save the current memory pressure level.
  AddParentAnnotation(CrashReporter::Annotation::MacMemoryPressure,
                      pressureLevelString);

  // Save the time we transitioned to the current memory pressure level.
  if (pressureLevelKey.isSome()) {
    AddParentAnnotation(pressureLevelKey.value(), timeChangedString);
  }

  AddParentAnnotation(CrashReporter::Annotation::MacMemoryPressureSysctl,
                      mLevelSysctl);
  AddParentAnnotation(CrashReporter::Annotation::MacAvailableMemorySysctl,
                      mAvailMemSysctl);
}

void nsAvailableMemoryWatcher::ReadSysctls() {
  // Pressure level
  uint32_t level;
  size_t size = sizeof(level);
  if (sysctlbyname("kern.memorystatus_vm_pressure_level", &level, &size, NULL,
                   0) == -1) {
    MP_LOG("Failure reading memory pressure sysctl");
  }
  mLevelSysctl = level;

  // Available memory percent
  int availPercent;
  size = sizeof(availPercent);
  if (sysctlbyname("kern.memorystatus_level", &availPercent, &size, NULL, 0) ==
      -1) {
    MP_LOG("Failure reading available memory level");
  }
  mAvailMemSysctl = availPercent;
}

/* virtual */
void nsAvailableMemoryWatcher::OnMemoryPressureChanged(
    MacMemoryPressureLevel aNewLevel) {
  MOZ_ASSERT(mInitialized);
  OnMemoryPressureChangedInternal(aNewLevel, /* aIsInitialLevel */ false);
}

void nsAvailableMemoryWatcher::OnMemoryPressureChangedInternal(
    MacMemoryPressureLevel aNewLevel, bool aIsInitialLevel) {
  MOZ_ASSERT(mInitialized || aIsInitialLevel);
  MP_LOG("MemoryPressureChange: existing level: %s, new level: %s",
         mLevel.ToString(), aNewLevel.ToString());

  // If 'aNewLevel' is not one of normal, warning, or critical, ASSERT
  // here so we can debug this scenario. For non-debug builds, ignore
  // the unexpected value which will be logged in crash reports.
  MOZ_ASSERT(aNewLevel.IsNormal() || aNewLevel.IsWarningOrAbove());

  if (mLevel == aNewLevel) {
    return;
  }

  // Start the memory pressure response if the new level is high enough
  // and the existing level was not.
  if ((mLevel < mResponseLevel) && (aNewLevel >= mResponseLevel)) {
    UpdateLowMemoryTimeStamp();
    LowMemoryResponse();
    if (mResponseMask) {
      StartPolling();
    }
  }

  // End the memory pressure reponse if the new level is not high enough.
  if ((mLevel >= mResponseLevel) && (aNewLevel < mResponseLevel)) {
    {
      MutexAutoLock lock(mMutex);
      RecordTelemetryEventOnHighMemory(lock);
    }
    StopPolling();
    MP_LOG("Issuing MemoryPressureState::NoPressure");
    NS_NotifyOfMemoryPressure(MemoryPressureState::NoPressure);
  }

  mLevel = aNewLevel;

  if (!aIsInitialLevel) {
    // Sysctls are already read by ::Init().
    ReadSysctls();
    MP_LOG("level sysctl: %d, available memory: %d percent", mLevelSysctl,
           mAvailMemSysctl);
  }
  UpdateParentAnnotations();
}

/* virtual */
// Add all annotations to the provided crash reporter instance.
void nsAvailableMemoryWatcher::AddChildAnnotations(
    const UniquePtr<ipc::CrashReporterHost>& aCrashReporter) {
  aCrashReporter->AddAnnotation(CrashReporter::Annotation::MacMemoryPressure,
                                mLevelStr);
  aCrashReporter->AddAnnotation(
      CrashReporter::Annotation::MacMemoryPressureNormalTime, mNormalTimeStr);
  aCrashReporter->AddAnnotation(
      CrashReporter::Annotation::MacMemoryPressureWarningTime, mWarningTimeStr);
  aCrashReporter->AddAnnotation(
      CrashReporter::Annotation::MacMemoryPressureCriticalTime,
      mCriticalTimeStr);
  aCrashReporter->AddAnnotation(
      CrashReporter::Annotation::MacMemoryPressureSysctl, mLevelSysctl);
  aCrashReporter->AddAnnotation(
      CrashReporter::Annotation::MacAvailableMemorySysctl, mAvailMemSysctl);
}

void nsAvailableMemoryWatcher::LowMemoryResponse() {
  if (mResponseMask & ResponseMask::eTabUnload) {
    MP_LOG("Attempting tab unload");
    mTabUnloader->UnloadTabAsync();
  } else {
    // Re-use OnUnloadAttemptCompleted() to issue the internal
    // memory pressure event.
    OnUnloadAttemptCompleted(NS_ERROR_NOT_AVAILABLE);
  }
}

NS_IMETHODIMP
nsAvailableMemoryWatcher::Notify(nsITimer* aTimer) {
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(mLevel >= mResponseLevel);
  LowMemoryResponse();
  return NS_OK;
}

// Override OnUnloadAttemptCompleted() so that we can issue Gecko memory
// pressure notifications only if eInternalMemoryPressure is set in
// mResponseMask. When called from the tab unloader, an |aResult| value of
// NS_OK indicates the tab unloader successfully unloaded a tab.
// NS_ERROR_NOT_AVAILABLE indicates the tab unloader did not unload any tabs.
NS_IMETHODIMP
nsAvailableMemoryWatcher::OnUnloadAttemptCompleted(nsresult aResult) {
  // On MacOS we don't access these members offthread; however we do on other
  // OSes and so they are guarded by the mutex.
  MutexAutoLock lock(mMutex);
  switch (aResult) {
    // A tab was unloaded successfully.
    case NS_OK:
      MP_LOG("Tab unloaded");
      ++mNumOfTabUnloading;
      break;

    // Either the tab unloader found no unloadable tabs OR we've been called
    // locally to explicitly issue the internal memory pressure event because
    // tab unloading is disabled in |mResponseMask|. In either case, attempt
    // to reduce memory use using the internal memory pressure notification.
    case NS_ERROR_NOT_AVAILABLE:
      if (mResponseMask & ResponseMask::eInternalMemoryPressure) {
        ++mNumOfMemoryPressure;
        MP_LOG("Tab not unloaded");
        MP_LOG("Issuing MemoryPressureState::LowMemory");
        NS_NotifyOfEventualMemoryPressure(MemoryPressureState::LowMemory);
      }
      break;

    // There was a pending task to unload a tab.
    case NS_ERROR_ABORT:
      break;

    default:
      MOZ_ASSERT_UNREACHABLE("Unexpected aResult");
      break;
  }
  return NS_OK;
}

NS_IMETHODIMP
nsAvailableMemoryWatcher::Observe(nsISupports* aSubject, const char* aTopic,
                                  const char16_t* aData) {
  nsresult rv = nsAvailableMemoryWatcherBase::Observe(aSubject, aTopic, aData);
  if (NS_FAILED(rv)) {
    return rv;
  }

  if (strcmp(aTopic, "xpcom-shutdown") == 0) {
    OnShutdown();
  } else if (strcmp(aTopic, NS_PREFBRANCH_PREFCHANGE_TOPIC_ID) == 0) {
    OnPrefChange();
  }
  return NS_OK;
}

void nsAvailableMemoryWatcher::OnShutdown() {
  StopPolling();
  Preferences::RemoveObserver(this, kResponseMask);
  Preferences::RemoveObserver(this, kPollingIntervalMS);
}

void nsAvailableMemoryWatcher::OnPrefChange() {
  MP_LOG("OnPrefChange()");
  // Handle the polling interval changing.
  uint32_t pollingInterval = Preferences::GetUint(kPollingIntervalMS);
  if (pollingInterval != mPollingInterval) {
    mPollingInterval = std::clamp(pollingInterval, kPollingIntervalMinMS,
                                  kPollingIntervalMaxMS);
    RestartPolling();
  }

  // Handle the response mask changing.
  uint32_t responseMask = Preferences::GetUint(kResponseMask);
  if (mResponseMask != responseMask) {
    mResponseMask = std::min(responseMask, kResponseMaskMax);

    // Do we need to turn on polling?
    if (mResponseMask && (mLevel >= mResponseLevel) && !IsPolling()) {
      StartPolling();
    }

    // Do we need to turn off polling?
    if (!mResponseMask && IsPolling()) {
      StopPolling();
    }
  }

  // Handle the response level changing.
  MacMemoryPressureLevel newResponseLevel;
  if (Preferences::GetBool(kResponseOnWarn, kResponseLevelOnWarnDefault)) {
    newResponseLevel = MacMemoryPressureLevel::Value::eWarning;
  } else {
    newResponseLevel = MacMemoryPressureLevel::Value::eCritical;
  }
  if (newResponseLevel == mResponseLevel) {
    return;
  }

  // Do we need to turn on polling?
  if (mResponseMask && (newResponseLevel <= mLevel)) {
    UpdateLowMemoryTimeStamp();
    LowMemoryResponse();
    StartPolling();
  }

  // Do we need to turn off polling?
  if (IsPolling() && (newResponseLevel > mLevel)) {
    {
      MutexAutoLock lock(mMutex);
      RecordTelemetryEventOnHighMemory(lock);
    }
    StopPolling();
    MP_LOG("Issuing MemoryPressureState::NoPressure");
    NS_NotifyOfMemoryPressure(MemoryPressureState::NoPressure);
  }
  mResponseLevel = newResponseLevel;
}

void nsAvailableMemoryWatcher::StartPolling() {
  MOZ_ASSERT(NS_IsMainThread());
  if (!mTimer) {
    MP_LOG("Starting poller");
    mTimer = NS_NewTimer();
    if (mTimer) {
      mTimer->InitWithCallback(this, mPollingInterval,
                               nsITimer::TYPE_REPEATING_SLACK);
    }
  }
}

void nsAvailableMemoryWatcher::StopPolling() {
  MOZ_ASSERT(NS_IsMainThread());
  if (mTimer) {
    MP_LOG("Pausing poller");
    mTimer->Cancel();
    mTimer = nullptr;
  }
}

void nsAvailableMemoryWatcher::RestartPolling() {
  if (IsPolling()) {
    StopPolling();
    StartPolling();
  } else {
    MOZ_ASSERT(!mTimer);
  }
}

NS_IMETHODIMP
nsAvailableMemoryWatcher::GetName(nsACString& aName) {
  aName.AssignLiteral("nsAvailableMemoryWatcher");
  return NS_OK;
}

}  // namespace mozilla
