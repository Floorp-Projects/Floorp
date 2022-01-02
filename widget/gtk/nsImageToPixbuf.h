/* vim:set sw=2 sts=2 et cin: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef NSIMAGETOPIXBUF_H_
#define NSIMAGETOPIXBUF_H_

#include "mozilla/Attributes.h"
#include "mozilla/Maybe.h"
#include "nsSize.h"

class imgIContainer;
typedef struct _GdkPixbuf GdkPixbuf;

namespace mozilla::gfx {
class SourceSurface;
}  // namespace mozilla::gfx

class nsImageToPixbuf final {
  using SourceSurface = mozilla::gfx::SourceSurface;

 public:
  // Friendlier version of ConvertImageToPixbuf for callers inside of
  // widget
  /**
   * The return value of all these, if not null, should be
   * released as needed by the caller using g_object_unref.
   */
  static GdkPixbuf* ImageToPixbuf(
      imgIContainer* aImage,
      const mozilla::Maybe<nsIntSize>& aOverrideSize = mozilla::Nothing());
  static GdkPixbuf* SourceSurfaceToPixbuf(SourceSurface* aSurface,
                                          int32_t aWidth, int32_t aHeight);

 private:
  ~nsImageToPixbuf() = default;
};

#endif
