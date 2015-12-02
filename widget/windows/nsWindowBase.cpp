/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsWindowBase.h"

#include "mozilla/MiscEvents.h"
#include "KeyboardLayout.h"
#include "WinUtils.h"
#include "npapi.h"

using namespace mozilla;
using namespace mozilla::widget;

static const wchar_t kUser32LibName[] =  L"user32.dll";
bool nsWindowBase::sTouchInjectInitialized = false;
InjectTouchInputPtr nsWindowBase::sInjectTouchFuncPtr;

bool
nsWindowBase::DispatchPluginEvent(const MSG& aMsg)
{
  if (!PluginHasFocus()) {
    return false;
  }
  WidgetPluginEvent pluginEvent(true, ePluginInputEvent, this);
  LayoutDeviceIntPoint point(0, 0);
  InitEvent(pluginEvent, &point);
  NPEvent npEvent;
  npEvent.event = aMsg.message;
  npEvent.wParam = aMsg.wParam;
  npEvent.lParam = aMsg.lParam;
  pluginEvent.mPluginEvent.Copy(npEvent);
  pluginEvent.retargetToFocusedDocument = true;
  return DispatchWindowEvent(&pluginEvent);
}

// static
bool
nsWindowBase::InitTouchInjection()
{
  if (!sTouchInjectInitialized) {
    // Initialize touch injection on the first call
    HMODULE hMod = LoadLibraryW(kUser32LibName);
    if (!hMod) {
      return false;
    }

    InitializeTouchInjectionPtr func =
      (InitializeTouchInjectionPtr)GetProcAddress(hMod, "InitializeTouchInjection");
    if (!func) {
      WinUtils::Log("InitializeTouchInjection not available.");
      return false;
    }

    if (!func(TOUCH_INJECT_MAX_POINTS, TOUCH_FEEDBACK_DEFAULT)) {
      WinUtils::Log("InitializeTouchInjection failure. GetLastError=%d", GetLastError());
      return false;
    }

    sInjectTouchFuncPtr =
      (InjectTouchInputPtr)GetProcAddress(hMod, "InjectTouchInput");
    if (!sInjectTouchFuncPtr) {
      WinUtils::Log("InjectTouchInput not available.");
      return false;
    }
    sTouchInjectInitialized = true;
  }
  return true;
}

bool
nsWindowBase::InjectTouchPoint(uint32_t aId, ScreenIntPoint& aPointerScreenPoint,
                               POINTER_FLAGS aFlags, uint32_t aPressure,
                               uint32_t aOrientation)
{
  if (aId > TOUCH_INJECT_MAX_POINTS) {
    WinUtils::Log("Pointer ID exceeds maximum. See TOUCH_INJECT_MAX_POINTS.");
    return false;
  }

  POINTER_TOUCH_INFO info;
  memset(&info, 0, sizeof(POINTER_TOUCH_INFO));

  info.touchFlags = TOUCH_FLAG_NONE;
  info.touchMask = TOUCH_MASK_CONTACTAREA|TOUCH_MASK_ORIENTATION|TOUCH_MASK_PRESSURE;
  info.pressure = aPressure;
  info.orientation = aOrientation;
  
  info.pointerInfo.pointerFlags = aFlags;
  info.pointerInfo.pointerType =  PT_TOUCH;
  info.pointerInfo.pointerId = aId;
  info.pointerInfo.ptPixelLocation.x = LogToPhys(aPointerScreenPoint.x);
  info.pointerInfo.ptPixelLocation.y = LogToPhys(aPointerScreenPoint.y);

  info.rcContact.top = info.pointerInfo.ptPixelLocation.y - 2;
  info.rcContact.bottom = info.pointerInfo.ptPixelLocation.y + 2;
  info.rcContact.left = info.pointerInfo.ptPixelLocation.x - 2;
  info.rcContact.right = info.pointerInfo.ptPixelLocation.x + 2;
  
  if (!sInjectTouchFuncPtr(1, &info)) {
    WinUtils::Log("InjectTouchInput failure. GetLastError=%d", GetLastError());
    return false;
  }
  return true;
}

