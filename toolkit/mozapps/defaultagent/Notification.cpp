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

// If the notification hasn't been activated or dismissed within 30 minutes,
// stop waiting for it.
#define NOTIFICATION_WAIT_TIMEOUT_MS (30 * 60 * 1000)
// If the mutex hasn't been released within a few minutes, something is wrong
// and we should give up on it
#define MUTEX_TIMEOUT_MS (10 * 60 * 1000)

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

static bool SetFollowupNotificationSuppressed(bool value) {
  return !RegistrySetValueBool(IsPrefixed::Unprefixed,
                               L"FollowupNotificationSuppressed", value)
              .isErr();
}

static bool GetFollowupNotificationSuppressed() {
  return RegistryGetValueBool(IsPrefixed::Unprefixed,
                              L"FollowupNotificationSuppressed")
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

// Returns true on success, false on failure.
static bool ConvertToWide(const mozilla::UniquePtr<char[]>& toConvert,
                          mozilla::UniquePtr<wchar_t[]>& result) {
  int bufferSize =
      MultiByteToWideChar(CP_UTF8, 0, toConvert.get(), -1, nullptr, 0);
  result = mozilla::MakeUnique<wchar_t[]>(bufferSize);
  int charsWritten = MultiByteToWideChar(CP_UTF8, 0, toConvert.get(), -1,
                                         result.get(), bufferSize);
  if (charsWritten == 0) {
    LOG_ERROR_MESSAGE(L"Unable to widen string: %#X", GetLastError());
    return false;
  }
  return true;
}

// Gets a string out of the specified INI file.
// Returns true on success, false on failure
static bool GetString(const wchar_t* iniPath, const char* section,
                      const char* key,
                      mozilla::UniquePtr<wchar_t[]>& toastString) {
  const unsigned int stringCount = 1;

  size_t keyLen = strlen(key);
  mozilla::UniquePtr<char[]> keyList = mozilla::MakeUnique<char[]>(keyLen + 2);
  strcpy(keyList.get(), key);
  keyList.get()[keyLen + 1] = '\0';

  mozilla::UniquePtr<char[]> narrowString;
  int result =
      ReadStrings(iniPath, keyList.get(), stringCount, &narrowString, section);
  if (result != OK) {
    LOG_ERROR_MESSAGE(
        L"Unable to retrieve INI string: section=%S, key=%S, result=%d",
        section, key, result);
    return false;
  }
  return ConvertToWide(narrowString, toastString);
}

// Gets all strings out of the localized INI file.
// Note that, at this time, the INI is not actually being localized - it only
// exists in English. This is why MaybeShowNotification() checks IsEnglish().
// See Bug 1621696 for more information.
// Returns true on success, false on failure
static bool GetStrings(Strings& strings) {
  mozilla::UniquePtr<wchar_t[]> installPath;
  bool success = GetInstallDirectory(installPath);
  if (!success) {
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
  mozilla::UniquePtr<char[]> localizedStrings[stringCount];
  int result = ReadStrings(iniPath.get(), keys, stringCount, localizedStrings);
  if (result != OK) {
    LOG_ERROR_MESSAGE(L"Unable to read localized strings: %d", result);
    return false;
  }

  return ConvertToWide(localizedStrings[0], strings.initialToast.text1) &&
         ConvertToWide(localizedStrings[1], strings.initialToast.text2) &&
         ConvertToWide(localizedStrings[2], strings.initialToast.action1) &&
         ConvertToWide(localizedStrings[3], strings.initialToast.action2) &&
         GetString(iniPath.get(), "Nonlocalized",
                   "InitialToastRelativeImagePath",
                   strings.initialToast.relImagePath) &&
         ConvertToWide(localizedStrings[0], strings.followupToast.text1) &&
         ConvertToWide(localizedStrings[1], strings.followupToast.text2) &&
         ConvertToWide(localizedStrings[4], strings.followupToast.action1) &&
         ConvertToWide(localizedStrings[3], strings.followupToast.action2) &&
         GetString(iniPath.get(), "Nonlocalized",
                   "FollowupToastRelativeImagePath",
                   strings.followupToast.relImagePath);
}

// This encapsulates the data that needs to be protected by a mutex because it
// will be shared by the main thread and the handler thread.
// To ensure the data is only written once, handlerDataHasBeenSet should be
// initialized to false, then set to true when the handler writes data into the
// structure.
struct HandlerData {
  NotificationActivities activitiesPerformed;
  bool handlerDataHasBeenSet;
};

// The value that ToastHandler writes into should be a global. We can't control
// when ToastHandler is called, and if this value isn't a global, ToastHandler
// may be called and attempt to access this after it has been deconstructed.
// Since this value is accessed by the handler thread and the main thread, it
// is protected by a mutex (gHandlerMutex).
// Since ShowNotification deconstructs the mutex, it might seem like once
// ShowNotification exits, we can just rely on the inability to wait on an
// invalid mutex to protect the deconstructed data, but it's possible that
// we could deconstruct the mutex while the handler is holding it and is
// already accessing the protected data.
static HandlerData gHandlerReturnData;
static HANDLE gHandlerMutex = INVALID_HANDLE_VALUE;

class ToastHandler : public WinToastLib::IWinToastHandler {
 private:
  NotificationType mWhichNotification;
  HANDLE mEvent;

 public:
  ToastHandler(NotificationType whichNotification, HANDLE event) {
    mWhichNotification = whichNotification;
    mEvent = event;
  }

  void FinishHandler(NotificationActivities& returnData) const {
    SetReturnData(returnData);

    BOOL success = SetEvent(mEvent);
    if (!success) {
      LOG_ERROR_MESSAGE(L"Event could not be set: %#X", GetLastError());
    }
  }

  void SetReturnData(NotificationActivities& toSet) const {
    DWORD result = WaitForSingleObject(gHandlerMutex, MUTEX_TIMEOUT_MS);
    if (result == WAIT_TIMEOUT) {
      LOG_ERROR_MESSAGE(L"Unable to obtain mutex ownership");
      return;
    } else if (result == WAIT_FAILED) {
      LOG_ERROR_MESSAGE(L"Failed to wait on mutex: %#X", GetLastError());
      return;
    } else if (result == WAIT_ABANDONED) {
      LOG_ERROR_MESSAGE(L"Found abandoned mutex");
      ReleaseMutex(gHandlerMutex);
      return;
    }

    // Only set this data once
    if (!gHandlerReturnData.handlerDataHasBeenSet) {
      gHandlerReturnData.activitiesPerformed = toSet;
      gHandlerReturnData.handlerDataHasBeenSet = true;
    }

    BOOL success = ReleaseMutex(gHandlerMutex);
    if (!success) {
      LOG_ERROR_MESSAGE(L"Unable to release mutex ownership: %#X",
                        GetLastError());
    }
  }

  void toastActivated() const override {
    NotificationActivities activitiesPerformed;
    activitiesPerformed.type = mWhichNotification;
    activitiesPerformed.shown = NotificationShown::Shown;
    activitiesPerformed.action = NotificationAction::ToastClicked;

    LaunchModernSettingsDialogDefaultApps();

    FinishHandler(activitiesPerformed);
  }

  void toastActivated(int actionIndex) const override {
    NotificationActivities activitiesPerformed;
    activitiesPerformed.type = mWhichNotification;
    activitiesPerformed.shown = NotificationShown::Shown;
    // Override this below
    activitiesPerformed.action = NotificationAction::NoAction;

    if (actionIndex == 0) {
      if (mWhichNotification == NotificationType::Initial) {
        // "Remind me later" button
        activitiesPerformed.action = NotificationAction::RemindMeLater;
        if (!SetFollowupNotificationRequestTime(GetCurrentTimestamp())) {
          LOG_ERROR_MESSAGE(L"Unable to schedule followup notification");
        }
      } else {
        // "Don't ask again" button
        // Do nothing. As long as we don't call
        // SetFollowupNotificationRequestTime, there will be no followup
        // notification.
        activitiesPerformed.action = NotificationAction::DismissedByButton;
      }
    } else if (actionIndex == 1) {
      // "Make Firefox the default" button, on both notifications.
      activitiesPerformed.action = NotificationAction::MakeFirefoxDefaultButton;
      LaunchModernSettingsDialogDefaultApps();
    }

    FinishHandler(activitiesPerformed);
  }

  void toastDismissed(WinToastDismissalReason state) const override {
    NotificationActivities activitiesPerformed;
    activitiesPerformed.type = mWhichNotification;
    activitiesPerformed.shown = NotificationShown::Shown;
    // Override this below
    activitiesPerformed.action = NotificationAction::NoAction;

    if (state == WinToastDismissalReason::TimedOut) {
      activitiesPerformed.action = NotificationAction::DismissedByTimeout;
    } else if (state == WinToastDismissalReason::ApplicationHidden) {
      activitiesPerformed.action =
          NotificationAction::DismissedByApplicationHidden;
    } else if (state == WinToastDismissalReason::UserCanceled) {
      activitiesPerformed.action = NotificationAction::DismissedToActionCenter;
    }

    FinishHandler(activitiesPerformed);
  }

  void toastFailed() const override {
    NotificationActivities activitiesPerformed;
    activitiesPerformed.type = mWhichNotification;
    activitiesPerformed.shown = NotificationShown::Error;
    activitiesPerformed.action = NotificationAction::NoAction;

    LOG_ERROR_MESSAGE(L"Toast notification failed to display");
    FinishHandler(activitiesPerformed);
  }
};

// This function blocks until the shown notification is activated or dismissed.
static NotificationActivities ShowNotification(
    NotificationType whichNotification, const wchar_t* aumi) {
  // Initially set the value that will be returned to error. If the notification
  // is shown successfully, we'll update it.
  NotificationActivities activitiesPerformed = {whichNotification,
                                                NotificationShown::Error,
                                                NotificationAction::NoAction};
  using namespace WinToastLib;

  if (!WinToast::isCompatible()) {
    LOG_ERROR_MESSAGE(L"System is not compatible with WinToast");
    return activitiesPerformed;
  }

  WinToast::instance()->setAppName(L"" MOZ_APP_BASENAME);
  std::wstring aumiStr = aumi;
  WinToast::instance()->setAppUserModelId(aumiStr);
  WinToast::WinToastError error;
  if (!WinToast::instance()->initialize(&error)) {
    LOG_ERROR_MESSAGE(WinToast::strerror(error).c_str());
    return activitiesPerformed;
  }

  Strings strings;
  if (!GetStrings(strings)) {
    return activitiesPerformed;
  }
  const ToastStrings* toastStrings = strings.GetToastStrings(whichNotification);

  // This event object will let the handler notify us when it has handled the
  // notification.
  nsAutoHandle event(CreateEventW(nullptr, TRUE, FALSE, nullptr));
  if (event.get() == nullptr) {
    LOG_ERROR_MESSAGE(L"Unable to create event object: %#X", GetLastError());
    return activitiesPerformed;
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
    return activitiesPerformed;
  }

  // We need the absolute image path, not the relative path.
  mozilla::UniquePtr<wchar_t[]> installPath;
  success = GetInstallDirectory(installPath);
  if (!success) {
    LOG_ERROR_MESSAGE(L"Failed to get install directory for the image path");
    return activitiesPerformed;
  }
  const wchar_t* absPathFormat = L"%s\\%s";
  int bufferSize = _scwprintf(absPathFormat, installPath.get(),
                              toastStrings->relImagePath.get());
  ++bufferSize;  // Extra character for terminating null
  mozilla::UniquePtr<wchar_t[]> absImagePath =
      mozilla::MakeUnique<wchar_t[]>(bufferSize);
  _snwprintf_s(absImagePath.get(), bufferSize, _TRUNCATE, absPathFormat,
               installPath.get(), toastStrings->relImagePath.get());

  // This is used to protect gHandlerReturnData.
  gHandlerMutex = CreateMutexW(nullptr, TRUE, nullptr);
  if (gHandlerMutex == nullptr) {
    LOG_ERROR_MESSAGE(L"Unable to create mutex: %#X", GetLastError());
    return activitiesPerformed;
  }
  // Automatically close this mutex when this function exits.
  nsAutoHandle autoMutex(gHandlerMutex);
  // No need to initialize gHandlerReturnData.activitiesPerformed, since it will
  // be set by the handler. But we do need to initialize
  // gHandlerReturnData.handlerDataHasBeenSet so the handler knows that no data
  // has been set yet.
  gHandlerReturnData.handlerDataHasBeenSet = false;
  success = ReleaseMutex(gHandlerMutex);
  if (!success) {
    LOG_ERROR_MESSAGE(L"Unable to release mutex ownership: %#X",
                      GetLastError());
  }

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
    return activitiesPerformed;
  }

  DWORD result = WaitForSingleObject(event.get(), NOTIFICATION_WAIT_TIMEOUT_MS);
  // Don't return after these errors. Attempt to hide the notification.
  if (result == WAIT_FAILED) {
    LOG_ERROR_MESSAGE(L"Unable to wait on event object: %#X", GetLastError());
  } else if (result == WAIT_TIMEOUT) {
    LOG_ERROR_MESSAGE(L"Timed out waiting for event object");
  } else {
    result = WaitForSingleObject(gHandlerMutex, MUTEX_TIMEOUT_MS);
    if (result == WAIT_TIMEOUT) {
      LOG_ERROR_MESSAGE(L"Unable to obtain mutex ownership");
      // activitiesPerformed is already set to error. No change needed.
    } else if (result == WAIT_FAILED) {
      LOG_ERROR_MESSAGE(L"Failed to wait on mutex: %#X", GetLastError());
      // activitiesPerformed is already set to error. No change needed.
    } else if (result == WAIT_ABANDONED) {
      LOG_ERROR_MESSAGE(L"Found abandoned mutex");
      ReleaseMutex(gHandlerMutex);
      // activitiesPerformed is already set to error. No change needed.
    } else {
      // Mutex is being held. It is safe to access gHandlerReturnData.
      // If gHandlerReturnData.handlerDataHasBeenSet is false, the handler never
      // ran. Use the error value activitiesPerformed already contains.
      if (gHandlerReturnData.handlerDataHasBeenSet) {
        activitiesPerformed = gHandlerReturnData.activitiesPerformed;
      }

      success = ReleaseMutex(gHandlerMutex);
      if (!success) {
        LOG_ERROR_MESSAGE(L"Unable to release mutex ownership: %#X",
                          GetLastError());
      }
    }
  }

  if (!WinToast::instance()->hideToast(id)) {
    LOG_ERROR_MESSAGE(L"Failed to hide notification");
  }
  return activitiesPerformed;
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
  bool success = GetInstallDirectory(installPath);
  if (!success) {
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
  if (!GetString(iniPath.get(), "locale", "locale", firefoxLocale)) {
    return false;
  }

  return _wcsnicmp(windowsLocale.get(), L"en-", 3) == 0 &&
         _wcsnicmp(firefoxLocale.get(), L"en-", 3) == 0;
}

// If a notification is shown, this function will block until the notification
// is activated or dismissed.
// aumi is the App User Model ID.
NotificationActivities MaybeShowNotification(
    const DefaultBrowserInfo& browserInfo, const wchar_t* aumi) {
  // Default to not showing a notification. Any other value will be returned
  // directly from ShowNotification.
  NotificationActivities activitiesPerformed = {NotificationType::Initial,
                                                NotificationShown::NotShown,
                                                NotificationAction::NoAction};

  if (!mozilla::IsWin10OrLater() || !IsEnglish()) {
    // Notifications aren't shown in versions prior to Windows 10 because the
    // notification API we want isn't available.
    // They are also not shown in non-English contexts because localization is
    // not yet being done for this component.
    return activitiesPerformed;
  }

  bool initialNotificationShown = GetInitialNotificationShown();
  if (!initialNotificationShown) {
    if (browserInfo.currentDefaultBrowser == Browser::EdgeWithBlink &&
        browserInfo.previousDefaultBrowser == Browser::Firefox) {
      return ShowNotification(NotificationType::Initial, aumi);
    }
    return activitiesPerformed;
  }
  activitiesPerformed.type = NotificationType::Followup;

  ULONGLONG followupNotificationRequestTime =
      GetFollowupNotificationRequestTime();
  bool followupNotificationRequested = followupNotificationRequestTime != 0;
  bool followupNotificationShown = GetFollowupNotificationShown();
  if (followupNotificationRequested && !followupNotificationShown &&
      !GetFollowupNotificationSuppressed()) {
    ULONGLONG secondsSinceRequestTime =
        SecondsPassedSince(followupNotificationRequestTime);

    if (secondsSinceRequestTime >= SEVEN_DAYS_IN_SECONDS) {
      // If we go to show the followup notification and the user has already
      // changed the default browser, permanently suppress the followup since
      // it's no longer relevant.
      if (browserInfo.currentDefaultBrowser == Browser::EdgeWithBlink) {
        return ShowNotification(NotificationType::Followup, aumi);
      } else {
        SetFollowupNotificationSuppressed(true);
      }
    }
  }
  return activitiesPerformed;
}

std::string GetStringForNotificationType(NotificationType type) {
  switch (type) {
    case NotificationType::Initial:
      return std::string("initial");
    case NotificationType::Followup:
      return std::string("followup");
  }
}

std::string GetStringForNotificationShown(NotificationShown shown) {
  switch (shown) {
    case NotificationShown::NotShown:
      return std::string("not-shown");
    case NotificationShown::Shown:
      return std::string("shown");
    case NotificationShown::Error:
      return std::string("error");
  }
}

std::string GetStringForNotificationAction(NotificationAction action) {
  switch (action) {
    case NotificationAction::DismissedByTimeout:
      return std::string("dismissed-by-timeout");
    case NotificationAction::DismissedToActionCenter:
      return std::string("dismissed-to-action-center");
    case NotificationAction::DismissedByButton:
      return std::string("dismissed-by-button");
    case NotificationAction::DismissedByApplicationHidden:
      return std::string("dismissed-by-application-hidden");
    case NotificationAction::RemindMeLater:
      return std::string("remind-me-later");
    case NotificationAction::MakeFirefoxDefaultButton:
      return std::string("make-firefox-default-button");
    case NotificationAction::ToastClicked:
      return std::string("toast-clicked");
    case NotificationAction::NoAction:
      return std::string("no-action");
  }
}
