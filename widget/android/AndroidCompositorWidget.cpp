/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set sw=2 ts=2 et tw=80 : */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "AndroidCompositorWidget.h"

#include "mozilla/gfx/Logging.h"
#include "mozilla/widget/PlatformWidgetTypes.h"
#include "nsWindow.h"

namespace mozilla {
namespace widget {

AndroidCompositorWidget::AndroidCompositorWidget(
    const AndroidCompositorWidgetInitData& aInitData,
    const layers::CompositorOptions& aOptions)
    : CompositorWidget(aOptions),
      mWidgetId(aInitData.widgetId()),
      mNativeWindow(nullptr),
      mFormat(WINDOW_FORMAT_RGBA_8888),
      mClientSize(aInitData.clientSize()) {}

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

bool AndroidCompositorWidget::OnResumeComposition() {
  OnCompositorSurfaceChanged();

  if (!mSurface) {
    gfxCriticalError() << "OnResumeComposition called with null Surface";
    return false;
  }

  JNIEnv* const env = jni::GetEnvForThread();
  ANativeWindow* const nativeWindow =
      ANativeWindow_fromSurface(env, reinterpret_cast<jobject>(mSurface.Get()));
  if (!nativeWindow) {
    gfxCriticalError() << "OnResumeComposition called with invalid Surface";
    return false;
  }

  const int32_t width = ANativeWindow_getWidth(nativeWindow);
  const int32_t height = ANativeWindow_getHeight(nativeWindow);
  mClientSize = LayoutDeviceIntSize(std::min(width, MOZ_WIDGET_MAX_SIZE),
                                    std::min(height, MOZ_WIDGET_MAX_SIZE));

  ANativeWindow_release(nativeWindow);

  // A negative return value from ANativeWindow_getWidth/Height can indicate the
  // underlying BufferQueue has been abandoned, and subsequent EGLSurface
  // creation will fail.
  if (mClientSize.width < 0 || mClientSize.height < 0) {
    gfxCriticalNote << "ANativeWindow_getWidth/Height returned error: "
                    << mClientSize;
  }

  return true;
}

EGLNativeWindowType AndroidCompositorWidget::GetEGLNativeWindow() {
  return (EGLNativeWindowType)mSurface.Get();
}

LayoutDeviceIntSize AndroidCompositorWidget::GetClientSize() {
  return mClientSize;
}

void AndroidCompositorWidget::NotifyClientSizeChanged(
    const LayoutDeviceIntSize& aClientSize) {
  mClientSize = aClientSize;
}

}  // namespace widget
}  // namespace mozilla
