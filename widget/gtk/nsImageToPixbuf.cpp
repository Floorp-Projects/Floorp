/* vim:set sw=4 sts=4 et cin: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include <gdk-pixbuf/gdk-pixbuf.h>

#include "gfxASurface.h"
#include "gfxImageSurface.h"
#include "gfxContext.h"
#include "gfxPlatform.h"
#include "mozilla/gfx/2D.h"
#include "mozilla/RefPtr.h"

#include "imgIContainer.h"

#include "nsAutoPtr.h"

#include "nsImageToPixbuf.h"

using mozilla::gfx::DataSourceSurface;
using mozilla::gfx::SurfaceFormat;
using mozilla::RefPtr;

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
    nsRefPtr<gfxASurface> thebesSurface =
      aImage->GetFrame(imgIContainer::FRAME_CURRENT,
                       imgIContainer::FLAG_SYNC_DECODE);

    // If the last call failed, it was probably because our call stack originates
    // in an imgINotificationObserver event, meaning that we're not allowed request
    // a sync decode. Presumably the originating event is something sensible like
    // OnStopFrame(), so we can just retry the call without a sync decode.
    if (!thebesSurface)
      thebesSurface = aImage->GetFrame(imgIContainer::FRAME_CURRENT,
                                       imgIContainer::FLAG_NONE);

    NS_ENSURE_TRUE(thebesSurface, nullptr);

    RefPtr<SourceSurface> surface =
      gfxPlatform::GetPlatform()->GetSourceSurfaceForSurface(nullptr,
                                                             thebesSurface);
    NS_ENSURE_TRUE(surface, nullptr);

    return SourceSurfaceToPixbuf(surface,
                                 surface->GetSize().width,
                                 surface->GetSize().height);
}

GdkPixbuf*
nsImageToPixbuf::SourceSurfaceToPixbuf(SourceSurface* aSurface,
                                       int32_t aWidth,
                                       int32_t aHeight)
{
    MOZ_ASSERT(aWidth <= aSurface->GetSize().width &&
               aHeight <= aSurface->GetSize().height,
               "Requested rect is bigger than the supplied surface");

    GdkPixbuf* pixbuf = gdk_pixbuf_new(GDK_COLORSPACE_RGB, TRUE, 8,
                                       aWidth, aHeight);
    if (!pixbuf)
        return nullptr;

    uint32_t destStride = gdk_pixbuf_get_rowstride (pixbuf);
    guchar* destPixels = gdk_pixbuf_get_pixels (pixbuf);

    RefPtr<DataSourceSurface> dataSurface = aSurface->GetDataSurface();
    DataSourceSurface::MappedSurface map;
    dataSurface->Map(DataSourceSurface::MapType::READ, &map);
    uint8_t* srcData = map.mData;
    int32_t srcStride = map.mStride;

    SurfaceFormat format = dataSurface->GetFormat();

    for (int32_t row = 0; row < aHeight; ++row) {
        for (int32_t col = 0; col < aWidth; ++col) {
            guchar* destPixel = destPixels + row * destStride + 4 * col;

            uint32_t* srcPixel =
                reinterpret_cast<uint32_t*>((srcData + row * srcStride + 4 * col));

            if (format == SurfaceFormat::B8G8R8A8) {
                const uint8_t a = (*srcPixel >> 24) & 0xFF;
                const uint8_t r = unpremultiply((*srcPixel >> 16) & 0xFF, a);
                const uint8_t g = unpremultiply((*srcPixel >>  8) & 0xFF, a);
                const uint8_t b = unpremultiply((*srcPixel >>  0) & 0xFF, a);

                *destPixel++ = r;
                *destPixel++ = g;
                *destPixel++ = b;
                *destPixel++ = a;
            } else {
                MOZ_ASSERT(format == SurfaceFormat::B8G8R8X8);

                const uint8_t r = (*srcPixel >> 16) & 0xFF;
                const uint8_t g = (*srcPixel >>  8) & 0xFF;
                const uint8_t b = (*srcPixel >>  0) & 0xFF;

                *destPixel++ = r;
                *destPixel++ = g;
                *destPixel++ = b;
                *destPixel++ = 0xFF; // A
            }
        }
    }

    dataSurface->Unmap();

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

    RefPtr<SourceSurface> surface =
        gfxPlatform::GetPlatform()->GetSourceSurfaceForSurface(nullptr,
                                                               imgSurface);

    return SourceSurfaceToPixbuf(surface, aWidth, aHeight);
}
