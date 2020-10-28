/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef _MOZILLA_WIDGET_GTK_WINDOW_SURFACE_WAYLAND_H
#define _MOZILLA_WIDGET_GTK_WINDOW_SURFACE_WAYLAND_H

#include <prthread.h>
#include "gfxImageSurface.h"
#include "mozilla/gfx/2D.h"
#include "mozilla/gfx/Types.h"
#include "nsWaylandDisplay.h"
#include "nsWindow.h"
#include "WindowSurface.h"

#define BACK_BUFFER_NUM 3

namespace mozilla {
namespace widget {

class WindowSurfaceWayland;

// Allocates and owns shared memory for Wayland drawing surface
class WaylandShmPool {
 public:
  bool Create(RefPtr<nsWaylandDisplay> aWaylandDisplay, int aSize);
  void Release();
  wl_shm_pool* GetShmPool() { return mShmPool; };
  void* GetImageData() { return mImageData; };
  void SetImageDataFromPool(class WaylandShmPool* aSourcePool,
                            int aImageDataSize);
  WaylandShmPool();
  ~WaylandShmPool();

 private:
  wl_shm_pool* mShmPool;
  int mShmPoolFd;
  int mAllocatedSize;
  void* mImageData;
};

// Holds actual graphics data for wl_surface
class WindowBackBuffer {
 public:
  explicit WindowBackBuffer(WindowSurfaceWayland* aWindowSurfaceWayland);
  ~WindowBackBuffer();

  already_AddRefed<gfx::DrawTarget> Lock();
  bool IsLocked() { return mIsLocked; };
  void Unlock() { mIsLocked = false; };

  void Attach(wl_surface* aSurface);
  void Detach(wl_buffer* aBuffer);
  bool IsAttached() { return mAttached; }
  void SetAttached() { mAttached = true; };

  void Clear();
  bool Create(int aWidth, int aHeight);
  bool Resize(int aWidth, int aHeight);
  bool SetImageDataFromBuffer(class WindowBackBuffer* aSourceBuffer);

  int GetWidth() { return mWidth; };
  int GetHeight() { return mHeight; };

  wl_buffer* GetWlBuffer() { return mWLBuffer; };

  bool IsMatchingSize(int aWidth, int aHeight) {
    return aWidth == GetWidth() && aHeight == GetHeight();
  }
  bool IsMatchingSize(class WindowBackBuffer* aBuffer) {
    return aBuffer->IsMatchingSize(GetWidth(), GetHeight());
  }
  static gfx::SurfaceFormat GetSurfaceFormat() { return mFormat; }

  RefPtr<nsWaylandDisplay> GetWaylandDisplay();

 private:
  void ReleaseShmSurface();

  static gfx::SurfaceFormat mFormat;
  WindowSurfaceWayland* mWindowSurfaceWayland;

  // WaylandShmPool provides actual shared memory we draw into
  WaylandShmPool mShmPool;

  // wl_buffer is a wayland object that encapsulates the shared memory
  // and passes it to wayland compositor by wl_surface object.
  wl_buffer* mWLBuffer;
  int mWidth;
  int mHeight;
  bool mAttached;
  bool mIsLocked;
};

class WindowImageSurface {
 public:
  static void Draw(gfx::SourceSurface* aSurface, gfx::DrawTarget* aDest,
                   const LayoutDeviceIntRegion& aRegion);

  void Draw(gfx::DrawTarget* aDest,
            LayoutDeviceIntRegion& aWaylandBufferDamage);

  WindowImageSurface(gfxImageSurface* aImageSurface,
                     const LayoutDeviceIntRegion& aUpdateRegion);

  bool OverlapsSurface(class WindowImageSurface& aBottomSurface);

  const LayoutDeviceIntRegion* GetUpdateRegion() { return &mUpdateRegion; };

 private:
  RefPtr<gfx::SourceSurface> mSurface;
  RefPtr<gfxImageSurface> mImageSurface;
  const LayoutDeviceIntRegion mUpdateRegion;
};

// WindowSurfaceWayland is an abstraction for wl_surface
// and related management
class WindowSurfaceWayland : public WindowSurface {
 public:
  explicit WindowSurfaceWayland(nsWindow* aWindow);
  ~WindowSurfaceWayland();

  // Lock() / Commit() are called by gecko when Firefox
  // wants to display something. Lock() returns a DrawTarget
  // where gecko paints. When gecko is done it calls Commit()
  // and we try to send the DrawTarget (backed by wl_buffer)
  // to wayland compositor.
  //
  // If we fail (wayland compositor is busy,
  // wl_surface is not created yet) we queue the painting
  // and we send it to wayland compositor in FrameCallbackHandler()/
  // CommitWaylandBuffer().
  already_AddRefed<gfx::DrawTarget> Lock(
      const LayoutDeviceIntRegion& aRegion) override;
  void Commit(const LayoutDeviceIntRegion& aInvalidRegion) final;

  // It's called from wayland compositor when there's the right
  // time to send wl_buffer to display. It's no-op if there's no
  // queued commits.
  void FrameCallbackHandler();

  // Try to commit all queued drawings to Wayland compositor. This is usually
  // called from other routines but can be used to explicitly flush
  // all drawings as we do when wl_buffer is released
  // (see WindowBackBufferShm::Detach() for instance).
  void CommitWaylandBuffer();