nsresult
nsWindowBase::SynthesizeNativeTouchPoint(uint32_t aPointerId,
                                         nsIWidget::TouchPointerState aPointerState,
                                         ScreenIntPoint aPointerScreenPoint,
                                         double aPointerPressure,
                                         uint32_t aPointerOrientation,
                                         nsIObserver* aObserver)
{
  AutoObserverNotifier notifier(aObserver, "touchpoint");

  if (!InitTouchInjection()) {
    return NS_ERROR_NOT_IMPLEMENTED;
  }

  bool hover = aPointerState & TOUCH_HOVER;
  bool contact = aPointerState & TOUCH_CONTACT;
  bool remove = aPointerState & TOUCH_REMOVE;
  bool cancel = aPointerState & TOUCH_CANCEL;

  // win api expects a value from 0 to 1024. aPointerPressure is a value
  // from 0.0 to 1.0.
  uint32_t pressure = (uint32_t)ceil(aPointerPressure * 1024);

  // If we already know about this pointer id get it's record
  PointerInfo* info = mActivePointers.Get(aPointerId);

  // We know about this pointer, send an update
  if (info) {
    POINTER_FLAGS flags = POINTER_FLAG_UPDATE;
    if (hover) {
      flags |= POINTER_FLAG_INRANGE;
    } else if (contact) {
      flags |= POINTER_FLAG_INCONTACT|POINTER_FLAG_INRANGE;
    } else if (remove) {
      flags = POINTER_FLAG_UP;
      // Remove the pointer from our tracking list. This is nsAutPtr wrapped,
      // so shouldn't leak.
      mActivePointers.Remove(aPointerId);
    }

    if (cancel) {
      flags |= POINTER_FLAG_CANCELED;
    }

    return !InjectTouchPoint(aPointerId, aPointerScreenPoint, flags,
                             pressure, aPointerOrientation) ?
      NS_ERROR_UNEXPECTED : NS_OK;
  }

  // Missing init state, error out
  if (remove || cancel) {
    return NS_ERROR_INVALID_ARG;
  }

  // Create a new pointer
  info = new PointerInfo(aPointerId, aPointerScreenPoint);

  POINTER_FLAGS flags = POINTER_FLAG_INRANGE;
  if (contact) {
    flags |= POINTER_FLAG_INCONTACT|POINTER_FLAG_DOWN;
  }

  mActivePointers.Put(aPointerId, info);
  return !InjectTouchPoint(aPointerId, aPointerScreenPoint, flags,
                           pressure, aPointerOrientation) ?
    NS_ERROR_UNEXPECTED : NS_OK;
}

nsresult
nsWindowBase::ClearNativeTouchSequence(nsIObserver* aObserver)
{
  AutoObserverNotifier notifier(aObserver, "cleartouch");
  if (!sTouchInjectInitialized) {
    return NS_OK;
  }

  // cancel all input points
  for (auto iter = mActivePointers.Iter(); !iter.Done(); iter.Next()) {
    nsAutoPtr<PointerInfo>& info = iter.Data();
    InjectTouchPoint(info.get()->mPointerId, info.get()->mPosition,
                     POINTER_FLAG_CANCELED);
    iter.Remove();
  }

  nsBaseWidget::ClearNativeTouchSequence(nullptr);

  return NS_OK;
}

bool
nsWindowBase::HandleAppCommandMsg(const MSG& aAppCommandMsg,
                                  LRESULT *aRetValue)
{
  ModifierKeyState modKeyState;
  NativeKey nativeKey(this, aAppCommandMsg, modKeyState);
  bool consumed = nativeKey.HandleAppCommandMessage();
  *aRetValue = consumed ? 1 : 0;
  return consumed;
}
