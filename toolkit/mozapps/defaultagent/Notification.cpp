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

#include "mozilla/ArrayUtils.h"
#include "mozilla/CmdLineAndEnvUtils.h"
#include "mozilla/UniquePtr.h"
#include "mozilla/Unused.h"
#include "mozilla/WinHeaderOnlyUtils.h"
#include "nsWindowsHelpers.h"
#include "readstrings.h"
#include "updatererrors.h"
#include "WindowsDefaultBrowser.h"

#include "common.h"
#include "DefaultBrowser.h"
#include "EventLog.h"
#include "Registry.h"
#include "SetDefaultBrowser.h"

#include "wintoastlib.h"

#define SEVEN_DAYS_IN_SECONDS (7 * 24 * 60 * 60)

// If the notification hasn't been activated or dismissed within 12 hours,
// stop waiting for it.
#define NOTIFICATION_WAIT_TIMEOUT_MS (12 * 60 * 60 * 1000)
// If the mutex hasn't been released within a few minutes, something is wrong
// and we should give up on it
#define MUTEX_TIMEOUT_MS (10 * 60 * 1000)

bool FirefoxInstallIsEnglish();

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

static bool ResetInitialNotificationShown() {
  return RegistryDeleteValue(IsPrefixed::Unprefixed,
                             L"InitialNotificationShown")
      .isOk();
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

// Returns 0 if no value is set.
static ULONGLONG GetFollowupNotificationRequestTime() {
  return RegistryGetValueQword(IsPrefixed::Unprefixed, L"FollowupRequestTime")
      .unwrapOr(mozilla::Some(0))
      .valueOr(0);
}

// Returns false if no value is set.
static bool GetPrefSetDefaultBrowserUserChoice() {
  return RegistryGetValueBool(IsPrefixed::Prefixed,
                              L"SetDefaultBrowserUserChoice")
      .unwrapOr(mozilla::Some(false))
      .valueOr(false);
}

struct ToastStrings {
  mozilla::UniquePtr<wchar_t[]> text1;
  mozilla::UniquePtr<wchar_t[]> text2;
  mozilla::UniquePtr<wchar_t[]> action1;
  mozilla::UniquePtr<wchar_t[]> action2;
  mozilla::UniquePtr<wchar_t[]> relImagePath;
};

struct Strings {
  // Toast notification button text is hard to localize because it tends to
  // overflow. Thus, we have 3 different toast notifications:
  //  - The initial notification, which includes a button with text like
  //    "Ask me later". Since we cannot easily localize this, we will display
  //    it only in English.
  //  - The followup notification, to be shown if the user clicked "Ask me
  //    later". Since we only have that button in English, we only need this
  //    notification in English.
  //  - The localized notification, which has much shorter button text to
  //    (hopefully) prevent overflow: just "Yes" and "No". Since we no longer
  //    have an "Ask me later" button, a followup localized notification is not
  //    needed.
  ToastStrings initialToast;
  ToastStrings followupToast;
  ToastStrings localizedToast;

  // Returned pointer points within this struct and should not be freed.
  const ToastStrings* GetToastStrings(NotificationType whichToast,
                                      bool englishStrings) const {
    if (!englishStrings) {
      return &localizedToast;
    }
    if (whichToast == NotificationType::Initial) {
      return &initialToast;
    }
    return &followupToast;
  }
};

// Gets all strings out of the relevant INI files.
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

  IniReader stringsReader(iniPath.get());
  stringsReader.AddKey("DefaultBrowserNotificationTitle",
                       &strings.initialToast.text1);
  stringsReader.AddKey("DefaultBrowserNotificationTitle",
                       &strings.followupToast.text1);
  stringsReader.AddKey("DefaultBrowserNotificationText",
                       &strings.initialToast.text2);
  stringsReader.AddKey("DefaultBrowserNotificationText",
                       &strings.followupToast.text2);
  stringsReader.AddKey("DefaultBrowserNotificationMakeFirefoxDefault",
                       &strings.initialToast.action1);
  stringsReader.AddKey("DefaultBrowserNotificationMakeFirefoxDefault",
                       &strings.followupToast.action1);
  stringsReader.AddKey("DefaultBrowserNotificationDontShowAgain",
                       &strings.initialToast.action2);
  stringsReader.AddKey("DefaultBrowserNotificationDontShowAgain",
                       &strings.followupToast.action2);
  int result = stringsReader.Read();
  if (result != OK) {
    LOG_ERROR_MESSAGE(L"Unable to read English strings: %d", result);
    return false;
  }

  const wchar_t* localizedIniFormat = L"%s\\defaultagent_localized.ini";
  bufferSize = _scwprintf(localizedIniFormat, installPath.get());
  ++bufferSize;  // Extra character for terminating null
  mozilla::UniquePtr<wchar_t[]> localizedIniPath =
      mozilla::MakeUnique<wchar_t[]>(bufferSize);
  _snwprintf_s(localizedIniPath.get(), bufferSize, _TRUNCATE,
               localizedIniFormat, installPath.get());

  IniReader localizedReader(localizedIniPath.get());
  localizedReader.AddKey("DefaultBrowserNotificationHeaderText",
                         &strings.localizedToast.text1);
  localizedReader.AddKey("DefaultBrowserNotificationBodyText",
                         &strings.localizedToast.text2);
  localizedReader.AddKey("DefaultBrowserNotificationYesButtonText",
                         &strings.localizedToast.action1);
  localizedReader.AddKey("DefaultBrowserNotificationNoButtonText",
                         &strings.localizedToast.action2);
  result = localizedReader.Read();
  if (result != OK) {
    LOG_ERROR_MESSAGE(L"Unable to read localized strings: %d", result);
    return false;
  }

  // IniReader is only capable of reading from one section at a time, so we need
  // to make another one to read the other section.
  IniReader nonlocalizedReader(iniPath.get(), "Nonlocalized");
  nonlocalizedReader.AddKey("InitialToastRelativeImagePath",
                            &strings.initialToast.relImagePath);
  nonlocalizedReader.AddKey("FollowupToastRelativeImagePath",
                            &strings.followupToast.relImagePath);
  nonlocalizedReader.AddKey("LocalizedToastRelativeImagePath",
                            &strings.localizedToast.relImagePath);
  result = nonlocalizedReader.Read();
  if (result != OK) {
    LOG_ERROR_MESSAGE(L"Unable to read non-localized strings: %d", result);
    return false;
  }

  return true;
}

