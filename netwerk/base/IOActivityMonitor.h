/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef IOActivityMonitor_h___
#define IOActivityMonitor_h___

#include "nsCOMPtr.h"
#include "nscore.h"
#include "nsClassHashtable.h"
#include "nsDataHashtable.h"
#include "nsHashKeys.h"
#include "nsIIOActivityData.h"
#include "nsISupports.h"
#include "nsITimer.h"
#include "prinrval.h"
#include "prio.h"
#include "private/pprio.h"
#include <stdint.h>

namespace mozilla { namespace net {

#define IO_ACTIVITY_ENABLED_PREF "io.activity.enabled"
#define IO_ACTIVITY_INTERVAL_PREF "io.activity.intervalMilliseconds"

//
// IOActivity keeps track of the amount of data
// sent and received for an FD / Location
//
struct IOActivity {
  // the resource location, can be:
  // - socket://ip:port
  // - file://absolute/path
  nsCString location;

  // bytes received/read (rx) and sent/written (tx)
  uint32_t rx;
  uint32_t tx;

  explicit IOActivity(const nsACString& aLocation) {
    location.Assign(aLocation);
    rx = 0;
    tx = 0;
  }

  // Returns true if no data was transferred
  bool Inactive() {
    return rx == 0 && tx == 0;
  }

  // Sets the data to zero
  void Reset() {
    rx = 0;
    tx = 0;
  }
};

typedef nsClassHashtable<nsCStringHashKey, IOActivity> Activities;

// XPCOM Wrapper for an IOActivity
class IOActivityData final : public nsIIOActivityData
{
public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSIIOACTIVITYDATA
  explicit IOActivityData(IOActivity aActivity)
      : mActivity(aActivity) {}
private:
  ~IOActivityData() = default;
  IOActivity mActivity;
};

// IOActivityMonitor has several roles:
// - maintains an IOActivity per resource and updates it
// - sends a dump of the activities to observers that wants
//   to get that info, via a timer
class IOActivityMonitor final
    : public nsITimerCallback
    , public nsINamed
{
public:
  IOActivityMonitor();

  NS_DECL_THREADSAFE_ISUPPORTS
  NS_DECL_NSITIMERCALLBACK
  NS_DECL_NSINAMED

  // initializes and destroys the singleton
  static nsresult Init(int32_t aInterval);
  static nsresult Shutdown();

  // collect amounts of data that are written/read by location
  static nsresult Read(const nsACString& location, uint32_t aAmount);
  static nsresult Write(const nsACString& location, uint32_t aAmount);

  static nsresult MonitorFile(PRFileDesc *aFd, const char* aPath);
  static nsresult MonitorSocket(PRFileDesc *aFd);
  static nsresult Read(PRFileDesc *fd, uint32_t aAmount);
  static nsresult Write(PRFileDesc *fd, uint32_t aAmount);

  static bool IsActive();

  // collects activities and notifies observers
  // this method can be called manually or via the timer callback
  static nsresult NotifyActivities();
private:
  virtual ~IOActivityMonitor() = default;
  nsresult Init_Internal(int32_t aInterval);
  nsresult Shutdown_Internal();

  IOActivity* GetActivity(const nsACString& location);
  nsresult Write_Internal(const nsACString& location, uint32_t aAmount);
  nsresult Read_Internal(const nsACString& location, uint32_t aAmount);
  nsresult NotifyActivities_Internal();

  Activities mActivities;

  // timer used to send notifications
  uint32_t mInterval;
  nsCOMPtr<nsITimer> mTimer;
  // protects mActivities accesses
  Mutex mLock;
};

} // namespace net
} // namespace mozilla

#endif /* IOActivityMonitor_h___ */
