/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "CompositorWidgetChild.h"
#include "mozilla/unused.h"

namespace mozilla {
namespace widget {

CompositorWidgetChild::CompositorWidgetChild(nsIWidget* aWidget)
{
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

void
CompositorWidgetChild::ResizeTransparentWindow(const gfx::IntSize& aSize)
{
  Unused << SendResizeTransparentWindow(aSize);
}

HDC CompositorWidgetChild::GetTransparentDC() const
{
  // Not supported in out-of-process mode.
  return nullptr;
}

} // namespace widget
} // namespace mozilla
