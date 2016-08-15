/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsShmImage.h"

#ifdef MOZ_HAVE_SHMIMAGE
#include "mozilla/X11Util.h"
#include "mozilla/ipc/SharedMemory.h"
#include "gfxPlatform.h"
#include "nsPrintfCString.h"
#include "nsTArray.h"

#include <errno.h>
#include <string.h>
#include <sys/ipc.h>
#include <sys/shm.h>

using namespace mozilla::ipc;
using namespace mozilla::gfx;

nsShmImage::nsShmImage(Display* aDisplay,
                       Drawable aWindow,
                       Visual* aVisual,
                       unsigned int aDepth)
  : mWindow(aWindow)
  , mVisual(aVisual)
  , mDepth(aDepth)
  , mFormat(mozilla::gfx::SurfaceFormat::UNKNOWN)
  , mSize(0, 0)
  , mPixmap(XCB_NONE)
  , mGC(XCB_NONE)
  , mRequestPending(false)
  , mShmSeg(XCB_NONE)
  , mShmId(-1)
  , mShmAddr(nullptr)
{
  mConnection = XGetXCBConnection(aDisplay);
  mozilla::PodZero(&mPutRequest);
  mozilla::PodZero(&mSyncRequest);
}

nsShmImage::~nsShmImage()
{
  DestroyImage();
}

// If XShm isn't available to our client, we'll try XShm once, fail,
// set this to false and then never try again.
static bool gShmAvailable = true;
bool nsShmImage::UseShm()
{
  return gShmAvailable;
}

bool
nsShmImage::CreateShmSegment()
{
  size_t size = SharedMemory::PageAlignedSize(BytesPerPixel(mFormat) *
                                              mSize.width * mSize.height);

  mShmId = shmget(IPC_PRIVATE, size, IPC_CREAT | 0600);
  if (mShmId == -1) {
    return false;
  }
  mShmAddr = (uint8_t*) shmat(mShmId, nullptr, 0);
  mShmSeg = xcb_generate_id(mConnection);

  // Mark the handle removed so that it will destroy the segment when unmapped.
  shmctl(mShmId, IPC_RMID, nullptr);

  if (mShmAddr == (void *)-1) {
    // Since mapping failed, the segment is already destroyed.
    mShmId = -1;

    nsPrintfCString warning("shmat(): %s (%d)\n", strerror(errno), errno);
    NS_WARNING(warning.get());
    return false;
  }

#ifdef DEBUG
  struct shmid_ds info;
  if (shmctl(mShmId, IPC_STAT, &info) < 0) {
    return false;
  }

  MOZ_ASSERT(size <= info.shm_segsz,
             "Segment doesn't have enough space!");
#endif

  return true;
}

void
nsShmImage::DestroyShmSegment()
{
  if (mShmId != -1) {
    shmdt(mShmAddr);
    mShmId = -1;
  }
}

static bool gShmInitialized = false;
static bool gUseShmPixmaps = false;

bool
nsShmImage::InitExtension()
{
  if (gShmInitialized) {
    return gShmAvailable;
  }

  gShmInitialized = true;

  const xcb_query_extension_reply_t* extReply;
  extReply = xcb_get_extension_data(mConnection, &xcb_shm_id);
  if (!extReply || !extReply->present) {
    gShmAvailable = false;
    return false;
  }

  xcb_shm_query_version_reply_t* shmReply = xcb_shm_query_version_reply(
      mConnection,
      xcb_shm_query_version(mConnection),
      nullptr);

  if (!shmReply) {
    gShmAvailable = false;
    return false;
  }

  gUseShmPixmaps = shmReply->shared_pixmaps &&
                   shmReply->pixmap_format == XCB_IMAGE_FORMAT_Z_PIXMAP;

  free(shmReply);

  return true;
}

bool
nsShmImage::CreateImage(const IntSize& aSize)
{
  MOZ_ASSERT(mConnection && mVisual);

  if (!InitExtension()) {
    return false;
  }

  mSize = aSize;

  BackendType backend = gfxPlatform::GetPlatform()->GetDefaultContentBackend();

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
    // Cairo/pixman lacks some fast paths for compositing BGRX onto BGRA, so
    // just report it as BGRX directly in that case.
    if (mVisual->red_mask == 0xff0000 &&
        mVisual->green_mask == 0xff00 &&
        mVisual->blue_mask == 0xff) {
      mFormat = backend == BackendType::CAIRO ? SurfaceFormat::B8G8R8X8 : SurfaceFormat::B8G8R8A8;
    }
    break;
  case 16:
    if (mVisual->red_mask == 0xf800 &&
        mVisual->green_mask == 0x07e0 &&
        mVisual->blue_mask == 0x1f) {
      mFormat = SurfaceFormat::R5G6B5_UINT16;
    }
    break;
  }

  if (mFormat == SurfaceFormat::UNKNOWN) {
    NS_WARNING("Unsupported XShm Image format!");
    gShmAvailable = false;
    return false;
  }

  if (!CreateShmSegment()) {
    DestroyImage();
    return false;
  }

  xcb_generic_error_t* error;
  xcb_void_cookie_t cookie;

  cookie = xcb_shm_attach_checked(mConnection, mShmSeg, mShmId, 0);

  if ((error = xcb_request_check(mConnection, cookie))) {
    NS_WARNING("Failed to attach MIT-SHM segment.");
    DestroyImage();
    gShmAvailable = false;
    free(error);
    return false;
  }

  if (gUseShmPixmaps) {
    mPixmap = xcb_generate_id(mConnection);
    cookie = xcb_shm_create_pixmap_checked(mConnection, mPixmap, mWindow,
                                           aSize.width, aSize.height, mDepth,
                                           mShmSeg, 0);

    if ((error = xcb_request_check(mConnection, cookie))) {
      // Disable shared pixmaps permanently if creation failed.
      mPixmap = XCB_NONE;
      gUseShmPixmaps = false;
      free(error);
    }
  }

  return true;
}

