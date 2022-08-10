/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_AlertNotificationIPCSerializer_h__
#define mozilla_AlertNotificationIPCSerializer_h__

#include "nsComponentManagerUtils.h"
#include "nsCOMPtr.h"
#include "nsIAlertsService.h"
#include "nsIPrincipal.h"
#include "nsString.h"

#include "ipc/IPCMessageUtils.h"

#include "mozilla/dom/PermissionMessageUtils.h"

namespace mozilla {
namespace ipc {

template <>
struct IPDLParamTraits<nsIAlertNotification*> {
  static void Write(IPC::MessageWriter* aWriter, IProtocol* aActor,
                    nsIAlertNotification* aParam) {
    bool isNull = !aParam;
    if (isNull) {
      WriteIPDLParam(aWriter, aActor, isNull);
      return;
    }

    nsString name, imageURL, title, text, cookie, dir, lang, data;
    bool textClickable, inPrivateBrowsing, requireInteraction, silent;
    nsCOMPtr<nsIPrincipal> principal;
    nsTArray<uint32_t> vibrate;

    if (NS_WARN_IF(NS_FAILED(aParam->GetName(name))) ||
        NS_WARN_IF(NS_FAILED(aParam->GetImageURL(imageURL))) ||
        NS_WARN_IF(NS_FAILED(aParam->GetTitle(title))) ||
        NS_WARN_IF(NS_FAILED(aParam->GetText(text))) ||
        NS_WARN_IF(NS_FAILED(aParam->GetTextClickable(&textClickable))) ||
        NS_WARN_IF(NS_FAILED(aParam->GetCookie(cookie))) ||
        NS_WARN_IF(NS_FAILED(aParam->GetDir(dir))) ||
        NS_WARN_IF(NS_FAILED(aParam->GetLang(lang))) ||
        NS_WARN_IF(NS_FAILED(aParam->GetData(data))) ||
        NS_WARN_IF(
            NS_FAILED(aParam->GetPrincipal(getter_AddRefs(principal)))) ||
        NS_WARN_IF(
            NS_FAILED(aParam->GetInPrivateBrowsing(&inPrivateBrowsing))) ||
        NS_WARN_IF(
            NS_FAILED(aParam->GetRequireInteraction(&requireInteraction))) ||
        NS_WARN_IF(NS_FAILED(aParam->GetSilent(&silent))) ||
        NS_WARN_IF(NS_FAILED(aParam->GetVibrate(vibrate)))) {
      // Write a `null` object if any getter returns an error. Otherwise, the
      // receiver will try to deserialize an incomplete object and crash.
      WriteIPDLParam(aWriter, aActor, /* isNull */ true);
      return;
    }

    WriteIPDLParam(aWriter, aActor, isNull);
    WriteIPDLParam(aWriter, aActor, name);
    WriteIPDLParam(aWriter, aActor, imageURL);
    WriteIPDLParam(aWriter, aActor, title);
    WriteIPDLParam(aWriter, aActor, text);
    WriteIPDLParam(aWriter, aActor, textClickable);
    WriteIPDLParam(aWriter, aActor, cookie);
    WriteIPDLParam(aWriter, aActor, dir);
    WriteIPDLParam(aWriter, aActor, lang);
    WriteIPDLParam(aWriter, aActor, data);
    WriteIPDLParam(aWriter, aActor, principal);
    WriteIPDLParam(aWriter, aActor, inPrivateBrowsing);
    WriteIPDLParam(aWriter, aActor, requireInteraction);
    WriteIPDLParam(aWriter, aActor, silent);
    WriteIPDLParam(aWriter, aActor, vibrate);
  }

  static bool Read(IPC::MessageReader* aReader, IProtocol* aActor,
                   RefPtr<nsIAlertNotification>* aResult) {
    bool isNull;
    NS_ENSURE_TRUE(ReadIPDLParam(aReader, aActor, &isNull), false);
    if (isNull) {
      *aResult = nullptr;
      return true;
    }

    nsString name, imageURL, title, text, cookie, dir, lang, data;
    bool textClickable, inPrivateBrowsing, requireInteraction, silent;
    nsCOMPtr<nsIPrincipal> principal;
    nsTArray<uint32_t> vibrate;

    if (!ReadIPDLParam(aReader, aActor, &name) ||
        !ReadIPDLParam(aReader, aActor, &imageURL) ||
        !ReadIPDLParam(aReader, aActor, &title) ||
        !ReadIPDLParam(aReader, aActor, &text) ||
        !ReadIPDLParam(aReader, aActor, &textClickable) ||
        !ReadIPDLParam(aReader, aActor, &cookie) ||
        !ReadIPDLParam(aReader, aActor, &dir) ||
        !ReadIPDLParam(aReader, aActor, &lang) ||
        !ReadIPDLParam(aReader, aActor, &data) ||
        !ReadIPDLParam(aReader, aActor, &principal) ||
        !ReadIPDLParam(aReader, aActor, &inPrivateBrowsing) ||
        !ReadIPDLParam(aReader, aActor, &requireInteraction) ||
        !ReadIPDLParam(aReader, aActor, &silent) ||
        !ReadIPDLParam(aReader, aActor, &vibrate)) {
      return false;
    }

    nsCOMPtr<nsIAlertNotification> alert =
        do_CreateInstance(ALERT_NOTIFICATION_CONTRACTID);
    if (NS_WARN_IF(!alert)) {
      *aResult = nullptr;
      return true;
    }
    nsresult rv = alert->Init(
        name, imageURL, title, text, textClickable, cookie, dir, lang, data,
        principal, inPrivateBrowsing, requireInteraction, silent, vibrate);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      *aResult = nullptr;
      return true;
    }
    *aResult = ToRefPtr(std::move(alert));
    return true;
  }
};

}  // namespace ipc
}  // namespace mozilla

#endif /* mozilla_AlertNotificationIPCSerializer_h__ */
