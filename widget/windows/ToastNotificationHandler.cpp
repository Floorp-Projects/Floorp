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
  if (NS_WARN_IF(FAILED(xml->CreateTextNode(
          HStringReference(static_cast<const wchar_t*>(aString.get())).Get(),
          &inputText)))) {
    return false;
  }
  ComPtr<IXmlNode> inputTextNode;
  if (NS_WARN_IF(FAILED(inputText.As(&inputTextNode)))) {
    return false;
  }
  ComPtr<IXmlNode> appendedChild;
  if (NS_WARN_IF(
          FAILED(node->AppendChild(inputTextNode.Get(), &appendedChild)))) {
    return false;
  }
  return true;
}

static bool SetAttribute(IXmlElement* element, const HSTRING name,
                         const nsAString& value) {
  HSTRING valueStr = HStringReference(static_cast<const wchar_t*>(
                                          PromiseFlatString(value).get()))
                         .Get();
  if (NS_WARN_IF(FAILED(element->SetAttribute(name, valueStr)))) {
    return false;
  }
  return true;
}

static bool AddActionNode(IXmlDocument* toastXml, IXmlNode* actionsNode,
                          const nsAString& actionTitle,
                          const nsAString& actionArgs) {
  ComPtr<IXmlElement> action;
  HRESULT hr =
      toastXml->CreateElement(HStringReference(L"action").Get(), &action);
  if (NS_WARN_IF(FAILED(hr))) {
    return false;
  }

  if (NS_WARN_IF(!SetAttribute(action.Get(), HStringReference(L"content").Get(),
                               actionTitle))) {
    return false;
  }

  if (NS_WARN_IF(!SetAttribute(
          action.Get(), HStringReference(L"arguments").Get(), actionArgs))) {
    return false;
  }
  if (NS_WARN_IF(!SetAttribute(action.Get(),
                               HStringReference(L"placement").Get(),
                               u"contextmenu"_ns))) {
    return false;
  }

  // Add <action> to <actions>
  ComPtr<IXmlNode> actionNode;
  hr = action.As(&actionNode);
  if (NS_WARN_IF(FAILED(hr))) {
    return false;
  }

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

  HRESULT hr;

  if (mHasImage) {
    ComPtr<IXmlNodeList> toastImageElements;
    hr = toastXml->GetElementsByTagName(HStringReference(L"image").Get(),
                                        &toastImageElements);
    if (NS_WARN_IF(FAILED(hr))) {
      return nullptr;
    }
    ComPtr<IXmlNode> imageNode;
    hr = toastImageElements->Item(0, &imageNode);
    if (NS_WARN_IF(FAILED(hr))) {
      return nullptr;
    }
    ComPtr<IXmlElement> image;
    hr = imageNode.As(&image);
    if (NS_WARN_IF(FAILED(hr))) {
      return nullptr;
    }
    if (NS_WARN_IF(!SetAttribute(image.Get(), HStringReference(L"src").Get(),
                                 mImageUri))) {
      return nullptr;
    }
  }

  ComPtr<IXmlNodeList> toastTextElements;
  hr = toastXml->GetElementsByTagName(HStringReference(L"text").Get(),
                                      &toastTextElements);
  if (NS_WARN_IF(FAILED(hr))) {
    return nullptr;
  }

  ComPtr<IXmlNode> titleTextNodeRoot;
  hr = toastTextElements->Item(0, &titleTextNodeRoot);
  if (NS_WARN_IF(FAILED(hr))) {
    return nullptr;
  }
  ComPtr<IXmlNode> msgTextNodeRoot;
  hr = toastTextElements->Item(1, &msgTextNodeRoot);
  if (NS_WARN_IF(FAILED(hr))) {
    return nullptr;
  }

  if (NS_WARN_IF(!SetNodeValueString(mTitle, titleTextNodeRoot.Get(),
                                     toastXml.Get()))) {
    return nullptr;
  }
  if (NS_WARN_IF(
          !SetNodeValueString(mMsg, msgTextNodeRoot.Get(), toastXml.Get()))) {
    return nullptr;
  }

  ComPtr<IXmlNodeList> toastElements;
  hr = toastXml->GetElementsByTagName(HStringReference(L"toast").Get(),
                                      &toastElements);
  if (NS_WARN_IF(FAILED(hr))) {
    return nullptr;
  }

  ComPtr<IXmlNode> toastNodeRoot;
  hr = toastElements->Item(0, &toastNodeRoot);
  if (NS_WARN_IF(FAILED(hr))) {
    return nullptr;
  }

  ComPtr<IXmlElement> actions;
  hr = toastXml->CreateElement(HStringReference(L"actions").Get(), &actions);
  if (NS_WARN_IF(FAILED(hr))) {
    return nullptr;
  }

  ComPtr<IXmlNode> actionsNode;
  hr = actions.As(&actionsNode);
  if (NS_WARN_IF(FAILED(hr))) {
    return nullptr;
  }

  nsCOMPtr<nsIStringBundleService> sbs =
      do_GetService(NS_STRINGBUNDLE_CONTRACTID);
  if (NS_WARN_IF(!sbs)) {
    return nullptr;
  }

  nsCOMPtr<nsIStringBundle> bundle;
  sbs->CreateBundle("chrome://alerts/locale/alert.properties",
                    getter_AddRefs(bundle));
  if (NS_WARN_IF(!bundle)) {
    return nullptr;
  }

  if (!mHostPort.IsEmpty()) {
    AutoTArray<nsString, 1> formatStrings = {mHostPort};

    ComPtr<IXmlNode> urlTextNodeRoot;
    hr = toastTextElements->Item(2, &urlTextNodeRoot);
    if (NS_WARN_IF(FAILED(hr))) {
      return nullptr;
    }

    nsAutoString urlReference;
    bundle->FormatStringFromName("source.label", formatStrings, urlReference);

    if (NS_WARN_IF(!SetNodeValueString(urlReference, urlTextNodeRoot.Get(),
                                       toastXml.Get()))) {
      return nullptr;
    }

    if (IsWin10AnniversaryUpdateOrLater()) {
      ComPtr<IXmlElement> placementText;
      hr = urlTextNodeRoot.As(&placementText);
      if (SUCCEEDED(hr)) {
        // placement is supported on Windows 10 Anniversary Update or later
        SetAttribute(placementText.Get(), HStringReference(L"placement").Get(),
                     u"attribution"_ns);
      }
    }

    nsAutoString disableButtonTitle;
    bundle->FormatStringFromName("webActions.disableForOrigin.label",
                                 formatStrings, disableButtonTitle);

    AddActionNode(toastXml.Get(), actionsNode.Get(), disableButtonTitle,
                  u"snooze"_ns);
  }

  nsAutoString settingsButtonTitle;
  bundle->GetStringFromName("webActions.settings.label", settingsButtonTitle);
  AddActionNode(toastXml.Get(), actionsNode.Get(), settingsButtonTitle,
                u"settings"_ns);

  ComPtr<IXmlNode> appendedChild;
  hr = toastNodeRoot->AppendChild(actionsNode.Get(), &appendedChild);
  if (NS_WARN_IF(FAILED(hr))) {
    return nullptr;
  }

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
  if (NS_WARN_IF(FAILED(hr))) {
    return NS_ERROR_FAILURE;
  }

  HSTRING data;
  hr = ser->GetXml(&data);
  if (NS_WARN_IF(FAILED(hr))) {
    return NS_ERROR_FAILURE;
  }

  uint32_t len = 0;
  const wchar_t* buffer = WindowsGetStringRawBuffer(data, &len);
  if (!buffer) {
    return NS_ERROR_FAILURE;
  }

  aString.Assign(buffer);
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
  HRESULT hr = GetActivationFactory(
      HStringReference(RuntimeClass_Windows_UI_Notifications_ToastNotification)
          .Get(),
      &factory);
  if (NS_WARN_IF(FAILED(hr))) {
    return false;
  }

  hr = factory->CreateToastNotification(aXml, &mNotification);
  if (NS_WARN_IF(FAILED(hr))) {
    return false;
  }

  RefPtr<ToastNotificationHandler> self = this;

  hr = mNotification->add_Activated(
      Callback<ToastActivationHandler>([self](IToastNotification* aNotification,
                                              IInspectable* aInspectable) {
        return self->OnActivate(aNotification, aInspectable);
      }).Get(),
      &mActivatedToken);
  if (NS_WARN_IF(FAILED(hr))) {
    return false;
  }

  hr = mNotification->add_Dismissed(
      Callback<ToastDismissedHandler>([self](IToastNotification* aNotification,
                                             IToastDismissedEventArgs* aArgs) {
        return self->OnDismiss(aNotification, aArgs);
      }).Get(),
      &mDismissedToken);
  if (NS_WARN_IF(FAILED(hr))) {
    return false;
  }

  hr = mNotification->add_Failed(
      Callback<ToastFailedHandler>([self](IToastNotification* aNotification,
                                          IToastFailedEventArgs* aArgs) {
        return self->OnFail(aNotification, aArgs);
      }).Get(),
      &mFailedToken);
  if (NS_WARN_IF(FAILED(hr))) {
    return false;
  }

  ComPtr<IToastNotificationManagerStatics> toastNotificationManagerStatics =
      GetToastNotificationManagerStatics();
  if (NS_WARN_IF(!toastNotificationManagerStatics)) {
    return false;
  }

  nsAutoString uid;
  if (NS_WARN_IF(!WinTaskbar::GetAppUserModelID(uid))) {
    return false;
  }

  HSTRING uidStr =
      HStringReference(static_cast<const wchar_t*>(uid.get())).Get();
  hr = toastNotificationManagerStatics->CreateToastNotifierWithId(uidStr,
                                                                  &mNotifier);
  if (NS_WARN_IF(FAILED(hr))) {
    return false;
  }

  hr = mNotifier->Show(mNotification.Get());
  if (NS_WARN_IF(FAILED(hr))) {
    return false;
  }

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
        HSTRING arguments;
        hr = eventArgs->get_Arguments(&arguments);
        if (SUCCEEDED(hr)) {
          uint32_t len = 0;
          const wchar_t* buffer = WindowsGetStringRawBuffer(arguments, &len);
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
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  rv = mImageFile->Append(u"notificationimages"_ns);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  rv = mImageFile->Create(nsIFile::DIRECTORY_TYPE, 0500);
  if (NS_FAILED(rv) && rv != NS_ERROR_FILE_ALREADY_EXISTS) {
    return rv;
  }

  nsID uuid;
  rv = nsID::GenerateUUIDInPlace(uuid);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  NSID_TrimBracketsASCII uuidStr(uuid);
  uuidStr.AppendLiteral(".bmp");
  mImageFile->AppendNative(uuidStr);

  nsCOMPtr<imgIContainer> imgContainer;
  rv = aRequest->GetImage(getter_AddRefs(imgContainer));
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

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
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  nsAutoCString uriStr;
  rv = fileURI->GetSpec(uriStr);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  AppendUTF8toUTF16(uriStr, mImageUri);

  mHasImage = true;

  return NS_OK;
}

}  // namespace widget
}  // namespace mozilla
