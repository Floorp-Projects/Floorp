/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef __mozilla_widget_nsShmImage_h__
#define __mozilla_widget_nsShmImage_h__

#include "mozilla/ipc/SharedMemorySysV.h"

#if defined(MOZ_X11) && defined(MOZ_HAVE_SHAREDMEMORYSYSV)
#  define MOZ_HAVE_SHMIMAGE
#endif

#ifdef MOZ_HAVE_SHMIMAGE

#include "mozilla/gfx/2D.h"
#include "nsIWidget.h"
#include "nsAutoPtr.h"

#include "mozilla/X11Util.h"
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/extensions/XShm.h>

#ifdef MOZ_WIDGET_QT
class QRect;
class QWindow;
#endif

class nsShmImage {
    // bug 1168843, compositor thread may create shared memory instances that are destroyed by main thread on shutdown, so this must use thread-safe RC to avoid hitting assertion
    NS_INLINE_DECL_THREADSAFE_REFCOUNTING(nsShmImage)

    typedef mozilla::ipc::SharedMemorySysV SharedMemorySysV;

public:
    static bool UseShm();
    static already_AddRefed<nsShmImage>
        Create(const gfxIntSize& aSize,
               Display* aDisplay, Visual* aVisual, unsigned int aDepth);
    static already_AddRefed<mozilla::gfx::DrawTarget>
        EnsureShmImage(const gfxIntSize& aSize,
                       Display* aDisplay, Visual* aVisual, unsigned int aDepth,
                       nsRefPtr<nsShmImage>& aImage);

private:
    ~nsShmImage() {
        if (mImage) {
            mozilla::FinishX(mDisplay);
            if (mXAttached) {
                XShmDetach(mDisplay, &mInfo);
            }
            XDestroyImage(mImage);
        }
    }

public:
    already_AddRefed<mozilla::gfx::DrawTarget> CreateDrawTarget();

#ifdef MOZ_WIDGET_GTK
    void Put(Display* aDisplay, Drawable aWindow, const nsIntRegion& aRegion);
#elif defined(MOZ_WIDGET_QT)
    void Put(QWindow* aWindow, QRect& aRect);
#endif

    gfxIntSize Size() const { return mSize; }

private:
    nsShmImage()
        : mImage(nullptr)
        , mDisplay(nullptr)
        , mFormat(mozilla::gfx::SurfaceFormat::UNKNOWN)
        , mXAttached(false)
    { mInfo.shmid = SharedMemorySysV::NULLHandle(); }

    nsRefPtr<SharedMemorySysV>   mSegment;
    XImage*                      mImage;
    Display*                     mDisplay;
    XShmSegmentInfo              mInfo;
    gfxIntSize                   mSize;
    mozilla::gfx::SurfaceFormat  mFormat;
    bool                         mXAttached;
};

#endif // MOZ_HAVE_SHMIMAGE

#endif
