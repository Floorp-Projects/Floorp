/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "WinCompositorWidgetProxy.h"
#include "nsWindow.h"
#include "VsyncDispatcher.h"

namespace mozilla {
namespace widget {

WinCompositorWidgetProxy::WinCompositorWidgetProxy(nsWindow* aWindow)
 : mWindow(aWindow),
   mWnd(reinterpret_cast<HWND>(aWindow->GetNativeData(NS_NATIVE_WINDOW)))
{
  MOZ_ASSERT(aWindow);
  MOZ_ASSERT(!aWindow->Destroyed());
  MOZ_ASSERT(mWnd && ::IsWindow(mWnd));
}

bool
WinCompositorWidgetProxy::PreRender(layers::LayerManagerComposite* aManager)
{
  // This can block waiting for WM_SETTEXT to finish
  // Using PreRender is unnecessarily pessimistic because
  // we technically only need to block during the present call
  // not all of compositor rendering
  mPresentLock.Enter();
  return true;
}

void
WinCompositorWidgetProxy::PostRender(layers::LayerManagerComposite* aManager)
{
  mPresentLock.Leave();
}

void
WinCompositorWidgetProxy::EndRemoteDrawing()
{
  mWindow->EndRemoteDrawing();
}

nsIWidget*
WinCompositorWidgetProxy::RealWidget()
{
  return mWindow;
}

LayoutDeviceIntSize
WinCompositorWidgetProxy::GetClientSize()
{
  RECT r;
  if (!::GetClientRect(mWnd, &r)) {
    return LayoutDeviceIntSize();
  }
  return LayoutDeviceIntSize(
    r.right - r.left,
    r.bottom - r.top);
}

already_AddRefed<gfx::DrawTarget>
WinCompositorWidgetProxy::StartRemoteDrawing()
{
  return mWindow->StartRemoteDrawing();
}

already_AddRefed<CompositorVsyncDispatcher>
WinCompositorWidgetProxy::GetCompositorVsyncDispatcher()
{
  RefPtr<CompositorVsyncDispatcher> cvd = mWindow->GetCompositorVsyncDispatcher();
  return cvd.forget();
}

void
WinCompositorWidgetProxy::EnterPresentLock()
{
  mPresentLock.Enter();
}

void
WinCompositorWidgetProxy::LeavePresentLock()
{
  mPresentLock.Leave();
}

} // namespace widget
} // namespace mozilla
