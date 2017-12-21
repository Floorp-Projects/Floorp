/* -*- Mode: c++; c-basic-offset: 4; tab-width: 20; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_widget_AndroidCompositorWidget_h
#define mozilla_widget_AndroidCompositorWidget_h

#include "GLDefs.h"
#include "mozilla/widget/InProcessCompositorWidget.h"

struct ANativeWindow;

namespace mozilla {
namespace widget {

/**
 * AndroidCompositorWidget inherits from InProcessCompositorWidget because
 * Android does not support OOP compositing yet. Once it does,
 * AndroidCompositorWidget will be made to inherit from CompositorWidget
 * instead.
 */
class AndroidCompositorWidget final : public InProcessCompositorWidget
{
public:
    using InProcessCompositorWidget::InProcessCompositorWidget;

    AndroidCompositorWidget* AsAndroid() override { return this; }

    EGLNativeWindowType GetEGLNativeWindow();

    EGLSurface GetPresentationEGLSurface();
    void SetPresentationEGLSurface(EGLSurface aVal);

    ANativeWindow* GetPresentationANativeWindow();
};

} // namespace widget
} // namespace mozilla

#endif // mozilla_widget_AndroidCompositorWidget_h
