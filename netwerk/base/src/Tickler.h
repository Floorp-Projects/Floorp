/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_net_Tickler_h
#define mozilla_net_Tickler_h

// The tickler sends a regular 0 byte UDP heartbeat out to a
// particular address for a short time after it has been touched. This
// is used on some mobile wifi chipsets to mitigate Power Save Polling
// (PSP) Mode when we are anticipating a response packet
// soon. Typically PSP adds 100ms of latency to a read event because
// the packet delivery is not triggered until the 802.11 beacon is
// delivered to the host (100ms is the standard Access Point
// configuration for the beacon interval.) Requesting a frequent
// transmission and getting a CTS frame from the AP at least that
// frequently allows for low latency receives when we have reason to
// expect them (e.g a SYN-ACK).
//
// The tickler is used to allow RTT based phases of web transport to
// complete quickly when on wifi - ARP, DNS, TCP handshake, SSL
// handshake, HTTP headers, and the TCP slow start phase. The
// transaction is given up to 400 miliseconds by default to get
// through those phases before the tickler is disabled.
//
// The tickler only applies to wifi on mobile right now. Hopefully it
// can also be restricted to particular handset models in the future.

#include "mozilla/Mutex.h"
#include "mozilla/TimeStamp.h"
#include "nsAutoPtr.h"
#include "nsISupports.h"
#include "nsIThread.h"
#include "nsThreadUtils.h"
#include "nsITimer.h"
#include "nsWeakReference.h"

class nsIPrefBranch;

namespace mozilla {
namespace net {

#if defined(ANDROID) && !defined(MOZ_B2G)
#define MOZ_USE_WIFI_TICKLER
#endif

#ifdef MOZ_USE_WIFI_TICKLER

class Tickler MOZ_FINAL : public nsSupportsWeakReference
{
public:
  NS_DECL_THREADSAFE_ISUPPORTS

  // These methods are main thread only
  Tickler();
  ~Tickler();
  void Cancel();
  nsresult Init();
  void SetIPV4Address(uint32_t address);
  void SetIPV4Port(uint16_t port);

  // Tickle the tickler to (re-)start the activity.
  // May call from any thread
  void Tickle();

private:
  friend class TicklerTimer;
  Mutex mLock;
  nsCOMPtr<nsIThread> mThread;
  nsCOMPtr<nsITimer> mTimer;
  nsCOMPtr<nsIPrefBranch> mPrefs;

  bool mActive;
  bool mCanceled;
  bool mEnabled;
  uint32_t mDelay;
  TimeDuration mDuration;
  PRFileDesc* mFD;

  TimeStamp mLastTickle;
  PRNetAddr mAddr;

  // These functions may be called from any thread
  void PostCheckTickler();
  void MaybeStartTickler();
  void MaybeStartTicklerUnlocked();

  // Tickler thread only
  void CheckTickler();
  void StartTickler();
  void StopTickler();
};

#else // not defined MOZ_USE_WIFI_TICKLER

class Tickler MOZ_FINAL : public nsISupports
{
public:
  NS_DECL_THREADSAFE_ISUPPORTS

  Tickler() { }
  ~Tickler() { }
  nsresult Init() { return NS_ERROR_NOT_IMPLEMENTED; }
  void Cancel() { }
  void SetIPV4Address(uint32_t) { };
  void SetIPV4Port(uint16_t) { }
  void Tickle() { }
};

#endif // defined MOZ_USE_WIFI_TICKLER

} // namespace mozilla::net
} // namespace mozilla

#endif // mozilla_net_Tickler_h
