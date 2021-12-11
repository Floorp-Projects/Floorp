/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set sw=2 ts=2 et tw=80 : */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "AndroidCompositorWidget.h"
#include "nsWindow.h"

namespace mozilla {
namespace widget {

AndroidCompositorWidget::AndroidCompositorWidget(
    const layers::CompositorOptions& aOptions, nsBaseWidget* aWidget)
    : InProcessCompositorWidget(aOptions, aWidget),
      mNativeWindow(nullptr),
      mFormat(WINDOW_FORMAT_RGBA_8888) {}

AndroidCompositorWidget::~AndroidCompositorWidget() {
  if (mNativeWindow) {
    ANativeWindow_release(mNativeWindow);
  }
}

already_AddRefed<gfx::DrawTarget>
AndroidCompositorWidget::StartRemoteDrawingInRegion(
    const LayoutDeviceIntRegion& aInvalidRegion,
    layers::BufferMode* aBufferMode) {
  if (!mNativeWindow) {
    EGLNativeWindowType window = GetEGLNativeWindow();
    JNIEnv* const env = jni::GetEnvForThread();
    mNativeWindow =
        ANativeWindow_fromSurface(env, reinterpret_cast<jobject>(window));
    if (mNativeWindow) {
      mFormat = ANativeWindow_getFormat(mNativeWindow);
      ANativeWindow_acquire(mNativeWindow);
    } else {
      return nullptr;
    }
  }

  if (mFormat != WINDOW_FORMAT_RGBA_8888 &&
      mFormat != WINDOW_FORMAT_RGBX_8888) {
    gfxCriticalNoteOnce << "Non supported format: " << mFormat;
    return nullptr;
  }

  // XXX Handle inOutDirtyBounds
  if (ANativeWindow_lock(mNativeWindow, &mBuffer, nullptr) != 0) {
    return nullptr;
  }

  const int bpp = 4;
  gfx::SurfaceFormat format = gfx::SurfaceFormat::R8G8B8A8;
  if (mFormat == WINDOW_FORMAT_RGBX_8888) {
    format = gfx::SurfaceFormat::R8G8B8X8;
  }

  RefPtr<gfx::DrawTarget> dt = gfx::Factory::CreateDrawTargetForData(
      gfx::BackendType::SKIA, static_cast<unsigned char*>(mBuffer.bits),
      gfx::IntSize(mBuffer.width, mBuffer.height), mBuffer.stride * bpp, format,
      true);

  return dt.forget();
}

void AndroidCompositorWidget::EndRemoteDrawingInRegion(
    gfx::DrawTarget* aDrawTarget, const LayoutDeviceIntRegion& aInvalidRegion) {
  ANativeWindow_unlockAndPost(mNativeWindow);
}

EGLNativeWindowType AndroidCompositorWidget::GetEGLNativeWindow() {
  return (EGLNativeWindowType)mWidget->GetNativeData(NS_JAVA_SURFACE);
}

EGLNativeWindowType AndroidCompositorWidget::GetPresentationEGLSurface() {
  return (EGLNativeWindowType)mWidget->GetNativeData(NS_PRESENTATION_SURFACE);
}

void AndroidCompositorWidget::SetPresentationEGLSurface(EGLSurface aVal) {
  mWidget->SetNativeData(NS_PRESENTATION_SURFACE, (uintptr_t)aVal);
}

ANativeWindow* AndroidCompositorWidget::GetPresentationANativeWindow() {
  return (ANativeWindow*)mWidget->GetNativeData(NS_PRESENTATION_WINDOW);
}

}  // namespace widget
}  // namespace mozilla
