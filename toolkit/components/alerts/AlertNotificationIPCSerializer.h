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

typedef nsIAlertNotification* AlertNotificationType;

namespace IPC {

template <>
struct ParamTraits<AlertNotificationType>
{
  typedef AlertNotificationType paramType;

  static void Write(Message* aMsg, const paramType& aParam)
  {
    bool isNull = !aParam;
    if (isNull) {
      WriteParam(aMsg, isNull);
      return;
    }

    nsString name, imageURL, title, text, cookie, dir, lang, data;
    bool textClickable, inPrivateBrowsing;
    nsCOMPtr<nsIPrincipal> principal;

    if (NS_WARN_IF(NS_FAILED(aParam->GetName(name))) ||
        NS_WARN_IF(NS_FAILED(aParam->GetImageURL(imageURL))) ||
        NS_WARN_IF(NS_FAILED(aParam->GetTitle(title))) ||
        NS_WARN_IF(NS_FAILED(aParam->GetText(text))) ||
        NS_WARN_IF(NS_FAILED(aParam->GetTextClickable(&textClickable))) ||
        NS_WARN_IF(NS_FAILED(aParam->GetCookie(cookie))) ||
        NS_WARN_IF(NS_FAILED(aParam->GetDir(dir))) ||
        NS_WARN_IF(NS_FAILED(aParam->GetLang(lang))) ||
        NS_WARN_IF(NS_FAILED(aParam->GetData(data))) ||
        NS_WARN_IF(NS_FAILED(aParam->GetPrincipal(getter_AddRefs(principal)))) ||
        NS_WARN_IF(NS_FAILED(aParam->GetInPrivateBrowsing(&inPrivateBrowsing)))) {

      // Write a `null` object if any getter returns an error. Otherwise, the
      // receiver will try to deserialize an incomplete object and crash.
      WriteParam(aMsg, /* isNull */ true);
      return;
    }

    WriteParam(aMsg, isNull);
    WriteParam(aMsg, name);
    WriteParam(aMsg, imageURL);
    WriteParam(aMsg, title);
    WriteParam(aMsg, text);
    WriteParam(aMsg, textClickable);
    WriteParam(aMsg, cookie);
    WriteParam(aMsg, dir);
    WriteParam(aMsg, lang);
    WriteParam(aMsg, data);
    WriteParam(aMsg, IPC::Principal(principal));
    WriteParam(aMsg, inPrivateBrowsing);
  }

  static bool Read(const Message* aMsg, void** aIter, paramType* aResult)
  {
    bool isNull;
    NS_ENSURE_TRUE(ReadParam(aMsg, aIter, &isNull), false);
    if (isNull) {
      *aResult = nullptr;
      return true;
    }

    nsString name, imageURL, title, text, cookie, dir, lang, data;
    bool textClickable, inPrivateBrowsing;
    IPC::Principal principal;

    if (!ReadParam(aMsg, aIter, &name) ||
        !ReadParam(aMsg, aIter, &imageURL) ||
        !ReadParam(aMsg, aIter, &title) ||
        !ReadParam(aMsg, aIter, &text) ||
        !ReadParam(aMsg, aIter, &textClickable) ||
        !ReadParam(aMsg, aIter, &cookie) ||
        !ReadParam(aMsg, aIter, &dir) ||
        !ReadParam(aMsg, aIter, &lang) ||
        !ReadParam(aMsg, aIter, &data) ||
        !ReadParam(aMsg, aIter, &principal) ||
        !ReadParam(aMsg, aIter, &inPrivateBrowsing)) {

      return false;
    }

    nsCOMPtr<nsIAlertNotification> alert =
      do_CreateInstance(ALERT_NOTIFICATION_CONTRACTID);
    if (NS_WARN_IF(!alert)) {
      *aResult = nullptr;
      return true;
    }
    nsresult rv = alert->Init(name, imageURL, title, text, textClickable,
                              cookie, dir, lang, data, principal,
                              inPrivateBrowsing);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      *aResult = nullptr;
      return true;
    }
    alert.forget(aResult);
    return true;
  }
};

} // namespace IPC

#endif /* mozilla_AlertNotificationIPCSerializer_h__ */
