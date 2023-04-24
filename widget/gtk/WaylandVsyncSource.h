/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef _WaylandVsyncSource_h_
#define _WaylandVsyncSource_h_

#include "base/thread.h"
#include "mozilla/RefPtr.h"
#include "mozilla/Maybe.h"
#include "mozilla/Mutex.h"
#include "mozilla/Monitor.h"
#include "mozilla/layers/NativeLayerWayland.h"
#include "MozContainer.h"
#include "nsWaylandDisplay.h"
#include "VsyncSource.h"

namespace mozilla {

using layers::NativeLayerRootWayland;

/*
 * WaylandVsyncSource
 *
 * This class provides a per-widget VsyncSource under Wayland, emulated using
 * frame callbacks on the widget surface with empty surface commits.
 *
 * Wayland does not expose vsync/vblank, as it considers that an implementation
 * detail the clients should not concern themselves with. Instead, frame
 * callbacks are provided whenever the compositor believes it is a good time to
 * start drawing the next frame for a particular surface, giving us as much
 * time as possible to do so.
 *
 * Note that the compositor sends frame callbacks only when it sees fit, and
 * when that may be is entirely up to the compositor. One cannot expect a
 * certain rate of callbacks, or any callbacks at all. Examples of common
 * variations would be surfaces moved between outputs with different refresh
 * rates, and surfaces that are hidden and therefore do not receieve any
 * callbacks at all. Other hypothetical scenarios of variation could be
 * throttling to conserve power, or because a user has requested it.
 *
 */
class WaylandVsyncSource final : public gfx::VsyncSource {
 public:
  explicit WaylandVsyncSource(nsWindow* aWindow);
  virtual ~WaylandVsyncSource();

  static Maybe<TimeDuration> GetFastestVsyncRate();

  void MaybeUpdateSource(MozContainer* aContainer);
  void MaybeUpdateSource(
      const RefPtr<NativeLayerRootWayland>& aNativeLayerRoot);

  void EnableMonitor();
  void DisableMonitor();

  void FrameCallback(wl_callback* aCallback, uint32_t aTime);
  // Returns whether we should keep firing.
  bool IdleCallback();

  TimeDuration GetVsyncRate() override;

  void EnableVsync() override;

  void DisableVsync() override;

  bool IsVsyncEnabled() override;

  void Shutdown() override;

 private:
  Maybe<TimeDuration> GetVsyncRateIfEnabled();

  void Refresh(const MutexAutoLock& aProofOfLock);
  void SetupFrameCallback(const MutexAutoLock& aProofOfLock);
  void CalculateVsyncRate(const MutexAutoLock& aProofOfLock,
                          TimeStamp aVsyncTimestamp);
  void* GetWindowForLogging() { return mWindow; };

  Mutex mMutex;
  bool mIsShutdown MOZ_GUARDED_BY(mMutex) = false;
  bool mVsyncEnabled MOZ_GUARDED_BY(mMutex) = false;
  bool mMonitorEnabled MOZ_GUARDED_BY(mMutex) = false;
  bool mCallbackRequested MOZ_GUARDED_BY(mMutex) = false;
  MozContainer* mContainer MOZ_GUARDED_BY(mMutex) = nullptr;
  RefPtr<NativeLayerRootWayland> mNativeLayerRoot MOZ_GUARDED_BY(mMutex);
  TimeDuration mVsyncRate MOZ_GUARDED_BY(mMutex);
  TimeStamp mLastVsyncTimeStamp MOZ_GUARDED_BY(mMutex);
  wl_callback* mCallback MOZ_GUARDED_BY(mMutex) = nullptr;

  guint mIdleTimerID = 0;   // Main thread only.
  nsWindow* const mWindow;  // Main thread only, except for logging.
  const guint mIdleTimeout;
};

}  // namespace mozilla

#endif  // _WaylandVsyncSource_h_
