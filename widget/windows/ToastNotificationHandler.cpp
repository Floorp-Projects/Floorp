/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sts=2 sw=2 et cin: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "ToastNotificationHandler.h"

#include "WidgetUtils.h"
#include "WinTaskbar.h"
#include "WinUtils.h"
#include "imgIContainer.h"
#include "imgIRequest.h"
#include "mozilla/WindowsVersion.h"
#include "mozilla/gfx/2D.h"
#include "nsDirectoryServiceDefs.h"
#include "nsIDUtils.h"
#include "nsIStringBundle.h"
#include "nsIURI.h"
#include "nsIWidget.h"
#include "nsIWindowMediator.h"
#include "nsNetUtil.h"
#include "nsPIDOMWindow.h"
#include "nsProxyRelease.h"

#include "ToastNotification.h"

namespace mozilla {
namespace widget {

typedef ABI::Windows::Foundation::ITypedEventHandler<
    ABI::Windows::UI::Notifications::ToastNotification*, IInspectable*>
    ToastActivationHandler;
typedef ABI::Windows::Foundation::ITypedEventHandler<
    ABI::Windows::UI::Notifications::ToastNotification*,
    ABI::Windows::UI::Notifications::ToastDismissedEventArgs*>
    ToastDismissedHandler;
typedef ABI::Windows::Foundation::ITypedEventHandler<
    ABI::Windows::UI::Notifications::ToastNotification*,
    ABI::Windows::UI::Notifications::ToastFailedEventArgs*>
    ToastFailedHandler;

using namespace ABI::Windows::Data::Xml::Dom;
using namespace ABI::Windows::Foundation;
using namespace ABI::Windows::UI::Notifications;
using namespace Microsoft::WRL;
using namespace Microsoft::WRL::Wrappers;
using namespace mozilla;

NS_IMPL_ISUPPORTS(ToastNotificationHandler, nsIAlertNotificationImageListener)

static bool SetNodeValueString(const nsString& aString, IXmlNode* node,
                               IXmlDocument* xml) {
  ComPtr<IXmlText> inputText;
  HRESULT hr;
  hr = xml->CreateTextNode(HStringReference(aString.get()).Get(), &inputText);
  NS_ENSURE_TRUE(SUCCEEDED(hr), false);

  ComPtr<IXmlNode> inputTextNode;
  hr = inputText.As(&inputTextNode);
  NS_ENSURE_TRUE(SUCCEEDED(hr), false);

  ComPtr<IXmlNode> appendedChild;
  hr = node->AppendChild(inputTextNode.Get(), &appendedChild);
  NS_ENSURE_TRUE(SUCCEEDED(hr), false);

  return true;
}

static bool SetAttribute(IXmlElement* element, const HStringReference& name,
                         const nsAString& value) {
  HString valueStr;
  valueStr.Set(PromiseFlatString(value).get());

  HRESULT hr = element->SetAttribute(name.Get(), valueStr.Get());
  NS_ENSURE_TRUE(SUCCEEDED(hr), false);

  return true;
}

static bool AddActionNode(IXmlDocument* toastXml, IXmlNode* actionsNode,
                          const nsAString& actionTitle,
                          const nsAString& actionArgs,
                          const nsAString& actionPlacement = u""_ns) {
  ComPtr<IXmlElement> action;
  HRESULT hr =
      toastXml->CreateElement(HStringReference(L"action").Get(), &action);
  NS_ENSURE_TRUE(SUCCEEDED(hr), false);

  bool success =
      SetAttribute(action.Get(), HStringReference(L"content"), actionTitle);
  NS_ENSURE_TRUE(success, false);

  success =
      SetAttribute(action.Get(), HStringReference(L"arguments"), actionArgs);
  NS_ENSURE_TRUE(success, false);

  if (!actionPlacement.IsEmpty()) {
    success = SetAttribute(action.Get(), HStringReference(L"placement"),
                           actionPlacement);
    NS_ENSURE_TRUE(success, false);
  }

  // Add <action> to <actions>
  ComPtr<IXmlNode> actionNode;
  hr = action.As(&actionNode);
  NS_ENSURE_TRUE(SUCCEEDED(hr), false);

  ComPtr<IXmlNode> appendedChild;
  hr = actionsNode->AppendChild(actionNode.Get(), &appendedChild);
  if (NS_WARN_IF(FAILED(hr))) {
    return false;
  }

  return true;
}

static ComPtr<IToastNotificationManagerStatics>
GetToastNotificationManagerStatics() {
  ComPtr<IToastNotificationManagerStatics> toastNotificationManagerStatics;
  if (NS_WARN_IF(FAILED(GetActivationFactory(
          HStringReference(
              RuntimeClass_Windows_UI_Notifications_ToastNotificationManager)
              .Get(),
          &toastNotificationManagerStatics)))) {
    return nullptr;
  }

  return toastNotificationManagerStatics;
}

ToastNotificationHandler::~ToastNotificationHandler() {
  if (mImageRequest) {
    mImageRequest->Cancel(NS_BINDING_ABORTED);
    mImageRequest = nullptr;
  }

  if (mHasImage && mImageFile) {
    DebugOnly<nsresult> rv = mImageFile->Remove(false);
    NS_ASSERTION(NS_SUCCEEDED(rv), "Cannot remove temporary image file");
  }

  UnregisterHandler();
}

void ToastNotificationHandler::UnregisterHandler() {
  if (mNotification && mNotifier) {
    mNotification->remove_Dismissed(mDismissedToken);
    mNotification->remove_Activated(mActivatedToken);
    mNotification->remove_Failed(mFailedToken);
    mNotifier->Hide(mNotification.Get());
  }

  mNotification = nullptr;
  mNotifier = nullptr;

  SendFinished();
}

nsresult ToastNotificationHandler::InitAlertAsync(
    nsIAlertNotification* aAlert) {
  return aAlert->LoadImage(/* aTimeout = */ 0, this, /* aUserData = */ nullptr,
                           getter_AddRefs(mImageRequest));
}

ComPtr<IXmlDocument> ToastNotificationHandler::CreateToastXmlDocument() {
  ComPtr<IToastNotificationManagerStatics> toastNotificationManagerStatics =
      GetToastNotificationManagerStatics();

  ToastTemplateType toastTemplate;
  if (mHostPort.IsEmpty()) {
    toastTemplate =
        mHasImage ? ToastTemplateType::ToastTemplateType_ToastImageAndText03
                  : ToastTemplateType::ToastTemplateType_ToastText03;
  } else {
    toastTemplate =
        mHasImage ? ToastTemplateType::ToastTemplateType_ToastImageAndText04
                  : ToastTemplateType::ToastTemplateType_ToastText04;
  }

  ComPtr<IXmlDocument> toastXml;
  toastNotificationManagerStatics->GetTemplateContent(toastTemplate, &toastXml);

  if (!toastXml) {
    return nullptr;
  }

  nsresult ns;
  HRESULT hr;
  bool success;

  if (mHasImage) {
    ComPtr<IXmlNodeList> toastImageElements;
    hr = toastXml->GetElementsByTagName(HStringReference(L"image").Get(),
                                        &toastImageElements);
    NS_ENSURE_TRUE(SUCCEEDED(hr), nullptr);

    ComPtr<IXmlNode> imageNode;
    hr = toastImageElements->Item(0, &imageNode);
    NS_ENSURE_TRUE(SUCCEEDED(hr), nullptr);

    ComPtr<IXmlElement> image;
    hr = imageNode.As(&image);
    NS_ENSURE_TRUE(SUCCEEDED(hr), nullptr);

    success = SetAttribute(image.Get(), HStringReference(L"src"), mImageUri);
    NS_ENSURE_TRUE(success, nullptr);
  }

  ComPtr<IXmlNodeList> toastTextElements;
  hr = toastXml->GetElementsByTagName(HStringReference(L"text").Get(),
                                      &toastTextElements);
  NS_ENSURE_TRUE(SUCCEEDED(hr), nullptr);

  ComPtr<IXmlNode> titleTextNodeRoot;
  hr = toastTextElements->Item(0, &titleTextNodeRoot);
  NS_ENSURE_TRUE(SUCCEEDED(hr), nullptr);

  ComPtr<IXmlNode> msgTextNodeRoot;
  hr = toastTextElements->Item(1, &msgTextNodeRoot);
  NS_ENSURE_TRUE(SUCCEEDED(hr), nullptr);

  success = SetNodeValueString(mTitle, titleTextNodeRoot.Get(), toastXml.Get());
  NS_ENSURE_TRUE(success, nullptr);

  success = SetNodeValueString(mMsg, msgTextNodeRoot.Get(), toastXml.Get());
  NS_ENSURE_TRUE(success, nullptr);

  ComPtr<IXmlNodeList> toastElements;
  hr = toastXml->GetElementsByTagName(HStringReference(L"toast").Get(),
                                      &toastElements);
  NS_ENSURE_TRUE(SUCCEEDED(hr), nullptr);

  ComPtr<IXmlNode> toastNodeRoot;
  hr = toastElements->Item(0, &toastNodeRoot);
  NS_ENSURE_TRUE(SUCCEEDED(hr), nullptr);

  if (mRequireInteraction) {
    ComPtr<IXmlElement> toastNode;
    hr = toastNodeRoot.As(&toastNode);
    NS_ENSURE_TRUE(SUCCEEDED(hr), nullptr);

    success = SetAttribute(toastNode.Get(), HStringReference(L"scenario"),
                           u"reminder"_ns);
    NS_ENSURE_TRUE(success, nullptr);
  }

  ComPtr<IXmlElement> actions;
  hr = toastXml->CreateElement(HStringReference(L"actions").Get(), &actions);
  NS_ENSURE_TRUE(SUCCEEDED(hr), nullptr);

  ComPtr<IXmlNode> actionsNode;
  hr = actions.As(&actionsNode);
  NS_ENSURE_TRUE(SUCCEEDED(hr), nullptr);

  nsCOMPtr<nsIStringBundleService> sbs =
      do_GetService(NS_STRINGBUNDLE_CONTRACTID);
  NS_ENSURE_TRUE(sbs, nullptr);

  nsCOMPtr<nsIStringBundle> bundle;
  sbs->CreateBundle("chrome://alerts/locale/alert.properties",
                    getter_AddRefs(bundle));
  NS_ENSURE_TRUE(bundle, nullptr);

  if (!mHostPort.IsEmpty()) {
    AutoTArray<nsString, 1> formatStrings = {mHostPort};

    ComPtr<IXmlNode> urlTextNodeRoot;
    hr = toastTextElements->Item(2, &urlTextNodeRoot);
    NS_ENSURE_TRUE(SUCCEEDED(hr), nullptr);

    nsAutoString urlReference;
    bundle->FormatStringFromName("source.label", formatStrings, urlReference);

    success =
        SetNodeValueString(urlReference, urlTextNodeRoot.Get(), toastXml.Get());
    NS_ENSURE_TRUE(success, nullptr);

    if (IsWin10AnniversaryUpdateOrLater()) {
      ComPtr<IXmlElement> placementText;
      hr = urlTextNodeRoot.As(&placementText);
      if (SUCCEEDED(hr)) {
        // placement is supported on Windows 10 Anniversary Update or later
        SetAttribute(placementText.Get(), HStringReference(L"placement"),
                     u"attribution"_ns);
      }
    }

    nsAutoString disableButtonTitle;
    ns = bundle->FormatStringFromName("webActions.disableForOrigin.label",
                                      formatStrings, disableButtonTitle);
    NS_ENSURE_SUCCESS(ns, nullptr);

    AddActionNode(toastXml.Get(), actionsNode.Get(), disableButtonTitle,
                  u"snooze"_ns, u"contextmenu"_ns);
  }

  nsAutoString settingsButtonTitle;
  bundle->GetStringFromName("webActions.settings.label", settingsButtonTitle);
  success =
      AddActionNode(toastXml.Get(), actionsNode.Get(), settingsButtonTitle,
                    u"settings"_ns, u"contextmenu"_ns);
  NS_ENSURE_TRUE(success, nullptr);

  for (const auto& action : mActions) {
    // Bug 1778596: include per-action icon from image URL.
    nsString title;
    ns = action->GetTitle(title);
    NS_ENSURE_SUCCESS(ns, nullptr);

    nsString actionString;
    ns = action->GetAction(actionString);
    NS_ENSURE_SUCCESS(ns, nullptr);

    success =
        AddActionNode(toastXml.Get(), actionsNode.Get(), title, actionString);
    NS_ENSURE_TRUE(success, nullptr);
  }

  ComPtr<IXmlNode> appendedChild;
  hr = toastNodeRoot->AppendChild(actionsNode.Get(), &appendedChild);
  NS_ENSURE_TRUE(SUCCEEDED(hr), nullptr);

  return toastXml;
}

nsresult ToastNotificationHandler::CreateToastXmlString(
    const nsAString& aImageURL, nsAString& aString) {
  HRESULT hr;

  if (!aImageURL.IsEmpty()) {
    // For testing: don't fetch and write image to disk, just include the URL.
    mHasImage = true;
    mImageUri.Assign(aImageURL);
  }

  ComPtr<IXmlDocument> toastXml = CreateToastXmlDocument();
  if (!toastXml) {
    return NS_ERROR_FAILURE;
  }

  ComPtr<IXmlNodeSerializer> ser;
  hr = toastXml.As(&ser);
  NS_ENSURE_TRUE(SUCCEEDED(hr), NS_ERROR_FAILURE);

  HString data;
  hr = ser->GetXml(data.GetAddressOf());
  NS_ENSURE_TRUE(SUCCEEDED(hr), NS_ERROR_FAILURE);

  uint32_t len = 0;
  const wchar_t* rawData = data.GetRawBuffer(&len);
  NS_ENSURE_TRUE(rawData, NS_ERROR_FAILURE);
  aString.Assign(rawData, len);

  return NS_OK;
}

bool ToastNotificationHandler::ShowAlert() {
  if (!mBackend->IsActiveHandler(mName, this)) {
    return false;
  }

  ComPtr<IXmlDocument> toastXml = CreateToastXmlDocument();

  if (!toastXml) {
    return false;
  }

  return CreateWindowsNotificationFromXml(toastXml.Get());
}

bool ToastNotificationHandler::CreateWindowsNotificationFromXml(
    IXmlDocument* aXml) {
  ComPtr<IToastNotificationFactory> factory;
  HRESULT hr;

  hr = GetActivationFactory(
      HStringReference(RuntimeClass_Windows_UI_Notifications_ToastNotification)
          .Get(),
      &factory);
  NS_ENSURE_TRUE(SUCCEEDED(hr), false);

  hr = factory->CreateToastNotification(aXml, &mNotification);
  NS_ENSURE_TRUE(SUCCEEDED(hr), false);

  RefPtr<ToastNotificationHandler> self = this;

  hr = mNotification->add_Activated(
      Callback<ToastActivationHandler>([self](IToastNotification* aNotification,
                                              IInspectable* aInspectable) {
        return self->OnActivate(aNotification, aInspectable);
      }).Get(),
      &mActivatedToken);
  NS_ENSURE_TRUE(SUCCEEDED(hr), false);

  hr = mNotification->add_Dismissed(
      Callback<ToastDismissedHandler>([self](IToastNotification* aNotification,
                                             IToastDismissedEventArgs* aArgs) {
        return self->OnDismiss(aNotification, aArgs);
      }).Get(),
      &mDismissedToken);
  NS_ENSURE_TRUE(SUCCEEDED(hr), false);

  hr = mNotification->add_Failed(
      Callback<ToastFailedHandler>([self](IToastNotification* aNotification,
                                          IToastFailedEventArgs* aArgs) {
        return self->OnFail(aNotification, aArgs);
      }).Get(),
      &mFailedToken);
  NS_ENSURE_TRUE(SUCCEEDED(hr), false);

  ComPtr<IToastNotificationManagerStatics> toastNotificationManagerStatics =
      GetToastNotificationManagerStatics();
  NS_ENSURE_TRUE(SUCCEEDED(hr), false);

  nsAutoString uid;
  if (NS_WARN_IF(!WinTaskbar::GetAppUserModelID(uid))) {
    return false;
  }

  HSTRING uidStr =
      HStringReference(static_cast<const wchar_t*>(uid.get())).Get();
  hr = toastNotificationManagerStatics->CreateToastNotifierWithId(uidStr,
                                                                  &mNotifier);
  NS_ENSURE_TRUE(SUCCEEDED(hr), false);

  hr = mNotifier->Show(mNotification.Get());
  NS_ENSURE_TRUE(SUCCEEDED(hr), false);

  if (mAlertListener) {
    mAlertListener->Observe(nullptr, "alertshow", mCookie.get());
  }

  return true;
}

void ToastNotificationHandler::SendFinished() {
  if (!mSentFinished && mAlertListener) {
    mAlertListener->Observe(nullptr, "alertfinished", mCookie.get());
  }

  mSentFinished = true;
}

HRESULT
ToastNotificationHandler::OnActivate(IToastNotification* notification,
                                     IInspectable* inspectable) {
  if (mAlertListener) {
    nsAutoString argString;
    if (inspectable) {
      ComPtr<IToastActivatedEventArgs> eventArgs;
      HRESULT hr = inspectable->QueryInterface(
          __uuidof(IToastActivatedEventArgs), (void**)&eventArgs);
      if (SUCCEEDED(hr)) {
        HString arguments;
        hr = eventArgs->get_Arguments(arguments.GetAddressOf());
        if (SUCCEEDED(hr)) {
          uint32_t len = 0;
          const wchar_t* buffer = arguments.GetRawBuffer(&len);
          if (buffer) {
            argString.Assign(buffer, len);
          }
        }
      }
    }

    if (argString.EqualsLiteral("settings")) {
      mAlertListener->Observe(nullptr, "alertsettingscallback", mCookie.get());
    } else if (argString.EqualsLiteral("snooze")) {
      mAlertListener->Observe(nullptr, "alertdisablecallback", mCookie.get());
    } else if (mClickable) {
      // When clicking toast, focus moves to another process, but we want to set
      // focus on Firefox process.
      nsCOMPtr<nsIWindowMediator> winMediator(
          do_GetService(NS_WINDOWMEDIATOR_CONTRACTID));
      if (winMediator) {
        nsCOMPtr<mozIDOMWindowProxy> navWin;
        winMediator->GetMostRecentWindow(u"navigator:browser",
                                         getter_AddRefs(navWin));
        if (navWin) {
          nsCOMPtr<nsIWidget> widget =
              WidgetUtils::DOMWindowToWidget(nsPIDOMWindowOuter::From(navWin));
          if (widget) {
            SetForegroundWindow(
                static_cast<HWND>(widget->GetNativeData(NS_NATIVE_WINDOW)));
          }
        }
      }
      mAlertListener->Observe(nullptr, "alertclickcallback", mCookie.get());
    }
  }
  mBackend->RemoveHandler(mName, this);
  return S_OK;
}

HRESULT
ToastNotificationHandler::OnDismiss(IToastNotification* notification,
                                    IToastDismissedEventArgs* aArgs) {
  SendFinished();
  mBackend->RemoveHandler(mName, this);
  return S_OK;
}

HRESULT
ToastNotificationHandler::OnFail(IToastNotification* notification,
                                 IToastFailedEventArgs* aArgs) {
  SendFinished();
  mBackend->RemoveHandler(mName, this);
  return S_OK;
}

nsresult ToastNotificationHandler::TryShowAlert() {
  if (NS_WARN_IF(!ShowAlert())) {
    mBackend->RemoveHandler(mName, this);
    return NS_ERROR_FAILURE;
  }
  return NS_OK;
}

NS_IMETHODIMP
ToastNotificationHandler::OnImageMissing(nsISupports*) {
  return TryShowAlert();
}

NS_IMETHODIMP
ToastNotificationHandler::OnImageReady(nsISupports*, imgIRequest* aRequest) {
  nsresult rv = AsyncSaveImage(aRequest);
  if (NS_FAILED(rv)) {
    return TryShowAlert();
  }
  return rv;
}

nsresult ToastNotificationHandler::AsyncSaveImage(imgIRequest* aRequest) {
  nsresult rv =
      NS_GetSpecialDirectory(NS_OS_TEMP_DIR, getter_AddRefs(mImageFile));
  NS_ENSURE_SUCCESS(rv, rv);

  rv = mImageFile->Append(u"notificationimages"_ns);
  NS_ENSURE_SUCCESS(rv, rv);

  rv = mImageFile->Create(nsIFile::DIRECTORY_TYPE, 0500);
  if (NS_FAILED(rv) && rv != NS_ERROR_FILE_ALREADY_EXISTS) {
    return rv;
  }

  nsID uuid;
  rv = nsID::GenerateUUIDInPlace(uuid);
  NS_ENSURE_SUCCESS(rv, rv);

  NSID_TrimBracketsASCII uuidStr(uuid);
  uuidStr.AppendLiteral(".bmp");
  mImageFile->AppendNative(uuidStr);

  nsCOMPtr<imgIContainer> imgContainer;
  rv = aRequest->GetImage(getter_AddRefs(imgContainer));
  NS_ENSURE_SUCCESS(rv, rv);

  nsMainThreadPtrHandle<ToastNotificationHandler> self(
      new nsMainThreadPtrHolder<ToastNotificationHandler>(
          "ToastNotificationHandler", this));

  nsCOMPtr<nsIFile> imageFile(mImageFile);
  RefPtr<mozilla::gfx::SourceSurface> surface = imgContainer->GetFrame(
      imgIContainer::FRAME_FIRST,
      imgIContainer::FLAG_SYNC_DECODE | imgIContainer::FLAG_ASYNC_NOTIFY);
  nsCOMPtr<nsIRunnable> r = NS_NewRunnableFunction(
      "ToastNotificationHandler::AsyncWriteBitmap",
      [self, imageFile, surface]() -> void {
        nsresult rv;
        if (!surface) {
          rv = NS_ERROR_FAILURE;
        } else {
          rv = WinUtils::WriteBitmap(imageFile, surface);
        }

        nsCOMPtr<nsIRunnable> cbRunnable = NS_NewRunnableFunction(
            "ToastNotificationHandler::AsyncWriteBitmapCb",
            [self, rv]() -> void {
              auto handler = const_cast<ToastNotificationHandler*>(self.get());
              handler->OnWriteBitmapFinished(rv);
            });

        NS_DispatchToMainThread(cbRunnable);
      });

  return mBackend->BackgroundDispatch(r);
}

void ToastNotificationHandler::OnWriteBitmapFinished(nsresult rv) {
  if (NS_SUCCEEDED(rv)) {
    OnWriteBitmapSuccess();
  }
  TryShowAlert();
}

nsresult ToastNotificationHandler::OnWriteBitmapSuccess() {
  nsresult rv;

  nsCOMPtr<nsIURI> fileURI;
  rv = NS_NewFileURI(getter_AddRefs(fileURI), mImageFile);
  NS_ENSURE_SUCCESS(rv, rv);

  nsAutoCString uriStr;
  rv = fileURI->GetSpec(uriStr);
  NS_ENSURE_SUCCESS(rv, rv);

  AppendUTF8toUTF16(uriStr, mImageUri);

  mHasImage = true;

  return NS_OK;
}

}  // namespace widget
}  // namespace mozilla
