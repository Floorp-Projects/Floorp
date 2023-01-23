/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "CompositorWidgetChild.h"
#include "mozilla/Unused.h"
#include "mozilla/gfx/Logging.h"
#include "mozilla/widget/CompositorWidgetVsyncObserver.h"
#include "mozilla/widget/PlatformWidgetTypes.h"
#include "nsBaseWidget.h"
#include "VsyncDispatcher.h"
#include "gfxPlatform.h"
#include "RemoteBackbuffer.h"

namespace mozilla {
namespace widget {

CompositorWidgetChild::CompositorWidgetChild(
    RefPtr<CompositorVsyncDispatcher> aVsyncDispatcher,
    RefPtr<CompositorWidgetVsyncObserver> aVsyncObserver,
    const CompositorWidgetInitData& aInitData)
    : mVsyncDispatcher(aVsyncDispatcher),
      mVsyncObserver(aVsyncObserver),
      mCompositorWnd(nullptr),
      mWnd(reinterpret_cast<HWND>(
          aInitData.get_WinCompositorWidgetInitData().hWnd())),
      mTransparencyMode(
          aInitData.get_WinCompositorWidgetInitData().transparencyMode()),
      mRemoteBackbufferProvider() {
  MOZ_ASSERT(XRE_IsParentProcess());
  MOZ_ASSERT(!gfxPlatform::IsHeadless());
  MOZ_ASSERT(mWnd && ::IsWindow(mWnd));
}

CompositorWidgetChild::~CompositorWidgetChild() {}

bool CompositorWidgetChild::Initialize() {
  mRemoteBackbufferProvider = std::make_unique<remote_backbuffer::Provider>();
  if (!mRemoteBackbufferProvider->Initialize(mWnd, OtherPid(),
                                             mTransparencyMode)) {
    return false;
  }

  auto maybeRemoteHandles = mRemoteBackbufferProvider->CreateRemoteHandles();
  if (!maybeRemoteHandles) {
    return false;
  }

  Unused << SendInitialize(*maybeRemoteHandles);

  return true;
}

void CompositorWidgetChild::EnterPresentLock() {
  Unused << SendEnterPresentLock();
}

void CompositorWidgetChild::LeavePresentLock() {
  Unused << SendLeavePresentLock();
}

void CompositorWidgetChild::OnDestroyWindow() {}

bool CompositorWidgetChild::OnWindowResize(const LayoutDeviceIntSize& aSize) {
  return true;
}

void CompositorWidgetChild::OnWindowModeChange(nsSizeMode aSizeMode) {}

void CompositorWidgetChild::UpdateTransparency(TransparencyMode aMode) {
  mTransparencyMode = aMode;
  mRemoteBackbufferProvider->UpdateTransparencyMode(aMode);
  Unused << SendUpdateTransparency(aMode);
}

void CompositorWidgetChild::NotifyVisibilityUpdated(nsSizeMode aSizeMode,
                                                    bool aIsFullyOccluded) {
  Unused << SendNotifyVisibilityUpdated(aSizeMode, aIsFullyOccluded);
};

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
  HWND parentWnd = reinterpret_cast<HWND>(aParentWnd);
  if (mWnd == parentWnd) {
    mCompositorWnd = reinterpret_cast<HWND>(aCompositorWnd);
    ::SetParent(mCompositorWnd, mWnd);
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
