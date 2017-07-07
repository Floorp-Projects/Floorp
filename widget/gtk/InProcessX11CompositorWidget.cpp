/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "HeadlessCompositorWidget.h"
#include "HeadlessWidget.h"
#include "mozilla/widget/PlatformWidgetTypes.h"

#include "InProcessX11CompositorWidget.h"
#include "nsWindow.h"

namespace mozilla {
namespace widget {

/* static */ RefPtr<CompositorWidget>
CompositorWidget::CreateLocal(const CompositorWidgetInitData& aInitData,
                              const layers::CompositorOptions& aOptions,
                              nsIWidget* aWidget)
{
  if (aInitData.type() == CompositorWidgetInitData::THeadlessCompositorWidgetInitData) {
    return new HeadlessCompositorWidget(aInitData.get_HeadlessCompositorWidgetInitData(),
                                        aOptions, static_cast<HeadlessWidget*>(aWidget));
  } else {
    return new InProcessX11CompositorWidget(aInitData.get_X11CompositorWidgetInitData(),
                                            aOptions, static_cast<nsWindow*>(aWidget));
  }
}

InProcessX11CompositorWidget::InProcessX11CompositorWidget(const X11CompositorWidgetInitData& aInitData,
                                                           const layers::CompositorOptions& aOptions,
                                                           nsWindow* aWindow)
  : X11CompositorWidget(aInitData, aOptions, aWindow)
{
}

void
InProcessX11CompositorWidget::ObserveVsync(VsyncObserver* aObserver)
{
  if (RefPtr<CompositorVsyncDispatcher> cvd = mWidget->GetCompositorVsyncDispatcher()) {
    cvd->SetCompositorVsyncObserver(aObserver);
  }
}

} // namespace widget
} // namespace mozilla
