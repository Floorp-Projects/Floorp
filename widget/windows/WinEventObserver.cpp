/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include <windows.h>
#include <winuser.h>
#include <wtsapi32.h>

#include "WinEventObserver.h"

#include "mozilla/ClearOnShutdown.h"
#include "mozilla/Logging.h"
#include "mozilla/StaticPtr.h"
#include "nsHashtablesFwd.h"
#include "nsdefs.h"

namespace mozilla::widget {

LazyLogModule gWinEventObserverLog("WinEventObserver");
#define LOG(...) MOZ_LOG(gWinEventObserverLog, LogLevel::Info, (__VA_ARGS__))

// static
StaticRefPtr<WinEventHub> WinEventHub::sInstance;

// static
bool WinEventHub::Ensure() {
  if (sInstance) {
    return true;
  }

  LOG("WinEventHub::Ensure()");

  RefPtr<WinEventHub> instance = new WinEventHub();
  if (!instance->Initialize()) {
    MOZ_ASSERT_UNREACHABLE("unexpected to be called");
    return false;
  }
  sInstance = instance;
  ClearOnShutdown(&sInstance);
  return true;
}

WinEventHub::WinEventHub() {
  MOZ_ASSERT(NS_IsMainThread());
  LOG("WinEventHub::WinEventHub()");
}

WinEventHub::~WinEventHub() {
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(mObservers.IsEmpty());
  LOG("WinEventHub::~WinEventHub()");

  if (mHWnd) {
    ::DestroyWindow(mHWnd);
    mHWnd = nullptr;
  }
}

bool WinEventHub::Initialize() {
  WNDCLASSW wc;
  HMODULE hSelf = ::GetModuleHandle(nullptr);

  if (!GetClassInfoW(hSelf, L"MozillaWinEventHubClass", &wc)) {
    ZeroMemory(&wc, sizeof(WNDCLASSW));
    wc.hInstance = hSelf;
    wc.lpfnWndProc = WinEventProc;
    wc.lpszClassName = L"MozillaWinEventHubClass";
    RegisterClassW(&wc);
  }

  mHWnd = ::CreateWindowW(L"MozillaWinEventHubClass", L"WinEventHub", 0, 0, 0,
                          0, 0, nullptr, nullptr, hSelf, nullptr);
  if (!mHWnd) {
    return false;
  }

  return true;
}

// static
LRESULT CALLBACK WinEventHub::WinEventProc(HWND aHwnd, UINT aMsg,
                                           WPARAM aWParam, LPARAM aLParam) {
  if (sInstance) {
    sInstance->ProcessWinEventProc(aHwnd, aMsg, aWParam, aLParam);
  }
  return ::DefWindowProc(aHwnd, aMsg, aWParam, aLParam);
}

void WinEventHub::ProcessWinEventProc(HWND aHwnd, UINT aMsg, WPARAM aWParam,
                                      LPARAM aLParam) {
  for (const auto& observer : mObservers) {
    observer->OnWinEventProc(aHwnd, aMsg, aWParam, aLParam);
  }
}

void WinEventHub::AddObserver(WinEventObserver* aObserver) {
  LOG("WinEventHub::AddObserver() aObserver %p", aObserver);

  mObservers.Insert(aObserver);
}

void WinEventHub::RemoveObserver(WinEventObserver* aObserver) {
  LOG("WinEventHub::RemoveObserver() aObserver %p", aObserver);

  mObservers.Remove(aObserver);
}

WinEventObserver::~WinEventObserver() {
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(mDestroyed);
}

void WinEventObserver::Destroy() {
  LOG("WinEventObserver::Destroy() this %p", this);

  WinEventHub::Get()->RemoveObserver(this);
  mDestroyed = true;
}

// static
already_AddRefed<DisplayStatusObserver> DisplayStatusObserver::Create(
    DisplayStatusListener* aListener) {
  if (!WinEventHub::Ensure()) {
    return nullptr;
  }
  RefPtr<DisplayStatusObserver> observer = new DisplayStatusObserver(aListener);
  WinEventHub::Get()->AddObserver(observer);
  return observer.forget();
}

DisplayStatusObserver::DisplayStatusObserver(DisplayStatusListener* aListener)
    : mListener(aListener) {
  MOZ_ASSERT(NS_IsMainThread());
  LOG("DisplayStatusObserver::DisplayStatusObserver() this %p", this);

  mDisplayStatusHandle = ::RegisterPowerSettingNotification(
      WinEventHub::Get()->GetWnd(), &GUID_SESSION_DISPLAY_STATUS,
      DEVICE_NOTIFY_WINDOW_HANDLE);
}

DisplayStatusObserver::~DisplayStatusObserver() {
  MOZ_ASSERT(NS_IsMainThread());
  LOG("DisplayStatusObserver::~DisplayStatusObserver() this %p", this);

  if (mDisplayStatusHandle) {
    ::UnregisterPowerSettingNotification(mDisplayStatusHandle);
    mDisplayStatusHandle = nullptr;
  }
}

void DisplayStatusObserver::OnWinEventProc(HWND aHwnd, UINT aMsg,
                                           WPARAM aWParam, LPARAM aLParam) {
  if (aMsg == WM_POWERBROADCAST && aWParam == PBT_POWERSETTINGCHANGE) {
    POWERBROADCAST_SETTING* setting = (POWERBROADCAST_SETTING*)aLParam;
    if (setting &&
        ::IsEqualGUID(setting->PowerSetting, GUID_SESSION_DISPLAY_STATUS) &&
        setting->DataLength == sizeof(DWORD)) {
      bool displayOn = PowerMonitorOff !=
                       static_cast<MONITOR_DISPLAY_STATE>(setting->Data[0]);

      LOG("DisplayStatusObserver::OnWinEventProc() displayOn %d this %p",
          displayOn, this);
      mListener->OnDisplayStateChanged(displayOn);
    }
  }
}

// static
already_AddRefed<SessionChangeObserver> SessionChangeObserver::Create(
    SessionChangeListener* aListener) {
  if (!WinEventHub::Ensure()) {
    return nullptr;
  }
  RefPtr<SessionChangeObserver> observer = new SessionChangeObserver(aListener);
  WinEventHub::Get()->AddObserver(observer);
  return observer.forget();
}

SessionChangeObserver::SessionChangeObserver(SessionChangeListener* aListener)
    : mListener(aListener) {
  MOZ_ASSERT(NS_IsMainThread());
  LOG("SessionChangeObserver::SessionChangeObserver() this %p", this);

  auto hwnd = WinEventHub::Get()->GetWnd();
  DebugOnly<BOOL> wtsRegistered =
      ::WTSRegisterSessionNotification(hwnd, NOTIFY_FOR_THIS_SESSION);
  NS_ASSERTION(wtsRegistered, "WTSRegisterSessionNotification failed!\n");
}
SessionChangeObserver::~SessionChangeObserver() {
  MOZ_ASSERT(NS_IsMainThread());
  LOG("SessionChangeObserver::~SessionChangeObserver() this %p", this);

  auto hwnd = WinEventHub::Get()->GetWnd();
  // Unregister notifications from terminal services
  ::WTSUnRegisterSessionNotification(hwnd);
}

void SessionChangeObserver::OnWinEventProc(HWND aHwnd, UINT aMsg,
                                           WPARAM aWParam, LPARAM aLParam) {
  if (aMsg == WM_WTSSESSION_CHANGE &&
      (aWParam == WTS_SESSION_LOCK || aWParam == WTS_SESSION_UNLOCK)) {
    Maybe<bool> isCurrentSession;
    DWORD currentSessionId = 0;
    if (!::ProcessIdToSessionId(::GetCurrentProcessId(), &currentSessionId)) {
      isCurrentSession = Nothing();
    } else {
      LOG("SessionChangeObserver::OnWinEventProc() aWParam %zu aLParam "
          "%" PRIdLPTR
          " "
          "currentSessionId %lu this %p",
          aWParam, aLParam, currentSessionId, this);

      isCurrentSession = Some(static_cast<DWORD>(aLParam) == currentSessionId);
    }
    mListener->OnSessionChange(aWParam, isCurrentSession);
  }
}

#undef LOG

}  // namespace mozilla::widget
