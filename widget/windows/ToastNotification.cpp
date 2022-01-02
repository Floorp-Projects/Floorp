/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sts=2 sw=2 et cin: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "ToastNotification.h"

#include "mozilla/WindowsVersion.h"
#include "nsComponentManagerUtils.h"
#include "nsCOMPtr.h"
#include "nsIObserverService.h"
#include "nsString.h"
#include "nsThreadUtils.h"
#include "ToastNotificationHandler.h"
#include "WinTaskbar.h"

namespace mozilla {
namespace widget {

NS_IMPL_ISUPPORTS(ToastNotification, nsIAlertsService, nsIObserver,
                  nsISupportsWeakReference)

ToastNotification::ToastNotification() = default;

ToastNotification::~ToastNotification() = default;

nsresult ToastNotification::Init() {
  if (!IsWin8OrLater()) {
    return NS_ERROR_NOT_IMPLEMENTED;
  }

  nsAutoString uid;
  if (NS_WARN_IF(!WinTaskbar::GetAppUserModelID(uid))) {
    // Windows Toast Notification requires AppId
    return NS_ERROR_NOT_IMPLEMENTED;
  }

  nsresult rv =
      NS_NewNamedThread("ToastBgThread", getter_AddRefs(mBackgroundThread));
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  nsCOMPtr<nsIObserverService> obsServ =
      do_GetService("@mozilla.org/observer-service;1");
  if (obsServ) {
    obsServ->AddObserver(this, "quit-application", true);
  }

  return NS_OK;
}

nsresult ToastNotification::BackgroundDispatch(nsIRunnable* runnable) {
  return mBackgroundThread->Dispatch(runnable, NS_DISPATCH_NORMAL);
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
ToastNotification::ShowAlert(nsIAlertNotification* aAlert,
                             nsIObserver* aAlertListener) {
  if (NS_WARN_IF(!aAlert)) {
    return NS_ERROR_INVALID_ARG;
  }

  nsAutoString cookie;
  nsresult rv = aAlert->GetCookie(cookie);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  nsAutoString name;
  rv = aAlert->GetName(name);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  nsAutoString title;
  rv = aAlert->GetTitle(title);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  nsAutoString text;
  rv = aAlert->GetText(text);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  bool textClickable;
  rv = aAlert->GetTextClickable(&textClickable);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  nsAutoString hostPort;
  rv = aAlert->GetSource(hostPort);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  RefPtr<ToastNotificationHandler> oldHandler = mActiveHandlers.Get(name);

  RefPtr<ToastNotificationHandler> handler = new ToastNotificationHandler(
      this, aAlertListener, name, cookie, title, text, hostPort, textClickable);
  mActiveHandlers.InsertOrUpdate(name, RefPtr{handler});

  rv = handler->InitAlertAsync(aAlert);
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
