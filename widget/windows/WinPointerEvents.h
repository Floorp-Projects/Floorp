/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef WinPointerEvents_h__
#define WinPointerEvents_h__

#include "mozilla/MouseEvents.h"
#include "nsWindowBase.h"

// Define PointerEvent related macros and structures when building code on
// Windows version before Win8.
#if WINVER < 0x0602

// These definitions are copied from WinUser.h. Some of them are not used but
// keep them here for future usage.
#  define WM_NCPOINTERUPDATE 0x0241
#  define WM_NCPOINTERDOWN 0x0242
#  define WM_NCPOINTERUP 0x0243
#  define WM_POINTERUPDATE 0x0245
#  define WM_POINTERDOWN 0x0246
#  define WM_POINTERUP 0x0247
#  define WM_POINTERENTER 0x0249
#  define WM_POINTERLEAVE 0x024A
#  define WM_POINTERACTIVATE 0x024B
#  define WM_POINTERCAPTURECHANGED 0x024C
#  define WM_TOUCHHITTESTING 0x024D
#  define WM_POINTERWHEEL 0x024E
#  define WM_POINTERHWHEEL 0x024F
#  define DM_POINTERHITTEST 0x0250

/*
 * Flags that appear in pointer input message parameters
 */
#  define POINTER_MESSAGE_FLAG_NEW 0x00000001        // New pointer
#  define POINTER_MESSAGE_FLAG_INRANGE 0x00000002    // Pointer has not departed
#  define POINTER_MESSAGE_FLAG_INCONTACT 0x00000004  // Pointer is in contact
#  define POINTER_MESSAGE_FLAG_FIRSTBUTTON 0x00000010   // Primary action
#  define POINTER_MESSAGE_FLAG_SECONDBUTTON 0x00000020  // Secondary action
#  define POINTER_MESSAGE_FLAG_THIRDBUTTON 0x00000040   // Third button
#  define POINTER_MESSAGE_FLAG_FOURTHBUTTON 0x00000080  // Fourth button
#  define POINTER_MESSAGE_FLAG_FIFTHBUTTON 0x00000100   // Fifth button
#  define POINTER_MESSAGE_FLAG_PRIMARY 0x00002000       // Pointer is primary
#  define POINTER_MESSAGE_FLAG_CONFIDENCE \
    0x00004000  // Pointer is considered unlikely to be accidental
#  define POINTER_MESSAGE_FLAG_CANCELED \
    0x00008000  // Pointer is departing in an abnormal manner

/*
 * Macros to retrieve information from pointer input message parameters
 */
#  define GET_POINTERID_WPARAM(wParam) (LOWORD(wParam))
#  define IS_POINTER_FLAG_SET_WPARAM(wParam, flag) \
    (((DWORD)HIWORD(wParam) & (flag)) == (flag))
#  define IS_POINTER_NEW_WPARAM(wParam) \
    IS_POINTER_FLAG_SET_WPARAM(wParam, POINTER_MESSAGE_FLAG_NEW)
#  define IS_POINTER_INRANGE_WPARAM(wParam) \
    IS_POINTER_FLAG_SET_WPARAM(wParam, POINTER_MESSAGE_FLAG_INRANGE)
#  define IS_POINTER_INCONTACT_WPARAM(wParam) \
    IS_POINTER_FLAG_SET_WPARAM(wParam, POINTER_MESSAGE_FLAG_INCONTACT)
#  define IS_POINTER_FIRSTBUTTON_WPARAM(wParam) \
    IS_POINTER_FLAG_SET_WPARAM(wParam, POINTER_MESSAGE_FLAG_FIRSTBUTTON)
#  define IS_POINTER_SECONDBUTTON_WPARAM(wParam) \
    IS_POINTER_FLAG_SET_WPARAM(wParam, POINTER_MESSAGE_FLAG_SECONDBUTTON)
#  define IS_POINTER_THIRDBUTTON_WPARAM(wParam) \
    IS_POINTER_FLAG_SET_WPARAM(wParam, POINTER_MESSAGE_FLAG_THIRDBUTTON)
#  define IS_POINTER_FOURTHBUTTON_WPARAM(wParam) \
    IS_POINTER_FLAG_SET_WPARAM(wParam, POINTER_MESSAGE_FLAG_FOURTHBUTTON)
#  define IS_POINTER_FIFTHBUTTON_WPARAM(wParam) \
    IS_POINTER_FLAG_SET_WPARAM(wParam, POINTER_MESSAGE_FLAG_FIFTHBUTTON)
#  define IS_POINTER_PRIMARY_WPARAM(wParam) \
    IS_POINTER_FLAG_SET_WPARAM(wParam, POINTER_MESSAGE_FLAG_PRIMARY)
#  define HAS_POINTER_CONFIDENCE_WPARAM(wParam) \
    IS_POINTER_FLAG_SET_WPARAM(wParam, POINTER_MESSAGE_FLAG_CONFIDENCE)
#  define IS_POINTER_CANCELED_WPARAM(wParam) \
    IS_POINTER_FLAG_SET_WPARAM(wParam, POINTER_MESSAGE_FLAG_CANCELED)

/*
 * WM_POINTERACTIVATE return codes
 */
#  define PA_ACTIVATE MA_ACTIVATE
#  define PA_NOACTIVATE MA_NOACTIVATE

#endif  // WINVER < 0x0602

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
  bool ShouldEnableInkCollector();
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
