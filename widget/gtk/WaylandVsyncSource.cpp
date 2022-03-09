/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifdef MOZ_WAYLAND

#  include "WaylandVsyncSource.h"
#  include "mozilla/UniquePtr.h"
#  include "nsThreadUtils.h"
#  include "nsISupportsImpl.h"
#  include "MainThreadUtils.h"

#  include <gdk/gdkwayland.h>

#  ifdef MOZ_LOGGING
#    include "mozilla/Logging.h"
#    include "nsTArray.h"
#    include "Units.h"
extern mozilla::LazyLogModule gWidgetVsync;
#    define LOG(...) \
      MOZ_LOG(gWidgetVsync, mozilla::LogLevel::Debug, (__VA_ARGS__))
#  else
#    define LOG(...)
#  endif /* MOZ_LOGGING */

using namespace mozilla::widget;

namespace mozilla {

static void WaylandVsyncSourceCallbackHandler(void* aData,
                                              struct wl_callback* aCallback,
                                              uint32_t aTime) {
  WaylandVsyncSource::WaylandDisplay* context =
      (WaylandVsyncSource::WaylandDisplay*)aData;
  wl_callback_destroy(aCallback);
  context->FrameCallback(aTime);
}

static void WaylandVsyncSourceCallbackHandler(void* aData, uint32_t aTime) {
  WaylandVsyncSource::WaylandDisplay* context =
      (WaylandVsyncSource::WaylandDisplay*)aData;
  context->FrameCallback(aTime);
}

static const struct wl_callback_listener WaylandVsyncSourceCallbackListener = {
    WaylandVsyncSourceCallbackHandler};

static float GetFPS(TimeDuration aVsyncRate) {
  return 1000.0 / aVsyncRate.ToMilliseconds();
}

static UniquePtr<LinkedList<WaylandVsyncSource::WaylandDisplay>>
    gWaylandDisplays;

Maybe<TimeDuration> WaylandVsyncSource::GetFastestVsyncRate() {
  Maybe<TimeDuration> retVal;
  if (gWaylandDisplays) {
    for (auto* display : *gWaylandDisplays) {
      if (display->IsVsyncEnabled()) {
        TimeDuration rate = display->GetVsyncRate();
        if (!retVal.isSome()) {
          retVal.emplace(rate);
        } else if (rate < *retVal) {
          retVal.ref() = rate;
        }
      }
    }
  }

  return retVal;
}

WaylandVsyncSource::WaylandDisplay::WaylandDisplay()
    : mMutex("WaylandVsyncSource"),
      mIsShutdown(false),
      mVsyncEnabled(false),
      mMonitorEnabled(false),
      mCallbackRequested(false),
      mContainer(nullptr),
      mVsyncRate(TimeDuration::FromMilliseconds(1000.0 / 60.0)),
      mLastVsyncTimeStamp(TimeStamp::Now()) {
  MOZ_ASSERT(NS_IsMainThread());

  if (!gWaylandDisplays) {
    gWaylandDisplays =
        MakeUnique<LinkedList<WaylandVsyncSource::WaylandDisplay>>();
  }
  gWaylandDisplays->insertBack(this);
}

WaylandVsyncSource::WaylandDisplay::~WaylandDisplay() {
  remove();  // Remove from the linked list.
  if (gWaylandDisplays->isEmpty()) {
    gWaylandDisplays = nullptr;
  }
}

void WaylandVsyncSource::WaylandDisplay::MaybeUpdateSource(
    MozContainer* aContainer) {
  MutexAutoLock lock(mMutex);

  LOG("WaylandVsyncSource::MaybeUpdateSource mContainer (nsWindow %p) fps %f",
      aContainer ? moz_container_get_nsWindow(aContainer) : nullptr,
      GetFPS(mVsyncRate));

  if (aContainer == mContainer) {
    LOG("  mContainer is the same, quit.");
    return;
  }

  mNativeLayerRoot = nullptr;
  mContainer = aContainer;

  if (mMonitorEnabled) {
    LOG("  monitor enabled, ask for Refresh()");
    mCallbackRequested = false;
    Refresh(lock);
  }
}

void WaylandVsyncSource::WaylandDisplay::MaybeUpdateSource(
    const RefPtr<NativeLayerRootWayland>& aNativeLayerRoot) {
  MutexAutoLock lock(mMutex);

  LOG("WaylandVsyncSource::MaybeUpdateSource aNativeLayerRoot fps %f",
      GetFPS(mVsyncRate));

  if (aNativeLayerRoot == mNativeLayerRoot) {
    LOG("  mNativeLayerRoot is the same, quit.");
    return;
  }

  mNativeLayerRoot = aNativeLayerRoot;
  mContainer = nullptr;

  if (mMonitorEnabled) {
    LOG("  monitor enabled, ask for Refresh()");
    mCallbackRequested = false;
    Refresh(lock);
  }
}

void WaylandVsyncSource::WaylandDisplay::Refresh(
    const MutexAutoLock& aProofOfLock) {
  LOG("WaylandDisplay::Refresh fps %f\n", GetFPS(mVsyncRate));

  if (!(mContainer || mNativeLayerRoot) || !mMonitorEnabled || !mVsyncEnabled ||
      mCallbackRequested) {
    // We don't need to do anything because:
    // * We are unwanted by our widget or monitor, or
    // * The last frame callback hasn't yet run to see that it had been shut
    //   down, so we can reuse it after having set mVsyncEnabled to true.
    LOG("  quit mContainer %d mNativeLayerRoot %d mMonitorEnabled %d "
        "mVsyncEnabled %d mCallbackRequested %d",
        !!mContainer, !!mNativeLayerRoot, mMonitorEnabled, mVsyncEnabled,
        !!mCallbackRequested);
    return;
  }

  if (mContainer) {
    struct wl_surface* surface = moz_container_wayland_surface_lock(mContainer);
    LOG("  refresh from mContainer, wl_surface %p", surface);
    if (!surface) {
      LOG("  we're missing wl_surface, register Refresh() callback");
      // The surface hasn't been created yet. Try again when the surface is
      // ready.
      RefPtr<WaylandVsyncSource::WaylandDisplay> self(this);
      moz_container_wayland_add_initial_draw_callback(
          mContainer, [self]() -> void {
            MutexAutoLock lock(self->mMutex);
            self->Refresh(lock);
          });
      return;
    }
    moz_container_wayland_surface_unlock(mContainer, &surface);
  }

  // Vsync is enabled, but we don't have a callback configured. Set one up so
  // we can get to work.
  SetupFrameCallback(aProofOfLock);
  mLastVsyncTimeStamp = TimeStamp::Now();
  TimeStamp outputTimestamp = mLastVsyncTimeStamp + GetVsyncRate();

  {
    MutexAutoUnlock unlock(mMutex);
    NotifyVsync(mLastVsyncTimeStamp, outputTimestamp);
  }
}

void WaylandVsyncSource::WaylandDisplay::EnableMonitor() {
  LOG("WaylandVsyncSource::EnableMonitor");
  MutexAutoLock lock(mMutex);
  if (mMonitorEnabled) {
    return;
  }
  mMonitorEnabled = true;
  Refresh(lock);
}

void WaylandVsyncSource::WaylandDisplay::DisableMonitor() {
  LOG("WaylandVsyncSource::DisableMonitor");
  MutexAutoLock lock(mMutex);
  if (!mMonitorEnabled) {
    return;
  }
  mMonitorEnabled = false;
  mCallbackRequested = false;
}

void WaylandVsyncSource::WaylandDisplay::SetupFrameCallback(
    const MutexAutoLock& aProofOfLock) {
  MOZ_ASSERT(!mCallbackRequested);

  LOG("WaylandVsyncSource::SetupFrameCallback");

  if (mNativeLayerRoot) {
    LOG("  use mNativeLayerRoot");
    mNativeLayerRoot->RequestFrameCallback(&WaylandVsyncSourceCallbackHandler,
                                           this);
  } else {
    struct wl_surface* surface = moz_container_wayland_surface_lock(mContainer);
    LOG("  use mContainer, wl_surface %p", surface);
    if (!surface) {
      // We don't have a surface, either due to being called before it was made
      // available in the mozcontainer, or after it was destroyed. We're all
      // done regardless.
      LOG("  missing wl_surface, quit.");
      return;
    }

    LOG("  register frame callback");
    wl_callback* callback = wl_surface_frame(surface);
    wl_callback_add_listener(callback, &WaylandVsyncSourceCallbackListener,
                             this);
    wl_surface_commit(surface);
    wl_display_flush(WaylandDisplayGet()->GetDisplay());
    moz_container_wayland_surface_unlock(mContainer, &surface);
  }

  mCallbackRequested = true;
}

void WaylandVsyncSource::WaylandDisplay::FrameCallback(uint32_t aTime) {
  LOG("WaylandVsyncSource::FrameCallback");

  MutexAutoLock lock(mMutex);
  mCallbackRequested = false;

  if (!mVsyncEnabled || !mMonitorEnabled) {
    // We are unwanted by either our creator or our consumer, so we just stop
    // here without setting up a new frame callback.
    LOG("  quit, mVsyncEnabled %d mMonitorEnabled %d", mVsyncEnabled,
        mMonitorEnabled);
    return;
  }

  // Configure our next frame callback.
  SetupFrameCallback(lock);

  int64_t tick = BaseTimeDurationPlatformUtils::TicksFromMilliseconds(aTime);
  TimeStamp callbackTimeStamp = TimeStamp::FromSystemTime(tick);
  double duration = (TimeStamp::Now() - callbackTimeStamp).ToMilliseconds();

  TimeStamp vsyncTimestamp;
  if (duration < 50 && duration > -50) {
    vsyncTimestamp = callbackTimeStamp;
  } else {
    vsyncTimestamp = TimeStamp::Now();
  }

  CalculateVsyncRate(lock, vsyncTimestamp);
  mLastVsyncTimeStamp = vsyncTimestamp;
  TimeStamp outputTimestamp = vsyncTimestamp + GetVsyncRate();

  {
    MutexAutoUnlock unlock(mMutex);
    NotifyVsync(mLastVsyncTimeStamp, outputTimestamp);
  }
}

TimeDuration WaylandVsyncSource::WaylandDisplay::GetVsyncRate() {
  return mVsyncRate;
}

void WaylandVsyncSource::WaylandDisplay::CalculateVsyncRate(
    const MutexAutoLock& aProofOfLock, TimeStamp aVsyncTimestamp) {
  double duration = (aVsyncTimestamp - mLastVsyncTimeStamp).ToMilliseconds();
  double curVsyncRate = mVsyncRate.ToMilliseconds();

  LOG("WaylandDisplay::CalculateVsyncRate start fps %f\n", GetFPS(mVsyncRate));

  double correction;
  if (duration > curVsyncRate) {
    correction = fmin(curVsyncRate, (duration - curVsyncRate) / 10);
    mVsyncRate += TimeDuration::FromMilliseconds(correction);
  } else {
    correction = fmin(curVsyncRate / 2, (curVsyncRate - duration) / 10);
    mVsyncRate -= TimeDuration::FromMilliseconds(correction);
  }

  LOG("  new fps %f correction %f\n", GetFPS(mVsyncRate), correction);
}

void WaylandVsyncSource::WaylandDisplay::EnableVsync() {
  MOZ_ASSERT(NS_IsMainThread());

  LOG("WaylandVsyncSource::EnableVsync fps %f\n", GetFPS(mVsyncRate));
  MutexAutoLock lock(mMutex);
  if (mVsyncEnabled || mIsShutdown) {
    LOG("  early quit");
    return;
  }
  mVsyncEnabled = true;
  Refresh(lock);
}

void WaylandVsyncSource::WaylandDisplay::DisableVsync() {
  LOG("WaylandVsyncSource::DisableVsync fps %f\n", GetFPS(mVsyncRate));
  MutexAutoLock lock(mMutex);
  mVsyncEnabled = false;
  mCallbackRequested = false;
}

bool WaylandVsyncSource::WaylandDisplay::IsVsyncEnabled() {
  MutexAutoLock lock(mMutex);
  return mVsyncEnabled;
}

void WaylandVsyncSource::WaylandDisplay::Shutdown() {
  MOZ_ASSERT(NS_IsMainThread());
  LOG("WaylandVsyncSource::Shutdown fps %f\n", GetFPS(mVsyncRate));
  MutexAutoLock lock(mMutex);
  mContainer = nullptr;
  mNativeLayerRoot = nullptr;
  mIsShutdown = true;
  mVsyncEnabled = false;
  mCallbackRequested = false;
}

}  // namespace mozilla

#endif  // MOZ_WAYLAND
