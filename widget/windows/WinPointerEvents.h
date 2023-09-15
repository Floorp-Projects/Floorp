/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef WinPointerEvents_h__
#define WinPointerEvents_h__

#include "mozilla/MouseEvents.h"
#include "touchinjection_sdk80.h"
#include <windef.h>

/******************************************************************************
 * WinPointerInfo
 *
 * This is a helper class to handle WM_POINTER*. It only supports Win8 or later.
 *
 ******************************************************************************/
class WinPointerInfo final : public mozilla::WidgetPointerHelper {
 public:
  WinPointerInfo() : WidgetPointerHelper(), mPressure(0), mButtons(0) {}

  WinPointerInfo(uint32_t aPointerId, uint32_t aTiltX, uint32_t aTiltY,
                 float aPressure, int16_t aButtons)
      : WidgetPointerHelper(aPointerId, aTiltX, aTiltY),
        mPressure(aPressure),
        mButtons(aButtons) {}

  float mPressure;
  int16_t mButtons;
};

class WinPointerEvents final {
 public:
  explicit WinPointerEvents();

 public:
  bool ShouldHandleWinPointerMessages(UINT aMsg, WPARAM aWParam);

  uint32_t GetPointerId(WPARAM aWParam) {
    return GET_POINTERID_WPARAM(aWParam);
  }
  bool GetPointerType(uint32_t aPointerId, POINTER_INPUT_TYPE* aPointerType);
  POINTER_INPUT_TYPE GetPointerType(uint32_t aPointerId);
  bool GetPointerInfo(uint32_t aPointerId, POINTER_INFO* aPointerInfo);
  bool GetPointerPenInfo(uint32_t aPointerId, POINTER_PEN_INFO* aPenInfo);
  bool ShouldRollupOnPointerEvent(UINT aMsg, WPARAM aWParam);
  bool ShouldFirePointerEventByWinPointerMessages();
  WinPointerInfo* GetCachedPointerInfo(UINT aMsg, WPARAM aWParam);
  void ConvertAndCachePointerInfo(UINT aMsg, WPARAM aWParam);
  void ConvertAndCachePointerInfo(WPARAM aWParam, WinPointerInfo* aInfo);

 private:
  // Function prototypes
  typedef BOOL(WINAPI* GetPointerTypePtr)(uint32_t aPointerId,
                                          POINTER_INPUT_TYPE* aPointerType);
  typedef BOOL(WINAPI* GetPointerInfoPtr)(uint32_t aPointerId,
                                          POINTER_INFO* aPointerInfo);
  typedef BOOL(WINAPI* GetPointerPenInfoPtr)(uint32_t aPointerId,
                                             POINTER_PEN_INFO* aPenInfo);

  void InitLibrary();

  static HMODULE sLibraryHandle;
  static const wchar_t kPointerLibraryName[];
  // Static function pointers
  static GetPointerTypePtr getPointerType;
  static GetPointerInfoPtr getPointerInfo;
  static GetPointerPenInfoPtr getPointerPenInfo;
  WinPointerInfo mPenPointerDownInfo;
  WinPointerInfo mPenPointerUpInfo;
  WinPointerInfo mPenPointerUpdateInfo;
};

#endif  // #ifndef WinPointerEvents_h__
