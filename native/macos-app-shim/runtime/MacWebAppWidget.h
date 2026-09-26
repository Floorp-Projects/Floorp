/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#ifndef mozilla_widget_MacWebAppWidget_h
#define mozilla_widget_MacWebAppWidget_h

#include "nsIWidget.h"
#include "nsIObserver.h"
#include "mozilla/layers/NativeLayerCA.h"

namespace mozilla::layers {
class NativeLayerRootRemoteMacParent;
}
namespace mozilla::widget {
class MacWebAppInput;
class MacWebAppPresentation;
class PlatformCompositorWidgetDelegate;

class MacWebAppWidget final : public nsIWidget, public nsIObserver {
 public:
  explicit MacWebAppWidget(const nsACString& aAppId);
  NS_DECL_ISUPPORTS_INHERITED
  NS_DECL_NSIOBSERVER
  nsresult Create(nsIWidget*, const LayoutDeviceIntRect&,
                  const widget::InitData&) override;
  using nsIWidget::Create;
  void Destroy() override;
  void* GetNativeData(uint32_t aType) override {
    return aType == NS_RAW_NATIVE_IME_CONTEXT ? this : nullptr;
  }
  bool IsMacWebAppWidget() const override { return true; }
  nsCString GetMacWebAppId() const { return mAppId; }
  already_AddRefed<nsIWidget> CreateMacWebAppWindow();
  void Show(bool aVisible) override;
  bool IsVisible() const override { return mVisible; }
  void Move(const DesktopPoint&) override;
  void Resize(const DesktopSize&, bool) override;
  void Resize(const DesktopRect&, bool) override;
  void SetSizeMode(nsSizeMode) override;
  nsSizeMode SizeMode() override { return mSizeMode; }
  nsresult MakeFullScreen(bool) override;
  void Enable(bool) override;
  bool IsEnabled() const override { return mEnabled; }
  void SetFocus(Raise, dom::CallerType) override;
  LayoutDeviceIntRect GetBounds() override { return mBounds; }
  void Invalidate(const LayoutDeviceIntRect&) override;
  nsresult SetTitle(const nsAString&) override;
  void SetCursor(const Cursor&) override;
  LayoutDeviceIntPoint WidgetToScreenOffset() override { return mBounds.TopLeft(); }
  void SetInputContext(const InputContext&, const InputContextAction&) override;
  InputContext GetInputContext() override { return mInputContext; }
  TextEventDispatcherListener* GetNativeTextEventDispatcherListener() override;
  MOZ_CAN_RUN_SCRIPT bool GetEditCommands(NativeKeyBindingsType,
      const WidgetKeyboardEvent&, nsTArray<CommandInt>&) override;
  DesktopToLayoutDeviceScale GetDesktopToDeviceScale() const override {
    return DesktopToLayoutDeviceScale(mScale);
  }
  bool SynchronouslyRepaintOnResize() override { return false; }
  // WebRender composites video and page layers into a BGRA IOSurface before
  // exporting it. This avoids sending platform-only HDR / YUV overlays to the
  // Shim while preserving the GPU compositor and zero-copy surface transport.
  bool WidgetTypeSupportsNativeCompositing() override { return false; }
  layers::NativeLayerRoot* GetNativeLayerRoot() override { return mLayerRoot; }
  void CreateCompositor(int, int) override;
  void DestroyCompositor() override;
  void GetCompositorWidgetInitData(CompositorWidgetInitData*) override;
  void SetCompositorWidgetDelegate(CompositorWidgetDelegate*) override;

 protected:
  ~MacWebAppWidget() override;
  double GetDefaultScaleInternal() override { return mScale; }

 private:
  void NotifySize();
  void UpdateGeometry(const DesktopRect&);
  bool CreateNativeWindow();
  bool RestoreNativeWindow();
  void SuspendNativeWindow();
  nsCString mAppId;
  nsString mTitle;
  uint32_t mWindowId;
  uint32_t mGeometryRequestId = 0;
  bool mCreated = false;
  bool mVisible = false;
  bool mRequestedVisible = false;
  bool mReconnectPending = false;
  bool mRestoring = false;
  bool mRestoreFocus = false;
  bool mEnabled = true;
  bool mObserving = false;
  bool mKey = false;
  double mScale = 1;
  nsSizeMode mSizeMode = nsSizeMode_Normal;
  LayoutDeviceIntRect mBounds;
  InputContext mInputContext;
  RefPtr<layers::NativeLayerRootCA> mLayerRoot;
  RefPtr<layers::NativeLayerRootRemoteMacParent> mRemoteRoot;
  RefPtr<MacWebAppPresentation> mPresentation;
  RefPtr<MacWebAppInput> mInput;
  PlatformCompositorWidgetDelegate* mCompositorDelegate = nullptr;
};
}  // namespace mozilla::widget
#endif
