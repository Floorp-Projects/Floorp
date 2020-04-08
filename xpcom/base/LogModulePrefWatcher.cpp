/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "LogModulePrefWatcher.h"

#include "mozilla/Logging.h"
#include "mozilla/Preferences.h"
#include "mozilla/Services.h"
#include "nsIObserverService.h"
#include "nsMemory.h"
#include "nsString.h"
#include "nsXULAppAPI.h"
#include "base/process_util.h"

static const char kLoggingPrefPrefix[] = "logging.";
static const char kLoggingConfigPrefPrefix[] = "logging.config";
static const int kLoggingConfigPrefixLen = sizeof(kLoggingConfigPrefPrefix) - 1;
static const char kLoggingPrefClearOnStartup[] =
    "logging.config.clear_on_startup";
static const char kLoggingPrefLogFile[] = "logging.config.LOG_FILE";
static const char kLoggingPrefAddTimestamp[] = "logging.config.add_timestamp";
static const char kLoggingPrefSync[] = "logging.config.sync";

namespace mozilla {

NS_IMPL_ISUPPORTS(LogModulePrefWatcher, nsIObserver)

/**
 * Resets all the preferences in the logging. branch
 * This is needed because we may crash while logging, and this would cause us
 * to log after restarting as well.
 *
 * If logging after restart is desired, set the logging.config.clear_on_startup
 * pref to false, or use the MOZ_LOG_FILE and MOZ_LOG_MODULES env vars.
 */
static void ResetExistingPrefs() {
  nsTArray<nsCString> names;
  nsresult rv =
      Preferences::GetRootBranch()->GetChildList(kLoggingPrefPrefix, names);
  if (NS_SUCCEEDED(rv)) {
    for (auto& name : names) {
      // Clearing the pref will cause it to reload, thus resetting the log level
      Preferences::ClearUser(name.get());
    }
  }
}

/**
 * Loads the log level from the given pref and updates the corresponding
 * LogModule.
 */
static void LoadPrefValue(const char* aName) {
  LogLevel logLevel = LogLevel::Disabled;

  nsresult rv;
  int32_t prefLevel = 0;
  nsAutoCString prefValue;

  if (strncmp(aName, kLoggingConfigPrefPrefix, kLoggingConfigPrefixLen) == 0) {
    nsAutoCString prefName(aName);

    if (prefName.EqualsLiteral(kLoggingPrefLogFile)) {
      rv = Preferences::GetCString(aName, prefValue);
      // The pref was reset. Clear the user file.
      if (NS_FAILED(rv) || prefValue.IsEmpty()) {
        LogModule::SetLogFile(nullptr);
        return;
      }

      // If the pref value doesn't have a PID placeholder, append it to the end.
      if (!strstr(prefValue.get(), MOZ_LOG_PID_TOKEN)) {
        prefValue.AppendLiteral(MOZ_LOG_PID_TOKEN);
      }

      LogModule::SetLogFile(prefValue.BeginReading());
    } else if (prefName.EqualsLiteral(kLoggingPrefAddTimestamp)) {
      bool addTimestamp = Preferences::GetBool(aName, false);
      LogModule::SetAddTimestamp(addTimestamp);
    } else if (prefName.EqualsLiteral(kLoggingPrefSync)) {
      bool sync = Preferences::GetBool(aName, false);
      LogModule::SetIsSync(sync);
    }
    return;
  }

  if (Preferences::GetInt(aName, &prefLevel) == NS_OK) {
    logLevel = ToLogLevel(prefLevel);
  } else if (Preferences::GetCString(aName, prefValue) == NS_OK) {
    if (prefValue.LowerCaseEqualsLiteral("error")) {
      logLevel = LogLevel::Error;
    } else if (prefValue.LowerCaseEqualsLiteral("warning")) {
      logLevel = LogLevel::Warning;
    } else if (prefValue.LowerCaseEqualsLiteral("info")) {
      logLevel = LogLevel::Info;
    } else if (prefValue.LowerCaseEqualsLiteral("debug")) {
      logLevel = LogLevel::Debug;
    } else if (prefValue.LowerCaseEqualsLiteral("verbose")) {
      logLevel = LogLevel::Verbose;
    }
  }

  const char* moduleName = aName + strlen(kLoggingPrefPrefix);
  LogModule::Get(moduleName)->SetLevel(logLevel);
}

static void LoadExistingPrefs() {
  nsIPrefBranch* root = Preferences::GetRootBranch();
  if (!root) {
    return;
  }

  nsTArray<nsCString> names;
  nsresult rv = root->GetChildList(kLoggingPrefPrefix, names);
  if (NS_SUCCEEDED(rv)) {
    for (auto& name : names) {
      LoadPrefValue(name.get());
    }
  }
}

LogModulePrefWatcher::LogModulePrefWatcher() = default;

void LogModulePrefWatcher::RegisterPrefWatcher() {
  RefPtr<LogModulePrefWatcher> prefWatcher = new LogModulePrefWatcher();
  Preferences::AddStrongObserver(prefWatcher, kLoggingPrefPrefix);

  nsCOMPtr<nsIObserverService> observerService =
      mozilla::services::GetObserverService();
  if (observerService && XRE_IsParentProcess()) {
    observerService->AddObserver(prefWatcher,
                                 "browser-delayed-startup-finished", false);
  }

  LoadExistingPrefs();
}

NS_IMETHODIMP
LogModulePrefWatcher::Observe(nsISupports* aSubject, const char* aTopic,
                              const char16_t* aData) {
  if (strcmp(NS_PREFBRANCH_PREFCHANGE_TOPIC_ID, aTopic) == 0) {
    NS_LossyConvertUTF16toASCII prefName(aData);
    LoadPrefValue(prefName.get());
  } else if (strcmp("browser-delayed-startup-finished", aTopic) == 0) {
    bool clear = Preferences::GetBool(kLoggingPrefClearOnStartup, true);
    if (clear) {
      ResetExistingPrefs();
    }
    nsCOMPtr<nsIObserverService> observerService =
        mozilla::services::GetObserverService();
    if (observerService) {
      observerService->RemoveObserver(this, "browser-delayed-startup-finished");
    }
  }

  return NS_OK;
}

}  // namespace mozilla
