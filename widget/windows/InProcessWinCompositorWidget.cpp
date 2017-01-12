/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "InProcessWinCompositorWidget.h"
#include "nsWindow.h"

namespace mozilla {
namespace widget {

/* static */ RefPtr<CompositorWidget>
CompositorWidget::CreateLocal(const CompositorWidgetInitData& aInitData,
                              const layers::CompositorOptions& aOptions,
                              nsIWidget* aWidget)
{
  return new InProcessWinCompositorWidget(aInitData, aOptions, static_cast<nsWindow*>(aWidget));
}

InProcessWinCompositorWidget::InProcessWinCompositorWidget(const CompositorWidgetInitData& aInitData,
                                                           const layers::CompositorOptions& aOptions,
                                                           nsWindow* aWindow)
 : WinCompositorWidget(aInitData, aOptions),
   mWindow(aWindow)
{
  MOZ_ASSERT(mWindow);
}

nsIWidget*
InProcessWinCompositorWidget::RealWidget()
{
  return mWindow;
}

void
InProcessWinCompositorWidget::ObserveVsync(VsyncObserver* aObserver)
{
  if (RefPtr<CompositorVsyncDispatcher> cvd = mWindow->GetCompositorVsyncDispatcher()) {
    cvd->SetCompositorVsyncObserver(aObserver);
  }
}

} // namespace widget
} // namespace mozilla
