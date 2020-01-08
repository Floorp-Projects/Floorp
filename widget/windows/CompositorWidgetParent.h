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

class CompositorWidgetParent final : public PCompositorWidgetParent,
                                     public WinCompositorWidget {
 public:
  explicit CompositorWidgetParent(const CompositorWidgetInitData& aInitData,
                                  const layers::CompositorOptions& aOptions);
  ~CompositorWidgetParent() override;

  bool PreRender(WidgetRenderingContext*) override;
  void PostRender(WidgetRenderingContext*) override;
  already_AddRefed<gfx::DrawTarget> StartRemoteDrawing() override;
  void EndRemoteDrawing() override;
  bool NeedsToDeferEndRemoteDrawing() override;
  LayoutDeviceIntSize GetClientSize() override;
  already_AddRefed<gfx::DrawTarget> GetBackBufferDrawTarget(
      gfx::DrawTarget* aScreenTarget, const gfx::IntRect& aRect,
      bool* aOutIsCleared) override;
  already_AddRefed<gfx::SourceSurface> EndBackBufferDrawing() override;
  bool InitCompositor(layers::Compositor* aCompositor) override;
  bool IsHidden() const override;

  bool HasGlass() const override;

  mozilla::ipc::IPCResult RecvEnterPresentLock() override;
  mozilla::ipc::IPCResult RecvLeavePresentLock() override;
  mozilla::ipc::IPCResult RecvUpdateTransparency(
      const nsTransparencyMode& aMode) override;
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
  bool RedrawTransparentWindow();

  // Ensure that a transparent surface exists, then return it.
  RefPtr<gfxASurface> EnsureTransparentSurface();

  HDC GetWindowSurface();
  void FreeWindowSurface(HDC dc);

  void CreateTransparentSurface(const gfx::IntSize& aSize);

  RefPtr<VsyncObserver> mVsyncObserver;
  Maybe<layers::LayersId> mRootLayerTreeID;

  HWND mWnd;

  gfx::CriticalSection mPresentLock;

  // Transparency handling.
  mozilla::Mutex mTransparentSurfaceLock;
  mozilla::Atomic<nsTransparencyMode, MemoryOrdering::Relaxed>
      mTransparencyMode;
  RefPtr<gfxASurface> mTransparentSurface;
  HDC mMemoryDC;
  HDC mCompositeDC;

  // Locked back buffer of BasicCompositor
  uint8_t* mLockedBackBufferData;

  bool mNotDeferEndRemoteDrawing;
};

}  // namespace widget
}  // namespace mozilla

#endif  // widget_windows_CompositorWidgetParent_h