static mozilla::WindowsError LaunchFirefoxToHandleDefaultBrowserAgent() {
  // Could also be `MOZ_APP_NAME.exe`, but there's no generality to be gained:
  // the WDBA is Firefox-only.
  FilePathResult firefoxPathResult = GetRelativeBinaryPath(L"firefox.exe");
  if (firefoxPathResult.isErr()) {
    return firefoxPathResult.unwrapErr();
  }
  std::wstring firefoxPath = firefoxPathResult.unwrap();

  const wchar_t* firefoxArgs[] = {firefoxPath.c_str(),
                                  L"-to-handle-default-browser-agent"};
  mozilla::UniquePtr<wchar_t[]> firefoxCmdLine(mozilla::MakeCommandLine(
      mozilla::ArrayLength(firefoxArgs), const_cast<wchar_t**>(firefoxArgs)));

  PROCESS_INFORMATION pi;
  STARTUPINFOW si = {sizeof(si)};
  if (!::CreateProcessW(firefoxPath.c_str(), firefoxCmdLine.get(), nullptr,
                        nullptr, false,
                        DETACHED_PROCESS | NORMAL_PRIORITY_CLASS, nullptr,
                        nullptr, &si, &pi)) {
    HRESULT hr = HRESULT_FROM_WIN32(GetLastError());
    LOG_ERROR(hr);
    return mozilla::WindowsError::FromHResult(hr);
  }

  CloseHandle(pi.hThread);
  CloseHandle(pi.hProcess);

  return mozilla::WindowsError::CreateSuccess();
}

/*
 * Set the default browser.
 *
 * First check if we can directly write UserChoice, if so attempt that.
 * If we can't write UserChoice, or if the attempt fails, fall back to
 * showing the Default Apps page of Settings.
 *
 * @param aAumi The AUMI of the installation to set as default.
 *
 * @return Success (SUCCEEDED(hr)) if all associations were set with
 *         UserChoice and checked successfully.
 *         Other return codes indicate a failure which causes us to
 *         fall back to Settings, see return codes of
 *         SetDefaultBrowserUserChoice().
 */
