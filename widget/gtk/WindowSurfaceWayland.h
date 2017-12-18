/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef _MOZILLA_WIDGET_GTK_WINDOW_SURFACE_WAYLAND_H
#define _MOZILLA_WIDGET_GTK_WINDOW_SURFACE_WAYLAND_H

#include <prthread.h>

namespace mozilla {
namespace widget {

// Our general connection to Wayland display server,
// holds our display connection and runs event loop.
class nsWaylandDisplay : public nsISupports {
  NS_DECL_THREADSAFE_ISUPPORTS

public:
  nsWaylandDisplay(wl_display *aDisplay);

  wl_shm*             GetShm();
  void                SetShm(wl_shm* aShm)   { mShm = aShm; };

  wl_display*         GetDisplay()           { return mDisplay; };
  wl_event_queue*     GetEventQueue()        { return mEventQueue; };
  gfx::SurfaceFormat  GetSurfaceFormat()     { return mFormat; };
  bool                DisplayLoop();
  bool                Matches(wl_display *aDisplay);

private:
  virtual ~nsWaylandDisplay();

  PRThread*           mThreadId;
  gfx::SurfaceFormat  mFormat;
  wl_shm*             mShm;
  wl_event_queue*     mEventQueue;
  wl_display*         mDisplay;
};

// Allocates and owns shared memory for Wayland drawing surface
class WaylandShmPool {
public:
  WaylandShmPool(nsWaylandDisplay* aDisplay, int aSize);
  ~WaylandShmPool();

  bool                Resize(int aSize);
  wl_shm_pool*        GetShmPool()    { return mShmPool;   };
  void*               GetImageData()  { return mImageData; };
  void                SetImageDataFromPool(class WaylandShmPool* aSourcePool,
                                           int aImageDataSize);

private:
  int CreateTemporaryFile(int aSize);

  wl_shm_pool*        mShmPool;
  int                 mShmPoolFd;
  int                 mAllocatedSize;
  void*               mImageData;
};

// Holds actual graphics data for wl_surface
class WindowBackBuffer {
public:
  WindowBackBuffer(nsWaylandDisplay* aDisplay, int aWidth, int aHeight);
  ~WindowBackBuffer();

  already_AddRefed<gfx::DrawTarget> Lock(const LayoutDeviceIntRegion& aRegion);

  void Attach(wl_surface* aSurface);
  void Detach();
  bool IsAttached() { return mAttached; }

  bool Resize(int aWidth, int aHeight);
  bool SetImageDataFromBackBuffer(class WindowBackBuffer* aSourceBuffer);

  bool IsMatchingSize(int aWidth, int aHeight)
  {
    return aWidth == mWidth && aHeight == mHeight;
  }
  bool IsMatchingSize(class WindowBackBuffer *aBuffer)
  {
    return aBuffer->mWidth == mWidth && aBuffer->mHeight == mHeight;
  }

private:
  void Create(int aWidth, int aHeight);
  void Release();

  // WaylandShmPool provides actual shared memory we draw into
  WaylandShmPool      mShmPool;

  // wl_buffer is a wayland object that encapsulates the shared memory
  // and passes it to wayland compositor by wl_surface object.
  wl_buffer*          mWaylandBuffer;
  int                 mWidth;
  int                 mHeight;
  bool                mAttached;
  nsWaylandDisplay*   mWaylandDisplay;
};

// WindowSurfaceWayland is an abstraction for wl_surface
// and related management
class WindowSurfaceWayland : public WindowSurface {
public:
  WindowSurfaceWayland(nsWindow *aWindow);
  ~WindowSurfaceWayland();

  already_AddRefed<gfx::DrawTarget> Lock(const LayoutDeviceIntRegion& aRegion) override;
  void                      Commit(const LayoutDeviceIntRegion& aInvalidRegion) final;
  void                      FrameCallbackHandler();

private:
  WindowBackBuffer*         GetBufferToDraw(int aWidth, int aHeight);

  // TODO: Do we need to hold a reference to nsWindow object?
  nsWindow*                 mWindow;
  nsWaylandDisplay*         mWaylandDisplay;
  WindowBackBuffer*         mFrontBuffer;
  WindowBackBuffer*         mBackBuffer;
  wl_callback*              mFrameCallback;
  wl_surface*               mFrameCallbackSurface;
  bool                      mDelayedCommit;
  bool                      mFullScreenDamage;
  bool                      mIsMainThread;
};

}  // namespace widget
}  // namespace mozilla

#endif // _MOZILLA_WIDGET_GTK_WINDOW_SURFACE_WAYLAND_H
