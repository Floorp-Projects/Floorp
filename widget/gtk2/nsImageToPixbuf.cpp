/* vim:set sw=4 sts=4 et cin: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include <gdk-pixbuf/gdk-pixbuf.h>

#include "gfxASurface.h"
#include "gfxImageSurface.h"
#include "gfxContext.h"

#include "imgIContainer.h"

#include "nsAutoPtr.h"

#include "nsImageToPixbuf.h"

NS_IMPL_ISUPPORTS1(nsImageToPixbuf, nsIImageToPixbuf)

inline unsigned char
unpremultiply (unsigned char color,
               unsigned char alpha)
{
    if (alpha == 0)
        return 0;
    // plus alpha/2 to round instead of truncate
    return (color * 255 + alpha / 2) / alpha;
}

NS_IMETHODIMP_(GdkPixbuf*)
nsImageToPixbuf::ConvertImageToPixbuf(imgIContainer* aImage)
{
    return ImageToPixbuf(aImage);
}

GdkPixbuf*
nsImageToPixbuf::ImageToPixbuf(imgIContainer* aImage)
{
    nsRefPtr<gfxImageSurface> frame;
    nsresult rv = aImage->CopyFrame(imgIContainer::FRAME_CURRENT,
                                    imgIContainer::FLAG_SYNC_DECODE,
                                    getter_AddRefs(frame));

    // If the last call failed, it was probably because our call stack originates
    // in an imgIDecoderObserver event, meaning that we're not allowed request
    // a sync decode. Presumably the originating event is something sensible like
    // OnStopFrame(), so we can just retry the call without a sync decode.
    if (NS_FAILED(rv))
        aImage->CopyFrame(imgIContainer::FRAME_CURRENT,
                          imgIContainer::FLAG_NONE,
                          getter_AddRefs(frame));

    if (!frame)
      return nsnull;

    return ImgSurfaceToPixbuf(frame, frame->Width(), frame->Height());
}

GdkPixbuf*
nsImageToPixbuf::ImgSurfaceToPixbuf(gfxImageSurface* aImgSurface, PRInt32 aWidth, PRInt32 aHeight)
{
    GdkPixbuf* pixbuf = gdk_pixbuf_new(GDK_COLORSPACE_RGB, TRUE, 8,
                                       aWidth, aHeight);
    if (!pixbuf)
        return nsnull;

    PRUint32 rowstride = gdk_pixbuf_get_rowstride (pixbuf);
    guchar* pixels = gdk_pixbuf_get_pixels (pixbuf);

    long cairoStride = aImgSurface->Stride();
    unsigned char* cairoData = aImgSurface->Data();

    gfxASurface::gfxImageFormat format = aImgSurface->Format();

    for (PRInt32 row = 0; row < aHeight; ++row) {
        for (PRInt32 col = 0; col < aWidth; ++col) {
            guchar* pixel = pixels + row * rowstride + 4 * col;

            PRUint32* cairoPixel = reinterpret_cast<PRUint32*>
                                                   ((cairoData + row * cairoStride + 4 * col));

            if (format == gfxASurface::ImageFormatARGB32) {
                const PRUint8 a = (*cairoPixel >> 24) & 0xFF;
                const PRUint8 r = unpremultiply((*cairoPixel >> 16) & 0xFF, a);
                const PRUint8 g = unpremultiply((*cairoPixel >>  8) & 0xFF, a);
                const PRUint8 b = unpremultiply((*cairoPixel >>  0) & 0xFF, a);

                *pixel++ = r;
                *pixel++ = g;
                *pixel++ = b;
                *pixel++ = a;
            } else {
                NS_ASSERTION(format == gfxASurface::ImageFormatRGB24,
                             "unexpected format");
                const PRUint8 r = (*cairoPixel >> 16) & 0xFF;
                const PRUint8 g = (*cairoPixel >>  8) & 0xFF;
                const PRUint8 b = (*cairoPixel >>  0) & 0xFF;

                *pixel++ = r;
                *pixel++ = g;
                *pixel++ = b;
                *pixel++ = 0xFF; // A
            }
        }
    }

    return pixbuf;
}

GdkPixbuf*
nsImageToPixbuf::SurfaceToPixbuf(gfxASurface* aSurface, PRInt32 aWidth, PRInt32 aHeight)
{
    if (aSurface->CairoStatus()) {
        NS_ERROR("invalid surface");
        return nsnull;
    }

    nsRefPtr<gfxImageSurface> imgSurface;
    if (aSurface->GetType() == gfxASurface::SurfaceTypeImage) {
        imgSurface = static_cast<gfxImageSurface*>
                                (static_cast<gfxASurface*>(aSurface));
    } else {
        imgSurface = new gfxImageSurface(gfxIntSize(aWidth, aHeight),
					 gfxImageSurface::ImageFormatARGB32);
                                       
        if (!imgSurface)
            return nsnull;

        nsRefPtr<gfxContext> context = new gfxContext(imgSurface);
        if (!context)
            return nsnull;

        context->SetOperator(gfxContext::OPERATOR_SOURCE);
        context->SetSource(aSurface);
        context->Paint();
    }

    return ImgSurfaceToPixbuf(imgSurface, aWidth, aHeight);
}
