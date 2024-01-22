/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include <windows.h>
#include <shlwapi.h>
#include <objbase.h>
#include <string.h>
#include <vector>

#include "nsAutoRef.h"
#include "nsDebug.h"
#include "nsProxyRelease.h"
#include "nsWindowsHelpers.h"
#include "nsString.h"

#include "common.h"
#include "DefaultBrowser.h"
#include "DefaultPDF.h"
#include "EventLog.h"
#include "Notification.h"
#include "Policy.h"
#include "Registry.h"
#include "ScheduledTask.h"
#include "ScheduledTaskRemove.h"
#include "SetDefaultBrowser.h"
#include "Telemetry.h"
#include "xpcpublic.h"
#include "mozilla/dom/Promise.h"
#include "mozilla/ErrorResult.h"

#include "DefaultAgent.h"

// The AGENT_REGKEY_NAME is dependent on MOZ_APP_VENDOR and MOZ_APP_BASENAME,
// so using those values in the mutex name prevents waiting on processes that
// are using completely different data.
#define REGISTRY_MUTEX_NAME \
  L"" MOZ_APP_VENDOR MOZ_APP_BASENAME L"DefaultBrowserAgentRegistryMutex"
// How long to wait on the registry mutex before giving up on it. This should
// be short. Although the WDBA runs in the background, uninstallation happens
// synchronously in the foreground.
#define REGISTRY_MUTEX_TIMEOUT_MS (3 * 1000)

namespace mozilla::default_agent {

// This class is designed to prevent concurrency problems when accessing the
// registry. It should be acquired before any usage of unprefixed registry
// entries.
class RegistryMutex {
 private:
  nsAutoHandle mMutex;
  bool mLocked;

 public:
  RegistryMutex() : mMutex(nullptr), mLocked(false) {}
  ~RegistryMutex() {
    Release();
    // nsAutoHandle will take care of closing the mutex's handle.
  }

  // Returns true on success, false on failure.
  bool Acquire() {
    if (mLocked) {
      return true;
    }

    if (mMutex.get() == nullptr) {
      // It seems like we would want to set the second parameter (bInitialOwner)
      // to TRUE, but the documentation for CreateMutexW suggests that, because
      // we aren't sure that the mutex doesn't already exist, we can't be sure
      // whether we got ownership via this mechanism.
      mMutex.own(CreateMutexW(nullptr, FALSE, REGISTRY_MUTEX_NAME));
      if (mMutex.get() == nullptr) {
        LOG_ERROR_MESSAGE(L"Couldn't open registry mutex: %#X", GetLastError());
        return false;
      }
    }

    DWORD mutexStatus =
        WaitForSingleObject(mMutex.get(), REGISTRY_MUTEX_TIMEOUT_MS);
    if (mutexStatus == WAIT_OBJECT_0) {
      mLocked = true;
    } else if (mutexStatus == WAIT_TIMEOUT) {
      LOG_ERROR_MESSAGE(L"Timed out waiting for registry mutex");
    } else if (mutexStatus == WAIT_ABANDONED) {
      // This isn't really an error for us. No one else is using the registry.
      // This status code means that we are supposed to check our data for
      // consistency, but there isn't really anything we can fix here.
      // This is an indication that an agent crashed though, which is clearly an
      // error, so log an error message.
      LOG_ERROR_MESSAGE(L"Found abandoned registry mutex. Continuing...");
      mLocked = true;
    } else {
      // The only other documented status code is WAIT_FAILED. In the case that
      // we somehow get some other code, that is also an error.
      LOG_ERROR_MESSAGE(L"Failed to wait on registry mutex: %#X",
                        GetLastError());
    }
    return mLocked;
  }

  bool IsLocked() { return mLocked; }

