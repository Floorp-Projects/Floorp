/* -*- Mode: c++; c-basic-offset: 2; tab-width: 20; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef SurfaceViewWrapperSupport_h__
#define SurfaceViewWrapperSupport_h__

#include "mozilla/java/SurfaceViewWrapperNatives.h"
#include <android/native_window.h>
#include <android/native_window_jni.h>

namespace mozilla {
namespace widget {

class SurfaceViewWrapperSupport final
    : public java::SurfaceViewWrapper::Natives<SurfaceViewWrapperSupport> {
 public:
  static bool IsSurfaceAbandoned(jni::Object::Param aSurface) {
    ANativeWindow* win =
        ANativeWindow_fromSurface(jni::GetEnvForThread(), aSurface.Get());
    if (!win) {
      return true;
    }

    // If the Surface's underlying BufferQueue has been abandoned, then
    // ANativeWindow_getWidth (or height) will return a negative value
    // to indicate an error.
    int32_t width = ANativeWindow_getWidth(win);

    ANativeWindow_release(win);
    return width < 0;
  }
};

}  // namespace widget
}  // namespace mozilla

#endif  // SurfaceViewWrapperSupport_h__
