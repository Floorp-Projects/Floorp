/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef widget_headless_HeadlessCompositorWidget_h
#define widget_headless_HeadlessCompositorWidget_h

#include "mozilla/widget/CompositorWidget.h"

#include "HeadlessWidget.h"

namespace mozilla {
namespace widget {

class HeadlessCompositorWidgetInitData;

class HeadlessCompositorWidget final : public CompositorWidget,
                                       public CompositorWidgetDelegate {
 public:
  HeadlessCompositorWidget(const HeadlessCompositorWidgetInitData& aInitData,
                           const layers::CompositorOptions& aOptions,
                           HeadlessWidget* aWindow);

  void NotifyClientSizeChanged(const LayoutDeviceIntSize& aClientSize);

  // CompositorWidget Overrides

  uintptr_t GetWidgetKey() override;

  LayoutDeviceIntSize GetClientSize() override;

  nsIWidget* RealWidget() override;
  CompositorWidgetDelegate* AsDelegate() override { return this; }

  void ObserveVsync(VsyncObserver* aObserver) override;

  // CompositorWidgetDelegate Overrides

  HeadlessCompositorWidget* AsHeadlessCompositorWidget() override {
    return this;
  }

 private:
  HeadlessWidget* mWidget;

  // See GtkCompositorWidget for the justification for this mutex.
  DataMutex<LayoutDeviceIntSize> mClientSize;
};

}  // namespace widget
}  // namespace mozilla

#endif  // widget_headless_HeadlessCompositor_h
