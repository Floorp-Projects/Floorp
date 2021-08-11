/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "WindowSurfaceWayland.h"

#include <errno.h>
#include <fcntl.h>
#include <sys/mman.h>

#include "nsPrintfCString.h"
#include "mozilla/gfx/2D.h"
#include "mozilla/gfx/Tools.h"
#include "gfx2DGlue.h"
#include "gfxPlatform.h"
#include "MozContainer.h"
#include "mozilla/ScopeExit.h"
#include "mozilla/StaticPrefs_widget.h"
#include "mozilla/WidgetUtils.h"
#include "nsTArray.h"

#ifdef MOZ_LOGGING
#  include "mozilla/Logging.h"
#  include "Units.h"
extern mozilla::LazyLogModule gWidgetWaylandLog;
#  define LOGWAYLAND(args) \
    MOZ_LOG(gWidgetWaylandLog, mozilla::LogLevel::Debug, args)
#else
#  define LOGWAYLAND(args)
#endif /* MOZ_LOGGING */

// Maximal compositing timeout it miliseconds
#define COMPOSITING_TIMEOUT 200

namespace mozilla::widget {

/*
  Wayland multi-thread rendering scheme

  Every rendering thread (main thread, compositor thread) contains its own
  nsWaylandDisplay object connected to Wayland compositor (Mutter, Weston, etc.)

  WindowSurfaceWayland implements WindowSurface class and draws nsWindow by
  WindowSurface interface (Lock, Commit) to screen through nsWaylandDisplay.

  ----------------------
  | Wayland compositor |
  ----------------------
             ^
             |
  ----------------------
  |  nsWaylandDisplay  |
  ----------------------
        ^          ^
        |          |
        |          |
        |       ---------------------------------        ------------------
        |       | WindowSurfaceWayland          |<------>| nsWindow       |
        |       |                               |        ------------------
        |       |  -----------------------      |
        |       |  | WaylandBufferSHM    |      |
        |       |  |                     |      |
        |       |  | ------------------- |      |
        |       |  | |  WaylandShmPool | |      |
        |       |  | ------------------- |      |
        |       |  -----------------------      |
        |       |                               |
        |       |  -----------------------      |
        |       |  | WaylandBufferSHM    |      |
        |       |  |                     |      |
        |       |  | ------------------- |      |
        |       |  | |  WaylandShmPool | |      |
        |       |  | ------------------- |      |
        |       |  -----------------------      |
        |       ---------------------------------
        |
        |
  ---------------------------------        ------------------
  | WindowSurfaceWayland          |<------>| nsWindow       |
  |                               |        ------------------
  |  -----------------------      |
  |  | WaylandBufferSHM    |      |
  |  |                     |      |
  |  | ------------------- |      |
  |  | |  WaylandShmPool | |      |
  |  | ------------------- |      |
  |  -----------------------      |
  |                               |
  |  -----------------------      |
  |  | WaylandBufferSHM    |      |
  |  |                     |      |
  |  | ------------------- |      |
  |  | |  WaylandShmPool | |      |
  |  | ------------------- |      |
  |  -----------------------      |
  ---------------------------------


nsWaylandDisplay

Is our connection to Wayland display server,
holds our display connection (wl_display) and event queue (wl_event_queue).

nsWaylandDisplay is created for every thread which sends data to Wayland
compositor. Wayland events for main thread is served by default Gtk+ loop,
for other threads (compositor) we must create wl_event_queue and run event loop.


WindowSurfaceWayland

Is a Wayland implementation of WindowSurface class for WindowSurfaceProvider,
we implement Lock() and Commit() interfaces from WindowSurface
for actual drawing.

One WindowSurfaceWayland draws one nsWindow so those are tied 1:1.
At Wayland level it holds one wl_surface object.

To perform visualiation of nsWindow, WindowSurfaceWayland contains one
wl_surface and two wl_buffer objects (owned by WaylandBufferSHM)
as we use double buffering. When nsWindow drawing is finished to wl_buffer,
the wl_buffer is attached to wl_surface and it's sent to Wayland compositor.

When there's no wl_buffer available for drawing (all wl_buffers are locked in
compositor for instance) we store the drawing to WindowImageSurface object
and draw later when wl_buffer becomes available or discard the
WindowImageSurface cache when whole screen is invalidated.

WaylandBufferSHM

Is a class which provides a wl_buffer for drawing.
Wl_buffer is a main Wayland object with actual graphics data.
Wl_buffer basically represent one complete window screen.
When double buffering is involved every window (GdkWindow for instance)
utilises two wl_buffers which are cycled. One is filed with data by application
and one is rendered by compositor.

WaylandBufferSHM is implemented by shared memory (shm).
It owns wl_buffer object, owns WaylandShmPool
(which provides the shared memory) and ties them together.

WaylandShmPool

WaylandShmPool acts as a manager of shared memory for WaylandBufferSHM.
Allocates it, holds reference to it and releases it.

We allocate shared memory (shm) by mmap(..., MAP_SHARED,...) as an interface
between us and wayland compositor. We draw our graphics data to the shm and
handle to wayland compositor by WaylandBufferSHM/WindowSurfaceWayland
(wl_buffer/wl_surface).
*/

#define EVENT_LOOP_DELAY (1000 / 240)

static const struct wl_callback_listener sFrameListenerWindowSurfaceWayland = {
    WindowSurfaceWayland::FrameCallbackHandler};

WindowSurfaceWayland::WindowSurfaceWayland(RefPtr<nsWindow> aWindow)
    : mWindow(std::move(aWindow)),
      mWaylandDisplay(WaylandDisplayGet()),
      mWaylandFullscreenDamage(false),
      mFrameCallback(nullptr),
      mLastCommittedSurfaceID(-1),
      mLastCommitTime(0),
      mDrawToWaylandBufferDirectly(true),
      mCanSwitchWaylandBuffer(true),
      mWLBufferIsDirty(false),
      mBufferCommitAllowed(false),
      mBufferNeedsClear(false),
      mSmoothRendering(StaticPrefs::widget_wayland_smooth_rendering()),
      mSurfaceReadyTimerID(),
      mSurfaceLock("WindowSurfaceWayland lock") {
  LOGWAYLAND(("WindowSurfaceWayland::WindowSurfaceWayland() [%p]\n", this));
  // Use slow compositing on KDE only.
  const char* currentDesktop = getenv("XDG_CURRENT_DESKTOP");
  if (currentDesktop && strstr(currentDesktop, "KDE") != nullptr) {
    mSmoothRendering = CACHE_NONE;
  }
}

WindowSurfaceWayland::~WindowSurfaceWayland() {
  LOGWAYLAND(("WindowSurfaceWayland::~WindowSurfaceWayland() [%p]\n", this));

  MutexAutoLock lock(mSurfaceLock);

  if (mSurfaceReadyTimerID) {
    g_source_remove(mSurfaceReadyTimerID);
    mSurfaceReadyTimerID = 0;
  }

  if (mWLBufferIsDirty) {
    NS_WARNING("Deleted WindowSurfaceWayland with a pending commit!");
  }

  if (mFrameCallback) {
    wl_callback_destroy(mFrameCallback);
  }
}

WaylandBufferSHM* WindowSurfaceWayland::CreateWaylandBuffer(
    const LayoutDeviceIntSize& aSize) {
  int availableBuffer;

  LOGWAYLAND(("WindowSurfaceWayland::CreateWaylandBuffer %d x %d\n",
              aSize.width, aSize.height));

  for (availableBuffer = 0; availableBuffer < BACK_BUFFER_NUM;
       availableBuffer++) {
    if (!mShmBackupBuffer[availableBuffer] ||
        (!mShmBackupBuffer[availableBuffer]->IsAttached() &&
         !mShmBackupBuffer[availableBuffer]->IsMatchingSize(aSize))) {
      break;
    }
  }

  // There isn't any free slot for additional buffer.
  if (availableBuffer == BACK_BUFFER_NUM) {
    LOGWAYLAND(("    no free buffer slot!\n"));
    return nullptr;
  }

  RefPtr<WaylandBufferSHM> buffer = WaylandBufferSHM::Create(aSize);
  if (!buffer) {
    LOGWAYLAND(("    failed to create back buffer!\n"));
    return nullptr;
  }

  buffer->SetBufferReleaseFunc(
      &WindowSurfaceWayland::BufferReleaseCallbackHandler);
  buffer->SetBufferReleaseData(this);

  mShmBackupBuffer[availableBuffer] = buffer;
  LOGWAYLAND(
      ("    created new buffer %p at %d!\n", buffer.get(), availableBuffer));
  return buffer.get();
}

WaylandBufferSHM* WindowSurfaceWayland::WaylandBufferFindAvailable(
    const LayoutDeviceIntSize& aSize) {
  LOGWAYLAND(("WindowSurfaceWayland::WaylandBufferFindAvailable %d x %d\n",
              aSize.width, aSize.height));

  // Try to find a buffer which matches the size
  for (int availableBuffer = 0; availableBuffer < BACK_BUFFER_NUM;
       availableBuffer++) {
    RefPtr<WaylandBufferSHM> buffer = mShmBackupBuffer[availableBuffer];
    if (buffer && !buffer->IsAttached() && buffer->IsMatchingSize(aSize)) {
      LOGWAYLAND(("    found match %d [%p]\n", availableBuffer, buffer.get()));
      return buffer.get();
    }
  }

  LOGWAYLAND(("    no buffer available!\n"));
  return nullptr;
}

WaylandBufferSHM* WindowSurfaceWayland::SetNewWaylandBuffer() {
  LOGWAYLAND(
      ("WindowSurfaceWayland::NewWaylandBuffer [%p] Requested buffer [%d "
       "x %d]\n",
       (void*)this, mWLBufferSize.width, mWLBufferSize.height));

  mWaylandBuffer = WaylandBufferFindAvailable(mWLBufferSize);
  if (mWaylandBuffer) {
    return mWaylandBuffer;
  }

  mWaylandBuffer = CreateWaylandBuffer(mWLBufferSize);
  return mWaylandBuffer;
}

// Recent
WaylandBufferSHM* WindowSurfaceWayland::GetWaylandBuffer() {
  LOGWAYLAND(
      ("WindowSurfaceWayland::GetWaylandBuffer [%p] Requested buffer [%d "
       "x %d] can switch %d\n",
       (void*)this, mWLBufferSize.width, mWLBufferSize.height,
       mCanSwitchWaylandBuffer));

#if MOZ_LOGGING
  LOGWAYLAND(("    Recent WaylandBufferSHM [%p]\n", mWaylandBuffer.get()));
  for (int i = 0; i < BACK_BUFFER_NUM; i++) {
    if (!mShmBackupBuffer[i]) {
      LOGWAYLAND(("        WaylandBufferSHM [%d] null\n", i));
    } else {
      LOGWAYLAND(
          ("        WaylandBufferSHM [%d][%p] width %d height %d attached %d\n",
           i, mShmBackupBuffer[i].get(), mShmBackupBuffer[i]->GetSize().width,
           mShmBackupBuffer[i]->GetSize().height,
           mShmBackupBuffer[i]->IsAttached()));
    }
  }
#endif

  // There's no buffer created yet, create a new one for partial screen updates.
  if (!mWaylandBuffer) {
    return SetNewWaylandBuffer();
  }

  if (mWaylandBuffer->IsAttached()) {
    if (mCanSwitchWaylandBuffer) {
      return SetNewWaylandBuffer();
    }
    LOGWAYLAND(("    Buffer is attached and we can't switch, return null\n"));
    return nullptr;
  }

  if (mWaylandBuffer->IsMatchingSize(mWLBufferSize)) {
    LOGWAYLAND(("    Size is ok, use the buffer [%d x %d]\n",
                mWLBufferSize.width, mWLBufferSize.height));
    return mWaylandBuffer;
  }

  if (mCanSwitchWaylandBuffer) {
    return SetNewWaylandBuffer();
  }

  LOGWAYLAND(
      ("    Buffer size does not match, requested %d x %d got %d x%d, return "
       "null.\n",
       mWaylandBuffer->GetSize().width, mWaylandBuffer->GetSize().height,
       mWLBufferSize.width, mWLBufferSize.height));
  return nullptr;
}

already_AddRefed<gfx::DrawTarget> WindowSurfaceWayland::LockWaylandBuffer() {
  // Allocated wayland buffer must match mozcontainer widget size.
  mWLBufferSize = mWindow->GetMozContainerSize();

  LOGWAYLAND(
      ("WindowSurfaceWayland::LockWaylandBuffer [%p] Requesting buffer %d x "
       "%d\n",
       (void*)this, mWLBufferSize.width, mWLBufferSize.height));

  WaylandBufferSHM* buffer = GetWaylandBuffer();
  LOGWAYLAND(("WindowSurfaceWayland::LockWaylandBuffer [%p] Got buffer %p\n",
              (void*)this, (void*)buffer));

  if (!buffer) {
    if (mLastCommitTime && (g_get_monotonic_time() / 1000) - mLastCommitTime >
                               COMPOSITING_TIMEOUT) {
      NS_WARNING(
          "Slow response from Wayland compositor, visual glitches ahead.");
    }
    return nullptr;
  }

  mCanSwitchWaylandBuffer = false;

  if (mBufferNeedsClear) {
    buffer->Clear();
    mBufferNeedsClear = false;
  }

  return buffer->Lock();
}

already_AddRefed<gfx::DrawTarget> WindowSurfaceWayland::LockImageSurface(
    const gfx::IntSize& aLockSize) {
  if (!mImageSurface || !(aLockSize <= mImageSurface->GetSize())) {
    mImageSurface = gfx::Factory::CreateDataSourceSurface(
        aLockSize, WaylandBufferSHM::GetSurfaceFormat());
  }
  gfx::DataSourceSurface::MappedSurface map = {nullptr, 0};
  if (!mImageSurface->Map(gfx::DataSourceSurface::READ_WRITE, &map)) {
    return nullptr;
  }
  return gfxPlatform::CreateDrawTargetForData(
      map.mData, mImageSurface->GetSize(), map.mStride,
      WaylandBufferSHM::GetSurfaceFormat());
}

static bool IsWindowFullScreenUpdate(
    LayoutDeviceIntSize& aScreenSize,
    const LayoutDeviceIntRegion& aUpdatedRegion) {
  if (aUpdatedRegion.GetNumRects() > 1) return false;

  gfx::IntRect rect = aUpdatedRegion.RectIter().Get().ToUnknownRect();
  return (rect.x == 0 && rect.y == 0 && aScreenSize.width == rect.width &&
          aScreenSize.height == rect.height);
}

static bool IsPopupFullScreenUpdate(
    LayoutDeviceIntSize& aScreenSize,
    const LayoutDeviceIntRegion& aUpdatedRegion) {
  // We know that popups can be drawn from two parts; a panel and an arrow.
  // Assume we redraw whole popups when we have two rects and bounding
  // box is equal to window borders.
  if (aUpdatedRegion.GetNumRects() > 2) return false;

  gfx::IntRect lockSize = aUpdatedRegion.GetBounds().ToUnknownRect();
  return (lockSize.x == 0 && lockSize.y == 0 &&
          aScreenSize.width == lockSize.width &&
          aScreenSize.height == lockSize.height);
}

already_AddRefed<gfx::DrawTarget> WindowSurfaceWayland::Lock(
    const LayoutDeviceIntRegion& aRegion) {
  if (mWindow->WindowType() == eWindowType_invisible) {
    return nullptr;
  }

  // Lock the surface *after* WaitForSyncEnd() call as is can fire
  // FlushPendingCommits().
  MutexAutoLock lock(mSurfaceLock);

  // Disable all commits (from potential frame callback/delayed handlers)
  // until next WindowSurfaceWayland::Commit() call.
  mBufferCommitAllowed = false;

  LayoutDeviceIntSize mozContainerSize = mWindow->GetMozContainerSize();
  gfx::IntRect lockSize = aRegion.GetBounds().ToUnknownRect();

  bool isTransparentPopup =
      mWindow->IsWaylandPopup() &&
      (eTransparencyTransparent == mWindow->GetTransparencyMode());

  bool windowRedraw = isTransparentPopup
                          ? IsPopupFullScreenUpdate(mozContainerSize, aRegion)
                          : IsWindowFullScreenUpdate(mozContainerSize, aRegion);
  if (windowRedraw) {
    // Clear buffer when we (re)draw new transparent popup window,
    // otherwise leave it as-is, mBufferNeedsClear can be set from previous
    // (already pending) commits which are cached now.
    mBufferNeedsClear =
        mWindow->WaylandSurfaceNeedsClear() || isTransparentPopup;

    // We do full buffer repaint so clear our cached drawings.
    mDelayedImageCommits.Clear();
    mWaylandBufferDamage.SetEmpty();
    mCanSwitchWaylandBuffer = true;
    mWLBufferIsDirty = false;

    // Store info that we can safely invalidate whole screen.
    mWaylandFullscreenDamage = true;
  } else {
    // We can switch buffer if there isn't any content committed
    // to active buffer.
    mCanSwitchWaylandBuffer = !mWLBufferIsDirty;
  }

  LOGWAYLAND(
      ("WindowSurfaceWayland::Lock [%p] [%d,%d] -> [%d x %d] rects %d "
       "MozContainer size [%d x %d]\n",
       (void*)this, lockSize.x, lockSize.y, lockSize.width, lockSize.height,
       aRegion.GetNumRects(), mozContainerSize.width, mozContainerSize.height));
  LOGWAYLAND(("   nsWindow = %p\n", mWindow.get()));
  LOGWAYLAND(("   isPopup = %d\n", mWindow->IsWaylandPopup()));
  LOGWAYLAND(("   isTransparentPopup = %d\n", isTransparentPopup));
  LOGWAYLAND(("   IsPopupFullScreenUpdate = %d\n",
              IsPopupFullScreenUpdate(mozContainerSize, aRegion)));
  LOGWAYLAND(("   IsWindowFullScreenUpdate = %d\n",
              IsWindowFullScreenUpdate(mozContainerSize, aRegion)));
  LOGWAYLAND(("   mBufferNeedsClear = %d\n", mBufferNeedsClear));
  LOGWAYLAND(("   mWLBufferIsDirty = %d\n", mWLBufferIsDirty));
  LOGWAYLAND(("   mCanSwitchWaylandBuffer = %d\n", mCanSwitchWaylandBuffer));
  LOGWAYLAND(("   windowRedraw = %d\n", windowRedraw));

  if (!(mMozContainerSize == mozContainerSize)) {
    LOGWAYLAND(("   screen size changed\n"));
    if (!windowRedraw) {
      LOGWAYLAND(("   screen size changed without redraw!\n"));
      // Screen (window) size changed and we still have some painting pending
      // for the last window size. That can happen when window is resized.
      // We won't draw it but wait for new content.
      mDelayedImageCommits.Clear();
      mWaylandBufferDamage.SetEmpty();
      mCanSwitchWaylandBuffer = true;
      mWLBufferIsDirty = false;
      mBufferNeedsClear = true;
    }
    mMozContainerSize = mozContainerSize;
  }

  mDrawToWaylandBufferDirectly = windowRedraw || mSmoothRendering == CACHE_NONE;
  if (!mDrawToWaylandBufferDirectly && mSmoothRendering == CACHE_SMALL) {
    mDrawToWaylandBufferDirectly =
        (lockSize.width * 2 > mozContainerSize.width &&
         lockSize.height * 2 > mozContainerSize.height);
  }

  if (!mDrawToWaylandBufferDirectly) {
    // Don't switch wl_buffers when we cache drawings.
    mCanSwitchWaylandBuffer = false;
    LOGWAYLAND(("   Indirect drawing, mCanSwitchWaylandBuffer = %d\n",
                mCanSwitchWaylandBuffer));
  }

  if (mDrawToWaylandBufferDirectly) {
    LOGWAYLAND(("   Direct drawing\n"));
    RefPtr<gfx::DrawTarget> dt = LockWaylandBuffer();
    if (dt) {
#if MOZ_LOGGING
      mWaylandBuffer->DumpToFile("Lock");
#endif
      if (!windowRedraw) {
        DrawDelayedImageCommits(dt, mWaylandBufferDamage);
#if MOZ_LOGGING
        mWaylandBuffer->DumpToFile("Lock-after-commit");
#endif
      }
      mWLBufferIsDirty = true;
      return dt.forget();
    }
  }

  // We do indirect drawing because there isn't any front buffer available.
  // Do indirect drawing to mImageSurface which is commited to wayland
  // wl_buffer by DrawDelayedImageCommits() later.
  mDrawToWaylandBufferDirectly = false;

  LOGWAYLAND(("   Indirect drawing.\n"));
  return LockImageSurface(gfx::IntSize(lockSize.XMost(), lockSize.YMost()));
}

bool WindowImageSurface::OverlapsSurface(
    class WindowImageSurface& aBottomSurface) {
  return mUpdateRegion.Contains(aBottomSurface.mUpdateRegion);
}

void WindowImageSurface::DrawToTarget(
    gfx::DrawTarget* aDest, LayoutDeviceIntRegion& aWaylandBufferDamage) {
#ifdef MOZ_LOGGING
  gfx::IntRect bounds = mUpdateRegion.GetBounds().ToUnknownRect();
  LOGWAYLAND(("WindowImageSurface::DrawToTarget\n"));
  LOGWAYLAND(("    rects num %d\n", mUpdateRegion.GetNumRects()));
  LOGWAYLAND(("    bounds [ %d, %d] -> [%d x %d]\n", bounds.x, bounds.y,
              bounds.width, bounds.height));
#endif
  for (auto iter = mUpdateRegion.RectIter(); !iter.Done(); iter.Next()) {
    gfx::IntRect r(iter.Get().ToUnknownRect());
    LOGWAYLAND(
        ("    draw rect [%d,%d] -> [%d x %d]\n", r.x, r.y, r.width, r.height));
    aDest->CopySurface(mImageSurface, r, gfx::IntPoint(r.x, r.y));
  }
  aWaylandBufferDamage.OrWith(mUpdateRegion);
}

WindowImageSurface::WindowImageSurface(
    gfx::DataSourceSurface* aImageSurface,
    const LayoutDeviceIntRegion& aUpdateRegion)
    : mImageSurface(aImageSurface), mUpdateRegion(aUpdateRegion) {}

bool WindowSurfaceWayland::DrawDelayedImageCommits(
    gfx::DrawTarget* aDrawTarget, LayoutDeviceIntRegion& aWaylandBufferDamage) {
  unsigned int imagesNum = mDelayedImageCommits.Length();
  LOGWAYLAND(("WindowSurfaceWayland::DrawDelayedImageCommits [%p] len %d\n",
              (void*)this, imagesNum));
  for (unsigned int i = 0; i < imagesNum; i++) {
    mDelayedImageCommits[i].DrawToTarget(aDrawTarget, aWaylandBufferDamage);
  }
  mDelayedImageCommits.Clear();

  return (imagesNum != 0);
}

void WindowSurfaceWayland::CacheImageSurface(
    const LayoutDeviceIntRegion& aRegion) {
#ifdef MOZ_LOGGING
  gfx::IntRect bounds = aRegion.GetBounds().ToUnknownRect();
  LOGWAYLAND(("WindowSurfaceWayland::CacheImageSurface [%p]\n", (void*)this));
  LOGWAYLAND(("    rects num %d\n", aRegion.GetNumRects()));
  LOGWAYLAND(("    bounds [ %d, %d] -> [%d x %d]\n", bounds.x, bounds.y,
              bounds.width, bounds.height));
#endif

  mImageSurface->Unmap();
  WindowImageSurface surf = WindowImageSurface(mImageSurface, aRegion);

  if (mDelayedImageCommits.Length()) {
    auto lastSurf = mDelayedImageCommits.PopLastElement();
    if (surf.OverlapsSurface(lastSurf)) {
#ifdef MOZ_LOGGING
      {
        gfx::IntRect size =
            lastSurf.GetUpdateRegion()->GetBounds().ToUnknownRect();
        LOGWAYLAND(("    removing [ %d, %d] -> [%d x %d]\n", size.x, size.y,
                    size.width, size.height));
      }
#endif
    } else {
      mDelayedImageCommits.AppendElement(lastSurf);
    }
  }

  mDelayedImageCommits.AppendElement(surf);
  // mImageSurface is owned by mDelayedImageCommits
  mImageSurface = nullptr;

  LOGWAYLAND(
      ("    There's %d cached images\n", int(mDelayedImageCommits.Length())));
}

bool WindowSurfaceWayland::CommitImageCacheToWaylandBuffer() {
  if (!mDelayedImageCommits.Length()) {
    return false;
  }

  MOZ_ASSERT(!mDrawToWaylandBufferDirectly);

  RefPtr<gfx::DrawTarget> dt = LockWaylandBuffer();
  if (!dt) {
    return false;
  }

  LOGWAYLAND(("   Flushing %ld cached WindowImageSurfaces to Wayland buffer\n",
              long(mDelayedImageCommits.Length())));

  return DrawDelayedImageCommits(dt, mWaylandBufferDamage);
}

void WindowSurfaceWayland::FlushPendingCommits() {
  MutexAutoLock lock(mSurfaceLock);
  if (FlushPendingCommitsLocked()) {
    mWaylandDisplay->QueueSyncBegin();
  }
}

// When a new window is created we may not have a valid wl_surface
// for drawing (Gtk haven't created it yet). All commits are queued
// and FlushPendingCommitsLocked() is called by timer when wl_surface is ready
// for drawing.
static int WaylandBufferFlushPendingCommits(void* data) {
  WindowSurfaceWayland* aSurface = static_cast<WindowSurfaceWayland*>(data);
  aSurface->FlushPendingCommits();
  return true;
}

bool WindowSurfaceWayland::FlushPendingCommitsLocked() {
  LOGWAYLAND(
      ("WindowSurfaceWayland::FlushPendingCommitsLocked [%p]\n", (void*)this));
  LOGWAYLAND(("    mDrawToWaylandBufferDirectly = %d\n",
              mDrawToWaylandBufferDirectly));
  LOGWAYLAND(("    mCanSwitchWaylandBuffer = %d\n", mCanSwitchWaylandBuffer));
  LOGWAYLAND(("    mFrameCallback = %p\n", mFrameCallback));
  LOGWAYLAND(("    mLastCommittedSurfaceID = %d\n", mLastCommittedSurfaceID));
  LOGWAYLAND(("    mWLBufferIsDirty = %d\n", mWLBufferIsDirty));
  LOGWAYLAND(("    mBufferCommitAllowed = %d\n", mBufferCommitAllowed));

  // Reset if we're hidden.
  MozContainer* container = mWindow->GetMozContainer();
  moz_container_wayland_lock(container);
  auto unlockContainer =
      MakeScopeExit([&] { moz_container_wayland_unlock(container); });

  LOGWAYLAND(("    mContainer = %p\n", container));

  if (moz_container_wayland_get_and_reset_remapped(container)) {
    LOGWAYLAND(
        ("    moz_container [%p] is remapped, clear callbacks.\n", container));
    mLastCommittedSurfaceID = -1;
    g_clear_pointer(&mFrameCallback, wl_callback_destroy);
  }
  if (moz_container_wayland_is_inactive(container)) {
    LOGWAYLAND(("    Quit - moz_container [%p] is inactive.\n", container));
    if (mSurfaceReadyTimerID) {
      g_source_remove(mSurfaceReadyTimerID);
      mSurfaceReadyTimerID = 0;
    }
    return false;
  }

  if (!mBufferCommitAllowed) {
    LOGWAYLAND(("    Quit - buffer commit is not allowed.\n"));
    return false;
  }

  if (CommitImageCacheToWaylandBuffer()) {
    mWLBufferIsDirty = true;
  }

  // There's nothing to do here
  if (!mWLBufferIsDirty) {
    LOGWAYLAND(("    Quit - no pending commit.\n"));
    return false;
  }

  MOZ_ASSERT(!mWaylandBuffer->IsAttached(),
             "We can't draw to attached wayland buffer!");

  LOGWAYLAND(("    Drawing pending commits.\n"));
  wl_surface* waylandSurface =
      moz_container_wayland_get_surface_locked(container);
  if (!waylandSurface) {
    LOGWAYLAND(
        ("    moz_container_wayland_get_surface_locked() failed, delay "
         "commit.\n"));

    if (!mSurfaceReadyTimerID) {
      mSurfaceReadyTimerID = (int)g_timeout_add(
          EVENT_LOOP_DELAY, &WaylandBufferFlushPendingCommits, this);
    }
    return true;
  }
  if (mSurfaceReadyTimerID) {
    g_source_remove(mSurfaceReadyTimerID);
    mSurfaceReadyTimerID = 0;
  }

  LOGWAYLAND(("    We have wl_surface %p ID [%d] to commit in.\n",
              waylandSurface,
              wl_proxy_get_id((struct wl_proxy*)waylandSurface)));

  wl_proxy_set_queue((struct wl_proxy*)waylandSurface,
                     mWaylandDisplay->GetEventQueue());

  // We have an active frame callback request so handle it.
  if (mFrameCallback) {
    int waylandSurfaceID =
        (int)wl_proxy_get_id((struct wl_proxy*)waylandSurface);
    if (waylandSurfaceID == mLastCommittedSurfaceID) {
      LOGWAYLAND(("    [%p] wait for frame callback ID %d.\n", (void*)this,
                  waylandSurfaceID));
      // We have an active frame callback pending from our recent surface.
      // It means we should defer the commit to FrameCallbackHandler().
      return true;
    }
    LOGWAYLAND(("    Removing wrong frame callback [%p] ID %d.\n",
                mFrameCallback,
                wl_proxy_get_id((struct wl_proxy*)mFrameCallback)));
    // If our stored wl_surface does not match the actual one it means the frame
    // callback is no longer active and we should release it.
    wl_callback_destroy(mFrameCallback);
    mFrameCallback = nullptr;
    mLastCommittedSurfaceID = -1;
  }

  if (mWaylandFullscreenDamage) {
    LOGWAYLAND(("    wl_surface_damage full screen\n"));
    wl_surface_damage_buffer(waylandSurface, 0, 0, INT_MAX, INT_MAX);
  } else {
    for (auto iter = mWaylandBufferDamage.RectIter(); !iter.Done();
         iter.Next()) {
      mozilla::LayoutDeviceIntRect r = iter.Get();
      LOGWAYLAND(("   wl_surface_damage_buffer [%d, %d] -> [%d, %d]\n", r.x,
                  r.y, r.width, r.height));
      wl_surface_damage_buffer(waylandSurface, r.x, r.y, r.width, r.height);
    }
  }

#if MOZ_LOGGING
  mWaylandBuffer->DumpToFile("Commit");
#endif

  // Clear all back buffer damage as we're committing
  // all requested regions.
  mWaylandFullscreenDamage = false;
  mWaylandBufferDamage.SetEmpty();

  mFrameCallback = wl_surface_frame(waylandSurface);
  wl_callback_add_listener(mFrameCallback, &sFrameListenerWindowSurfaceWayland,
                           this);

  mWaylandBuffer->AttachAndCommit(waylandSurface);
  wl_display_flush(GetWaylandDisplay()->GetDisplay());

  mLastCommittedSurfaceID =
      (int)wl_proxy_get_id((struct wl_proxy*)waylandSurface);
  mLastCommitTime = g_get_monotonic_time() / 1000;

  // There's no pending commit, all changes are sent to compositor.
  mWLBufferIsDirty = false;

  return true;
}

void WindowSurfaceWayland::Commit(const LayoutDeviceIntRegion& aInvalidRegion) {
#ifdef MOZ_LOGGING
  {
    gfx::IntRect lockSize = aInvalidRegion.GetBounds().ToUnknownRect();
    LOGWAYLAND(
        ("WindowSurfaceWayland::Commit [%p] damage size [%d, %d] -> [%d x %d] "
         "MozContainer [%d x %d]\n",
         (void*)this, lockSize.x, lockSize.y, lockSize.width, lockSize.height,
         mMozContainerSize.width, mMozContainerSize.height));
    LOGWAYLAND(("    mDrawToWaylandBufferDirectly = %d\n",
                mDrawToWaylandBufferDirectly));
  }
#endif

  MutexAutoLock lock(mSurfaceLock);

  if (mDrawToWaylandBufferDirectly) {
    mWaylandBufferDamage.OrWith(aInvalidRegion);
  } else {
    CacheImageSurface(aInvalidRegion);
  }

  mBufferCommitAllowed = true;
  if (FlushPendingCommitsLocked()) {
    mWaylandDisplay->QueueSyncBegin();
  }
}

void WindowSurfaceWayland::FrameCallbackHandler() {
  MOZ_ASSERT(mFrameCallback != nullptr,
             "FrameCallbackHandler() called without valid frame callback!");
  MOZ_ASSERT(mLastCommittedSurfaceID != -1,
             "FrameCallbackHandler() called without valid wl_surface!");
  LOGWAYLAND(("WindowSurfaceWayland::FrameCallbackHandler [%p]\n", this));

  MutexAutoLock lock(mSurfaceLock);

  wl_callback_destroy(mFrameCallback);
  mFrameCallback = nullptr;

  if (FlushPendingCommitsLocked()) {
    mWaylandDisplay->QueueSyncBegin();
  }
}

void WindowSurfaceWayland::FrameCallbackHandler(void* aData,
                                                struct wl_callback* aCallback,
                                                uint32_t aTime) {
  auto* surface = reinterpret_cast<WindowSurfaceWayland*>(aData);
  surface->FrameCallbackHandler();
}

void WindowSurfaceWayland::BufferReleaseCallbackHandler(wl_buffer* aBuffer) {
  FlushPendingCommits();
}

void WindowSurfaceWayland::BufferReleaseCallbackHandler(void* aData,
                                                        wl_buffer* aBuffer) {
  auto* surface = reinterpret_cast<WindowSurfaceWayland*>(aData);
  surface->BufferReleaseCallbackHandler(aBuffer);
}

}  // namespace mozilla::widget
