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
#  include "mozilla/ScopeExit.h"
#  include "nsGtkUtils.h"
#  include "mozilla/StaticPrefs_layout.h"
#  include "mozilla/StaticPrefs_widget.h"
#  include "nsWindow.h"

#  include <gdk/gdkwayland.h>

#  ifdef MOZ_LOGGING
#    include "mozilla/Logging.h"
#    include "nsTArray.h"
#    include "Units.h"
extern mozilla::LazyLogModule gWidgetVsync;
#    undef LOG
#    define LOG(str, ...)                             \
      MOZ_LOG(gWidgetVsync, mozilla::LogLevel::Debug, \
              ("[nsWindow %p]: " str, GetWindowForLogging(), ##__VA_ARGS__))
#  else
#    define LOG(...)
#  endif /* MOZ_LOGGING */

using namespace mozilla::widget;

namespace mozilla {

static void WaylandVsyncSourceCallbackHandler(void* aData,
                                              struct wl_callback* aCallback,
                                              uint32_t aTime) {
  RefPtr context = static_cast<WaylandVsyncSource*>(aData);
  context->FrameCallback(aCallback, aTime);
}

static const struct wl_callback_listener WaylandVsyncSourceCallbackListener = {
    WaylandVsyncSourceCallbackHandler};

static void NativeLayerRootWaylandVsyncCallback(void* aData, uint32_t aTime) {
  RefPtr context = static_cast<WaylandVsyncSource*>(aData);
  context->FrameCallback(nullptr, aTime);
}

static float GetFPS(TimeDuration aVsyncRate) {
  return 1000.0f / float(aVsyncRate.ToMilliseconds());
}

static nsTArray<WaylandVsyncSource*> gWaylandVsyncSources;

Maybe<TimeDuration> WaylandVsyncSource::GetFastestVsyncRate() {
  Maybe<TimeDuration> retVal;
  for (auto* source : gWaylandVsyncSources) {
    auto rate = source->GetVsyncRateIfEnabled();
    if (!rate) {
      continue;
    }
    if (!retVal.isSome()) {
      retVal.emplace(*rate);
    } else if (*rate < *retVal) {
      retVal.ref() = *rate;
    }
  }

  return retVal;
}

WaylandVsyncSource::WaylandVsyncSource(nsWindow* aWindow)
    : mMutex("WaylandVsyncSource"),
      mVsyncRate(TimeDuration::FromMilliseconds(1000.0 / 60.0)),
      mLastVsyncTimeStamp(TimeStamp::Now()),
      mWindow(aWindow),
      mIdleTimeout(1000 / StaticPrefs::layout_throttled_frame_rate()) {
  MOZ_ASSERT(NS_IsMainThread());
  gWaylandVsyncSources.AppendElement(this);
}

WaylandVsyncSource::~WaylandVsyncSource() {
  gWaylandVsyncSources.RemoveElement(this);
}

void WaylandVsyncSource::MaybeUpdateSource(MozContainer* aContainer) {
  MutexAutoLock lock(mMutex);

  LOG("WaylandVsyncSource::MaybeUpdateSource fps %f", GetFPS(mVsyncRate));

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

void WaylandVsyncSource::MaybeUpdateSource(
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

void WaylandVsyncSource::Refresh(const MutexAutoLock& aProofOfLock) {
  mMutex.AssertCurrentThreadOwns();

  LOG("WaylandVsyncSource::Refresh fps %f\n", GetFPS(mVsyncRate));

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
    MozContainerSurfaceLock lock(mContainer);
    struct wl_surface* surface = lock.GetSurface();
    LOG("  refresh from mContainer, wl_surface %p", surface);
    if (!surface) {
      LOG("  we're missing wl_surface, register Refresh() callback");
      // The surface hasn't been created yet. Try again when the surface is
      // ready.
      RefPtr<WaylandVsyncSource> self(this);
      moz_container_wayland_add_initial_draw_callback_locked(
          mContainer, [self]() -> void {
            MutexAutoLock lock(self->mMutex);
            self->Refresh(lock);
          });
      return;
    }
  }

  // Vsync is enabled, but we don't have a callback configured. Set one up so
  // we can get to work.
  SetupFrameCallback(aProofOfLock);
  const TimeStamp lastVSync = TimeStamp::Now();
  mLastVsyncTimeStamp = lastVSync;
  TimeStamp outputTimestamp = mLastVsyncTimeStamp + mVsyncRate;

  {
    MutexAutoUnlock unlock(mMutex);
    NotifyVsync(lastVSync, outputTimestamp);
  }
}

void WaylandVsyncSource::EnableMonitor() {
  LOG("WaylandVsyncSource::EnableMonitor");
  MutexAutoLock lock(mMutex);
  if (mMonitorEnabled) {
    return;
  }
  mMonitorEnabled = true;
  Refresh(lock);
}

void WaylandVsyncSource::DisableMonitor() {
  LOG("WaylandVsyncSource::DisableMonitor");
  MutexAutoLock lock(mMutex);
  if (!mMonitorEnabled) {
    return;
  }
  mMonitorEnabled = false;
  mCallbackRequested = false;
}

void WaylandVsyncSource::SetupFrameCallback(const MutexAutoLock& aProofOfLock) {
  mMutex.AssertCurrentThreadOwns();
  MOZ_ASSERT(!mCallbackRequested);

  LOG("WaylandVsyncSource::SetupFrameCallback");

  if (mNativeLayerRoot) {
    LOG("  use mNativeLayerRoot");
    mNativeLayerRoot->RequestFrameCallback(&NativeLayerRootWaylandVsyncCallback,
                                           this);
  } else {
    MozContainerSurfaceLock lock(mContainer);
    struct wl_surface* surface = lock.GetSurface();
    LOG("  use mContainer, wl_surface %p", surface);
    if (!surface) {
      // We don't have a surface, either due to being called before it was made
      // available in the mozcontainer, or after it was destroyed. We're all
      // done regardless.
      LOG("  missing wl_surface, quit.");
      return;
    }

    LOG("  register frame callback");
    MozClearPointer(mCallback, wl_callback_destroy);
    mCallback = wl_surface_frame(surface);
    wl_callback_add_listener(mCallback, &WaylandVsyncSourceCallbackListener,
                             this);
    wl_surface_commit(surface);
    wl_display_flush(WaylandDisplayGet()->GetDisplay());

    if (!mIdleTimerID) {
      mIdleTimerID = g_timeout_add(
          mIdleTimeout,
          [](void* data) -> gint {
            RefPtr vsync = static_cast<WaylandVsyncSource*>(data);
            if (vsync->IdleCallback()) {
              // We want to fire again, so don't clear mIdleTimerID
              return G_SOURCE_CONTINUE;
            }
            // No need for g_source_remove, caller does it for us.
            vsync->mIdleTimerID = 0;
            return G_SOURCE_REMOVE;
          },
          this);
    }
  }

  mCallbackRequested = true;
}

bool WaylandVsyncSource::IdleCallback() {
  LOG("WaylandVsyncSource::IdleCallback");
  MOZ_DIAGNOSTIC_ASSERT(NS_IsMainThread());

  RefPtr<nsWindow> window;
  TimeStamp lastVSync;
  TimeStamp outputTimestamp;
  {
    MutexAutoLock lock(mMutex);

    if (!mVsyncEnabled || !mMonitorEnabled) {
      // We are unwanted by either our creator or our consumer, so we just stop
      // here without setting up a new frame callback.
      LOG("  quit, mVsyncEnabled %d mMonitorEnabled %d", mVsyncEnabled,
          mMonitorEnabled);
      return false;
    }

    const auto now = TimeStamp::Now();
    const auto timeSinceLastVSync = now - mLastVsyncTimeStamp;
    if (timeSinceLastVSync.ToMilliseconds() < mIdleTimeout) {
      // We're not idle, we want to fire the timer again.
      return true;
    }

    LOG("  fire idle vsync");
    CalculateVsyncRate(lock, now);
    mLastVsyncTimeStamp = lastVSync = now;

    outputTimestamp = mLastVsyncTimeStamp + mVsyncRate;
    window = mWindow;
  }

  // This could disable vsync.
  window->NotifyOcclusionState(OcclusionState::OCCLUDED);

  if (window->IsDestroyed()) {
    return false;
  }
  // Make sure to fire vsync now even if we get disabled afterwards.
  // This gives an opportunity to clean up after the visibility state change.
  // FIXME: Do we really need to do this?
  NotifyVsync(lastVSync, outputTimestamp);
  return StaticPrefs::widget_wayland_vsync_keep_firing_at_idle();
}

void WaylandVsyncSource::FrameCallback(wl_callback* aCallback, uint32_t aTime) {
  LOG("WaylandVsyncSource::FrameCallback");
  MOZ_DIAGNOSTIC_ASSERT(NS_IsMainThread());

  {
    // This might enable vsync.
    RefPtr window = mWindow;
    window->NotifyOcclusionState(OcclusionState::VISIBLE);
    // NotifyOcclusionState can destroy us.
    if (window->IsDestroyed()) {
      return;
    }
  }

  MutexAutoLock lock(mMutex);
  mCallbackRequested = false;

  // NotifyOcclusionState() can clear and create new mCallback by
  // EnableVsync()/Refresh(). So don't delete newly created frame callback.
  if (aCallback && aCallback == mCallback) {
    MozClearPointer(mCallback, wl_callback_destroy);
  }

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
  const auto callbackTimeStamp = TimeStamp::FromSystemTime(tick);
  const auto now = TimeStamp::Now();

  // If the callback timestamp is close enough to our timestamp, use it,
  // otherwise use the current time.
  const TimeStamp& vsyncTimestamp =
      std::abs((now - callbackTimeStamp).ToMilliseconds()) < 50.0
          ? callbackTimeStamp
          : now;

  CalculateVsyncRate(lock, vsyncTimestamp);
  mLastVsyncTimeStamp = vsyncTimestamp;
  const TimeStamp outputTimestamp = vsyncTimestamp + mVsyncRate;

  {
    MutexAutoUnlock unlock(mMutex);
    NotifyVsync(vsyncTimestamp, outputTimestamp);
  }
}

TimeDuration WaylandVsyncSource::GetVsyncRate() {
  MutexAutoLock lock(mMutex);
  return mVsyncRate;
}

Maybe<TimeDuration> WaylandVsyncSource::GetVsyncRateIfEnabled() {
  MutexAutoLock lock(mMutex);
  if (!mVsyncEnabled) {
    return Nothing();
  }
  return Some(mVsyncRate);
}

void WaylandVsyncSource::CalculateVsyncRate(const MutexAutoLock& aProofOfLock,
                                            TimeStamp aVsyncTimestamp) {
  mMutex.AssertCurrentThreadOwns();

  double duration = (aVsyncTimestamp - mLastVsyncTimeStamp).ToMilliseconds();
  double curVsyncRate = mVsyncRate.ToMilliseconds();

  LOG("WaylandVsyncSource::CalculateVsyncRate start fps %f\n",
      GetFPS(mVsyncRate));

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

void WaylandVsyncSource::EnableVsync() {
  MOZ_ASSERT(NS_IsMainThread());

  MutexAutoLock lock(mMutex);

  LOG("WaylandVsyncSource::EnableVsync fps %f\n", GetFPS(mVsyncRate));
  if (mVsyncEnabled || mIsShutdown) {
    LOG("  early quit");
    return;
  }
  mVsyncEnabled = true;
  Refresh(lock);
}

void WaylandVsyncSource::DisableVsync() {
  MutexAutoLock lock(mMutex);

  LOG("WaylandVsyncSource::DisableVsync fps %f\n", GetFPS(mVsyncRate));
  mVsyncEnabled = false;
  mCallbackRequested = false;
}

bool WaylandVsyncSource::IsVsyncEnabled() {
  MutexAutoLock lock(mMutex);
  return mVsyncEnabled;
}

void WaylandVsyncSource::Shutdown() {
  MOZ_ASSERT(NS_IsMainThread());
  MutexAutoLock lock(mMutex);

  LOG("WaylandVsyncSource::Shutdown fps %f\n", GetFPS(mVsyncRate));
  mContainer = nullptr;
  mNativeLayerRoot = nullptr;
  mIsShutdown = true;
  mVsyncEnabled = false;
  mCallbackRequested = false;
  MozClearHandleID(mIdleTimerID, g_source_remove);
  MozClearPointer(mCallback, wl_callback_destroy);
}

}  // namespace mozilla

#endif  // MOZ_WAYLAND
