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

private:
  int CreateTemporaryFile(int aSize);

  wl_shm_pool*        mShmPool;
  int                 mShmPoolFd;
  int                 mAllocatedSize;
  void*               mImageData;
};

}  // namespace widget
}  // namespace mozilla

#endif // _MOZILLA_WIDGET_GTK_WINDOW_SURFACE_WAYLAND_H
