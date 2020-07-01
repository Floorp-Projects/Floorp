/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "Notification.h"

#include <shlwapi.h>
#include <wchar.h>
#include <windows.h>
#include <winnt.h>

#include "mozilla/WindowsVersion.h"
#include "mozilla/WinHeaderOnlyUtils.h"
#include "nsWindowsHelpers.h"
#include "readstrings.h"
#include "updatererrors.h"
#include "WindowsDefaultBrowser.h"

#include "common.h"
#include "DefaultBrowser.h"
#include "EventLog.h"
#include "Registry.h"

#include "wintoastlib.h"

#define SEVEN_DAYS_IN_SECONDS (7 * 24 * 60 * 60)

// Rename this constant from readstrings.h so it is more clear what it is
// in this context
#define MAX_INI_STRING_LEN MAX_TEXT_LEN

// If the notification hasn't been activated or dismissed within 30 minutes,
// stop waiting for it.
#define NOTIFICATION_WAIT_TIMEOUT_MS (30 * 60 * 1000)

static bool SetInitialNotificationShown(bool wasShown) {
  return !RegistrySetValueBool(IsPrefixed::Unprefixed,
                               L"InitialNotificationShown", wasShown)
              .isErr();
}

static bool GetInitialNotificationShown() {
  return RegistryGetValueBool(IsPrefixed::Unprefixed,
                              L"InitialNotificationShown")
      .unwrapOr(mozilla::Some(false))
      .valueOr(false);
}

static bool SetFollowupNotificationShown(bool wasShown) {
  return !RegistrySetValueBool(IsPrefixed::Unprefixed,
                               L"FollowupNotificationShown", wasShown)
              .isErr();
}

static bool GetFollowupNotificationShown() {
  return RegistryGetValueBool(IsPrefixed::Unprefixed,
                              L"FollowupNotificationShown")
      .unwrapOr(mozilla::Some(false))
      .valueOr(false);
}

// Note that the "Request Time" represents the time at which a followup was
// requested, not the time at which it is supposed to be shown.
static bool SetFollowupNotificationRequestTime(ULONGLONG time) {
  return !RegistrySetValueQword(IsPrefixed::Unprefixed, L"FollowupRequestTime",
                                time)
              .isErr();
}

// Returns 0 if no value is set.
static ULONGLONG GetFollowupNotificationRequestTime() {
  return RegistryGetValueQword(IsPrefixed::Unprefixed, L"FollowupRequestTime")
      .unwrapOr(mozilla::Some(0))
      .valueOr(0);
}

static ULONGLONG GetCurrentTimestamp() {
  FILETIME filetime;
  GetSystemTimeAsFileTime(&filetime);
  ULARGE_INTEGER integerTime;
  integerTime.u.LowPart = filetime.dwLowDateTime;
  integerTime.u.HighPart = filetime.dwHighDateTime;
  return integerTime.QuadPart;
}

static ULONGLONG SecondsPassedSince(ULONGLONG initialTime) {
  ULONGLONG now = GetCurrentTimestamp();
  // Since this is returning an unsigned value, let's make sure we don't try to
  // return anything negative
  if (initialTime >= now) {
    return 0;
  }

  // These timestamps are expressed in 100-nanosecond intervals
  return (now - initialTime) / 10  // To microseconds
         / 1000                    // To milliseconds
         / 1000;                   // To seconds
}

enum class NotificationType {
  Initial,
  Followup,
};

struct ToastStrings {
  mozilla::UniquePtr<wchar_t[]> text1;
  mozilla::UniquePtr<wchar_t[]> text2;
  mozilla::UniquePtr<wchar_t[]> action1;
  mozilla::UniquePtr<wchar_t[]> action2;
  mozilla::UniquePtr<wchar_t[]> relImagePath;
};

struct Strings {
  ToastStrings initialToast;
  ToastStrings followupToast;

  // Returned pointer points within this struct and should not be freed.
  const ToastStrings* GetToastStrings(NotificationType whichToast) const {
    if (whichToast == NotificationType::Initial) {
      return &initialToast;
    }
    return &followupToast;
  }
};

