/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set sw=2 ts=8 et tw=80 : */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef ClosingService_h__
#define ClosingService_h__

#include "nsTArray.h"
#include "nspr.h"
#include "mozilla/Telemetry.h"
#include "mozilla/Monitor.h"

//-----------------------------------------------------------------------------
// ClosingService
//-----------------------------------------------------------------------------

// A helper class carrying call to PR_Close on FD to a separate thread -
// closingThread. This may be a workaround for shutdown blocks that are caused
// by serial calls to close on UDP and TCP sockets.
// This service is started by nsIOService and also the class adds itself as an
// observer to "xpcom-shutdown-threads" notification where we join the thread
// and remove reference.
// During worktime of the thread the class is also self-referenced,
// since observer service might throw the reference away sooner than the thread
// is actually done.

namespace mozilla {
namespace net {

class ClosingService final
{
public:
  NS_INLINE_DECL_THREADSAFE_REFCOUNTING(ClosingService)

  ClosingService();

  // Attaching this layer on tcp or udp sockets PRClose will be send to the
  // closingThread.
  static nsresult AttachIOLayer(PRFileDesc *aFd);
  static void Start();
  static void Shutdown();
  void PostRequest(PRFileDesc *aFd);

private:
  ~ClosingService() {}
  nsresult StartInternal();
  void ShutdownInternal();
  void ThreadFunc();
  static void ThreadFunc(void *aClosure)
    { static_cast<ClosingService*>(aClosure)->ThreadFunc(); }

  void SendPRCloseTelemetry(PRIntervalTime aStart,
                            mozilla::Telemetry::ID aIDNormal,
                            mozilla::Telemetry::ID aIDShutdown,
                            mozilla::Telemetry::ID aIDConnectivityChange,
                            mozilla::Telemetry::ID aIDLinkChange,
                            mozilla::Telemetry::ID aIDOffline);

  static ClosingService* sInstance;
  Atomic<bool> mShutdown;
  nsTArray<PRFileDesc *> mQueue;
  mozilla::Monitor mMonitor;
  PRThread *mThread;
};

} // namespace net
} // namespace mozilla

#endif // ClosingService_h_
