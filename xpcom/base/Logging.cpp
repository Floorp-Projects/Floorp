/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/Logging.h"

#include <algorithm>

#include "mozilla/ClearOnShutdown.h"
#include "mozilla/Mutex.h"
#include "mozilla/StaticPtr.h"
#include "nsClassHashtable.h"
#include "NSPRLogModulesParser.h"

#include "prenv.h"

// NB: Initial amount determined by auditing the codebase for the total amount
//     of unique module names and padding up to the next power of 2.
const uint32_t kInitialModuleCount = 256;

namespace mozilla {

LogLevel
ToLogLevel(int32_t aLevel)
{
  aLevel = std::min(aLevel, static_cast<int32_t>(LogLevel::Verbose));
  aLevel = std::max(aLevel, static_cast<int32_t>(LogLevel::Disabled));
  return static_cast<LogLevel>(aLevel);
}

class LogModuleManager
{
public:
  LogModuleManager()
    : mModulesLock("logmodules")
    , mModules(kInitialModuleCount)
  {
  }

  ~LogModuleManager()
  {
    // NB: mModules owns all of the log modules, they will get destroyed by
    //     its destructor.
  }

  /**
   * Loads config from env vars if present.
   */
  void Init()
  {
    const char* modules = PR_GetEnv("NSPR_LOG_MODULES");
    NSPRLogModulesParser(modules, [] (const char* aName, LogLevel aLevel) {
        LogModule::Get(aName)->SetLevel(aLevel);
    });
  }

  LogModule* CreateOrGetModule(const char* aName)
  {
    OffTheBooksMutexAutoLock guard(mModulesLock);
    LogModule* module = nullptr;
    if (!mModules.Get(aName, &module)) {
      module = new LogModule(LogLevel::Disabled);
      mModules.Put(aName, module);
    }

    return module;
  }

private:
  OffTheBooksMutex mModulesLock;
  nsClassHashtable<nsCharPtrHashKey, LogModule> mModules;
};

StaticAutoPtr<LogModuleManager> sLogModuleManager;

LogModule*
LogModule::Get(const char* aName)
{
  // This is just a pass through to the LogModuleManager so
  // that the LogModuleManager implementation can be kept internal.
  MOZ_ASSERT(sLogModuleManager != nullptr);
  return sLogModuleManager->CreateOrGetModule(aName);
}

void
LogModule::Init()
{
  // NB: This method is not threadsafe; it is expected to be called very early
  //     in startup prior to any other threads being run.
  if (sLogModuleManager) {
    // Already initialized.
    return;
  }

  // NB: We intentionally do not register for ClearOnShutdown as that happens
  //     before all logging is complete. And, yes, that means we leak, but
  //     we're doing that intentionally.
  sLogModuleManager = new LogModuleManager();
  sLogModuleManager->Init();
}

} // namespace mozilla