void
nsShmImage::DestroyImage()
{
  if (mGC) {
    xcb_free_gc(mConnection, mGC);
    mGC = XCB_NONE;
  }
  if (mPixmap != XCB_NONE) {
    xcb_free_pixmap(mConnection, mPixmap);
    mPixmap = XCB_NONE;
  }
  if (mShmSeg != XCB_NONE) {
    xcb_shm_detach_checked(mConnection, mShmSeg);
    mShmSeg = XCB_NONE;
  }
  DestroyShmSegment();
}

already_AddRefed<DrawTarget>
nsShmImage::CreateDrawTarget(const mozilla::LayoutDeviceIntRegion& aRegion)
{
  // Wait for any in-flight requests to complete.
  // Typically X clients would wait for a XShmCompletionEvent to be received,
  // but this works as it's sent immediately after the request is processed.
  if (mRequestPending) {
    xcb_get_input_focus_reply_t* reply;
    if ((reply = xcb_get_input_focus_reply(mConnection, mSyncRequest, nullptr))) {
      free(reply);
    }
    mRequestPending = false;

    xcb_generic_error_t* error;
    if ((error = xcb_request_check(mConnection, mPutRequest))) {
      gShmAvailable = false;
      free(error);
      return nullptr;
    }
  }

  // Due to bug 1205045, we must avoid making GTK calls off the main thread to query window size.
  // Instead we just track the largest offset within the image we are drawing to and grow the image
  // to accomodate it. Since usually the entire window is invalidated on the first paint to it,
  // this should grow the image to the necessary size quickly without many intermediate reallocations.
  IntRect bounds = aRegion.GetBounds().ToUnknownRect();
  IntSize size(bounds.XMost(), bounds.YMost());
  if (size.width > mSize.width || size.height > mSize.height) {
    DestroyImage();
    if (!CreateImage(size)) {
      return nullptr;
    }
  }

  return gfxPlatform::CreateDrawTargetForData(
    reinterpret_cast<unsigned char*>(mShmAddr)
      + BytesPerPixel(mFormat) * (bounds.y * mSize.width + bounds.x),
    bounds.Size(),
    BytesPerPixel(mFormat) * mSize.width,
    mFormat);
}

void
nsShmImage::Put(const mozilla::LayoutDeviceIntRegion& aRegion)
{
  AutoTArray<xcb_rectangle_t, 32> xrects;
  xrects.SetCapacity(aRegion.GetNumRects());

  for (auto iter = aRegion.RectIter(); !iter.Done(); iter.Next()) {
    const mozilla::LayoutDeviceIntRect &r = iter.Get();
    xcb_rectangle_t xrect = { (short)r.x, (short)r.y, (unsigned short)r.width, (unsigned short)r.height };
    xrects.AppendElement(xrect);
  }

  if (!mGC) {
    mGC = xcb_generate_id(mConnection);
    xcb_create_gc(mConnection, mGC, mWindow, 0, nullptr);
  }

  xcb_set_clip_rectangles(mConnection, XCB_CLIP_ORDERING_YX_BANDED, mGC, 0, 0,
                          xrects.Length(), xrects.Elements());

  if (mPixmap != XCB_NONE) {
    mPutRequest = xcb_copy_area_checked(mConnection, mPixmap, mWindow, mGC,
                                        0, 0, 0, 0, mSize.width, mSize.height);
  } else {
    mPutRequest = xcb_shm_put_image_checked(mConnection, mWindow, mGC,
                                            mSize.width, mSize.height,
                                            0, 0, mSize.width, mSize.height,
                                            0, 0, mDepth,
                                            XCB_IMAGE_FORMAT_Z_PIXMAP, 0,
                                            mShmSeg, 0);
  }

  // Send a request that returns a response so that we don't have to start a
  // sync in nsShmImage::CreateDrawTarget to retrieve the result of mPutRequest.
  mSyncRequest = xcb_get_input_focus(mConnection);
  mRequestPending = true;

  xcb_flush(mConnection);
}

#endif  // MOZ_HAVE_SHMIMAGE
