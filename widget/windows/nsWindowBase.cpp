/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsWindowBase.h"

#include "mozilla/MiscEvents.h"
#include "mozilla/PresShell.h"
#include "mozilla/StaticPrefs_apz.h"
#include "KeyboardLayout.h"
#include "WinUtils.h"
#include "npapi.h"

using namespace mozilla;
using namespace mozilla::widget;

static const wchar_t kUser32LibName[] = L"user32.dll";
bool nsWindowBase::sTouchInjectInitialized = false;
InjectTouchInputPtr nsWindowBase::sInjectTouchFuncPtr;

// static
bool nsWindowBase::InitTouchInjection() {
  if (!sTouchInjectInitialized) {
    // Initialize touch injection on the first call
    HMODULE hMod = LoadLibraryW(kUser32LibName);
    if (!hMod) {
      return false;
    }

    InitializeTouchInjectionPtr func =
        (InitializeTouchInjectionPtr)GetProcAddress(hMod,
                                                    "InitializeTouchInjection");
    if (!func) {
      WinUtils::Log("InitializeTouchInjection not available.");
      return false;
    }

    if (!func(TOUCH_INJECT_MAX_POINTS, TOUCH_FEEDBACK_DEFAULT)) {
      WinUtils::Log("InitializeTouchInjection failure. GetLastError=%d",
                    GetLastError());
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

bool nsWindowBase::InjectTouchPoint(uint32_t aId, LayoutDeviceIntPoint& aPoint,
                                    POINTER_FLAGS aFlags, uint32_t aPressure,
                                    uint32_t aOrientation) {
  if (aId > TOUCH_INJECT_MAX_POINTS) {
    WinUtils::Log("Pointer ID exceeds maximum. See TOUCH_INJECT_MAX_POINTS.");
    return false;
  }

  POINTER_TOUCH_INFO info{};

  info.touchFlags = TOUCH_FLAG_NONE;
  info.touchMask =
      TOUCH_MASK_CONTACTAREA | TOUCH_MASK_ORIENTATION | TOUCH_MASK_PRESSURE;
  info.pressure = aPressure;
  info.orientation = aOrientation;

  info.pointerInfo.pointerFlags = aFlags;
  info.pointerInfo.pointerType = PT_TOUCH;
  info.pointerInfo.pointerId = aId;
  info.pointerInfo.ptPixelLocation.x = aPoint.x;
  info.pointerInfo.ptPixelLocation.y = aPoint.y;

  info.rcContact.top = info.pointerInfo.ptPixelLocation.y - 2;
  info.rcContact.bottom = info.pointerInfo.ptPixelLocation.y + 2;
  info.rcContact.left = info.pointerInfo.ptPixelLocation.x - 2;
  info.rcContact.right = info.pointerInfo.ptPixelLocation.x + 2;

  for (int i = 0; i < 3; i++) {
    if (sInjectTouchFuncPtr(1, &info)) {
      break;
    }
    DWORD error = GetLastError();
    if (error == ERROR_NOT_READY && i < 2) {
      // We sent it too quickly after the previous injection (see bug 1535140
      // comment 10). On the first loop iteration we just yield (via Sleep(0))
      // and try again. If it happens again on the second loop iteration we
      // explicitly Sleep(1) and try again. If that doesn't work either we just
      // error out.
      ::Sleep(i);
      continue;
    }
    WinUtils::Log("InjectTouchInput failure. GetLastError=%d", error);
    return false;
  }
  return true;
}

void nsWindowBase::ChangedDPI() {
  if (mWidgetListener) {
    if (PresShell* presShell = mWidgetListener->GetPresShell()) {
      presShell->BackingScaleFactorChanged();
    }
  }
}

static Result<POINTER_FLAGS, nsresult> PointerStateToFlag(
    nsWindowBase::TouchPointerState aPointerState, bool isUpdate) {
  bool hover = aPointerState & nsWindowBase::TOUCH_HOVER;
  bool contact = aPointerState & nsWindowBase::TOUCH_CONTACT;
  bool remove = aPointerState & nsWindowBase::TOUCH_REMOVE;
  bool cancel = aPointerState & nsWindowBase::TOUCH_CANCEL;

  POINTER_FLAGS flags;
  if (isUpdate) {
    // We know about this pointer, send an update
    flags = POINTER_FLAG_UPDATE;
    if (hover) {
      flags |= POINTER_FLAG_INRANGE;
    } else if (contact) {
      flags |= POINTER_FLAG_INCONTACT | POINTER_FLAG_INRANGE;
    } else if (remove) {
      flags = POINTER_FLAG_UP;
    }

    if (cancel) {
      flags |= POINTER_FLAG_CANCELED;
    }
  } else {
    // Missing init state, error out
    if (remove || cancel) {
      return Err(NS_ERROR_INVALID_ARG);
    }

    // Create a new pointer
    flags = POINTER_FLAG_INRANGE;
    if (contact) {
      flags |= POINTER_FLAG_INCONTACT | POINTER_FLAG_DOWN;
    }
  }
  return flags;
}

nsresult nsWindowBase::SynthesizeNativeTouchPoint(
    uint32_t aPointerId, nsIWidget::TouchPointerState aPointerState,
    LayoutDeviceIntPoint aPoint, double aPointerPressure,
    uint32_t aPointerOrientation, nsIObserver* aObserver) {
  AutoObserverNotifier notifier(aObserver, "touchpoint");

  if (StaticPrefs::apz_test_fails_with_native_injection() ||
      !InitTouchInjection()) {
    // If we don't have touch injection from the OS, or if we are running a test
    // that cannot properly inject events to satisfy the OS requirements (see
    // bug 1313170)  we can just fake it and synthesize the events from here.
    MOZ_ASSERT(NS_IsMainThread());
    if (aPointerState == TOUCH_HOVER) {
      return NS_ERROR_UNEXPECTED;
    }

    if (!mSynthesizedTouchInput) {
      mSynthesizedTouchInput = MakeUnique<MultiTouchInput>();
    }

    WidgetEventTime time = CurrentMessageWidgetEventTime();
    LayoutDeviceIntPoint pointInWindow = aPoint - WidgetToScreenOffset();
    MultiTouchInput inputToDispatch = UpdateSynthesizedTouchState(
        mSynthesizedTouchInput.get(), time.mTime, time.mTimeStamp, aPointerId,
        aPointerState, pointInWindow, aPointerPressure, aPointerOrientation);
    DispatchTouchInput(inputToDispatch);
    return NS_OK;
  }

  // win api expects a value from 0 to 1024. aPointerPressure is a value
  // from 0.0 to 1.0.
  uint32_t pressure = (uint32_t)ceil(aPointerPressure * 1024);

  // If we already know about this pointer id get it's record
  return mActivePointers.WithEntryHandle(aPointerId, [&](auto&& entry) {
    POINTER_FLAGS flags;
    // Can't use MOZ_TRY_VAR because it confuses WithEntryHandle
    auto result = PointerStateToFlag(aPointerState, !!entry);
    if (result.isOk()) {
      flags = result.unwrap();
    } else {
      return result.unwrapErr();
    }

    if (!entry) {
      entry.Insert(MakeUnique<PointerInfo>(aPointerId, aPoint,
                                           PointerInfo::PointerType::TOUCH));
    } else {
      if (entry.Data()->mType != PointerInfo::PointerType::TOUCH) {
        return NS_ERROR_UNEXPECTED;
      }
      if (aPointerState & TOUCH_REMOVE) {
        // Remove the pointer from our tracking list. This is UniquePtr wrapped,
        // so shouldn't leak.
        entry.Remove();
      }
    }

    return !InjectTouchPoint(aPointerId, aPoint, flags, pressure,
                             aPointerOrientation)
               ? NS_ERROR_UNEXPECTED
               : NS_OK;
  });
}

nsresult nsWindowBase::ClearNativeTouchSequence(nsIObserver* aObserver) {
  AutoObserverNotifier notifier(aObserver, "cleartouch");
  if (!sTouchInjectInitialized) {
    return NS_OK;
  }

  // cancel all input points
  for (auto iter = mActivePointers.Iter(); !iter.Done(); iter.Next()) {
    auto* info = iter.UserData();
    if (info->mType != PointerInfo::PointerType::TOUCH) {
      continue;
    }
    InjectTouchPoint(info->mPointerId, info->mPosition, POINTER_FLAG_CANCELED);
    iter.Remove();
  }

  nsBaseWidget::ClearNativeTouchSequence(nullptr);

  return NS_OK;
}

#if !defined(NTDDI_WIN10_RS5) || (NTDDI_VERSION < NTDDI_WIN10_RS5)
static CreateSyntheticPointerDevicePtr CreateSyntheticPointerDevice;
static DestroySyntheticPointerDevicePtr DestroySyntheticPointerDevice;
static InjectSyntheticPointerInputPtr InjectSyntheticPointerInput;
#endif
static HSYNTHETICPOINTERDEVICE sSyntheticPenDevice;

static bool InitPenInjection() {
  if (sSyntheticPenDevice) {
    return true;
  }
#if !defined(NTDDI_WIN10_RS5) || (NTDDI_VERSION < NTDDI_WIN10_RS5)
  HMODULE hMod = LoadLibraryW(kUser32LibName);
  if (!hMod) {
    return false;
  }
  CreateSyntheticPointerDevice =
      (CreateSyntheticPointerDevicePtr)GetProcAddress(
          hMod, "CreateSyntheticPointerDevice");
  if (!CreateSyntheticPointerDevice) {
    WinUtils::Log("CreateSyntheticPointerDevice not available.");
    return false;
  }
  DestroySyntheticPointerDevice =
      (DestroySyntheticPointerDevicePtr)GetProcAddress(
          hMod, "DestroySyntheticPointerDevice");
  if (!DestroySyntheticPointerDevice) {
    WinUtils::Log("DestroySyntheticPointerDevice not available.");
    return false;
  }
  InjectSyntheticPointerInput = (InjectSyntheticPointerInputPtr)GetProcAddress(
      hMod, "InjectSyntheticPointerInput");
  if (!InjectSyntheticPointerInput) {
    WinUtils::Log("InjectSyntheticPointerInput not available.");
    return false;
  }
#endif
  sSyntheticPenDevice =
      CreateSyntheticPointerDevice(PT_PEN, 1, POINTER_FEEDBACK_DEFAULT);
  return !!sSyntheticPenDevice;
}

nsresult nsWindowBase::SynthesizeNativePenInput(
    uint32_t aPointerId, nsIWidget::TouchPointerState aPointerState,
    LayoutDeviceIntPoint aPoint, double aPressure, uint32_t aRotation,
    int32_t aTiltX, int32_t aTiltY, nsIObserver* aObserver) {
  AutoObserverNotifier notifier(aObserver, "peninput");
  if (!InitPenInjection()) {
    return NS_ERROR_UNEXPECTED;
  }

  // win api expects a value from 0 to 1024. aPointerPressure is a value
  // from 0.0 to 1.0.
  uint32_t pressure = (uint32_t)ceil(aPressure * 1024);

  // If we already know about this pointer id get it's record
  return mActivePointers.WithEntryHandle(aPointerId, [&](auto&& entry) {
    POINTER_FLAGS flags;
    // Can't use MOZ_TRY_VAR because it confuses WithEntryHandle
    auto result = PointerStateToFlag(aPointerState, !!entry);
    if (result.isOk()) {
      flags = result.unwrap();
    } else {
      return result.unwrapErr();
    }

    if (!entry) {
      entry.Insert(MakeUnique<PointerInfo>(aPointerId, aPoint,
                                           PointerInfo::PointerType::PEN));
    } else {
      if (entry.Data()->mType != PointerInfo::PointerType::PEN) {
        return NS_ERROR_UNEXPECTED;
      }
      if (aPointerState & TOUCH_REMOVE) {
        // Remove the pointer from our tracking list. This is UniquePtr wrapped,
        // so shouldn't leak.
        entry.Remove();
      }
    }

    POINTER_TYPE_INFO info{};

    info.type = PT_PEN;
    info.penInfo.pointerInfo.pointerType = PT_PEN;
    info.penInfo.pointerInfo.pointerFlags = flags;
    info.penInfo.pointerInfo.pointerId = aPointerId;
    info.penInfo.pointerInfo.ptPixelLocation.x = aPoint.x;
    info.penInfo.pointerInfo.ptPixelLocation.y = aPoint.y;

    info.penInfo.penFlags = PEN_FLAG_NONE;
    info.penInfo.penMask = PEN_MASK_PRESSURE | PEN_MASK_ROTATION |
                           PEN_MASK_TILT_X | PEN_MASK_TILT_Y;
    info.penInfo.pressure = pressure;
    info.penInfo.rotation = aRotation;
    info.penInfo.tiltX = aTiltX;
    info.penInfo.tiltY = aTiltY;

    return InjectSyntheticPointerInput(sSyntheticPenDevice, &info, 1)
               ? NS_OK
               : NS_ERROR_UNEXPECTED;
  });
};

bool nsWindowBase::HandleAppCommandMsg(const MSG& aAppCommandMsg,
                                       LRESULT* aRetValue) {
  ModifierKeyState modKeyState;
  NativeKey nativeKey(this, aAppCommandMsg, modKeyState);
  bool consumed = nativeKey.HandleAppCommandMessage();
  *aRetValue = consumed ? 1 : 0;
  return consumed;
}
