/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef IOActivityMonitor_h___
#define IOActivityMonitor_h___

#include "mozilla/dom/ChromeUtilsBinding.h"
#include "nsCOMPtr.h"
#include "nscore.h"
#include "nsClassHashtable.h"
#include "nsTHashMap.h"
#include "nsHashKeys.h"
#include "nsISupports.h"
#include "prinrval.h"
#include "prio.h"
#include "private/pprio.h"
#include <stdint.h>

namespace mozilla {

namespace dom {
class Promise;
}

namespace net {

#define IO_ACTIVITY_ENABLED_PREF "io.activity.enabled"

typedef nsTHashMap<nsCStringHashKey, dom::IOActivityDataDictionary> Activities;

// IOActivityMonitor has several roles:
// - maintains an IOActivity per resource and updates it
// - sends a dump of the activities to a promise via RequestActivities
class IOActivityMonitor final : public nsINamed {
 public:
  IOActivityMonitor();

  NS_DECL_THREADSAFE_ISUPPORTS
  NS_DECL_NSINAMED

  // initializes and destroys the singleton
  static nsresult Init();
  static nsresult Shutdown();

  // collect amounts of data that are written/read by location
  static nsresult Read(const nsACString& location, uint32_t aAmount);
  static nsresult Write(const nsACString& location, uint32_t aAmount);

  static nsresult MonitorFile(PRFileDesc* aFd, const char* aPath);
  static nsresult MonitorSocket(PRFileDesc* aFd);
  static nsresult Read(PRFileDesc* fd, uint32_t aAmount);
  static nsresult Write(PRFileDesc* fd, uint32_t aAmount);

  static bool IsActive();
  static void RequestActivities(dom::Promise* aPromise);

 private:
  ~IOActivityMonitor() = default;
  nsresult InitInternal();
  nsresult ShutdownInternal();
  bool IncrementActivity(const nsACString& location, uint32_t aRx,
                         uint32_t aTx);
  nsresult WriteInternal(const nsACString& location, uint32_t aAmount);
  nsresult ReadInternal(const nsACString& location, uint32_t aAmount);
  void RequestActivitiesInternal(dom::Promise* aPromise);

  Activities mActivities;
  // protects mActivities accesses
  Mutex mLock;
};

}  // namespace net
}  // namespace mozilla

#endif /* IOActivityMonitor_h___ */
