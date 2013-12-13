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
    nsRefPtr<gfxASurface> surface =
      aImage->GetFrame(imgIContainer::FRAME_CURRENT,
                       imgIContainer::FLAG_SYNC_DECODE);

    // If the last call failed, it was probably because our call stack originates
    // in an imgINotificationObserver event, meaning that we're not allowed request
    // a sync decode. Presumably the originating event is something sensible like
    // OnStopFrame(), so we can just retry the call without a sync decode.
    if (!surface)
        surface = aImage->GetFrame(imgIContainer::FRAME_CURRENT,
                                   imgIContainer::FLAG_NONE);

    NS_ENSURE_TRUE(surface, nullptr);

    nsRefPtr<gfxImageSurface> frame(surface->GetAsReadableARGB32ImageSurface());
    NS_ENSURE_TRUE(frame, nullptr);

    return ImgSurfaceToPixbuf(frame, frame->Width(), frame->Height());
}

GdkPixbuf*
nsImageToPixbuf::ImgSurfaceToPixbuf(gfxImageSurface* aImgSurface, int32_t aWidth, int32_t aHeight)
{
    GdkPixbuf* pixbuf = gdk_pixbuf_new(GDK_COLORSPACE_RGB, TRUE, 8,
                                       aWidth, aHeight);
    if (!pixbuf)
        return nullptr;

    uint32_t rowstride = gdk_pixbuf_get_rowstride (pixbuf);
    guchar* pixels = gdk_pixbuf_get_pixels (pixbuf);

    long cairoStride = aImgSurface->Stride();
    unsigned char* cairoData = aImgSurface->Data();

    gfxImageFormat format = aImgSurface->Format();

    for (int32_t row = 0; row < aHeight; ++row) {
        for (int32_t col = 0; col < aWidth; ++col) {
            guchar* pixel = pixels + row * rowstride + 4 * col;

            uint32_t* cairoPixel = reinterpret_cast<uint32_t*>
                                                   ((cairoData + row * cairoStride + 4 * col));

            if (format == gfxImageFormatARGB32) {
                const uint8_t a = (*cairoPixel >> 24) & 0xFF;
                const uint8_t r = unpremultiply((*cairoPixel >> 16) & 0xFF, a);
                const uint8_t g = unpremultiply((*cairoPixel >>  8) & 0xFF, a);
                const uint8_t b = unpremultiply((*cairoPixel >>  0) & 0xFF, a);

                *pixel++ = r;
                *pixel++ = g;
                *pixel++ = b;
                *pixel++ = a;
            } else {
                NS_ASSERTION(format == gfxImageFormatRGB24,
                             "unexpected format");
                const uint8_t r = (*cairoPixel >> 16) & 0xFF;
                const uint8_t g = (*cairoPixel >>  8) & 0xFF;
                const uint8_t b = (*cairoPixel >>  0) & 0xFF;

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
nsImageToPixbuf::SurfaceToPixbuf(gfxASurface* aSurface, int32_t aWidth, int32_t aHeight)
{
    if (aSurface->CairoStatus()) {
        NS_ERROR("invalid surface");
        return nullptr;
    }

    nsRefPtr<gfxImageSurface> imgSurface;
    if (aSurface->GetType() == gfxSurfaceTypeImage) {
        imgSurface = static_cast<gfxImageSurface*>
                                (static_cast<gfxASurface*>(aSurface));
    } else {
        imgSurface = new gfxImageSurface(gfxIntSize(aWidth, aHeight),
					 gfxImageFormatARGB32);
                                       
        if (!imgSurface)
            return nullptr;

        nsRefPtr<gfxContext> context = new gfxContext(imgSurface);
        if (!context)
            return nullptr;

        context->SetOperator(gfxContext::OPERATOR_SOURCE);
        context->SetSource(aSurface);
        context->Paint();
    }

    return ImgSurfaceToPixbuf(imgSurface, aWidth, aHeight);
}
