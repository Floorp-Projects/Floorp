/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "InProcessX11CompositorWidget.h"

#include "nsWindow.h"

namespace mozilla {
namespace widget {

/* static */ RefPtr<CompositorWidget>
CompositorWidget::CreateLocal(const CompositorWidgetInitData& aInitData, nsIWidget* aWidget)
{
  return new InProcessX11CompositorWidget(aInitData, static_cast<nsWindow*>(aWidget));
}

InProcessX11CompositorWidget::InProcessX11CompositorWidget(const CompositorWidgetInitData& aInitData,
                                                           nsWindow* aWindow)
  : X11CompositorWidget(aInitData, aWindow)
{
}

void
InProcessX11CompositorWidget::ObserveVsync(VsyncObserver* aObserver)
{
  RefPtr<CompositorVsyncDispatcher> cvd = mWidget->GetCompositorVsyncDispatcher();
  cvd->SetCompositorVsyncObserver(aObserver);
}

} // namespace widget
} // namespace mozilla