  void Release() {
    if (mLocked) {
      if (mMutex.get() == nullptr) {
        LOG_ERROR_MESSAGE(L"Unexpectedly missing registry mutex");
        return;
      }
      BOOL success = ReleaseMutex(mMutex.get());
      if (!success) {
        LOG_ERROR_MESSAGE(L"Failed to release registry mutex");
      }
      mLocked = false;
    }
  }
};

// Returns true if the registry value name given is one of the
// install-directory-prefixed values used by the Windows Default Browser Agent.
// ex: "C:\Program Files\Mozilla Firefox|PreviousDefault"
//     Returns true
// ex: "InitialNotificationShown"
//     Returns false
static bool IsPrefixedValueName(const wchar_t* valueName) {
  // Prefixed value names use '|' as a delimiter. None of the
  // non-install-directory-prefixed value names contain one.
  return wcschr(valueName, L'|') != nullptr;
}

static void RemoveAllRegistryEntries() {
  mozilla::UniquePtr<wchar_t[]> installPath = mozilla::GetFullBinaryPath();
  if (!PathRemoveFileSpecW(installPath.get())) {
    return;
  }

  HKEY rawRegKey = nullptr;
  if (ERROR_SUCCESS !=
      RegOpenKeyExW(HKEY_CURRENT_USER, AGENT_REGKEY_NAME, 0,
                    KEY_WRITE | KEY_QUERY_VALUE | KEY_WOW64_64KEY,
                    &rawRegKey)) {
    return;
  }
  nsAutoRegKey regKey(rawRegKey);

  DWORD maxValueNameLen = 0;
  if (ERROR_SUCCESS != RegQueryInfoKeyW(regKey.get(), nullptr, nullptr, nullptr,
                                        nullptr, nullptr, nullptr, nullptr,
                                        &maxValueNameLen, nullptr, nullptr,
                                        nullptr)) {
    return;
  }
  // The length that RegQueryInfoKeyW returns is without a terminator.
  maxValueNameLen += 1;

  mozilla::UniquePtr<wchar_t[]> valueName =
      mozilla::MakeUnique<wchar_t[]>(maxValueNameLen);

  DWORD valueIndex = 0;
  // Set this to true if we encounter values in this key that are prefixed with
  // different install directories, indicating that this key is still in use
  // by other installs.
  bool keyStillInUse = false;

  while (true) {
    DWORD valueNameLen = maxValueNameLen;
    LSTATUS ls =
        RegEnumValueW(regKey.get(), valueIndex, valueName.get(), &valueNameLen,
                      nullptr, nullptr, nullptr, nullptr);
    if (ls != ERROR_SUCCESS) {
      break;
    }

    if (!wcsnicmp(valueName.get(), installPath.get(),
                  wcslen(installPath.get()))) {
      RegDeleteValueW(regKey.get(), valueName.get());
      // Only increment the index if we did not delete this value, because if
      // we did then the indexes of all the values after that one just got
      // decremented, meaning the index we already have now refers to a value
      // that we haven't looked at yet.
    } else {
      valueIndex++;
      if (IsPrefixedValueName(valueName.get())) {
        // If this is not one of the unprefixed value names, it must be one of
        // the install-directory prefixed values.
        keyStillInUse = true;
      }
    }
  }

  regKey.reset();

  // If no other installs are using this key, remove it now.
  if (!keyStillInUse) {
    // Use RegDeleteTreeW to remove the cache as well, which is in subkey.
    RegDeleteTreeW(HKEY_CURRENT_USER, AGENT_REGKEY_NAME);
  }
}

// This function adds a registry value with this format:
//    <install-dir>|Installed=1
// RemoveAllRegistryEntries() determines whether the registry key is in use
// by other installations by checking for install-directory-prefixed value
// names. Although Firefox mirrors some preferences into install-directory-
// prefixed values, the WDBA no longer uses any prefixed values. Adding this one
// makes uninstallation work as expected slightly more reliably.
static void WriteInstallationRegistryEntry() {
  mozilla::WindowsErrorResult<mozilla::Ok> result =
      RegistrySetValueBool(IsPrefixed::Prefixed, L"Installed", true);
  if (result.isErr()) {
    LOG_ERROR_MESSAGE(L"Failed to write installation registry entry: %#X",
                      result.unwrapErr().AsHResult());
  }
}

// Returns false (without setting aResult) if reading last run time failed.
static bool CheckIfAppRanRecently(bool* aResult) {
  const ULONGLONG kTaskExpirationDays = 90;
  const ULONGLONG kTaskExpirationSeconds = kTaskExpirationDays * 24 * 60 * 60;

  MaybeQwordResult lastRunTimeResult =
      RegistryGetValueQword(IsPrefixed::Prefixed, L"AppLastRunTime");
  if (lastRunTimeResult.isErr()) {
    return false;
  }
  mozilla::Maybe<ULONGLONG> lastRunTimeMaybe = lastRunTimeResult.unwrap();
  if (!lastRunTimeMaybe.isSome()) {
    return false;
  }

  ULONGLONG secondsSinceLastRunTime =
      SecondsPassedSince(lastRunTimeMaybe.value());

  *aResult = secondsSinceLastRunTime < kTaskExpirationSeconds;
  return true;
}

// Use the macro to inject all of the definitions for nsISupports.
NS_IMPL_ISUPPORTS(DefaultAgent, nsIDefaultAgent)

NS_IMETHODIMP
DefaultAgent::RegisterTask(const nsAString& aUniqueToken) {
  // We aren't actually going to check whether we got the mutex here.
  // Ideally we would acquire it since registration might migrate registry
  // entries. But it is preferable to ignore a mutex wait timeout here
  // because:
  //   1. Otherwise the task doesn't get registered at all
  //   2. If another installation's agent is holding the mutex, it either
  //      is far enough out of date that it doesn't yet use the migrated
  //      values, or it already did the migration for us.
  RegistryMutex regMutex;
  regMutex.Acquire();

  WriteInstallationRegistryEntry();

  HRESULT hr =
      default_agent::RegisterTask(PromiseFlatString(aUniqueToken).get());
  return SUCCEEDED(hr) ? NS_OK : NS_ERROR_FAILURE;
}

NS_IMETHODIMP
DefaultAgent::UpdateTask(const nsAString& aUniqueToken) {
  // Not checking if we got the mutex for the same reason we didn't in
  // register-task
  RegistryMutex regMutex;
  regMutex.Acquire();

  WriteInstallationRegistryEntry();

  HRESULT hr = default_agent::UpdateTask(PromiseFlatString(aUniqueToken).get());
  return SUCCEEDED(hr) ? NS_OK : NS_ERROR_FAILURE;
}

NS_IMETHODIMP
DefaultAgent::UnregisterTask(const nsAString& aUniqueToken) {
  HRESULT hr = RemoveTasks(PromiseFlatString(aUniqueToken).get(),
                           WhichTasks::WdbaTaskOnly);
  return SUCCEEDED(hr) ? NS_OK : NS_ERROR_FAILURE;
}

NS_IMETHODIMP
DefaultAgent::Uninstall(const nsAString& aUniqueToken) {
  // We aren't actually going to check whether we got the mutex here.
  // Ideally we would acquire it since we are about to access the registry,
  // so we would like to block simultaneous users of our registry key.
  // But there are two reasons that it is preferable to ignore a mutex
  // wait timeout here:
  //   1. If we fail to uninstall our prefixed registry entries, the
  //      registry key containing them will never be removed, even when the
  //      last installation is uninstalled.
  //   2. If we timed out waiting on the mutex, it implies that there are
  //      other installations. If there are other installations, there will
  //      be other prefixed registry entries. If there are other prefixed
  //      registry entries, we won't remove the whole key or touch the
  //      unprefixed entries during uninstallation. Therefore, we should
  //      be able to safely uninstall without stepping on anyone's toes.
  RegistryMutex regMutex;
  regMutex.Acquire();

  RemoveAllRegistryEntries();
  return NS_OK;
}

NS_IMETHODIMP
DefaultAgent::DoTask(const nsAString& aUniqueToken, const bool aForce) {
  // Acquire() has a short timeout. Since this runs in the background, we
  // could use a longer timeout in this situation. However, if another
  // installation's agent is already running, it will update CurrentDefault,
  // possibly send a ping, and possibly show a notification.
  // Once all that has happened, there is no real reason to do it again. We
  // only send one ping per day, so we aren't going to do that again. And
  // the only time we ever show a second notification is 7 days after the
  // first one, so we aren't going to do that again either.
  // If the other process didn't take those actions, there is no reason that
  // this process would take them.
  // If the other process fails, this one will most likely fail for the same
  // reason.
  // So we'll just bail if we can't get the mutex quickly.
  RegistryMutex regMutex;
  if (!regMutex.Acquire()) {
    return NS_ERROR_NOT_AVAILABLE;
  }

  // Check that Firefox ran recently, if not then stop here.
  // Also stop if no timestamp was found, which most likely indicates
  // that Firefox was not yet run.
  bool ranRecently = false;
  if (!aForce && (!CheckIfAppRanRecently(&ranRecently) || !ranRecently)) {
    return NS_ERROR_FAILURE;
  }

  DefaultBrowserResult defaultBrowserResult = GetDefaultBrowserInfo();
  DefaultBrowserInfo browserInfo{};
  if (defaultBrowserResult.isOk()) {
    browserInfo = defaultBrowserResult.unwrap();
  } else {
    browserInfo.currentDefaultBrowser = Browser::Error;
    browserInfo.previousDefaultBrowser = Browser::Error;
  }

  DefaultPdfResult defaultPdfResult = GetDefaultPdfInfo();
  DefaultPdfInfo pdfInfo{};
  if (defaultPdfResult.isOk()) {
    pdfInfo = defaultPdfResult.unwrap();
  } else {
    pdfInfo.currentDefaultPdf = PDFHandler::Error;
  }

  NotificationActivities activitiesPerformed;
  // We block while waiting for the notification which prevents STA thread
  // callbacks from running as the event loop won't run. Moving notification
  // handling to an MTA thread prevents this conflict.
  activitiesPerformed = MaybeShowNotification(
      browserInfo, PromiseFlatString(aUniqueToken).get(), aForce);

  HRESULT hr = SendDefaultAgentPing(browserInfo, pdfInfo, activitiesPerformed);
  return SUCCEEDED(hr) ? NS_OK : NS_ERROR_FAILURE;
}

NS_IMETHODIMP
DefaultAgent::AppRanRecently(bool* aRanRecently) {
  bool ranRecently = false;
  *aRanRecently = CheckIfAppRanRecently(&ranRecently) && ranRecently;
  return NS_OK;
}

NS_IMETHODIMP
DefaultAgent::GetDefaultBrowser(nsAString& aDefaultBrowser) {
  Browser browser = default_agent::GetDefaultBrowser();
  aDefaultBrowser = NS_ConvertUTF8toUTF16(GetStringForBrowser(browser));
  return NS_OK;
}

NS_IMETHODIMP
DefaultAgent::GetReplacePreviousDefaultBrowser(
    const nsAString& aDefaultBrowser, nsAString& aPreviousDefaultBrowser) {
  Browser browser =
      GetBrowserFromString(std::string(NS_ConvertUTF16toUTF8(aDefaultBrowser)));
  Browser previousBrowser =
      default_agent::GetReplacePreviousDefaultBrowser(browser);
  aPreviousDefaultBrowser =
      NS_ConvertUTF8toUTF16(GetStringForBrowser(previousBrowser));
  return NS_OK;
}

NS_IMETHODIMP
DefaultAgent::GetDefaultPdfHandler(nsAString& aDefaultPdfHandler) {
  PDFHandler pdf = default_agent::GetDefaultPdfInfo()
                       .unwrapOr({PDFHandler::Error})
                       .currentDefaultPdf;
  aDefaultPdfHandler = NS_ConvertUTF8toUTF16(GetStringForPDFHandler(pdf));
  return NS_OK;
}

NS_IMETHODIMP
DefaultAgent::SendPing(const nsAString& aDefaultBrowser,
                       const nsAString& aPreviousDefaultBrowser,
                       const nsAString& aDefaultPdfHandler,
                       const nsAString& aNotificationShown,
                       const nsAString& aNotificationAction) {
  DefaultBrowserInfo browserInfo = {
      GetBrowserFromString(std::string(NS_ConvertUTF16toUTF8(aDefaultBrowser))),
      GetBrowserFromString(
          std::string(NS_ConvertUTF16toUTF8(aPreviousDefaultBrowser)))};

  DefaultPdfInfo pdfInfo = {GetPDFHandlerFromString(
      std::string(NS_ConvertUTF16toUTF8(aDefaultPdfHandler)))};

  // The JS implementation has never supported the "two notification flow",
  // i.e., displaying a followup notification.
  NotificationShown shown = GetNotificationShownFromString(aNotificationShown);
  NotificationAction action =
      GetNotificationActionFromString(aNotificationAction);
  NotificationActivities activitiesPerformed = {NotificationType::Initial,
                                                shown, action};

  HRESULT hr = SendDefaultAgentPing(browserInfo, pdfInfo, activitiesPerformed);
  return SUCCEEDED(hr) ? NS_OK : NS_ERROR_FAILURE;
}

NS_IMETHODIMP
DefaultAgent::SetDefaultBrowserUserChoice(
    const nsAString& aAumid, const nsTArray<nsString>& aExtraFileExtensions) {
  return default_agent::SetDefaultBrowserUserChoice(
      PromiseFlatString(aAumid).get(), aExtraFileExtensions);
}

NS_IMETHODIMP
DefaultAgent::SetDefaultBrowserUserChoiceAsync(
    const nsAString& aAumid, const nsTArray<nsString>& aExtraFileExtensions,
    JSContext* aCx, dom::Promise** aPromise) {
  if (!NS_IsMainThread()) {
    return NS_ERROR_NOT_SAME_THREAD;
  }

  ErrorResult rv;
  RefPtr<dom::Promise> promise =
      dom::Promise::Create(xpc::CurrentNativeGlobal(aCx), rv);
  if (MOZ_UNLIKELY(rv.Failed())) {
    return rv.StealNSResult();
  }

  // A holder to pass the promise through the background task and back to
  // the main thread when finished.
  auto promiseHolder = MakeRefPtr<nsMainThreadPtrHolder<dom::Promise>>(
      "SetDefaultBrowserUserChoiceAsync promise", promise);

  nsresult result = NS_DispatchBackgroundTask(
      NS_NewRunnableFunction(
          "SetDefaultBrowserUserChoiceAsync",
          // Make a local copy of the aAudmid parameter which is a reference
          // which will go out of scope
          [aumid = nsString(aAumid), promiseHolder = std::move(promiseHolder),
           aExtraFileExtensions =
               CopyableTArray<nsString>(aExtraFileExtensions)] {
            nsresult rv = default_agent::SetDefaultBrowserUserChoice(
                PromiseFlatString(aumid).get(), aExtraFileExtensions);

            NS_DispatchToMainThread(NS_NewRunnableFunction(
                "SetDefaultBrowserUserChoiceAsync callback",
                [rv, promiseHolder = std::move(promiseHolder)] {
                  dom::Promise* promise = promiseHolder.get()->get();
                  if (NS_SUCCEEDED(rv)) {
                    promise->MaybeResolveWithUndefined();
                  } else {
                    promise->MaybeReject(rv);
                  }
                }));
          }),
      NS_DISPATCH_EVENT_MAY_BLOCK);

  promise.forget(aPromise);
  return result;
}

NS_IMETHODIMP
DefaultAgent::SetDefaultExtensionHandlersUserChoice(
    const nsAString& aAumid, const nsTArray<nsString>& aFileExtensions) {
  return default_agent::SetDefaultExtensionHandlersUserChoice(
      PromiseFlatString(aAumid).get(), aFileExtensions);
}

NS_IMETHODIMP
DefaultAgent::AgentDisabled(bool* aDisabled) {
  *aDisabled = IsAgentDisabled();
  return NS_OK;
}

}  // namespace mozilla::default_agent
