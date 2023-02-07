/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sts=2 sw=2 et cin: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "ToastNotification.h"

#include <windows.h>
#include <appmodel.h>
#include <ktmw32.h>

#include "mozilla/Buffer.h"
#include "mozilla/DynamicallyLinkedFunctionPtr.h"
#include "mozilla/mscom/COMWrappers.h"
#include "mozilla/Logging.h"
#include "mozilla/Services.h"
#include "mozilla/WidgetUtils.h"
#include "mozilla/WindowsVersion.h"
#include "nsAppRunner.h"
#include "nsComponentManagerUtils.h"
#include "nsCOMPtr.h"
#include "nsIObserverService.h"
#include "nsString.h"
#include "nsThreadUtils.h"
#include "nsWindowsHelpers.h"
#include "nsXREDirProvider.h"
#include "prenv.h"
#include "ToastNotificationHandler.h"
#include "WinTaskbar.h"
#include "WinUtils.h"

namespace mozilla {
namespace widget {

static LazyLogModule sWASLog("WindowsAlertsService");

NS_IMPL_ISUPPORTS(ToastNotification, nsIAlertsService, nsIWindowsAlertsService,
                  nsIAlertsDoNotDisturb, nsIObserver)

ToastNotification::ToastNotification() = default;

ToastNotification::~ToastNotification() = default;

nsresult ToastNotification::Init() {
  if (!IsWin8OrLater()) {
    return NS_ERROR_NOT_IMPLEMENTED;
  }

  if (!PR_GetEnv("XPCSHELL_TEST_PROFILE_DIR")) {
    // Windows Toast Notification requires AppId.  But allow `xpcshell` to
    // create the service to test other functionality.
    if (!EnsureAumidRegistered()) {
      MOZ_LOG(sWASLog, LogLevel::Warning, ("Failed to register AUMID!"));
      return NS_ERROR_NOT_IMPLEMENTED;
    }
  } else {
    MOZ_LOG(sWASLog, LogLevel::Info, ("Using dummy AUMID in xpcshell test"));
    mAumid.emplace(u"XpcshellTestToastAumid"_ns);
  }

  MOZ_LOG(sWASLog, LogLevel::Info,
          ("Using AUMID: '%s'", NS_ConvertUTF16toUTF8(mAumid.ref()).get()));

  nsCOMPtr<nsIObserverService> obsServ =
      mozilla::services::GetObserverService();
  if (obsServ) {
    obsServ->AddObserver(this, "quit-application", true);
  }

  return NS_OK;
}

bool ToastNotification::EnsureAumidRegistered() {
  // Check if this is an MSIX install, app identity is provided by the package
  // so no registration is necessary.
  if (AssignIfMsixAumid(mAumid)) {
    MOZ_LOG(
        sWASLog, LogLevel::Info,
        ("Found MSIX AUMID: '%s'", NS_ConvertUTF16toUTF8(mAumid.ref()).get()));
    return true;
  }

  nsAutoString installHash;
  nsresult rv = gDirServiceProvider->GetInstallHash(installHash);
  NS_ENSURE_SUCCESS(rv, false);

  // Check if toasts were registered during NSIS/MSI installation.
  if (AssignIfNsisAumid(installHash, mAumid)) {
    MOZ_LOG(sWASLog, LogLevel::Info,
            ("Found AUMID from installer (with install hash '%s'): '%s'",
             NS_ConvertUTF16toUTF8(installHash).get(),
             NS_ConvertUTF16toUTF8(mAumid.ref()).get()));
    return true;
  }

  // No AUMID registered, fall through to runtime registration for development
  // and portable builds.
  if (RegisterRuntimeAumid(installHash, mAumid)) {
    MOZ_LOG(
        sWASLog, LogLevel::Info,
        ("Updated AUMID registration at runtime (for install hash '%s'): '%s'",
         NS_ConvertUTF16toUTF8(installHash).get(),
         NS_ConvertUTF16toUTF8(mAumid.ref()).get()));
    return true;
  }

  MOZ_LOG(sWASLog, LogLevel::Warning,
          ("Failed to register AUMID at runtime! (for install hash '%s')",
           NS_ConvertUTF16toUTF8(installHash).get()));
  return false;
}

bool ToastNotification::AssignIfMsixAumid(Maybe<nsAutoString>& aAumid) {
  // `GetCurrentApplicationUserModelId` added in Windows 8.
  DynamicallyLinkedFunctionPtr<decltype(&GetCurrentApplicationUserModelId)>
      pGetCurrentApplicationUserModelId(L"kernel32.dll",
                                        "GetCurrentApplicationUserModelId");
  if (!pGetCurrentApplicationUserModelId) {
    return false;
  }

  UINT32 len = 0;
  // ERROR_INSUFFICIENT_BUFFER signals that we're in an MSIX package, and
  // therefore should use the package's AUMID.
  if (pGetCurrentApplicationUserModelId(&len, nullptr) !=
      ERROR_INSUFFICIENT_BUFFER) {
    MOZ_LOG(sWASLog, LogLevel::Debug, ("Not an MSIX package"));
    return false;
  }
  mozilla::Buffer<wchar_t> buffer(len);
  LONG success = pGetCurrentApplicationUserModelId(&len, buffer.Elements());
  NS_ENSURE_TRUE(success == ERROR_SUCCESS, false);

  aAumid.emplace(buffer.Elements());
  return true;
}

bool ToastNotification::AssignIfNsisAumid(nsAutoString& aInstallHash,
                                          Maybe<nsAutoString>& aAumid) {
  nsAutoString nsisAumidName =
      u""_ns MOZ_TOAST_APP_NAME u"Toast-"_ns + aInstallHash;
  nsAutoString nsisAumidPath = u"AppUserModelId\\"_ns + nsisAumidName;
  if (!WinUtils::HasRegistryKey(HKEY_CLASSES_ROOT, nsisAumidPath.get())) {
    MOZ_LOG(sWASLog, LogLevel::Debug,
            ("No CustomActivator value from installer in key 'HKCR\\%s'",
             NS_ConvertUTF16toUTF8(nsisAumidPath).get()));
    return false;
  }

  aAumid.emplace(nsisAumidName);
  return true;
}

bool ToastNotification::RegisterRuntimeAumid(nsAutoString& aInstallHash,
                                             Maybe<nsAutoString>& aAumid) {
  // Portable AUMID slightly differs from installed AUMID so we can
  // differentiate installed to HKCU vs portable installs if necessary.
  nsAutoString portableAumid =
      u""_ns MOZ_TOAST_APP_NAME u"PortableToast-"_ns + aInstallHash;

  nsCOMPtr<nsIFile> appdir;
  nsresult rv = gDirServiceProvider->GetGREDir()->Clone(getter_AddRefs(appdir));
  NS_ENSURE_SUCCESS(rv, false);

  nsCOMPtr<nsIFile> icon;
  rv = appdir->Clone(getter_AddRefs(icon));
  NS_ENSURE_SUCCESS(rv, false);

  rv = icon->Append(u"browser"_ns);
  NS_ENSURE_SUCCESS(rv, false);

  rv = icon->Append(u"VisualElements"_ns);
  NS_ENSURE_SUCCESS(rv, false);

  rv = icon->Append(u"VisualElements_70.png"_ns);
  NS_ENSURE_SUCCESS(rv, false);

  nsAutoString iconPath;
  rv = icon->GetPath(iconPath);
  NS_ENSURE_SUCCESS(rv, false);

  nsCOMPtr<nsIFile> comDll;
  rv = appdir->Clone(getter_AddRefs(comDll));
  NS_ENSURE_SUCCESS(rv, false);

  rv = comDll->Append(u"notificationserver.dll"_ns);
  NS_ENSURE_SUCCESS(rv, false);

  nsAutoString dllPath;
  rv = comDll->GetPath(dllPath);
  NS_ENSURE_SUCCESS(rv, false);

  nsAutoHandle txn;
  // Manipulate the registry using a transaction so that any failures are
  // rolled back.
  wchar_t transactionName[] = L"" MOZ_TOAST_APP_NAME L" toast registration";
  txn.own(::CreateTransaction(nullptr, nullptr, TRANSACTION_DO_NOT_PROMOTE, 0,
                              0, 0, transactionName));
  NS_ENSURE_TRUE(txn.get() != INVALID_HANDLE_VALUE, false);

  LSTATUS status;

  auto RegisterKey = [&](const nsAString& path, nsAutoRegKey& key) {
    HKEY rawKey;
    status = ::RegCreateKeyTransactedW(
        HKEY_CURRENT_USER, PromiseFlatString(path).get(), 0, nullptr,
        REG_OPTION_NON_VOLATILE, KEY_ALL_ACCESS, nullptr, &rawKey, nullptr, txn,
        nullptr);
    NS_ENSURE_TRUE(status == ERROR_SUCCESS, false);

    key.own(rawKey);
    return true;
  };
  auto RegisterValue = [&](nsAutoRegKey& key, const nsAString& name,
                           unsigned long type, const nsAString& data) {
    status = ::RegSetValueExW(
        key, PromiseFlatString(name).get(), 0, type,
        static_cast<const BYTE*>(PromiseFlatString(data).get()),
        (data.Length() + 1) * sizeof(wchar_t));

    return status == ERROR_SUCCESS;
  };

  // clang-format off
  /* Writes the following keys and values to the registry.
   * HKEY_CURRENT_USER\Software\Classes\AppID\{GUID}                                                      DllSurrogate    : REG_SZ        = ""
   *                                   \AppUserModelId\{MOZ_TOAST_APP_NAME}PortableToast-{install hash}   CustomActivator : REG_SZ        = {GUID}
   *                                                                                                      DisplayName     : REG_EXPAND_SZ = {display name}
   *                                                                                                      IconUri         : REG_EXPAND_SZ = {icon path}
   *                                   \CLSID\{GUID}                                                      AppID           : REG_SZ        = {GUID}
   *                                                \InprocServer32                                       (Default)       : REG_SZ        = {notificationserver.dll path}
   */
  // clang-format on

  constexpr nsLiteralString classes = u"Software\\Classes\\"_ns;

  nsAutoString aumid = classes + u"AppUserModelId\\"_ns + portableAumid;
  nsAutoRegKey aumidKey;
  NS_ENSURE_TRUE(RegisterKey(aumid, aumidKey), false);

  nsAutoString guidStr;
  {
    DWORD bufferSizeBytes = NSID_LENGTH * sizeof(wchar_t);
    Buffer<wchar_t> guidBuffer(bufferSizeBytes);
    status = ::RegGetValueW(HKEY_CURRENT_USER, aumid.get(), L"CustomActivator",
                            RRF_RT_REG_SZ, 0, guidBuffer.Elements(),
                            &bufferSizeBytes);

    CLSID unused;
    if (status == ERROR_SUCCESS &&
        SUCCEEDED(CLSIDFromString(guidBuffer.Elements(), &unused))) {
      guidStr = guidBuffer.Elements();
    } else {
      nsIDToCString uuidString(nsID::GenerateUUID());
      size_t len = strlen(uuidString.get());
      MOZ_ASSERT(len == NSID_LENGTH - 1);
      CopyASCIItoUTF16(nsDependentCSubstring(uuidString.get(), len), guidStr);
    }

    if (status == ERROR_SUCCESS) {
      MOZ_LOG(sWASLog, LogLevel::Debug,
              ("Existing CustomActivator guid found: '%s'",
               NS_ConvertUTF16toUTF8(guidStr).get()));
    } else {
      MOZ_LOG(sWASLog, LogLevel::Debug,
              ("New CustomActivator guid generated: '%s'",
               NS_ConvertUTF16toUTF8(guidStr).get()));
    }
  }
  NS_ENSURE_TRUE(
      RegisterValue(aumidKey, u"CustomActivator"_ns, REG_SZ, guidStr), false);
  nsAutoString brandName;
  WidgetUtils::GetBrandShortName(brandName);
  NS_ENSURE_TRUE(
      RegisterValue(aumidKey, u"DisplayName"_ns, REG_EXPAND_SZ, brandName),
      false);
  NS_ENSURE_TRUE(
      RegisterValue(aumidKey, u"IconUri"_ns, REG_EXPAND_SZ, iconPath), false);

  nsAutoString appid = classes + u"AppID\\"_ns + guidStr;
  nsAutoRegKey appidKey;
  NS_ENSURE_TRUE(RegisterKey(appid, appidKey), false);
  NS_ENSURE_TRUE(RegisterValue(appidKey, u"DllSurrogate"_ns, REG_SZ, u""_ns),
                 false);

  nsAutoString clsid = classes + u"CLSID\\"_ns + guidStr;
  nsAutoRegKey clsidKey;
  NS_ENSURE_TRUE(RegisterKey(clsid, clsidKey), false);
  NS_ENSURE_TRUE(RegisterValue(clsidKey, u"AppID"_ns, REG_SZ, guidStr), false);

  nsAutoString inproc = clsid + u"\\InprocServer32"_ns;
  nsAutoRegKey inprocKey;
  NS_ENSURE_TRUE(RegisterKey(inproc, inprocKey), false);
  // Set the component's path to this DLL
  NS_ENSURE_TRUE(RegisterValue(inprocKey, u""_ns, REG_SZ, dllPath), false);

  NS_ENSURE_TRUE(::CommitTransaction(txn), false);

  MOZ_LOG(
      sWASLog, LogLevel::Debug,
      ("Updated registration for CustomActivator value in key 'HKCU\\%s': '%s'",
       NS_ConvertUTF16toUTF8(aumid).get(),
       NS_ConvertUTF16toUTF8(guidStr).get()));
  aAumid.emplace(portableAumid);
  return true;
}

nsresult ToastNotification::BackgroundDispatch(nsIRunnable* runnable) {
  return NS_DispatchBackgroundTask(runnable);
}

NS_IMETHODIMP
ToastNotification::GetSuppressForScreenSharing(bool* aRetVal) {
  *aRetVal = mSuppressForScreenSharing;
  return NS_OK;
}

NS_IMETHODIMP
ToastNotification::SetSuppressForScreenSharing(bool aSuppress) {
  mSuppressForScreenSharing = aSuppress;
  return NS_OK;
}

NS_IMETHODIMP
ToastNotification::Observe(nsISupports* aSubject, const char* aTopic,
                           const char16_t* aData) {
  // Got quit-application
  // The handlers destructors will do the right thing (de-register with
  // Windows).
  for (auto iter = mActiveHandlers.Iter(); !iter.Done(); iter.Next()) {
    RefPtr<ToastNotificationHandler> handler = iter.UserData();
    iter.Remove();

    // Break the cycle between the handler and the MSCOM notification so the
    // handler's destructor will be called.
    handler->UnregisterHandler();
  }

  return NS_OK;
}

NS_IMETHODIMP
ToastNotification::ShowAlertNotification(
    const nsAString& aImageUrl, const nsAString& aAlertTitle,
    const nsAString& aAlertText, bool aAlertTextClickable,
    const nsAString& aAlertCookie, nsIObserver* aAlertListener,
    const nsAString& aAlertName, const nsAString& aBidi, const nsAString& aLang,
    const nsAString& aData, nsIPrincipal* aPrincipal, bool aInPrivateBrowsing,
    bool aRequireInteraction) {
  nsCOMPtr<nsIAlertNotification> alert =
      do_CreateInstance(ALERT_NOTIFICATION_CONTRACTID);
  if (NS_WARN_IF(!alert)) {
    return NS_ERROR_FAILURE;
  }
  // vibrate is unused for now
  nsTArray<uint32_t> vibrate;
  nsresult rv = alert->Init(aAlertName, aImageUrl, aAlertTitle, aAlertText,
                            aAlertTextClickable, aAlertCookie, aBidi, aLang,
                            aData, aPrincipal, aInPrivateBrowsing,
                            aRequireInteraction, false, vibrate);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }
  return ShowAlert(alert, aAlertListener);
}

NS_IMETHODIMP
ToastNotification::ShowPersistentNotification(const nsAString& aPersistentData,
                                              nsIAlertNotification* aAlert,
                                              nsIObserver* aAlertListener) {
  return ShowAlert(aAlert, aAlertListener);
}

NS_IMETHODIMP
ToastNotification::SetManualDoNotDisturb(bool aDoNotDisturb) {
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
ToastNotification::GetManualDoNotDisturb(bool* aRet) {
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
ToastNotification::ShowAlert(nsIAlertNotification* aAlert,
                             nsIObserver* aAlertListener) {
  NS_ENSURE_ARG(aAlert);

  if (mSuppressForScreenSharing) {
    return NS_OK;
  }

  nsAutoString cookie;
  MOZ_TRY(aAlert->GetCookie(cookie));

  nsAutoString name;
  MOZ_TRY(aAlert->GetName(name));

  nsAutoString title;
  MOZ_TRY(aAlert->GetTitle(title));

  nsAutoString text;
  MOZ_TRY(aAlert->GetText(text));

  bool textClickable;
  MOZ_TRY(aAlert->GetTextClickable(&textClickable));

  nsAutoString hostPort;
  MOZ_TRY(aAlert->GetSource(hostPort));

  bool requireInteraction;
  MOZ_TRY(aAlert->GetRequireInteraction(&requireInteraction));

  nsTArray<RefPtr<nsIAlertAction>> actions;
  MOZ_TRY(aAlert->GetActions(actions));

  RefPtr<ToastNotificationHandler> oldHandler = mActiveHandlers.Get(name);

  NS_ENSURE_TRUE(mAumid.isSome(), NS_ERROR_UNEXPECTED);
  RefPtr<ToastNotificationHandler> handler = new ToastNotificationHandler(
      this, mAumid.ref(), aAlertListener, name, cookie, title, text, hostPort,
      textClickable, requireInteraction, actions);
  mActiveHandlers.InsertOrUpdate(name, RefPtr{handler});

  nsresult rv = handler->InitAlertAsync(aAlert);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    mActiveHandlers.Remove(name);
    handler->UnregisterHandler();
    return rv;
  }

  // If there was a previous handler with the same name then unregister it.
  if (oldHandler) {
    oldHandler->UnregisterHandler();
  }

  return NS_OK;
}

NS_IMETHODIMP
ToastNotification::GetXmlStringForWindowsAlert(nsIAlertNotification* aAlert,
                                               nsAString& aString) {
  NS_ENSURE_ARG(aAlert);

  nsAutoString cookie;
  MOZ_TRY(aAlert->GetCookie(cookie));

  nsAutoString name;
  MOZ_TRY(aAlert->GetName(name));

  nsAutoString title;
  MOZ_TRY(aAlert->GetTitle(title));

  nsAutoString text;
  MOZ_TRY(aAlert->GetText(text));

  bool textClickable;
  MOZ_TRY(aAlert->GetTextClickable(&textClickable));

  nsAutoString hostPort;
  MOZ_TRY(aAlert->GetSource(hostPort));

  bool requireInteraction;
  MOZ_TRY(aAlert->GetRequireInteraction(&requireInteraction));

  nsTArray<RefPtr<nsIAlertAction>> actions;
  MOZ_TRY(aAlert->GetActions(actions));

  NS_ENSURE_TRUE(mAumid.isSome(), NS_ERROR_UNEXPECTED);
  RefPtr<ToastNotificationHandler> handler = new ToastNotificationHandler(
      this, mAumid.ref(), nullptr /* aAlertListener */, name, cookie, title,
      text, hostPort, textClickable, requireInteraction, actions);

  nsAutoString imageURL;
  MOZ_TRY(aAlert->GetImageURL(imageURL));

  return handler->CreateToastXmlString(imageURL, aString);
}

NS_IMETHODIMP
ToastNotification::CloseAlert(const nsAString& aAlertName) {
  RefPtr<ToastNotificationHandler> handler;
  if (NS_WARN_IF(!mActiveHandlers.Get(aAlertName, getter_AddRefs(handler)))) {
    return NS_OK;
  }
  mActiveHandlers.Remove(aAlertName);
  handler->UnregisterHandler();
  return NS_OK;
}

bool ToastNotification::IsActiveHandler(const nsAString& aAlertName,
                                        ToastNotificationHandler* aHandler) {
  RefPtr<ToastNotificationHandler> handler;
  if (NS_WARN_IF(!mActiveHandlers.Get(aAlertName, getter_AddRefs(handler)))) {
    return false;
  }
  return handler == aHandler;
}

void ToastNotification::RemoveHandler(const nsAString& aAlertName,
                                      ToastNotificationHandler* aHandler) {
  // The alert may have been replaced; only remove it from the active
  // handlers map if it's the same.
  if (IsActiveHandler(aAlertName, aHandler)) {
    // Terrible things happen if the destructor of a handler is called inside
    // the hashtable .Remove() method. Wait until we have returned from there.
    RefPtr<ToastNotificationHandler> kungFuDeathGrip(aHandler);
    mActiveHandlers.Remove(aAlertName);
    aHandler->UnregisterHandler();
  }
}

}  // namespace widget
}  // namespace mozilla
