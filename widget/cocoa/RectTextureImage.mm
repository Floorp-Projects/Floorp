/* -*- Mode: objc; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "RectTextureImage.h"

#include "gfxUtils.h"
#include "GLContextCGL.h"
#include "mozilla/layers/GLManager.h"
#include "mozilla/gfx/MacIOSurface.h"
#include "OGLShaderProgram.h"
#include "ScopedGLHelpers.h"

namespace mozilla {
namespace widget {

RectTextureImage::RectTextureImage()
  : mGLContext(nullptr)
  , mTexture(0)
  , mInUpdate(false)
{
}

RectTextureImage::~RectTextureImage()
{
  DeleteTexture();
}

already_AddRefed<gfx::DrawTarget>
RectTextureImage::BeginUpdate(const LayoutDeviceIntSize& aNewSize,
                              const LayoutDeviceIntRegion& aDirtyRegion)
{
  MOZ_ASSERT(!mInUpdate, "Beginning update during update!");
  mUpdateRegion = aDirtyRegion;
  bool needRecreate = false;
  if (aNewSize != mBufferSize) {
    mBufferSize = aNewSize;
    mUpdateRegion =
      LayoutDeviceIntRect(LayoutDeviceIntPoint(0, 0), aNewSize);
    needRecreate = true;
  }

  if (mUpdateRegion.IsEmpty()) {
    return nullptr;
  }

  if (!mIOSurface || needRecreate) {
    DeleteTexture();
    mIOSurface = MacIOSurface::CreateIOSurface(mBufferSize.width,
                                               mBufferSize.height);

    if (!mIOSurface) {
      return nullptr;
    }
  }

  mInUpdate = true;

  mIOSurface->Lock(false);
  unsigned char* ioData = (unsigned char*)mIOSurface->GetBaseAddress();
  gfx::IntSize size(mBufferSize.width, mBufferSize.height);
  int32_t stride = mIOSurface->GetBytesPerRow();
  gfx::SurfaceFormat format = gfx::SurfaceFormat::B8G8R8A8;
  RefPtr<gfx::DrawTarget> drawTarget =
    gfx::Factory::CreateDrawTargetForData(gfx::BackendType::SKIA,
                                          ioData, size,
                                          stride, format);
  return drawTarget.forget();
}

void
RectTextureImage::EndUpdate()
{
  MOZ_ASSERT(mInUpdate, "Ending update while not in update");
  mIOSurface->Unlock(false);
  mInUpdate = false;
}

void
RectTextureImage::UpdateFromCGContext(const LayoutDeviceIntSize& aNewSize,
                                      const LayoutDeviceIntRegion& aDirtyRegion,
                                      CGContextRef aCGContext)
{
  gfx::IntSize size = gfx::IntSize(CGBitmapContextGetWidth(aCGContext),
                                   CGBitmapContextGetHeight(aCGContext));
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
  gl::GLContext* gl = aManager->gl();

  bool bound = BindIOSurfaceToTexture(gl);
  if (!bound) {
    return;
  }

  layers::ShaderProgramOGL* program =
    aManager->GetProgram(LOCAL_GL_TEXTURE_RECTANGLE_ARB,
                         gfx::SurfaceFormat::R8G8B8A8);

  gl->fActiveTexture(LOCAL_GL_TEXTURE0);
  gl::ScopedBindTexture texture(gl, mTexture, LOCAL_GL_TEXTURE_RECTANGLE_ARB);

  aManager->ActivateProgram(program);
  program->SetProjectionMatrix(aManager->GetProjMatrix());
  program->SetLayerTransform(gfx::Matrix4x4(aTransform).PostTranslate(aLocation.x, aLocation.y, 0));
  program->SetTextureTransform(gfx::Matrix4x4());
  program->SetRenderOffset(nsIntPoint(0, 0));
  program->SetTexCoordMultiplier(mBufferSize.width, mBufferSize.height);
  program->SetTextureUnit(0);

  aManager->BindAndDrawQuad(program,
                            gfx::Rect(0.0, 0.0, mBufferSize.width, mBufferSize.height),
                            gfx::Rect(0.0, 0.0, 1.0f, 1.0f));
}

void
RectTextureImage::DeleteTexture()
{
  if (mTexture) {
    MOZ_ASSERT(mGLContext);
    mGLContext->MakeCurrent();
    mGLContext->fDeleteTextures(1, &mTexture);
    mTexture = 0;
  }
}

bool
RectTextureImage::BindIOSurfaceToTexture(gl::GLContext* aGL)
{
  if (!mIOSurface) {
    // If our size is zero or MacIOSurface::CreateIOSurface failed for some
    // other reason, there's nothing we can bind.
    return false;
  }

  if (!mTexture) {
    MOZ_ASSERT(aGL);
    aGL->fGenTextures(1, &mTexture);
    aGL->fActiveTexture(LOCAL_GL_TEXTURE0);
    gl::ScopedBindTexture texture(aGL, mTexture, LOCAL_GL_TEXTURE_RECTANGLE_ARB);
    aGL->fTexParameteri(LOCAL_GL_TEXTURE_RECTANGLE_ARB,
                        LOCAL_GL_TEXTURE_MIN_FILTER,
                        LOCAL_GL_LINEAR);
    aGL->fTexParameteri(LOCAL_GL_TEXTURE_RECTANGLE_ARB,
                        LOCAL_GL_TEXTURE_MAG_FILTER,
                        LOCAL_GL_LINEAR);
    aGL->fTexParameteri(LOCAL_GL_TEXTURE_RECTANGLE_ARB,
                        LOCAL_GL_TEXTURE_WRAP_T,
                        LOCAL_GL_CLAMP_TO_EDGE);
    aGL->fTexParameteri(LOCAL_GL_TEXTURE_RECTANGLE_ARB,
                        LOCAL_GL_TEXTURE_WRAP_S,
                        LOCAL_GL_CLAMP_TO_EDGE);

    mIOSurface->CGLTexImageIOSurface2D(aGL,
                                       gl::GLContextCGL::Cast(aGL)->GetCGLContext(),
                                       0);
    mGLContext = aGL;
  }

  return true;
}

} // namespace widget
} // namespace mozilla
