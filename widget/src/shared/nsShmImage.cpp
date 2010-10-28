/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is the Mozilla browser.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1999
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Chris Jones <jones.chris.g@gmail.com>
 *   Oleg Romashin <romaxa@gmail.com>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#if defined(MOZ_WIDGET_GTK2)
#include <gtk/gtk.h>
#include <gdk/gdkx.h>
#elif defined(MOZ_WIDGET_QT)
#include <QWidget>
#endif

#include "nsShmImage.h"
#include "gfxPlatform.h"
#include "gfxImageSurface.h"

#ifdef MOZ_HAVE_SHMIMAGE

// If XShm isn't available to our client, we'll try XShm once, fail,
// set this to false and then never try again.
static PRBool gShmAvailable = PR_TRUE;
PRBool nsShmImage::UseShm()
{
    return gfxPlatform::GetPlatform()->
        ScreenReferenceSurface()->GetType() == gfxASurface::SurfaceTypeImage
        && gShmAvailable;
}

already_AddRefed<nsShmImage>
nsShmImage::Create(const gfxIntSize& aSize,
                   Visual* aVisual, unsigned int aDepth)
{
    Display* dpy = DISPLAY();

    nsRefPtr<nsShmImage> shm = new nsShmImage();
    shm->mImage = XShmCreateImage(dpy, aVisual, aDepth,
                                  ZPixmap, nsnull,
                                  &(shm->mInfo),
                                  aSize.width, aSize.height);
    if (!shm->mImage) {
        return nsnull;
    }

    size_t size = shm->mImage->bytes_per_line * shm->mImage->height;
    shm->mSegment = new SharedMemorySysV();
    if (!shm->mSegment->Create(size) || !shm->mSegment->Map(size)) {
        return nsnull;
    }

    shm->mInfo.shmid = shm->mSegment->GetHandle();
    shm->mInfo.shmaddr =
        shm->mImage->data = static_cast<char*>(shm->mSegment->memory());
    shm->mInfo.readOnly = False;

    int xerror = 0;
#if defined(MOZ_WIDGET_GTK2)
    gdk_error_trap_push();
    Status attachOk = XShmAttach(dpy, &shm->mInfo);
    xerror = gdk_error_trap_pop();
#elif defined(MOZ_WIDGET_QT)
    Status attachOk = XShmAttach(dpy, &shm->mInfo);
#endif

    if (!attachOk || xerror) {
        // Assume XShm isn't available, and don't attempt to use it
        // again.
        gShmAvailable = PR_FALSE;
        return nsnull;
    }

    shm->mXAttached = PR_TRUE;
    shm->mSize = aSize;
    switch (shm->mImage->depth) {
    case 24:
        shm->mFormat = gfxASurface::ImageFormatRGB24; break;
    case 16:
        shm->mFormat = gfxASurface::ImageFormatRGB16_565; break;
    default:
        NS_WARNING("Unsupported XShm Image depth!");
        gShmAvailable = PR_FALSE;
        return nsnull;
    }
    return shm.forget();
}

already_AddRefed<gfxASurface>
nsShmImage::AsSurface()
{
    return nsRefPtr<gfxASurface>(
        new gfxImageSurface(static_cast<unsigned char*>(mSegment->memory()),
                            mSize,
                            mImage->bytes_per_line,
                            mFormat)
        ).forget();
}

#if defined(MOZ_WIDGET_GTK2)
void
nsShmImage::Put(GdkWindow* aWindow, GdkRectangle* aRects, GdkRectangle* aEnd)
{
    GdkDrawable* gd;
    gint dx, dy;
    gdk_window_get_internal_paint_info(aWindow, &gd, &dx, &dy);

    Display* dpy = gdk_x11_get_default_xdisplay();
    Drawable d = GDK_DRAWABLE_XID(gd);

    GC gc = XCreateGC(dpy, d, 0, nsnull);
    for (GdkRectangle* r = aRects; r < aEnd; r++) {
        XShmPutImage(dpy, d, gc, mImage,
                     r->x, r->y,
                     r->x - dx, r->y - dy,
                     r->width, r->height,
                     False);
    }
    XFreeGC(dpy, gc);

#ifdef MOZ_WIDGET_GTK2
    // FIXME/bug 597336: we need to ensure that the shm image isn't
    // scribbled over before all its pending XShmPutImage()s complete.
    // However, XSync() is an unnecessarily heavyweight
    // synchronization mechanism; other options are possible.  If this
    // XSync is shown to hurt responsiveness, we need to explore the
    // other options.
    XSync(dpy, False);
#endif
}
#elif defined(MOZ_WIDGET_QT)
void
nsShmImage::Put(QWidget* aWindow, QRect& aRect)
{
    Display* dpy = aWindow->x11Info().display();
    Drawable d = aWindow->handle();

    GC gc = XCreateGC(dpy, d, 0, nsnull);
    // Avoid out of bounds painting
    QRect inter = aRect.intersected(aWindow->rect());
    XShmPutImage(dpy, d, gc, mImage,
                 inter.x(), inter.y(),
                 inter.x(), inter.y(),
                 inter.width(), inter.height(),
                 False);
    XFreeGC(dpy, gc);
}
#endif

already_AddRefed<gfxASurface>
nsShmImage::EnsureShmImage(const gfxIntSize& aSize, Visual* aVisual, unsigned int aDepth,
               nsRefPtr<nsShmImage>& aImage)
{
    if (!aImage || aImage->Size() != aSize) {
        // Because we XSync() after XShmAttach() to trap errors, we
        // know that the X server has the old image's memory mapped
        // into its address space, so it's OK to destroy the old image
        // here even if there are outstanding Puts.  The Detach is
        // ordered after the Puts.
        aImage = nsShmImage::Create(aSize, aVisual, aDepth);
    }
    return !aImage ? nsnull : aImage->AsSurface();
}

#endif  // defined(MOZ_X11) && defined(MOZ_HAVE_SHAREDMEMORYSYSV)