static HRESULT SetDefaultBrowserFromNotification(const wchar_t* aumi) {
  // TODO maybe fall back to protocol dialog on Windows 11 (bug 1719832)?

  HRESULT hr = E_FAIL;
  if (GetPrefSetDefaultBrowserUserChoice()) {
    hr = SetDefaultBrowserUserChoice(aumi);
  }

  if (!FAILED(hr)) {
    mozilla::Unused << LaunchFirefoxToHandleDefaultBrowserAgent();
  } else {
    LOG_ERROR_MESSAGE(L"Failed to SetDefaultBrowserUserChoice: %#X",
                      GetLastError());
    LaunchModernSettingsDialogDefaultApps();
  }
  return hr;
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
  const std::wstring mAumiStr;

 public:
  ToastHandler(NotificationType whichNotification, HANDLE event,
               const wchar_t* aumi)
      : mWhichNotification(whichNotification), mEvent(event), mAumiStr(aumi) {}

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

    // Notification strings are written to indicate the default browser is
    // restored to Firefox when the notification body is clicked to prevent
    // ambiguity when buttons aren't pressed.
    SetDefaultBrowserFromNotification(mAumiStr.c_str());

    FinishHandler(activitiesPerformed);
  }

  void toastActivated(int actionIndex) const override {
    NotificationActivities activitiesPerformed;
    activitiesPerformed.type = mWhichNotification;
    activitiesPerformed.shown = NotificationShown::Shown;
    // Override this below
    activitiesPerformed.action = NotificationAction::NoAction;

    if (actionIndex == 0) {
      // "Make Firefox the default" button, on both the initial and followup
      // notifications. "Yes" button on the localized notification.
      activitiesPerformed.action = NotificationAction::MakeFirefoxDefaultButton;

      SetDefaultBrowserFromNotification(mAumiStr.c_str());
    } else if (actionIndex == 1) {
      // Do nothing. As long as we don't call
      // SetFollowupNotificationRequestTime, there will be no followup
      // notification.
      activitiesPerformed.action = NotificationAction::DismissedByButton;
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

  WinToast::instance()->setAppName(L"" MOZ_APP_DISPLAYNAME);
  std::wstring aumiStr = aumi;
  WinToast::instance()->setAppUserModelId(aumiStr);
  WinToast::instance()->setShortcutPolicy(
      WinToastLib::WinToast::SHORTCUT_POLICY_REQUIRE_NO_CREATE);
  WinToast::WinToastError error;
  if (!WinToast::instance()->initialize(&error)) {
    LOG_ERROR_MESSAGE(WinToast::strerror(error).c_str());
    return activitiesPerformed;
  }

  bool isEnglishInstall = FirefoxInstallIsEnglish();

  Strings strings;
  if (!GetStrings(strings)) {
    return activitiesPerformed;
  }
  const ToastStrings* toastStrings =
      strings.GetToastStrings(whichNotification, isEnglishInstall);

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
  toastTemplate.setScenario(WinToastTemplate::Scenario::Reminder);
  ToastHandler* handler =
      new ToastHandler(whichNotification, event.get(), aumi);
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

// Previously this function checked that the Firefox build was using English.
// This was checked because of the peculiar way we were localizing toast
// notifications where we used a completely different set of strings in English.
//
// We've since unified the notification flows but need to clean up unused code
// and config files - Bug 1826375.
bool FirefoxInstallIsEnglish() { return false; }

// If a notification is shown, this function will block until the notification
// is activated or dismissed.
// aumi is the App User Model ID.
NotificationActivities MaybeShowNotification(
    const DefaultBrowserInfo& browserInfo, const wchar_t* aumi, bool force) {
  // Default to not showing a notification. Any other value will be returned
  // directly from ShowNotification.
  NotificationActivities activitiesPerformed = {NotificationType::Initial,
                                                NotificationShown::NotShown,
                                                NotificationAction::NoAction};

  // Reset notification state machine, user setting default browser to Firefox
  // is a strong signal that they intend to have it as the default browser.
  if (browserInfo.currentDefaultBrowser == Browser::Firefox) {
    ResetInitialNotificationShown();
  }

  bool initialNotificationShown = GetInitialNotificationShown();
  if (!initialNotificationShown || force) {
    if ((browserInfo.currentDefaultBrowser == Browser::EdgeWithBlink &&
         browserInfo.previousDefaultBrowser == Browser::Firefox) ||
        force) {
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

void EnsureValidNotificationAction(std::string& actionString) {
  if (actionString != "dismissed-by-timeout" &&
      actionString != "dismissed-to-action-center" &&
      actionString != "dismissed-by-button" &&
      actionString != "dismissed-by-application-hidden" &&
      actionString != "remind-me-later" &&
      actionString != "make-firefox-default-button" &&
      actionString != "toast-clicked" && actionString != "no-action") {
    actionString = "no-action";
  }
}
