/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef GTEST_MockWinWidget_H
#define GTEST_MockWinWidget_H

#include <windows.h>

#include "nsBaseWidget.h"
#include "Units.h"

class MockWinWidget : public nsBaseWidget {
 public:
  static RefPtr<MockWinWidget> Create(DWORD aStyle, DWORD aExStyle,
                                      const LayoutDeviceIntRect& aRect);

  NS_DECL_ISUPPORTS_INHERITED

  void NotifyOcclusionState(mozilla::widget::OcclusionState aState) override;

  void SetExpectation(mozilla::widget::OcclusionState aExpectation) {
    mExpectation = aExpectation;
  }

  bool IsExpectingCall() const { return mExpectation != mCurrentState; }

  HWND GetWnd() { return mWnd; }

  nsSizeMode SizeMode() override;
  void SetSizeMode(nsSizeMode aMode) override {}

  void* GetNativeData(uint32_t aDataType) override { return nullptr; }

  virtual nsresult Create(nsIWidget* aParent, nsNativeWidget aNativeParent,
                          const LayoutDeviceIntRect& aRect,
                          InitData* aInitData = nullptr) override {
    return NS_OK;
  }
  virtual nsresult Create(nsIWidget* aParent, nsNativeWidget aNativeParent,
                          const DesktopIntRect& aRect,
                          InitData* aInitData = nullptr) override {
    return NS_OK;
  }
  virtual void Show(bool aState) override {}
  virtual bool IsVisible() const override { return true; }
  virtual void Move(double aX, double aY) override {}
  virtual void Resize(double aWidth, double aHeight, bool aRepaint) override {}
  virtual void Resize(double aX, double aY, double aWidth, double aHeight,
                      bool aRepaint) override {}

  virtual void Enable(bool aState) override {}
  virtual bool IsEnabled() const override { return true; }
  virtual void SetFocus(Raise, mozilla::dom::CallerType aCallerType) override {}
  virtual void Invalidate(const LayoutDeviceIntRect& aRect) override {}
  virtual nsresult SetTitle(const nsAString& title) override { return NS_OK; }
  virtual LayoutDeviceIntPoint WidgetToScreenOffset() override {
    return LayoutDeviceIntPoint(0, 0);
  }
  virtual nsresult DispatchEvent(mozilla::WidgetGUIEvent* aEvent,
                                 nsEventStatus& aStatus) override {
    return NS_OK;
  }
  virtual void SetInputContext(const InputContext& aContext,
                               const InputContextAction& aAction) override {}
  virtual InputContext GetInputContext() override { abort(); }

 private:
  MockWinWidget();
  ~MockWinWidget();

  bool Initialize(DWORD aStyle, DWORD aExStyle,
                  const LayoutDeviceIntRect& aRect);

  HWND mWnd = 0;

  mozilla::widget::OcclusionState mExpectation =
      mozilla::widget::OcclusionState::UNKNOWN;
  mozilla::widget::OcclusionState mCurrentState =
      mozilla::widget::OcclusionState::UNKNOWN;
};

#endif
