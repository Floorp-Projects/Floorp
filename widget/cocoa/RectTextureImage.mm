/* -*- Mode: objc; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "RectTextureImage.h"

#include "gfxUtils.h"
#include "GLContextCGL.h"
#include "GLUploadHelpers.h"
#include "mozilla/layers/GLManager.h"
#include "mozilla/gfx/MacIOSurface.h"
#include "OGLShaderProgram.h"
#include "ScopedGLHelpers.h"

namespace mozilla {
namespace widget {

RectTextureImage::RectTextureImage(gl::GLContext* aGLContext)
  : mGLContext(aGLContext)
  , mTexture(0)
  , mInUpdate(false)
{
}

RectTextureImage::~RectTextureImage()
{
  if (mTexture) {
    mGLContext->MakeCurrent();
    mGLContext->fDeleteTextures(1, &mTexture);
    mTexture = 0;
  }
}

LayoutDeviceIntSize
RectTextureImage::TextureSizeForSize(const LayoutDeviceIntSize& aSize)
{
  return LayoutDeviceIntSize(gfx::NextPowerOfTwo(aSize.width),
                             gfx::NextPowerOfTwo(aSize.height));
}

already_AddRefed<gfx::DrawTarget>
RectTextureImage::BeginUpdate(const LayoutDeviceIntSize& aNewSize,
                              const LayoutDeviceIntRegion& aDirtyRegion)
{
  MOZ_ASSERT(!mInUpdate, "Beginning update during update!");
  mUpdateRegion = aDirtyRegion;
  if (aNewSize != mUsedSize) {
    mUsedSize = aNewSize;
    mUpdateRegion =
      LayoutDeviceIntRect(LayoutDeviceIntPoint(0, 0), aNewSize);
  }

  if (mUpdateRegion.IsEmpty()) {
    return nullptr;
  }

  LayoutDeviceIntSize neededBufferSize = TextureSizeForSize(mUsedSize);
  if (!mUpdateDrawTarget || mBufferSize != neededBufferSize) {
    gfx::IntSize size(neededBufferSize.width, neededBufferSize.height);
    mUpdateDrawTarget = nullptr;
    mUpdateDrawTargetData = nullptr;
    gfx::SurfaceFormat format = gfx::SurfaceFormat::B8G8R8A8;
    int32_t stride = size.width * gfx::BytesPerPixel(format);
    mUpdateDrawTargetData = MakeUnique<unsigned char[]>(stride * size.height);
    mUpdateDrawTarget =
      gfx::Factory::CreateDrawTargetForData(gfx::BackendType::SKIA,
                                            mUpdateDrawTargetData.get(), size,
                                            stride, format);
    mBufferSize = neededBufferSize;
  }

  mInUpdate = true;

  RefPtr<gfx::DrawTarget> drawTarget = mUpdateDrawTarget;
  return drawTarget.forget();
}

#define NSFoundationVersionWithProperStrideSupportForSubtextureUpload NSFoundationVersionNumber10_6_3

static bool
CanUploadSubtextures()
{
  return NSFoundationVersionNumber >= NSFoundationVersionWithProperStrideSupportForSubtextureUpload;
}

void
RectTextureImage::EndUpdate(bool aKeepSurface)
{
  MOZ_ASSERT(mInUpdate, "Ending update while not in update");

  bool needInit = !mTexture;
  LayoutDeviceIntRegion updateRegion = mUpdateRegion;
  if (mTextureSize != mBufferSize) {
    mTextureSize = mBufferSize;
    needInit = true;
  }

  if (needInit || !CanUploadSubtextures()) {
    updateRegion =
      LayoutDeviceIntRect(LayoutDeviceIntPoint(0, 0), mTextureSize);
  }

  gfx::IntPoint srcPoint = updateRegion.GetBounds().TopLeft().ToUnknownPoint();
  gfx::SurfaceFormat format = mUpdateDrawTarget->GetFormat();
  int bpp = gfx::BytesPerPixel(format);
  int32_t stride = mBufferSize.width * bpp;
  unsigned char* data = mUpdateDrawTargetData.get();
  data += srcPoint.y * stride + srcPoint.x * bpp;

  UploadImageDataToTexture(mGLContext, data, stride, format,
                           updateRegion.ToUnknownRegion(), mTexture, nullptr,
                           needInit, /* aPixelBuffer = */ false,
                           LOCAL_GL_TEXTURE0,
                           LOCAL_GL_TEXTURE_RECTANGLE_ARB);



  if (!aKeepSurface) {
    mUpdateDrawTarget = nullptr;
    mUpdateDrawTargetData = nullptr;
  }

  mInUpdate = false;
}

void
RectTextureImage::UpdateFromCGContext(const LayoutDeviceIntSize& aNewSize,
                                      const LayoutDeviceIntRegion& aDirtyRegion,
                                      CGContextRef aCGContext)
{
  gfx::IntSize size = gfx::IntSize(CGBitmapContextGetWidth(aCGContext),
                                   CGBitmapContextGetHeight(aCGContext));
  mBufferSize.SizeTo(size.width, size.height);
  RefPtr<gfx::DrawTarget> dt = BeginUpdate(aNewSize, aDirtyRegion);
  if (dt) {
    gfx::Rect rect(0, 0, size.width, size.height);
    gfxUtils::ClipToRegion(dt, GetUpdateRegion().ToUnknownRegion());
    RefPtr<gfx::SourceSurface> sourceSurface =
      dt->CreateSourceSurfaceFromData(static_cast<uint8_t *>(CGBitmapContextGetData(aCGContext)),
                                      size,
                                      CGBitmapContextGetBytesPerRow(aCGContext),
                                      gfx::SurfaceFormat::B8G8R8A8);
    dt->DrawSurface(sourceSurface, rect, rect,
                    gfx::DrawSurfaceOptions(),
                    gfx::DrawOptions(1.0, gfx::CompositionOp::OP_SOURCE));
    dt->PopClip();
    EndUpdate();
  }
}

void
RectTextureImage::Draw(layers::GLManager* aManager,
                       const LayoutDeviceIntPoint& aLocation,
                       const gfx::Matrix4x4& aTransform)
{
  layers::ShaderProgramOGL* program =
    aManager->GetProgram(LOCAL_GL_TEXTURE_RECTANGLE_ARB, gfx::SurfaceFormat::R8G8B8A8);

  aManager->gl()->fActiveTexture(LOCAL_GL_TEXTURE0);
  aManager->gl()->fBindTexture(LOCAL_GL_TEXTURE_RECTANGLE_ARB, mTexture);

  aManager->ActivateProgram(program);
  program->SetProjectionMatrix(aManager->GetProjMatrix());
  program->SetLayerTransform(gfx::Matrix4x4(aTransform).PostTranslate(aLocation.x, aLocation.y, 0));
  program->SetTextureTransform(gfx::Matrix4x4());
  program->SetRenderOffset(nsIntPoint(0, 0));
  program->SetTexCoordMultiplier(mUsedSize.width, mUsedSize.height);
  program->SetTextureUnit(0);

  aManager->BindAndDrawQuad(program,
                            gfx::Rect(0.0, 0.0, mUsedSize.width, mUsedSize.height),
                            gfx::Rect(0.0, 0.0, 1.0f, 1.0f));

  aManager->gl()->fBindTexture(LOCAL_GL_TEXTURE_RECTANGLE_ARB, 0);
}

} // namespace widget
} // namespace mozilla
