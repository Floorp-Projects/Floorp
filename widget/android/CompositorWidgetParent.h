/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef widget_android_CompositorWidgetParent_h
#define widget_android_CompositorWidgetParent_h

#include "AndroidCompositorWidget.h"
#include "mozilla/VsyncDispatcher.h"
#include "mozilla/widget/PCompositorWidgetParent.h"

namespace mozilla {
namespace widget {

class CompositorWidgetParent final : public PCompositorWidgetParent,
                                     public AndroidCompositorWidget {
 public:
  explicit CompositorWidgetParent(const CompositorWidgetInitData& aInitData,
                                  const layers::CompositorOptions& aOptions);
  ~CompositorWidgetParent() override;

  // CompositorWidget overrides

  nsIWidget* RealWidget() override;
  void ObserveVsync(VsyncObserver* aObserver) override;
  RefPtr<VsyncObserver> GetVsyncObserver() const override;

  // AndroidCompositorWidget overrides

  void OnCompositorSurfaceChanged() override;

 private:
  RefPtr<VsyncObserver> mVsyncObserver;
};

}  // namespace widget
}  // namespace mozilla

#endif  // widget_android_CompositorWidgetParent_h
