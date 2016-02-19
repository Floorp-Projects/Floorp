/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#if defined(MOZ_WIDGET_GTK)
#include <gtk/gtk.h>
#include <gdk/gdkx.h>
#elif defined(MOZ_WIDGET_QT)
#include <QWindow>
#endif

#include "nsShmImage.h"
#ifdef MOZ_WIDGET_GTK
#include "gfxPlatformGtk.h"
#endif

#ifdef MOZ_HAVE_SHMIMAGE
#include "mozilla/X11Util.h"

#include "mozilla/ipc/SharedMemory.h"

#include <errno.h>
#include <string.h>
#include <sys/ipc.h>
#include <sys/shm.h>

using namespace mozilla::ipc;
using namespace mozilla::gfx;

// If XShm isn't available to our client, we'll try XShm once, fail,
// set this to false and then never try again.
static bool gShmAvailable = true;
bool nsShmImage::UseShm()
{
#ifdef MOZ_WIDGET_GTK
    return (gShmAvailable && !gfxPlatformGtk::GetPlatform()->UseXRender());
#else
    return gShmAvailable;
#endif
}

#ifdef MOZ_WIDGET_GTK
static int gShmError = 0;

static int
TrapShmError(Display* aDisplay, XErrorEvent* aEvent)
{
    // store the error code and ignore the error
    gShmError = aEvent->error_code;
    return 0;
}
#endif

bool
nsShmImage::CreateShmSegment()
{
  if (!mImage) {
    return false;
  }

  size_t size = SharedMemory::PageAlignedSize(mImage->bytes_per_line * mImage->height);

  mInfo.shmid = shmget(IPC_PRIVATE, size, IPC_CREAT | 0600);
  if (mInfo.shmid == -1) {
    return false;
  }

  mInfo.shmaddr = (char *)shmat(mInfo.shmid, nullptr, 0);
  if (mInfo.shmaddr == (void *)-1) {
    nsPrintfCString warning("shmat(): %s (%d)\n", strerror(errno), errno);
    NS_WARNING(warning.get());
    return false;
  }

  // Mark the handle as deleted so that, should this process go away, the
  // segment is cleaned up.
  shmctl(mInfo.shmid, IPC_RMID, 0);

#ifdef DEBUG
  struct shmid_ds info;
  if (shmctl(mInfo.shmid, IPC_STAT, &info) < 0) {
    return false;
  }

  MOZ_ASSERT(size <= info.shm_segsz,
             "Segment doesn't have enough space!");
#endif

  mInfo.readOnly = False;

  mImage->data = mInfo.shmaddr;

  return true;
}

void
nsShmImage::DestroyShmSegment()
{
  if (mInfo.shmid != -1) {
    shmdt(mInfo.shmaddr);
    mInfo.shmid = -1;
  }
}

bool
nsShmImage::CreateImage(const LayoutDeviceIntSize& aSize,
                        Display* aDisplay, Visual* aVisual, unsigned int aDepth)
{
  mDisplay = aDisplay;
  mImage = XShmCreateImage(aDisplay, aVisual, aDepth,
                           ZPixmap, nullptr,
                           &mInfo,
                           aSize.width, aSize.height);
  if (!mImage || !CreateShmSegment()) {
    return false;
  }

#if defined(MOZ_WIDGET_GTK)
  gShmError = 0;
  XErrorHandler previousHandler = XSetErrorHandler(TrapShmError);
  Status attachOk = XShmAttach(aDisplay, &mInfo);
  XSync(aDisplay, False);
  XSetErrorHandler(previousHandler);
  if (gShmError) {
    attachOk = 0;
  }
#elif defined(MOZ_WIDGET_QT)
  Status attachOk = XShmAttach(aDisplay, &mInfo);
#endif

  if (!attachOk) {
    // Assume XShm isn't available, and don't attempt to use it
    // again.
    gShmAvailable = false;
    return false;
  }

  mXAttached = true;
  mSize = aSize;
  mFormat = SurfaceFormat::UNKNOWN;
  switch (mImage->depth) {
  case 32:
    if ((mImage->red_mask == 0xff0000) &&
        (mImage->green_mask == 0xff00) &&
        (mImage->blue_mask == 0xff)) {
      mFormat = SurfaceFormat::B8G8R8A8;
    }
    break;
  case 24:
    // Only support the BGRX layout, and report it as BGRA to the compositor.
    // The alpha channel will be discarded when we put the image.
    if ((mImage->red_mask == 0xff0000) &&
        (mImage->green_mask == 0xff00) &&
        (mImage->blue_mask == 0xff)) {
      mFormat = SurfaceFormat::B8G8R8A8;
    }
    break;
  case 16:
    mFormat = SurfaceFormat::R5G6B5_UINT16;
    break;
  }

  if (mFormat == SurfaceFormat::UNKNOWN) {
    NS_WARNING("Unsupported XShm Image format!");
    gShmAvailable = false;
    return false;
  }

  return true;
}

