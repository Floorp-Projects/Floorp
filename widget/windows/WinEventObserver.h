/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef widget_windows_WinEventObserver_h
#define widget_windows_WinEventObserver_h

#include <windows.h>

#include "mozilla/Maybe.h"
#include "nsISupportsImpl.h"
#include "nsTHashSet.h"

namespace mozilla {

namespace widget {

class WinEventObserver {
  NS_INLINE_DECL_THREADSAFE_REFCOUNTING(WinEventObserver)
 public:
  virtual void OnWinEventProc(HWND aHwnd, UINT aMsg, WPARAM aWParam,
                              LPARAM aLParam) {}
  virtual void Destroy();

 protected:
  virtual ~WinEventObserver();

  bool mDestroyed = false;
};

// Uses singleton window to observe events like display status and session
// change.
class WinEventHub final {
  NS_INLINE_DECL_THREADSAFE_REFCOUNTING(WinEventHub)

 public:
  // Returns true if the singleton exists (or was created). Will return
  // false if the singleton couldn't be created, in which case a call
  // to Get() will return a nullptr. It is safe to call this function
  // repeatedly.
  static bool Ensure();
  static RefPtr<WinEventHub> Get() { return sInstance; }

  void AddObserver(WinEventObserver* aObserver);
  void RemoveObserver(WinEventObserver* aObserver);

  HWND GetWnd() { return mHWnd; }

 private:
  WinEventHub();
  ~WinEventHub();

  static LRESULT CALLBACK WinEventProc(HWND aHwnd, UINT aMsg, WPARAM aWParam,
                                       LPARAM aLParam);

  bool Initialize();
  void ProcessWinEventProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);

  HWND mHWnd = nullptr;
  nsTHashSet<nsRefPtrHashKey<WinEventObserver>> mObservers;

  static StaticRefPtr<WinEventHub> sInstance;
};

class DisplayStatusListener {
 public:
  virtual void OnDisplayStateChanged(bool aDisplayOn) = 0;
};

// Observe Display on/off event
class DisplayStatusObserver final : public WinEventObserver {
 public:
  static already_AddRefed<DisplayStatusObserver> Create(
      DisplayStatusListener* aListener);

 private:
  explicit DisplayStatusObserver(DisplayStatusListener* aListener);
  virtual ~DisplayStatusObserver();
  void OnWinEventProc(HWND aHwnd, UINT aMsg, WPARAM aWParam,
                      LPARAM aLParam) override;

  DisplayStatusListener* mListener;

  HPOWERNOTIFY mDisplayStatusHandle = nullptr;
};

class SessionChangeListener {
 public:
  virtual void OnSessionChange(WPARAM aStatusCode,
                               Maybe<bool> aIsCurrentSession) = 0;
};

// Observe session lock/unlock event
class SessionChangeObserver : public WinEventObserver {
 public:
  static already_AddRefed<SessionChangeObserver> Create(
      SessionChangeListener* aListener);

 private:
  explicit SessionChangeObserver(SessionChangeListener* aListener);
  virtual ~SessionChangeObserver();

  void Initialize();
  void OnWinEventProc(HWND aHwnd, UINT aMsg, WPARAM aWParam,
                      LPARAM aLParam) override;

  SessionChangeListener* mListener;
};

}  // namespace widget
}  // namespace mozilla

#endif  // widget_windows_WinEventObserver_h
