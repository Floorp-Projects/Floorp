/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#pragma once

#include <windows.ui.notifications.h>
#include <windows.data.xml.dom.h>
#include "mozwrlbase.h"
#include "nsString.h"


class ToastNotificationHandler {
    typedef ABI::Windows::UI::Notifications::IToastNotification IToastNotification;
    typedef ABI::Windows::UI::Notifications::IToastDismissedEventArgs IToastDismissedEventArgs;
    typedef ABI::Windows::UI::Notifications::IToastNotificationManagerStatics IToastNotificationManagerStatics;
    typedef ABI::Windows::UI::Notifications::ToastTemplateType ToastTemplateType;
    typedef ABI::Windows::Data::Xml::Dom::IXmlNode IXmlNode;
    typedef ABI::Windows::Data::Xml::Dom::IXmlDocument IXmlDocument;

    void SetNodeValueString(HSTRING inputString,Microsoft::WRL::ComPtr<IXmlNode> node,
                                            Microsoft::WRL::ComPtr<IXmlDocument> xml);
  public:
    ToastNotificationHandler() {};
    ~ToastNotificationHandler() {};

    bool DisplayNotification(HSTRING title, HSTRING msg, HSTRING imagePath,
                             const nsAString& aCookie, const nsAString& aAppId);
    bool DisplayTextNotification(HSTRING title, HSTRING msg,
                                 const nsAString& aCookie,
                                 const nsAString& aAppId);
    HRESULT OnActivate(IToastNotification *notification, IInspectable *inspectable);
    HRESULT OnDismiss(IToastNotification *notification,
                      IToastDismissedEventArgs* aArgs);

  private:
    nsString mCookie;
    Microsoft::WRL::ComPtr<IToastNotificationManagerStatics> mToastNotificationManagerStatics;

    bool CreateWindowsNotificationFromXml(IXmlDocument *toastXml,
                                          const nsAString& aAppId);
    Microsoft::WRL::ComPtr<IXmlDocument> InitializeXmlForTemplate(ToastTemplateType templateType);
};
