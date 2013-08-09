/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#pragma once

#include <windows.ui.notifications.h>
#include <windows.data.xml.dom.h>
#include "mozwrlbase.h"

using namespace Microsoft::WRL;

class ToastNotificationHandler {
    typedef ABI::Windows::UI::Notifications::IToastNotification IToastNotification;
    typedef ABI::Windows::Data::Xml::Dom::IXmlNode IXmlNode;
    typedef ABI::Windows::Data::Xml::Dom::IXmlDocument IXmlDocument;

    void SetNodeValueString(HSTRING inputString, ComPtr<IXmlNode> node, ComPtr<IXmlDocument> xml);
  public:
    ToastNotificationHandler() {};
    ~ToastNotificationHandler() {};

    void DisplayNotification(HSTRING title, HSTRING msg, HSTRING imagePath);
    HRESULT OnActivate(IToastNotification *notification, IInspectable *inspectable);
};
