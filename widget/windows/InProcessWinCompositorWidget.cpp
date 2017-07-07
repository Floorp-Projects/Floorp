/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "gfxPlatform.h"
#include "HeadlessCompositorWidget.h"
#include "HeadlessWidget.h"
#include "mozilla/widget/PlatformWidgetTypes.h"

#include "InProcessWinCompositorWidget.h"
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
    return new InProcessWinCompositorWidget(aInitData.get_WinCompositorWidgetInitData(),
                                            aOptions, static_cast<nsWindow*>(aWidget));
  }
}

InProcessWinCompositorWidget::InProcessWinCompositorWidget(const WinCompositorWidgetInitData& aInitData,
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
