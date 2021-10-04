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
#include "nsExceptionHandler.h"
#include "nsICrashReporter.h"
#include "nsISupports.h"
#include "nsMemoryPressure.h"

#define MP_LOG(...) MOZ_LOG(gMPLog, mozilla::LogLevel::Debug, (__VA_ARGS__))
static mozilla::LazyLogModule gMPLog("MemoryPressure");

namespace mozilla {

class nsAvailableMemoryWatcher final : public nsAvailableMemoryWatcherBase {
 public:
  NS_DECL_ISUPPORTS_INHERITED

  nsAvailableMemoryWatcher();
  nsresult Init() override;

  void OnMemoryPressureChanged(MacMemoryPressureLevel aLevel) override;
  void AddChildAnnotations(
      const UniquePtr<ipc::CrashReporterHost>& aCrashReporter) override;

 private:
  ~nsAvailableMemoryWatcher(){};

  void OnMemoryPressureChangedInternal(MacMemoryPressureLevel aNewLevel,
                                       bool aIsInitialLevel);

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

  void ReadSysctls();

  // Init has been called.
  bool mInitialized;

  // The memory pressure reported to the application by macOS.
  MacMemoryPressureLevel mLevel;

  // The value of the vm.memory_pressure sysctl. The OS notifies the
  // application when the memory pressure level changes, but the sysctl
  // value can be read at any time. Unofficially, the sysctl value
  // corresponds to the OS memory pressure level with 4=>critical,
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
};

NS_IMPL_ISUPPORTS(nsAvailableMemoryWatcher, nsIAvailableMemoryWatcherBase);

nsAvailableMemoryWatcher::nsAvailableMemoryWatcher()
    : mInitialized(false),
      mLevel(MacMemoryPressureLevel::Value::eUnset),
      mLevelSysctl(0xFFFFFFFF),
      mAvailMemSysctl(-1),
      mLevelStr("Unset"),
      mNormalTimeStr("Unset"),
      mWarningTimeStr("Unset"),
      mCriticalTimeStr("Unset") {}

nsresult nsAvailableMemoryWatcher::Init() {
  // Users of nsAvailableMemoryWatcher should use
  // nsAvailableMemoryWatcherBase::GetSingleton() and not call Init directly.
  MOZ_ASSERT(!mInitialized);
  if (mInitialized) {
    return NS_ERROR_ALREADY_INITIALIZED;
  }

  nsresult rv = nsAvailableMemoryWatcherBase::Init();
  if (NS_FAILED(rv)) {
    return rv;
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

  MP_LOG("Transitioning to %s at time %s", pressureLevelString.get(),
         timeChangedString.get());

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

  // If 'aNewLevel' is not one of normal, warning, or critical, ASSERT
  // here so we can debug this scenario. For non-debug builds, ignore
  // the unexpected value which will be logged in crash reports.
  MOZ_ASSERT(aNewLevel.IsNormal() || aNewLevel.IsWarningOrAbove());

  if (mLevel == aNewLevel) {
    return;
  }

  // If the first change callback (identified by aIsInitialLevel) is
  // warning or critical level, we were launched while the OS is already
  // under memory pressure. In that scenario, we'll use the current time
  // for the memory pressure start time.
  if (mLevel.IsUnsetOrNormal() && aNewLevel.IsWarningOrAbove()) {
    UpdateLowMemoryTimeStamp();
  }

  // Transition from warning or critical level to normal.
  if (mLevel.IsWarningOrAbove() && aNewLevel.IsNormal()) {
    RecordTelemetryEventOnHighMemory();
  }

  mLevel = aNewLevel;

  if (!aIsInitialLevel) {
    // Sysctls are already read by ::Init().
    ReadSysctls();
  }
  MP_LOG("level: %s, level sysctl: %d, available memory: %d percent",
         mLevel.ToString(), mLevelSysctl, mAvailMemSysctl);
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
}  // namespace mozilla
