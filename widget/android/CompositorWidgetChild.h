/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef widget_android_CompositorWidgetChild_h
#define widget_android_CompositorWidgetChild_h

#include "AndroidCompositorWidget.h"
#include "mozilla/widget/PCompositorWidgetChild.h"
#include "mozilla/widget/CompositorWidgetVsyncObserver.h"

namespace mozilla {
namespace widget {

class CompositorWidgetChild final : public PCompositorWidgetChild,
                                    public PlatformCompositorWidgetDelegate {
 public:
  CompositorWidgetChild(RefPtr<CompositorVsyncDispatcher> aVsyncDispatcher,
                        RefPtr<CompositorWidgetVsyncObserver> aVsyncObserver,
                        const CompositorWidgetInitData&);
  ~CompositorWidgetChild() override;

  bool Initialize();

  mozilla::ipc::IPCResult RecvObserveVsync() override;
  mozilla::ipc::IPCResult RecvUnobserveVsync() override;

  // PlatformCompositorWidgetDelegate overrides

  void NotifyClientSizeChanged(const LayoutDeviceIntSize& aClientSize) override;

 private:
  RefPtr<CompositorVsyncDispatcher> mVsyncDispatcher;
  RefPtr<CompositorWidgetVsyncObserver> mVsyncObserver;
};

}  // namespace widget
}  // namespace mozilla

#endif  // widget_android_CompositorWidgetChild_h
