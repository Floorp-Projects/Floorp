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
#include "nsPrintfCString.h"
#include "nsTArray.h"

#include <errno.h>
#include <string.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <X11/Xlibint.h>

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

static int gShmError = 0;

static int
TrapShmError(Display* aDisplay, XErrorEvent* aEvent)
{
  // store the error code and ignore the error
  gShmError = aEvent->error_code;
  return 0;
}

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

  gShmError = 0;
  XErrorHandler previousHandler = XSetErrorHandler(TrapShmError);
  Status attachOk = XShmAttach(mDisplay, &mInfo);
  XSync(mDisplay, False);
  XSetErrorHandler(previousHandler);
  if (gShmError) {
    attachOk = 0;
  }

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
nsShmImage::CreateDrawTarget(const mozilla::LayoutDeviceIntRegion& aRegion)
{
  // Wait for any in-flight XShmPutImage requests to complete.
  WaitForRequest();

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
nsShmImage::WaitForRequest()
{
  if (!mRequest) {
    return;
  }

  while (long(LastKnownRequestProcessed(mDisplay) - mRequest) < 0) {
    LockDisplay(mDisplay);
    _XReadEvents(mDisplay);
    UnlockDisplay(mDisplay);
  }

  mRequest = 0;
}

void
nsShmImage::Put(const mozilla::LayoutDeviceIntRegion& aRegion)
{
  if (!mImage) {
    return;
  }

  AutoTArray<XRectangle, 32> xrects;
  xrects.SetCapacity(aRegion.GetNumRects());

  for (auto iter = aRegion.RectIter(); !iter.Done(); iter.Next()) {
    const mozilla::LayoutDeviceIntRect &r = iter.Get();
    XRectangle xrect = { (short)r.x, (short)r.y, (unsigned short)r.width, (unsigned short)r.height };
    xrects.AppendElement(xrect);
  }

  GC gc = XCreateGC(mDisplay, mWindow, 0, nullptr);
  XSetClipRectangles(mDisplay, gc, 0, 0, xrects.Elements(), xrects.Length(), YXBanded);

  mRequest = XNextRequest(mDisplay);
  XShmPutImage(mDisplay, mWindow, gc, mImage,
               0, 0,
               0, 0,
               mImage->width, mImage->height,
               True);

  XFreeGC(mDisplay, gc);

  XFlush(mDisplay);
}

#endif  // MOZ_HAVE_SHMIMAGE
