/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "CompositorWidgetChild.h"
#include "mozilla/Unused.h"
#include "mozilla/widget/CompositorWidgetVsyncObserver.h"
#include "mozilla/widget/PlatformWidgetTypes.h"
#include "nsBaseWidget.h"
#include "VsyncDispatcher.h"
#include "gfxPlatform.h"

namespace mozilla {
namespace widget {

CompositorWidgetChild::CompositorWidgetChild(
    RefPtr<CompositorVsyncDispatcher> aVsyncDispatcher,
    RefPtr<CompositorWidgetVsyncObserver> aVsyncObserver,
    const CompositorWidgetInitData& aInitData)
    : mVsyncDispatcher(aVsyncDispatcher),
      mVsyncObserver(aVsyncObserver),
      mCompositorWnd(nullptr),
      mParentWnd(reinterpret_cast<HWND>(
          aInitData.get_WinCompositorWidgetInitData().hWnd())) {
  MOZ_ASSERT(XRE_IsParentProcess());
  MOZ_ASSERT(!gfxPlatform::IsHeadless());
  MOZ_ASSERT(mParentWnd && ::IsWindow(mParentWnd));
}

CompositorWidgetChild::~CompositorWidgetChild() {}

void CompositorWidgetChild::EnterPresentLock() {
  Unused << SendEnterPresentLock();
}

void CompositorWidgetChild::LeavePresentLock() {
  Unused << SendLeavePresentLock();
}

void CompositorWidgetChild::OnDestroyWindow() {}

void CompositorWidgetChild::UpdateTransparency(nsTransparencyMode aMode) {
  Unused << SendUpdateTransparency(aMode);
}

void CompositorWidgetChild::ClearTransparentWindow() {
  Unused << SendClearTransparentWindow();
}

mozilla::ipc::IPCResult CompositorWidgetChild::RecvObserveVsync() {
  mVsyncDispatcher->SetCompositorVsyncObserver(mVsyncObserver);
  return IPC_OK();
}

mozilla::ipc::IPCResult CompositorWidgetChild::RecvUnobserveVsync() {
  mVsyncDispatcher->SetCompositorVsyncObserver(nullptr);
  return IPC_OK();
}

mozilla::ipc::IPCResult CompositorWidgetChild::RecvUpdateCompositorWnd(
    const WindowsHandle& aCompositorWnd, const WindowsHandle& aParentWnd,
    UpdateCompositorWndResolver&& aResolve) {
  MOZ_ASSERT(mParentWnd);

  HWND parentWnd = reinterpret_cast<HWND>(aParentWnd);
  if (mParentWnd == parentWnd) {
    mCompositorWnd = reinterpret_cast<HWND>(aCompositorWnd);
    ::SetParent(mCompositorWnd, mParentWnd);
    aResolve(true);
  } else {
    aResolve(false);
    gfxCriticalNote << "Parent winow does not match";
    MOZ_ASSERT_UNREACHABLE("unexpected to happen");
  }

  return IPC_OK();
}

}  // namespace widget
}  // namespace mozilla
