/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "ToastNotificationHandler.h"
#include "MetroUtils.h"
#include "mozilla/Services.h"
#include "FrameworkView.h"

using namespace ABI::Windows::Foundation;
using namespace ABI::Windows::Data::Xml::Dom;
using namespace Microsoft::WRL;
using namespace Microsoft::WRL::Wrappers;
using namespace mozilla;
using namespace ABI::Windows::UI::Notifications;

typedef __FITypedEventHandler_2_Windows__CUI__CNotifications__CToastNotification_IInspectable_t ToastActivationHandler;
typedef __FITypedEventHandler_2_Windows__CUI__CNotifications__CToastNotification_Windows__CUI__CNotifications__CToastDismissedEventArgs ToastDismissHandler;

void
ToastNotificationHandler::DisplayNotification(HSTRING title,
                                              HSTRING msg,
                                              HSTRING imagePath,
                                              const nsAString& aCookie)
{
  mCookie = aCookie;

  ComPtr<IToastNotificationManagerStatics> toastNotificationManagerStatics;
  AssertHRESULT(GetActivationFactory(HStringReference(RuntimeClass_Windows_UI_Notifications_ToastNotificationManager).Get(),
                                    toastNotificationManagerStatics.GetAddressOf()));

  ComPtr<IXmlDocument> toastXml;
  toastNotificationManagerStatics->GetTemplateContent(ToastTemplateType::ToastTemplateType_ToastImageAndText03, &toastXml);

  ComPtr<IXmlNodeList> toastTextElements, toastImageElements;
  ComPtr<IXmlNode> titleTextNodeRoot, msgTextNodeRoot, imageNodeRoot, srcAttribute;

  HSTRING textNodeStr, imageNodeStr, srcNodeStr;
  HSTRING_HEADER textHeader, imageHeader, srcHeader;
  WindowsCreateStringReference(L"text", 4, &textHeader, &textNodeStr);
  WindowsCreateStringReference(L"image", 5, &imageHeader, &imageNodeStr);
  WindowsCreateStringReference(L"src", 3, &srcHeader, &srcNodeStr);
  toastXml->GetElementsByTagName(textNodeStr, &toastTextElements);
  toastXml->GetElementsByTagName(imageNodeStr, &toastImageElements);

  AssertHRESULT(toastTextElements->Item(0, &titleTextNodeRoot));
  AssertHRESULT(toastTextElements->Item(1, &msgTextNodeRoot));
  AssertHRESULT(toastImageElements->Item(0, &imageNodeRoot));

  ComPtr<IXmlNamedNodeMap> attributes;
  AssertHRESULT(imageNodeRoot->get_Attributes(&attributes));
  AssertHRESULT(attributes->GetNamedItem(srcNodeStr, &srcAttribute));

  SetNodeValueString(title, titleTextNodeRoot.Get(), toastXml.Get());
  SetNodeValueString(msg, msgTextNodeRoot.Get(), toastXml.Get());
  SetNodeValueString(imagePath, srcAttribute.Get(), toastXml.Get());

  ComPtr<IToastNotification> notification;
  ComPtr<IToastNotificationFactory> factory;
  AssertHRESULT(GetActivationFactory(HStringReference(RuntimeClass_Windows_UI_Notifications_ToastNotification).Get(),
                factory.GetAddressOf()));
  AssertHRESULT(factory->CreateToastNotification(toastXml.Get(), &notification));

  EventRegistrationToken activatedToken;
  AssertHRESULT(notification->add_Activated(Callback<ToastActivationHandler>(this,
      &ToastNotificationHandler::OnActivate).Get(), &activatedToken));
  EventRegistrationToken dismissedToken;
  AssertHRESULT(notification->add_Dismissed(Callback<ToastDismissHandler>(this,
      &ToastNotificationHandler::OnDismiss).Get(), &dismissedToken));

  ComPtr<IToastNotifier> notifier;
  toastNotificationManagerStatics->CreateToastNotifier(&notifier);
  notifier->Show(notification.Get());

  MetroUtils::FireObserver("metro_native_toast_shown", mCookie.get());
}

void ToastNotificationHandler::SetNodeValueString(HSTRING inputString, ComPtr<IXmlNode> node, ComPtr<IXmlDocument> xml) { 
  ComPtr<IXmlText> inputText;
  ComPtr<IXmlNode> inputTextNode, pAppendedChild;

  AssertHRESULT(xml->CreateTextNode(inputString, &inputText));
  AssertHRESULT(inputText.As(&inputTextNode));
  AssertHRESULT(node->AppendChild(inputTextNode.Get(), &pAppendedChild));
}

HRESULT ToastNotificationHandler::OnActivate(IToastNotification *notification, IInspectable *inspectable) {
  MetroUtils::FireObserver("metro_native_toast_clicked", mCookie.get());
  return S_OK;
}

HRESULT
ToastNotificationHandler::OnDismiss(IToastNotification *notification,
                                    IToastDismissedEventArgs* aArgs)
{
  MetroUtils::FireObserver("metro_native_toast_dismissed", mCookie.get());
  return S_OK;
}
