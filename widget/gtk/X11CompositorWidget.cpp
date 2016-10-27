/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "X11CompositorWidget.h"

#include "gfxPlatformGtk.h"
#include "mozilla/layers/CompositorThread.h"
#include "mozilla/widget/InProcessCompositorWidget.h"
#include "mozilla/widget/PlatformWidgetTypes.h"
#include "nsWindow.h"

namespace mozilla {
namespace widget {

X11CompositorWidget::X11CompositorWidget(const CompositorWidgetInitData& aInitData,
                                         nsWindow* aWindow)
      : mWidget(aWindow)
{
  // If we have a nsWindow, then grab the already existing display connection
  // If we don't, then use the init data to connect to the display
  if (aWindow) {
    mXDisplay = aWindow->XDisplay();
  } else {
    mXDisplay = XOpenDisplay(aInitData.XDisplayString().get());
  }
  mXWindow = (Window)aInitData.XWindow();

  // Grab the window's visual and depth
  XWindowAttributes windowAttrs;
  XGetWindowAttributes(mXDisplay, mXWindow, &windowAttrs);

  Visual*   visual = windowAttrs.visual;
  int       depth = windowAttrs.depth;

  // Initialize the window surface provider
  mProvider.Initialize(
    mXDisplay,
    mXWindow,
    visual,
    depth
    );

  mClientSize = aInitData.InitialClientSize();
}

X11CompositorWidget::~X11CompositorWidget()
{
  mProvider.CleanupResources();

  // If we created our own display connection, we need to destroy it
  if (!mWidget && mXDisplay) {
    XCloseDisplay(mXDisplay);
    mXDisplay = nullptr;
  }
}

already_AddRefed<gfx::DrawTarget>
X11CompositorWidget::StartRemoteDrawing()
{
  return nullptr;
}
void
X11CompositorWidget::EndRemoteDrawing()
{
}

already_AddRefed<gfx::DrawTarget>
X11CompositorWidget::StartRemoteDrawingInRegion(LayoutDeviceIntRegion& aInvalidRegion,
                                                layers::BufferMode* aBufferMode)
{
  return mProvider.StartRemoteDrawingInRegion(aInvalidRegion,
                                              aBufferMode);
}

void X11CompositorWidget::EndRemoteDrawingInRegion(gfx::DrawTarget* aDrawTarget,
                              LayoutDeviceIntRegion& aInvalidRegion)
{
  mProvider.EndRemoteDrawingInRegion(aDrawTarget,
                                     aInvalidRegion);
}

nsIWidget* X11CompositorWidget::RealWidget()
{
  return mWidget;
}

void
X11CompositorWidget::NotifyClientSizeChanged(const LayoutDeviceIntSize& aClientSize)
{
  mClientSize = aClientSize;
}

LayoutDeviceIntSize
X11CompositorWidget::GetClientSize()
{
  return mClientSize;
}

uintptr_t
X11CompositorWidget::GetWidgetKey()
{
  return reinterpret_cast<uintptr_t>(mWidget);
}

} // namespace widget
} // namespace mozilla
