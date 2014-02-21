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

#include "nsIWidget.h"
#include "gfxTypes.h"
#include "nsAutoPtr.h"

#include "mozilla/X11Util.h"
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/extensions/XShm.h>

#if defined(MOZ_WIDGET_GTK)
#define DISPLAY gdk_x11_get_default_xdisplay
#elif defined(MOZ_WIDGET_QT)
#define DISPLAY mozilla::DefaultXDisplay
#endif

class QRect;
class QWindow;
class gfxASurface;

class nsShmImage {
    NS_INLINE_DECL_REFCOUNTING(nsShmImage)

    typedef mozilla::ipc::SharedMemorySysV SharedMemorySysV;

public:
    typedef gfxImageFormat Format;

    static bool UseShm();
    static already_AddRefed<nsShmImage>
        Create(const gfxIntSize& aSize, Visual* aVisual, unsigned int aDepth);
    static already_AddRefed<gfxASurface>
        EnsureShmImage(const gfxIntSize& aSize, Visual* aVisual, unsigned int aDepth,
                       nsRefPtr<nsShmImage>& aImage);

    ~nsShmImage() {
        if (mImage) {
            mozilla::FinishX(DISPLAY());
            if (mXAttached) {
                XShmDetach(DISPLAY(), &mInfo);
            }
            XDestroyImage(mImage);
        }
    }

    already_AddRefed<gfxASurface> AsSurface();

#if (MOZ_WIDGET_GTK == 2)
    void Put(GdkWindow* aWindow, GdkRectangle* aRects, GdkRectangle* aEnd);
#elif (MOZ_WIDGET_GTK == 3)
    void Put(GdkWindow* aWindow, cairo_rectangle_list_t* aRects);
#elif defined(MOZ_WIDGET_QT)
    void Put(QWindow* aWindow, QRect& aRect);
#endif

    gfxIntSize Size() const { return mSize; }

private:
    nsShmImage()
        : mImage(nullptr)
        , mXAttached(false)
    { mInfo.shmid = SharedMemorySysV::NULLHandle(); }

    nsRefPtr<SharedMemorySysV>   mSegment;
    XImage*                      mImage;
    XShmSegmentInfo              mInfo;
    gfxIntSize                   mSize;
    Format                       mFormat;
    bool                         mXAttached;
};

#endif // MOZ_HAVE_SHMIMAGE

#endif
