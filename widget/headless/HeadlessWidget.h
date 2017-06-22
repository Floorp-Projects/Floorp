/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */


#ifndef HEADLESSWIDGET_H
#define HEADLESSWIDGET_H

#include "mozilla/widget/InProcessCompositorWidget.h"
#include "nsBaseWidget.h"

namespace mozilla {
namespace widget {

class HeadlessWidget : public nsBaseWidget
{
public:
  HeadlessWidget() {}

  NS_DECL_ISUPPORTS_INHERITED

  void* GetNativeData(uint32_t aDataType) override
  {
    // Headless widgets have no native data.
    return nullptr;
  }

  virtual nsresult Create(nsIWidget* aParent,
                          nsNativeWidget aNativeParent,
                          const LayoutDeviceIntRect& aRect,
                          nsWidgetInitData* aInitData = nullptr) override;
  using nsBaseWidget::Create; // for Create signature not overridden here
  virtual already_AddRefed<nsIWidget> CreateChild(const LayoutDeviceIntRect& aRect,
                                                  nsWidgetInitData* aInitData = nullptr,
                                                  bool aForceUseIWidgetParent = false) override;

  virtual void Show(bool aState) override;
  virtual bool IsVisible() const override;
  virtual void Move(double aX, double aY) override;
  virtual void Resize(double aWidth,
                      double aHeight,
                      bool   aRepaint) override;
  virtual void Resize(double aX,
                      double aY,
                      double aWidth,
                      double aHeight,
                      bool   aRepaint) override;
  virtual void SetSizeMode(nsSizeMode aMode) override;
  virtual nsresult MakeFullScreen(bool aFullScreen,
                                  nsIScreen* aTargetScreen = nullptr) override;
  virtual void Enable(bool aState) override;
  virtual bool IsEnabled() const override;
  virtual nsresult SetFocus(bool aRaise) override;
  virtual nsresult ConfigureChildren(const nsTArray<Configuration>& aConfigurations) override
  {
    MOZ_ASSERT_UNREACHABLE("Headless widgets do not support configuring children.");
    return NS_ERROR_FAILURE;
  }
  virtual void Invalidate(const LayoutDeviceIntRect& aRect) override
  {
    // TODO: see if we need to do anything here.
  }
  virtual nsresult SetTitle(const nsAString& title) override {
    // Headless widgets have no title, so just ignore it.
    return NS_OK;
  }
  virtual LayoutDeviceIntPoint WidgetToScreenOffset() override;
  virtual void SetInputContext(const InputContext& aContext,
                               const InputContextAction& aAction) override
  {
    mInputContext = aContext;
  }
  virtual InputContext GetInputContext() override
  {
    return mInputContext;
  }

  virtual LayerManager*
  GetLayerManager(PLayerTransactionChild* aShadowManager = nullptr,
                  LayersBackend aBackendHint = mozilla::layers::LayersBackend::LAYERS_NONE,
                  LayerManagerPersistence aPersistence = LAYER_MANAGER_CURRENT) override;

  virtual nsresult DispatchEvent(WidgetGUIEvent* aEvent,
                                 nsEventStatus& aStatus) override;

private:
  ~HeadlessWidget() {}
  bool mEnabled;
  bool mVisible;
  // The size mode before entering fullscreen mode.
  nsSizeMode mLastSizeMode;
  InputContext mInputContext;
  // In headless there is no window manager to track window bounds
  // across size mode changes, so we must track it to emulate.
  LayoutDeviceIntRect mRestoreBounds;
  void SendSetZLevelEvent();
};

} // namespace widget
} // namespace mozilla

#endif
