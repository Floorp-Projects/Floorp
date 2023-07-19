/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef widget_windows_CompositorWidgetParent_h
#define widget_windows_CompositorWidgetParent_h

#include "WinCompositorWidget.h"
#include "mozilla/Maybe.h"
#include "mozilla/widget/PCompositorWidgetParent.h"

namespace mozilla {
namespace widget {

enum class TransparencyMode : uint8_t;

namespace remote_backbuffer {
class Client;
}

class CompositorWidgetParent final : public PCompositorWidgetParent,
                                     public WinCompositorWidget {
 public:
  explicit CompositorWidgetParent(const CompositorWidgetInitData& aInitData,
                                  const layers::CompositorOptions& aOptions);
  ~CompositorWidgetParent() override;

  bool Initialize(const RemoteBackbufferHandles& aRemoteHandles);

  bool PreRender(WidgetRenderingContext*) override;
  void PostRender(WidgetRenderingContext*) override;
  already_AddRefed<gfx::DrawTarget> StartRemoteDrawingInRegion(
      const LayoutDeviceIntRegion& aInvalidRegion,
      layers::BufferMode* aBufferMode) override;
  void EndRemoteDrawingInRegion(
      gfx::DrawTarget* aDrawTarget,
      const LayoutDeviceIntRegion& aInvalidRegion) override;
  bool NeedsToDeferEndRemoteDrawing() override;
  LayoutDeviceIntSize GetClientSize() override;
  already_AddRefed<gfx::DrawTarget> GetBackBufferDrawTarget(
      gfx::DrawTarget* aScreenTarget, const gfx::IntRect& aRect,
      bool* aOutIsCleared) override;
  already_AddRefed<gfx::SourceSurface> EndBackBufferDrawing() override;
  bool InitCompositor(layers::Compositor* aCompositor) override;
  bool IsHidden() const override;

  nsSizeMode GetWindowSizeMode() const override;
  bool GetWindowIsFullyOccluded() const override;

  mozilla::ipc::IPCResult RecvInitialize(
      const RemoteBackbufferHandles& aRemoteHandles) override;
  mozilla::ipc::IPCResult RecvEnterPresentLock() override;
  mozilla::ipc::IPCResult RecvLeavePresentLock() override;
  mozilla::ipc::IPCResult RecvUpdateTransparency(
      const TransparencyMode& aMode) override;
  mozilla::ipc::IPCResult RecvNotifyVisibilityUpdated(
      const nsSizeMode& aSizeMode, const bool& aIsFullyOccluded) override;
  mozilla::ipc::IPCResult RecvClearTransparentWindow() override;
  void ActorDestroy(ActorDestroyReason aWhy) override;

  nsIWidget* RealWidget() override;
  void ObserveVsync(VsyncObserver* aObserver) override;
  RefPtr<VsyncObserver> GetVsyncObserver() const override;

  // PlatformCompositorWidgetDelegate Overrides
  void UpdateCompositorWnd(const HWND aCompositorWnd,
                           const HWND aParentWnd) override;
  void SetRootLayerTreeID(const layers::LayersId& aRootLayerTreeId) override;

 private:
  RefPtr<VsyncObserver> mVsyncObserver;
  Maybe<layers::LayersId> mRootLayerTreeID;

  HWND mWnd;

  gfx::CriticalSection mPresentLock;

  // Transparency handling.
  mozilla::Atomic<uint32_t, MemoryOrdering::Relaxed> mTransparencyMode;

  // Visibility handling.
  mozilla::Atomic<nsSizeMode, MemoryOrdering::Relaxed> mSizeMode;
  mozilla::Atomic<bool, MemoryOrdering::Relaxed> mIsFullyOccluded;

  std::unique_ptr<remote_backbuffer::Client> mRemoteBackbufferClient;
};

}  // namespace widget
}  // namespace mozilla

#endif  // widget_windows_CompositorWidgetParent_h
