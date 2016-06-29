/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef RectTextureImage_h_
#define RectTextureImage_h_

#include "mozilla/RefPtr.h"

class MacIOSurface;

namespace mozilla {

namespace gl {
class GLContext;
} // namespace gl

namespace widget {

// Manages a texture which can resize dynamically, binds to the
// LOCAL_GL_TEXTURE_RECTANGLE_ARB texture target and is automatically backed
// by a power-of-two size GL texture. The latter two features are used for
// compatibility with older Mac hardware which we block GL layers on.
// RectTextureImages are used both for accelerated GL layers drawing and for
// OMTC BasicLayers drawing.
class RectTextureImage {
public:
  RectTextureImage();

  virtual ~RectTextureImage();

  already_AddRefed<gfx::DrawTarget>
    BeginUpdate(const LayoutDeviceIntSize& aNewSize,
                const LayoutDeviceIntRegion& aDirtyRegion =
                  LayoutDeviceIntRegion());
  void EndUpdate();

  void UpdateIfNeeded(const LayoutDeviceIntSize& aNewSize,
                      const LayoutDeviceIntRegion& aDirtyRegion,
                      void (^aCallback)(gfx::DrawTarget*,
                                        const LayoutDeviceIntRegion&))
  {
    RefPtr<gfx::DrawTarget> drawTarget = BeginUpdate(aNewSize, aDirtyRegion);
    if (drawTarget) {
      aCallback(drawTarget, GetUpdateRegion());
      EndUpdate();
    }
  }

  void UpdateFromCGContext(const LayoutDeviceIntSize& aNewSize,
                           const LayoutDeviceIntRegion& aDirtyRegion,
                           CGContextRef aCGContext);

  LayoutDeviceIntRegion GetUpdateRegion() {
    MOZ_ASSERT(mInUpdate, "update region only valid during update");
    return mUpdateRegion;
  }

  void Draw(mozilla::layers::GLManager* aManager,
            const LayoutDeviceIntPoint& aLocation,
            const gfx::Matrix4x4& aTransform = gfx::Matrix4x4());


protected:
  void DeleteTexture();
  void BindIOSurfaceToTexture(gl::GLContext* aGL);

  RefPtr<MacIOSurface> mIOSurface;
  gl::GLContext* mGLContext;
  LayoutDeviceIntRegion mUpdateRegion;
  LayoutDeviceIntSize mBufferSize;
  GLuint mTexture;
  bool mInUpdate;
};

} // namespace widget
} // namespace mozilla

#endif // RectTextureImage_h_