// Gets a string out of the specified INI file. This function should not be used
// for localized strings.
// Returns true on success, false on failure
static bool GetString(const wchar_t* iniPath, const wchar_t* section,
                      const wchar_t* key,
                      mozilla::UniquePtr<wchar_t[]>& toastString) {
  toastString = mozilla::MakeUnique<wchar_t[]>(MAX_INI_STRING_LEN);
  DWORD stringLength = GetPrivateProfileStringW(
      section, key, nullptr, toastString.get(), MAX_INI_STRING_LEN, iniPath);
  if (stringLength <= 0) {
    LOG_ERROR_MESSAGE(L"Unable to retrieve string: section=%s, key=%s", section,
                      key);
    return false;
  }
  return true;
}

// Returns true on success, false on failure.
static bool ConvertToWide(const char* toConvert,
                          mozilla::UniquePtr<wchar_t[]>& result) {
  int bufferSize = MultiByteToWideChar(CP_UTF8, 0, toConvert, -1, nullptr, 0);
  result = mozilla::MakeUnique<wchar_t[]>(bufferSize);
  int charsWritten =
      MultiByteToWideChar(CP_UTF8, 0, toConvert, -1, result.get(), bufferSize);
  if (charsWritten == 0) {
    LOG_ERROR_MESSAGE(L"Unable to widen localized string: %#X", GetLastError());
    return false;
  }
  return true;
}

// Gets all strings out of the localized INI file.
// Note that, at this time, the INI is not actually being localized - it only
// exists in English. This is why MaybeShowNotification() checks IsEnglish().
// See Bug 1621696 for more information.
// Returns true on success, false on failure
static bool GetStrings(Strings& strings) {
  mozilla::UniquePtr<wchar_t[]> installPath;
  nsresult rv = GetInstallDirectory(installPath);
  if (NS_FAILED(rv)) {
    LOG_ERROR_MESSAGE(L"Failed to get install directory when getting strings");
    return false;
  }
  const wchar_t* iniFormat = L"%s\\defaultagent.ini";
  int bufferSize = _scwprintf(iniFormat, installPath.get());
  ++bufferSize;  // Extra character for terminating null
  mozilla::UniquePtr<wchar_t[]> iniPath =
      mozilla::MakeUnique<wchar_t[]>(bufferSize);
  _snwprintf_s(iniPath.get(), bufferSize, _TRUNCATE, iniFormat,
               installPath.get());

  // The ReadStrings function is designed to read localized strings.
  const unsigned int stringCount = 5;
  const char* keys =
      "DefaultBrowserNotificationTitle\0"
      "DefaultBrowserNotificationText\0"
      "DefaultBrowserNotificationRemindMeLater\0"
      "DefaultBrowserNotificationMakeFirefoxDefault\0"
      "DefaultBrowserNotificationDontShowAgain\0";
  char localizedStrings[stringCount][MAX_INI_STRING_LEN];
  int result = ReadStrings(iniPath.get(), keys, stringCount, localizedStrings);
  if (result != OK) {
    LOG_ERROR_MESSAGE(L"Unable to read localized strings: %d", result);
    return false;
  }

  return ConvertToWide(localizedStrings[0], strings.initialToast.text1) &&
         ConvertToWide(localizedStrings[1], strings.initialToast.text2) &&
         ConvertToWide(localizedStrings[2], strings.initialToast.action1) &&
         ConvertToWide(localizedStrings[3], strings.initialToast.action2) &&
         GetString(iniPath.get(), L"Nonlocalized",
                   L"InitialToastRelativeImagePath",
                   strings.initialToast.relImagePath) &&
         ConvertToWide(localizedStrings[0], strings.followupToast.text1) &&
         ConvertToWide(localizedStrings[1], strings.followupToast.text2) &&
         ConvertToWide(localizedStrings[4], strings.followupToast.action1) &&
         ConvertToWide(localizedStrings[3], strings.followupToast.action2) &&
         GetString(iniPath.get(), L"Nonlocalized",
                   L"FollowupToastRelativeImagePath",
                   strings.followupToast.relImagePath);
}

class ToastHandler : public WinToastLib::IWinToastHandler {
 private:
  NotificationType mWhichNotification;
  HANDLE mEvent;

