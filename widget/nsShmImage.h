/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef __mozilla_widget_nsShmImage_h__
#define __mozilla_widget_nsShmImage_h__

#if defined(MOZ_X11)
#  define MOZ_HAVE_SHMIMAGE
#endif

#ifdef MOZ_HAVE_SHMIMAGE

#include "mozilla/gfx/2D.h"
#include "nsIWidget.h"
#include "Units.h"

#include <X11/Xlib-xcb.h>
#include <xcb/shm.h>

class nsShmImage {
  // bug 1168843, compositor thread may create shared memory instances that are destroyed by main thread on shutdown, so this must use thread-safe RC to avoid hitting assertion
  NS_INLINE_DECL_THREADSAFE_REFCOUNTING(nsShmImage)

public:
  static bool UseShm();

  already_AddRefed<mozilla::gfx::DrawTarget>
    CreateDrawTarget(const mozilla::LayoutDeviceIntRegion& aRegion);

  void Put(const mozilla::LayoutDeviceIntRegion& aRegion);

  nsShmImage(Display* aDisplay,
             Drawable aWindow,
             Visual* aVisual,
             unsigned int aDepth);

private:
  ~nsShmImage();

  bool InitExtension();

  bool CreateShmSegment();
  void DestroyShmSegment();

  bool CreateImage(const mozilla::gfx::IntSize& aSize);
  void DestroyImage();

  xcb_connection_t*            mConnection;
  Window                       mWindow;
  Visual*                      mVisual;
  unsigned int                 mDepth;

  mozilla::gfx::SurfaceFormat  mFormat;
  mozilla::gfx::IntSize        mSize;

  xcb_pixmap_t                 mPixmap;
  xcb_gcontext_t               mGC;
  xcb_void_cookie_t            mPutRequest;
  xcb_get_input_focus_cookie_t mSyncRequest;
  bool                         mRequestPending;

  xcb_shm_seg_t                mShmSeg;
  int                          mShmId;
  uint8_t*                     mShmAddr;
};

#endif // MOZ_HAVE_SHMIMAGE

#endif
