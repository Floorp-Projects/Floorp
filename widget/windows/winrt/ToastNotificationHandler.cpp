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

bool
ToastNotificationHandler::DisplayNotification(HSTRING title,
                                              HSTRING msg,
                                              HSTRING imagePath,
                                              const nsAString& aCookie,
                                              const nsAString& aAppId)
{
  mCookie = aCookie;

  Microsoft::WRL::ComPtr<IXmlDocument> toastXml =
    InitializeXmlForTemplate(ToastTemplateType::ToastTemplateType_ToastImageAndText03);
  Microsoft::WRL::ComPtr<IXmlNodeList> toastTextElements, toastImageElements;
  Microsoft::WRL::ComPtr<IXmlNode> titleTextNodeRoot, msgTextNodeRoot, imageNodeRoot, srcAttribute;

  HSTRING textNodeStr, imageNodeStr, srcNodeStr;
  HSTRING_HEADER textHeader, imageHeader, srcHeader;
  WindowsCreateStringReference(L"text", 4, &textHeader, &textNodeStr);
  WindowsCreateStringReference(L"image", 5, &imageHeader, &imageNodeStr);
  WindowsCreateStringReference(L"src", 3, &srcHeader, &srcNodeStr);
  toastXml->GetElementsByTagName(textNodeStr, &toastTextElements);
  toastXml->GetElementsByTagName(imageNodeStr, &toastImageElements);

  AssertRetHRESULT(toastTextElements->Item(0, &titleTextNodeRoot), false);
  AssertRetHRESULT(toastTextElements->Item(1, &msgTextNodeRoot), false);
  AssertRetHRESULT(toastImageElements->Item(0, &imageNodeRoot), false);

  Microsoft::WRL::ComPtr<IXmlNamedNodeMap> attributes;
  AssertRetHRESULT(imageNodeRoot->get_Attributes(&attributes), false);
  AssertRetHRESULT(attributes->GetNamedItem(srcNodeStr, &srcAttribute), false);

  SetNodeValueString(title, titleTextNodeRoot.Get(), toastXml.Get());
  SetNodeValueString(msg, msgTextNodeRoot.Get(), toastXml.Get());
  SetNodeValueString(imagePath, srcAttribute.Get(), toastXml.Get());

  return CreateWindowsNotificationFromXml(toastXml.Get(), aAppId);
}

bool
ToastNotificationHandler::DisplayTextNotification(HSTRING title,
                                                  HSTRING msg,
                                                  const nsAString& aCookie,
                                                  const nsAString& aAppId)
{
  mCookie = aCookie;

  Microsoft::WRL::ComPtr<IXmlDocument> toastXml =
    InitializeXmlForTemplate(ToastTemplateType::ToastTemplateType_ToastText03);
  Microsoft::WRL::ComPtr<IXmlNodeList> toastTextElements;
  Microsoft::WRL::ComPtr<IXmlNode> titleTextNodeRoot, msgTextNodeRoot;

  HSTRING textNodeStr;
  HSTRING_HEADER textHeader;
  WindowsCreateStringReference(L"text", 4, &textHeader, &textNodeStr);
  toastXml->GetElementsByTagName(textNodeStr, &toastTextElements);

  AssertRetHRESULT(toastTextElements->Item(0, &titleTextNodeRoot), false);
  AssertRetHRESULT(toastTextElements->Item(1, &msgTextNodeRoot), false);

  SetNodeValueString(title, titleTextNodeRoot.Get(), toastXml.Get());
  SetNodeValueString(msg, msgTextNodeRoot.Get(), toastXml.Get());

  return CreateWindowsNotificationFromXml(toastXml.Get(), aAppId);
}

Microsoft::WRL::ComPtr<IXmlDocument>
ToastNotificationHandler::InitializeXmlForTemplate(ToastTemplateType templateType) {
  Microsoft::WRL::ComPtr<IXmlDocument> toastXml;

  AssertRetHRESULT(GetActivationFactory(HStringReference(RuntimeClass_Windows_UI_Notifications_ToastNotificationManager).Get(),
    mToastNotificationManagerStatics.GetAddressOf()), nullptr);

  mToastNotificationManagerStatics->GetTemplateContent(templateType, &toastXml);

  return toastXml;
}

bool
ToastNotificationHandler::CreateWindowsNotificationFromXml(IXmlDocument *toastXml,
                                                           const nsAString& aAppId)
{
  Microsoft::WRL::ComPtr<IToastNotification> notification;
  Microsoft::WRL::ComPtr<IToastNotificationFactory> factory;
  AssertRetHRESULT(GetActivationFactory(HStringReference(RuntimeClass_Windows_UI_Notifications_ToastNotification).Get(),
    factory.GetAddressOf()), false);
  AssertRetHRESULT(factory->CreateToastNotification(toastXml, &notification),
                   false);

  EventRegistrationToken activatedToken;
  AssertRetHRESULT(notification->add_Activated(Callback<ToastActivationHandler>(this,
    &ToastNotificationHandler::OnActivate).Get(), &activatedToken), false);
  EventRegistrationToken dismissedToken;
  AssertRetHRESULT(notification->add_Dismissed(Callback<ToastDismissHandler>(this,
    &ToastNotificationHandler::OnDismiss).Get(), &dismissedToken), false);

  Microsoft::WRL::ComPtr<IToastNotifier> notifier;
  if (aAppId.IsEmpty()) {
    AssertRetHRESULT(mToastNotificationManagerStatics->CreateToastNotifier(
                       &notifier), false);
  } else {
    AssertRetHRESULT(mToastNotificationManagerStatics->CreateToastNotifierWithId(
                    HStringReference(PromiseFlatString(aAppId).get()).Get(),
                    &notifier), false);
  }
  AssertRetHRESULT(notifier->Show(notification.Get()), false);

  MetroUtils::FireObserver("metro_native_toast_shown", mCookie.get());

  return true;
}

void ToastNotificationHandler::SetNodeValueString(HSTRING inputString,
  Microsoft::WRL::ComPtr<IXmlNode> node, Microsoft::WRL::ComPtr<IXmlDocument> xml) {
  Microsoft::WRL::ComPtr<IXmlText> inputText;
  Microsoft::WRL::ComPtr<IXmlNode> inputTextNode, pAppendedChild;

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
  delete this;
  return S_OK;
}
