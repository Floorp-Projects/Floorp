/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef _MOZILLA_WIDGET_GTK_WINDOW_SURFACE_WAYLAND_H
#define _MOZILLA_WIDGET_GTK_WINDOW_SURFACE_WAYLAND_H

#include <prthread.h>
#include "mozilla/gfx/Types.h"
#include "nsWaylandDisplay.h"

#define BACK_BUFFER_NUM 2

namespace mozilla {
namespace widget {

// Allocates and owns shared memory for Wayland drawing surface
class WaylandShmPool {
 public:
  WaylandShmPool(nsWaylandDisplay* aDisplay, int aSize);
  ~WaylandShmPool();

  bool Resize(int aSize);
  wl_shm_pool* GetShmPool() { return mShmPool; };
  void* GetImageData() { return mImageData; };
  void SetImageDataFromPool(class WaylandShmPool* aSourcePool,
                            int aImageDataSize);

 private:
  int CreateTemporaryFile(int aSize);

  wl_shm_pool* mShmPool;
  int mShmPoolFd;
  int mAllocatedSize;
  void* mImageData;
};

// Holds actual graphics data for wl_surface
class WindowBackBuffer {
 public:
  WindowBackBuffer(nsWaylandDisplay* aDisplay, int aWidth, int aHeight);
  ~WindowBackBuffer();

  already_AddRefed<gfx::DrawTarget> Lock();

  void Attach(wl_surface* aSurface);
  void Detach(wl_buffer* aBuffer);
  bool IsAttached() { return mAttached; }

  void Clear();
  bool Resize(int aWidth, int aHeight);
  bool SetImageDataFromBuffer(class WindowBackBuffer* aSourceBuffer);

  bool IsMatchingSize(int aWidth, int aHeight) {
    return aWidth == mWidth && aHeight == mHeight;
  }
  bool IsMatchingSize(class WindowBackBuffer* aBuffer) {
    return aBuffer->mWidth == mWidth && aBuffer->mHeight == mHeight;
  }

  static gfx::SurfaceFormat GetSurfaceFormat() { return mFormat; }

 private:
  void Create(int aWidth, int aHeight);
  void Release();

  // WaylandShmPool provides actual shared memory we draw into
  WaylandShmPool mShmPool;

  // wl_buffer is a wayland object that encapsulates the shared memory
  // and passes it to wayland compositor by wl_surface object.
  wl_buffer* mWaylandBuffer;
  int mWidth;
  int mHeight;
  bool mAttached;
  nsWaylandDisplay* mWaylandDisplay;
  static gfx::SurfaceFormat mFormat;
};

// WindowSurfaceWayland is an abstraction for wl_surface
// and related management
class WindowSurfaceWayland : public WindowSurface {
 public:
  explicit WindowSurfaceWayland(nsWindow* aWindow);
  ~WindowSurfaceWayland();

  already_AddRefed<gfx::DrawTarget> Lock(
      const LayoutDeviceIntRegion& aRegion) override;
  void Commit(const LayoutDeviceIntRegion& aInvalidRegion) final;
  void FrameCallbackHandler();
  void DelayedCommitHandler();

 private:
  WindowBackBuffer* GetWaylandBufferToDraw(int aWidth, int aHeight);

  already_AddRefed<gfx::DrawTarget> LockWaylandBuffer(int aWidth, int aHeight,
                                                      bool aClearBuffer);
  already_AddRefed<gfx::DrawTarget> LockImageSurface(
      const gfx::IntSize& aLockSize);
  bool CommitImageSurfaceToWaylandBuffer(const LayoutDeviceIntRegion& aRegion);
  void CommitWaylandBuffer();
  void CalcRectScale(LayoutDeviceIntRect& aRect, int scale);

  // TODO: Do we need to hold a reference to nsWindow object?
  nsWindow* mWindow;
  nsWaylandDisplay* mWaylandDisplay;
  WindowBackBuffer* mWaylandBuffer;
  LayoutDeviceIntRegion mWaylandBufferDamage;
  WindowBackBuffer* mBackupBuffer[BACK_BUFFER_NUM];
  RefPtr<gfxImageSurface> mImageSurface;
  wl_callback* mFrameCallback;
  wl_surface* mLastCommittedSurface;
  MessageLoop* mDisplayThreadMessageLoop;
  WindowSurfaceWayland** mDelayedCommitHandle;
  bool mDrawToWaylandBufferDirectly;
  bool mPendingCommit;
  bool mWaylandBufferFullScreenDamage;
  bool mIsMainThread;
  bool mNeedScaleFactorUpdate;
  bool mWaitToFullScreenUpdate;
};

}  // namespace widget
}  // namespace mozilla

#endif  // _MOZILLA_WIDGET_GTK_WINDOW_SURFACE_WAYLAND_H
