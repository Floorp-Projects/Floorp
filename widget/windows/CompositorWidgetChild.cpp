/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "CompositorWidgetChild.h"
#include "mozilla/unused.h"
#include "mozilla/widget/CompositorWidgetVsyncObserver.h"
#include "nsBaseWidget.h"
#include "VsyncDispatcher.h"

namespace mozilla {
namespace widget {

CompositorWidgetChild::CompositorWidgetChild(RefPtr<CompositorVsyncDispatcher> aVsyncDispatcher,
                                             RefPtr<CompositorWidgetVsyncObserver> aVsyncObserver)
 : mVsyncDispatcher(aVsyncDispatcher),
   mVsyncObserver(aVsyncObserver)
{
  MOZ_ASSERT(XRE_IsParentProcess());
}

CompositorWidgetChild::~CompositorWidgetChild()
{
}

void
CompositorWidgetChild::EnterPresentLock()
{
  Unused << SendEnterPresentLock();
}

void
CompositorWidgetChild::LeavePresentLock()
{
  Unused << SendLeavePresentLock();
}

void
CompositorWidgetChild::OnDestroyWindow()
{
}

void
CompositorWidgetChild::UpdateTransparency(nsTransparencyMode aMode)
{
  Unused << SendUpdateTransparency(static_cast<int32_t>(aMode));
}

void
CompositorWidgetChild::ClearTransparentWindow()
{
  Unused << SendClearTransparentWindow();
}

HDC
CompositorWidgetChild::GetTransparentDC() const
{
  // Not supported in out-of-process mode.
  return nullptr;
}

bool
CompositorWidgetChild::RecvObserveVsync()
{
  mVsyncDispatcher->SetCompositorVsyncObserver(mVsyncObserver);
  return true;
}

bool
CompositorWidgetChild::RecvUnobserveVsync()
{
  mVsyncDispatcher->SetCompositorVsyncObserver(nullptr);
  return true;
}

} // namespace widget
} // namespace mozilla