  RefPtr<nsWaylandDisplay> GetWaylandDisplay() { return mWaylandDisplay; };

  // Image cache mode can be set by widget.wayland_cache_mode
  typedef enum {
    // Cache and clip all drawings, default. It's slowest
    // but also without any rendered artifacts.
    CACHE_ALL = 0,
    // Cache drawing only when back buffer is missing. May produce
    // some rendering artifacts and flickering when partial screen update
    // is rendered.
    CACHE_MISSING = 1,
    // Don't cache anything, draw only when back buffer is available.
    CACHE_NONE = 2
  } RenderingCacheMode;

 private:
  WindowBackBuffer* GetWaylandBufferWithSwitch();
  WindowBackBuffer* GetWaylandBufferRecent();
  WindowBackBuffer* SetNewWaylandBuffer();
  WindowBackBuffer* CreateWaylandBuffer(int aWidth, int aHeight);
  WindowBackBuffer* WaylandBufferFindAvailable(int aWidth, int aHeight);

  already_AddRefed<gfx::DrawTarget> LockWaylandBuffer();
  void UnlockWaylandBuffer();

  already_AddRefed<gfx::DrawTarget> LockImageSurface(
      const gfx::IntSize& aLockSize);

  void CacheImageSurface(const LayoutDeviceIntRegion& aRegion);
  bool CommitImageCacheToWaylandBuffer();

  void DrawDelayedImageCommits(gfx::DrawTarget* aDrawTarget,
                               LayoutDeviceIntRegion& aWaylandBufferDamage);

  // TODO: Do we need to hold a reference to nsWindow object?
  nsWindow* mWindow;
  // Buffer screen rects helps us understand if we operate on
  // the same window size as we're called on WindowSurfaceWayland::Lock().
  // mLockedScreenRect is window size when our wayland buffer was allocated.
  LayoutDeviceIntRect mLockedScreenRect;

  // mWLBufferRect is an intersection of mozcontainer widgetsize and
  // mLockedScreenRect size. It can be different than mLockedScreenRect
  // during resize when mBounds are updated immediately but actual
  // GtkWidget size is updated asynchronously (see Bug 1489463).
  LayoutDeviceIntRect mWLBufferRect;
  RefPtr<nsWaylandDisplay> mWaylandDisplay;

  // Actual buffer (backed by wl_buffer) where all drawings go into.
  // Drawn areas are stored at mWaylandBufferDamage and if there's
  // any uncommited drawings which needs to be send to wayland compositor
  // the mBufferPendingCommit is set.
  WindowBackBuffer* mWaylandBuffer;
  WindowBackBuffer* mShmBackupBuffer[BACK_BUFFER_NUM];

  // When mWaylandFullscreenDamage we invalidate whole surface,
  // otherwise partial screen updates (mWaylandBufferDamage) are used.
  bool mWaylandFullscreenDamage;
  LayoutDeviceIntRegion mWaylandBufferDamage;

  // After every commit to wayland compositor a frame callback is requested.
  // Any next commit to wayland compositor will happen when frame callback
  // comes from wayland compositor back as it's the best time to do the commit.
  wl_callback* mFrameCallback;
  wl_surface* mLastCommittedSurface;

  // Cached drawings. If we can't get WaylandBuffer (wl_buffer) at
  // WindowSurfaceWayland::Lock() we direct gecko rendering to
  // mImageSurface.
  // If we can't get WaylandBuffer at WindowSurfaceWayland::Commit()
  // time, mImageSurface is moved to mDelayedImageCommits which
  // holds all cached drawings.
  // mDelayedImageCommits can be drawn by FrameCallbackHandler()
  // or when WaylandBuffer is detached.
  RefPtr<gfxImageSurface> mImageSurface;
  AutoTArray<WindowImageSurface, 30> mDelayedImageCommits;

  int64_t mLastCommitTime;

  // Indicates that we don't have any cached drawings at mDelayedImageCommits
  // and WindowSurfaceWayland::Lock() returned WaylandBuffer to gecko
  // to draw into.
  bool mDrawToWaylandBufferDirectly;

  // Set when our cached drawings (mDelayedImageCommits) contains
  // full screen damage. That means we can safely switch WaylandBuffer
  // at LockWaylandBuffer().
  bool mCanSwitchWaylandBuffer;

  // Set when actual WaylandBuffer contains drawings which are not send to
  // wayland compositor yet.
  bool mBufferPendingCommit;

  // We can't send WaylandBuffer (wl_buffer) to compositor when gecko
  // is rendering into it (i.e. between WindowSurfaceWayland::Lock() /
  // WindowSurfaceWayland::Commit()).
  // Thus we use mBufferCommitAllowed to disable commit by
  // CommitWaylandBuffer().
  bool mBufferCommitAllowed;

  // We need to clear WaylandBuffer when entire transparent window is repainted.
  // This typically apply to popup windows.
  bool mBufferNeedsClear;

  // Cache all drawings except fullscreen updates.
  // Avoid any rendering artifacts for significant performance penality.
  bool mSmoothRendering;

  bool mIsMainThread;
};

}  // namespace widget
}  // namespace mozilla

#endif  // _MOZILLA_WIDGET_GTK_WINDOW_SURFACE_WAYLAND_H