 public:
  ToastHandler(NotificationType whichNotification, HANDLE event) {
    mWhichNotification = whichNotification;
    mEvent = event;
  }

  void FireEvent() const {
    BOOL success = SetEvent(mEvent);
    if (!success) {
      LOG_ERROR_MESSAGE(L"Event could not be set: %#X", GetLastError());
    }
  }

  void toastActivated() const override {
    LaunchModernSettingsDialogDefaultApps();

    FireEvent();
  }

  void toastActivated(int actionIndex) const override {
    if (actionIndex == 0) {
      if (mWhichNotification == NotificationType::Initial) {
        // "Remind me later" button
        if (!SetFollowupNotificationRequestTime(GetCurrentTimestamp())) {
          LOG_ERROR_MESSAGE(L"Unable to schedule followup notification");
        }
      } else {
        // "Don't ask again" button
        // Do nothing. As long as we don't call
        // SetFollowupNotificationRequestTime, there will be no followup
        // notification.
      }
    } else if (actionIndex == 1) {
      // "Make Firefox the default" button, on both notifications.
      LaunchModernSettingsDialogDefaultApps();
    }

    FireEvent();
  }

  void toastDismissed(WinToastDismissalReason state) const override {
    FireEvent();
  }

  void toastFailed() const override {
    LOG_ERROR_MESSAGE(L"Toast notification failed to display");
    FireEvent();
  }
};

// This function blocks until the shown notification is activated or dismissed.
static void ShowNotification(NotificationType whichNotification,
                             const wchar_t* aumi) {
  using namespace WinToastLib;

  if (!WinToast::isCompatible()) {
    LOG_ERROR_MESSAGE(L"System is not compatible with WinToast");
    return;
  }

  WinToast::instance()->setAppName(L"" MOZ_APP_BASENAME);
  std::wstring aumiStr = aumi;
  WinToast::instance()->setAppUserModelId(aumiStr);
  WinToast::WinToastError error;
  if (!WinToast::instance()->initialize(&error)) {
    LOG_ERROR_MESSAGE(WinToast::strerror(error).c_str());
    return;
  }

  Strings strings;
  if (!GetStrings(strings)) {
    return;
  }
  const ToastStrings* toastStrings = strings.GetToastStrings(whichNotification);

  // This event object will let the handler notify us when it has handled the
  // notification.
  nsAutoHandle event(CreateEventW(nullptr, TRUE, FALSE, nullptr));
  if (event.get() == nullptr) {
    LOG_ERROR_MESSAGE(L"Unable to create event object: %#X", GetLastError());
    return;
  }

  bool success = false;
  if (whichNotification == NotificationType::Initial) {
    success = SetInitialNotificationShown(true);
  } else {
    success = SetFollowupNotificationShown(true);
  }
  if (!success) {
    // Return early in this case to prevent the notification from being shown
    // on every run.
    LOG_ERROR_MESSAGE(L"Unable to set notification as displayed");
    return;
  }

  // We need the absolute image path, not the relative path.
  mozilla::UniquePtr<wchar_t[]> installPath;
  nsresult rv = GetInstallDirectory(installPath);
  if (NS_FAILED(rv)) {
    LOG_ERROR_MESSAGE(L"Failed to get install directory for the image path");
    return;
  }
  const wchar_t* absPathFormat = L"%s\\%s";
  int bufferSize = _scwprintf(absPathFormat, installPath.get(),
                              toastStrings->relImagePath.get());
  ++bufferSize;  // Extra character for terminating null
  mozilla::UniquePtr<wchar_t[]> absImagePath =
      mozilla::MakeUnique<wchar_t[]>(bufferSize);
  _snwprintf_s(absImagePath.get(), bufferSize, _TRUNCATE, absPathFormat,
               installPath.get(), toastStrings->relImagePath.get());

  // Finally ready to assemble the notification and dispatch it.
  WinToastTemplate toastTemplate =
      WinToastTemplate(WinToastTemplate::ImageAndText02);
  toastTemplate.setTextField(toastStrings->text1.get(),
                             WinToastTemplate::FirstLine);
  toastTemplate.setTextField(toastStrings->text2.get(),
                             WinToastTemplate::SecondLine);
  toastTemplate.addAction(toastStrings->action1.get());
  toastTemplate.addAction(toastStrings->action2.get());
  toastTemplate.setImagePath(absImagePath.get());
  ToastHandler* handler = new ToastHandler(whichNotification, event.get());
  INT64 id = WinToast::instance()->showToast(toastTemplate, handler, &error);
  if (id < 0) {
    LOG_ERROR_MESSAGE(WinToast::strerror(error).c_str());
    return;
  }

  DWORD result = WaitForSingleObject(event.get(), NOTIFICATION_WAIT_TIMEOUT_MS);
  // Don't return after these errors. Attempt to hide the notification.
  if (result == WAIT_FAILED) {
    LOG_ERROR_MESSAGE(L"Unable to wait on event object: %#X", GetLastError());
  } else if (result == WAIT_TIMEOUT) {
    LOG_ERROR_MESSAGE(L"Timed out waiting for event object");
  }

  if (!WinToast::instance()->hideToast(id)) {
    LOG_ERROR_MESSAGE(L"Failed to hide notification");
  }
}