nsShmImage::~nsShmImage()
{
  if (mImage) {
    mozilla::FinishX(mDisplay);
    if (mXAttached) {
      XShmDetach(mDisplay, &mInfo);
    }
    XDestroyImage(mImage);
  }
  DestroyShmSegment();
}

already_AddRefed<DrawTarget>
nsShmImage::CreateDrawTarget()
{
  return gfxPlatform::GetPlatform()->CreateDrawTargetForData(
    reinterpret_cast<unsigned char*>(mImage->data),
    mSize.ToUnknownSize(),
    mImage->bytes_per_line,
    mFormat);
}

#ifdef MOZ_WIDGET_GTK
void
nsShmImage::Put(Display* aDisplay, Drawable aWindow,
                const LayoutDeviceIntRegion& aRegion)
{
    GC gc = XCreateGC(aDisplay, aWindow, 0, nullptr);
    LayoutDeviceIntRegion bounded;
    bounded.And(aRegion,
                LayoutDeviceIntRect(0, 0, mImage->width, mImage->height));
    for (auto iter = bounded.RectIter(); !iter.Done(); iter.Next()) {
        const LayoutDeviceIntRect& r = iter.Get();
        XShmPutImage(aDisplay, aWindow, gc, mImage,
                     r.x, r.y,
                     r.x, r.y,
                     r.width, r.height,
                     False);
    }

    XFreeGC(aDisplay, gc);

    // FIXME/bug 597336: we need to ensure that the shm image isn't
    // scribbled over before all its pending XShmPutImage()s complete.
    // However, XSync() is an unnecessarily heavyweight
    // synchronization mechanism; other options are possible.  If this
    // XSync is shown to hurt responsiveness, we need to explore the
    // other options.
    XSync(aDisplay, False);
}

#elif defined(MOZ_WIDGET_QT)
void
nsShmImage::Put(QWindow* aWindow, QRect& aRect)
{
    Display* dpy = gfxQtPlatform::GetXDisplay(aWindow);
    Drawable d = aWindow->winId();

    GC gc = XCreateGC(dpy, d, 0, nullptr);
    // Avoid out of bounds painting
    QRect inter = aRect.intersected(aWindow->geometry());
    XShmPutImage(dpy, d, gc, mImage,
                 inter.x(), inter.y(),
                 inter.x(), inter.y(),
                 inter.width(), inter.height(),
                 False);
    XFreeGC(dpy, gc);
}
#endif

already_AddRefed<DrawTarget>
nsShmImage::EnsureShmImage(const LayoutDeviceIntSize& aSize,
                           Display* aDisplay, Visual* aVisual, unsigned int aDepth,
                           RefPtr<nsShmImage>& aImage)
{
  if (!aImage || aImage->Size() != aSize) {
    // Because we XSync() after XShmAttach() to trap errors, we
    // know that the X server has the old image's memory mapped
    // into its address space, so it's OK to destroy the old image
    // here even if there are outstanding Puts.  The Detach is
    // ordered after the Puts.
    aImage = new nsShmImage;
    if (!aImage->CreateImage(aSize, aDisplay, aVisual, aDepth)) {
      aImage = nullptr;
    }
  }
  return !aImage ? nullptr : aImage->CreateDrawTarget();
}

#endif  // MOZ_HAVE_SHMIMAGE
