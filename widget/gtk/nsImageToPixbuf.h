/* vim:set sw=4 sts=4 et cin: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef NSIMAGETOPIXBUF_H_
#define NSIMAGETOPIXBUF_H_

#include "nsIImageToPixbuf.h"
#include "mozilla/Attributes.h"

namespace mozilla {
namespace gfx {
class SourceSurface;
}
}

class nsImageToPixbuf MOZ_FINAL : public nsIImageToPixbuf {
    typedef mozilla::gfx::SourceSurface SourceSurface;

    public:
        NS_DECL_ISUPPORTS
        NS_IMETHOD_(GdkPixbuf*) ConvertImageToPixbuf(imgIContainer* aImage);

        // Friendlier version of ConvertImageToPixbuf for callers inside of
        // widget
        /**
         * The return value of all these, if not null, should be
         * released as needed by the caller using g_object_unref.
         */
        static GdkPixbuf* ImageToPixbuf(imgIContainer * aImage);
        static GdkPixbuf* SourceSurfaceToPixbuf(SourceSurface* aSurface,
                                                int32_t aWidth,
                                                int32_t aHeight);

    private:
        ~nsImageToPixbuf() {}
};


// fc2389b8-c650-4093-9e42-b05e5f0685b7
#define NS_IMAGE_TO_PIXBUF_CID \
{ 0xfc2389b8, 0xc650, 0x4093, \
  { 0x9e, 0x42, 0xb0, 0x5e, 0x5f, 0x06, 0x85, 0xb7 } }

#endif