// This function checks that both the Firefox build and the operating system
// are using English. This is checked because this feature is not yet being
// localized.
bool IsEnglish() {
  mozilla::UniquePtr<wchar_t[]> windowsLocale =
      mozilla::MakeUnique<wchar_t[]>(LOCALE_NAME_MAX_LENGTH);
  int result =
      GetUserDefaultLocaleName(windowsLocale.get(), LOCALE_NAME_MAX_LENGTH);
  if (result == 0) {
    LOG_ERROR_MESSAGE(L"Unable to get locale: %#X", GetLastError());
    return false;
  }

  mozilla::UniquePtr<wchar_t[]> installPath;
  nsresult rv = GetInstallDirectory(installPath);
  if (NS_FAILED(rv)) {
    LOG_ERROR_MESSAGE(L"Failed to get install directory when getting strings");
    return false;
  }
  const wchar_t* iniFormat = L"%s\\locale.ini";
  int bufferSize = _scwprintf(iniFormat, installPath.get());
  ++bufferSize;  // Extra character for terminating null
  mozilla::UniquePtr<wchar_t[]> iniPath =
      mozilla::MakeUnique<wchar_t[]>(bufferSize);
  _snwprintf_s(iniPath.get(), bufferSize, _TRUNCATE, iniFormat,
               installPath.get());

  mozilla::UniquePtr<wchar_t[]> firefoxLocale;
  if (!GetString(iniPath.get(), L"locale", L"locale", firefoxLocale)) {
    return false;
  }

  return _wcsnicmp(windowsLocale.get(), L"en-", 3) == 0 &&
         _wcsnicmp(firefoxLocale.get(), L"en-", 3) == 0;
}

// If a notification is shown, this function will block until the notification
// is activated or dismissed.
// aumi is the App User Model ID.
void MaybeShowNotification(const DefaultBrowserInfo& browserInfo,
                           const wchar_t* aumi) {
  if (!mozilla::IsWin10OrLater() || !IsEnglish()) {
    // Notifications aren't shown in versions prior to Windows 10 because the
    // notification API we want isn't available.
    // They are also not shown in non-English contexts because localization is
    // not yet being done for this component.
    return;
  }

  bool initialNotificationShown = GetInitialNotificationShown();
  if (!initialNotificationShown) {
    if (browserInfo.currentDefaultBrowser == Browser::EdgeWithBlink &&
        browserInfo.previousDefaultBrowser == Browser::Firefox) {
      ShowNotification(NotificationType::Initial, aumi);
    }
    return;
  }

  ULONGLONG followupNotificationRequestTime =
      GetFollowupNotificationRequestTime();
  bool followupNotificationRequested = followupNotificationRequestTime != 0;
  bool followupNotificationShown = GetFollowupNotificationShown();
  if (followupNotificationRequested && !followupNotificationShown) {
    ULONGLONG secondsSinceRequestTime =
        SecondsPassedSince(followupNotificationRequestTime);

    if (secondsSinceRequestTime >= SEVEN_DAYS_IN_SECONDS) {
      ShowNotification(NotificationType::Followup, aumi);
    }
  }
}
