/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_Fuzzyfox_h
#define mozilla_Fuzzyfox_h

#include "nsIObserver.h"
#include "nsIObserverService.h"
#include "nsISupportsPrimitives.h"
#include "nsThreadUtils.h"
#include "mozilla/TimeStamp.h"

/*
 * This topic publishes the new canonical time according to Fuzzyfox,
 * in microseconds since the epoch. If code needs to know the current time,
 * it should listen for this topic and keep track of the 'current' time,
 * so as to respect Fuzzyfox and be in sync with the rest of the browser's
 * timekeeping.
 */
#define FUZZYFOX_UPDATECLOCK_OBSERVER_TOPIC "fuzzyfox-update-clocks"

/*
 * For Fuzzyfox's security guarentees to hold, the browser must not execute
 * actions while it should be paused. We currently only pause the main thread,
 * so actions that occur on other threads should be queued until the browser
 * unpaused (and moreso than unpauses: until it reaches a downtick.)
 * This topic indicates when any queued outbound events should be delivered.
 * TODO: Bug 1484300 and 1484299 would apply this to other communication
 * channels
 */
#define FUZZYFOX_FIREOUTBOUND_OBSERVER_TOPIC "fuzzyfox-fire-outbound"

namespace mozilla {

/*
 * Fuzzyfox is an implementation of the Fermata concept presented in
 * Trusted Browsers for Uncertain Times.
 *
 * Web Browsers expose explicit (performance.now()) and implicit
 * (WebVTT, Video Frames) timers that, when combined with algorithmic
 * improvements such as edge thresholding, produce extremely high
 * resolution clocks.
 *
 * High Resolution clocks can be used to time network accesses, browser
 * cache reads, web page rendering, access to the CPU cache, and other
 * operations - and the time these operations take to perform can yield
 * detailed information about user information we want to keep private.
 *
 * Fuzzyfox limits the information disclosure by limiting an attacker's
 * ability to create a high resolution clock. It does this by introducing
 * a concept called 'fuzzy time' that degrades all clocks (explicit and
 * implicit). This is done through a combination of holding time constant
 * during program execution and pausing program execution.
 *
 * @InProceedings{KS16,
 *   author = {David Kohlbrenner and Hovav Shacham},
 *   title = {Trusted Browsers for Uncertain Times},
 *   booktitle = {Proceedings of USENIX Security 2016},
 *   pages = {463-80},
 *   year = 2016,
 *   editor = {Thorsten Holz and Stefan Savage},
 *   month = aug,
 *   organization = {USENIX}
 * }
 * https://www.usenix.org/system/files/conference/usenixsecurity16/sec16_paper_kohlbrenner.pdf
 *
 * Fuzzyfox is an adaptation of
 *   W.-M. Hu, “Reducing timing channels with fuzzy time,” in
 *   Proceedings of IEEE Security and Privacy (“Oakland”)
 *   1991, T. F. Lunt and J. McLean, Eds. IEEE Computer
 *   Society, May 1991, pp. 8–20.
 */
class Fuzzyfox final : public Runnable, public nsIObserver {
 public:
  NS_DECL_ISUPPORTS_INHERITED
  NS_DECL_NSIOBSERVER

  static void Start();

  NS_IMETHOD
  Run() override;

 private:
  Fuzzyfox();
  ~Fuzzyfox();

  uint64_t ActualTime();

  uint64_t PickDuration();

  void UpdateClocks(uint64_t aNewTime, TimeStamp aNewTimeStamp);

  uint64_t FloorToGrain(uint64_t aValue);

  TimeStamp FloorToGrain(TimeStamp aValue);

  uint64_t CeilToGrain(uint64_t aValue);

  TimeStamp CeilToGrain(TimeStamp aValue);

  bool mSanityCheck;
  uint64_t mStartTime;
  uint32_t mDuration;

  enum Tick {
    eUptick,
    eDowntick,
  };

  Tick mTickType;

  nsCOMPtr<nsIObserverService> mObs = nullptr;
  nsCOMPtr<nsISupportsPRInt64> mTimeUpdateWrapper = nullptr;

  static Atomic<bool, Relaxed> sFuzzyfoxEnabledPrefMapped;
};

}  // namespace mozilla

#endif /* mozilla_Fuzzyfox_h */
