/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsWindowBase_h_
#define nsWindowBase_h_

#include "mozilla/EventForwards.h"
#include "nsBaseWidget.h"
#include "nsClassHashtable.h"

#include <windows.h>
#include "touchinjection_sdk80.h"

/*
 * nsWindowBase - Base class of common methods other classes need to access
 * in both win32 and winrt window classes.
 */
class nsWindowBase : public nsBaseWidget {
 public:
  typedef mozilla::WidgetEventTime WidgetEventTime;

  /*
   * Return the HWND or null for this widget.
   */
  HWND GetWindowHandle() {
    return static_cast<HWND>(GetNativeData(NS_NATIVE_WINDOW));
  }

  /*
   * Return the parent window, if it exists.
   */
  virtual nsWindowBase* GetParentWindowBase(bool aIncludeOwner) = 0;

  /*
   * Return true if this is a top level widget.
   */
  virtual bool IsTopLevelWidget() = 0;

  /*
   * Init a standard gecko event for this widget.
   * @param aEvent the event to initialize.
   * @param aPoint message position in physical coordinates.
   */
  virtual void InitEvent(mozilla::WidgetGUIEvent& aEvent,
                         LayoutDeviceIntPoint* aPoint = nullptr) = 0;

  /*
   * Returns WidgetEventTime instance which is initialized with current message
   * time.
   */
  virtual WidgetEventTime CurrentMessageWidgetEventTime() const = 0;

  /*
   * Dispatch a gecko event for this widget.
   * Returns true if it's consumed.  Otherwise, false.
   */
  virtual bool DispatchWindowEvent(mozilla::WidgetGUIEvent* aEvent) = 0;

  /*
   * Dispatch a gecko keyboard event for this widget. This
   * is called by KeyboardLayout to dispatch gecko events.
   * Returns true if it's consumed.  Otherwise, false.
   */
  virtual bool DispatchKeyboardEvent(mozilla::WidgetKeyboardEvent* aEvent) = 0;

  /*
   * Dispatch a gecko wheel event for this widget. This
   * is called by ScrollHandler to dispatch gecko events.
   * Returns true if it's consumed.  Otherwise, false.
   */
  virtual bool DispatchWheelEvent(mozilla::WidgetWheelEvent* aEvent) = 0;

  /*
   * Dispatch a gecko content command event for this widget. This
   * is called by ScrollHandler to dispatch gecko events.
   * Returns true if it's consumed.  Otherwise, false.
   */
  virtual bool DispatchContentCommandEvent(
      mozilla::WidgetContentCommandEvent* aEvent) = 0;

  /*
   * Touch input injection apis
   */
  virtual nsresult SynthesizeNativeTouchPoint(uint32_t aPointerId,
                                              TouchPointerState aPointerState,
                                              LayoutDeviceIntPoint aPoint,
                                              double aPointerPressure,
                                              uint32_t aPointerOrientation,
                                              nsIObserver* aObserver) override;
  virtual nsresult ClearNativeTouchSequence(nsIObserver* aObserver) override;

  virtual nsresult SynthesizeNativePenInput(uint32_t aPointerId,
                                            TouchPointerState aPointerState,
                                            LayoutDeviceIntPoint aPoint,
                                            double aPressure,
                                            uint32_t aRotation, int32_t aTiltX,
                                            int32_t aTiltY, int32_t aButton,
                                            nsIObserver* aObserver) override;

  /*
   * WM_APPCOMMAND common handler.
   * Sends events via NativeKey::HandleAppCommandMessage().
   */
  virtual bool HandleAppCommandMsg(const MSG& aAppCommandMsg,
                                   LRESULT* aRetValue);

  const InputContext& InputContextRef() const { return mInputContext; }

 protected:
  virtual int32_t LogToPhys(double aValue) = 0;
  void ChangedDPI();

  static bool InitTouchInjection();
  bool InjectTouchPoint(uint32_t aId, LayoutDeviceIntPoint& aPoint,
                        POINTER_FLAGS aFlags, uint32_t aPressure = 1024,
                        uint32_t aOrientation = 90);

  class PointerInfo {
   public:
    enum class PointerType : uint8_t {
      TOUCH,
      PEN,
    };

    PointerInfo(int32_t aPointerId, LayoutDeviceIntPoint& aPoint,
                PointerType aType)
        : mPointerId(aPointerId), mPosition(aPoint), mType(aType) {}

    int32_t mPointerId;
    LayoutDeviceIntPoint mPosition;
    PointerType mType;
  };

  nsClassHashtable<nsUint32HashKey, PointerInfo> mActivePointers;
  static bool sTouchInjectInitialized;
  static InjectTouchInputPtr sInjectTouchFuncPtr;

  // This is used by SynthesizeNativeTouchPoint to maintain state between
  // multiple synthesized points, in the case where we can't call InjectTouch
  // directly.
  mozilla::UniquePtr<mozilla::MultiTouchInput> mSynthesizedTouchInput;

 protected:
  InputContext mInputContext;
};

#endif  // nsWindowBase_h_
