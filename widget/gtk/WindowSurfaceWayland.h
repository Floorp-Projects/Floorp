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
#include "WaylandDMABufSurface.h"

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
  virtual already_AddRefed<gfx::DrawTarget> Lock() = 0;
  virtual void Unlock() = 0;
  virtual bool IsLocked() = 0;

  void Attach(wl_surface* aSurface);
  virtual void Detach(wl_buffer* aBuffer) = 0;
  virtual bool IsAttached() = 0;

  virtual void Clear() = 0;
  virtual bool Resize(int aWidth, int aHeight) = 0;

  virtual int GetWidth() = 0;
  virtual int GetHeight() = 0;
  virtual wl_buffer* GetWlBuffer() = 0;
  virtual void SetAttached() = 0;

  virtual bool SetImageDataFromBuffer(
      class WindowBackBuffer* aSourceBuffer) = 0;

  bool IsMatchingSize(int aWidth, int aHeight) {
    return aWidth == GetWidth() && aHeight == GetHeight();
  }
  bool IsMatchingSize(class WindowBackBuffer* aBuffer) {
    return aBuffer->IsMatchingSize(GetWidth(), GetHeight());
  }

  static gfx::SurfaceFormat GetSurfaceFormat() { return mFormat; }

  nsWaylandDisplay* GetWaylandDisplay() { return mWaylandDisplay; };

  WindowBackBuffer(nsWaylandDisplay* aWaylandDisplay)
      : mWaylandDisplay(aWaylandDisplay){};
  virtual ~WindowBackBuffer(){};

 private:
  static gfx::SurfaceFormat mFormat;
  nsWaylandDisplay* mWaylandDisplay;
};

class WindowBackBufferShm : public WindowBackBuffer {
 public:
  WindowBackBufferShm(nsWaylandDisplay* aWaylandDisplay, int aWidth,
                      int aHeight);
  ~WindowBackBufferShm();

  already_AddRefed<gfx::DrawTarget> Lock();
  bool IsLocked() { return mIsLocked; };
  void Unlock() { mIsLocked = false; };

  void Detach(wl_buffer* aBuffer);
  bool IsAttached() { return mAttached; }
  void SetAttached() { mAttached = true; };

  void Clear();
  bool Resize(int aWidth, int aHeight);
  bool SetImageDataFromBuffer(class WindowBackBuffer* aSourceBuffer);

  int GetWidth() { return mWidth; };
  int GetHeight() { return mHeight; };

  wl_buffer* GetWlBuffer() { return mWaylandBuffer; };

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
  bool mIsLocked;
};

class WindowBackBufferDMABuf : public WindowBackBuffer {
 public:
  WindowBackBufferDMABuf(nsWaylandDisplay* aWaylandDisplay, int aWidth,
                         int aHeight);
  ~WindowBackBufferDMABuf();

  bool IsAttached();
  void SetAttached();

  int GetWidth();
  int GetHeight();
  wl_buffer* GetWlBuffer();

  bool SetImageDataFromBuffer(class WindowBackBuffer* aSourceBuffer);

  already_AddRefed<gfx::DrawTarget> Lock();
  bool IsLocked();
  void Unlock();

  void Clear();
  void Detach(wl_buffer* aBuffer);
  bool Resize(int aWidth, int aHeight);

 private:
  WaylandDMABufSurface mDMAbufSurface;
};

class WindowImageSurface {
 public:
  static void Draw(gfx::SourceSurface* aSurface, gfx::DrawTarget* aDest,
                   const LayoutDeviceIntRegion& aRegion);

  void Draw(gfx::DrawTarget* aDest,
            LayoutDeviceIntRegion& aWaylandBufferDamage);

  WindowImageSurface(gfx::SourceSurface* aSurface,
                     const LayoutDeviceIntRegion& aUpdateRegion);

 private:
  RefPtr<gfx::SourceSurface> mSurface;
  const LayoutDeviceIntRegion mUpdateRegion;
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
  WindowBackBuffer* CreateWaylandBuffer(int aWidth, int aHeight);
  WindowBackBuffer* GetWaylandBufferToDraw(bool aCanSwitchBuffer);

  already_AddRefed<gfx::DrawTarget> LockWaylandBuffer(bool aCanSwitchBuffer);
  void UnlockWaylandBuffer();

  already_AddRefed<gfx::DrawTarget> LockImageSurface(
      const gfx::IntSize& aLockSize);
  bool CommitImageSurfaceToWaylandBuffer(
      const LayoutDeviceIntRegion& aRegion,
      LayoutDeviceIntRegion& aWaylandBufferDamage);
  void CommitWaylandBuffer();
  void CalcRectScale(LayoutDeviceIntRect& aRect, int scale);

  void DrawDelayedImageCommits(gfx::DrawTarget* aDrawTarget,
                               LayoutDeviceIntRegion& aWaylandBufferDamage);

  // TODO: Do we need to hold a reference to nsWindow object?
  nsWindow* mWindow;
  // Buffer screen rects helps us understand if we operate on
  // the same window size as we're called on WindowSurfaceWayland::Lock().
  // mBufferScreenRect is window size when our wayland buffer was allocated.
  LayoutDeviceIntRect mBufferScreenRect;
  nsWaylandDisplay* mWaylandDisplay;
  WindowBackBuffer* mWaylandBuffer;
  LayoutDeviceIntRegion mWaylandBufferDamage;
  WindowBackBuffer* mBackupBuffer[BACK_BUFFER_NUM];
  wl_callback* mFrameCallback;
  wl_surface* mLastCommittedSurface;
  MessageLoop* mDisplayThreadMessageLoop;
  WindowSurfaceWayland** mDelayedCommitHandle;
  RefPtr<gfxImageSurface> mImageSurface;
  AutoTArray<WindowImageSurface, 30> mDelayedImageCommits;
  bool mDrawToWaylandBufferDirectly;
  bool mPendingCommit;
  bool mWholeWindowBufferDamage;
  bool mBufferNeedsClear;
  bool mIsMainThread;
  bool mNeedScaleFactorUpdate;

  static bool UseDMABufBackend();
  static bool mUseDMABufInitialized;
  static bool mUseDMABuf;
};

}  // namespace widget
}  // namespace mozilla

#endif  // _MOZILLA_WIDGET_GTK_WINDOW_SURFACE_WAYLAND_H
