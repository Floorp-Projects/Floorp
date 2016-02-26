/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

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

  // Mark the handle removed so that it will destroy the segment when unmapped.
  shmctl(mInfo.shmid, IPC_RMID, nullptr);

  if (mInfo.shmaddr == (void *)-1) {
    // Since mapping failed, the segment is already destroyed.
    mInfo.shmid = -1;

    nsPrintfCString warning("shmat(): %s (%d)\n", strerror(errno), errno);
    NS_WARNING(warning.get());
    return false;
  }

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
nsShmImage::CreateImage(const IntSize& aSize)
{
  MOZ_ASSERT(mDisplay && mVisual);

  mFormat = SurfaceFormat::UNKNOWN;
  switch (mDepth) {
  case 32:
    if (mVisual->red_mask == 0xff0000 &&
        mVisual->green_mask == 0xff00 &&
        mVisual->blue_mask == 0xff) {
      mFormat = SurfaceFormat::B8G8R8A8;
    }
    break;
  case 24:
    // Only support the BGRX layout, and report it as BGRA to the compositor.
    // The alpha channel will be discarded when we put the image.
    if (mVisual->red_mask == 0xff0000 &&
        mVisual->green_mask == 0xff00 &&
        mVisual->blue_mask == 0xff) {
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

  mImage = XShmCreateImage(mDisplay, mVisual, mDepth,
                           ZPixmap, nullptr,
                           &mInfo,
                           aSize.width, aSize.height);
  if (!mImage || !CreateShmSegment()) {
    DestroyImage();
    return false;
  }

#ifdef MOZ_WIDGET_GTK
  gShmError = 0;
  XErrorHandler previousHandler = XSetErrorHandler(TrapShmError);
  Status attachOk = XShmAttach(mDisplay, &mInfo);
  XSync(mDisplay, False);
  XSetErrorHandler(previousHandler);
  if (gShmError) {
    attachOk = 0;
  }
#else
  Status attachOk = XShmAttach(mDisplay, &mInfo);
#endif

  if (!attachOk) {
    DestroyShmSegment();
    DestroyImage();

    // Assume XShm isn't available, and don't attempt to use it
    // again.
    gShmAvailable = false;
    return false;
  }

  return true;
}

void
nsShmImage::DestroyImage()
{
  if (mImage) {
    mozilla::FinishX(mDisplay);
    if (mInfo.shmid != -1) {
      XShmDetach(mDisplay, &mInfo);
    }
    XDestroyImage(mImage);
    mImage = nullptr;
  }
  DestroyShmSegment();
}

already_AddRefed<DrawTarget>
nsShmImage::CreateDrawTarget(const LayoutDeviceIntRegion& aRegion)
{
  // Due to bug 1205045, we must avoid making GTK calls off the main thread to query window size.
  // Instead we just track the largest offset within the image we are drawing to and grow the image
  // to accomodate it. Since usually the entire window is invalidated on the first paint to it,
  // this should grow the image to the necessary size quickly without many intermediate reallocations.
  IntRect bounds = aRegion.GetBounds().ToUnknownRect();
  IntSize size(bounds.XMost(), bounds.YMost());
  if (!mImage || size.width > mImage->width || size.height > mImage->height) {
    DestroyImage();
    if (!CreateImage(size)) {
      return nullptr;
    }
  }

  return gfxPlatform::GetPlatform()->CreateDrawTargetForData(
    reinterpret_cast<unsigned char*>(mImage->data)
      + bounds.y * mImage->bytes_per_line + bounds.x * BytesPerPixel(mFormat),
    bounds.Size(),
    mImage->bytes_per_line,
    mFormat);
}

void
nsShmImage::Put(const LayoutDeviceIntRegion& aRegion)
{
  if (!mImage) {
    return;
  }

  GC gc = XCreateGC(mDisplay, mWindow, 0, nullptr);
  LayoutDeviceIntRegion bounded;
  bounded.And(aRegion,
              LayoutDeviceIntRect(0, 0, mImage->width, mImage->height));
  for (auto iter = bounded.RectIter(); !iter.Done(); iter.Next()) {
    const LayoutDeviceIntRect& r = iter.Get();
    XShmPutImage(mDisplay, mWindow, gc, mImage,
                 r.x, r.y,
                 r.x, r.y,
                 r.width, r.height,
                 False);
  }

  XFreeGC(mDisplay, gc);

  // FIXME/bug 597336: we need to ensure that the shm image isn't
  // scribbled over before all its pending XShmPutImage()s complete.
  // However, XSync() is an unnecessarily heavyweight
  // synchronization mechanism; other options are possible.  If this
  // XSync is shown to hurt responsiveness, we need to explore the
  // other options.
  XSync(mDisplay, False);
}

#endif  // MOZ_HAVE_SHMIMAGE
