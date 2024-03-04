/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef NSWINDOW_H_
#define NSWINDOW_H_

#include "nsBaseWidget.h"
#include "gfxPoint.h"

#include "nsTArray.h"

@class UIWindow;
@class UIView;
@class ChildView;

namespace mozilla::widget {
class TextInputHandler;
}

class nsWindow final : public nsBaseWidget {
  typedef nsBaseWidget Inherited;

 public:
  nsWindow();

  NS_INLINE_DECL_REFCOUNTING_INHERITED(nsWindow, Inherited)

  //
  // nsIWidget
  //

  [[nodiscard]] nsresult Create(
      nsIWidget* aParent, nsNativeWidget aNativeParent,
      const LayoutDeviceIntRect& aRect,
      mozilla::widget::InitData* aInitData = nullptr) override;
  void Destroy() override;
  void Show(bool aState) override;
  void Enable(bool aState) override {}
  bool IsEnabled() const override { return true; }
  bool IsVisible() const override { return mVisible; }
  void SetFocus(Raise, mozilla::dom::CallerType aCallerType) override;
  LayoutDeviceIntPoint WidgetToScreenOffset() override;

  void SetBackgroundColor(const nscolor& aColor) override;
  void* GetNativeData(uint32_t aDataType) override;

  void Move(double aX, double aY) override;
  nsSizeMode SizeMode() override { return mSizeMode; }
  void SetSizeMode(nsSizeMode aMode) override;
  void EnteredFullScreen(bool aFullScreen);
  void Resize(double aWidth, double aHeight, bool aRepaint) override;
  void Resize(double aX, double aY, double aWidth, double aHeight,
              bool aRepaint) override;
  LayoutDeviceIntRect GetScreenBounds() override;
  void ReportMoveEvent();
  void ReportSizeEvent();
  void ReportSizeModeEvent(nsSizeMode aMode);

  CGFloat BackingScaleFactor();
  void BackingScaleFactorChanged();
  float GetDPI() override {
    // XXX: terrible
    return 326.0f;
  }
  double GetDefaultScaleInternal() override { return BackingScaleFactor(); }
  int32_t RoundsWidgetCoordinatesTo() override;

  nsresult SetTitle(const nsAString& aTitle) override { return NS_OK; }

  void Invalidate(const LayoutDeviceIntRect& aRect) override;
  nsresult DispatchEvent(mozilla::WidgetGUIEvent* aEvent,
                         nsEventStatus& aStatus) override;

  void WillPaintWindow();
  bool PaintWindow(LayoutDeviceIntRegion aRegion);

  bool HasModalDescendents() { return false; }

  // virtual nsresult
  // NotifyIME(const IMENotification& aIMENotification) override;
  void SetInputContext(const InputContext& aContext,
                       const InputContextAction& aAction) override;
  InputContext GetInputContext() override;
  TextEventDispatcherListener* GetNativeTextEventDispatcherListener() override;

  mozilla::widget::TextInputHandler* GetTextInputHandler() const {
    return mTextInputHandler;
  }
  bool IsVirtualKeyboardDisabled() const;

  /*
  virtual bool ExecuteNativeKeyBinding(
                      NativeKeyBindingsType aType,
                      const mozilla::WidgetKeyboardEvent& aEvent,
                      DoCommandCallback aCallback,
                      void* aCallbackData) override;
  */

 protected:
  virtual ~nsWindow();
  void BringToFront();
  nsWindow* FindTopLevel();
  bool IsTopLevel();
  nsresult GetCurrentOffset(uint32_t& aOffset, uint32_t& aLength);
  nsresult DeleteRange(int aOffset, int aLen);

  void TearDownView();

  ChildView* mNativeView;
  bool mVisible;
  nsSizeMode mSizeMode;
  nsTArray<nsWindow*> mChildren;
  nsWindow* mParent;

  mozilla::widget::InputContext mInputContext;
  RefPtr<mozilla::widget::TextInputHandler> mTextInputHandler;

  void OnSizeChanged(const mozilla::gfx::IntSize& aSize);

  static void DumpWindows();
  static void DumpWindows(const nsTArray<nsWindow*>& wins, int indent = 0);
  static void LogWindow(nsWindow* win, int index, int indent);
};

#endif /* NSWINDOW_H_ */
