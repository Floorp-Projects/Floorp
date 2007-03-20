/* vim:set sw=4 sts=4 et cin: */
/* ***** BEGIN LICENSE BLOCK *****
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
 * The Original Code is mozilla.org widget code.
 *
 * The Initial Developer of the Original Code is
 * Christian Biesinger <cbiesinger@web.de>.
 * Portions created by the Initial Developer are Copyright (C) 2006
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
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

#include <gdk-pixbuf/gdk-pixbuf.h>

#ifdef MOZ_CAIRO_GFX
#include "gfxASurface.h"
#include "gfxImageSurface.h"
#include "gfxContext.h"
#endif

#include "nsIGdkPixbufImage.h"

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
nsImageToPixbuf::ConvertImageToPixbuf(nsIImage* aImage)
{
    return ImageToPixbuf(aImage);
}

GdkPixbuf*
nsImageToPixbuf::ImageToPixbuf(nsIImage* aImage)
{
#ifdef MOZ_CAIRO_GFX
    PRInt32 width = aImage->GetWidth(),
            height = aImage->GetHeight();

    nsRefPtr<gfxASurface> surface;
    aImage->GetSurface(getter_AddRefs(surface));

    return SurfaceToPixbuf(surface, width, height);
#else
    nsCOMPtr<nsIGdkPixbufImage> img(do_QueryInterface(aImage));
    if (img)
        return img->GetGdkPixbuf();
    return NULL;
#endif
}

#ifdef MOZ_CAIRO_GFX
GdkPixbuf*
nsImageToPixbuf::SurfaceToPixbuf(gfxASurface* aSurface, PRInt32 aWidth, PRInt32 aHeight)
{
    nsRefPtr<gfxImageSurface> imgSurface;
    if (aSurface->GetType() == gfxASurface::SurfaceTypeImage) {
        imgSurface = NS_STATIC_CAST(gfxImageSurface*,
                                    NS_STATIC_CAST(gfxASurface*, aSurface));
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

    GdkPixbuf* pixbuf = gdk_pixbuf_new(GDK_COLORSPACE_RGB, PR_TRUE, 8,
                                       aWidth, aHeight);
    if (!pixbuf)
        return nsnull;

    PRUint32 rowstride = gdk_pixbuf_get_rowstride (pixbuf);
    guchar* pixels = gdk_pixbuf_get_pixels (pixbuf);

    long cairoStride = imgSurface->Stride();
    unsigned char* cairoData = imgSurface->Data();

    gfxASurface::gfxImageFormat format = imgSurface->Format();

    for (PRInt32 row = 0; row < aHeight; ++row) {
        for (PRInt32 col = 0; col < aWidth; ++col) {
            guchar* pixel = pixels + row * rowstride + 4 * col;

            PRUint32* cairoPixel = NS_REINTERPRET_CAST(PRUint32*,
                                   (cairoData + row * cairoStride + 4 * col));

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
#endif
