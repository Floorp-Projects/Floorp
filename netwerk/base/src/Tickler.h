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

#if defined(ANDROID) && !defined(MOZ_B2G)
#define MOZ_USE_WIFI_TICKLER
#endif

#include "mozilla/Attributes.h"
#include "nsISupports.h"
#include <stdint.h>

#ifdef MOZ_USE_WIFI_TICKLER
#include "mozilla/Mutex.h"
#include "mozilla/TimeStamp.h"
#include "nsAutoPtr.h"
#include "nsISupports.h"
#include "nsIThread.h"
#include "nsITimer.h"
#include "nsWeakReference.h"
#include "prio.h"

class nsIPrefBranch;
#endif

namespace mozilla {
namespace net {

#ifdef MOZ_USE_WIFI_TICKLER

// 8f769ed6-207c-4af9-9f7e-9e832da3754e
#define NS_TICKLER_IID \
{ 0x8f769ed6, 0x207c, 0x4af9, \
  { 0x9f, 0x7e, 0x9e, 0x83, 0x2d, 0xa3, 0x75, 0x4e } }

class Tickler MOZ_FINAL : public nsSupportsWeakReference
{
public:
  NS_DECL_THREADSAFE_ISUPPORTS
  NS_DECLARE_STATIC_IID_ACCESSOR(NS_TICKLER_IID)

  // These methods are main thread only
  Tickler();
  void Cancel();
  nsresult Init();
  void SetIPV4Address(uint32_t address);
  void SetIPV4Port(uint16_t port);

  // Tickle the tickler to (re-)start the activity.
  // May call from any thread
  void Tickle();

private:
  ~Tickler();

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

NS_DEFINE_STATIC_IID_ACCESSOR(Tickler, NS_TICKLER_IID)

#else // not defined MOZ_USE_WIFI_TICKLER

class Tickler MOZ_FINAL : public nsISupports
{
  ~Tickler() { }
public:
  NS_DECL_THREADSAFE_ISUPPORTS

  Tickler() { }
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
