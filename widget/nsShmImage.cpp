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

already_AddRefed<nsShmImage>
nsShmImage::Create(const LayoutDeviceIntSize& aSize,
                   Display* aDisplay, Visual* aVisual, unsigned int aDepth)
{
    RefPtr<nsShmImage> shm = new nsShmImage();
    shm->mDisplay = aDisplay;
    shm->mImage = XShmCreateImage(aDisplay, aVisual, aDepth,
                                  ZPixmap, nullptr,
                                  &(shm->mInfo),
                                  aSize.width, aSize.height);
    if (!shm->mImage) {
        return nullptr;
    }

    size_t size = SharedMemory::PageAlignedSize(
        shm->mImage->bytes_per_line * shm->mImage->height);
    shm->mSegment = new SharedMemorySysV();
    if (!shm->mSegment->Create(size) || !shm->mSegment->Map(size)) {
        return nullptr;
    }

    shm->mInfo.shmid = shm->mSegment->GetHandle();
    shm->mInfo.shmaddr =
        shm->mImage->data = static_cast<char*>(shm->mSegment->memory());
    shm->mInfo.readOnly = False;

#if defined(MOZ_WIDGET_GTK)
    gShmError = 0;
    XErrorHandler previousHandler = XSetErrorHandler(TrapShmError);
    Status attachOk = XShmAttach(aDisplay, &shm->mInfo);
    XSync(aDisplay, False);
    XSetErrorHandler(previousHandler);
    if (gShmError) {
      attachOk = 0;
    }
#elif defined(MOZ_WIDGET_QT)
    Status attachOk = XShmAttach(aDisplay, &shm->mInfo);
#endif

    if (!attachOk) {
        // Assume XShm isn't available, and don't attempt to use it
        // again.
        gShmAvailable = false;
        return nullptr;
    }

    shm->mXAttached = true;
    shm->mSize = aSize;
    switch (shm->mImage->depth) {
    case 32:
        if ((shm->mImage->red_mask == 0xff0000) &&
            (shm->mImage->green_mask == 0xff00) &&
            (shm->mImage->blue_mask == 0xff)) {
            shm->mFormat = SurfaceFormat::B8G8R8A8;
            break;
        }
        goto unsupported;
    case 24:
        // Only xRGB is supported.
        if ((shm->mImage->red_mask == 0xff0000) &&
            (shm->mImage->green_mask == 0xff00) &&
            (shm->mImage->blue_mask == 0xff)) {
            shm->mFormat = SurfaceFormat::B8G8R8X8;
            break;
        }
        goto unsupported;
    case 16:
        shm->mFormat = SurfaceFormat::R5G6B5_UINT16;
        break;
    unsupported:
    default:
        NS_WARNING("Unsupported XShm Image format!");
        gShmAvailable = false;
        return nullptr;
    }
    return shm.forget();
}

already_AddRefed<DrawTarget>
nsShmImage::CreateDrawTarget()
{
  return gfxPlatform::GetPlatform()->CreateDrawTargetForData(
    static_cast<unsigned char*>(mSegment->memory()),
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
    LayoutDeviceIntRegion::OldRectIterator iter(bounded);
    for (const LayoutDeviceIntRect *r = iter.Next(); r; r = iter.Next()) {
        XShmPutImage(aDisplay, aWindow, gc, mImage,
                     r->x, r->y,
                     r->x, r->y,
                     r->width, r->height,
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
        aImage = nsShmImage::Create(aSize, aDisplay, aVisual, aDepth);
    }
    return !aImage ? nullptr : aImage->CreateDrawTarget();
}

#endif  // defined(MOZ_X11) && defined(MOZ_HAVE_SHAREDMEMORYSYSV)
