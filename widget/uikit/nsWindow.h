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

class nsWindow final : public nsBaseWidget {
  typedef nsBaseWidget Inherited;

 public:
  nsWindow();

  NS_INLINE_DECL_REFCOUNTING_INHERITED(nsWindow, Inherited)

  //
  // nsIWidget
  //

  [[nodiscard]] virtual nsresult
      Create(nsIWidget* aParent, nsNativeWidget aNativeParent, const LayoutDeviceIntRect& aRect,
             nsWidgetInitData* aInitData = nullptr) override;
  virtual void Destroy() override;
  virtual void Show(bool aState) override;
  virtual void Enable(bool aState) override {}
  virtual bool IsEnabled() const override { return true; }
  virtual bool IsVisible() const override { return mVisible; }
  virtual void SetFocus(Raise, mozilla::dom::CallerType aCallerType) override;
  virtual LayoutDeviceIntPoint WidgetToScreenOffset() override;

  virtual void SetBackgroundColor(const nscolor& aColor) override;
  virtual void* GetNativeData(uint32_t aDataType) override;

  virtual void Move(double aX, double aY) override;
  virtual void SetSizeMode(nsSizeMode aMode) override;
  void EnteredFullScreen(bool aFullScreen);
  virtual void Resize(double aWidth, double aHeight, bool aRepaint) override;
  virtual void Resize(double aX, double aY, double aWidth, double aHeight, bool aRepaint) override;
  virtual LayoutDeviceIntRect GetScreenBounds() override;
  void ReportMoveEvent();
  void ReportSizeEvent();
  void ReportSizeModeEvent(nsSizeMode aMode);

  CGFloat BackingScaleFactor();
  void BackingScaleFactorChanged();
  virtual float GetDPI() override {
    // XXX: terrible
    return 326.0f;
  }
  virtual double GetDefaultScaleInternal() override { return BackingScaleFactor(); }
  virtual int32_t RoundsWidgetCoordinatesTo() override;

  virtual nsresult SetTitle(const nsAString& aTitle) override { return NS_OK; }

  virtual void Invalidate(const LayoutDeviceIntRect& aRect) override;
  virtual nsresult ConfigureChildren(const nsTArray<Configuration>& aConfigurations) override;
  virtual nsresult DispatchEvent(mozilla::WidgetGUIEvent* aEvent, nsEventStatus& aStatus) override;

  void WillPaintWindow();
  bool PaintWindow(LayoutDeviceIntRegion aRegion);

  bool HasModalDescendents() { return false; }

  // virtual nsresult
  // NotifyIME(const IMENotification& aIMENotification) override;
  virtual void SetInputContext(const InputContext& aContext, const InputContextAction& aAction);
  virtual InputContext GetInputContext();
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
  nsTArray<nsWindow*> mChildren;
  nsWindow* mParent;
  InputContext mInputContext;

  void OnSizeChanged(const mozilla::gfx::IntSize& aSize);

  static void DumpWindows();
  static void DumpWindows(const nsTArray<nsWindow*>& wins, int indent = 0);
  static void LogWindow(nsWindow* win, int index, int indent);
};

#endif /* NSWINDOW_H_ */
