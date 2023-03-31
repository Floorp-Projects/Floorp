/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include <windef.h>
#include <winuser.h>
#include "mozilla/StaticPrefs_storage.h"
#include "mozilla/StaticPrefs_widget.h"
#include "nsWindowLoggedMessages.h"
#include "nsWindow.h"
#include "WinUtils.h"
#include <map>
#include <algorithm>

namespace mozilla::widget {

// NCCALCSIZE_PARAMS and WINDOWPOS are relatively large structures, so store
// them as a pointer to save memory
using NcCalcSizeVariantData =
    Variant<UniquePtr<std::pair<NCCALCSIZE_PARAMS, WINDOWPOS>>, RECT>;
// to save memory, hold the raw data and only convert to string
// when requested
using MessageSpecificData =
    Variant<std::pair<WPARAM, LPARAM>,  // WM_SIZE, WM_MOVE
            WINDOWPOS,  // WM_WINDOWPOSCHANGING, WM_WINDOWPOSCHANGED
            std::pair<WPARAM, RECT>,      // WM_SIZING, WM_DPICHANGED, WM_MOVING
            std::pair<WPARAM, nsString>,  // WM_SETTINGCHANGE
            std::pair<bool, NcCalcSizeVariantData>,  // WM_NCCALCSIZE
            MINMAXINFO                               // WM_GETMINMAXINFO
            >;

struct WindowMessageData {
  long mEventCounter;
  bool mIsPreEvent;
  MessageSpecificData mSpecificData;
  mozilla::Maybe<bool> mResult;
  LRESULT mRetValue;
  WindowMessageData(long eventCounter, bool isPreEvent,
                    MessageSpecificData&& specificData,
                    mozilla::Maybe<bool> result, LRESULT retValue)
      : mEventCounter(eventCounter),
        mIsPreEvent(isPreEvent),
        mSpecificData(std::move(specificData)),
        mResult(result),
        mRetValue(retValue) {}
  // Disallow copy constructor/operator since MessageSpecificData has a
  // UniquePtr
  WindowMessageData(const WindowMessageData&) = delete;
  WindowMessageData& operator=(const WindowMessageData&) = delete;
  WindowMessageData(WindowMessageData&&) = default;
  WindowMessageData& operator=(WindowMessageData&&) = default;
};

struct WindowMessageDataSortKey {
  long mEventCounter;
  bool mIsPreEvent;
  explicit WindowMessageDataSortKey(const WindowMessageData& data)
      : mEventCounter(data.mEventCounter), mIsPreEvent(data.mIsPreEvent) {}
  bool operator<(const WindowMessageDataSortKey& other) const {
    if (mEventCounter < other.mEventCounter) {
      return true;
    }
    if (other.mEventCounter < mEventCounter) {
      return false;
    }
    if (mIsPreEvent && !other.mIsPreEvent) {
      return true;
    }
    if (other.mIsPreEvent && !mIsPreEvent) {
      return false;
    }
    // they're equal
    return false;
  }
};

struct CircularMessageBuffer {
  // Only used when the vector is at its maximum size
  size_t mNextFreeIndex = 0;
  std::vector<WindowMessageData> mMessages;
};
static std::map<HWND, std::map<UINT, CircularMessageBuffer>> gWindowMessages;

static HWND GetHwndFromWidget(nsIWidget* windowWidget) {
  nsWindow* window = static_cast<nsWindow*>(windowWidget);
  return window->GetWindowHandle();
}

MessageSpecificData MakeMessageSpecificData(UINT event, WPARAM wParam,
                                            LPARAM lParam) {
  // Since we store this data for every message we log, make sure it's of a
  // reasonable size. Keep in mind we're storing up to 10 (number of message
  // types)
  // * 6 (default number of messages per type to keep) of these messages per
  // window.
  static_assert(sizeof(MessageSpecificData) <= 48);
  switch (event) {
    case WM_SIZE:
    case WM_MOVE:
      return MessageSpecificData(std::make_pair(wParam, lParam));
    case WM_WINDOWPOSCHANGING:
    case WM_WINDOWPOSCHANGED: {
      LPWINDOWPOS windowPosPtr = reinterpret_cast<LPWINDOWPOS>(lParam);
      WINDOWPOS windowPos = *windowPosPtr;
      return MessageSpecificData(std::move(windowPos));
    }
    case WM_SIZING:
    case WM_DPICHANGED:
    case WM_MOVING: {
      LPRECT rectPtr = reinterpret_cast<LPRECT>(lParam);
      RECT rect = *rectPtr;
      return MessageSpecificData(std::make_pair(wParam, std::move(rect)));
    }
    case WM_SETTINGCHANGE: {
      LPCWSTR wideStrPtr = reinterpret_cast<LPCWSTR>(lParam);
      nsString str(wideStrPtr);
      return MessageSpecificData(std::make_pair(wParam, std::move(str)));
    }
    case WM_NCCALCSIZE: {
      bool shouldIndicateValidArea = wParam == TRUE;
      if (shouldIndicateValidArea) {
        LPNCCALCSIZE_PARAMS ncCalcSizeParamsPtr =
            reinterpret_cast<LPNCCALCSIZE_PARAMS>(lParam);
        NCCALCSIZE_PARAMS ncCalcSizeParams = *ncCalcSizeParamsPtr;
        WINDOWPOS windowPos = *ncCalcSizeParams.lppos;
        UniquePtr<std::pair<NCCALCSIZE_PARAMS, WINDOWPOS>> ncCalcSizeData =
            MakeUnique<std::pair<NCCALCSIZE_PARAMS, WINDOWPOS>>(
                std::pair(std::move(ncCalcSizeParams), std::move(windowPos)));
        return MessageSpecificData(
            std::make_pair(shouldIndicateValidArea,
                           NcCalcSizeVariantData(std::move(ncCalcSizeData))));
      } else {
        LPRECT rectPtr = reinterpret_cast<LPRECT>(lParam);
        RECT rect = *rectPtr;
        return MessageSpecificData(std::make_pair(
            shouldIndicateValidArea, NcCalcSizeVariantData(std::move(rect))));
      }
    }
    case WM_GETMINMAXINFO: {
      PMINMAXINFO minMaxInfoPtr = reinterpret_cast<PMINMAXINFO>(lParam);
      MINMAXINFO minMaxInfo = *minMaxInfoPtr;
      return MessageSpecificData(std::move(minMaxInfo));
    }
    default:
      MOZ_ASSERT_UNREACHABLE(
          "Unhandled message type in MakeMessageSpecificData");
      return MessageSpecificData(std::make_pair(wParam, lParam));
  }
}

void AppendFriendlyMessageSpecificData(nsCString& str, UINT event,
                                       bool isPreEvent,
                                       const MessageSpecificData& data) {
  switch (event) {
    case WM_SIZE: {
      const auto& params = data.as<std::pair<WPARAM, LPARAM>>();
      nsAutoCString tempStr =
          WmSizeParamInfo(params.first, params.second, isPreEvent);
      str.AppendASCII(tempStr);
      break;
    }
    case WM_MOVE: {
      const auto& params = data.as<std::pair<WPARAM, LPARAM>>();
      XLowWordYHighWordParamInfo(str, params.second, "upperLeft", isPreEvent);
      break;
    }
    case WM_WINDOWPOSCHANGING:
    case WM_WINDOWPOSCHANGED: {
      const auto& params = data.as<WINDOWPOS>();
      WindowPosParamInfo(str, reinterpret_cast<uint64_t>(&params),
                         "newSizeAndPos", isPreEvent);
      break;
    }
    case WM_SIZING: {
      const auto& params = data.as<std::pair<WPARAM, RECT>>();
      WindowEdgeParamInfo(str, params.first, "edge", isPreEvent);
      str.AppendASCII(" ");
      RectParamInfo(str, reinterpret_cast<uint64_t>(&params.second), "rect",
                    isPreEvent);
      break;
    }
    case WM_DPICHANGED: {
      const auto& params = data.as<std::pair<WPARAM, RECT>>();
      XLowWordYHighWordParamInfo(str, params.first, "newDPI", isPreEvent);
      str.AppendASCII(" ");
      RectParamInfo(str, reinterpret_cast<uint64_t>(&params.second),
                    "suggestedSizeAndPos", isPreEvent);
      break;
    }
    case WM_MOVING: {
      const auto& params = data.as<std::pair<WPARAM, RECT>>();
      RectParamInfo(str, reinterpret_cast<uint64_t>(&params.second), "rect",
                    isPreEvent);
      break;
    }
    case WM_SETTINGCHANGE: {
      const auto& params = data.as<std::pair<WPARAM, nsString>>();
      UiActionParamInfo(str, params.first, "uiAction", isPreEvent);
      str.AppendASCII(" ");
      WideStringParamInfo(
          str,
          reinterpret_cast<uint64_t>((const wchar_t*)(params.second.Data())),
          "paramChanged", isPreEvent);
      break;
    }
    case WM_NCCALCSIZE: {
      const auto& params = data.as<std::pair<bool, NcCalcSizeVariantData>>();
      bool shouldIndicateValidArea = params.first;
      if (shouldIndicateValidArea) {
        const auto& validAreaParams =
            params.second
                .as<UniquePtr<std::pair<NCCALCSIZE_PARAMS, WINDOWPOS>>>();
        // Make pointer point to the cached data
        validAreaParams->first.lppos = &validAreaParams->second;
        nsAutoCString tempStr = WmNcCalcSizeParamInfo(
            TRUE, reinterpret_cast<uint64_t>(&validAreaParams->first),
            isPreEvent);
        str.AppendASCII(tempStr);
      } else {
        RECT rect = params.second.as<RECT>();
        nsAutoCString tempStr = WmNcCalcSizeParamInfo(
            FALSE, reinterpret_cast<uint64_t>(&rect), isPreEvent);
        str.AppendASCII(tempStr);
      }
      break;
    }
    case WM_GETMINMAXINFO: {
      const auto& params = data.as<MINMAXINFO>();
      MinMaxInfoParamInfo(str, reinterpret_cast<uint64_t>(&params), "",
                          isPreEvent);
      break;
    }
    default:
      MOZ_ASSERT(false,
                 "Unhandled message type in AppendFriendlyMessageSpecificData");
      str.AppendASCII("???");
  }
}

nsCString MakeFriendlyMessage(UINT event, bool isPreEvent, long eventCounter,
                              const MessageSpecificData& data,
                              mozilla::Maybe<bool> result, LRESULT retValue) {
  nsCString str;
  const char* eventName = mozilla::widget::WinUtils::WinEventToEventName(event);
  MOZ_ASSERT(eventName, "Unknown event name in MakeFriendlyMessage");
  eventName = eventName ? eventName : "(unknown)";
  str.AppendPrintf("%6ld %04x (%s) - ", eventCounter, event, eventName);
  AppendFriendlyMessageSpecificData(str, event, isPreEvent, data);
  const char* resultMsg =
      result.isSome() ? (result.value() ? "true" : "false") : "initial call";
  str.AppendPrintf(" 0x%08llX (%s)",
                   result.isSome() ? static_cast<uint64_t>(retValue) : 0,
                   resultMsg);
  return str;
}

void WindowClosed(HWND hwnd) { gWindowMessages.erase(hwnd); }

void LogWindowMessage(HWND hwnd, UINT event, bool isPreEvent, long eventCounter,
                      WPARAM wParam, LPARAM lParam, mozilla::Maybe<bool> result,
                      LRESULT retValue) {
  auto& hwndMessages = gWindowMessages[hwnd];
  auto& hwndWindowMessages = hwndMessages[event];
  WindowMessageData messageData = {
      eventCounter, isPreEvent, MakeMessageSpecificData(event, wParam, lParam),
      result, retValue};
  uint32_t numberOfMessagesToKeep =
      StaticPrefs::widget_windows_messages_to_log();
  if (hwndWindowMessages.mMessages.size() < numberOfMessagesToKeep) {
    // haven't reached limit yet
    hwndWindowMessages.mMessages.push_back(std::move(messageData));
  } else {
    hwndWindowMessages.mMessages[hwndWindowMessages.mNextFreeIndex] =
        std::move(messageData);
  }
  hwndWindowMessages.mNextFreeIndex =
      (hwndWindowMessages.mNextFreeIndex + 1) % numberOfMessagesToKeep;
}

void GetLatestWindowMessages(RefPtr<nsIWidget> windowWidget,
                             nsTArray<nsCString>& messages) {
  HWND hwnd = GetHwndFromWidget(windowWidget);
  const auto& rawMessages = gWindowMessages[hwnd];
  nsTArray<std::pair<WindowMessageDataSortKey, nsCString>>
      sortKeyAndMessageArray;
  sortKeyAndMessageArray.SetCapacity(
      rawMessages.size() * StaticPrefs::widget_windows_messages_to_log());
  for (const auto& eventAndMessage : rawMessages) {
    for (const auto& messageData : eventAndMessage.second.mMessages) {
      nsCString message = MakeFriendlyMessage(
          eventAndMessage.first, messageData.mIsPreEvent,
          messageData.mEventCounter, messageData.mSpecificData,
          messageData.mResult, messageData.mRetValue);
      WindowMessageDataSortKey sortKey(messageData);
      sortKeyAndMessageArray.AppendElement(
          std::make_pair(sortKey, std::move(message)));
    }
  }
  std::sort(sortKeyAndMessageArray.begin(), sortKeyAndMessageArray.end());
  messages.SetCapacity(sortKeyAndMessageArray.Length());
  for (const std::pair<WindowMessageDataSortKey, nsCString>& entry :
       sortKeyAndMessageArray) {
    messages.AppendElement(std::move(entry.second));
  }
}
}  // namespace mozilla::widget
