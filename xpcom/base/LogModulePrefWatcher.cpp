/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "LogModulePrefWatcher.h"

#include "mozilla/Logging.h"
#include "mozilla/Preferences.h"
#include "nsString.h"

static const char kLoggingPrefPrefix[] = "logging.";

namespace mozilla {

NS_IMPL_ISUPPORTS(LogModulePrefWatcher, nsIObserver)

/**
 * Loads the log level from the given pref and updates the corresponding
 * LogModule.
 */
void
LoadPrefValue(const char* aName)
{
  LogLevel logLevel = LogLevel::Disabled;

  int32_t prefLevel = 0;
  nsAutoCString prefStr;

  if (Preferences::GetInt(aName, &prefLevel) == NS_OK) {
    logLevel = ToLogLevel(prefLevel);
  } else if (Preferences::GetCString(aName, &prefStr) == NS_OK) {
    if (prefStr.LowerCaseEqualsLiteral("error")) {
      logLevel = LogLevel::Error;
    } else if (prefStr.LowerCaseEqualsLiteral("warning")) {
      logLevel = LogLevel::Warning;
    } else if (prefStr.LowerCaseEqualsLiteral("info")) {
      logLevel = LogLevel::Info;
    } else if (prefStr.LowerCaseEqualsLiteral("debug")) {
      logLevel = LogLevel::Debug;
    } else if (prefStr.LowerCaseEqualsLiteral("verbose")) {
      logLevel = LogLevel::Verbose;
    }
  }

  const char* moduleName = aName + strlen(kLoggingPrefPrefix);
  LogModule::Get(moduleName)->SetLevel(logLevel);
}

void
LoadExistingPrefs()
{
  nsIPrefBranch* root = Preferences::GetRootBranch();
  if (!root) {
    return;
  }

  uint32_t count;
  char** names;
  nsresult rv = root->GetChildList(kLoggingPrefPrefix, &count, &names);
  if (NS_SUCCEEDED(rv) && count) {
    for (size_t i = 0; i < count; i++) {
      LoadPrefValue(names[i]);
    }
    NS_FREE_XPCOM_ALLOCATED_POINTER_ARRAY(count, names);
  }
}

LogModulePrefWatcher::LogModulePrefWatcher()
{
}

void
LogModulePrefWatcher::RegisterPrefWatcher()
{
  RefPtr<LogModulePrefWatcher> prefWatcher = new LogModulePrefWatcher();
  Preferences::AddStrongObserver(prefWatcher, kLoggingPrefPrefix);
  LoadExistingPrefs();
}

NS_IMETHODIMP
LogModulePrefWatcher::Observe(nsISupports* aSubject, const char* aTopic,
                              const char16_t* aData)
{
  if (strcmp(NS_PREFBRANCH_PREFCHANGE_TOPIC_ID, aTopic) == 0) {
    NS_LossyConvertUTF16toASCII prefName(aData);
    LoadPrefValue(prefName.get());
  }

  return NS_OK;
}

} // namespace mozilla
