/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_AvailableMemoryWatcher_h
#define mozilla_AvailableMemoryWatcher_h

#include "mozilla/ipc/CrashReporterHost.h"
#include "mozilla/UniquePtr.h"
#include "MemoryPressureLevelMac.h"
#include "nsCOMPtr.h"
#include "nsIAvailableMemoryWatcherBase.h"
#include "nsIObserver.h"
#include "nsIObserverService.h"

namespace mozilla {

// This class implements a platform-independent part to watch the system's
// memory situation and invoke the registered callbacks when we detect
// a low-memory situation or a high-memory situation.
// The actual logic to monitor the memory status is implemented in a subclass
// of nsAvailableMemoryWatcherBase per platform.
class nsAvailableMemoryWatcherBase : public nsIAvailableMemoryWatcherBase,
                                     public nsIObserver {
  static StaticRefPtr<nsAvailableMemoryWatcherBase> sSingleton;
  static const char* const kObserverTopics[];

  TimeStamp mLowMemoryStart;

 protected:
  uint32_t mNumOfTabUnloading;
  uint32_t mNumOfMemoryPressure;

  nsCOMPtr<nsITabUnloader> mTabUnloader;
  nsCOMPtr<nsIObserverService> mObserverSvc;
  // Do not change this value off the main thread.
  bool mInteracting;

  virtual ~nsAvailableMemoryWatcherBase() = default;
  virtual nsresult Init();
  void Shutdown();
  void UpdateLowMemoryTimeStamp();
  void RecordTelemetryEventOnHighMemory();

 public:
  static already_AddRefed<nsAvailableMemoryWatcherBase> GetSingleton();

  nsAvailableMemoryWatcherBase();

#if defined(XP_DARWIN)
  virtual void OnMemoryPressureChanged(MacMemoryPressureLevel aNewLevel){};
  virtual void AddChildAnnotations(
      const UniquePtr<ipc::CrashReporterHost>& aCrashReporter){};
#endif

  NS_DECL_THREADSAFE_ISUPPORTS
  NS_DECL_NSIAVAILABLEMEMORYWATCHERBASE
  NS_DECL_NSIOBSERVER
};

// Method to create a platform-specific object
already_AddRefed<nsAvailableMemoryWatcherBase> CreateAvailableMemoryWatcher();

}  // namespace mozilla

#endif  // ifndef mozilla_AvailableMemoryWatcher_h
