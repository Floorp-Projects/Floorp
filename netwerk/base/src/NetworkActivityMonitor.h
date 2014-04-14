/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef NetworkActivityMonitor_h___
#define NetworkActivityMonitor_h___

#include "nsCOMPtr.h"
#include "prio.h"
#include "prinrval.h"

class nsIObserverService;

namespace mozilla { namespace net {

class NetworkActivityMonitor
{
public:
  enum Direction {
    kUpload   = 0,
    kDownload = 1
  };

  NetworkActivityMonitor();
  ~NetworkActivityMonitor();

  static nsresult Init(int32_t blipInterval);
  static nsresult Shutdown();

  static nsresult AttachIOLayer(PRFileDesc *fd);
  static nsresult DataInOut(Direction direction);

private:
  nsresult Init_Internal(int32_t blipInterval);
  void PostNotification(Direction direction);

  static NetworkActivityMonitor * gInstance;
  PRDescIdentity                  mLayerIdentity;
  PRIOMethods                     mLayerMethods;
  PRIntervalTime                  mBlipInterval;
  PRIntervalTime                  mLastNotificationTime[2];
  nsCOMPtr<nsIObserverService>    mObserverService;
};

}} // namespace mozilla::net

#endif /* NetworkActivityMonitor_h___ */
