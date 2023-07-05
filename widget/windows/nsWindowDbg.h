/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef WindowDbg_h__
#define WindowDbg_h__

/*
 * nsWindowDbg - Debug related utilities for nsWindow.
 */

#include "nsWindowDefs.h"
#include "mozilla/BaseProfilerMarkersPrerequisites.h"

// Enables debug output for popup rollup hooks
// #define POPUP_ROLLUP_DEBUG_OUTPUT

// Enable window size and state debug output
// #define WINSTATE_DEBUG_OUTPUT

// nsIWidget defines a set of debug output statements
// that are called in various places within the code.
// #define WIDGET_DEBUG_OUTPUT

// Enable IS_VK_DOWN debug output
// #define DEBUG_VK

namespace mozilla::widget {

class MOZ_RAII AutoProfilerMessageMarker {
 public:
  explicit AutoProfilerMessageMarker(Span<const char> aMsgLoopName, HWND hWnd,
                                     UINT msg, WPARAM wParam, LPARAM lParam);

  ~AutoProfilerMessageMarker();

 protected:
  Maybe<MarkerOptions> mOptions;
  Span<const char> mMsgLoopName;
  UINT mMsg;
  WPARAM mWParam;
  LPARAM mLParam;
};

// Windows message debugging data
struct EventMsgInfo {
  const char* mStr;
  UINT mId;
  std::function<nsAutoCString(WPARAM, LPARAM, bool)> mParamInfoFn;
  std::function<void(nsCString&, WPARAM, const char*, bool)> mWParamInfoFn;
  const char* mWParamName;
  std::function<void(nsCString&, LPARAM, const char*, bool)> mLParamInfoFn;
  const char* mLParamName;
  void LogParameters(nsCString& str, WPARAM wParam, LPARAM lParam,
                     bool isPreCall);
};
extern std::unordered_map<UINT, EventMsgInfo> gAllEvents;

// RAII-style class to log before and after an event is handled.
class NativeEventLogger final {
 public:
  template <size_t N>
  NativeEventLogger(const char (&aMsgLoopName)[N], HWND hwnd, UINT msg,
                    WPARAM wParam, LPARAM lParam)
      : NativeEventLogger(Span(aMsgLoopName), hwnd, msg, wParam, lParam) {}

  void SetResult(LRESULT lresult, bool result) {
    mRetValue = lresult;
    mResult = mozilla::Some(result);
  }
  ~NativeEventLogger();

 private:
  NativeEventLogger(Span<const char> aMsgLoopName, HWND hwnd, UINT msg,
                    WPARAM wParam, LPARAM lParam);
  bool NativeEventLoggerInternal();

  AutoProfilerMessageMarker mProfilerMarker;
  const char* mMsgLoopName;
  const HWND mHwnd;
  const UINT mMsg;
  const WPARAM mWParam;
  const LPARAM mLParam;
  mozilla::Maybe<long> mEventCounter;
  // not const because these will be set after the event is handled
  mozilla::Maybe<bool> mResult;
  LRESULT mRetValue = 0;

  bool mShouldLogPostCall;
};

struct EnumValueAndName {
  uint64_t mFlag;
  const char* mName;
};

// Appends to str a description of the flags passed in.
// flagsAndNames is a list of flag values with a string description
// for each one. These are processed in order, so if there are
// flag values that are combination of individual values (for example
// something like WS_OVERLAPPEDWINDOW) they need to come first
// in the flagsAndNames array.
// A 0 flag value will only be written if the flags input is exactly
// 0, and it must come last in the flagsAndNames array.
// Returns whether any info was appended to str.
bool AppendFlagsInfo(nsCString& str, uint64_t flags,
                     const nsTArray<EnumValueAndName>& flagsAndNames,
                     const char* name);

nsAutoCString WmSizeParamInfo(uint64_t wParam, uint64_t lParam, bool isPreCall);
void XLowWordYHighWordParamInfo(nsCString& str, uint64_t value,
                                const char* name, bool isPreCall);
void WindowPosParamInfo(nsCString& str, uint64_t value, const char* name,
                        bool isPreCall);
void WindowEdgeParamInfo(nsCString& str, uint64_t value, const char* name,
                         bool isPreCall);
void RectParamInfo(nsCString& str, uint64_t value, const char* name,
                   bool isPreCall);
void UiActionParamInfo(nsCString& str, uint64_t value, const char* name,
                       bool isPreCall);
void WideStringParamInfo(nsCString& result, uint64_t value, const char* name,
                         bool isPreCall);
void MinMaxInfoParamInfo(nsCString& result, uint64_t value, const char* name,
                         bool isPreCall);
nsAutoCString WmNcCalcSizeParamInfo(uint64_t wParam, uint64_t lParam,
                                    bool isPreCall);
}  // namespace mozilla::widget

#if defined(POPUP_ROLLUP_DEBUG_OUTPUT)
typedef struct {
  char* mStr;
  int mId;
} MSGFEventMsgInfo;

#  define DISPLAY_NMM_PRT(_arg) \
    MOZ_LOG(gWindowsLog, mozilla::LogLevel::Info, ((_arg)));
#else
#  define DISPLAY_NMM_PRT(_arg)
#endif  // defined(POPUP_ROLLUP_DEBUG_OUTPUT)

#if defined(DEBUG)
void DDError(const char* msg, HRESULT hr);
#endif  // defined(DEBUG)

#if defined(DEBUG_VK)
bool is_vk_down(int vk);
#  define IS_VK_DOWN is_vk_down
#else
#  define IS_VK_DOWN(a) (GetKeyState(a) < 0)
#endif  // defined(DEBUG_VK)

#endif /* WindowDbg_h__ */
