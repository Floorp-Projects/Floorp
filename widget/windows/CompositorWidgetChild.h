/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef widget_windows_CompositorWidgetChild_h
#define widget_windows_CompositorWidgetChild_h

#include "WinCompositorWidget.h"
#include "mozilla/widget/PCompositorWidgetChild.h"
#include "mozilla/widget/CompositorWidgetVsyncObserver.h"

namespace mozilla {
class CompositorVsyncDispatcher;

namespace widget {

namespace remote_backbuffer {
class Provider;
}

class CompositorWidgetChild final : public PCompositorWidgetChild,
                                    public PlatformCompositorWidgetDelegate {
 public:
  CompositorWidgetChild(RefPtr<CompositorVsyncDispatcher> aVsyncDispatcher,
                        RefPtr<CompositorWidgetVsyncObserver> aVsyncObserver,
                        const CompositorWidgetInitData& aInitData);
  ~CompositorWidgetChild() override;

  bool Initialize();

  void EnterPresentLock() override;
  void LeavePresentLock() override;
  void OnDestroyWindow() override;
  bool OnWindowResize(const LayoutDeviceIntSize& aSize) override;
  void OnWindowModeChange(nsSizeMode aSizeMode) override;
  void UpdateTransparency(TransparencyMode aMode) override;
  void NotifyVisibilityUpdated(nsSizeMode aSizeMode,
                               bool aIsFullyOccluded) override;
  void ClearTransparentWindow() override;

  mozilla::ipc::IPCResult RecvObserveVsync() override;
  mozilla::ipc::IPCResult RecvUnobserveVsync() override;
  mozilla::ipc::IPCResult RecvUpdateCompositorWnd(
      const WindowsHandle& aCompositorWnd, const WindowsHandle& aParentWnd,
      UpdateCompositorWndResolver&& aResolve) override;

 private:
  RefPtr<CompositorVsyncDispatcher> mVsyncDispatcher;
  RefPtr<CompositorWidgetVsyncObserver> mVsyncObserver;
  HWND mCompositorWnd;

  HWND mWnd;
  TransparencyMode mTransparencyMode;

  std::unique_ptr<remote_backbuffer::Provider> mRemoteBackbufferProvider;
};

}  // namespace widget
}  // namespace mozilla

#endif  // widget_windows_CompositorWidgetChild_h
