/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef _WaylandVsyncSource_h_
#define _WaylandVsyncSource_h_

#include "base/thread.h"
#include "mozilla/RefPtr.h"
#include "mozilla/LinkedList.h"
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
  WaylandVsyncSource() { mGlobalDisplay = new WaylandDisplay(); }

  virtual ~WaylandVsyncSource() { MOZ_ASSERT(NS_IsMainThread()); }

  virtual Display& GetGlobalDisplay() override { return *mGlobalDisplay; }

  static Maybe<TimeDuration> GetFastestVsyncRate();

  class WaylandDisplay final : public mozilla::gfx::VsyncSource::Display,
                               public LinkedListElement<WaylandDisplay> {
   public:
    WaylandDisplay();

    void MaybeUpdateSource(MozContainer* aContainer);
    void MaybeUpdateSource(
        const RefPtr<NativeLayerRootWayland>& aNativeLayerRoot);

    void EnableMonitor();
    void DisableMonitor();

    void FrameCallback(uint32_t aTime);

    TimeDuration GetVsyncRate() override;

    virtual void EnableVsync() override;

    virtual void DisableVsync() override;

    virtual bool IsVsyncEnabled() override;

    virtual void Shutdown() override;

   private:
    ~WaylandDisplay() override;
    void Refresh(const MutexAutoLock& aProofOfLock);
    void SetupFrameCallback(const MutexAutoLock& aProofOfLock);
    void CalculateVsyncRate(const MutexAutoLock& aProofOfLock,
                            TimeStamp aVsyncTimestamp);

    Mutex mMutex;
    bool mIsShutdown;
    bool mVsyncEnabled;
    bool mMonitorEnabled;
    bool mCallbackRequested;
    MozContainer* mContainer;
    RefPtr<NativeLayerRootWayland> mNativeLayerRoot;
    TimeDuration mVsyncRate;
    TimeStamp mLastVsyncTimeStamp;
  };

 private:
  // We need a refcounted VsyncSource::Display to use chromium IPC runnables.
  RefPtr<WaylandDisplay> mGlobalDisplay;
};

}  // namespace mozilla

#endif  // _WaylandVsyncSource_h_
